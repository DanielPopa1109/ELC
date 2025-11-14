################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../BSW/Sys/EcuM/EcuM.c 

C_DEPS += \
./BSW/Sys/EcuM/EcuM.d 

OBJS += \
./BSW/Sys/EcuM/EcuM.o 


# Each subdirectory must supply rules for building sources it contributes
BSW/Sys/EcuM/%.o BSW/Sys/EcuM/%.su BSW/Sys/EcuM/%.cyclo: ../BSW/Sys/EcuM/%.c BSW/Sys/EcuM/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/APPL/SMon" -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Diag/Dcm" -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Diag/Dem" -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Sys/EcuM" -I../Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Com/CanH" -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Nvm" -Oz -ffunction-sections -fdata-sections -fno-strict-aliasing -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-BSW-2f-Sys-2f-EcuM

clean-BSW-2f-Sys-2f-EcuM:
	-$(RM) ./BSW/Sys/EcuM/EcuM.cyclo ./BSW/Sys/EcuM/EcuM.d ./BSW/Sys/EcuM/EcuM.o ./BSW/Sys/EcuM/EcuM.su

.PHONY: clean-BSW-2f-Sys-2f-EcuM

