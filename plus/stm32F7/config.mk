#
#	config.mk for 'stm32F7xx.elf' for STM32F7xx
#

ipconfigMULTI_INTERFACE=false
ipconfigUSE_IPv6=false

#GCC_BIN=C:/Ac6/SystemWorkbench/plugins/fr.ac6.mcu.externaltools.arm-none.win32_1.7.0.201602121829/tools/compiler/bin
#GCC_BIN=C:/Ac6/7.2.1-1.1-20180401-0515/bin
GCC_PREFIX=arm-none-eabi

GCC_BIN=$(HOME)/Ac6/SystemWorkbench/plugins/fr.ac6.mcu.externaltools.arm-none.linux64_1.17.0.201812190825/tools/compiler/bin

C_SRCS =
S_SRCS =

# Four choices for network and FAT driver:

#ST_Library = Abe_Library
ST_Library = ST_Library

USE_FREERTOS_PLUS=true

# ChaN's FS versus +FAT
USE_PLUS_FAT=true

TCP_SOURCE=
#TCP_SOURCE=/Source

USE_IPERF=false

SMALL_SIZE=false

USE_LOG_EVENT=true

ipconfigUSE_HTTP=false
ipconfigUSE_FTP=true

USE_TCP_MEM_STATS=false

ifeq ($(ipconfigMULTI_INTERFACE),false)
	ipconfigUSE_IPv6=false
endif

# The word 'fireworks' here can be replaced by any other string
# Others would write 'foo'
ROOT_PATH = $(subst /fireworks,,$(abspath ../fireworks))
PRJ_PATH = $(subst /fireworks,,$(abspath ./fireworks))

# The path where this Makefile is located:
DEMO_PATH = $(PRJ_PATH)/Src

LDLIBS=

DEFS =

DEFS += -DDEBUG
DEFS += -DCORE_CM4F
DEFS += -DCR_INTEGER_PRINTF
DEFS += -DUSE_FREERTOS_PLUS=1
DEFS += -DGNUC_NEED_SBRK=1

LD_EXTRA_FLAGS =

FREERTOS_ROOT = \
	$(ROOT_PATH)/Framework/FreeRTOS_v9.0.0

DEMOS_ROOT = \
	$(ROOT_PATH)/Common

FREERTOS_PATH = \
	$(FREERTOS_ROOT)

FREERTOS_PORT_PATH = \
	$(FREERTOS_ROOT)/portable/GCC/ARM_CM7/r0p1

TCP_UTILITIES=$(ROOT_PATH)/framework/FreeRTOS-Plus-TCP-multi/tools/tcp_utilities

# The following can not be used for this M7_r0p1 :
#
# FREERTOS_PORT_PATH = \
# 	$(FREERTOS_ROOT)/portable/GCC/ARM_CM4F

INC_RTOS = \
	$(PRJ_PATH)/$(ST_Library)\include/ \
	$(PRJ_PATH)/$(ST_Library)\include/Legacy/ \
	$(PRJ_PATH)/$(ST_Library)/CMSIS/Include/ \
	$(PRJ_PATH)/$(ST_Library)/CMSIS/Device/ST/STM32F7xx/Include/ \
	$(PRJ_PATH)/$(ST_Library)/include/ \
	$(FREERTOS_PORT_PATH)/ \
	$(FREERTOS_ROOT)/include/ \
	$(FREERTOS_ROOT)/CMSIS_RTOS/ \
	$(TCP_UTILITIES)/include/

PLUS_FAT_PATH = \
	$(ROOT_PATH)/Framework/FreeRTOS-Plus-FAT

ifeq ($(ipconfigMULTI_INTERFACE),true)
	DEFS += -D ipconfigMULTI_INTERFACE=1
	DEFS += -D ipconfigUSE_LOOPBACK=1
	PLUS_TCP_PATH = \
		$(ROOT_PATH)/Framework/FreeRTOS-Plus-TCP-multi
else
	DEFS += -D ipconfigMULTI_INTERFACE=0
	ipconfigUSE_IPv6=false
	PLUS_TCP_PATH = \
		$(ROOT_PATH)/Framework/FreeRTOS-Plus-TCP
endif

ifeq ($(ipconfigUSE_IPv6),true)
	DEFS += -D ipconfigUSE_IPv6=1
else
	DEFS += -D ipconfigUSE_IPv6=0
endif

PROTOCOLS_PATH = \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/Protocols

#PLUS_TCP_PATH = \
#	$(PRJ_PATH)/Middlewares/Third_Party/FreeRTOS-Plus-TCP

