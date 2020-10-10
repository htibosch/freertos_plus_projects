#==============================================================================
#
#	RTOSDemo project
#
#==============================================================================
MY_BOARD=SAM4E_XPLAINED_PRO

ipconfigMULTI_INTERFACE=false
ipconfigUSE_IPv6=false

# GCC_BIN=C:\PROGRA~2\Atmel\ATMELT~1\ARMGCC~1\Native\4.8.1443\ARM-GN~1\bin
# GCC_PREFIX=arm-none-eabi

#GCC_BIN=C:/PROGRA~2/Atmel/Studio/7.0/toolchain/arm/arm-gnu-toolchain/bin
GCC_PREFIX=arm-none-eabi

GCC_BIN=$(HOME)/gcc-arm-none-eabi-9-2020-q2-update/bin

makeUSE_TICKLESS_IDLE=false
makeUSE_MICREL_KSZ8851=false

USE_IPERF=true

# Base paths
ROOT_PATH = $(subst /fireworks,,$(abspath ../fireworks))
CUR_PATH  = $(subst /fireworks,,$(abspath ./fireworks))

MY_PATH = $(CUR_PATH)

# E:\Home\plus\sam4e\FreeRTOS_Plus_TCP_and_FAT_SAM4E\src\ASF\sam\drivers
# E:\Home\plus\sam4e\FreeRTOS_Plus_TCP_and_FAT_SAM4E\src\config.mk
DRVR_PATH = $(CUR_PATH)/ASF/sam/drivers

GEN_FRAMEWORK_PATH = $(ROOT_PATH)/../Framework
COMMON_PATH = $(ROOT_PATH)/../Common
#FREERTOS_PATH = $(GEN_FRAMEWORK_PATH)/FreeRTOS_V8.2.2
FREERTOS_PATH = $(GEN_FRAMEWORK_PATH)/FreeRTOS_V9.0.0rc2/FreeRTOS/Source

RTOS_MEM_PATH = $(FREERTOS_PATH)/portable/MemMang
SERVICE_PATH = $(CUR_PATH)/ASF/common/services
UTIL_PATH = $(CUR_PATH)/ASF/sam/drivers
COMPONENTS_PATH = $(CUR_PATH)/ASF/sam/components
CMSIS_INCLUDE_PATH = $(CUR_PATH)/ASF/sam/utils/cmsis/sam4e/include
PLUS_FAT_PATH = $(GEN_FRAMEWORK_PATH)/FreeRTOS-Plus-FAT
UTILITIES_PATH = $(GEN_FRAMEWORK_PATH)/Utilities
ifeq ($(ipconfigMULTI_INTERFACE),true)
	DEFS += -D ipconfigMULTI_INTERFACE=1
	PLUS_TCP_PATH = \
		$(ROOT_PATH)/../Framework/FreeRTOS-Plus-TCP-multi
else
	DEFS += -D ipconfigMULTI_INTERFACE=0
	PLUS_TCP_PATH = \
		$(ROOT_PATH)/../Framework/FreeRTOS-Plus-TCP
endif

ifeq ($(ipconfigUSE_IPv6),true)
	DEFS += -D ipconfigUSE_IPv6=1
else
	DEFS += -D ipconfigUSE_IPv6=0
endif

PROTOCOLS_PATH     = $(PLUS_TCP_PATH)/source/protocols
COMMON_UTILS_PATH = $(COMMON_PATH)/Utilities

C_SRCS=
CPP_SRCS=
S_SRCS=

DEFS += -DBOARD=$(MY_BOARD) -DFREERTOS_USED
DEFS += -D__SAM4E16E__=1

INC_PATH = 

INC_PATH += \
	$(PLUS_FAT_PATH)/include/ \
	$(PLUS_FAT_PATH)/portable/ATSAM4E/ \
	$(PLUS_FAT_PATH)/portable/common/

ifeq ($(makeUSE_MICREL_KSZ8851),true)
	DEFS += -DUSE_MICREL_KSZ8851=1
	INC_PATH += \
		$(SERVICE_PATH)/spi/ \
		$(SERVICE_PATH)/gpio/ \
		$(DRVR_PATH)/spi/
