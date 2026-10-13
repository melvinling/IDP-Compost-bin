# Bokashi simulator

Run from `Monitoring` in PowerShell:

```powershell
.\simulator\start.ps1
```

Or double-click **Start Simulator.cmd** in the `Monitoring` folder. The launcher
prepares the virtual bin and opens your default browser at
**http://127.0.0.1:8787/** after the server is ready. Keep the terminal open;
Ctrl+C stops the server. Choose **Too dry** or **Too wet**, then press **Play**.

If the simulator is already running, launching again opens the existing bin
without rebuilding or resetting it. Its original terminal owns the server.
For a different port, use `start.ps1 -Port 8788`. To start without opening a
browser, use `start.ps1 -NoBrowser`. CMD also forwards these options, for example
`".\Start Simulator.cmd" -NoBrowser -Port 8788` from a command prompt.

For a new instance, the launcher builds the native
engine before starting the app. Node.js and a C++17 `g++` compiler must be on PATH;
both are available on this machine. No npm install, Python or internet is needed
to run the simulator. Python is only needed when regenerating the model.

Startup failures explain what to fix: missing Node.js/compiler folders in PATH,
compiler errors, or a port used by another application. For a port conflict,
close that application or choose `-Port 8788`; the launcher does not close it.

The browser shows six readings and charts, all three virtual relay states,
stage/mode, cooldowns, mix flags, synthetic prediction and its confidence.
CSV export contains the loaded history and all six measurements; blanks represent
missing values. Time stamps are estimates from the browser clock. History is
bounded to 900 snapshots (one hour at four-second sampling); reset clears it.

1. Choose **Too dry**, **Too wet**, **Balanced** or **Mix due**. Scenarios reset
   the virtual bin and select automatic / finishing. The clock stays paused.
2. Press **Play**, select 1×, 10× or 60×, or use **Step 4 seconds** while paused.
3. Dry inputs request a two-second water pulse; wet inputs request an eight-second
   fan pulse. A completed water pulse queues mixing. The mixer runs for five
   seconds. Observe the event log and cooldowns.
4. **Missing moisture** inhibits automatic operation at once. **Frozen acquisition**
   stops snapshots; after 12 virtual seconds, values become stale and automatic
   outputs are inhibited. **Fermentation** keeps the actuators OFF.
5. **Adjust sensors and failures** edits all six sensor inputs and individual
   missing flags. Edits take effect at the next snapshot; Play or Step to acquire.
6. **Stop All** latches the controller OFF. **Reset stop latch** leaves manual mode
   and OFF outputs. To test manual starts, select finishing and manual. They last
   five seconds, with the same cooldowns and fan/pump interlock.
7. **Reset bin** returns to manual / fermentation at virtual time zero. **Pause
   clock** freezes the simulation; **Pause display** freezes chart presentation
   while acquisition and controller time continue.

Resetting or selecting a scenario resumes the display and clears the previous
bin's readings/charts. If the server disconnects, simulation actions are disabled
and a recovery message appears; restarting the launcher reconnects the page.
Changes show a progress message and disable duplicate submissions. Stop All
stays available while connected. A request that takes longer than five seconds
shows a timeout; check the current status before retrying, since the change might
already have reached the server.

The [user acceptance report](../docs/USER-ACCEPTANCE.md) records the validated
startup and browser journeys, screenshots and CSV evidence.

`engine.cpp` uses the production `Controller.cpp` and `Prediction.cpp` directly.
Every virtual advance runs control checks at intervals of at most 20 ms and
acquires snapshots every four seconds. This reproduces controller decisions;
it does not simulate ESP32 FreeRTOS scheduling, UART traffic, relays or loads.
The native engine never links RelayDriver or calls GPIO. Its verified pin values
only satisfy the controller's virtual configuration check and do not configure
the physical firmware.

The illustrative response adds 0.5 moisture percentage points per second while
watering, removes 0.12 while drying, and loses 0.001 in the background. Temperature
drifts toward 25°C and CO₂ toward 800 ppm; a fan speeds up that drift. Mixing
does not change the average moisture. EC, pH and distance stay at their chosen
values. These arbitrary responses let you see feedback and timing; they are not
calibrated fermentation or material models.

The Node server binds only to `127.0.0.1`, serves an explicit asset allowlist,
and checks a per-session token for changes. Simulation changes use a form POST
to `/api/simulation`; actuator commands use the same `/api/control` schema as
the ESP32. A reset changes the history boot ID. There is one virtual bin shared
by tabs opened on the same server. No hardware or cloud connection exists.
