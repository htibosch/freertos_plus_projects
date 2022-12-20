#
#	config.mk for 'RTOSDemo.elf' for Zynq / Xilinx
#

# Four choices for network and FAT driver:

ipconfigMULTI_INTERFACE=true
ipconfigUSE_IPv6=true

TCP_SOURCE=
#TCP_SOURCE=/Source

USE_FREERTOS_PLUS=true

# ChaN's FS versus +FAT
USE_PLUS_FAT=true

USE_TCP_DEMO_CLI=true
ipconfigUSE_NTP_DEMO=true

# Fill in you Xilinx version

#	C:\Xilinx\SDK\2016.4\gnu\arm\nt\bin
#GCC_PREFIX=arm-xilinx-eabi
# XILINX_PATH=C:/Xilinx/SDK/2016.4
# GCC_BIN=$(XILINX_PATH)/gnu/arm/nt/bin

# GCC_PREFIX=aarch64-none-elf
# XILINX_PATH=C:/Xilinx/SDK/2017.1
# GCC_BIN=$(XILINX_PATH)/gnu/aarch64/nt/aarch64-none/bin


GCC_PREFIX=arm-none-eabi
XILINX_PATH=E:\Xilinx\SDK\2019.1
GCC_BIN=$(XILINX_PATH)/gnu/aarch32/nt/gcc-arm-none-eabi/bin

SMALL_SIZE=false

ipconfigUSE_WOLFSSL=false

ipconfigUSE_HTTP=false
ipconfigUSE_FTP=true

USE_PCAP=true

ipconfigUSE_TCP_NET_STAT=false

ipconfigUSE_TCP_MEM_STATS=false

USE_LOG_EVENT=true

USE_TELNET=true

USE_NTOP_TEST=false

USE_IPERF=true

ROOT_PATH = $(subst /fireworks,,$(abspath ../../fireworks))
PRJ_PATH = $(subst /fireworks,,$(abspath ../fireworks))
CUR_PATH = $(subst /fireworks,,$(abspath ./fireworks))

HOME_PATH = $(subst /fireworks,,$(abspath ../../../fireworks))
AMAZON_ROOT = $(HOME_PATH)/amazon-freertos/amazon-freertos

# FreeRTOS_PATH = $(ROOT_PATH)/framework/FreeRTOSV8.2.1
# FreeRTOS_PATH = $(ROOT_PATH)/framework/FreeRTOS_V8.2.2
# FreeRTOS_PATH = $(ROOT_PATH)/framework/FreeRTOSV8.2.3
# FreeRTOS_PATH = $(ROOT_PATH)/framework/FreeRTOS_v9.0.0

FreeRTOS_PATH = $(ROOT_PATH)/framework/FreeRTOS_V10.4.3

# FreeRTOS_PATH = $(ROOT_PATH)/framework/FreeRTOS_V10.4.4

#FreeRTOS_PATH = $(ROOT_PATH)/framework/FreeRTOS_v10.2.1
#FreeRTOS_PATH = $(ROOT_PATH)/framework/FreeRTOS_v10.0.0
#FreeRTOS_PATH = $(ROOT_PATH)/framework/FreeRTOS_V10.2.0

# FreeRTOS_v10.0.0
# FreeRTOS_V10.1.1
# FreeRTOS_V10.2.0
# FreeRTOS_V10.2.1
# FreeRTOS_V8.1.2
# FreeRTOS_V8.2.0
# FreeRTOS_V8.2.1
# FreeRTOS_V8.2.2
# FreeRTOS_V8.2.3
# FreeRTOS_v9.0.0
# FreeRTOS_V9.0.0rc2

Utilities_PATH = $(ROOT_PATH)/framework/Utilities
MYSOURCE_PATH = $(ROOT_PATH)/framework/MySource
CommonUtilities_PATH = $(ROOT_PATH)/Common/Utilities
# The path where this Makefile is located:
DEMO_PATH = $(PRJ_PATH)/RTOSDemo
#WOLFSSL_PATH = $(ROOT_PATH)/framework/wolfssl
#WOLFSSL_PATH = $(ROOT_PATH)/framework/wolfssl-3.10.4
WOLFSSL_PATH = $(ROOT_PATH)/framework/wolfssl.master

#LDLIBS=-Wl,--start-group,-lxil,-lgcc,-lc,--end-group
LDLIBS=-Wl,--start-group,-lgcc,-lc,--end-group

