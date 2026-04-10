# The-Cyberpunk-Aesthetic-of-VFD
The fluorescent glow emitted by VFD displays possesses such a distinct cyber-futuristic vibe. As you can see, this type of display differs significantly from the common LED screens we encounter daily, and its operating principle is equally fascinating.![image](images/2.jpg)

You can conceptualize it as a miniaturized Cathode Ray Tube (CRT), with its core mechanism relying on thermionic emission to excite phosphors. If you observe closely, you will notice several extremely fine tungsten filaments at the very top. These filaments are coated with oxides of barium, strontium, and calcium.![image](images/6.jpg)

When heated to 600–650°C, the thermal energy allows electrons on the cathode surface to gain sufficient energy, forming an "electron cloud" around the filaments. To prevent oxidation of the tungsten, the interior of the VFD is maintained in a high-vacuum state.

Situated beneath the filaments is a mesh-like grid, which functions effectively as a switch. When a positive voltage is applied, it attracts and accelerates the electrons emitted by the filaments, allowing them to pass through; conversely, a negative voltage repels them.

Beneath each phosphor segment lies a tiny anode. When a positive voltage is applied here, electrons surge through the grid "switch" and violently bombard the phosphor on the anode. Upon absorbing this energy, the phosphors release their captivating colors, exuding a unique charm of retro-futurism.![image](images/8.jpg)
#Note! This screen emits a slight sound during operation. This is due to the AC filament scheme used to ensure uniform and vibrant display; the noise comes from filament oscillation.

You can adjust the PWM frequency in the code to find the most suitable and quietest setting for your environment. Caution: Do not set the PWM frequency too high, or it will burn out the filament; too low, and the display will appear dim.
This screen uses an ESP-07S microcontroller, and the Arduino IDE is recommended for the development environment. The board reserves two push-button switches and one LED (active low) for your own development.
You can directly flash the test code and develop custom programs based on it. A ready-made clock program is also provided. Before using the clock code that connects to Wi-Fi, you need to import the WiFiManager library to ensure it compiles and flashes correctly.
