################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../NXP/Drivers/source/lpc17xx_adc.c \
../NXP/Drivers/source/lpc17xx_clkpwr.c \
../NXP/Drivers/source/lpc17xx_gpdma.c \
../NXP/Drivers/source/lpc17xx_gpio.c \
../NXP/Drivers/source/lpc17xx_i2c.c \
../NXP/Drivers/source/lpc17xx_libcfg_default.c \
../NXP/Drivers/source/lpc17xx_nvic.c \
../NXP/Drivers/source/lpc17xx_pinsel.c \
../NXP/Drivers/source/lpc17xx_ssp.c \
../NXP/Drivers/source/lpc17xx_uart.c \
../NXP/Drivers/source/lpc17xx_wdt.c 

OBJS += \
./NXP/Drivers/source/lpc17xx_adc.o \
./NXP/Drivers/source/lpc17xx_clkpwr.o \
./NXP/Drivers/source/lpc17xx_gpdma.o \
./NXP/Drivers/source/lpc17xx_gpio.o \
./NXP/Drivers/source/lpc17xx_i2c.o \
./NXP/Drivers/source/lpc17xx_libcfg_default.o \
./NXP/Drivers/source/lpc17xx_nvic.o \
./NXP/Drivers/source/lpc17xx_pinsel.o \
./NXP/Drivers/source/lpc17xx_ssp.o \
./NXP/Drivers/source/lpc17xx_uart.o \
./NXP/Drivers/source/lpc17xx_wdt.o 

C_DEPS += \
./NXP/Drivers/source/lpc17xx_adc.d \
./NXP/Drivers/source/lpc17xx_clkpwr.d \
./NXP/Drivers/source/lpc17xx_gpdma.d \
./NXP/Drivers/source/lpc17xx_gpio.d \
./NXP/Drivers/source/lpc17xx_i2c.d \
./NXP/Drivers/source/lpc17xx_libcfg_default.d \
./NXP/Drivers/source/lpc17xx_nvic.d \
./NXP/Drivers/source/lpc17xx_pinsel.d \
./NXP/Drivers/source/lpc17xx_ssp.d \
./NXP/Drivers/source/lpc17xx_uart.d \
./NXP/Drivers/source/lpc17xx_wdt.d 


# Each subdirectory must supply rules for building sources it contributes
NXP/Drivers/source/%.o: ../NXP/Drivers/source/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -DDEBUG -D__USE_CMSIS=CMSISv1p30_LPC17xx -D__CODE_RED -D__REDLIB__ -I"E:\SatishWork\KSNDMC\TWS\1768\TWS5_1_0_5_recovered_Jan2026\Example01" -I"E:\SatishWork\KSNDMC\TWS\1768\TWS5_1_0_5_recovered_Jan2026\Example01\NXP\Drivers\include" -I"E:\SatishWork\KSNDMC\TWS\1768\TWS5_1_0_5_recovered_Jan2026\Example01\fat_sd" -I"E:\SatishWork\KSNDMC\TWS\1768\TWS5_1_0_5_recovered_Jan2026\Example01\USBHostLite" -I"C:\code_red\RedSuiteNXP_4.2.0_349\redsuite\Tools\arm-none-eabi\include\machine" -I"E:\SatishWork\KSNDMC\TWS\1768\TWS5_1_0_5_recovered_Jan2026\FreeRTOS_Library\demo_code" -I"E:\SatishWork\KSNDMC\TWS\1768\TWS5_1_0_5_recovered_Jan2026\CMSISv1p30_LPC17xx\inc" -I"E:\SatishWork\KSNDMC\TWS\1768\TWS5_1_0_5_recovered_Jan2026\FreeRTOS_Library\include" -I"E:\SatishWork\KSNDMC\TWS\1768\TWS5_1_0_5_recovered_Jan2026\FreeRTOS_Library\portable" -O0 -g3 -fsigned-char -c -fmessage-length=0 -fno-builtin -ffunction-sections -mcpu=cortex-m3 -mthumb -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


