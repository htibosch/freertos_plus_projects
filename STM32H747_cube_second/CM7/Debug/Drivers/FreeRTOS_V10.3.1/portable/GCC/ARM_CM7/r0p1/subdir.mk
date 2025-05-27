################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (9-2020-q2-update)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/FreeRTOS_V10.3.1/portable/GCC/ARM_CM7/r0p1/port.c 

OBJS += \
./Drivers/FreeRTOS_V10.3.1/portable/GCC/ARM_CM7/r0p1/port.o 

C_DEPS += \
./Drivers/FreeRTOS_V10.3.1/portable/GCC/ARM_CM7/r0p1/port.d 


# Each subdirectory must supply rules for building sources it contributes
Drivers/FreeRTOS_V10.3.1/portable/GCC/ARM_CM7/r0p1/%.o: ../Drivers/FreeRTOS_V10.3.1/portable/GCC/ARM_CM7/r0p1/%.c Drivers/FreeRTOS_V10.3.1/portable/GCC/ARM_CM7/r0p1/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m7 -std=gnu11 -g3 -DUSE_HAL_DRIVER -DSTM32H747xx -DSTM32H7xx -DCORE_CM7 -DDEBUG -DUSE_LOG_EVENT=0 -DLOG_EVENT_COUNT=100 -DLOG_EVENT_NAME_LEN=32 -DEVENT_MAY_WRAP=0 -DipconfigUSE_TCP_MEM_STATS=0 -DSTATIC_LOG_MEMORY=1 -DSYMBOLS_USED=1 -c -I../Core/Inc -I"E:/Home/plus/STM32H747_cube_second/Drivers/FreeRTOS-Plus-TCP/source/portable/NetworkInterface/STM32Hxx" -I../../Drivers/STM32H7xx_HAL_Driver/Inc -I../../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy -I../../Drivers/CMSIS/Device/ST/STM32H7xx/Include -I../../Drivers/CMSIS/Include -I"E:/Home/plus/STM32H747_cube_second/CM7/Drivers/FreeRTOS_V10.3.1/include" -I"E:/Home/plus/STM32H747_cube_second/CM7/Drivers/FreeRTOS_V10.3.1/portable/GCC/ARM_CM7/r0p1" -I"E:/Home/plus/STM32H747_cube_second/Drivers/Utilities" -I"E:/Home/plus/STM32H747_cube_second/Drivers/FreeRTOS-Plus-TCP/include" -I"E:/Home/plus/STM32H747_cube_second/Drivers/FreeRTOS-Plus-TCP/source/portable/Compiler/GCC" -I"E:/Home/plus/STM32H747_cube_second/Drivers/FreeRTOS-Plus-TCP/source/portable/NetworkInterface/include" -I"E:/Home/plus/STM32H747_cube_second/Drivers/FreeRTOS-Plus-TCP/tools/tcp_utilities/include" -O2 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers-2f-FreeRTOS_V10-2e-3-2e-1-2f-portable-2f-GCC-2f-ARM_CM7-2f-r0p1

clean-Drivers-2f-FreeRTOS_V10-2e-3-2e-1-2f-portable-2f-GCC-2f-ARM_CM7-2f-r0p1:
	-$(RM) ./Drivers/FreeRTOS_V10.3.1/portable/GCC/ARM_CM7/r0p1/port.d ./Drivers/FreeRTOS_V10.3.1/portable/GCC/ARM_CM7/r0p1/port.o

.PHONY: clean-Drivers-2f-FreeRTOS_V10-2e-3-2e-1-2f-portable-2f-GCC-2f-ARM_CM7-2f-r0p1

