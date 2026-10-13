# User acceptance validation

**Result: all 22 checks passed — 15 browser journeys and 7 Windows launcher checks.**

Date: 2026-10-13 (Asia/Kuala_Lumpur). Tests used installed Windows PowerShell, Node.js, g++ and Microsoft Edge with the real native C++ simulator. The default-browser opening request was also exercised through the shipped CMD launcher.

## Improvements validated

- Double-click startup now opens the default browser after readiness; relaunch reuses the current bin without rebuilding or resetting it.
- Missing dependencies, compiler failures and occupied ports show repair instructions.
- Bin reset and scenario selection clear old readings/charts and resume the display.
- Offline simulation actions are disabled. Pending changes show progress, prevent duplicates, time out after five seconds and recover after errors.
- Stop All remains available while connected during an ordinary pending simulation request. Missing sensor checkboxes now have distinct accessible names.

## Browser journeys

| # | User steps | Expected | Actual | Result |
| --- | --- | --- | --- | --- |
| U01: First screen | Open the simulator without selecting a scenario. | Connected, manual/fermentation, six measurements and all outputs OFF. | Manual/fermentation shown; six cards; all three outputs OFF; simulation notice visible. | **PASS** |
| U02: Too dry | Select Too dry; step the paused clock three times. | Pump turns OFF at 2s, mixing starts, finishes at 9s, and cooldown remains. | Pump OFF logged at 2.00s; mixer ON after watering and OFF at 9.00s; pump cooldown displayed 50s at 12s. | **PASS** |
| U03: Too wet | Select Too wet; step twice. | Fan runs eight seconds; pump stays OFF and fan cooldown is 30s. | Fan ON with pump OFF; fan OFF logged at 8.00s; displayed cooldown 30s. | **PASS** |
| U04: Balanced, mix due and fermentation | Select each scenario and advance the mix scenario. | Balanced stays OFF; due mixing completes; fermentation stays OFF. | Balanced predicted idle; due mixer completed and timer reset; fermentation kept all outputs OFF. | **PASS** |
| U05: Manual operation | Reset; select finishing/manual; start pump, stop it, try fan during cooldown; test all manual starts. | Five-second expiry, cooldown and fan/pump interlock are acknowledged. | Pump and fan expired after five seconds; active pump rejected fan; pump cooldown rejected retrigger; individual mixer stop worked. | **PASS** |
| U06: Stop All and reset | Start a wet scenario, Stop All, select Automatic, step, then Reset stop latch. | Latch prevents restarts; reset leaves manual/OFF. | STOP LATCHED remained effective after Automatic and Step; reset showed manual mode and OFF outputs. | **PASS** |
| U07: Sensor editing and invalid entries | Edit six sensor inputs; apply then step; try a blank and moisture 101. | Values update only on acquisition; invalid forms do not reach the server. | Displayed 80.0 cm, 1234 µS/cm, pH 6.2, 1200 ppm, 50.0%, 31.5°C after acquisition; blank EC and moisture 101 focused invalid fields without sending requests. | **PASS** |
| U08: Missing and frozen readings | Test each of six missing checkboxes; then select Frozen acquisition and step 12s. | Failures are clearly labelled, values hidden, automatic outputs OFF. | Each missing flag produced a hidden value and failure label; all six became Stale reading after 12 virtual seconds; outputs OFF. | **PASS** |
| U09: Clock and display controls | Use Play/Pause at 1x, 10x, 60x; pause display while clock advances; resume; step while paused. | Clock pause freezes time; display pause freezes readings while time continues. | All three speeds acknowledged; clock stopped during pause; display stayed frozen while clock advanced and refreshed on resume. | **PASS** |
| U10: Reset and scenario while display paused | Pause a wet display and Reset bin; pause again and select Too dry. | Previous readings disappear; live display resumes with the new bin. | Wet 80.0% changed to reset 50.0%; subsequent dry scenario showed 25.0%; Pause display button restored both times. | **PASS** |
| U11: Pending requests, errors and timeout | Hold a Step request; use Stop All; wait for 5s timeout; inject a rejected request; retry normally. | Progress is visible, duplicates disabled, Stop All works, and controls recover. | Progress shown; duplicate actions disabled; Stop All worked; held request timed out after five seconds; error and retry paths restored controls. | **PASS** |
| U12: Disconnect and restart | Stop the actual server, observe disabled controls, restart it without reloading the page. | Recovery message, disabled actions, automatic reconnection and fresh defaults. | Actual server shutdown disabled simulation controls and Stop All; restart automatically reconnected and restored manual/50.0%/OFF defaults. | **PASS** |
| U13: CSV export | Acquire normal readings, mark moisture missing, acquire again, download CSV. | Eight columns: time/uptime and six measurements; missing moisture blank. | Downloaded compost-readings.csv with eight columns and parseable UTC timestamps; final 8.000-second row had a blank moisture value. | **PASS** |
| U14: Refresh and shared tabs | Reload; open a second tab; choose Too wet there and observe the first tab. | History reloads; both tabs show the same virtual bin. | Reload restored history; Too wet selected in a second tab appeared as fan ON / 80.0% in the original tab. | **PASS** |
| U15: Keyboard, mobile and zoom | Tab and Enter on clock controls; inspect 390px mobile and 200% zoom layout. | Visible focus, labelled fields, operable keyboard controls, no horizontal clipping. | Tab/Enter operated Play/Pause with visible focus; 390px mobile and 200% CSS zoom had no horizontal overflow; no page errors. | **PASS** |