endif

# Include path
INC_PATH += \
	$(MY_PATH)/ \
	./ \
	$(MY_SOURCE_PATH)/ \
	$(BRDS_PATH)/ \
	$(FREERTOS_PATH)/ \
	$(FREERTOS_PATH)/include/ \
	$(FREERTOS_PATH)/portable/GCC/ARM_CM4F \
	$(PLUS_TCP_PATH)/ \
	$(PROTOCOLS_PATH)/include/ \
	$(BOOTLOADER_PATH)/ \
	$(CUR_PATH)/ASF/sam/utils/ \
	$(CUR_PATH)/ASF/sam/utils/header_files/ \
	$(CUR_PATH)/ASF/sam/utils/cmsis/sam4e/include/ \
	$(CUR_PATH)/ASF/common/utils/stdio/stdio_serial/ \
	$(CUR_PATH)/ASF/common/utils/ \
	$(CUR_PATH)/ASF/common/boards/ \
	$(CUR_PATH)/ASF/sam/boards/ \
	$(CUR_PATH)/ASF/sam/boards/sam4e_xplained_pro/ \
	$(CUR_PATH)/ASF/sam/utils/preprocessor/ \
	$(CUR_PATH)/ASF/thirdparty/CMSIS/Include/ \
	$(CUR_PATH)/ASF/sam/utils/cmsis/sam4e/source/templates/ \
	$(CUR_PATH)/ASF/sam/utils/fpu/ \
	$(CUR_PATH)/ASF/sam/drivers/uart/ \
	$(CUR_PATH)/ASF/sam/drivers/usart/ \
	$(UTIL_PATH)/ \
	$(UTIL_PATH)/PREPROCESSOR/ \
	$(UTIL_PATH)/cmsis/sam4e/include/ \
	$(CUR_PATH)/ASF/sam/utils/cmsis/sam4e/source/templates/ \
	$(UTIL_PATH)/cmsis/include/ \
	$(UTIL_PATH)/fpu/ \
	$(UTIL_PATH)/interrupt/ \
	$(PLUS_TCP_PATH)/include/ \
	$(PLUS_TCP_PATH)/source/portable/Compiler/GCC/ \
	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/DriverSAM/ \
	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/include/ \
	$(SERVICE_PATH)/ioport/ \
	$(SERVICE_PATH)/ioport/sam0/ \
	$(SERVICE_PATH)/clock/ \
	$(SERVICE_PATH)/flash_efc/ \
	$(SERVICE_PATH)/serial/ \
	$(DRVR_PATH)/ \
	$(DRVR_PATH)/gmac/ \
	$(DRVR_PATH)/pmc/ \
	$(DRVR_PATH)/matrix/ \
	$(DRVR_PATH)/gpio/ \
	$(DRVR_PATH)/wdt/ \
	$(DRVR_PATH)/pio/ \
	$(DRVR_PATH)/efc/ \
	$(DRVR_PATH)/tc/ \
	$(DRVR_PATH)/pdc/ \
	$(DRVR_PATH)/hsmci/ \
	$(DRVR_PATH)/ebi/smc/ \
	$(DRVR_PATH)/phy/ksz8051mnl/ \
	$(COMMON_UTILS_PATH)/include/ \
	$(UTILITIES_PATH)/include/ \
	$(UTILITIES_PATH)/

# S_SRCS

C_SRCS += \
	main.c \
	NotificationTests.c \
	hr_gettime.c \
	CreateAndVerifyExampleFiles.c \
	ff_stdio_tests_with_cwd.c

INC_PATH += \
	$(COMPONENTS_PATH)/ \
	$(COMPONENTS_PATH)/memory/sd_mmc/ \
	$(SERVICE_PATH)/delay \
	$(SERVICE_PATH)/delay/sam/

C_SRCS += \
	$(COMPONENTS_PATH)/memory/sd_mmc/sd_mmc.c \
	$(SERVICE_PATH)/delay/sam/cycle_counter.c \
	$(DRVR_PATH)/pdc/pdc.c \
	$(DRVR_PATH)/hsmci/hsmci.c

