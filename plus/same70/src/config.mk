#==============================================================================
#
#	RTOSDemo project
#
#==============================================================================
MY_BOARD=SAME70_XPLAINED

ipconfigMULTI_INTERFACE=false
ipconfigUSE_IPv6=false

# GCC_BIN=C:\PROGRA~2\Atmel\ATMELT~1\ARMGCC~1\Native\4.8.1443\ARM-GN~1\bin
# GCC_PREFIX=arm-none-eabi

#GCC_BIN=C:/PROGRA~2/Atmel/Studio/7.0/toolchain/arm/arm-gnu-toolchain/bin
GCC_PREFIX=arm-none-eabi
GCC_BIN=$(HOME)/gcc-arm-none-eabi-9-2020-q2-update/bin

makeUSE_TICKLESS_IDLE = false

USE_IPERF=true
USE_FREERTOS_FAT=false
USE_USB_CDC=false
USE_LOG_EVENT=false
STATIC_LOG_MEMORY=true

# Base paths
ROOT_PATH = $(subst /fireworks,,$(abspath ../../fireworks))
PRJ_PATH  = $(subst /fireworks,,$(abspath ../fireworks))
CUR_PATH  = $(subst /fireworks,,$(abspath ./fireworks))

MY_PATH = $(CUR_PATH)

DRVR_PATH = $(CUR_PATH)/ASF/sam/drivers

GEN_FRAMEWORK_PATH = $(ROOT_PATH)/Framework
# FREERTOS_PATH = $(GEN_FRAMEWORK_PATH)/FreeRTOS_v9.0.0
FREERTOS_PATH = $(GEN_FRAMEWORK_PATH)/FreeRTOS_v10.0.0
# D:\Home\plus\same70\FreeRTOS_Plus_TCP_and_FAT_ATSAME70\FreeRTOS/FreeRTOS_v10.0.0

RTOS_MEM_PATH = $(FREERTOS_PATH)/portable/MemMang
SERVICE_PATH = $(CUR_PATH)/ASF/common/services
UTIL_PATH = $(CUR_PATH)/ASF/sam/utils
COMPONENTS_PATH = $(CUR_PATH)/ASF/sam/components
CMSIS_INCLUDE_PATH = $(CUR_PATH)/ASF/sam/utils/cmsis/same70/include
PLUS_FAT_PATH = $(GEN_FRAMEWORK_PATH)/FreeRTOS-Plus-FAT
UTILITIES_PATH = $(GEN_FRAMEWORK_PATH)/Utilities
COMMON_UTILITIES = $(ROOT_PATH)/Framework/Utilities

# D:\Home\plus\same70\FreeRTOS_Plus_TCP_and_FAT_ATSAME70\FreeRTOS\FreeRTOS-Plus-TCP\include
ifeq ($(ipconfigMULTI_INTERFACE),true)
	DEFS += -D ipconfigMULTI_INTERFACE=1
	PLUS_TCP_PATH = \
		$(GEN_FRAMEWORK_PATH)/FreeRTOS-Plus-TCP-multi
else
	DEFS += -D ipconfigMULTI_INTERFACE=0
	PLUS_TCP_PATH = \
		$(GEN_FRAMEWORK_PATH)/FreeRTOS-Plus-TCP
endif

PROTOCOLS_PATH     = $(PLUS_TCP_PATH)/source/protocols

C_SRCS=
CPP_SRCS=
S_SRCS=

DEFS += -DBOARD=$(MY_BOARD)
DEFS += -DFREERTOS_USED
DEFS += -D__ATSAME70Q21__=1
DEFS += -D__SAME70Q21__=1

# SAME70 will be define automatically
# DEFS += -DSAME70=1

INC_PATH = 

INC_PATH += \
	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/DriverSAM/

INC_PATH += \
	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/include/
	
