#
#	config.mk for 'stm32F40.elf' for STM32F4xx
#

#C:\Ac6\SystemWorkbench\plugins\fr.ac6.mcu.externaltools.arm-none.win32_1.7.0.201602121829\tools\compiler\bin

#GCC_BIN=C:/Ac6/SystemWorkbench/plugins/fr.ac6.mcu.externaltools.arm-none.win32_1.7.0.201602121829/tools/compiler/bin
#GCC_BIN=C:/Ac6_v2.8/SystemWorkbench/plugins/fr.ac6.mcu.externaltools.arm-none.win32_1.17.0.201812190825/tools/compiler/bin
#GCC_BIN=C:/Ac6/7.2.1-1.1-20180401-0515/bin
GCC_PREFIX=arm-none-eabi

#GCC_BIN=/usr/bin
GCC_BIN=$(HOME)/Ac6/SystemWorkbench/plugins/fr.ac6.mcu.externaltools.arm-none.linux64_1.17.0.201812190825/tools/compiler/bin


C_SRCS =
S_SRCS =

# Four choices for network and FAT driver:

USE_FREERTOS_PLUS=true

ipconfigMULTI_INTERFACE=false
ipconfigUSE_IPv6=false

HAS_TCP_TESTER=true

ipconfigUSE_TCP_MEM_STATS=false

ipconfigTCP_IP_SANITY=false

USE_LINT=false

# ChaN's FS versus +FAT
USE_PLUS_FAT=false

HAS_ECHO_TEST=false

USE_IPERF=true

SMALL_SIZE=false

ipconfigUSE_HTTP=false
ipconfigUSE_FTP=false

USE_STM324xG_EVAL=false

USE_LOG_EVENT=false

USE_ECHO_TASK=true

HAS_SDRAM_TEST=false

USE_TELNET=true

# The word 'fireworks' here can be replaced by any other string
# Others would write 'foo'
ROOT_PATH = $(subst /fireworks,,$(abspath ../../fireworks))
PRJ_PATH = $(subst /fireworks,,$(abspath ../fireworks))
CUR_PATH = $(subst /fireworks,,$(abspath ./fireworks))

HOME_PATH = $(subst /fireworks,,$(abspath ../../../fireworks))
#AMAZON_ROOT = $(HOME_PATH)/amazon-freertos/amazon-freertos
AMAZON_ROOT = $(HOME_PATH)/amazon-freertos/amazon-freertos.dns_cache_size

# The path where this Makefile is located:
DEMO_PATH = $(PRJ_PATH)/Src

HAL_PATH = $(PRJ_PATH)/Drivers/STM32F4xx_HAL_Driver
#HAL_PATH = $(PRJ_PATH)/Drivers/STM32F4xx_HAL_Driver_1.24.0
CMSIS_PATH = $(HAL_PATH)/CMSIS

LDLIBS=

DEFS =

DEFS += -DDEBUG
DEFS += -DCORE_CM4F
DEFS += -DCR_INTEGER_PRINTF
DEFS += -DUSE_FREERTOS_PLUS=1
DEFS += -DGNUC_NEED_SBRK=1
DEFS += -DSTM32F4xx=1

LD_EXTRA_FLAGS =

FREERTOS_ROOT = \
	$(ROOT_PATH)/Framework/FreeRTOS_v9.0.0

DEMOS_ROOT = \
	$(ROOT_PATH)/Common

FREERTOS_PATH = \
	$(FREERTOS_ROOT)

ifeq ($(ipconfigMULTI_INTERFACE),true)
	DEFS += -DipconfigMULTI_INTERFACE=1
	PLUS_TCP_PATH = \
		$(ROOT_PATH)/Framework/FreeRTOS-Plus-TCP-multi
	LINT_INCLUDES=./includes_multi.lnt
else
	DEFS += -DipconfigMULTI_INTERFACE=0
	ipconfigUSE_IPv6=false
	PLUS_TCP_PATH = \
		$(ROOT_PATH)/Framework/FreeRTOS-Plus-TCP
	LINT_INCLUDES=./includes_single.lnt
