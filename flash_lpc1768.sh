#!/bin/bash
set -e
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LINKSERVER="/tmp/linkserver/LinkServer"
CONFIG="${ROOT}/scripts/lpc1768-mculink.json"
HEX="${ROOT}/Example01/Debug/LPC1768_TWS_FULL.hex"
AXF="${ROOT}/Example01/Debug/Example01.axf"

echo "=== Flashing LPC1768 (TWS 5.1.0.5 Full Firmware) ==="
pkill -9 -f redlinkserv 2>/dev/null || true
pkill -9 -f LinkServer 2>/dev/null || true

echo "1. Mass Erasing LPC1768 (512 KB)..."
"${LINKSERVER}" flash -p '#1' -u none "${CONFIG}" erase
pkill -9 -f redlinkserv 2>/dev/null || true

echo "2. Programming Boot Stub (0x0) + Application (0x18000)..."
# Allow exit code 1 here: the CPU reboots after write (breaking SWD Terminate),
# but the sectors ARE written. We check for the known success string instead.
"${LINKSERVER}" flash -p '#1' -u none "${CONFIG}" load "${HEX}" 2>&1 | tee /tmp/lpc1768_flash.log || true
if grep -q "Sectors written:" /tmp/lpc1768_flash.log; then
    echo "   Flash write confirmed OK (sectors written)."
else
    echo "   ERROR: Flash write did not complete!" && exit 1
fi
pkill -9 -f redlinkserv 2>/dev/null || true

echo "3. Issuing Hardware Reset Pulse..."
"${LINKSERVER}" probe '#1' wiretimedreset 50 || true
pkill -9 -f redlinkserv 2>/dev/null || true

echo "=== LPC1768 Flashed and Running! ==="