INC_PATH = \
	$(PRJ_PATH)/ \
	$(PRJ_PATH)/Inc/ \
	$(INC_RTOS)/ \
	$(PLUS_TCP_PATH)/include/ \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/Compiler/GCC/ \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/protocols/include/ \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/NetworkInterface/include/ \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/NetworkInterface/STM32Fxx/ \
	$(PLUS_TCP_PATH)/tools/tcp_utilities/include/ \
	$(PRJ_PATH)/Third_Party/USB/ \
	$(ROOT_PATH)/Framework/Utilities/include/ \
	$(ROOT_PATH)/Common/Utilities/ \
	$(ROOT_PATH)/Common/Utilities/include/

S_SRCS += \
	$(PRJ_PATH)/Src/startup_stm32f746xx.S

C_SRCS += \
	$(PRJ_PATH)/Src/main.c \
	$(PRJ_PATH)/Src/memcpy.c \
	$(PRJ_PATH)/Src/stm32f7xx_it.c \
	$(ROOT_PATH)/Common/Utilities/printf-stdarg.c \
	$(PRJ_PATH)/Src/hr_gettime.c \
	$(DEMOS_ROOT)/FreeRTOS_Plus_FAT_Demos/CreateAndVerifyExampleFiles.c \
	$(DEMOS_ROOT)/FreeRTOS_Plus_FAT_Demos/test/ff_stdio_tests_with_cwd.c \
	$(PRJ_PATH)/Src/system_stm32f7xx.c \
	$(PRJ_PATH)/Src/udp_packets.c

C_SRCS += \
	$(FREERTOS_PATH)/event_groups.c \
	$(FREERTOS_PATH)/list.c \
	$(FREERTOS_PATH)/queue.c \
	$(FREERTOS_PATH)/tasks.c \
	$(FREERTOS_PATH)/timers.c \
	$(FREERTOS_PATH)/portable/MemMang/heap_5.c \
	$(FREERTOS_PORT_PATH)/port.c

DEFS += -D STM32F7xx=1
DEFS += -D STM32F746xx=1

ifeq ($(USE_PLUS_FAT),true)
	DEFS += -DUSE_PLUS_FAT=1
	DEFS += -DMX_FILE_SYS=3
	DEFS += -DSDIO_USES_DMA=1

	ipconfigUSE_HTTP=true

	INC_PATH += \
		$(PLUS_FAT_PATH)/portable/common/ \
		$(PLUS_FAT_PATH)/portable/stm32f7xx/ \
		$(PLUS_FAT_PATH)/include/
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
		$(PLUS_FAT_PATH)/ff_locking.c \
		$(PLUS_FAT_PATH)/ff_time.c \
 		$(PLUS_FAT_PATH)/portable/STM32F7xx/ff_sddisk.c \
 		$(PLUS_FAT_PATH)/portable/STM32F7xx/stm32f7xx_hal_sd.c \
		$(PLUS_FAT_PATH)/portable/common/ff_ramdisk.c \
		$(PLUS_FAT_PATH)/ff_stdio.c \
		$(PLUS_FAT_PATH)/ff_sys.c
endif


ifeq ($(ipconfigUSE_HTTP),true)
	DEFS += -DipconfigUSE_HTTP=1
	DEFS += -DipconfigUSE_FTP=1
	ifeq ($(USES_LWIP),true)
		C_SRCS += \
			$(PROTOCOLS_PATH)/Common/tcp_lwip.c
	endif

	C_SRCS += \
		$(PROTOCOLS_PATH)/Common/FreeRTOS_TCP_server.c \
		$(PROTOCOLS_PATH)/HTTP/FreeRTOS_HTTP_server.c \
		$(PROTOCOLS_PATH)/HTTP/FreeRTOS_HTTP_commands.c \
		$(PROTOCOLS_PATH)/FTP/FreeRTOS_FTP_server.c \
		$(PROTOCOLS_PATH)/FTP/FreeRTOS_FTP_commands.c \
		$(PROTOCOLS_PATH)/NTP/NTPDemo.c
endif

ifeq ($(USE_IPERF),true)
	DEFS += -DUSE_IPERF=1
	C_SRCS += \
		$(ROOT_PATH)/Common/Utilities/iperf_task_v3_0d.c
else
	DEFS += -DUSE_IPERF=0
endif

ifeq ($(USE_LOG_EVENT),true)
	DEFS += -D USE_LOG_EVENT=1
	DEFS += -D LOG_EVENT_COUNT=100
	DEFS += -D LOG_EVENT_NAME_LEN=40
	DEFS += -D STATIC_LOG_MEMORY=1
	DEFS += -D EVENT_MAY_WRAP=1
	C_SRCS += \
		$(ROOT_PATH)/Framework/Utilities/eventLogging.c
