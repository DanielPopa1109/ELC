################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ThreadSafe/newlib_lock_glue.c 

C_DEPS += \
./ThreadSafe/newlib_lock_glue.d 

OBJS += \
./ThreadSafe/newlib_lock_glue.o 


# Each subdirectory must supply rules for building sources it contributes
ThreadSafe/%.o ThreadSafe/%.su ThreadSafe/%.cyclo: ../ThreadSafe/%.c ThreadSafe/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -DSTM32_THREAD_SAFE_STRATEGY=4 -c -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/APPL/SMon" -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Diag/Dcm" -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Sys/EcuM" -I../Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Com/CanH" -I../ThreadSafe -Oz -ffunction-sections -fdata-sections -fno-strict-aliasing -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-ThreadSafe

clean-ThreadSafe:
	-$(RM) ./ThreadSafe/newlib_lock_glue.cyclo ./ThreadSafe/newlib_lock_glue.d ./ThreadSafe/newlib_lock_glue.o ./ThreadSafe/newlib_lock_glue.su

.PHONY: clean-ThreadSafe

