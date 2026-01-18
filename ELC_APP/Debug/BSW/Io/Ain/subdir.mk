################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../BSW/Io/Ain/Ain.c 

C_DEPS += \
./BSW/Io/Ain/Ain.d 

OBJS += \
./BSW/Io/Ain/Ain.o 


# Each subdirectory must supply rules for building sources it contributes
BSW/Io/Ain/%.o BSW/Io/Ain/%.su BSW/Io/Ain/%.cyclo: ../BSW/Io/Ain/%.c BSW/Io/Ain/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I"C:/Users/Daniel/Desktop/SPM/SPM_APP/APPL/SMon" -I"C:/Users/Daniel/Desktop/SPM/SPM_APP/BSW/Diag/Dcm" -I"C:/Users/Daniel/Desktop/SPM/SPM_APP/BSW/Diag/Dem" -I"C:/Users/Daniel/Desktop/SPM/SPM_APP/BSW/Io/Ain" -I"C:/Users/Daniel/Desktop/SPM/SPM_APP/BSW/Mem" -I"C:/Users/Daniel/Desktop/SPM/SPM_APP/BSW/Sys/EcuM" -I../Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -Oz -ffunction-sections -fdata-sections -mslow-flash-data -fno-strict-aliasing -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-BSW-2f-Io-2f-Ain

clean-BSW-2f-Io-2f-Ain:
	-$(RM) ./BSW/Io/Ain/Ain.cyclo ./BSW/Io/Ain/Ain.d ./BSW/Io/Ain/Ain.o ./BSW/Io/Ain/Ain.su

.PHONY: clean-BSW-2f-Io-2f-Ain

