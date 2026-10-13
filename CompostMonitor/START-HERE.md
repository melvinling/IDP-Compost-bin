# ESP32 local compost monitor: automatic direct reads

Open **CompostMonitor.ino** in Arduino IDE. Keep **Dashboard.h** and
**ModbusProtocol.h** beside it. No extra Arduino library installation is needed.

The main program now reads all sensors automatically after boot. No scan or
isolated setup step is needed. Both RS485 sensors stay connected. It sends only
holding-register reads and never writes sensor settings or controls relays.

## Current settings

| Sensor | Modbus ID | Baud | Format | Measurements |
| --- | --- | --- | --- | --- |
| SN-3001-TR-ECPH-N01 | 1 | 4800 | 8N1 | EC 0x0002, pH 0x0003 / 10 |
| XM7870B | 1 | 9600 | 8N1 | CO2 ppm 0x0000 |

These are documented **factory defaults**, not verified installed settings.
The ESP32 changes its own UART speed for each sensor; sensor configuration is
untouched. The soil registers are read separately so an EC failure does not
hide a valid pH reply. Soil ECPH supplies EC/pH; temperature and moisture are
not added for this model.

Different baud rates on one shared bus are a diagnostic arrangement. Devices
can receive unintelligible traffic sent at the other speed. Verify actual
replies before relying on readings. Sensors with changed settings require
editing **SOIL_ADDRESS, SOIL_BAUD, CO2_ADDRESS, CO2_BAUD** near the top of the
sketch. Two devices at the same address and baud cannot be identified reliably.

## Wiring reference

Use your actual wiring, which you have reported as confirmed. The previous
hand-drawn image is not used as a verified wiring record here.

| Connection | Destination |
| --- | --- |
| ESP32 GPIO17 (UART TX) | XY-485 TTL input from ESP32 |
| ESP32 GPIO16 (UART RX) | XY-485 TTL output to ESP32 |
| XY-485 GND | ESP32 GND / common supply negative |
| Both sensors A | Adapter A+ |
| Both sensors B | Adapter B- |
| Both sensors supply positive / negative | Confirmed 12 V supply / negative |
| A0221AM red / black | 3.3 V / GND |
| A0221AM yellow / white | GPIO27 trigger / GPIO35 PWM output |

You reported adapter RXD to GPIO16 and TXD to GPIO17, adapter VCC at 5 V,
and board plus RS485 sensors powered by 12 V. The module's TTL output into
GPIO16 must still be 3.3 V compatible. The software cannot verify voltage
levels, module pin direction, common ground or actual sensor power.

The board's 12 V supply connects to its DC input, never an ESP32 GPIO. The
USB programming adapter stays on GPIO1/3 and GND; leave its VCC disconnected.
This firmware expects an auto-direction RS485 adapter. GPIO32/33/25/26 are
onboard relay pins; GPIO16/17 and 27/35 are used for sensors.
Switch off supplies before changing any wiring.

## Upload yourself

1. Open **CompostMonitor.ino** in Arduino IDE.
2. Select **ESP32 Dev Module** and **COM8** (or your current adapter port).
3. Click **Upload**. If Connecting persists, hold IO0, press/release EN,
   then release IO0 when writing begins.
4. Open Serial Monitor at **115200 baud**. Release IO0 and press EN alone.
5. Confirm **COMPOST MONITOR READY - RX diagnostic v5** appears, followed by
   soil ID1/4800 and CO2 ID1/9600. Reads start automatically after one second.

Expected log structure (values depend on real replies):

```text
RS485 soil EC | ID=1 baud=4800 | OK raw=...
RS485 soil pH | ID=1 baud=4800 | OK raw=...
RS485 CO2 | ID=1 baud=9600 | OK raw=...
Sample ... | distance ... | EC ... | pH ... | CO2 ...
```

Missing data prints **unavailable**. Each failed read reports no received
bytes, an exception, or bytes without a matching CRC-valid reply. Old setup
flags saved by earlier firmware do not prevent automatic reads. Each reboot
starts monitoring again; a pause is not stored across resets.

## Open the live graphs

1. Join Wi-Fi **CompostBin**, password **compost123**.
2. Keep the connection when your device reports no internet.
3. Open **http://192.168.4.1/** including http://.

The page contains distance, EC, pH and CO2 graphs. It works offline with no
router, account or CDN. Sampling is scheduled every **2 seconds**, whether
or not a browser is connected. The three Modbus reads each time out after
600 ms; a missing device does not prevent the next sensor being attempted.
Valid replies can arrive much sooner. Browser requests are serviced between
transaction checks.

Each graph has its own status. Missing readings are JSON null and graph gaps;
real sensor zeros remain zeros. pH outside 0-14 is rejected. Current readings
are cleared at every new sample so failures do not reuse old measurements.
Ultrasonic no-signal/out-of-range results also remain unavailable.

Up to **900 samples (about 30 minutes)** are kept in ESP32 RAM; older samples
are overwritten and reboot clears history. CSV exports the browser's loaded
history with blanks for missing values. Time labels use the viewing device's
clock; CSV timestamps are UTC. This is not permanent logging or internet time
synchronization. Pause display affects only the web display, not sensing.

