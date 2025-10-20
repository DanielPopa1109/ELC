################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../BSW/Diag/Dem/Dem.c 

C_DEPS += \
./BSW/Diag/Dem/Dem.d 

OBJS += \
./BSW/Diag/Dem/Dem.o 


# Each subdirectory must supply rules for building sources it contributes
BSW/Diag/Dem/%.o BSW/Diag/Dem/%.su BSW/Diag/Dem/%.cyclo: ../BSW/Diag/Dem/%.c BSW/Diag/Dem/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I"C:/Users/Daniel/Desktop/ELC/ELC_APP/APPL/SMon" -I"C:/Users/Daniel/Desktop/ELC/ELC_APP/BSW/Diag/Dcm" -I"C:/Users/Daniel/Desktop/ELC/ELC_APP/BSW/Diag/Dem" -I"C:/Users/Daniel/Desktop/ELC/ELC_APP/BSW/Mem" -I"C:/Users/Daniel/Desktop/ELC/ELC_APP/BSW/Sys/EcuM" -I../Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I"C:/Users/Daniel/Desktop/ELC/ELC_APP/BSW/Com/CanH" -Oz -ffunction-sections -fdata-sections -fno-strict-aliasing -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-BSW-2f-Diag-2f-Dem

clean-BSW-2f-Diag-2f-Dem:
	-$(RM) ./BSW/Diag/Dem/Dem.cyclo ./BSW/Diag/Dem/Dem.d ./BSW/Diag/Dem/Dem.o ./BSW/Diag/Dem/Dem.su

.PHONY: clean-BSW-2f-Diag-2f-Dem

