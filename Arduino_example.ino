#include <Arduino.h>
#include <Ticker.h>
#include <WiFiManager.h>
#include <ESP8266WiFi.h>
#include <time.h>

// ==================== Pin Definitions ====================
#define PT6315_CS_PIN      16  // D0
#define PT6315_CLK_PIN     14  // D5
#define PT6315_DATA_PIN    12  // D6
#define PWM_PIN            2   // D4 (GPIO2) - VFD filament

#define GPIO15_PIN         15  // D8 - Force high level
#define MODE_BUTTON_PIN    5   // D1 - Time/Date toggle button
#define BRIGHT_BUTTON_PIN  4   // D2 - Brightness toggle button

// ==================== PT6315 Command Definitions ====================
#define CMD_4DIG_24SEG     0x00
#define CMD_5DIG_23SEG     0x01
#define CMD_6DIG_22SEG     0x02
#define CMD_7DIG_21SEG     0x03
#define CMD_8DIG_20SEG     0x04
#define CMD_9DIG_19SEG     0x05
#define CMD_10DIG_18SEG    0x06
#define CMD_11DIG_17SEG    0x07
#define CMD_12DIG_16SEG    0x08

#define CMD_WRITE_DATA_TO_DISPLAY_MODE  0x80
#define CMD_WRITE_DATA_TO_LED_PORT      0x01
#define CMD_READ_KEY_DATA               0x10

#define CMD_INCREMENT_ADDRESS_MODE      0x00
#define CMD_FIXED_ADDRESS_MODE          0x40
#define CMD_ADDRESS_SET                 0xC0

#define CMD_NORMAL_MODE     0x00
#define CMD_TEST_MODE       0x80

#define CMD_DISPLAY_OFF     0x00
#define CMD_DISPLAY_ON      0x08

// Icon bit masks
#define DISPLAY_ICON_REC        0x02
#define DISPLAY_ICON_CLOCK      0x04
#define DISPLAY_ICON_3D_ON      0x08
#define DISPLAY_ICON_WIFI_ON    0x10

// Brightness levels
#define BRIGHTNESS_MIN      0
#define BRIGHTNESS_MAX      7

// ==================== Pin Operation Macros ====================
#define SET_PT6315_CS      digitalWrite(PT6315_CS_PIN, 1)
#define CLR_PT6315_CS      digitalWrite(PT6315_CS_PIN, 0)
#define SET_PT6315_CLK     digitalWrite(PT6315_CLK_PIN, 1)
#define CLR_PT6315_CLK     digitalWrite(PT6315_CLK_PIN, 0)
#define SET_PT6315_DATA    digitalWrite(PT6315_DATA_PIN, 1)
#define CLR_PT6315_DATA    digitalWrite(PT6315_DATA_PIN, 0)

// ==================== Character Segment Table (21 segments) ====================
static const unsigned char font[][3] = {
  {0x8f,0x88,0x0f}, // 0
  {0x84,0x80,0x08}, // 1
  {0x87,0x0f,0x0f}, // 2
  {0x87,0x87,0x0f}, // 3
  {0x8d,0x87,0x08}, // 4
  {0x0f,0x87,0x0f}, // 5
  {0x0f,0x8f,0x0f}, // 6
  {0x87,0x80,0x08}, // 7
  {0x8f,0x8f,0x0f}, // 8
  {0x8f,0x87,0x0f}, // 9
  {0x8a,0x8f,0x09}, // A 10
  {0x8b,0x8f,0x07}, // B 11
  {0x0e,0x08,0x0e}, // C 12
  {0x8f,0x88,0x07}, // D 13
  {0x0f,0x0f,0x0f}, // E 14
  {0x0f,0x0f,0x01}, // F 15
  {0x0e,0x8c,0x06}, // G 16
  {0x8d,0x8f,0x09}, // H 17
  {0x22,0x22,0x06}, // I 18
  {0x84,0x88,0x0f}, // J 19
  {0x4d,0x4B,0x09}, // K 20
  {0x09,0x08,0x0f}, // L 21
  {0xdd,0x8a,0x09}, // M 22
  {0x9d,0xca,0x09}, // N 23
  {0x8a,0x88,0x06}, // O 24
  {0x8b,0x0f,0x01}, // P 25
  {0x8a,0xc8,0x06}, // Q 26
  {0x8b,0x4f,0x09}, // R 27
  {0x0e,0x87,0x07}, // S 28
  {0x27,0x22,0x24}, // T 29
  {0x8d,0x88,0x8f}, // U 30
  {0x95,0xc2,0x08}, // V 31
  {0x8d,0xda,0x09}, // W 32
  {0x55,0x52,0x09}, // X 33
  {0x55,0x22,0x04}, // Y 34
  {0x47,0x12,0x0f}, // Z 35
  {0xff,0xff,0xff}, // Full on 36
  {0x00,0x00,0x00}  // Clear 37
};

