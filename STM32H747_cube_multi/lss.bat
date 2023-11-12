@echo off

:: E:\Home\plus\STM32H747_cube_second\CM7\Debug\STM32H747_cube_second_CM7
:: arm-none-eabi-gcc.exe

set GCC_BIN=E:\ST\STM32CubeIDE_1.3.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.7-2018-q2-update.win32_1.4.0.202007081208\tools\bin
set CPY_OBJ=arm-none-eabi-objdump.exe
set GCC_NM=arm-none-eabi-nm.exe
set GCC_SIZ=arm-none-eabi-size
set ELF_DIR=E:\Home\plus\STM32H747_cube_second\CM7\Debug
set ELF_NAME=STM32H747_cube_second_CM7

cd %ELF_DIR%

:: echo %GCC_BIN%\%CPY_OBJ% -S %ELF_DIR%\%ELF_NAME%.elf
%GCC_BIN%\%CPY_OBJ% -S %ELF_DIR%\%ELF_NAME%.elf > %ELF_DIR%\%ELF_NAME%.lss
%GCC_BIN%\%GCC_NM% -n %ELF_DIR%\%ELF_NAME%.elf > %ELF_DIR%\%ELF_NAME%.sym
%GCC_BIN%\%GCC_SIZ% %ELF_DIR%\%ELF_NAME%.elf > %ELF_DIR%\%ELF_NAME%.size


echo See %ELF_DIR%\%ELF_NAME%.lss
echo See %ELF_DIR%\%ELF_NAME%.sym
echo See %ELF_DIR%\%ELF_NAME%.size

cd E:\Home\plus\STM32H747_cube_second

