################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Src/adc.c \
../Src/can.c \
../Src/crc.c \
../Src/dma.c \
../Src/freertos.c \
../Src/gpio.c \
../Src/iwdg.c \
../Src/main.c \
../Src/stm32f1xx_hal_msp.c \
../Src/stm32f1xx_hal_timebase_tim.c \
../Src/stm32f1xx_it.c \
../Src/syscalls_dummy.c \
../Src/system_stm32f1xx.c \
../Src/tim.c 

C_DEPS += \
./Src/adc.d \
./Src/can.d \
./Src/crc.d \
./Src/dma.d \
./Src/freertos.d \
./Src/gpio.d \
./Src/iwdg.d \
./Src/main.d \
./Src/stm32f1xx_hal_msp.d \
./Src/stm32f1xx_hal_timebase_tim.d \
./Src/stm32f1xx_it.d \
./Src/syscalls_dummy.d \
./Src/system_stm32f1xx.d \
./Src/tim.d 

OBJS += \
./Src/adc.o \
./Src/can.o \
./Src/crc.o \
./Src/dma.o \
./Src/freertos.o \
./Src/gpio.o \
./Src/iwdg.o \
./Src/main.o \
./Src/stm32f1xx_hal_msp.o \
./Src/stm32f1xx_hal_timebase_tim.o \
./Src/stm32f1xx_it.o \
./Src/syscalls_dummy.o \
./Src/system_stm32f1xx.o \
./Src/tim.o 


# Each subdirectory must supply rules for building sources it contributes
Src/%.o Src/%.su Src/%.cyclo: ../Src/%.c Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m3 -std=gnu11 -g3 -DDEBUG -DUSE_HAL_DRIVER -DSTM32F103xB -c -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/APPL/SMon" -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Diag/Dcm" -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Diag/Dem" -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Sys/EcuM" -I../Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc -I../Drivers/STM32F1xx_HAL_Driver/Inc/Legacy -I../Middlewares/Third_Party/FreeRTOS/Source/include -I../Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2 -I../Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM3 -I../Drivers/CMSIS/Device/ST/STM32F1xx/Include -I../Drivers/CMSIS/Include -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Com/CanH" -I"C:/Users/Daniel/Desktop/GitHub Repositories/ELC_APP/BSW/Nvm" -Oz -ffunction-sections -fdata-sections -fno-strict-aliasing -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfloat-abi=soft -mthumb -o "$@"

clean: clean-Src

clean-Src:
	-$(RM) ./Src/adc.cyclo ./Src/adc.d ./Src/adc.o ./Src/adc.su ./Src/can.cyclo ./Src/can.d ./Src/can.o ./Src/can.su ./Src/crc.cyclo ./Src/crc.d ./Src/crc.o ./Src/crc.su ./Src/dma.cyclo ./Src/dma.d ./Src/dma.o ./Src/dma.su ./Src/freertos.cyclo ./Src/freertos.d ./Src/freertos.o ./Src/freertos.su ./Src/gpio.cyclo ./Src/gpio.d ./Src/gpio.o ./Src/gpio.su ./Src/iwdg.cyclo ./Src/iwdg.d ./Src/iwdg.o ./Src/iwdg.su ./Src/main.cyclo ./Src/main.d ./Src/main.o ./Src/main.su ./Src/stm32f1xx_hal_msp.cyclo ./Src/stm32f1xx_hal_msp.d ./Src/stm32f1xx_hal_msp.o ./Src/stm32f1xx_hal_msp.su ./Src/stm32f1xx_hal_timebase_tim.cyclo ./Src/stm32f1xx_hal_timebase_tim.d ./Src/stm32f1xx_hal_timebase_tim.o ./Src/stm32f1xx_hal_timebase_tim.su ./Src/stm32f1xx_it.cyclo ./Src/stm32f1xx_it.d ./Src/stm32f1xx_it.o ./Src/stm32f1xx_it.su ./Src/syscalls_dummy.cyclo ./Src/syscalls_dummy.d ./Src/syscalls_dummy.o ./Src/syscalls_dummy.su ./Src/system_stm32f1xx.cyclo ./Src/system_stm32f1xx.d ./Src/system_stm32f1xx.o ./Src/system_stm32f1xx.su ./Src/tim.cyclo ./Src/tim.d ./Src/tim.o ./Src/tim.su

.PHONY: clean-Src