// ==================== PWM Parameters ====================
const int PWM_FREQ = 13000;
const int PWM_DUTY = 128;

// ==================== Time and Date Variables ====================
volatile int hours = 0;
volatile int minutes = 0;
volatile int seconds = 0;
volatile int year = 0;    // Last two digits of year
volatile int month = 0;
volatile int day = 0;

volatile bool timeUpdated = false;    // Second change flag

// ==================== Display Mode ====================
enum DisplayMode { MODE_TIME, MODE_DATE };
DisplayMode currentMode = MODE_TIME;
DisplayMode pendingMode = MODE_TIME;
bool showingModeName = false;
unsigned long modeNameStartTime = 0;
const unsigned long MODE_NAME_DURATION = 1000;

// ==================== Brightness Control ====================
int brightnessLevel = BRIGHTNESS_MAX;
bool showingBrightness = false;
unsigned long brightnessStartTime = 0;
const unsigned long BRIGHTNESS_DURATION = 500;

// ==================== Icon Status ====================
volatile uint8_t iconMask = 0;

// ==================== NTP Settings ====================
const char* ntpServer1 = "ntp1.aliyun.com";
const char* ntpServer2 = "ntp2.aliyun.com";
const char* ntpServer3 = "ntp3.aliyun.com";
const long gmtOffset_sec = -5 * 3600;
const int daylightOffset_sec = 0;

// ==================== WiFi Status ====================
bool wifiConnected = false;
bool wifiConnecting = false;
WiFiManager wifiManager;

// ==================== Ticker Objects ====================
Ticker timeTicker;
Ticker ntpTicker;
Ticker wifiStateCheckTicker;
Ticker wifiConnectingTicker;        // Icon flashing during connection
Ticker configModeTicker;             // Display refresh in config mode

// ==================== Button Debounce ====================
unsigned long lastModePress = 0;
unsigned long lastBrightPress = 0;
const unsigned long debounceDelay = 200;

// ==================== Function Declarations ====================
void initPWM();
void initVFD();
void pt6315_write_byte(unsigned char data);
void vfd_clear();
void display_char(unsigned char pos, unsigned char data, unsigned char dot);
void display_icon(unsigned data);
void set_vfd_display_mode(unsigned char mode);
void set_vfd_data_mode(unsigned char mode);
void set_vfd_brightness(unsigned char level);
void displayTimeDate();
void displayModeName();
void displayBrightness();
void displayYES();
void displayFAIL();
void displayWiFi();                     // Added: display " WiFi "
void updateTime();
bool updateTimeFromNTP();
void checkWiFiState();
void updateIcons();
void wifiConnectingBlink();
void configModeDisplay();

