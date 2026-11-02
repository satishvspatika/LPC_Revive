#!/bin/bash
set -e
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EX_DIR="${PROJECT_ROOT}/Example01"
SCRIPTS_DIR="${PROJECT_ROOT}/scripts"
OUT_DIR="${EX_DIR}/Debug"
TC_DIR="/Applications/MCUXpressoIDE_24.9.25/ide/plugins/com.nxp.mcuxpresso.tools.macosx_24.9.0.202407110857/tools/bin"
CC="${TC_DIR}/arm-none-eabi-gcc"
AS="${TC_DIR}/arm-none-eabi-as"
LD="${TC_DIR}/arm-none-eabi-ld"
OBJCOPY="${TC_DIR}/arm-none-eabi-objcopy"
SIZE="${TC_DIR}/arm-none-eabi-size"

echo "=== 1. Compiling upgraded sources (LPC1768) ==="
CFLAGS="-c -mcpu=cortex-m3 -mthumb -O0 -g -fcommon \
  -D__REDLIB__ -DDEBUG -D__CODE_RED -D__USE_CMSIS=CMSISv1p30_LPC17xx \
  -I${EX_DIR} \
  -I${PROJECT_ROOT}/CMSISv1p30_LPC17xx/inc \
  -I${PROJECT_ROOT}/FreeRTOS_Library/include \
  -I${PROJECT_ROOT}/FreeRTOS_Library/portable \
  -I${EX_DIR}/NXP/Drivers/include \
  -I${EX_DIR}/fat_sd \
  -I${EX_DIR}/USBHostLite"

${CC} ${CFLAGS} "${EX_DIR}/gprs.c" -o "${OUT_DIR}/gprs.o"
${CC} ${CFLAGS} "${EX_DIR}/ui.c" -o "${OUT_DIR}/ui.o"
${CC} ${CFLAGS} "${EX_DIR}/memory.c" -o "${OUT_DIR}/memory.o"
${CC} ${CFLAGS} "${EX_DIR}/kbd.c" -o "${OUT_DIR}/kbd.o"
${CC} ${CFLAGS} "${EX_DIR}/sensor.c" -o "${OUT_DIR}/sensor.o"
${CC} ${CFLAGS} "${EX_DIR}/disk.c" -o "${OUT_DIR}/disk.o"
${CC} ${CFLAGS} "${EX_DIR}/lcd.c" -o "${OUT_DIR}/lcd.o"

echo "=== 2. Linking Example01.axf at 0x00018000 ==="
${CC} \
  -nostdlib \
  -L"${PROJECT_ROOT}/CMSISv1p30_LPC17xx/Debug" \
  -L"${PROJECT_ROOT}/FreeRTOS_Library/Debug" \
  -L"${EX_DIR}/Debug" \
  -L/Applications/MCUXpressoIDE_24.9.25/ide/plugins/com.nxp.mcuxpresso.tools.macosx_24.9.0.202407110857/tools/arm-none-eabi/lib/thumb/v7-m/nofp \
  -L/Applications/MCUXpressoIDE_24.9.25/ide/plugins/com.nxp.mcuxpresso.tools.macosx_24.9.0.202407110857/tools/lib/gcc/arm-none-eabi/13.2.1/thumb/v7-m/nofp \
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