endif

INC_RTOS = \
	$(PRJ_PATH)/Inc/ \
	$(HAL_PATH)/Inc/ \
	$(HAL_PATH)/Inc/Legacy/ \
	$(CMSIS_PATH)/Include/ \
	$(CMSIS_PATH)/Device/ST/STM32F4xx/Include/ \
	$(FREERTOS_ROOT)/portable/GCC/ARM_CM4F/ \
	$(FREERTOS_ROOT)/include/ \
	$(FREERTOS_ROOT)/CMSIS_RTOS/ \
	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/STM32Fxx/ \
	$(ROOT_PATH)/framework/MySource/

PLUS_FAT_PATH = \
	$(ROOT_PATH)/Framework/FreeRTOS-Plus-FAT

# ROOT_PATH = $(subst /fireworks,,$(abspath ../../fireworks))
# E:\Home\stm\stm32F40\Src\config.mk
# E:\Home\stm\


ifeq ($(ipconfigUSE_IPv6),true)
	DEFS += -DipconfigUSE_IPv6=1
else
	DEFS += -DipconfigUSE_IPv6=0
endif


ifeq ($(ipconfigTCP_IP_SANITY),true)
	DEFS += -DipconfigTCP_IP_SANITY=1
endif

PROTOCOLS_PATH = \
	$(PLUS_TCP_PATH)/source/protocols

#PLUS_TCP_PATH = \
#	$(PRJ_PATH)/Middlewares/Third_Party/FreeRTOS-Plus-TCP

INC_PATH = \
	$(INC_RTOS) \
	$(CUR_PATH)/ \
	$(FREERTOS_PATH)/portable/GCC/ARM_CM4F/ \
	$(PLUS_TCP_PATH)/include/ \
	$(PLUS_TCP_PATH)/source/portable/Compiler/GCC/ \
	$(PLUS_TCP_PATH)/source/protocols/include/ \
	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/include/ \
	$(CUR_PATH)/Third_Party/USB/ \
	$(ROOT_PATH)/Utilities/include/ \
	$(ROOT_PATH)/Framework/Utilities/include/ \
	$(ROOT_PATH)/Framework/Utilities/ \
	$(ROOT_PATH)/Common/Utilities/include/ \
	$(ROOT_PATH)/Common/Utilities/

S_SRCS += \
	$(CMSIS_PATH)/Device/ST/STM32F4xx/Source/Templates/gcc/startup_stm32f407xx.S

C_SRCS += \
	$(CUR_PATH)/main.c \
	$(CUR_PATH)/memcpy.c \
	$(CUR_PATH)/stm32f4xx_it.c \
	$(ROOT_PATH)/Common/Utilities/printf-stdarg.c \
	$(CUR_PATH)/hr_gettime.c \
	$(CMSIS_PATH)/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx.c

ifeq ($(HAS_SDRAM_TEST),true)
	C_SRCS += \
		$(CUR_PATH)/sdram_test.c
	DEFS += -DHAS_SDRAM_TEST=1
endif

ifeq ($(USE_ECHO_TASK),true)
	C_SRCS += \
		$(CUR_PATH)/echo_client.c
	DEFS += -DUSE_ECHO_TASK=1
else
	DEFS += -DUSE_ECHO_TASK=0
endif

#	$(CMSIS_PATH)/Device/ST/STM32F4xx/Source/Templates/system_stm32f4xx_non_cube
	

C_SRCS += \
	$(FREERTOS_PATH)/event_groups.c \
	$(FREERTOS_PATH)/list.c \
	$(FREERTOS_PATH)/queue.c \
	$(FREERTOS_PATH)/tasks.c \
	$(FREERTOS_PATH)/timers.c \
	$(FREERTOS_PATH)/portable/MemMang/heap_5.c \
	$(FREERTOS_PATH)/portable/GCC/ARM_CM4F/port.c

