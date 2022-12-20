#
#	config.mk for 'stm32F7xx.elf' for STM32F7xx
#

ipconfigMULTI_INTERFACE=true
ipconfigUSE_IPv6=true

GCC_BIN=C:/Ac6_v2.9/SystemWorkbench/plugins/fr.ac6.mcu.externaltools.arm-none.win32_1.17.0.201812190825/tools/compiler/bin
GCC_PREFIX=arm-none-eabi

C_SRCS =
S_SRCS =

# Four choices for network and FAT driver:

TCP_SOURCE=
#TCP_SOURCE=/Source

#ST_Library = Abe_Library
ST_Library = ST_Library

# THis is the module with a CLI to do all sorts of tests like: "dnsq", "ping", etc
USE_TCP_DEMO_CLI=true

# Some module might want to know which IP-stack is being used.
USE_FREERTOS_PLUS=true

# ChaN's FS versus +FAT
USE_PLUS_FAT=true

# both a client and a server for simple TCP echo.
HAS_ECHO_TEST=true

# A http client for testing.
USE_ECHO_TASK=true

# Use NTP code
USE_NTP_DEMO=true

# Include the IPERF3 server for speed and other network tests
USE_IPERF=true

# Include a test with digital audio / SAI
HAS_AUDIO=false

# Disable all debugging and let the compiler optimise for size
SMALL_SIZE=false

# Use a logging system for debugging that is ISR-proof
USE_LOG_EVENT=true

# Use the HHTP server from the protocols directory
# This will need FreeRTOS+FAT
ipconfigUSE_HTTP=false

# Use the FTP server from the protocols directory
# This will need FreeRTOS+FAT
ipconfigUSE_FTP=true

# Enable writing of PCAP files. 
# This will need FreeRTOS+FAT
USE_PCAP=false

# Collect information about the usage of memory by FreeRTOS+TCP
USE_TCP_MEM_STATS=false

USE_MBEDTLS=false

# The word 'fireworks' here can be replaced by any other string
# Others would write 'foo'
ROOT_PATH = $(subst /fireworks,,$(abspath ../fireworks))
PRJ_PATH = $(subst /fireworks,,$(abspath ./fireworks))

HOME_PATH = $(subst /fireworks,,$(abspath ../../fireworks))

LDLIBS=

DEFS =

DEFS += -DCR_INTEGER_PRINTF
DEFS += -DUSE_FREERTOS_PLUS=1
DEFS += -DGNUC_NEED_SBRK=1

DEFS += -DipconfigNETWORK_BUFFER_DEBUGGING=0

# The standard heap for pvPortMalloc()/vPortFree() is
# located in the internal SRAM, not the external SDRAM.
DEFS += -DHEAP_IN_SDRAM=0

LD_EXTRA_FLAGS =

FREERTOS_ROOT = \
	$(ROOT_PATH)/framework/FreeRTOS_v9.0.0
#	$(ROOT_PATH)/framework/FreeRTOS_V10.5.1

FREERTOS_PORT_PATH = \
	$(FREERTOS_ROOT)/portable/GCC/ARM_CM7/r0p1

PLUS_FAT_PATH = \
	$(ROOT_PATH)/framework/FreeRTOS-Plus-FAT

MBEDTLS_PATH = \
	$(ROOT_PATH)/framework/secure/mbedtls

ifeq ($(ipconfigMULTI_INTERFACE),true)
	DEFS += -D ipconfigMULTI_INTERFACE=1
	DEFS += -D ipconfigUSE_LOOPBACK=1
	PLUS_TCP_PATH = \
		$(ROOT_PATH)/framework/FreeRTOS-Plus-TCP-multi.v2.3.1
else
	DEFS += -D ipconfigMULTI_INTERFACE=0
	ipconfigUSE_IPv6=false
	PLUS_TCP_PATH = \
		$(ROOT_PATH)/framework/FreeRTOS-Plus-TCP.v2.3.4
