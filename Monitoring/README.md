# Bokashi bin: ESP32 firmware and simulator

This is the main implementation for `Monitoring.ino`: six measurements, a drying
fan on logical relay 1, water pump on relay 2, and mixer motor on relay 3. A local
browser simulator runs the same C++ controller and Random Forest predictor.
Sibling sketches remain separate references.

The firmware boots in **manual / fermentation**, with outputs OFF. Physical
outputs are disabled until actual GPIO mapping and active levels are confirmed.
The checked-in model was trained on artificial data and is a demonstration.
No firmware upload or physical operation was performed.

## Run the simulator

Double-click **Start Simulator.cmd**, or run from this folder:

```powershell
.\simulator\start.ps1
```

The launcher opens your default browser at **http://127.0.0.1:8787/** when ready.
Keep the terminal open; Ctrl+C stops the server. Launching again reuses the existing
bin. Add `-NoBrowser` to skip browser opening, or `-Port 8788` for another port.
The launcher compiles the C++ engine using `g++`, then starts Node.js. Both are
installed here. No npm packages, internet, Python or ESP32 are needed to run it.

Choose a scenario, then Play or Step. The app supports all six sensor inputs,
individual missing flags, frozen acquisition, 1×/10×/60× time, actuator controls,
event log, charts and CSV. [Simulator instructions](simulator/README.md) explain
scenarios, timing and the illustrative physics.

## ESP32 compile and dashboard

Open **Monitoring.ino** in Arduino IDE with `src` intact. Select **ESP32 Dev
Module** (`esp32:esp32:esp32`). The verified toolchain uses Espressif core 3.3.12.
Or compile from this folder:

```powershell
.\tools\compile.ps1
```

After checking the actual wiring and relay configuration, upload manually using
the selected USB-to-UART port. This project has no automatic upload script.
Open Serial Monitor at 115200 baud. Join **CompostControl**, password
**compost123**, and open **http://192.168.4.1/**. Edit the credentials in
`src/Config.h` before commissioning. The offline dashboard needs no router/CDN.

## Controls and process stages

| Behavior | Setting |
| --- | --- |
| Boot / reset bin | Manual, fermentation, outputs OFF |
| Fermentation | All actuators inhibited, including manual starts |
| Finishing | Selected explicitly; enables eligible manual/automatic requests |
| Manual starts | Five seconds; retriggering never resets the continuous limit |
| Automatic water / dry / mix | 2 / 8 / 5-second pulses |
| Pump / fan / motor cooldown | 60 / 30 / 60 seconds after turning OFF |
| Mixing | Requested after completed watering, or every 30 minutes in finishing |
| Automatic eligibility | Ready prediction for current context; all six valid inputs younger than 12 seconds |
| Interlock | Fan and water pump cannot be commanded ON together |
| Continuous limit | At 10 seconds, latch all outputs OFF |
| Individual Stop | Inhibits that automatic channel until mode explicitly reselected |
| Stop All | Latches OFF and switches to manual |
| Reset stop latch | Clears latch, leaves manual/OFF; cooldowns remain |
| Mode / stage change | Immediately OFF; prior inference invalidated |

Stop commands take precedence over predictions. Automatic pulses can end early
when inputs become stale, predictions change, or the operator changes state.
Context changes wait for the next completed sensor snapshot before automatic
operation can resume. Completing a mixer pulse resets its periodic timer.
Timer comparisons tolerate the 32-bit millis rollover.

The controller checks every 20 ms in a dedicated FreeRTOS task. Sensor acquisition,
model inference, JSON generation and HTTP sends do not hold its mutex. Only the
relay driver writes hardware pins. Status describes **commanded outputs**; no
feedback confirms fan rotation, water delivery, mixing or contact state.

## Wiring and profiles

| Connection | Configuration |
| --- | --- |
| Ultrasonic A0221AM trigger / PWM | GPIO27 / GPIO35 input; falling-edge trigger |
| RS485 UART TX / RX | GPIO17 / GPIO16 |
| Soil, default four-parameter profile | Assumed SN-3001-TR-ECTHPH-N01; ID 1, 4800 baud, 8N1 |
| Soil moisture | Holding register 0x0000, raw / 10, 0–100% |
| Soil temperature | 0x0001, signed int16 / 10, −40–80°C |
| Soil EC | 0x0002, raw µS/cm |
| Soil pH | 0x0003, raw / 10, 0–14 |
| CO₂, XM7870B | ID 1, 9600 baud, register 0x0000, raw ppm |
| Fan / water pump / motor | Logical relays 1 / 2 / 3; actual GPIO and polarity unconfirmed |

