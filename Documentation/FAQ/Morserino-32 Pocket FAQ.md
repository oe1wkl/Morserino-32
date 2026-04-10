# Morserino-32 Pocket FAQ

This FAQ is compiled from the official user manual and recent community discussions on groups.io.

## 1. Power Supply, Battery, and Charging
* **What type of battery should I use?**
    * Use a **14500 Li-Ion (3.7V) cell**. 14500 cells have the same form factor as common AA batteries, but we need 3.6-3.7 V cells, i.e. Li-Ion. The maximum size is a length of 52mm and a width of 14.5 mm.
    * **Recommendation**: Use a **protected cell** (one with built-in protection against deep discharge) to prevent early battery death. Community members frequently recommend high-quality brands like Keeppower (800mAh) or XTar. 
* **Where can I buy a 14500 battery in the US?**
    * While not explicitly in the manual, community discussions suggest retailers like Amazon (brands such as JESSPOW) or specialized battery shops.
* **Do I need a specific USB-C charger?**
    * No. Any normal **5V USB charger** (like a standard phone charger) will work. 
    * The M32 Pocket consumes a maximum of **500 - 600 mA** when charging the battery.
* **How do I charge the battery?**
    * Connect the USB-C cable and ensure the **Power Switch is in the ON (I) position**. The battery will **not** charge if the hardware switch is OFF.
    * **Status Indication**: As of firmware version 8, The M32 Pocket displays a battery/charging icon on the top line of the display whenever you are in a menu.
* **Low Battery "Gotcha"**: If the battery voltage is dangerously low, an empty battery symbol will appear and the device will not boot. Symptoms like loss of audio during keying suggest that the battery voltage is too low and the battery needs recharging.

## 2. Cases and 3D Printing
* **Where can I find 3D printing files?**
    * While the manual doesn't list URLs, community members point to **Printables** (e.g., Model 1550518 by Michael K Johnson/KZ4LY) for FreeCAD, STEP, and STL files for the M32 Pocket.
* **How do I move the sensor pads to a new case?**
    * The touch paddles are **capacitive** and made from single-sided PCB material. 
    * **Process**: The PCB material is glued to the case - a bit of acetone can be used to weaken the glue so that you can carefully remove the PCBs from the original case and attach them to the new one. Be careful to reconnect the wires in the same way as they were with the old case.
* **The "Missing" Red Button**: On the M32 Pocket, the "red button" (FN button) is **integrated into the case** rather than being a separate protruding part. It is a cut-out in the case, located to the lower right of the rotary encoder.


## 3. External Keys and Connections
* **Which jack do I use for external keys?**
    * Use the **3.5 mm 3-pole (TRS) jack** labeled "External Paddle". It is the jack closest to the USB connector.
* **Wiring Guide**:
    * **Dual-Lever Paddle**: Tip = Left, Ring = Right, Sleeve = Ground.
    * **Straight Key**: Tip = Key, Sleeve = Ground.
* **Configuration**: To use a straight key or a bug in Echo Trainer or Transceiver modes, you must change the **Keyer Mode** preference to **Straight Key**.

## 4. Audio Input and Output
* **What is the pinout for the Audio I/O jack?**
    * The M32 Pocket uses a **3.5 mm 4-pole (TRRS) jack**.
    * **M32 Pocket Pinout**: Tip and 1st Ring = Audio/Headphones Out; 2nd Ring = Ground; **Sleeve = Audio In**.
    * **Note**: This is different from the 1st/2nd edition Morserinos where the sleeve was audio out.

* **How to get an audio signal from a transceiver into the M32Pocket for CW decoding?**
	* First of all, you need a splitter audio jack Y Adapter Cable Stereo - **1x 3.5 mm Plug CTIA 4-Pin TRRS to 2x 3.5 mm Female**. 
	* Plug in the 4-male plug into the M32/P  headset socket
and plug your headphones into the green socket of your splitter cable.
Turn on your Receiver and tune in a good, clean CW station. Turn the volume down to very low! It is crucial not to override the M32/p input when decoding.
	* Now connect your TRX output (for headphones or external speaker) with a 3,5mm cable to the pink female socket of the splitter cable.  The TRX will silence now (of course).
	* 
Set M32/p to decoder-mode. You will hear *nothing* in your headphone. WSte teh volume on the M32  1/3 approx. 
	* Slowly turn the TRX volume up. When M32/p detects a decodable audiosignal it will begin to work. With every detected dit or dah you will hear a beep. And simultaneously the M32Pocket will show a little black square in the upper left corner of the display (blinking).
	* Be careful not to overload the input, as in that case decoding stops and no sound is sent to your headset. 
 
* **How do I adjust audio input levels?**
    * Use the **FN button** to toggle between speed and volume control while a mode is active.
    * For the M32 Pocket, a long press of the FN button in a menu keys the transmitter/sidetone, which can be used to set levels on connected external devices.

## 5. Bluetooth and VBand
* **How to enable Bluetooth?**
	* Use the preference **BLT Kbd Output**. 
* **How do I connect to VBand?**
    * The Morserino can connect to **VBand** (hamradio.solutions/vband) via Bluetooth. 
    * Community tips suggest ensuring your browser supports the necessary protocols and setting the VBand input to "Straight Key" if your paddles aren't recognized correctly.
* **Bluetooth Keyboard**: The M32 Pocket can act as a **Bluetooth keyboard**. Decoded characters are sent as if typed on a US QWERTY keyboard.

* **Can I use Bluetooth to stream audio?**
	* No, this is not possible. It would also not be useful, as Bluetooth audio introduces a noticeable delay, which makes correct keying very hard.


## 6. Firmware Updates
* **What is the easiest update method?**
    * **Webserial**: Using **Google Chrome** or **Microsoft Edge** to visit `morserino.info` is the easiest method as it requires no command-line tools or firmware downloads.
* **Major "Gotchas"**:
	* **Use the correct firmware flasher:** On `morserino.info` there are links to two different instances of the firmware flasher, one for the older Morserino-.32 versions, and one for the M32 Pocket. 


    * **Stay Awake**: The M32 Pocket **must be turned ON** and **must not be in sleep mode** when connecting for an update.
    * **Cable Quality**: Ensure you use a **data-capable USB cable**, not just a "charging-only" cable.