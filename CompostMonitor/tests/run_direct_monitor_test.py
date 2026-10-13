"""Build a host harness around polling functions extracted from the real sketch."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
source = (root / 'CompostMonitor/CompostMonitor.ino').read_text(encoding='utf-8')
# The shared stub imports the auto-scan header; verify identical protocol helpers.
assert (root / 'CompostMonitor/ModbusProtocol.h').read_bytes() == (root / 'RS485AutoScan/ModbusProtocol.h').read_bytes()

def section(start, end):
    return source[source.index(start):source.index(end)]

extracted = section('constexpr int ULTRASONIC_TRIGGER', 'WebServer server')
extracted += section('struct Sample', 'constexpr uint32_t SCAN_BAUDS').replace('String commandLine;\n', '')
extracted += section('struct Transaction', 'void pauseMonitoring()')
extracted += section('void commitSample()', 'void appendNumber(')
out = root / '.build/direct-monitor-test'
out.mkdir(parents=True, exist_ok=True)
(out / 'direct_monitor_extracted.h').write_text(extracted, encoding='utf-8')
exe = out / 'direct-monitor-test.exe'
subprocess.run([
    r'C:\msys64\mingw64\bin\g++.exe', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-static',
    '-I', str(out), '-o', str(exe), str(root / 'CompostMonitor/tests/direct_monitor_test.cpp')
], check=True)
subprocess.run([str(exe)], check=True)