Confirm the soil probe's actual model/register map. Select `SoilProfile::EcPh` in
`src/Config.h` for an EC/pH-only probe; moisture and temperature then report
unsupported and automatic control remains inhibited. Values are never invented
to fill missing hardware inputs.

Acquisition starts every four seconds. Ultrasound is bounded to 65 ms; each
single-register RS485 read has a 600 ms response deadline and a 25 ms settling
interval. Five transactions fit within the acquisition period even on timeouts.
Readings fail independently; stale/invalid values serialize as null, valid zeros
remain numeric. The protocol checks address/function/byte count/CRC, rejects
request echoes, and makes only function-03 reads. No sensor configuration writes
or automatic scans are performed.

The retained same-ID/different-baud bus is a diagnostic arrangement. Verify the
sensors independently; a normal shared bus requires compatible serial settings
and unique addresses. CRC alone cannot establish sensor identity on an ambiguous
bus. Confirm converter direction, common ground, A/B, sensor supplies and ESP32
3.3 V signal compatibility. The reported 12 V belongs at the board's documented
DC input, never a GPIO; the reported 5 V converter supply does not prove safe
TTL output levels.

For receive diagnosis, set Serial Monitor to Newline and send `TRACE ON`,
`RX INVERTED`, `RX NORMAL`, or `TRACE OFF`. RX changes occur at sample boundaries
and do not alter TX or sensor settings. Trace output contains request bytes and
up to 64 received bytes. EC request `01 03 00 02 00 01 25 CA` returned unchanged is
an echo; a valid read response has `01 03 02 HH LL CRClo CRChi` with matching CRC.
RX inversion remains a diagnostic option, not a verified electrical fix.

### Configure the three relays

`ACTUATORS_ENABLED` defaults false, all relay pins −1, and polarities unconfigured.
The physical firmware never initializes or writes those relay pins in this state.
After verifying actual channels, OFF levels, contact/load ratings and protection,
edit these values in `src/Config.h`:

```cpp
constexpr bool ACTUATORS_ENABLED = true;
constexpr int FAN_RELAY = /* verified relay 1 GPIO */;
constexpr int PUMP_RELAY = /* verified relay 2 GPIO */;
constexpr int MOTOR_RELAY = /* verified relay 3 GPIO */;
constexpr Polarity FAN_POLARITY = /* ActiveHigh or ActiveLow */;
constexpr Polarity PUMP_POLARITY = /* ActiveHigh or ActiveLow */;
constexpr Polarity MOTOR_POLARITY = /* ActiveHigh or ActiveLow */;
```

All three mappings must be configured and distinct. The validator rejects sensor,
programming, flash, input-only and strapping pins. Allowed classic ESP32 pins are
18/19/21/22/23/25/26/32/33, excluding any configured sensor pin. The driver sets OFF
latches before enabling the three outputs. Verify disconnected loads first; code
cannot guarantee pre-initialization electrical behavior or replace load isolation.

## Machine learning

`ml/train.py` trains a seed-42 Random Forest with 32 trees and depth 6 on 900 of
1,200 dummy examples, with 300 held out by artificial scenario group. Its 99.67%
synthetic holdout accuracy measures imitation of the dummy labels. A decision tree
and the label-rule baseline both scored 100%; these results do not establish RF
as the best algorithm for real compost.

Inputs are moisture, temperature, EC, pH, CO₂, distance, stage and two mixing flags.
Outputs are idle, water, dry or mix. `src/GeneratedForest.h` is the deployment
artifact. Python is unnecessary on ESP32 and in the simulator. The header's
probabilities match sklearn on 4,519 parity cases. Confidence is uncalibrated.
The controller applies independent timing and inhibition rules to model requests.

