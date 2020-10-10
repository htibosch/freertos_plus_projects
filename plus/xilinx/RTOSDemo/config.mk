#
#	config.mk for 'RTOSDemo.elf' for Zynq / Xilinx
#

# Four choices for network and FAT driver:

USE_FREERTOS_PLUS=true

# ChaN's FS versus +FAT
USE_PLUS_FAT=true

# Some experiments called 'tamas'
USE_TAMAS=false
USE_TOM=true

# Fill in your Xilinx version

GCC_PREFIX=arm-none-eabi

# XILINX_PATH=E:/Xilinx/SDK/2019.1
# GCC_BIN=$(XILINX_PATH)/gnu/aarch32/nt/gcc-arm-none-eabi/bin

XILINX_PATH=$(HOME)/gcc-arm-none-eabi-7-2017-q4-major
GCC_BIN=$(XILINX_PATH)/bin


SMALL_SIZE=false

ipconfigUSE_WOLFSSL=false

ipconfigUSE_HTTP=false
ipconfigUSE_FTP=true

# Testing TCP Scaling Window for Steve Currie

ipconfigMULTI_INTERFACE=false
ipconfigUSE_IPv6=true


ipconfigOLD_MULTI=false
ipconfigUSE_TCP_MEM_STATS=true

ifeq ($(ipconfigMULTI_INTERFACE),false)
	ipconfigUSE_IPv6=false
	ipconfigOLD_MULTI=false
endif

USE_LOG_EVENT=true

USE_TELNET=true

USE_IPERF=true

ROOT_PATH = $(subst /fireworks,,$(abspath ../../fireworks))
PRJ_PATH = $(subst /fireworks,,$(abspath ../fireworks))
CUR_PATH = $(subst /fireworks,,$(abspath ./fireworks))

HOME_PATH = $(subst /fireworks,,$(abspath ../../../fireworks))
AMAZON_ROOT = $(HOME_PATH)/amazon-freertos/amazon-freertos

# FreeRTOS_PATH = $(ROOT_PATH)/Framework/FreeRTOSV8.2.1
# FreeRTOS_PATH = $(ROOT_PATH)/Framework/FreeRTOS_V8.2.2
# FreeRTOS_PATH = $(ROOT_PATH)/Framework/FreeRTOSV8.2.3
# FreeRTOS_PATH = $(ROOT_PATH)/Framework/FreeRTOS_v9.0.0

# /home/hein/work/plus/Framework/FreeRTOS_V10.2.1/include/
FreeRTOS_PATH = $(ROOT_PATH)/Framework/FreeRTOS_V10.2.1
#FreeRTOS_PATH = $(ROOT_PATH)/Framework/FreeRTOS_V10.2.0

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

Utilities_PATH = $(ROOT_PATH)/Framework/Utilities
CommonUtilities_PATH = $(ROOT_PATH)/Common/Utilities
# The path where this Makefile is located:
DEMO_PATH = $(PRJ_PATH)/RTOSDemo
#WOLFSSL_PATH = $(ROOT_PATH)/Framework/wolfssl
#WOLFSSL_PATH = $(ROOT_PATH)/Framework/wolfssl-3.10.4
WOLFSSL_PATH = $(ROOT_PATH)/Framework/wolfssl.master

#LDLIBS=-Wl,--start-group,-lxil,-lgcc,-lc,--end-group
LDLIBS=-Wl,--start-group,-lgcc,-lc,--end-group

CONFIG_USE_LWIP=false
USE_TCP_TESTER=true

DEFS =

LD_EXTRA_FLAGS =-nostartfiles \
	-Xlinker -Map=RTOSDemo.map \
	-Xlinker --gc-sections \
	-Wl,--build-id=none


INC_RTOS = \
	$(FreeRTOS_PATH)/include/ \
	$(FreeRTOS_PATH)/include/private/ \
	$(FreeRTOS_PATH)/portable/GCC/ARM_CA9/

CORTEX_PATH = \
	$(PRJ_PATH)/RTOSDemo_bsp/ps7_cortexa9_0/include

LIBS =

LIB_PATH = \
	$(PRJ_PATH)/RTOSDemo_bsp/ps7_cortexa9_0/lib

PLUS_FAT_PATH = \
	$(ROOT_PATH)/Framework/FreeRTOS-Plus-FAT

ZYNQ_POR_TPATH= \
	$(PLUS_FAT_PATH)/portable/Zynq.2019.3/

#	$(PLUS_FAT_PATH)/portable/zynq/

LINKER_SCRIPT=$(DEMO_PATH)/src/lscript.ld

INCLUDE_PS7_INIT=false

