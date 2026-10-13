param([int]$Port = 8787, [switch]$NoBrowser)
$ErrorActionPreference = "Stop"
try {
    if ($Port -lt 1024 -or $Port -gt 65535) { throw "Choose a port between 1024 and 65535." }
    $simulatorUrl = "http://127.0.0.1:$Port/"
    $probe = New-Object System.Net.Sockets.TcpClient
    $portOpen = $false
    try {
        $connect = $probe.BeginConnect("127.0.0.1", $Port, $null, $null)
        if ($connect.AsyncWaitHandle.WaitOne(1000)) { $probe.EndConnect($connect); $portOpen = $true }
    } catch { $portOpen = $false } finally { $probe.Close() }
    if ($portOpen) {
        $existingSimulator = $false
        try {
            $response = Invoke-WebRequest -Uri ($simulatorUrl + "api/data") -UseBasicParsing -TimeoutSec 3
            $data = $response.Content | ConvertFrom-Json
            $existingSimulator = $data.simulation -eq $true -and $data.schemaVersion -eq 2
        } catch { $existingSimulator = $false }
        if (-not $existingSimulator) {
            throw "Port $Port is used by another application. Close that application or run start.ps1 -Port 8788."
        }
        Write-Host "Bokashi simulator is already running: $simulatorUrl"
        Write-Host "Reuse the open simulator. Stop it from its original terminal with Ctrl+C."
        if (-not $NoBrowser) { Start-Process -FilePath $simulatorUrl }
        exit 0
    }
    if (-not (Get-Command node -ErrorAction SilentlyContinue)) {
        throw "Node.js was not found. Install Node.js or add its folder to PATH, then reopen this launcher."
    }
    if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
        throw "The g++ compiler was not found. Add your C++ compiler's bin folder to PATH, then reopen this launcher."
    }
    Write-Host "Preparing your virtual bin..."
    try { & "$PSScriptRoot\build.ps1" }
    catch { throw "The C++ engine build failed. Review the compiler message above, correct it, then reopen the launcher." }
    if ($LASTEXITCODE -ne 0) { throw "The simulator build failed. Review the compiler message above and try again." }
    Write-Host "Keep this terminal open. Ctrl+C stops the simulator."
    $serverArguments = @("$PSScriptRoot\server.cjs", "--port", "$Port")
    if (-not $NoBrowser) { $serverArguments += "--open" }
    & node @serverArguments
    if ($LASTEXITCODE -ne 0) { throw "The simulator stopped unexpectedly. Check the message above, then reopen the launcher." }
} catch {
    Write-Host ("Unable to start Bokashi simulator: " + $_.Exception.Message) -ForegroundColor Red
    exit 1
}