CONFIG_USE_LWIP=false
USE_TCP_TESTER=false

DEFS =

LD_EXTRA_FLAGS =-nostartfiles \
	-Xlinker -Map=RTOSDemo.map \
	-Xlinker --gc-sections \
	-Wl,--build-id=none


INC_RTOS = \
	$(FreeRTOS_PATH)/include/private \
	$(FreeRTOS_PATH)/include

CORTEX_PATH = \
	$(PRJ_PATH)/RTOSDemo_bsp/ps7_cortexa9_0/include

LIBS =

LIB_PATH = \
	$(PRJ_PATH)/RTOSDemo_bsp/ps7_cortexa9_0/lib

PLUS_FAT_PATH = \
	$(ROOT_PATH)/framework/FreeRTOS-Plus-FAT

#PLUS_FAT_PATH = \
#	$(ROOT_PATH)/../amazon-freertos/Renesas_FAT

ZYNQ_PORTABLE_PATH = \
	$(PLUS_FAT_PATH)/portable/Zynq.2019.3
#	$(ROOT_PATH)/../amazon-freertos/Lab-Project-FreeRTOS-FAT/portable/Zynq.2019.3

#	$(PLUS_FAT_PATH)/portable/Zynq.2019.3
#	$(PLUS_FAT_PATH)/portable/Zynq.modi

#	$(PLUS_FAT_PATH)/portable/zynq/

LINKER_SCRIPT=$(DEMO_PATH)/src/lscript.ld

INCLUDE_PS7_INIT=false

# Use the standard pvPortMalloc()/vPortFree()
# myMalloc/myFree not available.
DEFS += -DHEAP_IN_SDRAM=1

ifeq ($(ipconfigMULTI_INTERFACE),true)
	DEFS += -DipconfigMULTI_INTERFACE=1
	PLUS_TCP_PATH = \
		$(ROOT_PATH)/framework/FreeRTOS-Plus-TCP-multi.v2.3.1
else
	DEFS += -DipconfigMULTI_INTERFACE=0
	PLUS_TCP_PATH = \
		$(ROOT_PATH)/framework/FreeRTOS-Plus-TCP.v2.3.4
	ipconfigUSE_IPv6=false
endif

# Check for \Home\amazon-freertos\ipv6\FreeRTOS-Plus-TCP\source\FreeRTOS_IP_Timers.c

ifeq (,$(wildcard $(PLUS_TCP_PATH)/source/FreeRTOS_IP_Timers.c))
    # Old style with big source files
    TCP_SHORT_SOURCE_FILES=false
else
    # New style with smaller source files
    TCP_SHORT_SOURCE_FILES=true
	TCP_SOURCE=/source
endif

NETWORK_PATH = \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/NetworkInterface

TCP_UTILITIES=$(ROOT_PATH)/framework/FreeRTOS-Plus-TCP-multi.v2.3.1/tools/tcp_utilities

ifeq ($(ipconfigUSE_IPv6),true)
	DEFS += -DipconfigUSE_IPv6=1
else
	DEFS += -DipconfigUSE_IPv6=0
endif

DEFS += -D CPU_ZYNQ=1

INC_PATH = \
	$(DEMO_PATH)/ \
	$(FreeRTOS_PATH)/include/ \
	$(FreeRTOS_PATH)/portable/GCC/ARM_CA9/ \
	$(DEMO_PATH)/src/Full_Demo/ \
	$(DEMO_PATH)/src/Full_Demo/FreeRTOS-Plus-CLI/ \
	$(DEMO_PATH)/src/Full_Demo/Standard-Demo-Tasks/include/ \
	$(ROOT_PATH)/Framework/Utilities/include/ \
	$(ROOT_PATH)/Framework/Utilities/ \
	$(CORTEX_PATH)/ \
	$(DEMO_PATH)/src/ \
	$(INC_RTOS)/ \
	$(TCP_UTILITIES)/include/

# Now collect all C and S sources:

C_SRCS =
S_SRCS =

#	HW_FLAGS=-march=armv7-a -mfloat-abi=soft -mfpu=neon
#	HW_FLAGS=-march=armv7-a -mfpu=neon
#HW_FLAGS=-mfpu=neon -mfloat-abi=soft -mtune=cortex-a9 -mcpu=cortex-a9 -march=armv7-a
HW_FLAGS=-mtune=cortex-a9 -mcpu=cortex-a9 -march=armv7-a -mfpu=neon


