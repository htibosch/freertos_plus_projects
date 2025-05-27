################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (9-2020-q2-update)
################################################################################

-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include Drivers/Utilities/subdir.mk
-include Drivers/STM32H7xx_HAL_Driver/subdir.mk
-include Drivers/FreeRTOS_V10.3.1/portable/MemMang/subdir.mk
-include Drivers/FreeRTOS_V10.3.1/portable/GCC/ARM_CM7/r0p1/subdir.mk
-include Drivers/FreeRTOS_V10.3.1/subdir.mk
-include Drivers/FreeRTOS-Plus-TCP/tools/tcp_utilities/subdir.mk
-include Drivers/FreeRTOS-Plus-TCP/source/portable/NetworkInterface/STM32Hxx/subdir.mk
-include Drivers/FreeRTOS-Plus-TCP/source/portable/NetworkInterface/Common/subdir.mk
-include Drivers/FreeRTOS-Plus-TCP/source/portable/BufferManagement/subdir.mk
-include Drivers/FreeRTOS-Plus-TCP/source/subdir.mk
-include Core/Startup/subdir.mk
-include Core/Src/subdir.mk
-include Common/Src/subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

OPTIONAL_TOOL_DEPS := \
$(wildcard ../makefile.defs) \
$(wildcard ../makefile.init) \
$(wildcard ../makefile.targets) \


BUILD_ARTIFACT_NAME := STM32H747_cube_second_CM7
BUILD_ARTIFACT_EXTENSION := elf
BUILD_ARTIFACT_PREFIX :=
BUILD_ARTIFACT := $(BUILD_ARTIFACT_PREFIX)$(BUILD_ARTIFACT_NAME)$(if $(BUILD_ARTIFACT_EXTENSION),.$(BUILD_ARTIFACT_EXTENSION),)

# Add inputs and outputs from these tool invocations to the build variables 
EXECUTABLES += \
STM32H747_cube_second_CM7.elf \

SIZE_OUTPUT += \
default.size.stdout \

OBJDUMP_LIST += \
STM32H747_cube_second_CM7.list \

OBJCOPY_BIN += \
STM32H747_cube_second_CM7.bin \


# All Target
all: main-build

# Main-build Target
main-build: STM32H747_cube_second_CM7.elf secondary-outputs

# Tool invocations
STM32H747_cube_second_CM7.elf: $(OBJS) $(USER_OBJS) E:\Home\plus\STM32H747_cube_second\CM7\STM32H747XIHX_FLASH.ld makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-gcc -o "STM32H747_cube_second_CM7.elf" @"objects.list" $(USER_OBJS) $(LIBS) -mcpu=cortex-m7 -T"E:\Home\plus\STM32H747_cube_second\CM7\STM32H747XIHX_FLASH.ld" --specs=nosys.specs -Wl,-Map="STM32H747_cube_second_CM7.map" -Wl,--gc-sections -static --specs=nano.specs -mfpu=fpv5-d16 -mfloat-abi=hard -mthumb -Wl,--start-group -lc -lm -Wl,--end-group
	@echo 'Finished building target: $@'
	@echo ' '

default.size.stdout: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-size  $(EXECUTABLES)
	@echo 'Finished building: $@'
	@echo ' '

STM32H747_cube_second_CM7.list: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objdump -h -S $(EXECUTABLES) > "STM32H747_cube_second_CM7.list"
	@echo 'Finished building: $@'
	@echo ' '

STM32H747_cube_second_CM7.bin: $(EXECUTABLES) makefile objects.list $(OPTIONAL_TOOL_DEPS)
	arm-none-eabi-objcopy  -O binary $(EXECUTABLES) "STM32H747_cube_second_CM7.bin"
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) STM32H747_cube_second_CM7.bin STM32H747_cube_second_CM7.elf STM32H747_cube_second_CM7.list default.size.stdout
	-@echo ' '

secondary-outputs: $(SIZE_OUTPUT) $(OBJDUMP_LIST) $(OBJCOPY_BIN)

fail-specified-linker-script-missing:
	@echo 'Error: Cannot find the specified linker script. Check the linker settings in the build configuration.'
	@exit 2

warn-no-linker-script-specified:
	@echo 'Warning: No linker script specified. Check the linker settings in the build configuration.'

.PHONY: all clean dependents main-build fail-specified-linker-script-missing warn-no-linker-script-specified

-include ../makefile.targets
