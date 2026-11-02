#!/bin/bash
set -e
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=========================================================="
echo "  Building Full TWS 5.1.0.5 System (LPC1768 & LPC845)"
echo "=========================================================="
"${ROOT}/Example01/build.sh"
echo ""
"${ROOT}/LPC845/build.sh"
echo ""
echo "=========================================================="
echo "  BUILD ALL SUCCESSFUL!"
echo "  - LPC1768: ${ROOT}/Example01/Debug/LPC1768_TWS_FULL.hex"
echo "  - LPC845:  ${ROOT}/LPC845/LPC845_TWS.hex"
echo "=========================================================="
