################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/USB/Core/DCD/LPC11UXX/Endpoint_LPC11Uxx.c 

OBJS += \
./Drivers/USB/Core/DCD/LPC11UXX/Endpoint_LPC11Uxx.o 

C_DEPS += \
./Drivers/USB/Core/DCD/LPC11UXX/Endpoint_LPC11Uxx.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/USB/Core/DCD/LPC11UXX/%.o: ../Drivers/USB/Core/DCD/LPC11UXX/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU C Compiler'
	arm-none-eabi-gcc -std=gnu99 -D__REDLIB__ -DDEBUG -D__CODE_RED -DCORE_M0 -D__LPC11U1X__ -DUSB_DEVICE_ONLY -I"D:\dev\polden\lpc-proj\lpc_chip_11uxx_lib\inc" -I"D:\dev\polden\lpc-proj\nxp_lpcxpresso_11u14_board_lib\inc" -O0 -Wall -c -fmessage-length=0 -fno-builtin -ffunction-sections -fdata-sections -mcpu=cortex-m0 -mthumb -D__REDLIB__ -specs=redlib.specs -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.o)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

