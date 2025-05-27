################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (9-2020-q2-update)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Src/clock-stm32h7.c \
../Core/Src/main.c \
../Core/Src/stm32h7xx_hal_msp.c \
../Core/Src/stm32h7xx_it.c \
../Core/Src/syscalls.c \
../Core/Src/sysmem.c 

OBJS += \
./Core/Src/clock-stm32h7.o \
./Core/Src/main.o \
./Core/Src/stm32h7xx_hal_msp.o \
./Core/Src/stm32h7xx_it.o \
./Core/Src/syscalls.o \
./Core/Src/sysmem.o 

C_DEPS += \
./Core/Src/clock-stm32h7.d \
./Core/Src/main.d \
./Core/Src/stm32h7xx_hal_msp.d \
./Core/Src/stm32h7xx_it.d \
./Core/Src/syscalls.d \
./Core/Src/sysmem.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Src/%.o: ../Core/Src/%.c Core/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DUSE_HAL_DRIVER -DSTM32H747xx -DSTM32H7xx -DCORE_CM7 -DDEBUG -DUSE_LOG_EVENT=0 -DLOG_EVENT_COUNT=100 -DLOG_EVENT_NAME_LEN=32 -DEVENT_MAY_WRAP=0 -DipconfigUSE_TCP_MEM_STATS=0 -DSTATIC_LOG_MEMORY=1 -DSYMBOLS_USED=1 -c -I../Core/Inc -I"E:/Home/plus/STM32H747_cube_second/Drivers/FreeRTOS-Plus-TCP/source/portable/NetworkInterface/STM32Hxx" -I../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../Drivers/CMSIS/Include -I"E:/Home/plus/STM32H747_cube_second/CM7/Drivers/FreeRTOS_V10.3.1/include" -I"E:/Home/plus/STM32H747_cube_second/CM7/Drivers/FreeRTOS_V10.3.1/portable/GCC/ARM_CM7/r0p1" -I"E:/Home/plus/STM32H747_cube_second/Drivers/Utilities" -I"E:/Home/plus/STM32H747_cube_second/Drivers/FreeRTOS-Plus-TCP/include" -I"E:/Home/plus/STM32H747_cube_second/Drivers/FreeRTOS-Plus-TCP/source/portable/Compiler/GCC" -I"E:/Home/plus/STM32H747_cube_second/Drivers/FreeRTOS-Plus-TCP/source/portable/NetworkInterface/include" -I"E:/Home/plus/STM32H747_cube_second/Drivers/FreeRTOS-Plus-TCP/tools/tcp_utilities/include" -O2 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Src

clean-Core-2f-Src:
	-$(RM) ./Core/Src/clock-stm32h7.d ./Core/Src/clock-stm32h7.o ./Core/Src/main.d ./Core/Src/main.o ./Core/Src/stm32h7xx_hal_msp.d ./Core/Src/stm32h7xx_hal_msp.o ./Core/Src/stm32h7xx_it.d ./Core/Src/stm32h7xx_it.o ./Core/Src/syscalls.d ./Core/Src/syscalls.o ./Core/Src/sysmem.d ./Core/Src/sysmem.o

.PHONY: clean-Core-2f-Src