## Optional commands

Serial Monitor at 115200 baud, **Newline**. None is needed to start.

| Command | Effect |
| --- | --- |
| STOP MONITORING | Pause only RS485 reads; ultrasonic and dashboard continue |
| START MONITORING | Resume direct reads at the selected sensor settings |
| TRACE ON | Include transmitted frames and raw received bytes in normal read logs |
| TRACE OFF | Return to compact normal read logs |
| RX INVERTED | Diagnostic: invert UART receive polarity only; TX stays normal |
| RX NORMAL | Restore normal UART receive polarity (also restored on reboot) |
| SCAN BUS | Optional read-only diagnostic at IDs 1-10 and six baud rates |
| SCAN BUS FULL | Optional read-only diagnostic at IDs 1-247, about 30 min with no replies |
| STOP SCAN | Cancel an optional scan |

During scanning, normal RS485 reads pause and ultrasonic sampling continues.
After the scan ends/cancels, direct monitoring resumes if it was running
before the scan. A previously paused monitor stays paused. Other commands are
rejected while scanning. Scanned measurements appear in Serial Monitor; the
graphs show measurements from normal sampling. Legacy SETUP ... ONLY and
CHECK ... ONLY commands have been removed; there are no sensor-setting writes.

## Test the captured inverted echoes

The supplied v4 trace repeatedly showed these RX bytes:

| Request | Received bytes |
| --- | --- |
| Soil EC | 7F 7E FB FF FD B5 6B |
| Soil pH | 7F 7E F9 FF FD 17 EB 00 |
| CO2 | 7F 7E FF FF FD F7 3D |

All three patterns exactly match a simulated inverted 8N1 waveform of the
corresponding transmitted request, decoded by a normal UART receiver. This
strongly suggests an inverted echo in the receive path. It does not establish
which hardware stage causes it, nor prove a sensor response has arrived.

After uploading v5 yourself, send **TRACE ON**, then **RX INVERTED** (115200
baud, Newline). Wait 10 seconds and copy CHECK, TX, RS485/RX, ULTRASONIC and
Sample lines. Then send **RX NORMAL** to restore the original setting.

If RX now repeats the exact eight TX bytes, that is a request echo, not a
measurement. Only a separate matching CRC-valid response is accepted. If real
replies appear, the test has confirmed a receive-polarity dependency. No A/B
wiring change, sensor setting change or new scan is performed by this command.
The UART now changes speed without repeatedly deleting/restarting its driver,
so the selected receive polarity survives changes between sensor baud rates.
Under TRACE ON, the ultrasonic raw pulse width is also printed: zero means no
complete pulse was captured. This diagnostic does not establish the cause of
the ultrasonic failure.

No-RX is not proof of a defective sensor. The earlier scans received zero
bytes: this simpler program tests direct measurement registers at the defaults,
but cannot fix an electrical connection or sensor configuration problem.

## Sources

- [Soil manufacturer's SN-3001 family manual](http://save.sdvms.com/产品说明书/土壤/五插针土壤变送器/威盟士五插针土壤四参数变送器（485型）.pdf):
  ECPH/N01 selection, Modbus RTU, default ID1/4800/8N1, EC/pH registers.
  A downloaded reference is in ../.build/references/Soil-SN-3001.pdf.
- [XUNCHIP XM7870 manufacturer manual, mirrored copy](https://device.report/m/6f619498612c092b25158f32926c7f0c665bd09fd7b2e63bb83ef2334453e64b):
  XM7870B RS485/Modbus RTU, default ID1/9600/8N1, register 0 CO2 ppm.
- [DYP A02 PWM interface](https://www.dypcn.com/uploads/A02-Output-Interfaces.pdf):
  falling-edge trigger, distance_cm = pulse_us / 57.5, period greater than 70 ms.
- [Espressif Wi-Fi API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html).
- [Modbus serial specification](https://www.modbus.org/file/secure/modbusoverserial.pdf).

## Validation

Compilation passed using ESP32 Arduino core **3.3.12**, ESP32 Dev Module.
v5 compilation uses **949,148 bytes flash (72%)** and **76,520 bytes global RAM
(23%)**. No ESP32 upload was performed.

Simulated firmware tests passed for automatic direct reads, correct IDs,
registers and baud order, echo/noise parsing, bounded timeouts, clearing stale
readings, partial soil replies, pH range checking and pause without bus requests.
Dashboard tests passed for live values, missing versus zero, independent EC/pH
statuses, four graphs, CSV, incremental polling, duplicate prevention, pause,
reconnection, reboot handling and phone layout, with no browser errors.

These checks do not establish live sensor communication or calibration.
No ESP32 upload was performed; upload and check actual results yourself.

The v5 change is checked by compilation and protocol tests that reject both
the captured inverted echoes and normal request echoes. The existing simulated
polling harness depends on the separate RS485AutoScan test stub, which is no
longer present locally; its earlier results do not validate this revision.