## Windows launcher checks

| # | User steps | Expected | Actual | Result |
| --- | --- | --- | --- | --- |
| L01: CMD startup from another folder with spaces | Execute the shipped CMD launcher from an unrelated directory, using -NoBrowser. | Native build succeeds, HTTP page is usable, no browser opener is run. | Actual CMD launch built the engine and served HTTP 200; working-directory and space handling passed. | **PASS** |
| L02: Relaunch reuses the existing bin | Run the launcher again while its engine executable is in use. | Exit successfully without rebuilding, resetting or starting a conflicting server. | Existing instance reused; boot ID unchanged; no rebuild attempted. | **PASS** |
| L03: Default browser opens after readiness | Execute the CMD launcher without -NoBrowser. | HTTP is ready before Windows accepts the default-browser open command. | Windows browser opener exited successfully after server readiness; HTTP 200 confirmed. | **PASS** |
| L04: Missing Node.js | Launch with a controlled PATH that excludes Node.js. | Clear Node.js/PATH guidance and nonzero exit. | Node.js/PATH instructions shown; exit 1. | **PASS** |
| L05: Missing C++ compiler | Launch with Node.js available but g++ absent from PATH. | Clear compiler/PATH guidance and nonzero exit. | Compiler bin-folder/PATH instructions shown; exit 1. | **PASS** |
| L06: Build failure | Use an isolated failing compiler fixture without altering the real compiler. | Compiler output and actionable build-failure message; no server starts. | Intentional compiler failure reported with repair/retry instructions; exit 1. | **PASS** |
| L07: Occupied port | Start an unrelated HTTP fixture on the requested port, then launch. | Clear port conflict and alternative-port instruction; fixture stays running. | Port conflict explained with -Port 8788; unrelated application untouched. | **PASS** |

The CMD launcher was executed from an unrelated working directory, against an isolated project copy whose path contains spaces. Failure fixtures modified only test-process PATH values or used a disposable failing compiler; the installed tools and unrelated port owner were left intact. Owned test servers were stopped after testing.

## Evidence

- [Structured browser results](../.build/user-acceptance/browser-results.json)
- [Structured launcher results](../.build/user-acceptance/launcher-results.json)
- [First screen](../.build/user-acceptance/first-screen.png)
- [Desktop screenshot](../.build/user-acceptance/desktop.png)
- [390px mobile screenshot](../.build/user-acceptance/mobile.png)
- [200% CSS zoom screenshot](../.build/user-acceptance/zoom-200.png)
- [Downloaded CSV](../.build/user-acceptance/export.csv)
- [Launcher startup log](../.build/user-acceptance/launcher-startup.log)
- [Default-browser opening log](../.build/user-acceptance/launcher-browser-open.log)
- [Missing Node.js log](../.build/user-acceptance/launcher-missing-node.log)
- [Missing compiler log](../.build/user-acceptance/launcher-missing-compiler.log)
- [Failed build log](../.build/user-acceptance/launcher-build-failure.log)
- [Occupied-port log](../.build/user-acceptance/launcher-port-conflict.log)

Full screenshots were visually reviewed: control labels, sensor cards, Stop All and wrapped layouts remain readable with no clipped controls. Zoom evidence uses 200% CSS zoom within Edge, rather than changing the browser toolbar zoom preference.

## Regression checks

- C++ controller, acquisition, relay driver, API and application checks passed, including the EC/pH-only profile.
- All 4,519 sklearn/C++ probability parity cases passed; the dataset and model were unchanged.
- Existing live simulator HTTP and ESP32 dashboard browser suites passed.
- Updated web asset bundle matched sources.
- ESP32 compilation passed: 1,029,604 bytes flash (78%) and 84,816 bytes global RAM (25%). No upload occurred.

Run from `Monitoring`:

```powershell
.\tools\test.ps1
node tests\simulator_test.cjs
npm run test:dashboard
npm run test:simulator-browser
npm run test:launcher
.\tools\compile.ps1
```

Browser tests need Playwright and installed Edge. PLAYWRIGHT_MODULE can point to an existing module directory. Launcher tests exercise Windows startup; their normal-open check opens the default browser. Evidence stays under `.build/user-acceptance`.

## Limits

These are automated operator journeys and a visual review, rather than a study with independent human participants or a formal accessibility certification. Browser assertions use visible controls, labels, DOM focus/layout and downloaded CSV; network interception is used only to introduce delays/errors and synchronize acknowledgements.

The simulator represents one virtual bin shared across its tabs. Its sensor response and Random Forest training data are synthetic. Hardware wiring, relay polarity, physical loads and real compost outcomes remain unvalidated; physical firmware outputs remain disabled. The controller rules, API schema and model are unchanged by these usability fixes.
