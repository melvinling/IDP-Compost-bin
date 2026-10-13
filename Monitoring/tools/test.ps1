param([string]$Cxx = "g++")
$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
New-Item -ItemType Directory -Force (Join-Path $projectRoot ".build\tests") | Out-Null
Push-Location $projectRoot
try {
    & python tools/bundle_web.py --check
    if ($LASTEXITCODE -ne 0) { throw "Web asset check failed" }
    & $Cxx -std=c++17 -Wall -Wextra -Werror -Itests/stubs -Isrc tests/core_test.cpp src/Sensors.cpp src/Controller.cpp src/Prediction.cpp src/RelayDriver.cpp src/History.cpp src/Api.cpp src/WebServerModule.cpp -o .build/tests/core_test.exe
    if ($LASTEXITCODE -ne 0) { throw "Host test compilation failed" }
    & .build/tests/core_test.exe
    if ($LASTEXITCODE -ne 0) { throw "Core tests failed" }
    & $Cxx -std=c++17 -Wall -Wextra -Werror -Itests/stubs -Isrc tests/application_test.cpp src/Application.cpp src/Sensors.cpp src/Controller.cpp src/RelayDriver.cpp src/History.cpp src/Api.cpp src/WebServerModule.cpp -o .build/tests/application_test.exe
    if ($LASTEXITCODE -ne 0) { throw "Application test compilation failed" }
    & .build/tests/application_test.exe
    if ($LASTEXITCODE -ne 0) { throw "Application tests failed" }
    # Exercise the documented alternative Config profile in an isolated source copy.
    # Production sources remain untouched; only the profile selection differs.
    $legacySources = Join-Path $projectRoot ".build\tests\ecph"
    New-Item -ItemType Directory -Force $legacySources | Out-Null
    Get-ChildItem -LiteralPath "src" -Filter "*.h" | Copy-Item -Destination $legacySources
    Copy-Item -LiteralPath "src\Sensors.cpp" -Destination $legacySources
    $legacyConfigPath = Join-Path $legacySources "Config.h"
    $legacyConfig = (Get-Content -LiteralPath $legacyConfigPath -Raw).Replace("SOIL_PROFILE = SoilProfile::EcTemperatureMoisturePh", "SOIL_PROFILE = SoilProfile::EcPh")
    Set-Content -LiteralPath $legacyConfigPath -Value $legacyConfig -Encoding utf8
    & $Cxx -std=c++17 -Wall -Wextra -Werror -Itests/stubs "-I$legacySources" tests/legacy_sensor_test.cpp "$legacySources\Sensors.cpp" -o .build/tests/legacy_sensor_test.exe
    if ($LASTEXITCODE -ne 0) { throw "EC/pH profile compilation failed" }
    & .build/tests/legacy_sensor_test.exe
    if ($LASTEXITCODE -ne 0) { throw "EC/pH profile tests failed" }
    & node --check web/app.js
    if ($LASTEXITCODE -ne 0) { throw "JavaScript syntax check failed" }
    foreach ($script in @("simulator/server.cjs", "simulator/sim.js")) {
        & node --check $script
        if ($LASTEXITCODE -ne 0) { throw "Simulator JavaScript syntax check failed" }
    }
} finally { Pop-Location }