endif

DEFS += -DLOGBUF_MAX_UNITS=64
DEFS += -DLOGBUF_AVG_LEN=72

#	$(PRJ_PATH)/Framework/Utilities/mySprintf.c


C_SRCS += \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/FreeRTOS_ARP.c \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/FreeRTOS_DHCP.c \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/FreeRTOS_DNS.c \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/FreeRTOS_IP.c \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/FreeRTOS_Sockets.c \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/FreeRTOS_Stream_Buffer.c \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/FreeRTOS_TCP_IP.c \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/FreeRTOS_TCP_WIN.c \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/FreeRTOS_UDP_IP.c \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/BufferManagement/BufferAllocation_1.c \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/NetworkInterface/STM32Fxx/NetworkInterface.c \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/NetworkInterface/STM32Fxx/stm32fxx_hal_eth.c \
	$(ROOT_PATH)/Common/Utilities/UDPLoggingPrintf.c \
	$(TCP_UTILITIES)/date_and_time.c \
	$(ROOT_PATH)/Common/Utilities/plus_echo_client.c \
	$(ROOT_PATH)/Common/Utilities/plus_echo_server.c


ifeq ($(USE_TCP_MEM_STATS),true)
	C_SRCS += \
		$(TCP_UTILITIES)/tcp_mem_stats.c
	DEFS += -DipconfigUSE_TCP_MEM_STATS
endif

C_SRCS += \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/NetworkInterface/Common/phyHandling.c

ifeq ($(ipconfigMULTI_INTERFACE),true)
	C_SRCS += \
		$(PLUS_TCP_PATH)$(TCP_SOURCE)/FreeRTOS_Routing.c \
		$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/NetworkInterface/loopback/NetworkInterface.c
endif

ifeq ($(ipconfigUSE_IPv6),true)
	C_SRCS += \
		$(PLUS_TCP_PATH)$(TCP_SOURCE)/FreeRTOS_ND.c
endif

C_SRCS += \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_adc.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_adc_ex.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_cortex.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_dac.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_dac_ex.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_dcmi.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_dma.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_dma_ex.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_flash.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_flash_ex.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_ll_sdmmc.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_rcc.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_rcc_ex.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_tim.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_rng.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_gpio.c
	
#	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_i2c.c \
#	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_nor.c \
#	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_pwr.c \
#	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_pwr_ex.c \
#	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_rtc.c \
#	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_rtc_ex.c \
#	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_sram.c \
#	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_tim_ex.c \
#	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_uart.c \
#	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_wwdg.c

#	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_ll_fsmc.c
#	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_flash_ramfunc.c

C_SRCS += \
	$(CHIP_SRC)

HW_FLAGS=

#	-nostdlib

#    arm-none-eabi-gcc
#        -nostartfiles
#        -Tstm32f407.ld
#        -Wl,-Map=stm32_example.map
#        -mcpu=cortex-m4
#        -mthumb
#        -o
#        stm32_example.elf
#        .build/src/system_stm32f4xx_eval.o
#
#        // a few more .o files ...
#
#        .build/src/startup-stm32f407.o

LD_EXTRA_FLAGS=\
	-nostartfiles \
	-Wl,-Map=stm32F7xx.map \
	-mcpu=cortex-m7 \
	-mthumb \
	-Wl,--gc-sections


# LINKER_SCRIPT=stm32f746.ld
LINKER_SCRIPT=STM32F746NGHx_FLASH.ld

ifeq ($(SMALL_SIZE),true)
	DEFS += -D ipconfigHAS_PRINTF=0
	DEFS += -D ipconfigHAS_DEBUG_PRINTF=0
	OPTIMIZATION = -Os -fno-builtin-memcpy -fno-builtin-memset
else
	OPTIMIZATION = -Os -fno-builtin-memcpy -fno-builtin-memset
#	OPTIMIZATION = -O0 -fno-builtin-memcpy -fno-builtin-memset
endif

TARGET = $(PRJ_PATH)/stm32F7xx.elf

WARNINGS = -Wall

DEBUG = -g3

C_EXTRA_FLAGS =\
	-mcpu=cortex-m7 \
	-mthumb \
	-mfloat-abi=softfp \
	-mfpu=vfpv4 \
	-fmessage-length=0 \
	-fno-builtin \
	-ffunction-sections \
	-fdata-sections
