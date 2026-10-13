param([string]$ArduinoCli = "arduino-cli", [switch]$SkipBundle,
      [string]$ArduinoData = (Join-Path $env:USERPROFILE "AppData\Local\Arduino15"))
$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $SkipBundle) {
    & python (Join-Path $PSScriptRoot "bundle_web.py")
    if ($LASTEXITCODE -ne 0) { throw "Web bundle failed" }
}
if (-not (Get-Command $ArduinoCli -ErrorAction SilentlyContinue)) {
    $bundledCli = Join-Path $env:USERPROFILE "OneDrive\Desktop\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
    if (Test-Path -LiteralPath $bundledCli) { $ArduinoCli = $bundledCli }
    else { throw "arduino-cli not found; pass -ArduinoCli with its full path" }
}
$buildPath = Join-Path $projectRoot ".build\esp32"
$cachePath = Join-Path $projectRoot ".build\cache"
New-Item -ItemType Directory -Force $buildPath, $cachePath | Out-Null
# Arduino recursively copies the sketch tree. Stage only firmware sources so
# test/browser/cache artifacts cannot race with or recursively enter the build.
$stagingPath = Join-Path $projectRoot (".build\input-" + [guid]::NewGuid().ToString("N") + "\Monitoring")
New-Item -ItemType Directory -Force $stagingPath | Out-Null
Copy-Item -LiteralPath (Join-Path $projectRoot "Monitoring.ino") -Destination $stagingPath
Copy-Item -LiteralPath (Join-Path $projectRoot "src") -Destination $stagingPath -Recurse
$cliConfig = Join-Path $projectRoot ".build\arduino-cli.yaml"
@"
directories:
  data: '$ArduinoData'
build_cache:
  path: '$cachePath'
"@ | Set-Content -LiteralPath $cliConfig -Encoding utf8
$taskTemp = Join-Path $projectRoot ".build\tmp"
New-Item -ItemType Directory -Force $taskTemp | Out-Null
$previousTemp = $env:TEMP
$previousTmp = $env:TMP
try {
    $env:TEMP = $taskTemp
    $env:TMP = $taskTemp
    & $ArduinoCli --config-file $cliConfig compile --fqbn esp32:esp32:esp32 --build-path $buildPath $stagingPath
    if ($LASTEXITCODE -ne 0) { throw "ESP32 compilation failed" }
} finally {
    $env:TEMP = $previousTemp
    $env:TMP = $previousTmp
}
