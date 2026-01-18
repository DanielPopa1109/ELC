################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../APPL/ParamH/ParamH.c 

C_DEPS += \
./APPL/ParamH/ParamH.d 

OBJS += \
./APPL/ParamH/ParamH.o 


# Each subdirectory must supply rules for building sources it contributes
APPL/ParamH/%.o APPL/ParamH/%.su APPL/ParamH/%.cyclo: ../APPL/ParamH/%.c APPL/ParamH/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/APPL/SMon" -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Diag/Dcm" -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Diag/Dem" -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Sys/EcuM" -I../Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Com/CanH" -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Nvm" -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/APPL/ParamH" -Oz -ffunction-sections -fdata-sections -fno-strict-aliasing -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-APPL-2f-ParamH

clean-APPL-2f-ParamH:
	-$(RM) ./APPL/ParamH/ParamH.cyclo ./APPL/ParamH/ParamH.d ./APPL/ParamH/ParamH.o ./APPL/ParamH/ParamH.su

.PHONY: clean-APPL-2f-ParamH

