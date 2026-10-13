# Validation record

Date: 2026-10-13. Target: Monitoring and its native simulator, ESP32 Dev Module.
No firmware was uploaded and no physical sensor or actuator was operated.

The later user-focused validation passed all 15 browser journeys and 7 Windows
launcher checks. See [USER-ACCEPTANCE.md](USER-ACCEPTANCE.md) for steps, outcomes,
screenshots, CSV and startup/error logs.

## Executed checks

| Check | Result |
| --- | --- |
| Web asset bundle / syntax | Generated header matches editable HTML/CSS/JS; dashboard and simulator JavaScript parse |
| Core C++ | g++ 13.1.0, C++17, -Wall -Wextra -Werror; production parser, sensors, controller, predictor, relay driver, history and HTTP handlers passed |
| EC/pH-only profile | Isolated build selects the documented alternative profile; three reads and unsupported moisture/temperature passed |
| Application | Real coordination/acquisition with instrumented predictor and fake platform passed; every completed invalid snapshot invokes inference |
| Model probability parity | 4,519 cases, all four probabilities within 0.000002 of sklearn; data, random probes and float32 threshold neighbours |
| Native engine build | C++17 / -O2 / -Wall -Wextra -Werror passed |
| Live simulator HTTP | Real Node server and native controller/predictor passed scenario, timing, control and playback checks |
| ESP32 dashboard browser | Playwright 1.62.1 with installed headless Edge; mocked transport; six charts, commands and CSV passed without page errors |
| Live simulator browser | Real Node/native backend; scenarios, sensor edits, missing/stale failures, controls, clock, event log and CSV passed without page errors |
| Visual review | Full desktop 1200px and mobile 390px screenshots reviewed; no horizontal overflow |
| Local preview | Loopback page returned HTTP 200; in-app preview requested |
| ESP32 firmware | Arduino CLI / Espressif core 3.3.12, esp32:esp32:esp32; compilation passed; final binary details below |

The Windows sandbox blocked loopback sockets and headless browser startup, and
prevented the ESP32 compiler loading its runtime DLLs. Those checks succeeded
outside the sandbox after automatic approval. The compile checks only built
firmware. Initial failures are not counted as successful validation.

## Coverage

- Modbus CRC, function/address/count validation, zeros, noise, request echoes,
  captured inverted echoes, wrong address, malformed/partial/bad CRC frames and
  exceptions; bounded diagnostic input, trace limits, sample-boundary RX inversion
  and read-only requests.
- Production acquisition order: EC register 2, pH 3, moisture 0, signed temperature
  1 at soil baud 4800; CO2 register 0 at 9600. Signed negative temperatures,
  independent invalid moisture/temperature/pH, ultrasonic limits, timestamps,
  four-second cadence, five-response timeouts and continuous interference.
- Manual/fermentation boot; finishing selection; configured manual runs despite
  missing measurements; five-second expiry and cooldown; auto water/fan/mix pulses
  and cooldown; post-water mixing and periodic interval; individual-stop inhibition,
  Stop All latch, reset/manual/OFF, immediate OFF on mode/stage changes.
- Invalid, stale, unavailable or old-context inference inhibition; fan/pump
  mutual exclusion; ten-second continuous runtime during manual retriggering;
  unsigned millis rollover. A test initially skipped safety ticks and hit the
  runtime latch; it was corrected to model regular pulse expiry.
- Disabled relay GPIO silence; duplicate/conflicting/unsupported pin or polarity
  rejection; three configured outputs initialized OFF before output enable;
  active-high/low levels and no writes to the unused fourth relay.
- Actual ESP32 HTTP handlers with stub transport: schema v2, zero/null/stale values,
  disabled Start, method/content/token/duplicate-field rejection, acknowledged
  stop/reset, incremental history, paging, restart and 900-row bounds.
- EC/pH-only profile: isolated copy of production acquisition with only the Config
  profile selection changed; three transactions, CO2 baud switching, unsupported
  moisture/temperature, null values and invalid automatic snapshot.
- Live simulator: normal/dry/wet/mix-due/missing/stale/fermentation scenarios,
  acquisition pause, virtual control ticks, water-to-mix sequence, cooldowns,
  manual interlock, stop/reset, POST token and sequence validation, playback,
  1x/10x/60x controls, sensor editing and failure toggles.
- Browser charts show null gaps and zeros; CSV containing all six measurements is downloaded
  with blank missing values; history deduplicates/caps/reset; display pause preserves
  polling/history; disconnection disables controls and reconnection retains history;
  all six cards and controls fit desktop/mobile layouts without page errors.

## Synthetic model

Seed 42, 32 trees, depth 6, 1,578 nodes. Split by artificial scenario group:
900 training / 300 holdout rows. RF accuracy 99.67%, macro F1 0.99662; decision
tree and label-rule baseline both 100%. These scores measure imitation of
artificial labels only. See `ml/report.json` for split groups, confusion matrices,
importances, versions and parameters. Dataset SHA-256:

```text
daf77c0d9db64420682185bbc47fe92b5e17729a4cd17d6466530235dd21df0c
```

The CSV is unchanged from preparation. The inference export stores normalized
leaf probabilities and exact double thresholds; C++ inputs match sklearn float32.
Confidence is not calibrated. The simpler baselines prevent a claim that RF has
been shown superior on this dummy policy.

## Firmware size

Final compilation passed against the finished sources and generated dashboard:

- Program storage: **1,029,604 / 1,310,720 bytes (78%)**.
- Global RAM: **84,816 / 327,680 bytes (25%)**; 242,864 bytes remain before local/runtime allocations.
- Binary: `.build/esp32/Monitoring.ino.bin`.
- SHA-256: `ba89425880d7bec52f98981c669a91bfe680dddf709afeed3416ed4b21c9d59c`.
- The model resides in constant program data. The default binary keeps physical
  actuators disabled and all relay pins/polarities unconfigured.

## Artifacts and reproduction

Run `tools/test.ps1`, `simulator/build.ps1`, `node tests/simulator_test.cjs`,
`npm run test:dashboard`, `npm run test:simulator-browser`, `npm run test:launcher`, and
`tools/compile.ps1` from Monitoring. Browser tests require Playwright and Edge or
Chrome; PLAYWRIGHT_MODULE/BROWSER_EXECUTABLE support existing installations.

Screenshots/CSV: `.build/dashboard/` and `.build/simulator/`. Firmware binaries:
`.build/esp32/`. Only firmware sources are staged for Arduino compilation.
The simulator starts with `simulator/start.ps1` or `Start Simulator.cmd`.

## Limits of the evidence

Host tests use fake UART, pulse interrupts, clocks, GPIO and HTTP transport.
The application test does not run the FreeRTOS task concurrently; controller
behavior is tested directly and task code is compiled by the ESP32 toolchain.
The native simulator exercises the same controller/predictor, while its clock,
sensors and moisture/ventilation response are artificial. It links no relay driver
and does not operate hardware.

Actual sensor model/registers, bus identity/baud compatibility, converter logic
levels/direction, ultrasonic calibration, FreeRTOS timing under load, relay
channel/polarity, OFF levels across power-up and load behavior remain unverified.
Both soil profile paths pass host acquisition tests; the default firmware compile
uses the assumed four-parameter profile. Physical operation
requires confirmed hardware configuration and calibrated measurements/settings.
Software OFF is a command, without proof of physically stopped loads.