C_SRCS += \
	$(FREERTOS_PATH)/tasks.c \
	$(FREERTOS_PATH)/queue.c \
	$(FREERTOS_PATH)/list.c \
	$(FREERTOS_PATH)/event_groups.c \
	$(FREERTOS_PATH)/timers.c \
	$(FREERTOS_PATH)/portable/MemMang/heap_5.c \
	$(FREERTOS_PATH)/portable/GCC/ARM_CM4F/port.c
C_SRCS += \
	$(PLUS_TCP_PATH)/source/FreeRTOS_ARP.c \
	$(PLUS_TCP_PATH)/source/FreeRTOS_DHCP.c \
	$(PLUS_TCP_PATH)/source/FreeRTOS_DNS.c \
	$(PLUS_TCP_PATH)/source/FreeRTOS_IP.c \
	$(PLUS_TCP_PATH)/source/FreeRTOS_Sockets.c \
	$(PLUS_TCP_PATH)/source/FreeRTOS_Stream_Buffer.c \
	$(PLUS_TCP_PATH)/source/FreeRTOS_TCP_IP.c \
	$(PLUS_TCP_PATH)/source/FreeRTOS_TCP_WIN.c \
	$(PLUS_TCP_PATH)/source/FreeRTOS_UDP_IP.c \
	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/Common/phyHandling.c \
	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/DriverSAM/gmac_SAM.c \
	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/DriverSAM/NetworkInterface.c \
	$(PLUS_TCP_PATH)/source/protocols/FTP/FreeRTOS_FTP_commands.c \
	$(PLUS_TCP_PATH)/source/protocols/FTP/FreeRTOS_FTP_server.c \
	$(PLUS_TCP_PATH)/source/protocols/HTTP/FreeRTOS_HTTP_commands.c \
	$(PLUS_TCP_PATH)/source/protocols/HTTP/FreeRTOS_HTTP_server.c \
	$(PLUS_TCP_PATH)/source/protocols/Common/FreeRTOS_TCP_server.c \
	$(PLUS_TCP_PATH)/source/portable/BufferManagement/BufferAllocation_2.c

ifeq ($(ipconfigMULTI_INTERFACE),true)
	C_SRCS += \
		$(PLUS_TCP_PATH)/source/FreeRTOS_Routing.c \
		$(PLUS_TCP_PATH)/source/portable/NetworkInterface/loopback/NetworkInterface.c
endif

ifeq ($(ipconfigUSE_IPv6),true)
	C_SRCS += \
		$(PLUS_TCP_PATH)/source/FreeRTOS_ND.c
endif

ifeq ($(makeUSE_MICREL_KSZ8851),true)
	C_SRCS += \
		$(PLUS_TCP_PATH)/source/portable/NetworkInterface/ksz8851snl/ksz8851snl.c \
		$(DRVR_PATH)/PIO/pio_handler.c \
		$(DRVR_PATH)/SPI/spi.c \
		$(PLUS_TCP_PATH)/source/portable/NetworkInterface/ksz8851snl/NetworkInterface.c
else
	C_SRCS += \
		$(PLUS_TCP_PATH)/source/portable/NetworkInterface/DriverSAM/NetworkInterface.c
endif

C_SRCS += \
	$(SERVICE_PATH)/clock/sam4e/sysclk.c \
	$(CUR_PATH)/ASF/common/utils/interrupt/interrupt_sam_nvic.c \
	$(CUR_PATH)/ASF/sam/boards/sam4e_xplained_pro/init.c \
	$(DRVR_PATH)/pmc/pmc.c \
	$(DRVR_PATH)/pmc/sleep.c \
	$(DRVR_PATH)/pio/pio.c \
	$(DRVR_PATH)/tc/tc.c \
	$(DRVR_PATH)/ebi/smc/smc.c \
	$(CUR_PATH)/ASF/sam/utils/cmsis/sam4e/source/templates/exceptions.c \
	$(CUR_PATH)/ASF/sam/utils/cmsis/sam4e/source/templates/gcc/startup_sam4e.c \
	$(CUR_PATH)/ASF/sam/utils/cmsis/sam4e/source/templates/system_sam4e.c \
	$(CUR_PATH)/ASF/sam/utils/syscalls/gcc/syscalls.c \
	$(COMMON_UTILS_PATH)/memcpy.c \
	$(COMMON_UTILS_PATH)/printf-stdarg.c \
	$(COMMON_UTILS_PATH)/UDPLoggingPrintf.c \
	$(UTILITIES_PATH)/eventLogging.c \
	$(UTILITIES_PATH)/telnet.c \
	$(COMMON_UTILS_PATH)/plus_echo_client.c \
	$(COMMON_UTILS_PATH)/plus_echo_server.c