// ==================== setup ====================
void setup() {
  // 1. Initialize PWM first
  pinMode(PWM_PIN, OUTPUT);
  initPWM();

  // 2. Setup other pins
  pinMode(GPIO15_PIN, OUTPUT);
  digitalWrite(GPIO15_PIN, HIGH);

  pinMode(MODE_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BRIGHT_BUTTON_PIN, INPUT_PULLUP);

  pinMode(PT6315_CS_PIN, OUTPUT);
  pinMode(PT6315_CLK_PIN, OUTPUT);
  pinMode(PT6315_DATA_PIN, OUTPUT);

  digitalWrite(PT6315_CS_PIN, HIGH);
  digitalWrite(PT6315_CLK_PIN, HIGH);
  digitalWrite(PT6315_DATA_PIN, HIGH);

  // 3. Initialize VFD
  initVFD();
  set_vfd_brightness(brightnessLevel);

  // 4. Initialize icons: 3D always on
  iconMask = DISPLAY_ICON_3D_ON;
  display_icon(iconMask);

  // 5. Full screen on for 2 seconds
  for (int i = 0; i < 6; i++) {
    display_char(i, 36, 0);
  }
  delay(5000);

  // 6. Display "HELLO "
  display_char(0, 'H', 0);
  display_char(1, 'E', 0);
  display_char(2, 'L', 0);
  display_char(3, 'L', 0);
  display_char(4, 'O', 0);
  display_char(5, 37, 0);
  delay(1000); // short pause

  // 7. Display " WiFi " and start connecting
  displayWiFi();

  // 8. WiFi connection process
  wifiManager.setDebugOutput(false);   // Disable library debug output
  wifiManager.setAPCallback(NULL);     // No callback needed
  wifiManager.setTimeout(20);           // 20 second timeout

  // Start connection blinking
  wifiConnecting = true;
  wifiConnectingTicker.attach_ms(500, wifiConnectingBlink);

  bool connected = wifiManager.autoConnect("VFD Clock");

  // Stop connection blinking
  wifiConnectingTicker.detach();
  wifiConnecting = false;

  if (connected) {
    // Connection successful
    wifiConnected = true;
    updateIcons();

    // Display " YES " for 2 seconds
    displayYES();
    delay(2000);

    // Get time from NTP
    updateTimeFromNTP();

    // First time display
    displayTimeDate();

    // Start normal tickers
    timeTicker.attach(1, updateTime);
    ntpTicker.attach(3600, []() {
      if (WiFi.status() == WL_CONNECTED) {
        updateTimeFromNTP();
      }
    });
    wifiStateCheckTicker.attach(10, checkWiFiState);
  } else {
    // Connection timeout, enter config mode
    wifiConnected = false;
    updateIcons();

    // Display " FAIL " and start config mode ticker
    displayFAIL();
    configModeTicker.attach_ms(500, configModeDisplay);

    // Enter WiFiManager configuration portal (AP mode)
    wifiManager.startConfigPortal("VFD Clock");

    // Reboot after configuration
    ESP.restart();
  }
}

// ==================== loop ====================
void loop() {
  unsigned long now = millis();

  // Mode toggle button
  if (digitalRead(MODE_BUTTON_PIN) == LOW && now - lastModePress > debounceDelay) {
    lastModePress = now;
    showingBrightness = false;
    pendingMode = (currentMode == MODE_TIME) ? MODE_DATE : MODE_TIME;
    showingModeName = true;
    modeNameStartTime = now;
    displayModeName();
    updateIcons();
  }

  // Brightness adjust button
  if (digitalRead(BRIGHT_BUTTON_PIN) == LOW && now - lastBrightPress > debounceDelay) {
    lastBrightPress = now;
    showingModeName = false;
    brightnessLevel++;
    if (brightnessLevel > BRIGHTNESS_MAX) brightnessLevel = BRIGHTNESS_MIN;
    set_vfd_brightness(brightnessLevel);
    showingBrightness = true;
    brightnessStartTime = now;
    displayBrightness();
  }

  // Check mode name display timeout
  if (showingModeName && now - modeNameStartTime >= MODE_NAME_DURATION) {
    showingModeName = false;
    currentMode = pendingMode;
    displayTimeDate();
    updateIcons();
  }

  // Check brightness display timeout
  if (showingBrightness && now - brightnessStartTime >= BRIGHTNESS_DURATION) {
    showingBrightness = false;
    displayTimeDate();
  }

  // Update display every second
  if (timeUpdated) {
    timeUpdated = false;
    if (!showingModeName && !showingBrightness) {
      displayTimeDate();
    }
    updateIcons();
  }
}

