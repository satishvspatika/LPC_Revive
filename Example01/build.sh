#!/bin/bash
set -e
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EX_DIR="${PROJECT_ROOT}/Example01"
SCRIPTS_DIR="${PROJECT_ROOT}/scripts"
OUT_DIR="${EX_DIR}/Debug"
# Auto-detect ARM GCC toolchain and MCUXpresso IDE
MCUX_BIN=$(ls -d /Applications/MCUXpressoIDE*/ide/plugins/com.nxp.mcuxpresso.tools.*/tools/bin 2>/dev/null | tail -n 1 || true)
if [ -z "$MCUX_BIN" ]; then
    MCUX_BIN=$(ls -d /Applications/MCUXpresso\ IDE*/MCUXpresso\ IDE.app/Contents/Eclipse/plugins/com.nxp.mcuxpresso.tools.*/tools/bin 2>/dev/null | tail -n 1 || true)
fi

if [ -n "$MCUX_BIN" ] && [ -x "${MCUX_BIN}/arm-none-eabi-gcc" ]; then
    TC_DIR="${MCUX_BIN}"
    CC="${TC_DIR}/arm-none-eabi-gcc"
    AS="${TC_DIR}/arm-none-eabi-as"
    LD="${TC_DIR}/arm-none-eabi-ld"
    OBJCOPY="${TC_DIR}/arm-none-eabi-objcopy"
    SIZE="${TC_DIR}/arm-none-eabi-size"
    MCUX_TOOLS=$(cd "${MCUX_BIN}/.." && pwd)
elif [ -x "${PROJECT_ROOT}/tools/arm-toolchain/bin/arm-none-eabi-gcc" ]; then
    TC_DIR="${PROJECT_ROOT}/tools/arm-toolchain/bin"
    CC="${TC_DIR}/arm-none-eabi-gcc"
    AS="${TC_DIR}/arm-none-eabi-as"
    LD="${TC_DIR}/arm-none-eabi-ld"
    OBJCOPY="${TC_DIR}/arm-none-eabi-objcopy"
    SIZE="${TC_DIR}/arm-none-eabi-size"
elif command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    CC="arm-none-eabi-gcc"
    AS="arm-none-eabi-as"
    LD="arm-none-eabi-ld"
    OBJCOPY="arm-none-eabi-objcopy"
    SIZE="arm-none-eabi-size"
else
    echo "[ERROR] arm-none-eabi-gcc toolchain not found!"
    echo "Please install MCUXpresso IDE or GNU Arm Embedded Toolchain, or add it to PATH."
    exit 127
fi

echo "=== 1. Compiling upgraded sources (LPC1768) ==="
CFLAGS="-c -mcpu=cortex-m3 -mthumb -O0 -g -fcommon \
  -D__REDLIB__ -DDEBUG -D__CODE_RED -D__USE_CMSIS=CMSISv1p30_LPC17xx \
  -I${EX_DIR} \
  -I${PROJECT_ROOT}/CMSISv1p30_LPC17xx/inc \
  -I${PROJECT_ROOT}/FreeRTOS_Library/include \
  -I${PROJECT_ROOT}/FreeRTOS_Library/portable \
  -I${PROJECT_ROOT}/FreeRTOS_Library/demo_code \
  -I${EX_DIR}/NXP/Drivers/include \
  -I${EX_DIR}/fat_sd \
  -I${EX_DIR}/USBHostLite"

${CC} ${CFLAGS} "${EX_DIR}/main.c" -o "${OUT_DIR}/main.o"
${CC} ${CFLAGS} "${EX_DIR}/gprs.c" -o "${OUT_DIR}/gprs.o"
${CC} ${CFLAGS} "${EX_DIR}/ui.c" -o "${OUT_DIR}/ui.o"
${CC} ${CFLAGS} "${EX_DIR}/memory.c" -o "${OUT_DIR}/memory.o"
${CC} ${CFLAGS} "${EX_DIR}/kbd.c" -o "${OUT_DIR}/kbd.o"
${CC} ${CFLAGS} "${EX_DIR}/sensor.c" -o "${OUT_DIR}/sensor.o"
${CC} ${CFLAGS} "${EX_DIR}/disk.c" -o "${OUT_DIR}/disk.o"
${CC} ${CFLAGS} "${EX_DIR}/lcd.c" -o "${OUT_DIR}/lcd.o"