PROCESSOR=ps7_cortexa9_0
CPU_PATH = $(PRJ_PATH)/RTOSDemo_bsp

ifeq ($(TCP_SHORT_SOURCE_FILES),true)
    # New style with smaller source files
	# /source/<sources>
	# /source/include
	# /source/portable
INC_PATH += \
	$(PLUS_TCP_PATH)/source/include/ \
	$(PLUS_TCP_PATH)/source/portable/Compiler/GCC/ \
	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/include/ \
	$(PLUS_TCP_PATH)/source/portable/NetworkInterface/Zynq/ \
	$(PLUS_TCP_PATH)/tools/tcp_utilities/include/
else
    # Old style with big source files
INC_PATH += \
	$(PLUS_TCP_PATH)/include/ \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/Compiler/GCC/ \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/NetworkInterface/include/ \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/NetworkInterface/STM32Fxx/ \
	$(PLUS_TCP_PATH)/tools/tcp_utilities/include/
endif

INC_PATH += \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/cpu_cortexa9_v2_1/src/ \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/emacps_v3_1/src/ \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/generic_v2_0/src/ \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/gpiops_v3_1/src/ \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/scugic_v3_1/src/ \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/scutimer_v2_1/src/ \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/scuwdt_v2_1/src/ \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/sdps_v2_6/src/ \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/standalone_v5_3/src/ \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/ttcps_v3_0/src/ \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/uartps_v3_1/src/ \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/usbps_v2_2/src/