// ==================== Display "WiFi" ====================
void displayWiFi() {
  // Clear all digits
  for (int i = 0; i < 6; i++) {
    display_char(i, 37, 0);
  }
  // Display " WiFi " (space W i F i space)
  display_char(0, 37, 0);  // space
  display_char(1, 'W', 0);
  display_char(2, 'I', 0);
  display_char(3, 'F', 0);
  display_char(4, 'I', 0);
  display_char(5, 37, 0);  // space
}

// ==================== Display "SUCCES" ====================
void displayYES() {
  for (int i = 0; i < 6; i++) display_char(i, 37, 0);
  display_char(0, 'S', 0);
  display_char(1, 'U', 0);
  display_char(2, 'C', 0);
  display_char(3, 'C', 0);
  display_char(4, 'E', 0);
  display_char(5, 'S', 0);
}

// ==================== Display " FAIL " ====================
void displayFAIL() {
  for (int i = 0; i < 6; i++) display_char(i, 37, 0);
  display_char(0, 36, 0);
  display_char(1, 'F', 0);
  display_char(2, 'A', 0);
  display_char(3, 'I', 0);
  display_char(4, 'L', 0);
  display_char(5, 36, 0);
}

// ==================== Config mode display refresh ====================
void configModeDisplay() {
  static bool blink = false;
  blink = !blink;
  displayFAIL();  // Keep showing FAIL
  uint8_t icon = DISPLAY_ICON_3D_ON;
  if (blink) icon |= DISPLAY_ICON_WIFI_ON;
  display_icon(icon);
}

// ==================== The following functions remain unchanged ====================
void initPWM() {
  analogWriteFreq(PWM_FREQ);
  analogWrite(PWM_PIN, PWM_DUTY);
  delay(10);
}

void initVFD() {
  set_vfd_data_mode(CMD_WRITE_DATA_TO_DISPLAY_MODE | CMD_INCREMENT_ADDRESS_MODE | CMD_NORMAL_MODE);
  set_vfd_display_mode(CMD_ADDRESS_SET);
  set_vfd_display_mode(CMD_7DIG_21SEG);
  vfd_clear();
  delay(1);
}

void set_vfd_brightness(unsigned char level) {
  CLR_PT6315_CS;
  pt6315_write_byte(0x80 | 0x08 | (level & 0x07));
  SET_PT6315_CS;
}

void delay_func(void) {
  uint8_t i = 5;
  while (i--);
}

void pt6315_write_byte(unsigned char data) {
  for (int i = 0; i < 8; i++) {
    CLR_PT6315_CLK;
    if (data & 0x01) SET_PT6315_DATA;
    else CLR_PT6315_DATA;
    data >>= 1;
    delay_func();
    SET_PT6315_CLK;
    delay_func();
  }
}

void vfd_clear(void) {
  CLR_PT6315_CS;
  pt6315_write_byte(0xC0);
  for (int i = 0; i < 21; i++) pt6315_write_byte(0x00);
  SET_PT6315_CS;
}

void display_char(unsigned char pos, unsigned char data, unsigned char dot) {
  if (pos > 5) return;
  unsigned char buf[3];
  if (data >= '0' && data <= '9') {
    buf[0] = font[data - '0'][0];
    buf[1] = font[data - '0'][1];
    buf[2] = font[data - '0'][2];
  } else if (data >= 'A' && data <= 'Z') {
    buf[0] = font[data - 'A' + 10][0];
    buf[1] = font[data - 'A' + 10][1];
    buf[2] = font[data - 'A' + 10][2];
  } else if (data < '0') {
    buf[0] = font[data][0];
    buf[1] = font[data][1];
    buf[2] = font[data][2];
  } else return;

  if ((pos == 1 || pos == 3 || pos == 4) && dot) {
    buf[2] |= 0x10;
  }

  CLR_PT6315_CS;
  pt6315_write_byte(0xC0 | (pos * 3));
  pt6315_write_byte(buf[0]);
  pt6315_write_byte(buf[1]);
  pt6315_write_byte(buf[2]);
  SET_PT6315_CS;
}

void display_icon(unsigned data) {
  CLR_PT6315_CS;
  pt6315_write_byte(0xD2);
  pt6315_write_byte(0x00);
  pt6315_write_byte(data);
  pt6315_write_byte(0x00);
  SET_PT6315_CS;
}