ifeq ($(USE_PLUS_FAT),true)
	DEFS += -DUSE_PLUS_FAT=1
	DEFS += -DMX_FILE_SYS=3
	DEFS += -DSDIO_USES_DMA=1

	ipconfigUSE_HTTP=true

	INC_PATH += \
		$(PLUS_FAT_PATH)/portable/common/ \
		$(PLUS_FAT_PATH)/portable/stm32f4xx/ \
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
		$(PLUS_FAT_PATH)/portable/common/ff_ramdisk.c \
		$(PLUS_FAT_PATH)/portable/STM32F4xx/ff_sddisk.c \
		$(PLUS_FAT_PATH)/portable/STM32F4xx/stm32f4xx_hal_sd.c \
		$(PLUS_FAT_PATH)/portable/STM32F4xx/stm32f4xx_ll_sdmmc.c \
		$(PLUS_FAT_PATH)/ff_stdio.c \
		$(PLUS_FAT_PATH)/ff_sys.c \
		$(DEMOS_ROOT)/FreeRTOS_Plus_FAT_Demos/CreateAndVerifyExampleFiles.c \
		$(DEMOS_ROOT)/FreeRTOS_Plus_FAT_Demos/test/ff_stdio_tests_with_cwd.c

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

ifeq ($(USE_STM324xG_EVAL),true)
	DEFS += -D USE_STM324xG_EVAL=1
	# for eval, use MII
	DEFS += -DipconfigUSE_RMII=0
else
	# for discovery, use RMII
	DEFS += -DipconfigUSE_RMII=1
endif

#  STM32F407xx
DEFS += -DSTM32F40_41xxx1
DEFS += -DSTM32F407xx=1

ifeq ($(USE_IPERF),true)
	DEFS += -D USE_IPERF=1
	C_SRCS += \
		$(ROOT_PATH)/Common/Utilities/iperf_task_v3_0d.c
else
	DEFS += -D USE_IPERF=0
endif

ifeq ($(USE_LOG_EVENT),true)
	DEFS += -DUSE_LOG_EVENT=1
	DEFS += -DLOG_EVENT_NAME_LEN=24
	DEFS += -DLOG_EVENT_COUNT=128
	DEFS += -DEVENT_MAY_WRAP=0
	C_SRCS += \
		$(ROOT_PATH)/Framework/Utilities/eventLogging.c
else
	DEFS += -D USE_LOG_EVENT=0
endif

ifeq ($(USE_TELNET),true)
	DEFS += -DUSE_TELNET=1
	C_SRCS += \
		$(ROOT_PATH)/Common/Utilities/telnet.c
else
	DEFS += -D USE_TELNET=0
endif

DEFS += -DLOGBUF_MAX_UNITS=100
DEFS += -DLOGBUF_AVG_LEN=72

#	$(PRJ_PATH)/Framework/Utilities/mySprintf.c


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
	$(PLUS_TCP_PATH)/source/portable/BufferManagement/BufferAllocation_2.c \
	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/STM32Fxx/NetworkInterface.c \
	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/STM32Fxx/stm32fxx_hal_eth.c \

ifeq ($(ipconfigUSE_TCP_MEM_STATS),true)
	DEFS += -DipconfigUSE_TCP_MEM_STATS=1
	C_SRCS += \
		$(PLUS_TCP_PATH)/tools/tcp_mem_stats.c
endif

ifeq ($(HAS_ECHO_TEST),true)
	DEFS += -DHAS_ECHO_TEST=1
	C_SRCS += \
		$(ROOT_PATH)/Common/Utilities/plus_echo_client.c \
		$(ROOT_PATH)/Common/Utilities/plus_echo_server.c
endif

ifeq ($(HAS_TCP_TESTER),true)
	DEFS += -DHAS_TCP_TESTER=1
	C_SRCS += \
			$(CUR_PATH)/tcp_tester.c
endif

LINT_C_FILES=

ifeq ($(USE_LINT),true)
	LINT_C_FILES=\
		$(PLUS_TCP_PATH)/source/FreeRTOS_Stream_Buffer.c