ifeq ($(ipconfigMULTI_INTERFACE),true)
	DEFS += -DipconfigMULTI_INTERFACE=1
	ifeq ($(ipconfigOLD_MULTI),true)
		PLUS_TCP_PATH = \
			$(ROOT_PATH)/Framework/FreeRTOS-Plus-TCP_multi_30_aug_2018
		DEFS += -DipconfigOLD_MULTI=1
	else
		PLUS_TCP_PATH = \
			$(ROOT_PATH)/Framework/FreeRTOS-Plus-TCP-multi
	endif
else
	DEFS += -DipconfigMULTI_INTERFACE=0
	PLUS_TCP_PATH = \
		$(ROOT_PATH)/Framework/FreeRTOS-Plus-TCP
endif

ifeq ($(ipconfigUSE_IPv6),true)
	DEFS += -DipconfigUSE_IPv6=1
else
	DEFS += -DipconfigUSE_IPv6=0
endif

	DEFS += -DipconfigULTRASCALE=0

ifeq ($(ipconfigUSE_TCP_MEM_STATS),true)
	DEFS += -DipconfigUSE_TCP_MEM_STATS=1
else
	DEFS += -DipconfigUSE_TCP_MEM_STATS=0
endif

DEFS += -D CPU_ZYNQ=1

INC_PATH = \
	$(DEMO_PATH)/ \
	$(DEMO_PATH)/src/Full_Demo/ \
	$(DEMO_PATH)/src/Full_Demo/FreeRTOS-Plus-CLI/ \
	$(DEMO_PATH)/src/Full_Demo/Standard-Demo-Tasks/include/ \
	$(ROOT_PATH)/Framework/Utilities/include/ \
	$(ROOT_PATH)/Framework/Utilities/ \
	$(CORTEX_PATH)/ \
	$(DEMO_PATH)/src/ \
	$(INC_RTOS)

# Now collect all C and S sources:

C_SRCS =
S_SRCS =

#	HW_FLAGS=-march=armv7-a -mfloat-abi=soft -mfpu=neon
#	HW_FLAGS=-march=armv7-a -mfpu=neon
#HW_FLAGS=-mfpu=neon -mfloat-abi=soft -mtune=cortex-a9 -mcpu=cortex-a9 -march=armv7-a
HW_FLAGS=-mtune=cortex-a9 -mcpu=cortex-a9 -march=armv7-a -mfpu=neon


PROCESSOR=ps7_cortexa9_0
CPU_PATH = $(PRJ_PATH)/RTOSDemo_bsp

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
	$(DEMO_PATH)/src/task_wdt.c \
	$(DEMO_PATH)/src/ff_stdio_tests_with_cwd.c \
	$(DEMO_PATH)/src/CreateAndVerifyExampleFiles.c \
	$(DEMO_PATH)/src/hr_gettime.c \
	$(DEMO_PATH)/src/platform.c \
	$(CommonUtilities_PATH)/UDPLoggingPrintf.c \
	$(CommonUtilities_PATH)/printf-stdarg.c
	
ifeq ($(ipconfigUSE_TCP_MEM_STATS),true)
	C_SRCS += $(PLUS_TCP_PATH)/tools/tcp_mem_stats.c
endif

ifeq ($(USE_IPERF),true)
	DEFS += -D USE_IPERF=1
	C_SRCS += \
		$(CommonUtilities_PATH)/iperf_task_v3_0d.c
else
	DEFS += -D USE_IPERF=0
endif

ifeq ($(USE_TOM),true)
	DEFS += -DUSE_TOM=1
#	C_SRCS += \
#		$(DEMO_PATH)/src/sendping.c
endif

#	$(DEMO_PATH)/src/uncached_memory.c

#	$(DEMO_PATH)/plus_echo_client.c

#C_SRCS += \
#	$(CommonUtilities_PATH)/memcpy.c

C_SRCS += \
	$(DEMO_PATH)/src/SimpleTCPEServer.c

LD_SRCS += \
	$(DEMO_PATH)/src/lscript.ld 

#	$(FreeRTOS_PATH)/croutine.c

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
	$(PLUS_TCP_PATH)/include/ \
	$(PLUS_TCP_PATH)/source/portable/Compiler/GCC/ \
	$(Utilities_PATH)/include/ \
	$(CommonUtilities_PATH)/include/

C_SRCS += \
	$(PLUS_TCP_PATH)/source/FreeRTOS_Stream_Buffer.c

ifeq ($(USE_FREERTOS_PLUS),true)
	DEFS += -DUSE_FREERTOS_PLUS=1
	DEFS += -DipconfigDNS_USE_CALLBACKS=1
	INC_PATH += \
		$(PLUS_TCP_PATH)/source/portable/NetworkInterface/ \
		$(PLUS_TCP_PATH)/source/portable/NetworkInterface/Zynq/
	C_SRCS += \
		$(PLUS_TCP_PATH)/source/portable/NetworkInterface/Zynq/NetworkInterface.c \
		$(PLUS_TCP_PATH)/source/portable/NetworkInterface/Zynq/x_emacpsif_dma.c \
		$(PLUS_TCP_PATH)/source/portable/NetworkInterface/Zynq/x_emacpsif_physpeed.c \
		$(PLUS_TCP_PATH)/source/portable/NetworkInterface/Zynq/x_emacpsif_hw.c \
		$(PLUS_TCP_PATH)/source/portable/NetworkInterface/Zynq/uncached_memory.c
	C_SRCS += \
		$(PLUS_TCP_PATH)/source/FreeRTOS_ARP.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_DHCP.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_DNS.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_IP.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_Sockets.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_TCP_IP.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_TCP_WIN.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_UDP_IP.c