# Include path
INC_PATH += \
	$(MY_PATH)/ \
	./ \
	$(MY_SOURCE_PATH)/ \
	$(BRDS_PATH)/ \
	$(FREERTOS_PATH)/ \
	$(FREERTOS_PATH)/include/ \
	$(FREERTOS_PATH)/portable/GCC/ARM_CM7/r0p1/ \
	$(PROTOCOLS_PATH)/include/ \
	$(BOOTLOADER_PATH)/ \
	$(CUR_PATH)/ASF/sam/utils/ \
	$(CUR_PATH)/ASF/sam/utils/header_files/ \
	$(CUR_PATH)/ASF/sam/utils/cmsis/same70/include/ \
	$(CUR_PATH)/ASF/common/utils/stdio/stdio_serial/ \
	$(CUR_PATH)/ASF/common/utils/ \
	$(CUR_PATH)/ASF/common/boards/ \
	$(CUR_PATH)/ASF/sam/boards/ \
	$(CUR_PATH)/ASF/sam/boards/same70_xplained/ \
	$(CUR_PATH)/ASF/sam/utils/preprocessor/ \
	$(CUR_PATH)/ASF/thirdparty/CMSIS/Include/ \
	$(CUR_PATH)/ASF/sam/utils/cmsis/same70/source/templates/ \
	$(CUR_PATH)/ASF/sam/utils/fpu/ \
	$(CUR_PATH)/ASF/sam/drivers/uart/ \
	$(CUR_PATH)/ASF/sam/drivers/usart/ \
	$(UTIL_PATH)/ \
	$(UTIL_PATH)/PREPROCESSOR/ \
	$(UTIL_PATH)/cmsis/same70/include/ \
	$(CUR_PATH)/ASF/sam/utils/cmsis/same70/source/templates/ \
	$(UTIL_PATH)/cmsis/include/ \
	$(UTIL_PATH)/fpu/ \
	$(UTIL_PATH)/interrupt/ \
	$(PLUS_TCP_PATH)/ \
	$(PLUS_TCP_PATH)/include/ \
	$(PLUS_TCP_PATH)/source/portable/Compiler/GCC/ \
	$(SERVICE_PATH)/ioport/ \
	$(SERVICE_PATH)/ioport/sam0/ \
	$(SERVICE_PATH)/clock/ \
	$(SERVICE_PATH)/gpio/ \
	$(SERVICE_PATH)/flash_efc/ \
	$(SERVICE_PATH)/serial/ \
	$(SERVICE_PATH)/sleepmgr/ \
	$(DRVR_PATH)/ \
	$(DRVR_PATH)/gmac/ \
	$(DRVR_PATH)/pmc/ \
	$(DRVR_PATH)/matrix/ \
	$(DRVR_PATH)/gpio/ \
	$(DRVR_PATH)/wdt/ \
	$(DRVR_PATH)/pio/ \
	$(DRVR_PATH)/efc/ \
	$(DRVR_PATH)/mpu/ \
	$(DRVR_PATH)/tc/ \
	$(DRVR_PATH)/pdc/ \
	$(DRVR_PATH)/sdramc/ \
	$(DRVR_PATH)/hsmci/ \
	$(DRVR_PATH)/ebi/smc/ \
	$(DRVR_PATH)/phy/ksz8051mnl/ \
	$(COMMON_UTILITIES)/include/ \
	$(UTILITIES_PATH)/include/ \
	$(UTILITIES_PATH)/

# S_SRCS

C_SRCS += \
	main.c \
	hr_gettime.c

INC_PATH += \
	$(COMPONENTS_PATH)/ \
	$(SERVICE_PATH)/delay \
	$(SERVICE_PATH)/delay/sam/ \
	$(COMPONENTS_PATH)/memory/sdram/is42s16100e/



#	INC_PATH += \
#		$(COMPONENTS_PATH)/memory/sd_mmc/ \
#	C_SRCS += \
#		$(COMPONENTS_PATH)/memory/sd_mmc/sd_mmc.c \
#		$(DRVR_PATH)/PDC/pdc.c \
#		$(DRVR_PATH)/hsmci/hsmci.c

C_SRCS += \
	$(SERVICE_PATH)/delay/sam/cycle_counter.c

C_SRCS += \
	$(FREERTOS_PATH)/tasks.c \
	$(FREERTOS_PATH)/queue.c \
	$(FREERTOS_PATH)/list.c \
	$(FREERTOS_PATH)/event_groups.c \
	$(FREERTOS_PATH)/timers.c \
	$(FREERTOS_PATH)/portable/MemMang/heap_5.c \
	$(FREERTOS_PATH)/portable/GCC/ARM_CM7/r0p1/port.c

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
	$(PLUS_TCP_PATH)/source/portable/BufferManagement/BufferAllocation_1.c

#	$(PLUS_TCP_PATH)/source/portable/BufferManagement/BufferAllocation_2.c

ifeq ($(ipconfigMULTI_INTERFACE),true)
	C_SRCS += \
		$(PLUS_TCP_PATH)/source/FreeRTOS_Routing.c \
		$(PLUS_TCP_PATH)/source/portable/NetworkInterface/loopback/NetworkInterface.c
endif

ifeq ($(USE_USB_CDC),true)

	# Do not use the Sleep manager
	DEFS += -DUDD_NO_SLEEP_MGR=1

	#tell the application that USB/CDC will be used
	DEFS += -DUSE_USB_CDC=1

	INC_PATH += \
		$(SERVICE_PATH)/usb \
		$(SERVICE_PATH)/usb/udc \
		$(SERVICE_PATH)/usb/class/cdc

	C_SRCS += \
		$(SERVICE_PATH)/usb/class/cdc/device/udi_cdc.c \
		$(SERVICE_PATH)/usb/class/cdc/device/udi_cdc_desc.c \
		$(SERVICE_PATH)/usb/udc/udc.c \
		$(DRVR_PATH)/udp/udp_device.c

endif	# ifeq ($(USE_USB_CDC),true)

C_SRCS += \
	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/DriverSAM/gmac_SAM.c \
	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/DriverSAM/NetworkInterface.c

