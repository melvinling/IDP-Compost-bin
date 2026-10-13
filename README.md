# ESP32 compost bin programs

The latest Bokashi firmware and local simulator are in **Monitoring**. Read
[Monitoring/README.md](Monitoring/README.md). Double-click
`Monitoring/Start Simulator.cmd` to start the virtual bin, then open
http://127.0.0.1:8787/. The earlier sketches below remain reference programs.

For a scan that starts automatically with no Serial Monitor commands, open
`RS485AutoScan/RS485AutoScan.ino`. Read `RS485AutoScan/START-HERE.md` and
upload it yourself. Both RS485 sensors stay connected; the full scan is read-only.

The main Wi-Fi dashboard is in `CompostMonitor/CompostMonitor.ino`.
It automatically reads soil ID1 at 4800 baud and CO2 ID1 at 9600 baud,
with both sensors connected. No scan or isolated setup command is needed.
Read `CompostMonitor/START-HERE.md` for upload, wiring and Wi-Fi access.
Its companion headers must stay in the same folder. Upload it yourself.

## First ESP32 compost bin test

Open `CompostBinTest/CompostBinTest.ino` in Arduino IDE. The program prints
`Compost bin ready` once per second at **115200 baud**. It does not configure
sensor pins or switch relays.

## 1. Set up Arduino IDE

This computer already has **Arduino IDE 2.3.9** installed at
`C:\Users\Acer\OneDrive\Desktop\Arduino IDE\Arduino IDE.exe`, and
**esp32 by Espressif Systems 3.3.12** installed. Open that IDE and continue
with board and port selection (steps 4 and 5). The installation steps below
are included for reference.

1. Install Arduino IDE from https://www.arduino.cc/en/software/.
2. In **File > Preferences > Additional Boards Manager URLs**, add:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`.
3. Open **Boards Manager**, search for `esp32`, and install
   **esp32 by Espressif Systems**.
4. If the ESP32 module is marked **ESP32-WROOM-32** or **ESP32-WROOM-32E**,
   select **Tools > Board > esp32 > ESP32 Dev Module**.
5. Select your USB-to-UART adapter under **Tools > Port**.
   If several ports appear, compare the list before and after plugging in
   the adapter. If none appears, install the adapter manufacturer's driver.

Official setup guide:
https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html

## 2. Connect the board

Use a USB-to-UART adapter with **3.3 V TX/RX signal levels**. An adapter's
power-output voltage setting does not necessarily change its TX/RX levels;
check its documentation.

| USB-to-UART adapter | ESP32 programming connection |
| --- | --- |
| TX | RX / GPIO3 |
| RX | TX / GPIO1 |
| GND | GND |

Leave the adapter's **VCC/power pin disconnected**. Power the relay board
through its documented low-voltage DC input, using the supply voltage stated
for that input. The board's exact supply requirements have not been verified
from the supplied component list. Verify them before applying power.

Keep sensors and motor/actuator loads disconnected during this first test.

## 3. Compile, upload, and run

1. Click **Verify** in Arduino IDE to compile the sketch.
2. With GPIO0 connected to GND, reset the ESP32 to enter upload mode.
   A **BOOT/FLASH** button, if fitted, may provide the GPIO0 connection.
   Reset using **EN/RESET**, if fitted, or cycle the board's DC power.
3. Click **Upload**. Keep GPIO0 low until the upload connects.
4. After upload completes, release BOOT/remove the GPIO0-to-GND jumper
   and reset the board again to run the sketch.
5. Open **Tools > Serial Monitor** and select **115200 baud**.

Expected output:

```text
Compost bin ready
Compost bin ready
Compost bin ready
```

Success means one line appears about every second for at least 10 seconds,
then continues after you reset the board.

If upload stays on `Connecting...`, check the selected COM port, crossed
TX/RX wiring, common GND, board power, and GPIO0 being low during reset.
If the output is unreadable, confirm Serial Monitor uses 115200 baud.
If the program does not run after upload, remove the GPIO0-to-GND connection
and reset again.

Official boot-mode guide:
https://docs.espressif.com/projects/esptool/en/latest/esp32/advanced-topics/boot-mode-selection.html

## Next step

After this test works, connect one sensor using its confirmed interface,
voltage requirements, and pin assignment. The earlier proposed sensor GPIOs
are not a verified wiring diagram for this relay board.
