################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../BSW/Com/CanH/CanH.c 

C_DEPS += \
./BSW/Com/CanH/CanH.d 

OBJS += \
./BSW/Com/CanH/CanH.o 


# Each subdirectory must supply rules for building sources it contributes
BSW/Com/CanH/%.o BSW/Com/CanH/%.su BSW/Com/CanH/%.cyclo: ../BSW/Com/CanH/%.c BSW/Com/CanH/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -DSTM32_THREAD_SAFE_STRATEGY=4 -c -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/APPL/SMon" -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Diag/Dcm" -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Sys/EcuM" -I../Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Com/CanH" -I../ThreadSafe -Oz -ffunction-sections -fdata-sections -fno-strict-aliasing -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-BSW-2f-Com-2f-CanH

clean-BSW-2f-Com-2f-CanH:
	-$(RM) ./BSW/Com/CanH/CanH.cyclo ./BSW/Com/CanH/CanH.d ./BSW/Com/CanH/CanH.o ./BSW/Com/CanH/CanH.su

.PHONY: clean-BSW-2f-Com-2f-CanH