The artificial labels prioritize fermentation idle, post-water mix, below 40%
water, above 60% dry, periodic mix, then idle. These moisture thresholds require
probe/material calibration. The other measurements do not determine dummy labels.
See [model details](ml/README.md), [recorded scores](ml/report.json) and
[dataset definitions](data/README.md). Collect real independent batches and
reviewed actions/outcomes before evaluating physical automatic control.

## Modules

| Files | Responsibility |
| --- | --- |
| `Sensors.cpp`, `ModbusProtocol.h` | Bounded physical acquisition and frame validation |
| `Types.h`, `Config.h` | Shared data, profiles, timing and configuration |
| `Prediction.cpp`, `GeneratedForest.h` | Pure model inference, no hardware writes |
| `Controller.cpp` | Authoritative stage/mode, timing, stops, freshness and interlocks |
| `RelayDriver.cpp`, `RelayConfiguration.h` | Verified physical output mapping and GPIO writes |
| `Application.cpp` | Coordination, per-snapshot prediction and FreeRTOS safety task |
| `History.cpp`, `Api.cpp`, `WebServerModule.cpp` | Bounded RAM history and acknowledged HTTP controls |
| `web/` | Offline dashboard, six charts and CSV |
| `simulator/` | Native virtual clock/response and loopback Node server |
| `ml/`, `data/` | Reproducible training, synthetic provenance and probability fixtures |

## API and web edits

`GET /api/data?after=<sequence>&boot=<id>` uses schemaVersion **2**. `sampleMs` and
`freshMs` communicate timing. `latest.readings` order is distance, EC, pH, CO₂,
moisture, temperature. Each reading includes value, valid, status, ageMs,
lastSuccessMs and exceptionCode. History rows append the new fields:

```text
[sequence, uptime_ms, distance_cm, ec_us_cm, ph, co2_ppm, moisture_pct, temperature_c]
```

Responses page at 60 rows, with next/more/reset. History is capped at 900 rows
(about one hour), clears on reboot, and has no persistent storage. CSV timestamps
use the viewing device's clock; no RTC synchronization is implemented.

Status adds stage, elapsed time, mixing flags, per-actuator cooldowns, fan requests
and commands, prediction action/confidence/model/provenance. Display pause keeps
polling and commands active. Disconnection retains history and disables controls.

`POST /api/control` accepts exactly one `command` form field, content type
`application/x-www-form-urlencoded`, with the current data response's
`X-Control-Token`. Commands:

```text
mode_manual, mode_auto, stage_fermentation, stage_finishing,
motor_start, motor_stop, pump_start, pump_stop, fan_start, fan_stop,
stop_all, reset_stop
```

Replies contain accepted/message/authoritative status. Invalid form/token is 400,
wrong content type 415, rejected Start 409, and GET control 405. The AP password
is the access boundary; the token protects against simple cross-site form commands.
Simulator endpoints additionally check loopback Host/Origin and reject missing
control tokens with 403.

After editing `web/index.html`, `style.css` or `app.js`, run:

```powershell
python tools\bundle_web.py
python tools\bundle_web.py --check
```

## Validation

```powershell
.\tools\test.ps1
.\simulator\build.ps1
node tests\simulator_test.cjs
# Browser suites require the development Playwright package and Edge/Chrome:
npm install --cache .build\npm-cache
npm run test:dashboard
npm run test:simulator-browser
npm run test:launcher
.\tools\compile.ps1
```

The portable tests compile production C++ modules with fake GPIO/UART/clock/HTTP,
exercise application coordination, and compare Python/C++ probabilities. Live
simulator tests run the real Node server/native engine; browser tests also cover
controls, charts, export and desktop/mobile layout. Build/test artifacts stay
under `.build`. Windows sandbox restrictions may require running compiler/browser
checks from normal PowerShell. `PLAYWRIGHT_MODULE` and `BROWSER_EXECUTABLE` can
point to an existing development installation.

[Validation record](docs/VALIDATION.md) distinguishes simulated checks and the
actual ESP32 compilation from physical verification. Real UART electronics,
FreeRTOS timing under load, probe calibration, relay mapping/polarity and actuator
operation still need hardware checks.

[User acceptance report](docs/USER-ACCEPTANCE.md) records 15 browser journeys and
seven Windows launcher checks, including reset/disconnection fixes and screenshots.
