################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../USBHostLite/rtc_cal.c \
../USBHostLite/usbhost_lpc17xx.c \
../USBHostLite/usbhost_ms.c 

OBJS += \
./USBHostLite/rtc_cal.o \
./USBHostLite/usbhost_lpc17xx.o \
./USBHostLite/usbhost_ms.o 

C_DEPS += \
./USBHostLite/rtc_cal.d \
./USBHostLite/usbhost_lpc17xx.d \
./USBHostLite/usbhost_ms.d 


# Each subdirectory must supply rules for building sources it contributes
USBHostLite/%.o: ../USBHostLite/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -DDEBUG -D__USE_CMSIS=CMSISv1p30_LPC17xx -D__CODE_RED -D__REDLIB__ -I"E:\SatishWork\KSNDMC\TWS\1768\TWS5_1_0_5_recovered_Jan2026\Example01" -I"E:\SatishWork\KSNDMC\TWS\1768\TWS5_1_0_5_recovered_Jan2026\Example01\NXP\Drivers\include" -I"E:\SatishWork\KSNDMC\TWS\1768\TWS5_1_0_5_recovered_Jan2026\Example01\fat_sd" -I"E:\SatishWork\KSNDMC\TWS\1768\TWS5_1_0_5_recovered_Jan2026\Example01\USBHostLite" -I"C:\code_red\RedSuiteNXP_4.2.0_349\redsuite\Tools\arm-none-eabi\include\machine" -I"E:\SatishWork\KSNDMC\TWS\1768\TWS5_1_0_5_recovered_Jan2026\FreeRTOS_Library\demo_code" -I"E:\SatishWork\KSNDMC\TWS\1768\TWS5_1_0_5_recovered_Jan2026\CMSISv1p30_LPC17xx\inc" -I"E:\SatishWork\KSNDMC\TWS\1768\TWS5_1_0_5_recovered_Jan2026\FreeRTOS_Library\include" -I"E:\SatishWork\KSNDMC\TWS\1768\TWS5_1_0_5_recovered_Jan2026\FreeRTOS_Library\portable" -O0 -g3 -fsigned-char -c -fmessage-length=0 -fno-builtin -ffunction-sections -mcpu=cortex-m3 -mthumb -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