void set_vfd_display_mode(unsigned char mode) {
  CLR_PT6315_CS;
  pt6315_write_byte(mode);
  SET_PT6315_CS;
}

void set_vfd_data_mode(unsigned char mode) {
  CLR_PT6315_CS;
  pt6315_write_byte(0x40 | mode);
  SET_PT6315_CS;
}

void displayTimeDate() {
  if (currentMode == MODE_TIME) {
    int h1 = hours / 10, h2 = hours % 10;
    int m1 = minutes / 10, m2 = minutes % 10;
    int s1 = seconds / 10, s2 = seconds % 10;
    display_char(0, '0' + h1, 0);
    display_char(1, '0' + h2, (seconds % 2) ? 1 : 0);
    display_char(2, '0' + m1, 0);
    display_char(3, '0' + m2, (seconds % 2) ? 1 : 0);
    display_char(4, '0' + s1, 0);
    display_char(5, '0' + s2, 0);
  } else {
    int y1 = year / 10, y2 = year % 10;
    int m1 = month / 10, m2 = month % 10;
    int d1 = day / 10, d2 = day % 10;
    display_char(0, '0' + y1, 0);
    display_char(1, '0' + y2, 1);
    display_char(2, '0' + m1, 0);
    display_char(3, '0' + m2, 1);
    display_char(4, '0' + d1, 0);
    display_char(5, '0' + d2, 0);
  }
}

void displayModeName() {
  for (int i = 0; i < 6; i++) display_char(i, 37, 0);
  if (pendingMode == MODE_TIME) {
    display_char(0, 37, 0);
    display_char(1, 'T', 0);
    display_char(2, 'I', 0);
    display_char(3, 'M', 0);
    display_char(4, 'E', 0);
    display_char(5, 37, 0);
  } else {
    display_char(0, 37, 0);
    display_char(1, 'D', 0);
    display_char(2, 'E', 0);
    display_char(3, 'T', 0);
    display_char(4, 'A', 0);
    display_char(5, 37, 0);
  }
}

void displayBrightness() {
  int level = brightnessLevel + 1;
  for (int i = 0; i < 6; i++) display_char(i, 37, 0);
  display_char(2, '0' + (level / 10), 0);
  display_char(3, '0' + (level % 10), 0);
}

void updateTime() {
  seconds++;
  if (seconds >= 60) {
    seconds = 0;
    minutes++;
    if (minutes >= 60) {
      minutes = 0;
      hours++;
      if (hours >= 24) hours = 0;
    }
  }
  timeUpdated = true;
}

void updateIcons() {
  uint8_t newMask = DISPLAY_ICON_3D_ON;

  if (wifiConnecting) {
    // Connecting, controlled by separate ticker
  } else {
    if (wifiConnected) {
      newMask |= DISPLAY_ICON_WIFI_ON;
    } else {
      if (seconds % 2 == 0) {
        newMask |= DISPLAY_ICON_WIFI_ON;
      }
    }
  }

  if (currentMode == MODE_TIME && !showingModeName) {
    newMask |= DISPLAY_ICON_CLOCK;
  }

  if (currentMode == MODE_TIME && !showingModeName && seconds % 2 == 0) {
    newMask |= DISPLAY_ICON_REC;
  }

  if (newMask != iconMask) {
    iconMask = newMask;
    display_icon(iconMask);
  }
}

void wifiConnectingBlink() {
  static bool blink = false;
  blink = !blink;
  uint8_t temp = DISPLAY_ICON_3D_ON;
  if (blink) temp |= DISPLAY_ICON_WIFI_ON;
  display_icon(temp);
}

void checkWiFiState() {
  bool currentStatus = (WiFi.status() == WL_CONNECTED);
  if (currentStatus != wifiConnected) {
    wifiConnected = currentStatus;
    updateIcons();
  }
}

bool updateTimeFromNTP() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return false;
  }
  hours = timeinfo.tm_hour;
  minutes = timeinfo.tm_min;
  seconds = timeinfo.tm_sec;
  year = timeinfo.tm_year % 100;
  month = timeinfo.tm_mon + 1;
  day = timeinfo.tm_mday;
  return true;
}