C_SRCS += \
	$(PLUS_FAT_PATH)/ff_crc.c \
	$(PLUS_FAT_PATH)/ff_dir.c \
	$(PLUS_FAT_PATH)/ff_error.c \
	$(PLUS_FAT_PATH)/ff_fat.c \
	$(PLUS_FAT_PATH)/ff_file.c \
	$(PLUS_FAT_PATH)/ff_ioman.c \
	$(PLUS_FAT_PATH)/ff_memory.c \
	$(PLUS_FAT_PATH)/ff_format.c \
	$(PLUS_FAT_PATH)/ff_string.c \
	$(PLUS_FAT_PATH)/ff_stdio.c \
	$(PLUS_FAT_PATH)/ff_sys.c \
	$(PLUS_FAT_PATH)/ff_time.c \
	$(PLUS_FAT_PATH)/ff_locking.c \
	$(PLUS_FAT_PATH)/portable/ATSAM4E/ff_sddisk.c

ifeq ($(USE_IPERF),true)
	DEFS += -DUSE_IPERF=1
	C_SRCS += \
		$(COMMON_UTILS_PATH)/iperf_task_v3_0d.c
endif

# "makeUSE_TICKLESS_IDLE" if true then tickless idle will be used
ifeq ($(makeUSE_TICKLESS_IDLE),true)
	DEFS += -DmakeUSE_TICKLESS_IDLE=1
	C_SRCS += \
		$(FREERTOS_PATH)/portable/gcc/sam4e/tc_timer.c
endif

LINKER_SCRIPT=$(CUR_PATH)/sam4e_flash.ld


TARGET = RTOSDemo.elf

# Later, use -Os
OPTIMIZATION = -Os
# OPTIMIZATION = -O0

#HT Do not use it for now
DEBUG=-g3

C_EXTRA_FLAGS= \
	-mthumb \
	-DDEBUG \
	-Dscanf=iscanf \
	-DARM_MATH_CM4=true \
	-Dprintf=iprintf \
	-D__FREERTOS__ \
	-mfloat-abi=softfp \
	-mfpu=fpv4-sp-d16 \
	-mcpu=cortex-m4 \
	-mlong-calls \
	-Wno-psabi \
	-pipe

#	-Wredundant-decls

C_ONLY_FLAGS= \
	-x c

#	-Wl,--defsym=__stack_size__=512
#	-Wl,--defsym=PROGRAM_START_OFFSET=$(PROGRAM_START_OFFSET)
#	-l_nand_flash_cortexm4 \
#	-L$(COMPONENTS_PATH)/memory/nand_flash/nand_flash_ebi/ftl_lib/gcc \

LD_EXTRA_FLAGS= \
	-mthumb \
	-Wl,-Map="$(MY_PATH)/RTOSDemo.map" \
	-Wl,--start-group \
	-larm_cortexM4lf_math \
	-lm \
	-L$(CUR_PATH)/ASF/thirdparty/CMSIS/Lib/GCC \
	-Wl,--end-group \
	-Wl,--gc-sections \
	-mcpu=cortex-m4 \
	-Wl,--entry=Reset_Handler \
	-Wl,--cref \
	-z muldefs

BOOT_HEX=E:/Home/plus/sam4e/Framework/boot_plus/sam4e_boot.h88

#	-Wl,-Map="GccBoardProject1.map"
#	-L"../cmsis/linkerScripts"
#	-L"../src/ASF/thirdparty/CMSIS/Lib/GCC"