endif

ifeq (,$(wildcard $(PLUS_TCP_PATH)/source/FreeRTOS_IP.c))
	# /source/FreeRTOS_IP.c does not exists
else
	TCP_SOURCE=/source
endif

TCP_UTILITIES=$(PLUS_TCP_PATH)/tools/tcp_utilities

PROTOCOLS_PATH = $(PLUS_TCP_PATH)/protocols

# Check for \Home\amazon-freertos\ipv6\FreeRTOS-Plus-TCP\source\FreeRTOS_IP_Timers.c

ifeq (,$(wildcard $(PLUS_TCP_PATH)/source/FreeRTOS_TCP_Reception.c))
    # Old style with big source files
    TCP_SHORT_SOURCE_FILES=false
else
    # New style with smaller source files
    TCP_SHORT_SOURCE_FILES=true
	TCP_SOURCE=/source
endif

ifeq ($(ipconfigUSE_IPv6),true)
	DEFS += -D ipconfigUSE_IPv6=1
else
	DEFS += -D ipconfigUSE_IPv6=0
endif

INC_PATH = \
	$(PRJ_PATH)/ \
	$(PRJ_PATH)/Inc/ \
	$(PRJ_PATH)/$(ST_Library)/include/ \
	$(PRJ_PATH)/$(ST_Library)/include/Legacy/ \
	$(PRJ_PATH)/$(ST_Library)/CMSIS/Include/ \
	$(PRJ_PATH)/$(ST_Library)/CMSIS/Device/ST/STM32F7xx/Include/ \
	$(PRJ_PATH)/$(ST_Library)/include/ \
	$(FREERTOS_PORT_PATH)/ \
	$(FREERTOS_ROOT)/include/ \
	$(FREERTOS_ROOT)/CMSIS_RTOS/ \
	$(TCP_UTILITIES)/include/ \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/include/ \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/Compiler/GCC/ \
	$(PROTOCOLS_PATH)/include/ \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/NetworkInterface/include/ \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/NetworkInterface/STM32Fxx/ \
	$(PLUS_TCP_PATH)/tools/tcp_utilities/include/ \
	$(PRJ_PATH)/Third_Party/USB/ \
	$(ROOT_PATH)/framework/utilities/ \
	$(ROOT_PATH)/framework/utilities/include/ \
	$(ROOT_PATH)/framework/mysource/ \
	$(ROOT_PATH)/framework/myaudio/ \
	$(ROOT_PATH)/Common/utilities/ \
	$(ROOT_PATH)/Common/utilities/include/

S_SRCS += \
	$(PRJ_PATH)/Src/startup_stm32f746xx.S

ifeq ($(USE_MBEDTLS),true)
	INC_PATH += \
		$(MBEDTLS_PATH)/include \
		$(MBEDTLS_PATH)/configs \
		$(MBEDTLS_PATH)

	DEFS += -DMBEDTLS_CONFIG_FILE="<configs/mbedtls_config.h>"
	DEFS += -DUSE_MBEDTLS=1