C_SRCS += $(wildcard $(CPU_PATH)/$(PROCESSOR)/libsrc/cpu_cortexa9_v2_1/src/*.c)

ifeq ($(USE_FREERTOS_PLUS),true)
	C_SRCS += \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/emacps_v3_1/src/xemacps.c \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/emacps_v3_1/src/xemacps_control.c \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/emacps_v3_1/src/xemacps_g.c \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/emacps_v3_1/src/xemacps_intr.c
else
	C_SRCS += $(wildcard $(CPU_PATH)/$(PROCESSOR)/libsrc/emacps_v3_1/src/*.c)
endif

#	arm-xilinx-eabi-gcc: error: E:/Home/plus/xilinx/RTOSDemo_bsp/ps7_cortexa9_0/libsrc/standalone_v5_3/src/asm_vectors.o: No such file or directory
#	arm-xilinx-eabi-gcc: error: E:/Home/plus/xilinx/RTOSDemo_bsp/ps7_cortexa9_0/libsrc/standalone_v5_3/src/boot.o: No such file or directory
#	arm-xilinx-eabi-gcc: error: E:/Home/plus/xilinx/RTOSDemo_bsp/ps7_cortexa9_0/libsrc/standalone_v5_3/src/cpu_init.o: No such file or directory
#	arm-xilinx-eabi-gcc: error: E:/Home/plus/xilinx/RTOSDemo_bsp/ps7_cortexa9_0/libsrc/standalone_v5_3/src/translation_table.o: No such file or directory
#	arm-xilinx-eabi-gcc: error: E:/Home/plus/xilinx/RTOSDemo_bsp/ps7_cortexa9_0/libsrc/standalone_v5_3/src/xil-crt0.o: No such file or directory
#	arm-xilinx-eabi-gcc: error: E:/Home/plus/xilinx/RTOSDemo_bsp/ps7_cortexa9_0/libsrc/emacps_v3_1/src/xemacps.o: No such file or directory
#	arm-xilinx-eabi-gcc: error: E:/Home/plus/xilinx/RTOSDemo_bsp/ps7_cortexa9_0/libsrc/emacps_v3_1/src/xemacps_control.o: No such file or directory
#	arm-xilinx-eabi-gcc: error: E:/Home/plus/xilinx/RTOSDemo_bsp/ps7_cortexa9_0/libsrc/emacps_v3_1/src/xemacps_g.o: No such file or directory
#	arm-xilinx-eabi-gcc: error: E:/Home/plus/xilinx/RTOSDemo_bsp/ps7_cortexa9_0/libsrc/emacps_v3_1/src/xemacps_intr.o: No such file or directory

C_SRCS += $(wildcard $(CPU_PATH)/$(PROCESSOR)/libsrc/generic_v2_0/src/*.c)
C_SRCS += $(wildcard $(CPU_PATH)/$(PROCESSOR)/libsrc/gpiops_v3_1/src/*.c)
C_SRCS += $(wildcard $(CPU_PATH)/$(PROCESSOR)/libsrc/scugic_v3_1/src/*.c)
C_SRCS += $(wildcard $(CPU_PATH)/$(PROCESSOR)/libsrc/scutimer_v2_1/src/*.c)
C_SRCS += $(wildcard $(CPU_PATH)/$(PROCESSOR)/libsrc/scuwdt_v2_1/src/*.c)
C_SRCS += $(wildcard $(CPU_PATH)/$(PROCESSOR)/libsrc/standalone_v5_3/src/*.c)
C_SRCS += $(wildcard $(CPU_PATH)/$(PROCESSOR)/libsrc/ttcps_v3_0/src/*.c)
C_SRCS += $(wildcard $(CPU_PATH)/$(PROCESSOR)/libsrc/uartps_v3_1/src/*.c)
C_SRCS += $(wildcard $(CPU_PATH)/$(PROCESSOR)/libsrc/usbps_v2_2/src/*.c)

#C_SRCS += $(wildcard $(CPU_PATH)/$(PROCESSOR)/libsrc/sdps_v2_6/src/*.c)

S_SRCS += \
	$(DEMO_PATH)/src/FreeRTOS_asm_vectors.S \
	$(FreeRTOS_PATH)/portable/GCC/ARM_CA9/portASM.S 

S_SRCS += \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/standalone_v5_3/src/asm_vectors.S \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/standalone_v5_3/src/boot.S \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/standalone_v5_3/src/cpu_init.S \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/standalone_v5_3/src/translation_table.S \
	$(CPU_PATH)/$(PROCESSOR)/libsrc/standalone_v5_3/src/xil-crt0.S

#	$(CPU_PATH)/$(PROCESSOR)/libsrc/standalone_v5_3/src/profile/dummy.S \
#	$(CPU_PATH)/$(PROCESSOR)/libsrc/standalone_v5_3/src/profile/profile_mcount_arm.S

C_SRCS += \
	$(DEMO_PATH)/src/FreeRTOS_tick_config.c \
	$(DEMO_PATH)/src/ParTest.c \
	$(DEMO_PATH)/src/main.c \
	$(DEMO_PATH)/src/test_source.c \
	$(DEMO_PATH)/src/task_wdt.c \
	$(DEMO_PATH)/src/ff_stdio_tests_with_cwd.c \
	$(DEMO_PATH)/src/CreateAndVerifyExampleFiles.c \
	$(DEMO_PATH)/src/hr_gettime.c \
	$(DEMO_PATH)/src/platform.c \
	$(CommonUtilities_PATH)/UDPLoggingPrintf.c \
	$(CommonUtilities_PATH)/printf-stdarg.c \
	$(TCP_UTILITIES)/http_client_test.c \
	$(CommonUtilities_PATH)/memcpy.c

ifeq ($(ipconfigUSE_TCP_MEM_STATS),true)
	C_SRCS += $(TCP_UTILITIES)/tcp_mem_stats.c
	DEFS += -DipconfigUSE_TCP_MEM_STATS=1
else
	DEFS += -DipconfigUSE_TCP_MEM_STATS=0
endif

ifeq ($(ipconfigUSE_TCP_NET_STAT),true)
	C_SRCS += $(TCP_UTILITIES)/tcp_netstat.c
	INC_PATH += $(TCP_UTILITIES)/include
	DEFS += -DipconfigUSE_TCP_NET_STAT=1
else
	DEFS += -DipconfigUSE_TCP_NET_STAT=0
endif

ifeq ($(USE_NTOP_TEST),true)
	DEFS += -DUSE_NTOP_TEST=1
	C_SRCS += \
		$(PLUS_TCP_PATH)$(TCP_SOURCE)/test/inet_pton_ntop_tests.c
	INC_PATH += \
		$(PLUS_TCP_PATH)$(TCP_SOURCE)/test
endif

ifeq ($(USE_IPERF),true)
	DEFS += -D USE_IPERF=1
	C_SRCS += \
		$(CommonUtilities_PATH)/iperf_task_v3_0d.c
else
	DEFS += -D USE_IPERF=0
endif

C_SRCS += \
	$(DEMO_PATH)/src/SimpleTCPEServer.c

LD_SRCS += \
	$(DEMO_PATH)/src/lscript.ld 

C_SRCS += \
	$(FreeRTOS_PATH)/event_groups.c \
	$(FreeRTOS_PATH)/list.c \
	$(FreeRTOS_PATH)/queue.c \
	$(FreeRTOS_PATH)/tasks.c \
	$(FreeRTOS_PATH)/stream_buffer.c

C_SRCS += \
	$(FreeRTOS_PATH)/portable/GCC/ARM_CA9/port.c 

C_SRCS += \
	$(FreeRTOS_PATH)/portable/MemMang/heap_4.c 

INC_PATH += \
	$(Utilities_PATH)/include/ \
	$(MYSOURCE_PATH)/ \
	$(CommonUtilities_PATH)/ \
	$(CommonUtilities_PATH)/include/

ifeq ($(USE_PLUS_FAT),true)
	DEFS += -DUSE_PLUS_FAT=1
	DEFS += -DMX_FILE_SYS=3

	ipconfigUSE_HTTP=true

	INC_PATH += \
		$(PLUS_FAT_PATH)/portable/common/ \
		$(ZYNQ_PORTABLE_PATH)/ \
		$(PLUS_FAT_PATH)/include
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
		$(PLUS_FAT_PATH)/portable/common/ff_ramdisk.c
endif

ifeq ($(USE_FREERTOS_PLUS),true)
	DEFS += -DUSE_FREERTOS_PLUS=1
	DEFS += -DipconfigDNS_USE_CALLBACKS=1
	INC_PATH += \
		$(NETWORK_PATH)/ \
		$(NETWORK_PATH)/Zynq/
	C_SRCS += \
		$(NETWORK_PATH)/Zynq/NetworkInterface.c \
		$(NETWORK_PATH)/Zynq/x_emacpsif_dma.c \
		$(NETWORK_PATH)/Zynq/x_emacpsif_physpeed.c \
		$(NETWORK_PATH)/Zynq/x_emacpsif_hw.c \
		$(NETWORK_PATH)/Zynq/uncached_memory.c

	ifeq ($(ipconfigMULTI_INTERFACE),true)
		C_SRCS += \
			$(PLUS_TCP_PATH)$(TCP_SOURCE)/FreeRTOS_DHCPv6.c \
			$(PLUS_TCP_PATH)$(TCP_SOURCE)/FreeRTOS_BitConfig.c \
			$(PLUS_TCP_PATH)$(TCP_SOURCE)/FreeRTOS_Routing.c \
			$(PLUS_TCP_PATH)$(TCP_SOURCE)/FreeRTOS_RA.c \
			$(PLUS_TCP_PATH)$(TCP_SOURCE)/FreeRTOS_ND.c
	endif

	INC_PATH += \
		$(PLUS_TCP_PATH)$(TCP_SOURCE)/EchoClients/
endif


ifeq ($(INCLUDE_PS7_INIT),true)
	DEFS += -DINCLUDE_PS7_INIT=1
	INC_PATH += \
		$(PRJ_PATH)/MicroZed_hw_platform/
	C_SRCS += \
		$(PRJ_PATH)/MicroZed_hw_platform/ps7_init.c
endif


ifeq ($(USE_PLUS_FAT),true)
	C_SRCS += \
		$(ZYNQ_PORTABLE_PATH)/ff_sddisk.c \
		$(ZYNQ_PORTABLE_PATH)/xsdps_info.c \
		$(ZYNQ_PORTABLE_PATH)/xsdps.c \
		$(ZYNQ_PORTABLE_PATH)/xsdps_sinit.c \
		$(ZYNQ_PORTABLE_PATH)/xsdps_options.c \
		$(ZYNQ_PORTABLE_PATH)/xsdps_g.c \
		$(PLUS_FAT_PATH)/ff_stdio.c \
		$(PLUS_FAT_PATH)/ff_sys.c
endif

ifeq ($(ipconfigUSE_HTTP),true)
	PROTOCOLS_PATH=$(PLUS_TCP_PATH)/Protocols

	INC_PATH += \
		$(PROTOCOLS_PATH)/Include

	C_SRCS += \
		$(PROTOCOLS_PATH)/Common/FreeRTOS_TCP_server.c \
		$(PROTOCOLS_PATH)/HTTP/FreeRTOS_HTTP_server.c \
		$(PROTOCOLS_PATH)/HTTP/FreeRTOS_HTTP_commands.c \
		$(PROTOCOLS_PATH)/FTP/FreeRTOS_FTP_server.c \
		$(PROTOCOLS_PATH)/FTP/FreeRTOS_FTP_commands.c
endif

C_SRCS += \
	$(TCP_UTILITIES)/date_and_time.c \
	$(TCP_UTILITIES)/ddos_testing.c

ifeq ($(USE_TCP_DEMO_CLI),true)
	DEFS += -DUSE_TCP_DEMO_CLI=1
	C_SRCS += \
		$(TCP_UTILITIES)/plus_tcp_demo_cli.c
else
	DEFS += -DUSE_TCP_DEMO_CLI=0
endif

ifeq ($(ipconfigUSE_NTP_DEMO),true)
	C_SRCS += \
		$(PROTOCOLS_PATH)/NTP/NTPDemo.c
	DEFS += -DipconfigUSE_NTP_DEMO=1
endif


ifeq ($(USE_TCP_TESTER),true)
	DEFS += -DUSE_TCP_TESTER=1

	C_SRCS += \
		$(DEMO_PATH)/tcp_tester.c
endif

ifeq ($(USE_LOG_EVENT),true)
	DEFS += -D USE_LOG_EVENT=1
	DEFS += -D LOG_EVENT_COUNT=200
	DEFS += -D LOG_EVENT_NAME_LEN=20
	DEFS += -D STATIC_LOG_MEMORY=1
	DEFS += -D EVENT_MAY_WRAP=1
	C_SRCS += \
		$(Utilities_PATH)/eventLogging.c
endif

ifeq ($(USE_PCAP),true)
	DEFS += -D ipconfigUSE_PCAP=1
	C_SRCS += \
		$(TCP_UTILITIES)/win_pcap.c
else
	DEFS += -D ipconfigUSE_PCAP=0
endif

#	$(PRJ_PATH)/../plus/Framework/Utilities/mySprintf.c
ifeq ($(TCP_SHORT_SOURCE_FILES),true)
	C_SRCS += \
		$(PLUS_TCP_PATH)/source/FreeRTOS_ARP.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_DHCP.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_DNS.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_DNS_Cache.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_DNS_Callback.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_DNS_Networking.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_DNS_Parser.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_ICMP.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_IP.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_IP_Timers.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_IP_Utils.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_Sockets.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_Stream_Buffer.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_TCP_IP.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_TCP_Reception.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_TCP_State_Handling.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_TCP_Transmission.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_TCP_Utils.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_TCP_WIN.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_Tiny_TCP.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_UDP_IP.c \
		$(PLUS_TCP_PATH)/source/portable/BufferManagement/BufferAllocation_1.c

else
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
		$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/BufferManagement/BufferAllocation_1.c

ifeq (,$(wildcard $(PLUS_TCP_PATH)/FreeRTOS_IP_Timers.c))
    # old sources before split-up
else
	C_SRCS += \
		$(PLUS_TCP_PATH)/FreeRTOS_IP_Timers.c \
		$(PLUS_TCP_PATH)/FreeRTOS_IP_Utils.c \
		$(PLUS_TCP_PATH)/FreeRTOS_ICMP.c
endif 

ifeq (,$(wildcard $(PLUS_TCP_PATH)/DNS/DNS_Cache.c))
    # old sources before split-up
else
	C_SRCS += \
		$(PLUS_TCP_PATH)/DNS/DNS_Cache.c \
		$(PLUS_TCP_PATH)/DNS/DNS_Callback.c \
		$(PLUS_TCP_PATH)/DNS/DNS_Networking.c \
		$(PLUS_TCP_PATH)/DNS/DNS_Parser.c
endif

ifeq (,$(wildcard $(PLUS_TCP_PATH)/FreeRTOS_DNS_Cache.c))
    # old sources before split-up
else
	C_SRCS += \
		$(PLUS_TCP_PATH)/FreeRTOS_DNS_Cache.c \
		$(PLUS_TCP_PATH)/FreeRTOS_DNS_Callback.c \
		$(PLUS_TCP_PATH)/FreeRTOS_DNS_Networking.c \
		$(PLUS_TCP_PATH)/FreeRTOS_DNS_Parser.c
endif

ifeq (,$(wildcard $(PLUS_TCP_PATH)/FreeRTOS_TCP_Utils.c))
    # old sources before split-up
else
	C_SRCS += \
		$(PLUS_TCP_PATH)/FreeRTOS_TCP_Reception.c \
		$(PLUS_TCP_PATH)/FreeRTOS_TCP_State_Handling.c \
		$(PLUS_TCP_PATH)/FreeRTOS_TCP_Transmission.c \
		$(PLUS_TCP_PATH)/FreeRTOS_TCP_Utils.c
endif

ifeq (,$(wildcard $(PLUS_TCP_PATH)/FreeRTOS_Tiny_TCP.c))
    # old sources before split-up
else
	C_SRCS += \
		$(PLUS_TCP_PATH)/FreeRTOS_Tiny_TCP.c
endif

ifeq (,$(wildcard $(PLUS_TCP_PATH)/DNS_Cache.c))
    # old sources before split-up
else
	C_SRCS += \
		$(PLUS_TCP_PATH)/DNS_Cache.c \
		$(PLUS_TCP_PATH)/DNS_Callback.c \
		$(PLUS_TCP_PATH)/DNS_Networking.c \
		$(PLUS_TCP_PATH)/DNS_Parser.c
endif

endif

C_SRCS += \
	$(ROOT_PATH)/Common/Utilities/pcap_live_player.c

ifeq ($(USE_TELNET),true)
	DEFS += -DUSE_TELNET=1
	C_SRCS += \
		$(ROOT_PATH)/Common/Utilities/telnet.c
else
	DEFS += -D USE_TELNET=0
endif

ifeq ($(ipconfigUSE_WOLFSSL),true)
	DEFS += -DipconfigUSE_WOLFSSL=1
	LIBS += \
		m

	INC_PATH += \
		$(WOLFSSL_PATH)/ \
		$(WOLFSSL_PATH)/ide/mdk-arm/MDK-ARM/wolfSSL/

	C_SRCS += \
		$(DEMO_PATH)/src/echoserver.c

	C_SRCS += \
		$(WOLFSSL_PATH)/wolfcrypt/src/aes.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/arc4.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/asn.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/blake2b.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/camellia.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/coding.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/des3.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/dh.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/dsa.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/ecc.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/error.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/hash.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/hc128.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/hmac.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/integer.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/logging.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/md4.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/md5.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/memory.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/wc_port.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/pwdbased.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/rabbit.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/random.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/ripemd.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/rsa.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/sha.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/sha256.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/sha512.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/wc_encrypt.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/tfm.c \
		$(WOLFSSL_PATH)/wolfcrypt/src/wolfmath.c \
		$(WOLFSSL_PATH)/src/crl.c \
		$(WOLFSSL_PATH)/src/internal.c \
		$(WOLFSSL_PATH)/src/io.c \
		$(WOLFSSL_PATH)/src/keys.c \
		$(WOLFSSL_PATH)/src/ocsp.c \
		$(WOLFSSL_PATH)/src/ssl.c \
		$(WOLFSSL_PATH)/src/tls.c
else
	DEFS += -DipconfigUSE_WOLFSSL=0
endif

LWIP_DIR = $(ROOT_PATH)/lwIP/

CORE_SRCS = $(LWIP_DIR)/core/init.c \
	    $(LWIP_DIR)/core/mem.c \
	    $(LWIP_DIR)/core/memp.c \
	    $(LWIP_DIR)/core/netif.c \
	    $(LWIP_DIR)/core/pbuf.c \
	    $(LWIP_DIR)/core/raw.c \
	    $(LWIP_DIR)/core/stats.c \
	    $(LWIP_DIR)/core/def.c \
	    $(LWIP_DIR)/core/timers.c \
	    $(LWIP_DIR)/core/sys.c

CORE_IPV4_SRCS = $(LWIP_DIR)/core/ipv4/ip_addr.c \
		 $(LWIP_DIR)/core/ipv4/icmp.c \
		 $(LWIP_DIR)/core/ipv4/igmp.c \
		 $(LWIP_DIR)/core/ipv4/inet.c \
		 $(LWIP_DIR)/core/ipv4/inet_chksum.c \
		 $(LWIP_DIR)/core/ipv4/ip.c \
		 $(LWIP_DIR)/core/ipv4/ip_frag.c

CORE_IPV6_SRCS = $(LWIP_DIR)/core/ipv6/inet6.c  \
		 $(LWIP_DIR)/core/ipv6/ip6_addr.c \
		 $(LWIP_DIR)/core/ipv6/icmp6.c \
		 $(LWIP_DIR)/core/ipv6/ip6.c

CORE_TCP_SRCS  = $(LWIP_DIR)/core/tcp.c \
	         $(LWIP_DIR)/core/tcp_in.c \
	         $(LWIP_DIR)/core/tcp_out.c 

CORE_DHCP_SRCS = $(LWIP_DIR)/core/dhcp.c

CORE_UDP_SRCS  = $(LWIP_DIR)/core/udp.c

CORE_SNMP_SRCS = $(LWIP_DIR)/core/snmp/asn1_dec.c \
		 $(LWIP_DIR)/core/snmp/asn1_enc.c \
		 $(LWIP_DIR)/core/snmp/mib2.c \
		 $(LWIP_DIR)/core/snmp/mib_structs.c \
		 $(LWIP_DIR)/core/snmp/msg_in.c \
		 $(LWIP_DIR)/core/snmp/msg_out.c

CORE_ARP_SRCS  = $(LWIP_DIR)/netif/etharp.c

API_SOCK_SRCS = $(LWIP_DIR)/api/api_lib.c \
		 $(LWIP_DIR)/api/api_msg.c \
		 $(LWIP_DIR)/api/err.c \
		 $(LWIP_DIR)/api/netbuf.c \
		 $(LWIP_DIR)/api/sockets.c \
		 $(LWIP_DIR)/api/tcpip.c

# create LWIP_SRCS based on configured options

LWIP_SRCS = $(CORE_SRCS)

IF_SRC = \
	$(LWIP_DIR)/port/xilinx/netif/xadapter.c \
	$(LWIP_DIR)/port/xilinx/sys_arch.c \
	$(LWIP_DIR)/port/xilinx/sys_arch_raw.c

#	$(LWIP_DIR)/port/xilinx/netif/xemacpsif.c

# we always include ARP, IPv4, TCP and UDP sources
LWIP_SRCS += $(CORE_ARP_SRCS)
LWIP_SRCS += $(CORE_IPV4_SRCS)
LWIP_SRCS += $(CORE_TCP_SRCS)
LWIP_SRCS += $(CORE_UDP_SRCS)
LWIP_SRCS += $(CORE_DHCP_SRCS)
LWIP_SRCS += $(API_SOCK_SRCS)
LWIP_SRCS += $(IF_SRC)
LWIP_SRCS += $(DEMO_PATH)/lwip_echo_server.c
LWIP_SRCS += $(DEMO_PATH)/plus_echo_server.c

LWIP_OBJS = $(LWIP_SRCS:%.c=%.o)


ifeq ($(CONFIG_USE_LWIP),true)
	DEFS += -DCONFIG_USE_LWIP=1
C_SRCS += \
	$(LWIP_SRCS)
INC_PATH += \
	$(LWIP_DIR)/include \
	$(LWIP_DIR)/include/ipv4 \
	$(LWIP_DIR)/port/xilinx/include \
	$(LWIP_DIR)/port/xilinx/include/netif
else
	DEFS += -DCONFIG_USE_LWIP=0
endif

ifeq ($(SMALL_SIZE),true)
	DEFS += -D ipconfigHAS_PRINTF=0
	DEFS += -D ipconfigHAS_DEBUG_PRINTF=0
endif

# OPTIMIZATION = -O0 -fno-builtin-memcpy -fno-builtin-memset
OPTIMIZATION = -Os -fno-builtin-memcpy -fno-builtin-memset
# OPTIMIZATION = -O1 -fno-builtin-memcpy -fno-builtin-memset

AS_EXTRA_FLAGS=-mfpu=neon

TARGET = $(DEMO_PATH)/RTOSDemo_dbg.elf

WARNINGS = -Wall -Wextra -Warray-bounds -Werror=implicit-function-declaration -Wmissing-declarations
 #-Wstrict-prototypes 
 #-Wmissing-prototypes
 #-Wundef

DEBUG = -g3

