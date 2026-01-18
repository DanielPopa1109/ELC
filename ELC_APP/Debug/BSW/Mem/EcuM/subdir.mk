################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../BSW/Mem/EcuM/EcuM.c 

C_DEPS += \
./BSW/Mem/EcuM/EcuM.d 

OBJS += \
./BSW/Mem/EcuM/EcuM.o 


# Each subdirectory must supply rules for building sources it contributes
BSW/Mem/EcuM/%.o BSW/Mem/EcuM/%.su BSW/Mem/EcuM/%.cyclo: ../BSW/Mem/EcuM/%.c BSW/Mem/EcuM/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I"C:/Users/Daniel/Desktop/SPM/SPM_APP/APPL/SMon" -I"C:/Users/Daniel/Desktop/SPM/SPM_APP/BSW/Diag/Dcm" -I"C:/Users/Daniel/Desktop/SPM/SPM_APP/BSW/Diag/Dem" -I"C:/Users/Daniel/Desktop/SPM/SPM_APP/BSW/Io/Ain" -I"C:/Users/Daniel/Desktop/SPM/SPM_APP/BSW/Mem" -I../Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -Oz -ffunction-sections -fdata-sections -mslow-flash-data -fno-strict-aliasing -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-BSW-2f-Mem-2f-EcuM

clean-BSW-2f-Mem-2f-EcuM:
	-$(RM) ./BSW/Mem/EcuM/EcuM.cyclo ./BSW/Mem/EcuM/EcuM.d ./BSW/Mem/EcuM/EcuM.o ./BSW/Mem/EcuM/EcuM.su

.PHONY: clean-BSW-2f-Mem-2f-EcuM

