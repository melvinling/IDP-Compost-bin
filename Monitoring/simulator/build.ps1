param([string]$Cxx = "g++")
$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$outputDir = Join-Path $projectRoot ".build\simulator"
New-Item -ItemType Directory -Force $outputDir | Out-Null
& $Cxx -std=c++17 -O2 -Wall -Wextra -Werror "-I$projectRoot\src" "$PSScriptRoot\engine.cpp" "$projectRoot\src\Controller.cpp" "$projectRoot\src\Prediction.cpp" -o "$outputDir\engine.exe"
if ($LASTEXITCODE -ne 0) { throw "Simulator engine compilation failed" }
Write-Host "Built the shared C++ controller and Random Forest simulator."
