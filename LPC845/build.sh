#!/bin/bash
set -e
DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SCRIPTS_DIR="${DIR}/../scripts"
# Auto-detect ARM GCC toolchain
if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    CC="arm-none-eabi-gcc"
    OBJCOPY="arm-none-eabi-objcopy"
    SIZE="arm-none-eabi-size"
else
    MCUX_BIN=$(ls -d /Applications/MCUXpressoIDE*/ide/plugins/com.nxp.mcuxpresso.tools.*/tools/bin 2>/dev/null | tail -n 1 || true)
    if [ -n "$MCUX_BIN" ] && [ -x "${MCUX_BIN}/arm-none-eabi-gcc" ]; then
        CC="${MCUX_BIN}/arm-none-eabi-gcc"
        OBJCOPY="${MCUX_BIN}/arm-none-eabi-objcopy"
        SIZE="${MCUX_BIN}/arm-none-eabi-size"
    elif [ -x "/Applications/MCUXpressoIDE_24.9.25/ide/plugins/com.nxp.mcuxpresso.tools.macosx_24.9.0.202407110857/tools/bin/arm-none-eabi-gcc" ]; then
        TC_DIR="/Applications/MCUXpressoIDE_24.9.25/ide/plugins/com.nxp.mcuxpresso.tools.macosx_24.9.0.202407110857/tools/bin"
        CC="${TC_DIR}/arm-none-eabi-gcc"
        OBJCOPY="${TC_DIR}/arm-none-eabi-objcopy"
        SIZE="${TC_DIR}/arm-none-eabi-size"
    else
        echo "[ERROR] arm-none-eabi-gcc toolchain not found!"
        echo "Please install MCUXpresso IDE or GNU Arm Embedded Toolchain, or add it to PATH."
        exit 127
    fi
fi

echo "=== 1. Compiling LPC845 main.c ==="
${CC} -c -mcpu=cortex-m0plus -mthumb \
    -DCPU_LPC845 -DCPU_LPC845M301JBD48 -D__USE_CMSIS \
    -I"${DIR}" \
    -I"${DIR}/CMSIS" \
    -I"${DIR}/device" \
    -I"${DIR}/drivers" \
    -I"${DIR}/board" \
    -I"${DIR}/utilities" \
    -O2 -g -Wall -fcommon -ffunction-sections -fdata-sections \
    "${DIR}/main.c" -o "${DIR}/main.o"

echo "=== 2. Linking LPC845_TWS.axf ==="
${CC} \
  -nostdlib \
  -mcpu=cortex-m0plus -mthumb \
  -L"${DIR}/Debug" \
  -T "${DIR}/Debug/lpcxpresso845max_gpio_led_output_Debug.ld" \
  -o "${DIR}/LPC845_TWS.axf" \
  "${DIR}/main.o" \
  "${DIR}/build_v2/board/clock_config.o" \
  "${DIR}/build_v2/board/pin_mux.o" \
  "${DIR}/build_v2/device/system_LPC845.o" \
  "${DIR}/build_v2/startup/startup_lpc845.o" \
  "${DIR}/build_v2/drivers/fsl_adc.o" \
  "${DIR}/build_v2/drivers/fsl_clock.o" \
  "${DIR}/build_v2/drivers/fsl_common.o" \
  "${DIR}/build_v2/drivers/fsl_gpio.o" \
  "${DIR}/build_v2/drivers/fsl_i2c.o" \
  "${DIR}/build_v2/drivers/fsl_power.o" \
  "${DIR}/build_v2/drivers/fsl_reset.o" \
  "${DIR}/build_v2/drivers/fsl_sctimer.o" \
  "${DIR}/build_v2/drivers/fsl_swm.o" \
  "${DIR}/build_v2/drivers/fsl_syscon.o" \
  "${DIR}/build_v2/drivers/fsl_usart.o"

echo "=== 3. Generating Binary & Patching Checksum ==="
${OBJCOPY} -O binary "${DIR}/LPC845_TWS.axf" "${DIR}/LPC845_TWS.bin"
python3 "${SCRIPTS_DIR}/lpc_checksum.py" "${DIR}/LPC845_TWS.bin"
${OBJCOPY} -I binary -O ihex "${DIR}/LPC845_TWS.bin" "${DIR}/LPC845_TWS.hex"
${SIZE} "${DIR}/LPC845_TWS.axf"
echo "=== LPC845 Build Successful! Output: ${DIR}/LPC845_TWS.hex ==="