# Locate Redlib libraries dynamically across any installed MCUXpresso IDE version
REDLIB_DIR=""
GCC_LIB_DIR=""

if [ -n "${MCUX_TOOLS}" ] && [ -d "${MCUX_TOOLS}/arm-none-eabi/lib/thumb/v7-m/nofp" ]; then
    REDLIB_DIR="${MCUX_TOOLS}/arm-none-eabi/lib/thumb/v7-m/nofp"
    GCC_LIB_DIR=$(ls -d "${MCUX_TOOLS}"/lib/gcc/arm-none-eabi/*/thumb/v7-m/nofp 2>/dev/null | tail -n 1 || true)
fi

if [ -z "${REDLIB_DIR}" ] || [ ! -f "${REDLIB_DIR}/libcr_c.a" ]; then
    REDLIB_C=$(find /Applications/MCUXpresso* -name "libcr_c.a" 2>/dev/null | grep -E "thumb/v7-?m" | head -n 1 || true)
    if [ -n "${REDLIB_C}" ]; then
        REDLIB_DIR=$(dirname "${REDLIB_C}")
        MCUX_BASE=$(cd "${REDLIB_DIR}/../../../.." && pwd)
        GCC_LIB_DIR=$(ls -d "${MCUX_BASE}"/lib/gcc/arm-none-eabi/*/thumb/v7-m/nofp 2>/dev/null | tail -n 1 || true)
    fi
fi

if [ -z "${REDLIB_DIR}" ] && [ -d "${PROJECT_ROOT}/tools/lib" ]; then
    REDLIB_DIR="${PROJECT_ROOT}/tools/lib"
fi

if [ -n "${REDLIB_DIR}" ] && [ -f "${REDLIB_DIR}/libcr_c.a" ]; then
    echo "=== 2. Linking Example01.axf at 0x00018000 (Redlib: ${REDLIB_DIR}) ==="
    ${CC} \
      -nostdlib \
      -L"${PROJECT_ROOT}/CMSISv1p30_LPC17xx/Debug" \
      -L"${PROJECT_ROOT}/FreeRTOS_Library/Debug" \
      -L"${EX_DIR}/Debug" \
      -L"${REDLIB_DIR}" \
      ${GCC_LIB_DIR:+-L"${GCC_LIB_DIR}"} \
      -Xlinker --gc-sections \
      -Xlinker -Map="${OUT_DIR}/Example01.map" \
      -mcpu=cortex-m3 -mthumb \
      -T "${EX_DIR}/Debug/example01.ld" \
      -o "${OUT_DIR}/Example01.axf" \
      "${OUT_DIR}/cr_startup_lpc17.o" \
      "${OUT_DIR}/disk.o" \
      "${OUT_DIR}/global_functions.o" \
      "${OUT_DIR}/gprs.o" \
      "${OUT_DIR}/ispuart.o" \
      "${OUT_DIR}/kbd.o" \
      "${OUT_DIR}/lcd.o" \
      "${OUT_DIR}/main.o" \
      "${OUT_DIR}/memory.o" \
      "${OUT_DIR}/sensor.o" \
      "${OUT_DIR}/ui.o" \
      "${OUT_DIR}/wdt.o" \
      "${OUT_DIR}/fat_sd/diskio.o" \
      "${OUT_DIR}/fat_sd/spi_sd_lpc17xx.o" \
      "${OUT_DIR}/fat_sd/ff.o" \
      "${OUT_DIR}/fat_sd/fattime.o" \
      "${OUT_DIR}/fat_sd/option/ccsbcs.o" \
      "${OUT_DIR}/USBHostLite/rtc_cal.o" \
      "${OUT_DIR}/USBHostLite/usbhost_lpc17xx.o" \
      "${OUT_DIR}/USBHostLite/usbhost_ms.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_adc.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_clkpwr.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_gpdma.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_gpio.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_i2c.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_nvic.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_pinsel.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_ssp.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_uart.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_wdt.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_libcfg_default.o" \
      -lCMSISv1p30_LPC17xx -lFreeRTOS_Library -lcr_c -lcr_eabihelpers -lgcc