#	DEFS += -DMBEDTLS_CONFIG_FILE="<configs/aws_mbedtls_config.h>"
	C_SRCS += \
		$(MBEDTLS_PATH)/programs/ssl/ssl_client2.c

	C_SRCS += \
		$(MBEDTLS_PATH)/mbedtls_freertos_port.c \
    	$(MBEDTLS_PATH)/library/aes.c \
    	$(MBEDTLS_PATH)/library/aesni.c \
    	$(MBEDTLS_PATH)/library/arc4.c \
    	$(MBEDTLS_PATH)/library/aria.c \
    	$(MBEDTLS_PATH)/library/asn1parse.c \
    	$(MBEDTLS_PATH)/library/asn1write.c \
    	$(MBEDTLS_PATH)/library/base64.c \
    	$(MBEDTLS_PATH)/library/bignum.c \
    	$(MBEDTLS_PATH)/library/blowfish.c \
    	$(MBEDTLS_PATH)/library/camellia.c \
    	$(MBEDTLS_PATH)/library/ccm.c \
    	$(MBEDTLS_PATH)/library/certs.c \
    	$(MBEDTLS_PATH)/library/chacha20.c \
    	$(MBEDTLS_PATH)/library/chachapoly.c \
    	$(MBEDTLS_PATH)/library/cipher.c \
    	$(MBEDTLS_PATH)/library/cipher_wrap.c \
    	$(MBEDTLS_PATH)/library/cmac.c \
    	$(MBEDTLS_PATH)/library/ctr_drbg.c \
    	$(MBEDTLS_PATH)/library/debug.c \
    	$(MBEDTLS_PATH)/library/des.c \
    	$(MBEDTLS_PATH)/library/dhm.c \
    	$(MBEDTLS_PATH)/library/ecdh.c \
    	$(MBEDTLS_PATH)/library/ecdsa.c \
    	$(MBEDTLS_PATH)/library/ecjpake.c \
    	$(MBEDTLS_PATH)/library/ecp.c \
    	$(MBEDTLS_PATH)/library/ecp_curves.c \
    	$(MBEDTLS_PATH)/library/entropy.c \
    	$(MBEDTLS_PATH)/library/entropy_poll.c \
    	$(MBEDTLS_PATH)/library/error.c \
    	$(MBEDTLS_PATH)/library/gcm.c \
    	$(MBEDTLS_PATH)/library/havege.c \
    	$(MBEDTLS_PATH)/library/hkdf.c \
    	$(MBEDTLS_PATH)/library/hmac_drbg.c \
    	$(MBEDTLS_PATH)/library/md.c \
    	$(MBEDTLS_PATH)/library/md2.c \
    	$(MBEDTLS_PATH)/library/md4.c \
    	$(MBEDTLS_PATH)/library/md5.c \
    	$(MBEDTLS_PATH)/library/md_wrap.c \
    	$(MBEDTLS_PATH)/library/memory_buffer_alloc.c \
    	$(MBEDTLS_PATH)/library/net_sockets.c \
    	$(MBEDTLS_PATH)/library/nist_kw.c \
    	$(MBEDTLS_PATH)/library/oid.c \
    	$(MBEDTLS_PATH)/library/padlock.c \
    	$(MBEDTLS_PATH)/library/pem.c \
    	$(MBEDTLS_PATH)/library/pk.c \
    	$(MBEDTLS_PATH)/library/pk_wrap.c \
    	$(MBEDTLS_PATH)/library/pkcs11.c \
    	$(MBEDTLS_PATH)/library/pkcs12.c \
    	$(MBEDTLS_PATH)/library/pkcs5.c \
    	$(MBEDTLS_PATH)/library/pkparse.c \
    	$(MBEDTLS_PATH)/library/pkwrite.c \
    	$(MBEDTLS_PATH)/library/platform.c \
    	$(MBEDTLS_PATH)/library/platform_util.c \
    	$(MBEDTLS_PATH)/library/poly1305.c \
    	$(MBEDTLS_PATH)/library/ripemd160.c \
    	$(MBEDTLS_PATH)/library/rsa.c \
    	$(MBEDTLS_PATH)/library/rsa_internal.c \
    	$(MBEDTLS_PATH)/library/sha1.c \
    	$(MBEDTLS_PATH)/library/sha256.c \
    	$(MBEDTLS_PATH)/library/sha512.c \
    	$(MBEDTLS_PATH)/library/ssl_cache.c \
    	$(MBEDTLS_PATH)/library/ssl_ciphersuites.c \
    	$(MBEDTLS_PATH)/library/ssl_cli.c \
    	$(MBEDTLS_PATH)/library/ssl_cookie.c \
    	$(MBEDTLS_PATH)/library/ssl_srv.c \
    	$(MBEDTLS_PATH)/library/ssl_ticket.c \
    	$(MBEDTLS_PATH)/library/ssl_tls.c \
    	$(MBEDTLS_PATH)/library/threading.c \
    	$(MBEDTLS_PATH)/library/timing.c \
    	$(MBEDTLS_PATH)/library/version.c \
    	$(MBEDTLS_PATH)/library/version_features.c \
    	$(MBEDTLS_PATH)/library/x509.c \
    	$(MBEDTLS_PATH)/library/x509_create.c \
    	$(MBEDTLS_PATH)/library/x509_crl.c \
    	$(MBEDTLS_PATH)/library/x509_crt.c \
    	$(MBEDTLS_PATH)/library/x509_csr.c \
    	$(MBEDTLS_PATH)/library/x509write_crt.c \
    	$(MBEDTLS_PATH)/library/x509write_csr.c \
    	$(MBEDTLS_PATH)/library/xtea.c
