################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../BSW/Com/Lin/Lin.c 

C_DEPS += \
./BSW/Com/Lin/Lin.d 

OBJS += \
./BSW/Com/Lin/Lin.o 


# Each subdirectory must supply rules for building sources it contributes
BSW/Com/Lin/%.o BSW/Com/Lin/%.su BSW/Com/Lin/%.cyclo: ../BSW/Com/Lin/%.c BSW/Com/Lin/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g1 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I../Core/Inc -IC:/Users/Daniel/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.5/Drivers/STM32F1xx_HAL_Driver/Inc -IC:/Users/Daniel/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.5/Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -IC:/Users/Daniel/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.5/Middlewares/Third_Party/FreeRTOS/Source/include -IC:/Users/Daniel/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.5/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -IC:/Users/Daniel/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.5/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -IC:/Users/Daniel/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.5/Drivers/CMSIS/Device/ST/STM32F1xx/Include -IC:/Users/Daniel/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.5/Drivers/CMSIS/Include -I"C:/Users/Daniel/Desktop/SPM/SPM_APP/APPL/SMon" -I"C:/Users/Daniel/Desktop/SPM/SPM_APP/BSW/Com/Lin" -I"C:/Users/Daniel/Desktop/SPM/SPM_APP/BSW/Diag/Dcm" -I"C:/Users/Daniel/Desktop/SPM/SPM_APP/BSW/Diag/Dem" -I"C:/Users/Daniel/Desktop/SPM/SPM_APP/BSW/Io/Ain" -I"C:/Users/Daniel/Desktop/SPM/SPM_APP/BSW/Mem" -I"C:/Users/Daniel/Desktop/SPM/SPM_APP/BSW/Sys/EcuM" -Oz -ffunction-sections -fdata-sections -mslow-flash-data -fno-strict-aliasing -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-BSW-2f-Com-2f-Lin

clean-BSW-2f-Com-2f-Lin:
	-$(RM) ./BSW/Com/Lin/Lin.cyclo ./BSW/Com/Lin/Lin.d ./BSW/Com/Lin/Lin.o ./BSW/Com/Lin/Lin.su

.PHONY: clean-BSW-2f-Com-2f-Lin