else
    echo "=== 2. Linking Example01.axf with GNU Arm Embedded Toolchain ==="
    ${CC} \
      -mcpu=cortex-m3 -mthumb \
      --specs=nano.specs --specs=nosys.specs \
      -L"${PROJECT_ROOT}/CMSISv1p30_LPC17xx/Debug" \
      -L"${PROJECT_ROOT}/FreeRTOS_Library/Debug" \
      -L"${EX_DIR}/Debug" \
      -Xlinker --gc-sections \
      -Xlinker -Map="${OUT_DIR}/Example01.map" \
      -T "${EX_DIR}/Debug/example01_gnu.ld" \
      -o "${OUT_DIR}/Example01.axf" \
      "${OUT_DIR}/cr_startup_lpc17.o" \
      "${OUT_DIR}/disk.o" \
      "${OUT_DIR}/global_functions.o" \
      "${OUT_DIR}/gprs.o" \
      "${OUT_DIR}/ispuart.o" \
      "${OUT_DIR}/kbd.o" \
      "${OUT_DIR}/lcd.o" \
      "${OUT_DIR}/main.o" \
      "${OUT_DIR}/memory.o" \
      "${OUT_DIR}/sensor.o" \
      "${OUT_DIR}/ui.o" \
      "${OUT_DIR}/wdt.o" \
      "${OUT_DIR}/fat_sd/diskio.o" \
      "${OUT_DIR}/fat_sd/spi_sd_lpc17xx.o" \
      "${OUT_DIR}/fat_sd/ff.o" \
      "${OUT_DIR}/fat_sd/fattime.o" \
      "${OUT_DIR}/fat_sd/option/ccsbcs.o" \
      "${OUT_DIR}/USBHostLite/rtc_cal.o" \
      "${OUT_DIR}/USBHostLite/usbhost_lpc17xx.o" \
      "${OUT_DIR}/USBHostLite/usbhost_ms.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_adc.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_clkpwr.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_gpdma.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_gpio.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_i2c.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_nvic.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_pinsel.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_ssp.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_uart.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_wdt.o" \
      "${OUT_DIR}/NXP/Drivers/source/lpc17xx_libcfg_default.o" \
      -lCMSISv1p30_LPC17xx -lFreeRTOS_Library -lm
fi

echo "=== 3. Assembling Sector 0 Boot Stub ==="
${AS} -mcpu=cortex-m3 -mthumb "${SCRIPTS_DIR}/boot_stub.s" -o "${OUT_DIR}/boot_stub.o"
${LD} -T "${SCRIPTS_DIR}/boot_stub.ld" "${OUT_DIR}/boot_stub.o" -o "${OUT_DIR}/boot_stub.elf"
${OBJCOPY} -O binary "${OUT_DIR}/boot_stub.elf" "${OUT_DIR}/boot_stub.bin"
python3 "${SCRIPTS_DIR}/lpc_checksum.py" "${OUT_DIR}/boot_stub.bin"
${OBJCOPY} -I binary -O ihex "${OUT_DIR}/boot_stub.bin" "${OUT_DIR}/boot_stub.hex"

echo "=== 4. Merging Boot Stub (0x0) + Application (0x18000) ==="
${OBJCOPY} -O ihex "${OUT_DIR}/Example01.axf" "${OUT_DIR}/Example01.hex"
cat "${OUT_DIR}/boot_stub.hex" "${OUT_DIR}/Example01.hex" | grep -v ":00000001FF" > "${OUT_DIR}/LPC1768_TWS_FULL.hex"
echo ":00000001FF" >> "${OUT_DIR}/LPC1768_TWS_FULL.hex"
${OBJCOPY} -I ihex -O binary "${OUT_DIR}/LPC1768_TWS_FULL.hex" "${OUT_DIR}/LPC1768_TWS_FULL.bin"

${SIZE} "${OUT_DIR}/Example01.axf"
echo "=== LPC1768 Build Completed Successfully! ==="
echo "Full Flashing HEX: ${OUT_DIR}/LPC1768_TWS_FULL.hex"