C_SRCS += \
	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/Common/phyHandling.c

C_SRCS += \
	$(SERVICE_PATH)/clock/same70/sysclk.c \
	$(CUR_PATH)/ASF/common/utils/interrupt/interrupt_sam_nvic.c \
	$(CUR_PATH)/ASF/sam/boards/same70_xplained/init.c \
	$(DRVR_PATH)/pmc/pmc.c \
	$(DRVR_PATH)/pmc/sleep.c \
	$(DRVR_PATH)/pio/pio.c \
	$(DRVR_PATH)/tc/tc.c \
	$(DRVR_PATH)/ebi/smc/smc.c \
	$(CUR_PATH)/ASF/sam/utils/cmsis/same70/source/templates/gcc/startup_same70.c \
	$(CUR_PATH)/ASF/sam/utils/cmsis/same70/source/templates/system_same70.c \
	$(CUR_PATH)/ASF/sam/utils/syscalls/gcc/syscalls.c \
	$(COMMON_UTILITIES)/UDPLoggingPrintf.c \
	$(COMMON_UTILITIES)/plus_echo_client.c \
	$(COMMON_UTILITIES)/plus_echo_server.c \
	$(COMMON_UTILITIES)/telnet.c \
	$(DRVR_PATH)/sdramc/sdramc.c \
	$(SERVICE_PATH)/sleepmgr/sam/sleepmgr.c \
	$(COMMON_UTILITIES)/printf-stdarg.c

#	$(CUR_PATH)/ASF/sam/utils/cmsis/same70/source/templates/exceptions.c
#	$(COMMON_UTILITIES)/memcpy.c


ifeq ($(USE_FREERTOS_FAT),true)
	INC_PATH += \
		$(PLUS_FAT_PATH)/include/ \
		$(PLUS_FAT_PATH)/portable/ATSAME70/ \
		$(PLUS_FAT_PATH)/portable/common/
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
		$(PLUS_FAT_PATH)/portable/ATSAME70/ff_sddisk.c \
		CreateAndVerifyExampleFiles.c \
		ff_stdio_tests_with_cwd.c \
		$(PLUS_TCP_PATH)/source/protocols/FTP/FreeRTOS_FTP_commands.c \
		$(PLUS_TCP_PATH)/source/protocols/FTP/FreeRTOS_FTP_server.c \
		$(PLUS_TCP_PATH)/source/protocols/HTTP/FreeRTOS_HTTP_commands.c \
		$(PLUS_TCP_PATH)/source/protocols/HTTP/FreeRTOS_HTTP_server.c \
		$(PLUS_TCP_PATH)/source/protocols/Common/FreeRTOS_TCP_server.c
	DEFS += -D USE_FREERTOS_FAT=1
endif


ifeq ($(USE_LOG_EVENT),true)
	DEFS += -DUSE_LOG_EVENT=1
	DEFS += -DLOG_EVENT_NAME_LEN=20
	DEFS += -DLOG_EVENT_COUNT=128
	C_SRCS += \
		$(COMMON_UTILITIES)/eventLogging.c
endif

ifeq ($(STATIC_LOG_MEMORY),true)
	DEFS += -DSTATIC_LOG_MEMORY=1
endif

ifeq ($(USE_IPERF),true)
	DEFS += -D USE_IPERF=1
	C_SRCS += \
		$(COMMON_UTILITIES)/iperf_task_v3_0d.c
else
	DEFS += -D USE_IPERF=0
endif

# "makeUSE_TICKLESS_IDLE" if true then tickless idle will be used
ifeq ($(makeUSE_TICKLESS_IDLE),true)
	DEFS += -DmakeUSE_TICKLESS_IDLE=1
	C_SRCS += \
		$(FREERTOS_PATH)/portable/gcc/sam4e/tc_timer.c
endif

# LINKER_SCRIPT=$(CUR_PATH)/ASF/sam/utils/linker_scripts/same70/same70q21/gcc/flash.ld
LINKER_SCRIPT=$(CUR_PATH)/samE70_flash.ld


TARGET = RTOSDemo.elf

# Later, use -Os
OPTIMIZATION = -O0
# OPTIMIZATION = -Os

#HT Do not use it for now
DEBUG=-g3

C_EXTRA_FLAGS= \
	-mthumb \
	-DDEBUG \
	-Dscanf=iscanf \
	-DARM_MATH_CM7=true \
	-Dprintf=iprintf \
	-D__FREERTOS__ \
    -mfloat-abi=softfp \
	-mfpu=fpv5-sp-d16 \
	-mcpu=cortex-m7 \
	-mlong-calls \
	-Wno-psabi \
	-pipe

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
	-lm \
	-L$(CUR_PATH)/ASF/thirdparty/CMSIS/Lib/GCC \
	-Wl,--end-group \
	-Wl,--gc-sections \
	-mcpu=cortex-m7 \
	-Wl,--entry=Reset_Handler \
	-Wl,--cref \
	-z muldefs