#		$(PLUS_TCP_PATH)/source/FreeRTOS_Sockets.c       \
#		$(PLUS_TCP_PATH)/source/FreeRTOS_TCP_WIN.c       \
#		$(PLUS_TCP_PATH)/source/FreeRTOS_DHCP.c          \
#		$(PLUS_TCP_PATH)/source/FreeRTOS_DNS.c           \
#		$(PLUS_TCP_PATH)/source/FreeRTOS_ARP.c           \
#		$(PLUS_TCP_PATH)/source/FreeRTOS_IP.c            \
#		$(PLUS_TCP_PATH)/source/FreeRTOS_UDP_IP.c        \
#		$(PLUS_TCP_PATH)/source/FreeRTOS_TCP_IP.c
	ifeq ($(ipconfigMULTI_INTERFACE),true)
		LINT_C_FILES+=\
			$(PLUS_TCP_PATH)/source/FreeRTOS_Routing.c
		ifeq ($(ipconfigUSE_IPv6),true)
			LINT_C_FILES+=\
				$(PLUS_TCP_PATH)/source/FreeRTOS_ND.c
		endif
	else
	endif
#	$(PLUS_TCP_PATH)/source/portable/Buffermanagement/BufferAllocation_2.c
#	$(PLUS_TCP_PATH)/source/portable/Buffermanagement/BufferAllocation_1.c
#	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/STM32Fxx/NetworkInterface.c
#	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/STM32Fxx/stm32fxx_hal_eth.c
endif

C_SRCS += \
	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/Common/phyHandling.c

ifeq ($(ipconfigMULTI_INTERFACE),true)
	C_SRCS += \
		$(PLUS_TCP_PATH)/source/FreeRTOS_Routing.c \
		$(PLUS_TCP_PATH)/source/portable/NetworkInterface/loopback/NetworkInterface.c
endif

ifeq ($(ipconfigUSE_IPv6),true)
	C_SRCS += \
		$(PLUS_TCP_PATH)/source/FreeRTOS_ND.c
endif

C_SRCS += \
	$(ROOT_PATH)/Common/Utilities/UDPLoggingPrintf.c \
	$(ROOT_PATH)/Common/Utilities/date_and_time.c

C_SRCS += \
	$(HAL_PATH)/Src/stm32f4xx_hal.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_adc.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_adc_ex.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_cortex.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_dac.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_dac_ex.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_dcmi.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_dma.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_dma_ex.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_flash.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_flash_ex.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_flash_ramfunc.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_gpio.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_i2c.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_nor.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_pwr.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_pwr_ex.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_rcc.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_rcc_ex.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_rng.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_rtc.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_rtc_ex.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_sram.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_tim.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_tim_ex.c \
	$(HAL_PATH)/Src/stm32f4xx_hal_uart.c \
	$(HAL_PATH)/Src/stm32f4xx_ll_fsmc.c

#	$(HAL_PATH)/Src/stm32f4xx_ll_sdmmc.c

C_SRCS += \
	$(CHIP_SRC)

HW_FLAGS=

#	-nostdlib

LD_EXTRA_FLAGS=\
	-Xlinker -Map="stm32F40.map" \
	-mcpu=cortex-m4 \
	-Wl,--gc-sections \
	-mthumb


LINKER_SCRIPT=stm32F40.ld

ifeq ($(SMALL_SIZE),true)
	DEFS += -D ipconfigHAS_PRINTF=0
	DEFS += -D ipconfigHAS_DEBUG_PRINTF=0
	OPTIMIZATION = -O0 -fno-builtin-memcpy -fno-builtin-memset
else
#	OPTIMIZATION = -Os -fno-builtin-memcpy -fno-builtin-memset
	OPTIMIZATION = -O0 -fno-builtin-memcpy -fno-builtin-memset
endif

TARGET = $(CUR_PATH)/stm32F40.elf

WARNINGS = -Wall -Wextra -Warray-bounds
#-Wundef

DEBUG = -g3

C_EXTRA_FLAGS =\
	-mcpu=cortex-m4 \
	-mthumb \
	-mfloat-abi=softfp \
	-mfpu=vfpv4 \
	-fmessage-length=0 \
	-fno-builtin \
	-ffunction-sections \
	-fdata-sections