endif # USE_MBEDTLS

C_SRCS += \
	$(PRJ_PATH)/Src/main.c \
	$(PRJ_PATH)/Src/memcpy.c \
	$(PRJ_PATH)/Src/stm32f7xx_it.c \
	$(ROOT_PATH)/Common/utilities/printf-stdarg.c \
	$(PRJ_PATH)/Src/hr_gettime.c \
	$(ROOT_PATH)/Common/FreeRTOS_Plus_FAT_Demos/CreateAndVerifyExampleFiles.c \
	$(ROOT_PATH)/Common/FreeRTOS_Plus_FAT_Demos/test/ff_stdio_tests_with_cwd.c \
	$(ROOT_PATH)/Common/utilities/UDPLoggingPrintf.c \
	$(PRJ_PATH)/Src/system_stm32f7xx.c \
	$(PRJ_PATH)/Src/udp_packets.c

C_SRCS += \
	$(FREERTOS_ROOT)/event_groups.c \
	$(FREERTOS_ROOT)/list.c \
	$(FREERTOS_ROOT)/queue.c \
	$(FREERTOS_ROOT)/tasks.c \
	$(FREERTOS_ROOT)/timers.c \
	$(FREERTOS_ROOT)/portable/MemMang/heap_5.c \
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
		$(PLUS_FAT_PATH)/ff_stdio.c \
		$(PLUS_FAT_PATH)/ff_sys.c \
		$(PLUS_FAT_PATH)/portable/common/ff_ramdisk.c \
 		$(PLUS_FAT_PATH)/portable/STM32F7xx/ff_sddisk.c \
 		$(PLUS_FAT_PATH)/portable/STM32F7xx/stm32f7xx_hal_sd.c

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
		$(PROTOCOLS_PATH)/FTP/FreeRTOS_FTP_commands.c
endif

ifeq ($(USE_NTP_DEMO),true)
	C_SRCS += \
		$(PROTOCOLS_PATH)/NTP/NTPDemo.c
	DEFS += -DUSE_NTP_DEMO=1
	DEFS += -DipconfigUSE_NTP_DEMO

endif

ifeq ($(USE_IPERF),true)
	DEFS += -DUSE_IPERF=1
	C_SRCS += \
		$(ROOT_PATH)/Common/Utilities/iperf_task_v3_0f.c
else
	DEFS += -DUSE_IPERF=0
endif

ifeq ($(USE_LOG_EVENT),true)
	DEFS += -D USE_LOG_EVENT=1
	DEFS += -D LOG_EVENT_COUNT=100
	DEFS += -D LOG_EVENT_NAME_LEN=40
	DEFS += -D STATIC_LOG_MEMORY=1
	DEFS += -D EVENT_MAY_WRAP=0
	C_SRCS += \
		$(ROOT_PATH)/framework/Utilities/eventLogging.c
endif