ifeq ($(ipconfigMULTI_INTERFACE),true)
	C_SRCS += \
		$(PLUS_TCP_PATH)/source/FreeRTOS_Routing.c \
		$(PLUS_TCP_PATH)/source/FreeRTOS_ND.c
endif

		# Using fixed buffers
		C_SRCS += $(PLUS_TCP_PATH)/source/portable/BufferManagement/BufferAllocation_1.c

	INC_PATH += \
		$(PLUS_TCP_PATH)/source/EchoClients/
endif


ifeq ($(INCLUDE_PS7_INIT),true)
	DEFS += -DINCLUDE_PS7_INIT=1
	INC_PATH += \
		$(PRJ_PATH)/MicroZed_hw_platform/
	C_SRCS += \
		$(PRJ_PATH)/MicroZed_hw_platform/ps7_init.c
endif


ifeq ($(USE_PLUS_FAT),true)
	DEFS += -DUSE_PLUS_FAT=1
	DEFS += -DMX_FILE_SYS=3

	ipconfigUSE_HTTP=true

	INC_PATH += \
		$(PLUS_FAT_PATH)/portable/common/ \
		$(ZYNQ_POR_TPATH)/ \
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
		$(PLUS_FAT_PATH)/portable/common/ff_ramdisk.c \
		$(ZYNQ_POR_TPATH)/ff_sddisk.c \
		$(ZYNQ_POR_TPATH)/xsdps_info.c \
		$(ZYNQ_POR_TPATH)/xsdps.c \
		$(ZYNQ_POR_TPATH)/xsdps_sinit.c \
		$(ZYNQ_POR_TPATH)/xsdps_options.c \
		$(ZYNQ_POR_TPATH)/xsdps_g.c \
		$(PLUS_FAT_PATH)/ff_stdio.c \
		$(PLUS_FAT_PATH)/ff_sys.c
endif


ifeq ($(ipconfigUSE_HTTP),true)
	PROTOCOLS_PATH=$(PLUS_TCP_PATH)/source/protocols
	INC_PATH += \
		$(PROTOCOLS_PATH)/include

	C_SRCS += \
		$(PROTOCOLS_PATH)/Common/FreeRTOS_TCP_server.c \
		$(PROTOCOLS_PATH)/HTTP/FreeRTOS_HTTP_server.c \
		$(PROTOCOLS_PATH)/HTTP/FreeRTOS_HTTP_commands.c \
		$(PROTOCOLS_PATH)/FTP/FreeRTOS_FTP_server.c \
		$(PROTOCOLS_PATH)/FTP/FreeRTOS_FTP_commands.c \
		$(PROTOCOLS_PATH)/NTP/NTPDemo.c
endif

C_SRCS += \
	$(CommonUtilities_PATH)/date_and_time.c

ifeq ($(USE_TCP_TESTER),true)
	DEFS += -DUSE_TCP_TESTER=1

	C_SRCS += \
		$(DEMO_PATH)/tcp_tester.c
endif

ifeq ($(USE_LOG_EVENT),true)
	DEFS += -D USE_LOG_EVENT=1
	DEFS += -D LOG_EVENT_COUNT=1024
	DEFS += -D LOG_EVENT_NAME_LEN=40
	DEFS += -D STATIC_LOG_MEMORY=1
	DEFS += -D EVENT_MAY_WRAP=0
	C_SRCS += \
		$(Utilities_PATH)/eventLogging.c
endif

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

ifeq ($(USE_TAMAS),true)
	DEFS += -DUSE_TAMAS=1
INC_PATH += \
	$(PRJ_PATH)/tamas/embedded/
C_SRCS += \
	$(PRJ_PATH)/tamas/embedded/tamas_main.c
endif

ifeq ($(SMALL_SIZE),true)
	DEFS += -D ipconfigHAS_PRINTF=0
	DEFS += -D ipconfigHAS_DEBUG_PRINTF=0
endif

# OPTIMIZATION = -Os -fno-builtin-memcpy -fno-builtin-memset
OPTIMIZATION = -O0 -fno-builtin-memcpy -fno-builtin-memset

AS_EXTRA_FLAGS=-mfpu=neon

TARGET = $(DEMO_PATH)/RTOSDemo_dbg.elf

WARNINGS = -Wall -Warray-bounds=2

DEBUG = -g3