ifeq ($(TCP_SHORT_SOURCE_FILES),true)
	C_SRCS += \
		$(wildcard $(PLUS_TCP_PATH)/source/*.c)

	C_SRCS += \
		$(PLUS_TCP_PATH)/source/portable/BufferManagement/BufferAllocation_1.c \
		$(PLUS_TCP_PATH)/source/portable/NetworkInterface/STM32Fxx/NetworkInterface.c \
		$(PLUS_TCP_PATH)/source/portable/NetworkInterface/STM32Fxx/stm32fxx_hal_eth.c

else
	C_SRCS += \
		$(wildcard $(PLUS_TCP_PATH)/*.c)
	C_SRCS += \
		$(wildcard $(PLUS_TCP_PATH)/source/*.c)

	C_SRCS += \
		$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/BufferManagement/BufferAllocation_1.c \
		$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/NetworkInterface/STM32Fxx/NetworkInterface.c \
		$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/NetworkInterface/STM32Fxx/stm32fxx_hal_eth.c
endif

# Use SDRAM and let it be managed by myMalloc()
# which is based on heap_4.c from the kernel.
#C_SRCS += \
#	$(ROOT_PATH)/framework/mysource/mymalloc.c \
#	$(PRJ_PATH)/Src/stm32746g_discovery_sdram.c

ifeq ($(USE_PCAP),true)
	DEFS += -D ipconfigUSE_PCAP=1
	C_SRCS += \
		$(TCP_UTILITIES)/win_pcap.c
else
	DEFS += -D ipconfigUSE_PCAP=0
endif

ifeq ($(HAS_AUDIO),true)
	DEFS += -DHAS_AUDIO=1
	C_SRCS += \
		$(ROOT_PATH)/framework/myaudio/stm32746g_discovery.c \
		$(ROOT_PATH)/framework/myaudio/stm32746g_discovery_audio.c \
		$(ROOT_PATH)/framework/myaudio/wm8994.c
	CPP_SRCS += \
		$(ROOT_PATH)/framework/myaudio/playerRecorder.cpp \
		$(ROOT_PATH)/framework/myaudio/fileWav.cpp
else
	DEFS += -DHAS_AUDIO=0
endif


DEFS += -DLOGBUF_MAX_UNITS=64
DEFS += -DLOGBUF_AVG_LEN=72

ifeq ($(HAS_ECHO_TEST),true)
	DEFS += -DHAS_ECHO_TEST=1
	C_SRCS += \
		$(ROOT_PATH)/Common/Utilities/plus_echo_client.c \
		$(ROOT_PATH)/Common/Utilities/plus_echo_server.c
endif

ifeq ($(USE_ECHO_TASK),true)
	C_SRCS += \
		$(TCP_UTILITIES)/http_client_test.c
	DEFS += -DUSE_ECHO_TASK=1
else
	DEFS += -DUSE_ECHO_TASK=0
endif


ifeq ($(USE_TCP_MEM_STATS),true)
	C_SRCS += \
		$(TCP_UTILITIES)/tcp_mem_stats.c
	DEFS += -DipconfigUSE_TCP_MEM_STATS
endif

C_SRCS += \
	$(PLUS_TCP_PATH)$(TCP_SOURCE)/portable/NetworkInterface/Common/phyHandling.c

ifeq ($(USE_TCP_DEMO_CLI),true)
	DEFS += -DUSE_TCP_DEMO_CLI=1
	C_SRCS += \
		$(TCP_UTILITIES)/plus_tcp_demo_cli.c \
		$(TCP_UTILITIES)/date_and_time.c
else
	DEFS += -DUSE_TCP_DEMO_CLI=0
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
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_i2c.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_ll_sdmmc.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_rcc.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_ll_fmc.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_rcc_ex.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_tim.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_rng.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_gpio.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_sai.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_sai_ex.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_sdram.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_pwr.c \
	$(PRJ_PATH)/$(ST_Library)/source/stm32f7xx_hal_rtc.c

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
#	OPTIMIZATION = -Os -fno-builtin-memcpy -fno-builtin-memset
	OPTIMIZATION = -O0 -fno-builtin-memcpy -fno-builtin-memset
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
