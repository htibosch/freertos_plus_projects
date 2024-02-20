/*
 * Some constants, hardware definitions and comments taken from ST's HAL driver
 * library, COPYRIGHT(c) 2015 STMicroelectronics.
 */

/*
 * FreeRTOS+TCP V2.3.4
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

/* Standard includes. */
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>

/* FreeRTOS includes. */
#include <FreeRTOS.h>
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"
#include "message_buffer.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_DHCP.h"
#include "FreeRTOS_DNS.h"
#include "NetworkInterface.h"
#include "NetworkBufferManagement.h"
#include "FreeRTOS_ARP.h"
#include "FreeRTOS_IP_Private.h"

#include "UDPLoggingPrintf.h"

#if ( ipconfigMULTI_INTERFACE != 0 )
	#include "FreeRTOS_Routing.h"
	#if ( ipconfigUSE_IPv6 != 0 )
		#include "FreeRTOS_ND.h"
	#endif
	#warning ipconfigMULTI_INTERFACE is defined
#endif

#if ( ipconfigUSE_HTTP != 0 ) || ( ipconfigUSE_FTP != 0 )
	#include "FreeRTOS_TCP_server.h"
#endif

#if ( USE_PLUS_FAT != 0 )
	/* FreeRTOS+FAT includes. */
	#include "ff_headers.h"
	#include "ff_stdio.h"
	#include "ff_ramdisk.h"
	#include "ff_sddisk.h"
#endif

#if ( ffconfigMKDIR_RECURSIVE != 0 )
	#define MKDIR_FALSE    , pdFALSE
#else
	#define MKDIR_FALSE
#endif

#if ( ipconfigUSE_TCP_WIN == 0 )
	#warning sure?
#endif

/* Demo application includes. */
#include "hr_gettime.h"

#if USE_TCP_DEMO_CLI
	#include "http_client_test.h"
	#include "plus_tcp_demo_cli.h"
#endif

#include "UDPLoggingPrintf.h"

#include "eventLogging.h"

/* ST includes. */
#include "stm32f4xx_hal.h"

#include "NTPDemo.h"

#if ( USE_TELNET != 0 )
	#include "telnet.h"
	extern void xSetupTelnet();
#endif

#if ( USE_NTOP_TEST != 0 )
	#include "inet_pton_ntop_tests.h"
#endif

#include "ddos_testing.h"

#include "tcp_connect_demo.h"

/*#include "E:\temp\tcp_testing\SimpleTCPEchoServer.h" */

#define TESTING_PATCH    0

#if ( TESTING_PATCH != 0 )
	#warning Testing a PR
#endif

#ifndef STM32F4xx
	#error STM32F4xx is NOT defined
#endif

#define LED_GREEN          0
#define LED_ORANGE         1
#define LED_RED            2
#define LED_BLUE           3

#define LED_MASK_GREEN     GPIO_PIN_12
#define LED_MASK_ORANGE    GPIO_PIN_13
#define LED_MASK_RED       GPIO_PIN_14
#define LED_MASK_BLUE      GPIO_PIN_15

#if ( ipconfigETHERNET_DRIVER_FILTERS_PACKETS == 0 )
	#warning please define ipconfigETHERNET_DRIVER_FILTERS_PACKETS as 1
#endif

#ifdef __OPTIMIZE__
	#warning __OPTIMIZE__ is used
#else
	#warning __OPTIMIZE__ is NOT used
#endif

/* Should be declared in a test-version of the library. */
BaseType_t xTCP_Introduce_bug;

/*	typedef enum xIPPreference */
/*	{ */
/*		xPreferenceNone, */
/*		xPreferenceIPv4, */
/*		#if ( ipconfigUSE_IPv6 != 0 ) */
/*			xPreferenceIPv6, */
/*		#endif */
/*	} IPPreference_t; */
/* */
/*	IPPreference_t xDNS_IP_Preference; */

void Init_LEDs( void );
void Set_LED_Number( BaseType_t xNumber,
					 BaseType_t xValue );
void Set_LED_Mask( uint32_t ulMask,
				   BaseType_t xValue );

NetworkBufferStats_t xNetworkBufferStats; /* declared in FreeRTOSIPConfig.h */

#define mainUDP_SERVER_STACK_SIZE       2048
#define mainUDP_SERVER_TASK_PRIORITY    2
static void vUDPTest( void * pvParameters );

const char * FreeRTOS_strerror_r( BaseType_t xErrnum,
								  char * pcBuffer,
								  size_t uxLength );

BaseType_t xRandom32( uint32_t * pulValue );

#if ( ipconfigMULTI_INTERFACE == 0 )
	#ifndef ipSIZE_OF_IPv4_ADDRESS
		#define ipSIZE_OF_IPv4_ADDRESS    4
	#endif
	#define FREERTOS_AF_INET4             FREERTOS_AF_INET
#endif

#if ( HAS_ECHO_TEST != 0 )
	/* The CLI command "plus" will start an echo server. */
	#include "plus_echo_server.h"
#endif

#include "snmp_tests.h"
/* Send SNMP messages to a particular UC3 device. */
static void snmp_test( void );

static void start_RNG( void );

#if ( ipconfigTCP_KEEP_ALIVE == 0 )
	#warning Please define ipconfigTCP_KEEP_ALIVE
#endif

/*#include "cmsis_os.h" */
/* FTP and HTTP servers execute in the TCP server work task. */
#define mainTCP_SERVER_TASK_PRIORITY     ( tskIDLE_PRIORITY + 3 )
#define mainTCP_SERVER_STACK_SIZE        2048

#define mainFAT_TEST_STACK_SIZE          2048

#define mainRUN_STDIO_TESTS              0
#define mainHAS_RAMDISK                  0
#define mainHAS_SDCARD                   1
#define mainSD_CARD_DISK_NAME            "/"
#define mainSD_CARD_TESTING_DIRECTORY    "/fattest"

/* Remove this define if you don't want to include the +FAT test code. */
#define mainHAS_FAT_TEST                 USE_PLUS_FAT

/* Set the following constants to 1 to include the relevant server, or 0 to
 * exclude the relevant server. */
#define mainCREATE_FTP_SERVER            USE_PLUS_FAT
#define mainCREATE_HTTP_SERVER           USE_PLUS_FAT


/* Define names that will be used for DNS, LLMNR and NBNS searches. */
#define mainHOST_NAME           "stm"
/* http://stm32f4/index.html */
#define mainDEVICE_NICK_NAME    "stm32f4"

#ifndef BUFFER_FROM_WHERE_CALL
	#define BUFFER_FROM_WHERE_CALL( name )
#endif

extern const char * pcApplicationHostnameHook( void );

SemaphoreHandle_t xServerSemaphore;

static void crc_test();


/* Private variables ---------------------------------------------------------*/
#ifdef HAL_ADC_MODULE_ENABLED
	ADC_HandleTypeDef hadc3;
#endif

#ifdef HAL_DAC_MODULE_ENABLED
	DAC_HandleTypeDef hdac;
#endif

#ifdef HAL_DCMI_MODULE_ENABLED
	DCMI_HandleTypeDef hdcmi;
#endif

#ifdef HAL_I2C_MODULE_ENABLED
	I2C_HandleTypeDef hi2c1;
#endif

#ifdef HAL_RTC_MODULE_ENABLED
	RTC_HandleTypeDef hrtc;
#endif

#define USE_USART3_LOGGING    0 /* Needs also HAL_UART_MODULE_ENABLED */
#define USE_USART6_LOGGING    0 /* Needs also HAL_UART_MODULE_ENABLED */

#if ( USE_USART3_LOGGING )
	UART_HandleTypeDef huart3;
#endif
#if ( USE_USART6_LOGGING )
	UART_HandleTypeDef huart6;
#endif

/*#define logPrintf  lUDPLoggingPrintf */

RNG_HandleTypeDef hrng;

#ifdef HAL_NOR_MODULE_ENABLED
	NOR_HandleTypeDef hnor1;
#endif
SRAM_HandleTypeDef hsram2;
SRAM_HandleTypeDef hsram3;
/*osThreadId defaultTaskHandle; */

uint8_t retSD;     /* Return value for SD */
char SD_Path[ 4 ]; /* SD logical drive path */

typedef enum
{
	eIPv4 = 0x01,
	eIPv6 = 0x02,
} eIPVersion;

#if ( USE_PLUS_FAT != 0 )
	FF_Disk_t * pxSDDisk;
	BaseType_t doMountCard;
	BaseType_t cardMounted;
#endif

BaseType_t xDoStartupTasks = pdFALSE;

#if ( mainHAS_RAMDISK != 0 )
	static FF_Disk_t * pxRAMDisk;
#endif

#if ( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
	static TCPServer_t * pxTCPServer = NULL;
#endif

#ifndef ARRAY_SIZE
	#define  ARRAY_SIZE( x )    ( int ) ( sizeof( x ) / sizeof( x )[ 0 ] )
#endif

/*
 * Just seeds the simple pseudo random number generator.
 */
static void prvSRand( UBaseType_t ulSeed );
uint32_t ulInitialSeed;

/* Testing with UDP checksum 0 */
#define USE_ZERO_COPY    1
NetworkBufferDescriptor_t * pxDescriptor = NULL;
static void setUDPCheckSum( Socket_t xSocket,
							BaseType_t iUseChecksum );
Socket_t xUDPServerSocket = NULL;

#if ( USE_TCP_DEMO_CLI == 0 )
	int verboseLevel;
#endif

/* Private function prototypes -----------------------------------------------*/

/*
 * Creates a RAM disk, then creates files on the RAM disk.  The files can then
 * be viewed via the FTP server and the command line interface.
 */
#if ( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
	static void prvCreateDiskAndExampleFiles( void );
#endif

extern void vStdioWithCWDTest( const char * pcMountPath );
extern void vCreateAndVerifyExampleFiles( const char * pcMountPath );

#if ( ipconfigUSE_CALLBACKS != 0 )
	static BaseType_t xOnUdpReceive( Socket_t xSocket,
									 void * pvData,
									 size_t xLength,
									 const struct freertos_sockaddr * pxFrom,
									 const struct freertos_sockaddr * pxDest );
#endif

/*
 * The task that runs the FTP and HTTP servers.
 */
static void prvServerWorkTask( void * pvParameters );
static BaseType_t vHandleOtherCommand( Socket_t xSocket,
									   char * pcBuffer );

void SystemClock_Config( void );
static void MX_GPIO_Init( void );
#ifdef HAL_ADC_MODULE_ENABLED
	static void MX_ADC3_Init( void );
#endif
#ifdef HAL_DAC_MODULE_ENABLED
	static void MX_DAC_Init( void );
#endif
#ifdef HAL_DCMI_MODULE_ENABLED
	static void MX_DCMI_Init( void );
#endif

static void MX_FSMC_Init( void );

#ifdef HAL_I2C_MODULE_ENABLED
	static void MX_I2C1_Init( void );
#endif

#ifdef HAL_RTC_MODULE_ENABLED
	static void MX_RTC_Init( void );
#endif

#if ( USE_USART3_LOGGING )
	static void MX_USART3_UART_Init( void );
#endif
#if ( USE_USART6_LOGGING )
	static void MX_USART6_UART_Init( void );
#endif

static void heapInit( void );

void vShowTaskTable( BaseType_t aDoClear );
#if ( USE_IPERF != 0 )
	void vIPerfInstall( void );
#endif

uint32_t ulServerIPAddress;

/* Default MAC address configuration.  The demo creates a virtual network
 * connection that uses this MAC address by accessing the raw Ethernet data
 * to and from a real network connection on the host PC.  See the
 * configNETWORK_INTERFACE_TO_USE definition for information on how to configure
 * the real network connection to use. */
static const uint8_t ucMACAddress[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };

/* Use by the pseudo random number generator. */
static UBaseType_t ulNextRand;

/* Handle of the task that runs the FTP and HTTP servers. */
static TaskHandle_t xServerWorkTaskHandle = NULL;

#if ( ipconfigMULTI_INTERFACE != 0 )
	NetworkInterface_t * pxSTM32Fxx_FillInterfaceDescriptor( BaseType_t xEMACIndex,
															 NetworkInterface_t * pxInterface );
	NetworkInterface_t * pxLoopback_FillInterfaceDescriptor( BaseType_t xEMACIndex,
															 NetworkInterface_t * pxInterface );
#endif /* ipconfigMULTI_INTERFACE */
/*-----------------------------------------------------------*/

#if ( ipconfigMULTI_INTERFACE != 0 )
	static NetworkInterface_t xInterfaces[ 2 ];
	static NetworkEndPoint_t xEndPoints[ 4 ];
#endif /* ipconfigMULTI_INTERFACE */

#define TCP_printf()          do {} while( 0 )
#define TCP_debug_printf()    do {} while( 0 )

static NetworkInterface_t xInterface;
static NetworkEndPoint_t xEndPoints[ 4 ];

#if( ipconfigMULTI_INTERFACE == 1 )

#include "net_setup.h"

static int setup_endpoints()
{
	const char * pcMACAddress = "00-11-11-11-11-42";

	/* Initialise the STM32F network interface structure. */
	pxSTM32Fxx_FillInterfaceDescriptor( 0, &xInterface );

	SetupV4_t setup_v4 =
	{
		.pcMACAddress = pcMACAddress,
		.pcIPAddress = "192.168.2.107",
		.pcMask = "255.255.255.0",
		.pcGateway = "192.168.2.1",
		.eType = eStatic,  /* or eDHCP if you wish. */
//		.eType = eDHCP,
		.pcDNS[0] = "118.98.44.10",
		.pcDNS[1] = "118.98.44.100"
	};

	SetupV4_t setup_v4_second =
	{
		.pcMACAddress = "00-11-11-11-11-07", // pcMACAddress,
		.pcIPAddress = "172.16.0.107",
		.pcMask = "255.255.224.0",
		.pcGateway = "172.16.0.1",
		.eType = eStatic,  /* or eDHCP if you wish. */
//		.eType = eDHCP,
//		.pcDNS[0] = "118.98.44.10",
//		.pcDNS[1] = "118.98.44.100"
	};

	SetupV6_t setup_v6_global =
	{
		.pcMACAddress = pcMACAddress,
		.pcIPAddress = "2600:70ff:c066::2001",  /* create it dynamically, based on the prefix. */
		.pcPrefix = "2600:70ff:c066::",
		.uxPrefixLength = 64U,
		.pcGateway = "fe80::ba27:ebff:fe5a:d751",  /* GW will be set during RA or during DHCP. */
		.eType = eStatic,
//		.eType = eRA,  /* Use Router Advertising. */
//		.eType = eDHCP,  /* Use DHCPv6. */
		.pcDNS[0] = "fe80::1",
		.pcDNS[1] = "2001:4860:4860::8888",
	};

	SetupV6_t setup_v6_local =
	{
		.pcMACAddress = pcMACAddress,
		.pcIPAddress = "fe80::7004",  /* A easy-to-remember link-local IP address. */
		.pcPrefix = "fe80::",
		.uxPrefixLength = 10U,
		.eType = eStatic,  /* A fixed IP-address. */
	};

//	xSetupEndpoint_v4( &xInterface, &( xEndPoints[ 1 ] ), &setup_v4_second );
	xSetupEndpoint_v4( &xInterface, &( xEndPoints[ 0 ] ), &setup_v4 );
	xSetupEndpoint_v6( &xInterface, &( xEndPoints[ 2 ] ), &setup_v6_global );
	xSetupEndpoint_v6( &xInterface, &( xEndPoints[ 3 ] ), &setup_v6_local );

	FreeRTOS_IPInit_Multi();
}
#else
static int setup_endpoints()
{
		const uint8_t ucIPAddress[ 4 ] = { 192, 168, 2, 114 };
		const uint8_t ucNetMask[ 4 ] = { 255, 255, 255, 0 };
		const uint8_t ucGatewayAddress[ 4 ] = { 192, 168, 2, 1 };
		const uint8_t ucDNSServerAddress[ 4 ] = { 118, 98, 44, 10 };
//		const uint8_t ucDNSServerAddress[ 4 ] = { 203, 130, 196, 6 };
		FreeRTOS_IPInit( ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress );
}
#endif

int main( void )
{
	uint32_t ulSeed = 0x5a5a5a5a;

	/* MCU Configuration----------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* Configure the system clock */
	/*SystemClock_Config(); */

	heapInit();


	#if ( USE_LOG_EVENT != 0 )
		{
			iEventLogInit();
			eventLogAdd( "Main" );
		}
	#endif

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	#ifdef HAL_ADC_MODULE_ENABLED
		MX_ADC3_Init();
	#endif
	#ifdef HAL_DAC_MODULE_ENABLED
		MX_DAC_Init();
	#endif
	#ifdef HAL_DCMI_MODULE_ENABLED
		MX_DCMI_Init();
	#endif
	MX_FSMC_Init();
	#ifdef HAL_I2C_MODULE_ENABLED
		MX_I2C1_Init();
	#endif
	#ifdef HAL_RTC_MODULE_ENABLED
		MX_RTC_Init();
	#endif
	#if ( USE_USART3_LOGGING )
		{
			MX_USART3_UART_Init();
		}
	#endif
	#if ( USE_USART6_LOGGING )
		{
			MX_USART6_UART_Init();
		}
	#endif

	Init_LEDs();

/*	const int version = 3; */
/*	TCP_printf( "This beautiful program version %d is started\n", version ); */

	/* Timer2 initialization function.
	 * ullGetHighResolutionTime() will be used to get the running time in uS. */
	vStartHighResolutionTimer();

	start_RNG();

	/* Initialise the network interface.
	 *
	 ***NOTE*** Tasks that use the network are created in the network event hook
	 * when the network is connected and ready for use (see the definition of
	 * vApplicationIPNetworkEventHook() below).  The address values passed in here
	 * are used if ipconfigUSE_DHCP is set to 0, or if ipconfigUSE_DHCP is set to 1
	 * but a DHCP server cannot be	contacted. */

	#if ( TESTING_PATCH == 0 )
		#if ( ipconfigUSE_IPv6 != 0 )
			xDNS_IP_Preference = xPreferenceIPv4;
		#endif
	#endif

    memcpy( ipLOCAL_MAC_ADDRESS, ucMACAddress, sizeof ucMACAddress );

    setup_endpoints();

	/* Create the task that handles the FTP and HTTP servers.  This will
	 * initialise the file system then wait for a notification from the network
	 * event hook before creating the servers.  The task is created at the idle
	 * priority, and sets itself to mainTCP_SERVER_TASK_PRIORITY after the file
	 * system has initialised. */
	xTaskCreate( prvServerWorkTask, "SvrWork", mainTCP_SERVER_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &xServerWorkTaskHandle );

	/* Start the RTOS scheduler. */
	FreeRTOS_debug_printf( ( "vTaskStartScheduler\n" ) );
	eventLogAdd( "vTaskStartScheduler" );
	vTaskStartScheduler();

	/* If all is well, the scheduler will now be running, and the following
	 * line will never be reached.  If the following line does execute, then
	 * there was insufficient FreeRTOS heap memory available for the idle and/or
	 * timer tasks	to be created.  See the memory management section on the
	 * FreeRTOS web site for more details.  http://www.freertos.org/a00111.html */
	for( ; ; )
	{
	}
}
/*-----------------------------------------------------------*/

void HAL_RNG_MspInit( RNG_HandleTypeDef * hrng )
{
	( void ) hrng;
/*	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct; */
	__HAL_RCC_RNG_CLK_ENABLE();

/*	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC; */
/*	PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI; */
/*	HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct); */
	/* Enable RNG clock source */
	RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;

	/* RNG Peripheral enable */
	RNG->CR |= RNG_CR_RNGEN;

	/*Select PLLQ output as RNG clock source */
/*	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RNG; */
/*	PeriphClkInitStruct.RngClockSelection = RCC_RNGCLKSOURCE_PLL; */
/*	HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct); */
}

BaseType_t xApplicationGetRandomNumber( uint32_t * pulValue )
{
	HAL_StatusTypeDef xResult;
	BaseType_t xReturn;
	uint32_t ulValue;

	xResult = HAL_RNG_GenerateRandomNumber( &hrng, &ulValue );

	if( xResult == HAL_OK )
	{
		xReturn = pdPASS;
		*pulValue = ulValue;
	}
	else
	{
		xReturn = pdFAIL;
	}

	return xReturn;
}

BaseType_t xRandom32( uint32_t * pulValue )
{
	return xApplicationGetRandomNumber( pulValue );
}

#if ( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
	static void prvCreateDiskAndExampleFiles( void )
	{
		#if ( mainHAS_RAMDISK != 0 )
			static uint8_t ucRAMDisk[ mainRAM_DISK_SECTORS * mainRAM_DISK_SECTOR_SIZE ];
		#endif

		#if USE_TCP_DEMO_CLI
			verboseLevel = 0;
		#endif
		#if ( mainHAS_RAMDISK != 0 )
			{
				FreeRTOS_printf( ( "Create RAM-disk\n" ) );
				/* Create the RAM disk. */
				pxRAMDisk = FF_RAMDiskInit( mainRAM_DISK_NAME, ucRAMDisk, mainRAM_DISK_SECTORS, mainIO_MANAGER_CACHE_SIZE );
				configASSERT( pxRAMDisk );

				/* Print out information on the RAM disk. */
				FF_RAMDiskShowPartition( pxRAMDisk );
			}
		#endif /* mainHAS_RAMDISK */
		#if ( mainHAS_SDCARD != 0 ) && ( USE_PLUS_FAT != 0 )
			{
				FreeRTOS_printf( ( "Mount SD-card\n" ) );
				/* Create the SD card disk. */
				pxSDDisk = FF_SDDiskInit( mainSD_CARD_DISK_NAME );
				FreeRTOS_printf( ( "Mount SD-card done\n" ) );
				#if ( mainRUN_STDIO_TESTS == 1 )
					if( pxSDDisk != NULL )
					{
						/* Remove the base directory again, ready for another loop. */
						ff_deltree( mainSD_CARD_TESTING_DIRECTORY );

						/* Make sure that the testing directory exists. */
						ff_mkdir( mainSD_CARD_TESTING_DIRECTORY MKDIR_FALSE );

						/* Create a few example files on the disk.  These are not deleted again. */
						vCreateAndVerifyExampleFiles( mainSD_CARD_TESTING_DIRECTORY );

						/* A few sanity checks only - can only be called after
						 * vCreateAndVerifyExampleFiles().  NOTE:  The tests take a relatively long
						 * time to execute and the FTP and HTTP servers will not start until the
						 * tests have completed. */

						vStdioWithCWDTest( mainSD_CARD_TESTING_DIRECTORY );
					}
				#endif /* if ( mainRUN_STDIO_TESTS == 1 ) */
			}
		#endif /* mainHAS_SDCARD */
	}
#endif /* ( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) ) */

/*-----------------------------------------------------------*/

#if ( USE_PLUS_FAT != 0 )
	static int iFATRunning = 0;

	static void prvFileSystemAccessTask( void * pvParameters )
	{
		const char * pcBasePath = ( const char * ) pvParameters;

		while( iFATRunning != 0 )
		{
			ff_deltree( pcBasePath );
			ff_mkdir( pcBasePath MKDIR_FALSE );
			vCreateAndVerifyExampleFiles( pcBasePath );
			vStdioWithCWDTest( pcBasePath );
		}

		FreeRTOS_printf( ( "%s: done\n", pcBasePath ) );
		vTaskDelete( NULL );
	}

#endif /* USE_PLUS_FAT */

#if ( USE_TELNET != 0 )
	Telnet_t * pxTelnetHandle = NULL;

	void vUDPLoggingHook( const char * pcMessage,
						  BaseType_t xLength )
	{
		if( ( pxTelnetHandle != NULL ) && ( pxTelnetHandle->xClients != NULL ) )
		{
/* Skip the telnet logging. */
/*#warning Skip the telnet logging */
			xTelnetSend( pxTelnetHandle, NULL, pcMessage, xLength );
		}

		#if ( USE_USART3_LOGGING != 0 )
			/*HAL_USART_Transmit(&huart3, (uint8_t *)pcMessage, xLength, 100); */
			HAL_UART_Transmit( &huart3, ( uint8_t * ) pcMessage, xLength, 100 );
		#endif
		#if ( USE_USART6_LOGGING != 0 )
			/*HAL_USART_Transmit(&huart3, (uint8_t *)pcMessage, xLength, 100); */
			HAL_UART_Transmit( &huart6, ( uint8_t * ) pcMessage, xLength, 100 );
		#endif
	}
#endif /* if ( USE_TELNET != 0 ) */

#if ( ipconfigUSE_DHCP_HOOK != 0 )
	eDHCPCallbackAnswer_t xApplicationDHCPHook( eDHCPCallbackPhase_t eDHCPPhase,
												uint32_t ulIPAddress )
	{
		eDHCPCallbackAnswer_t eAnswer = eDHCPContinue;

		const char * name = "Unknown";

		switch( eDHCPPhase )
		{
			case eDHCPPhasePreDiscover:
				name = "Discover"; /* Driver is about to send a DHCP discovery. */
/*#warning Testing here */
/*			eAnswer = eDHCPUseDefaults; */
				break;

			case eDHCPPhasePreRequest:
				name = "Request"; /* Driver is about to request DHCP an IP address. */
/*#warning Testing here */
/*			eAnswer = eDHCPUseDefaults; */
				break;
		}

		FreeRTOS_printf( ( "DHCP %s address %xip\n", name, ( unsigned ) FreeRTOS_ntohl( ulIPAddress ) ) );
		return eAnswer;
	}
#endif /* ipconfigUSE_DHCP_HOOK */

#if ( USE_SIMPLE_TCP_SERVER != 0 )
	void vStartSimpleTCPServerTasks( uint16_t usStackSize,
									 UBaseType_t uxPriority );
#endif

#if 1
	static void prvServerWorkTask( void * pvParameters )
	{
		#if ( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
			const TickType_t xInitialBlockTime = pdMS_TO_TICKS( 200UL );
			BaseType_t xWasPresent = pdFALSE, xIsPresent = pdFALSE;
		#endif
		BaseType_t xHadSocket = pdFALSE;
		char pcBuffer[ 64 ];
		BaseType_t xCount;

		eventLogAdd( "prvServerWorkTask" );
		FreeRTOS_printf( ( "Start prvServerWorkTask\n" ) );

/* A structure that defines the servers to be created.  Which servers are
 * included in the structure depends on the mainCREATE_HTTP_SERVER and
 * mainCREATE_FTP_SERVER settings at the top of this file. */
		#if ( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
			static const struct xSERVER_CONFIG xServerConfiguration[] =
			{
				#if ( mainCREATE_HTTP_SERVER == 1 )
					/* Server type,		port number,	backlog,    root dir. */
					{ eSERVER_HTTP, 80, 12, configHTTP_ROOT },
				#endif

				#if ( mainCREATE_FTP_SERVER == 1 )
					/* Server type,		port number,	backlog,    root dir. */
					{ eSERVER_FTP,  21, 12, ""              }
				#endif
			};
		#endif /* if ( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) ) */

		/* Remove compiler warning about unused parameter. */
		( void ) pvParameters;

		/* If the CLI is included in the build then register commands that allow
		 * the file system to be accessed. */
		#if ( mainCREATE_UDP_CLI_TASKS == 1 )
			{
				vRegisterFileSystemCLICommands();
			}
		#endif /* mainCREATE_UDP_CLI_TASKS */



		/* The priority of this task can be raised now the disk has been
		 * initialised. */
		vTaskPrioritySet( NULL, mainTCP_SERVER_TASK_PRIORITY );

		/* Wait until the network is up before creating the servers.  The
		 * notification is given from the network event hook. */
		ulTaskNotifyTake( pdTRUE, portMAX_DELAY );


		FreeRTOS_printf( ( "prvServerWorkTask starts running\n" ) );

		/* Create the servers defined by the xServerConfiguration array above. */
		#if ( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
			pxTCPServer = FreeRTOS_CreateTCPServer( xServerConfiguration, sizeof( xServerConfiguration ) / sizeof( xServerConfiguration[ 0 ] ) );
			configASSERT( pxTCPServer );
		#endif

		#if ( CONFIG_USE_LWIP != 0 )
			{
				vLWIP_Init();
			}
		#endif
		xServerSemaphore = xSemaphoreCreateBinary();
		configASSERT( xServerSemaphore != NULL );

		#if ( USE_TELNET != 0 )
			Telnet_t xTelnet;
			memset( &xTelnet, '\0', sizeof xTelnet );
		#endif

		for( ; ; )
		{
			TickType_t xReceiveTimeOut = pdMS_TO_TICKS( 200 );

			xSemaphoreTake( xServerSemaphore, xReceiveTimeOut );

			if( xDoStartupTasks != pdFALSE )
			{
				xDoStartupTasks = pdFALSE;

				/* Tasks that use the TCP/IP stack can be created here. */
				/* Start a new task to fetch logging lines and send them out. */
				vUDPLoggingTaskCreate();

				/*bosman_open_listen_socket(); */

				#if ( mainCREATE_TCP_ECHO_TASKS_SINGLE == 1 )
					{
						/* See http://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/TCP_Echo_Clients.html */
						vStartHTTPClientTest( configMINIMAL_STACK_SIZE * 4, tskIDLE_PRIORITY + 1 );
					}
				#endif

				#if ( mainCREATE_SIMPLE_TCP_ECHO_SERVER == 1 )
					{
						/* See http://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/TCP_Echo_Server.html */
						vStartSimpleTCPServerTasks( configMINIMAL_STACK_SIZE * 4, tskIDLE_PRIORITY + 1 );
					}
				#endif

				#if ( USE_NTP_DEMO != 0 )
					{
						/*vStartNTPTask( configMINIMAL_STACK_SIZE * 4, tskIDLE_PRIORITY + 1 ); */
					}
				#endif /* ( USE_NTP_DEMO != 0 ) */
				/*xTaskCreate( TcpServerTask,     "TCPTask", mainTCP_SERVER_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, NULL ); */
				/*vStartSimpleTCPServerTasks( 2048U, 2U ); */
			}

			#if USE_TCP_DEMO_CLI
				{
					/* Let the CLI task do its regular work. */
					xHandleTesting();
				}
			#endif

			#if ( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
				if( doMountCard )
				{
					doMountCard = 0;
					/* Create the disk used by the FTP and HTTP servers. */
					FreeRTOS_printf( ( "Creating files\n" ) );
					prvCreateDiskAndExampleFiles();
					cardMounted = ( pxSDDisk != NULL ) ? 1 : 0;

					if( cardMounted )
					{
						xIsPresent = xWasPresent = FF_SDDiskDetect( pxSDDisk );
					}

					FreeRTOS_printf( ( "FF_SDDiskDetect returns -> %ld\n", xIsPresent ) );
				}

				if( cardMounted )
				{
					xIsPresent = FF_SDDiskDetect( pxSDDisk );

					if( xWasPresent != xIsPresent )
					{
						if( xIsPresent == pdFALSE )
						{
							FreeRTOS_printf( ( "SD-card now removed (%ld -> %ld)\n", xWasPresent, xIsPresent ) );

							if( ( pxSDDisk != NULL ) && ( pxSDDisk->pxIOManager != NULL ) )
							{
								/* Invalidate all open file handles so they
								 * will get closed by the application. */
								FF_Invalidate( pxSDDisk->pxIOManager );
								FF_SDDiskUnmount( pxSDDisk );
							}
						}
						else
						{
							FreeRTOS_printf( ( "SD-card now present (%ld -> %ld)\n", xWasPresent, xIsPresent ) );

							if( pxSDDisk != NULL )
							{
								BaseType_t xResult;
								FF_SDDiskReinit( pxSDDisk );
								xResult = FF_SDDiskMount( pxSDDisk );
								FF_PRINTF( "FF_SDDiskMount: SD-card %s\n", xResult > 0 ? "OK" : "Failed" );

								if( xResult > 0 )
								{
									FF_SDDiskShowPartition( pxSDDisk );
								}
							}
							else
							{
								prvCreateDiskAndExampleFiles();
							}
						}

						xWasPresent = xIsPresent;
					}
					else
					{
						/*				FreeRTOS_printf( ( "SD-card still %d\n", xIsPresent ) ); */
					}

					FreeRTOS_TCPServerWork( pxTCPServer, xInitialBlockTime );
				}
			#endif /* mainCREATE_FTP_SERVER == 1 || mainCREATE_HTTP_SERVER == 1 */

			#if ( configUSE_TIMERS == 0 )
				{
					/* As there is not Timer task, toggle the LED 'manually'. */
/*				vParTestToggleLED( mainLED ); */
					HAL_GPIO_TogglePin( GPIOG, GPIO_PIN_6 );
				}
			#endif
			xSocket_t xSocket = xLoggingGetSocket();
			struct freertos_sockaddr xSourceAddress;
			socklen_t xSourceAddressLength = sizeof( xSourceAddress );
/*{ */
/*	extern void vSocketWakeupCallback( Socket_t xSocket ); */
/* */
/*	FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_WAKEUP_CALLBACK, &( vSocketWakeupCallback ),  sizeof( void * ) ); */
/*	#error disable please */
/*} */
			{
/*			static BaseType_t xNumber = -1; */
/*			if( xNumber >= 0 ) */
/*				Set_LED_Number( xNumber, pdFALSE ); */
/*			if( ++xNumber >= 4 ) */
/*				xNumber = 0; */
/*			Set_LED_Number( xNumber, pdTRUE ); */
			}

			if( xSocket == NULL )
			{
				continue;
			}

			if( xHadSocket == pdFALSE )
			{
				xHadSocket = pdTRUE;
				FreeRTOS_printf( ( "prvCommandTask started\n" ) );

				/* xServerSemaphore will be given to when there is data for xSocket
				 * and also as soon as there is USB/CDC data. */
				FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_SET_SEMAPHORE, ( void * ) &xServerSemaphore, sizeof( xServerSemaphore ) );
				TickType_t xReceiveTimeOut = pdMS_TO_TICKS( 10 );
				FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof( xReceiveTimeOut ) ); 
				#if ( ipconfigUSE_CALLBACKS != 0 )
					{
						#warning enabled again
/*				F_TCP_UDP_Handler_t xHandler; */
/*				memset( &xHandler, '\0', sizeof ( xHandler ) ); */
/*				xHandler.pOnUdpReceive = xOnUdpReceive; */
/*				FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_UDP_RECV_HANDLER, ( void * ) &xHandler, sizeof( xHandler ) ); */
					}
				#endif
				#if ( USE_TELNET != 0 )
					{
						extern TickType_t uxTelnetAcceptTmout;
						uxTelnetAcceptTmout = pdMS_TO_TICKS( 0U );
						xTelnetCreate( &xTelnet, TELNET_PORT_NUMBER );

						if( xTelnet.xParentSocket != 0 )
						{
							FreeRTOS_setsockopt( xTelnet.xParentSocket, 0, FREERTOS_SO_SET_SEMAPHORE, ( void * ) &xServerSemaphore, sizeof( xServerSemaphore ) );
							pxTelnetHandle = &xTelnet;
						}

						/* For testing only : start up a second telnet server. */
/*						xSetupTelnet(); */
					}
				#endif /* if ( USE_TELNET != 0 ) */
				#if ( USE_ECHO_TASK != 0 ) && ( USE_TCP_DEMO_CLI != 0 )
					{
						vStartHTTPClientTest( configMINIMAL_STACK_SIZE * 4, tskIDLE_PRIORITY + 1 );
					}
				#endif /* ( USE_ECHO_TASK != 0 ) */
				{
					xSocket_t xTest;
					void * id1;
					BaseType_t r1;
					void * id2;

					xTest = xSocket;
					/* Check the current value of the SocketID. */
					id1 = pvSocketGetSocketID( xTest );
					/* Change value of the SocketID. */
					r1 = xSocketSetSocketID( xTest, ( void * ) 0x12345678 );
					/* Check the current value of the SocketID. */
					id2 = pvSocketGetSocketID( xTest );
					FreeRTOS_printf( ( "SocketSetSocketID %p -> %p r1 = %d", id1, id2, ( int ) r1 ) );

					xTest = FREERTOS_INVALID_SOCKET;
					id1 = pvSocketGetSocketID( xTest );
					r1 = xSocketSetSocketID( xTest, ( void * ) 0x12345678 );
					id2 = pvSocketGetSocketID( xTest );
					FreeRTOS_printf( ( "SocketSetSocketID %p -> %p r1 = %d", id1, id2, ( int ) r1 ) );
				}
			}

			#if ( USE_ZERO_COPY )
				{
					if( pxDescriptor != NULL )
					{
						vReleaseNetworkBufferAndDescriptor( BUFFER_FROM_WHERE_CALL( "main.c" ) pxDescriptor );
						pxDescriptor = NULL;
					}

					char * ppcBuffer;
					xCount = FreeRTOS_recvfrom( xSocket, ( void * ) &ppcBuffer, sizeof( pcBuffer ) - 1, FREERTOS_MSG_DONTWAIT | FREERTOS_ZERO_COPY, &xSourceAddress, &xSourceAddressLength );

					if( xCount > 0 )
					{
						if( ( ( size_t ) xCount ) > ( sizeof pcBuffer - 1 ) )
						{
							xCount = ( BaseType_t ) ( sizeof pcBuffer - 1 );
						}

						memcpy( pcBuffer, ppcBuffer, xCount );
						pcBuffer[ xCount ] = '\0';
						pxDescriptor = pxUDPPayloadBuffer_to_NetworkBuffer( ( const void * ) ppcBuffer );
					}
				}
			#else /* if ( USE_ZERO_COPY ) */
				{
					xCount = FreeRTOS_recvfrom( xSocket, ( void * ) pcBuffer, sizeof( pcBuffer ) - 1, FREERTOS_MSG_DONTWAIT,
												&xSourceAddress, &xSourceAddressLength );
				}
			#endif /* if( USE_ZERO_COPY ) */
			#if ( USE_TELNET != 0 )
				{
					if( xCount <= 0 ) /*  && ( xTelnet.xParentSocket != NULL ) ) */
					{
						struct freertos_sockaddr fromAddr;
						xCount = xTelnetRecv( &xTelnet, &fromAddr, pcBuffer, sizeof( pcBuffer ) - 1 );

						if( xCount > 0 )
						{
							xTelnetSend( &xTelnet, &fromAddr, pcBuffer, xCount );
						}
					}
				}
			#endif /* if ( USE_TELNET != 0 ) */

			if( xCount > 0 )
			{
				pcBuffer[ xCount ] = '\0';

				/* Strip any line terminators. */
				while( ( xCount > 0 ) && ( ( pcBuffer[ xCount - 1 ] == 13 ) || ( pcBuffer[ xCount - 1 ] == 10 ) ) )
				{
					pcBuffer[ --xCount ] = 0;
				}

				if( ( pcBuffer[ 0 ] == 'f' ) && isdigit( pcBuffer[ 1 ] ) && ( pcBuffer[ 2 ] == ' ' ) )
				{
					/* Ignore messages like "f7 dnsq" */
					if( pcBuffer[ 1 ] != '4' )
					{
						continue;
					}

					xCount -= 3;
					/* "f7 ver" */
					memmove( pcBuffer, pcBuffer + 3, xCount + 1 );
				}

				#if ( USE_TCP_DEMO_CLI != 0 )
					if( xHandleTestingCommand( pcBuffer, sizeof( pcBuffer ) ) == pdTRUE )
					{
						/* Command has been handled. */
					}
					else
				#endif /* ( USE_TCP_DEMO_CLI != 0 ) */

				if( vHandleOtherCommand( xSocket, pcBuffer ) == pdTRUE )
				{
					/* Command has been handled locally. */
				}
				else
				{
					FreeRTOS_printf( ( "Unknown command: '%s'\n", pcBuffer ) );
				}
			}
		} /* for( ;; ) */
	}
/*-----------------------------------------------------------*/
#endif /* if 1 */

static BaseType_t vHandleOtherCommand( Socket_t xSocket,
									   char * pcBuffer )
{
	/* Assume that the command will be handled. */
	BaseType_t xReturn = pdTRUE;

	( void ) xSocket;

	if( strncmp( pcBuffer, "snmp_test", 9 ) == 0 )
	{
		/* Send SNMP messages to a particular UC3 device. */
		snmp_test();
	}

	#if ( ipconfigMULTI_INTERFACE != 0 ) && ( TESTING_PATCH == 0 )
		else if( ( strncmp( pcBuffer, "dns4", 4 ) == 0 ) || ( strncmp( pcBuffer, "dns6", 4 ) == 0 ) )
		{
/*	#warning please enable again */
/*	FreeRTOS_printf( ( "DNS type disabled\n" ) ); */
			BaseType_t xCode = 4;
			#if ( ipconfigUSE_IPv6 != 0 )
				if( pcBuffer[ 3 ] == '6' )
				{
					xDNS_IP_Preference = xPreferenceIPv6;
					xCode = 6;
				}
				else
			#endif
			{
				xDNS_IP_Preference = xPreferenceIPv4;
			}

			FreeRTOS_printf( ( "DNS now IPv%d encoded\n", ( int ) xCode ) );
		}
	#endif /* ( ipconfigMULTI_INTERFACE != 0 ) */
	else if( strncmp( pcBuffer, "buffer_test", 11 ) == 0 )
	{
		/* Send SNMP messages to a particular UC3 device. */
		int index;

		for( index = 0; index < 2; index++ )
		{
			size_t length = index ? 1024 : 0U;
			NetworkBufferDescriptor_t * pxNetworkBuffer = pxGetNetworkBufferWithDescriptor( BUFFER_FROM_WHERE_CALL( "main(1).c" ) length, 1000U );
			FreeRTOS_printf( ( "%d: Buffer = %p length = %u\n",
							   index,
							   pxNetworkBuffer->pucEthernetBuffer,
							   pxNetworkBuffer->xDataLength ) );
		}
	}
	else if( strncmp( pcBuffer, "ver", 3 ) == 0 )
	{
		FreeRTOS_printf( ( "Verbose is %d\n", verboseLevel ) );
	}
	else if( strncmp( pcBuffer, "arp", 3 ) == 0 )
	{
		FreeRTOS_PrintARPCache();
	}
	else if( strncmp( pcBuffer, "hasbug", 6 ) == 0 )
	{
		/* hasbug 0 = all normal. */
		/* hasbug 1 = protocol error. */
		/* hasbug 2 = hanging sochet. */
		extern BaseType_t xTCP_Introduce_bug;
		char * ptr = pcBuffer + 6;

		while( *ptr && !isdigit( *ptr ) )
		{
			ptr++;
		}

		if( isdigit( *ptr ) )
		{
			xTCP_Introduce_bug = *ptr - '0';
		}

		#if ( TELNET_USES_REUSE_SOCKETS != 0 )
			BaseType_t xReusable = pdTRUE;
		#else
			BaseType_t xReusable = pdFALSE;
		#endif
		FreeRTOS_printf( ( "xTCP_Introduce_bug = %d reusable %s\n", ( int ) xTCP_Introduce_bug, xReusable ? "true" : "false" ) );
/*		FreeRTOS_printf( ( "xTCP_Introduce_bug not implemented. \n" ) ); */
	}
	else if( strncmp( pcBuffer, "connect", 7 ) == 0 )
	{
		handle_user_test( FreeRTOS_inet_addr_quick( 192, 168, 2, 5 ), FreeRTOS_htons( 32002 ) );
	}
/*	else if( strncmp( pcBuffer, "showcache", 9 ) == 0 ) */
/*	{ */
/*		extern void dns_show_cache( void ); */
/*		dns_show_cache(); */
/*	} */
	else if( strncmp( pcBuffer, "rx", 2 ) == 0 )
	{
		extern uint32_t rxPacketCount;
		FreeRTOS_printf( ( "STM rx = %lu\n", rxPacketCount ) );
		rxPacketCount = 0U;
	}

	else if( strncmp( pcBuffer, "stm ddos", 8 ) == 0 )
	{
		vDDoSCommand( pcBuffer + 8 );
	}

	else if( strncmp( pcBuffer, "stm client", 10 ) == 0 )
	{
		vTCPClientCommand( pcBuffer + 10 );
	}

	else if( strncmp( pcBuffer, "stm server", 10 ) == 0 )
	{
		vTCPServerCommand( pcBuffer + 10 );
	}

	else if( strncmp( pcBuffer, "stm buffer", 10 ) == 0 )
	{
		vBufferCommand( pcBuffer + 10 );
	}

	else if( strncmp( pcBuffer, "stm", 3 ) == 0 )
	{
		int32_t diff;

		if( xNetworkBufferStats.ulAllocated >= xNetworkBufferStats.ulReleased )
		{
			diff = ( int32_t ) 0 - ( ( int32_t ) ( xNetworkBufferStats.ulAllocated - xNetworkBufferStats.ulReleased ) );
		}
		else
		{
			diff = ( int32_t ) ( xNetworkBufferStats.ulAllocated - xNetworkBufferStats.ulReleased );
		}

		FreeRTOS_printf( ( "Alloc %lu Failed %lu Released %lu Busy %ld\n",
						   xNetworkBufferStats.ulAllocated,
						   xNetworkBufferStats.ulFailed,
						   xNetworkBufferStats.ulReleased,
						   diff ) );
	}

	else if( strncmp( pcBuffer, "time", 4 ) == 0 )
	{
		time_t currentTime;
		extern time_t time( time_t * puxTime );

		time( &( currentTime ) );
		FreeRTOS_printf( ( "Current time %u TickCount %u HR-time %u\n",
						   ( unsigned ) currentTime,
						   ( unsigned ) xTaskGetTickCount(),
						   ( unsigned ) ullGetHighResolutionTime() ) );
	}
	#if ( HAS_ECHO_TEST != 0 )
		else if( strncmp( pcBuffer, "plus abort", 10 ) == 0 )
		{
			FreeRTOS_printf( ( "Change uxServerAbort from %u\n", ( unsigned ) uxServerAbort ) );
			uxServerAbort = 0x03;
		}
		else if( strncmp( pcBuffer, "plus", 4 ) == 0 )
		{
			int iDoStart = 1;
			char * ptr = pcBuffer + 4;

			while( *ptr && !isdigit( *ptr ) )
			{
				ptr++;
			}

			if( !ulServerIPAddress )
			{
				ulServerIPAddress = FreeRTOS_GetIPAddress();
			}

			if( *ptr )
			{
				int value2;
				char ch;
				char * tail = ptr;
				int rc = sscanf( ptr, "%d%c%d", &iDoStart, &ch, &value2 );

				while( *tail && !isspace( ( int ) *tail ) )
				{
					tail++;
				}

				if( isspace( ( int ) *tail ) )
				{
					*( tail++ ) = 0;

					while( isspace( ( int ) *tail ) )
					{
						tail++;
					}
				}

				if( rc >= 3 )
				{
					ulServerIPAddress = FreeRTOS_inet_addr( ptr );
					iDoStart = 1;
				}
				else if( rc < 1 )
				{
					iDoStart = 0;
				}

				if( *tail )
				{
					unsigned uValue;

					if( sscanf( tail, "%u", &uValue ) >= 1 )
					{
						FreeRTOS_printf( ( "Exchange %u bytes\n", uValue ) );
						uxClientSendCount = uValue;
					}
				}
			}

			plus_echo_start( iDoStart );
			FreeRTOS_printf( ( "Plus testing has %s for %xip\n", iDoStart ? "started" : "stopped",
							   ( unsigned ) FreeRTOS_ntohl( ulServerIPAddress ) ) );
		}
	#endif /* ( HAS_ECHO_TEST != 0 ) */
	else if( strncmp( pcBuffer, "metrics", 7 ) == 0 )
	{
		MetricsType_t xMetrics;
		vGetMetrics( &( xMetrics ) );
		vShowMetrics( &( xMetrics ) );
	}

	#if ( ipconfigUSE_IPv6 != 0 )
		else if( strncmp( pcBuffer, "trial", 5 ) == 0 )
		{
			IPv6_Address_t xIPAddress;
			IPv6_Address_t xPrefix;
			IPv6_Address_t xGateWay;
			IPv6_Address_t xDNSServer;

			FreeRTOS_inet_pton6( "2001:470:ec54::", xPrefix.ucBytes );
			FreeRTOS_inet_pton6( "2001:4860:4860::8888", xDNSServer.ucBytes );

			FreeRTOS_CreateIPv6Address( &xIPAddress, &xPrefix, 64, pdTRUE );
			FreeRTOS_inet_pton6( "fe80::9355:69c7:585a:afe7", xGateWay.ucBytes );
			FreeRTOS_printf( ( "Prefix %pip IP %pip GW %pip DNS %pip \n",
							   xPrefix.ucBytes,
							   xIPAddress.ucBytes,
							   xGateWay.ucBytes,
							   xDNSServer.ucBytes ) );

			char source[] = "2001:4860:4860:4860:4860:4860:4860:8888";
			uint16_t usTarget[ 16 ];
			memset( usTarget, 255, sizeof usTarget );
			FreeRTOS_inet_pton6( source, ( void * ) usTarget );
			FreeRTOS_printf( ( "Result = %x:%x:%x:%x:%x:%x:%x:%x\n",
							   usTarget[ 0 ], usTarget[ 1 ], usTarget[ 2 ], usTarget[ 3 ],
							   usTarget[ 4 ], usTarget[ 5 ], usTarget[ 6 ], usTarget[ 7 ] ) );
			FreeRTOS_printf( ( "Result = %x:%x:%x:%x:%x:%x:%x:%x\n",
							   usTarget[ 8 ], usTarget[ 9 ], usTarget[ 10 ], usTarget[ 11 ],
							   usTarget[ 12 ], usTarget[ 13 ], usTarget[ 14 ], usTarget[ 15 ] ) );
		}
	#endif /* ( ipconfigUSE_IPv6 != 0 ) */

	else if( strncmp( pcBuffer, "ledtest", 7 ) == 0 )
	{
		static BaseType_t xNumber = -1;

		if( xNumber >= 0 )
		{
			Set_LED_Number( xNumber, pdFALSE );
		}

		if( ++xNumber >= 4 )
		{
			xNumber = 0;
		}

		Set_LED_Number( xNumber, pdTRUE );
		FreeRTOS_printf( ( "ledtest %d\n", ( int ) xNumber ) );
	}
	#if ( ipconfigUSE_IPv6 != 0 )
		else if( strncmp( pcBuffer, "hopbyhop", 8 ) == 0 )
		{
			extern void ICMPEchoTest_v6( size_t uxOptionCount,
										 BaseType_t xSendRequest );

			ICMPEchoTest_v6( 4, pdTRUE );
			ICMPEchoTest_v6( 4, pdFALSE );
		}
	#endif
	#if ( ipconfigUSE_IPv6 != 0 )
		else if( strncmp( pcBuffer, "echotest", 8 ) == 0 )
		{
			size_t xOoptionCount;
			extern void ICMPEchoTest( size_t uxOptionCount,
									  BaseType_t xSendRequest );

			for( xOoptionCount = 0U; xOoptionCount <= 10U; xOoptionCount += 2U )
			{
				ICMPEchoTest( xOoptionCount, pdTRUE );
				ICMPEchoTest( xOoptionCount, pdFALSE );
			}
		}
	#endif /* if ( ipconfigUSE_IPv6 != 0 ) */
	#if ( ipconfigMULTI_INTERFACE == 0 )
		else if( strncmp( pcBuffer, "arp", 4 ) == 0 )
		{
			xARPWaitResolution( FreeRTOS_GetGatewayAddress(), pdMS_TO_TICKS( 100 ) );
			FreeRTOS_printf( ( "Looked up %xip\n", ( unsigned ) FreeRTOS_ntohl( FreeRTOS_GetGatewayAddress() ) ) );
		}
	#endif /* ( ipconfigMULTI_INTERFACE == 0 ) */
	#if ( HAS_ARP_TEST != 0 )
		else if( strncmp( pcBuffer, "arptest", 7 ) == 0 )
		{
			extern void arp_test( void );
			arp_test();
		}
	#endif
	#if ( HAS_SDRAM_TEST != 0 )
		else if( strncmp( pcBuffer, "memtest", 7 ) == 0 )
		{
			extern void sdram_test( void );
			sdram_test();
		}
	#endif /* ( HAS_SDRAM_TEST != 0 ) */
	#if ( USE_IPERF != 0 )
		else if( strncmp( pcBuffer, "iperf", 5 ) == 0 )
		{
			vIPerfInstall();
		}
	#endif

	else if( strncmp( pcBuffer, "printf", 6 ) == 0 )
	{
		size_t uxLength = 10;
		char pcBuffer[ uxLength ];
		BaseType_t xIndex;

		for( xIndex = 0; xIndex < 20; xIndex++ )
		{
			memset( pcBuffer, 'A', uxLength );
			FreeRTOS_strerror_r( xIndex, pcBuffer, uxLength );
			FreeRTOS_printf( ( "Errno %ld = '%s'\n", xIndex, pcBuffer ) );
		}
	}
	else if( strncmp( pcBuffer, "printf", 6 ) == 0 )
	{
		extern void printf_test( void );
		printf_test();
	}

	#if ( USE_NTOP_TEST != 0 )
		else if( strncmp( pcBuffer, "ntop", 4 ) == 0 )
		{
			FreeRTOS_printf( ( "Starting ntop test\n" ) );
			inet_pton_ntop_tests();
		}
	#endif

	else if( strncmp( pcBuffer, "errno", 4 ) == 0 )
	{
		char * ptr = pcBuffer + 4;

		while( *ptr && !isdigit( *ptr ) )
		{
			ptr++;
		}

		if( *ptr )
		{
			char buffer[ 17 ];
			int errnum;

			sscanf( ptr, "%d", &errnum );
			FreeRTOS_printf( ( "errno %d = '%s'\n",
							   errnum, FreeRTOS_strerror_r( errnum, buffer, sizeof buffer ) ) );
		}
	}

	else if( strncmp( pcBuffer, "udprecv", 7 ) == 0 )
	{
		const uint8_t * puc = pxDescriptor->pucEthernetBuffer;
		UDPPacket_t * pxPacket = ( UDPPacket_t * ) puc;
		FreeRTOS_printf( ( "Checksum %04x\n", FreeRTOS_ntohs( pxPacket->xUDPHeader.usChecksum ) ) );
	}

	else if( strncmp( pcBuffer, "udpsend", 7 ) == 0 )
	{
		if( xUDPServerSocket == NULL )
		{
			xUDPServerSocket = FreeRTOS_socket( FREERTOS_AF_INET, FREERTOS_SOCK_DGRAM, FREERTOS_IPPROTO_UDP );

			if( xUDPServerSocket == NULL )
			{
				xUDPServerSocket = ( Socket_t ) FREERTOS_INVALID_SOCKET;
			}

			if( xUDPServerSocket == ( Socket_t ) FREERTOS_INVALID_SOCKET )
			{
				FreeRTOS_printf( ( " Creation of UDP socket failed\n" ) );
			}
		}

		if( xUDPServerSocket != ( Socket_t ) FREERTOS_INVALID_SOCKET )
		{
			BaseType_t useChecksum = pdTRUE;
			char * ptr = pcBuffer + 7;

			while( isspace( ( int ) *ptr ) )
			{
				ptr++;
			}

			if( *ptr == '0' )
			{
				useChecksum = pdFALSE;
			}

			setUDPCheckSum( xUDPServerSocket, useChecksum );
			char pcMessage[ 80 ];
			static BaseType_t xMustDelay = pdTRUE;
			static int xCounter;
			int rc = snprintf( pcMessage, sizeof pcMessage, "TESTUDP Hello world %d\n", xCounter );
			struct freertos_sockaddr xAddress;
			xAddress.sin_len = sizeof xAddress; /* length of this structure. */
			#if ( ipconfigUSE_IPv6 != 0 )
				{
					xAddress.sin_family = FREERTOS_AF_INET6;
					FreeRTOS_inet_pton6( "fe80::6816:5e9b:80a0:9edb", xAddress.sin_address.xIP_IPv6.ucBytes );
				}
			#else
				{
					xAddress.sin_family = FREERTOS_AF_INET;
					xAddress.sin_addr = FreeRTOS_inet_addr_quick( 192, 168, 2, 255 );
				}
			#endif

			xAddress.sin_port = FreeRTOS_htons( configUDP_LOGGING_PORT_REMOTE );
			FreeRTOS_sendto( xUDPServerSocket, pcMessage, rc, 0, &xAddress, sizeof xAddress );

			if( xMustDelay != 0 )
			{
				vTaskDelay( pdMS_TO_TICKS( 20U ) );
				xMustDelay = pdFALSE;
			}

			xAddress.sin_port = FreeRTOS_htons( configUDP_LOGGING_PORT_LOCAL );
			FreeRTOS_sendto( xUDPServerSocket, pcMessage, rc, 0, &xAddress, sizeof xAddress );

			FreeRTOS_printf( ( "Sent message %d useChecksum %d\n", ( int ) xCounter, ( int ) useChecksum ) );
			xCounter++;
		}
	}

	else if( strncmp( pcBuffer, "udp", 3 ) == 0 )
	{
		const char pcMessage[] = "1234567890";
		const char * ptr = pcBuffer + 3;

		while( *ptr && !isspace( ( int ) *ptr ) )
		{
			ptr++;
		}

		while( isspace( ( int ) *ptr ) )
		{
			ptr++;
		}

		if( *ptr )
		{
		}
		else
		{
			ptr = pcMessage;
		}

		#if ( ipconfigUSE_IPv6 != 0 )
			{
				int index;
				const char * ip_address[] =
				{
					/*	"fe80::9355:69c7:585a:afe7",	/ * raspberry ff02::1:ff5a:afe7, 33:33:ff:5a:af:e7 * / */
					"fe80::6816:5e9b:80a0:9edb", /* laptop Hein */
				};

				for( index = 0; index < ARRAY_SIZE( ip_address ); index++ )
				{
					struct freertos_sockaddr xAddress;
					memset( &xAddress, '\0', sizeof xAddress );
					xAddress.sin_len = sizeof( xAddress ); /* length of this structure. */
					xAddress.sin_family = FREERTOS_AF_INET6;
					xAddress.sin_port = FreeRTOS_htons( configUDP_LOGGING_PORT_REMOTE );
					xAddress.sin_flowinfo = 0; /* IPv6 flow information. */
					FreeRTOS_inet_pton6( ip_address[ index ], xAddress.sin_address.xIP_IPv6.ucBytes );
					FreeRTOS_sendto( xSocket, ptr, strlen( ptr ), 0, ( const struct freertos_sockaddr * ) &( xAddress ), sizeof( xAddress ) );
				}
			}
		#endif /* if ( ipconfigUSE_IPv6 != 0 ) */

/*
 *      {
 *          struct freertos_sockaddr xAddress_IPv4;
 *          xAddress_IPv4.sin_family = FREERTOS_AF_INET4;
 *          xAddress_IPv4.sin_len = sizeof( xAddress_IPv4 );		// length of this structure
 *          xAddress_IPv4.sin_port = FreeRTOS_htons( configUDP_LOGGING_PORT_REMOTE );
 *          xAddress_IPv4.sin_addr = FreeRTOS_inet_addr_quick( 192, 168, 2, 5 );
 *
 *          FreeRTOS_sendto( xSocket, ptr, strlen( ptr ), 0, &( xAddress_IPv4 ), sizeof( xAddress_IPv4 ) );
 *      }
 */
	}

	#if ( ipconfigUSE_IPv6 != 0 )
		else if( strncmp( pcBuffer, "address", 7 ) == 0 )
		{
			IPv6_Address_t xPrefix, xIPAddress;
			xPrefix.ucBytes[ 0 ] = 0xfe;
			xPrefix.ucBytes[ 1 ] = 0x80;
			memset( xPrefix.ucBytes + 2, '\0', sizeof( xPrefix.ucBytes ) - 2 );
			FreeRTOS_CreateIPv6Address( &xIPAddress, &xPrefix, 64, pdTRUE );
			FreeRTOS_printf( ( "IP address = %pip\n", xIPAddress.ucBytes ) );

			/* 2001:470:ec54 */
			/* 2001:0470:ec54 */
			xPrefix.ucBytes[ 0 ] = 0x20;
			xPrefix.ucBytes[ 1 ] = 0x01;
			xPrefix.ucBytes[ 2 ] = 0x04;
			xPrefix.ucBytes[ 3 ] = 0x70;
			xPrefix.ucBytes[ 4 ] = 0xec;
			xPrefix.ucBytes[ 5 ] = 0x54;
			xPrefix.ucBytes[ 6 ] = 0x00;
			xPrefix.ucBytes[ 7 ] = 0x00;
			memset( xPrefix.ucBytes + 8, '\0', sizeof( xPrefix.ucBytes ) - 8 );
			FreeRTOS_CreateIPv6Address( &xIPAddress, &xPrefix, 64, pdTRUE );
			FreeRTOS_printf( ( "IP address = %pip\n", xIPAddress.ucBytes ) );
		}
	#endif /* if ( ipconfigUSE_IPv6 != 0 ) */

	#if ( ipconfigUSE_IPv6 != 0 )
		else if( strncmp( pcBuffer, "test2", 5 ) == 0 )
		{
			IPv6_Address_t xLeft, xRight;
			struct SPair
			{
				const char * pcLeft;
				const char * pcRight;
				int prefixLength;
			};
			struct SPair pairs[] =
			{
				{ "2001:470:ec54::", "2001:470:ec54::", 64 },
				{ "fe80::",          "fe80::",          10 },
				{ "fe80::",          "feBF::",          10 },
				{ "fe80::",          "feff::",          10 },
			};
			int index;

			for( index = 0; index < ARRAY_SIZE( pairs ); index++ )
			{
				BaseType_t xResult;

				FreeRTOS_inet_pton6( pairs[ index ].pcLeft, xLeft.ucBytes );
				FreeRTOS_inet_pton6( pairs[ index ].pcRight, xRight.ucBytes );
				xResult = xCompareIPv6_Address( &xLeft, &xRight, 10 );
				FreeRTOS_printf( ( "Compare %pip with %pip = %s\n", xLeft.ucBytes, xRight.ucBytes, xResult ? "differ" : "same" ) );
			}
		}
	#endif /* ( ipconfigUSE_IPv6 != 0 ) */

	else if( strncmp( pcBuffer, "crctest", 7 ) == 0 )
	{
		crc_test();
	}

/*	else if( memcmp( pcBuffer, "tcpconfig", 9 ) == 0 ) */
/*	{ */
/*		int lTxBufSize; */
/*		int lTxWinSize; */
/*		int lRxBufSize; */
/*		int lRxWinSize; */
/*		int lZeroCopy = 1; */
/*		int lContentsCheck = 0; */
/*		int rc = sscanf( pcBuffer+9, "%d%d%d%d%d%d", */
/*			&( lTxBufSize ), */
/*			&( lTxWinSize ), */
/*			&( lRxBufSize ), */
/*			&( lRxWinSize ), */
/*			&( lZeroCopy  ), */
/*			&( lContentsCheck ) ); */
/* */
/*		if( rc >= 4 ) */
/*		{ */
/*			extern char useZeroCopy, useContentsCheck; */
/*			lTxBufSize *= ipconfigTCP_MSS; */
/*			lRxBufSize *= ipconfigTCP_MSS; */
/*			WinProperties_t xWinProps = { */
/*				.lTxBufSize = lTxBufSize, / **< Unit: bytes * / */
/*				.lTxWinSize = lTxWinSize, / **< Unit: MSS * / */
/*				.lRxBufSize = lRxBufSize, / **< Unit: bytes * / */
/*				.lRxWinSize = lRxWinSize  / **< Unit: MSS * / */
/*			}; */
/*			FreeRTOS_printf( ( "lTxBufSize = %d (%d)\n", ( int ) xWinProps.lTxBufSize, ( int ) ( xWinProps.lTxBufSize / ipconfigTCP_MSS ) ) ); */
/*			FreeRTOS_printf( ( "lTxWinSize = %d\n", ( int ) xWinProps.lTxWinSize ) ); */
/*			FreeRTOS_printf( ( "lRxBufSize = %d (%d)\n", ( int ) xWinProps.lRxBufSize, ( int ) ( xWinProps.lRxBufSize / ipconfigTCP_MSS ) ) ); */
/*			FreeRTOS_printf( ( "lRxWinSize = %d\n", ( int ) xWinProps.lRxWinSize ) ); */
/*			FreeRTOS_printf( ( "ZeroCopy   = %d\n", ( int ) lZeroCopy ) ); */
/*			FreeRTOS_printf( ( "CheckData  = %d\n", ( int ) lContentsCheck ) ); */
/* */
/*			if( rc >= 5 ) */
/*			{ */
/*				useZeroCopy = lZeroCopy; */
/*			} */
/*			if( rc >= 6 ) */
/*			{ */
/*				useContentsCheck = lContentsCheck; */
/*			} */
/*			tcp_set_props( &( xWinProps ) ); */
/*		} */
/*		else */
/*		{ */
/*			FreeRTOS_printf( ( "Usage: \"tcpconfig <TxBuf> <TxWin> <RxBuf> <RxWin>\n" ) ); */
/*		} */
/*		xTCPWindowLoggingLevel = 1; */
/*	} */
	else if( memcmp( pcBuffer, "mem", 3 ) == 0 )
	{
		uint32_t now = xPortGetFreeHeapSize();
		uint32_t total = 0; /*xPortGetOrigHeapSize( ); */
		uint32_t perc = total ? ( ( 100 * now ) / total ) : 100;
		lUDPLoggingPrintf( "mem Low %u, Current %lu / %lu (%lu perc free)\n",
						   xPortGetMinimumEverFreeHeapSize(),
						   now, total, perc );
	}

	else if( strncmp( pcBuffer, "netstat", 7 ) == 0 )
	{
		FreeRTOS_netstat();
	}

	#if ( USE_LOG_EVENT != 0 )
		else if( strncmp( pcBuffer, "event", 4 ) == 0 )
		{
			if( pcBuffer[ 5 ] == 'c' )
			{
				int rc = iEventLogClear();
				lUDPLoggingPrintf( "cleared %d events\n", rc );
			}
			else
			{
				eventLogDump();
			}
		}
	#endif /* USE_LOG_EVENT */

	else if( strncmp( pcBuffer, "list", 4 ) == 0 )
	{
		vShowTaskTable( pcBuffer[ 4 ] == 'c' );
	}

	#if ( mainHAS_RAMDISK != 0 )
		else if( strncmp( pcBuffer, "dump", 4 ) == 0 )
		{
			FF_RAMDiskStoreImage( pxRAMDisk, "ram_disk.img" );
		}
	#endif /* mainHAS_RAMDISK */

	#if ( USE_SIMPLE_TCP_SERVER != 0 )
		else if( strncmp( pcBuffer, "simple", 6 ) == 0 )
		{
			vStartSimpleTCPServerTasks( 640, 2 );
		}
	#endif /* USE_SIMPLE_TCP_SERVER */

	#if ( USE_PLUS_FAT != 0 )
		else if( strncmp( pcBuffer, "format", 6 ) == 0 )
		{
			if( pxSDDisk != 0 )
			{
				FF_SDDiskFormat( pxSDDisk, 0 );
			}
		}
	#endif

	#if ( mainHAS_FAT_TEST != 0 )
		else if( strncmp( pcBuffer, "fattest", 7 ) == 0 )
		{
			int level = 0;
			const char pcMountPath[] = "/test";

			if( sscanf( pcBuffer + 7, "%d", &level ) == 1 )
			{
				#if USE_TCP_DEMO_CLI
					verboseLevel = level;
				#endif
			}

			FreeRTOS_printf( ( "FAT test %d\n", level ) );
			ff_mkdir( pcMountPath MKDIR_FALSE );

			if( ( level != 0 ) && ( iFATRunning == 0 ) )
			{
				int x;

				iFATRunning = 1;

				if( level < 1 )
				{
					level = 1;
				}

				if( level > 10 )
				{
					level = 10;
				}

				for( x = 0; x < level; x++ )
				{
					char pcName[ 16 ];
					static char pcBaseDirectoryNames[ 10 ][ 16 ];
					sprintf( pcName, "FAT_%02d", x + 1 );
					snprintf( pcBaseDirectoryNames[ x ], sizeof pcBaseDirectoryNames[ x ], "%s/%d",
							  pcMountPath, x + 1 );
					ff_mkdir( pcBaseDirectoryNames[ x ] MKDIR_FALSE );

	#define mainFAT_TEST__TASK_PRIORITY    ( tskIDLE_PRIORITY + 1 )
					xTaskCreate( prvFileSystemAccessTask,
								 pcName,
								 mainFAT_TEST_STACK_SIZE,
								 ( void * ) pcBaseDirectoryNames[ x ],
								 mainFAT_TEST__TASK_PRIORITY,
								 NULL );
				}
			}
			else if( ( level == 0 ) && ( iFATRunning != 0 ) )
			{
				iFATRunning = 0;
				FreeRTOS_printf( ( "FAT test stopped\n" ) );
			}
		} /* if( strncmp( pcBuffer, "fattest", 7 ) == 0 ) */
	#endif /* mainHAS_FAT_TEST */
	else
	{
		xReturn = pdFALSE;
	}

	return xReturn;
} /* if( xCount > 0 ) */

uint32_t echoServerIPAddress()
{
	return ulServerIPAddress;
}
/*-----------------------------------------------------------*/

uint32_t HAL_GetTick()
{
	if( xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED )
	{
		static uint32_t ulCounter;
		ulCounter++;
		return ulCounter / 10;
	}
	else
	{
		return ( uint32_t ) xTaskGetTickCount();
	}
}

void vApplicationIdleHook( void )
{
}
/*-----------------------------------------------------------*/

volatile const char * pcAssertedFileName;
volatile int iAssertedErrno;
volatile uint32_t ulAssertedLine;
#if ( USE_PLUS_FAT != 0 )
	volatile FF_Error_t xAssertedFF_Error;
#endif
void vAssertCalled( const char * pcFile,
					uint32_t ulLine )
{
	volatile uint32_t ulBlockVariable = 0UL;

	ulAssertedLine = ulLine;
	#if ( USE_PLUS_FAT != 0 )
		iAssertedErrno = stdioGET_ERRNO();
		xAssertedFF_Error = stdioGET_FF_ERROR();
	#endif
	pcAssertedFileName = strrchr( pcFile, '/' );

	if( pcAssertedFileName == 0 )
	{
		pcAssertedFileName = strrchr( pcFile, '\\' );
	}

	if( pcAssertedFileName != NULL )
	{
		pcAssertedFileName++;
	}
	else
	{
		pcAssertedFileName = pcFile;
	}

	FreeRTOS_printf( ( "vAssertCalled( %s, %ld\n", pcFile, ulLine ) );

	/* Setting ulBlockVariable to a non-zero value in the debugger will allow
	 * this function to be exited. */
	taskDISABLE_INTERRUPTS();
	{
		while( ulBlockVariable == 0UL )
		{
			__asm volatile ( "NOP" );
		}
	}
	taskENABLE_INTERRUPTS();
}
/*-----------------------------------------------------------*/


/** System Clock Configuration
 */
void SystemClock_Config( void )
{
	RCC_OscInitTypeDef RCC_OscInitStruct;
	RCC_ClkInitTypeDef RCC_ClkInitStruct;
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;

	__PWR_CLK_ENABLE();

	__HAL_PWR_VOLTAGESCALING_CONFIG( PWR_REGULATOR_VOLTAGE_SCALE1 );

	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.LSIState = RCC_LSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 25;
	RCC_OscInitStruct.PLL.PLLN = 336;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 7;
	HAL_RCC_OscConfig( &RCC_OscInitStruct );

	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1
								  | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
	HAL_RCC_ClockConfig( &RCC_ClkInitStruct, FLASH_LATENCY_5 );

	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
	PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
	HAL_RCCEx_PeriphCLKConfig( &PeriphClkInitStruct );

	HAL_RCC_MCOConfig( RCC_MCO1, RCC_MCO1SOURCE_HSE, RCC_MCODIV_1 );
}
/*-----------------------------------------------------------*/

/* ADC3 init function */
#ifdef HAL_ADC_MODULE_ENABLED
	void MX_ADC3_Init( void )
	{
		ADC_ChannelConfTypeDef sConfig;

		/**Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
		 */
		hadc3.Instance = ADC3;
		hadc3.Init.ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV2;
		hadc3.Init.Resolution = ADC_RESOLUTION12b;
		hadc3.Init.ScanConvMode = DISABLE;
		hadc3.Init.ContinuousConvMode = DISABLE;
		hadc3.Init.DiscontinuousConvMode = DISABLE;
		hadc3.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
		hadc3.Init.DataAlign = ADC_DATAALIGN_RIGHT;
		hadc3.Init.NbrOfConversion = 1;
		hadc3.Init.DMAContinuousRequests = DISABLE;
		hadc3.Init.EOCSelection = EOC_SINGLE_CONV;
		HAL_ADC_Init( &hadc3 );

		/**Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
		 */
		sConfig.Channel = ADC_CHANNEL_7;
		sConfig.Rank = 1;
		sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
		HAL_ADC_ConfigChannel( &hadc3, &sConfig );
	}
/*-----------------------------------------------------------*/
#endif /* HAL_ADC_MODULE_ENABLED */

#ifdef HAL_DAC_MODULE_ENABLED
/* DAC init function */
	void MX_DAC_Init( void )
	{
		DAC_ChannelConfTypeDef sConfig;

		/**DAC Initialization
		 */
		hdac.Instance = DAC;
		HAL_DAC_Init( &hdac );

		/**DAC channel OUT1 config
		 */
		sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
		sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
		HAL_DAC_ConfigChannel( &hdac, &sConfig, DAC_CHANNEL_1 );
	}
/*-----------------------------------------------------------*/
#endif /* HAL_DAC_MODULE_ENABLED */

/* DCMI init function */
#ifdef HAL_DCMI_MODULE_ENABLED
	void MX_DCMI_Init( void )
	{
		hdcmi.Instance = DCMI;
		hdcmi.Init.SynchroMode = DCMI_SYNCHRO_HARDWARE;
		hdcmi.Init.PCKPolarity = DCMI_PCKPOLARITY_FALLING;
		hdcmi.Init.VSPolarity = DCMI_VSPOLARITY_LOW;
		hdcmi.Init.HSPolarity = DCMI_HSPOLARITY_LOW;
		hdcmi.Init.CaptureRate = DCMI_CR_ALL_FRAME;
		hdcmi.Init.ExtendedDataMode = DCMI_EXTEND_DATA_8B;
		hdcmi.Init.JPEGMode = DCMI_JPEG_DISABLE;
		HAL_DCMI_Init( &hdcmi );
	}
#endif /* ifdef HAL_DCMI_MODULE_ENABLED */
/*-----------------------------------------------------------*/

/* I2C1 init function */
#ifdef HAL_I2C_MODULE_ENABLED
	void MX_I2C1_Init( void )
	{
		hi2c1.Instance = I2C1;
		hi2c1.Init.ClockSpeed = 100000;
		hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
		hi2c1.Init.OwnAddress1 = 0;
		hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
		hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLED;
		hi2c1.Init.OwnAddress2 = 0;
		hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLED;
		hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLED;
		HAL_I2C_Init( &hi2c1 );
	}
/*-----------------------------------------------------------*/
#endif /* HAL_I2C_MODULE_ENABLED */

/* RTC init function */
#ifdef HAL_RTC_MODULE_ENABLED
	void MX_RTC_Init( void )
	{
		RTC_TimeTypeDef sTime;
		RTC_DateTypeDef sDate;
		RTC_AlarmTypeDef sAlarm;

		/**Initialize RTC and set the Time and Date
		 */
		hrtc.Instance = RTC;
		hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
		hrtc.Init.AsynchPrediv = 127;
		hrtc.Init.SynchPrediv = 255;
		hrtc.Init.OutPut = RTC_OUTPUT_ALARMA;
		hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
		hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
		HAL_RTC_Init( &hrtc );

		sTime.Hours = 0;
		sTime.Minutes = 0;
		sTime.Seconds = 0;
		sTime.SubSeconds = 0;
		sTime.TimeFormat = RTC_HOURFORMAT12_AM;
		sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
		sTime.StoreOperation = RTC_STOREOPERATION_RESET;
		HAL_RTC_SetTime( &hrtc, &sTime, FORMAT_BCD );

		sDate.WeekDay = RTC_WEEKDAY_MONDAY;
		sDate.Month = RTC_MONTH_JANUARY;
		sDate.Date = 1;
		sDate.Year = 0;
		HAL_RTC_SetDate( &hrtc, &sDate, FORMAT_BCD );

		/**Enable the Alarm A
		 */
		sAlarm.AlarmTime.Hours = 0;
		sAlarm.AlarmTime.Minutes = 0;
		sAlarm.AlarmTime.Seconds = 0;
		sAlarm.AlarmTime.SubSeconds = 0;
		sAlarm.AlarmTime.TimeFormat = RTC_HOURFORMAT12_AM;
		sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
		sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
		sAlarm.AlarmMask = RTC_ALARMMASK_NONE;
		sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
		sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
		sAlarm.AlarmDateWeekDay = 1;
		sAlarm.Alarm = RTC_ALARM_A;
		HAL_RTC_SetAlarm( &hrtc, &sAlarm, FORMAT_BCD );
	}
/*-----------------------------------------------------------*/
#endif /* HAL_RTC_MODULE_ENABLED */

/* USART3 init function */
#if ( USE_USART3_LOGGING )
	void MX_USART3_UART_Init( void )
	{
		huart3.Instance = USART3;
		huart3.Init.BaudRate = 115200;
		huart3.Init.WordLength = UART_WORDLENGTH_8B;
		huart3.Init.StopBits = UART_STOPBITS_1;
		huart3.Init.Parity = UART_PARITY_NONE;
		huart3.Init.Mode = UART_MODE_TX_RX;
		huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
		huart3.Init.OverSampling = UART_OVERSAMPLING_16;
		HAL_UART_Init( &huart3 );
	}
#endif /* if ( USE_USART3_LOGGING ) */
/*-----------------------------------------------------------*/

/* USART3 init function */
#if ( USE_USART6_LOGGING )
	void MX_USART6_UART_Init( void )
	{
		huart6.Instance = USART6;
		huart6.Init.BaudRate = 115200;
		huart6.Init.WordLength = UART_WORDLENGTH_8B;
		huart6.Init.StopBits = UART_STOPBITS_1;
		huart6.Init.Parity = UART_PARITY_NONE;
		huart6.Init.Mode = UART_MODE_TX_RX;
		huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
		huart6.Init.OverSampling = UART_OVERSAMPLING_16;
		HAL_UART_Init( &huart6 );
	}
#endif /* if ( USE_USART6_LOGGING ) */
/*-----------------------------------------------------------*/

/** Configure pins
 *  PE2   ------> SYS_TRACECLK
 *  PB5   ------> USB_OTG_HS_ULPI_D7
 *  PE5   ------> SYS_TRACED2
 *  PE6   ------> SYS_TRACED3
 *  PA12   ------> USB_OTG_FS_DP
 *  PI3   ------> I2S2_SD
 *  PA11   ------> USB_OTG_FS_DM
 *  PA10   ------> USB_OTG_FS_ID
 *  PI11   ------> USB_OTG_HS_ULPI_DIR
 *  PI0   ------> I2S2_WS
 *  PA9   ------> USB_OTG_FS_VBUS
 *  PA8   ------> RCC_MCO_1
 *  PH4   ------> USB_OTG_HS_ULPI_NXT
 *  PG7   ------> USART6_CK
 *  PC0   ------> USB_OTG_HS_ULPI_STP
 *  PA5   ------> USB_OTG_HS_ULPI_CK
 *  PB12   ------> USB_OTG_HS_ULPI_D5
 *  PB13   ------> USB_OTG_HS_ULPI_D6
 *  PA3   ------> USB_OTG_HS_ULPI_D0
 *  PB1   ------> USB_OTG_HS_ULPI_D2
 *  PB0   ------> USB_OTG_HS_ULPI_D1
 *  PB10   ------> USB_OTG_HS_ULPI_D3
 *  PB11   ------> USB_OTG_HS_ULPI_D4
 */
void MX_GPIO_Init( void )
{
	GPIO_InitTypeDef GPIO_InitStruct;

	/* GPIO Ports Clock Enable */
	__GPIOA_CLK_ENABLE();
	__GPIOB_CLK_ENABLE();
	__GPIOC_CLK_ENABLE();
	__GPIOD_CLK_ENABLE();
	__GPIOE_CLK_ENABLE();
	__GPIOF_CLK_ENABLE();
	__GPIOG_CLK_ENABLE();
	__GPIOH_CLK_ENABLE();
	__GPIOI_CLK_ENABLE();

	/*Configure GPIO pins : PE2 PE5 PE6 */
/*	GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_5|GPIO_PIN_6; */
/*	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; */
/*	GPIO_InitStruct.Pull = GPIO_NOPULL; */
/*	GPIO_InitStruct.Speed = GPIO_SPEED_LOW; */
/*	GPIO_InitStruct.Alternate = GPIO_AF0_TRACE; */
/*	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct); */

	/*Configure GPIO pins : PB5 PB12 PB13 PB1
	 *                     PB0 PB10 PB11 */
	GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_1
						  | GPIO_PIN_0 | GPIO_PIN_10 | GPIO_PIN_11;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
	HAL_GPIO_Init( GPIOB, &GPIO_InitStruct );

	/*Configure GPIO pin : PG15 */
	GPIO_InitStruct.Pin = GPIO_PIN_15;
	GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init( GPIOG, &GPIO_InitStruct );

	/*Configure GPIO pins : PG12 PG8 PG6 */
	GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_8 | GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init( GPIOG, &GPIO_InitStruct );

	/*Configure GPIO pins : PA12 PA11 PA10 */
	GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_11 | GPIO_PIN_10;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
	HAL_GPIO_Init( GPIOA, &GPIO_InitStruct );

	/*Configure GPIO pins : PI3 PI0 */
	GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_0;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
	HAL_GPIO_Init( GPIOI, &GPIO_InitStruct );

	/*Configure GPIO pin : PI2 */
	GPIO_InitStruct.Pin = GPIO_PIN_2;
	GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init( GPIOI, &GPIO_InitStruct );

	/*Configure GPIO pin : PI9 */
	GPIO_InitStruct.Pin = GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init( GPIOI, &GPIO_InitStruct );

	/*Configure GPIO pins : PH15 PH5 */
	GPIO_InitStruct.Pin = GPIO_PIN_15 | GPIO_PIN_5;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init( GPIOH, &GPIO_InitStruct );

	/*Configure GPIO pin : PI11 */
	GPIO_InitStruct.Pin = GPIO_PIN_11;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
	HAL_GPIO_Init( GPIOI, &GPIO_InitStruct );

	/*Configure GPIO pin : PH13 */
	GPIO_InitStruct.Pin = GPIO_PIN_13;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init( GPIOH, &GPIO_InitStruct );

	/*Configure GPIO pin : PA9 */
	GPIO_InitStruct.Pin = GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init( GPIOA, &GPIO_InitStruct );

	/*Configure GPIO pin : PA8 */
	GPIO_InitStruct.Pin = GPIO_PIN_8;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
	HAL_GPIO_Init( GPIOA, &GPIO_InitStruct );

	/*Configure GPIO pin : PC7 */
	GPIO_InitStruct.Pin = GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init( GPIOC, &GPIO_InitStruct );

	/*Configure GPIO pin : PH4 */
	GPIO_InitStruct.Pin = GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
	HAL_GPIO_Init( GPIOH, &GPIO_InitStruct );

	/*Configure GPIO pin : PG7 */
	GPIO_InitStruct.Pin = GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
	HAL_GPIO_Init( GPIOG, &GPIO_InitStruct );

	/*Configure GPIO pin : PF7 */
	GPIO_InitStruct.Pin = GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init( GPIOF, &GPIO_InitStruct );

	/*Configure GPIO pin : PF6 */
	GPIO_InitStruct.Pin = GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init( GPIOF, &GPIO_InitStruct );

	/*Configure GPIO pin : PC0 */
	GPIO_InitStruct.Pin = GPIO_PIN_0;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
	HAL_GPIO_Init( GPIOC, &GPIO_InitStruct );

	/*Configure GPIO pin : PB2 */
	GPIO_InitStruct.Pin = GPIO_PIN_2;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init( GPIOB, &GPIO_InitStruct );

	/*Configure GPIO pins : PA5 PA3 */
	GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_3;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
	HAL_GPIO_Init( GPIOA, &GPIO_InitStruct );

	/*Configure GPIO pin : PF11 */
	GPIO_InitStruct.Pin = GPIO_PIN_11;
	GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init( GPIOF, &GPIO_InitStruct );

	/*Configure GPIO pin : PB14 */
	GPIO_InitStruct.Pin = GPIO_PIN_14;
	GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init( GPIOB, &GPIO_InitStruct );
}
/*-----------------------------------------------------------*/

/* The following hook-function will be called from "STM32F4xx\ff_sddisk.c"
 * in case the GPIO contact of the card-detect has changed. */

void vApplicationCardDetectChangeHookFromISR( BaseType_t * pxHigherPriorityTaskWoken );

void vApplicationCardDetectChangeHookFromISR( BaseType_t * pxHigherPriorityTaskWoken )
{
	/* This routine will be called on every change of the Card-detect
	 * GPIO pin.  The TCP server is probably waiting for in event in a
	 * select() statement.  Wake it up.*/
	#if ( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
		if( pxTCPServer != NULL )
		{
			FreeRTOS_TCPServerSignalFromISR( pxTCPServer, pxHigherPriorityTaskWoken );
		}
	#else
		{
			( void ) pxHigherPriorityTaskWoken;
		}
	#endif
}
/*-----------------------------------------------------------*/

/* FSMC initialization function */
static void MX_FSMC_Init( void )
{
	FSMC_NORSRAM_TimingTypeDef Timing;

	#ifdef HAL_NOR_MODULE_ENABLED

		/** Perform the NOR1 memory initialization sequence
		 */
		hnor1.Instance = FSMC_NORSRAM_DEVICE;
		hnor1.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;
		/* hnor1.Init */
		hnor1.Init.NSBank = FSMC_NORSRAM_BANK1;
		hnor1.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
		hnor1.Init.MemoryType = FSMC_MEMORY_TYPE_NOR;
		hnor1.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16;
		hnor1.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;
		hnor1.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
		hnor1.Init.WrapMode = FSMC_WRAP_MODE_DISABLE;
		hnor1.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
		hnor1.Init.WriteOperation = FSMC_WRITE_OPERATION_DISABLE;
		hnor1.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
		hnor1.Init.ExtendedMode = FSMC_EXTENDED_MODE_DISABLE;
		hnor1.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
		hnor1.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;

		/* Timing */
		Timing.AddressSetupTime = 15;
		Timing.AddressHoldTime = 15;
		Timing.DataSetupTime = 255;
		Timing.BusTurnAroundDuration = 15;
		Timing.CLKDivision = 16;
		Timing.DataLatency = 17;
		Timing.AccessMode = FSMC_ACCESS_MODE_A;
		/* ExtTiming */

		HAL_NOR_Init( &hnor1, &Timing, NULL );
	#endif /* HAL_NOR_MODULE_ENABLED */

	/** Perform the SRAM2 memory initialization sequence
	 */
	hsram2.Instance = FSMC_NORSRAM_DEVICE;
	hsram2.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;
	/* hsram2.Init */
	hsram2.Init.NSBank = FSMC_NORSRAM_BANK2;
	hsram2.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
	hsram2.Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
	hsram2.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16;
	hsram2.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;
	hsram2.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
	hsram2.Init.WrapMode = FSMC_WRAP_MODE_DISABLE;
	hsram2.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
	hsram2.Init.WriteOperation = FSMC_WRITE_OPERATION_DISABLE;
	hsram2.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
	hsram2.Init.ExtendedMode = FSMC_EXTENDED_MODE_DISABLE;
	hsram2.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
	hsram2.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;
	/* Timing */
	Timing.AddressSetupTime = 15;
	Timing.AddressHoldTime = 15;
	Timing.DataSetupTime = 255;
	Timing.BusTurnAroundDuration = 15;
	Timing.CLKDivision = 16;
	Timing.DataLatency = 17;
	Timing.AccessMode = FSMC_ACCESS_MODE_A;
	/* ExtTiming */

	HAL_SRAM_Init( &hsram2, &Timing, NULL );

	/** Perform the SRAM3 memory initialization sequence
	 */
	hsram3.Instance = FSMC_NORSRAM_DEVICE;
	hsram3.Extended = FSMC_NORSRAM_EXTENDED_DEVICE;
	/* hsram3.Init */
	hsram3.Init.NSBank = FSMC_NORSRAM_BANK3;
	hsram3.Init.DataAddressMux = FSMC_DATA_ADDRESS_MUX_DISABLE;
	hsram3.Init.MemoryType = FSMC_MEMORY_TYPE_SRAM;
	hsram3.Init.MemoryDataWidth = FSMC_NORSRAM_MEM_BUS_WIDTH_16;
	hsram3.Init.BurstAccessMode = FSMC_BURST_ACCESS_MODE_DISABLE;
	hsram3.Init.WaitSignalPolarity = FSMC_WAIT_SIGNAL_POLARITY_LOW;
	hsram3.Init.WrapMode = FSMC_WRAP_MODE_DISABLE;
	hsram3.Init.WaitSignalActive = FSMC_WAIT_TIMING_BEFORE_WS;
	hsram3.Init.WriteOperation = FSMC_WRITE_OPERATION_DISABLE;
	hsram3.Init.WaitSignal = FSMC_WAIT_SIGNAL_DISABLE;
	hsram3.Init.ExtendedMode = FSMC_EXTENDED_MODE_DISABLE;
	hsram3.Init.AsynchronousWait = FSMC_ASYNCHRONOUS_WAIT_DISABLE;
	hsram3.Init.WriteBurst = FSMC_WRITE_BURST_DISABLE;
	/* Timing */
	Timing.AddressSetupTime = 15;
	Timing.AddressHoldTime = 15;
	Timing.DataSetupTime = 255;
	Timing.BusTurnAroundDuration = 15;
	Timing.CLKDivision = 16;
	Timing.DataLatency = 17;
	Timing.AccessMode = FSMC_ACCESS_MODE_A;
	/* ExtTiming */

	HAL_SRAM_Init( &hsram3, &Timing, NULL );
}
/*-----------------------------------------------------------*/

#ifdef USE_FULL_ASSERT

	/**
	 * @brief Reports the name of the source file and the source line number
	 * where the assert_param error has occurred.
	 * @param file: pointer to the source file name
	 * @param line: assert_param error line source number
	 * @retval None
	 */

	volatile const char * assert_fname = NULL;
	volatile int assert_line = 0;
	volatile int assert_may_proceed = 0;

	void assert_failed( uint8_t * pucFilename,
						uint32_t ulLineNr )
	{
		assert_fname = ( const char * ) pucFilename;
		assert_line = ( int ) ulLineNr;
		taskDISABLE_INTERRUPTS();

		while( assert_may_proceed == pdFALSE )
		{
		}

		taskENABLE_INTERRUPTS();
	}
#endif /* ifdef USE_FULL_ASSERT */
/*-----------------------------------------------------------*/

#if ( USE_TCP_DEMO_CLI == 0 )
	void showEndPoint( NetworkEndPoint_t * pxEndPoint )
	{
		FreeRTOS_printf( ( "USE_TCP_DEMO_CLI is not defined\n" ) );
	}
	void vApplicationPingReplyHook( ePingReplyStatus_t eStatus,
									uint16_t usIdentifier )
	{
	}
#endif

/* Called by FreeRTOS+TCP when the network connects or disconnects.  Disconnect
 * events are only received if implemented in the MAC driver. */
#if ( ipconfigMULTI_INTERFACE != 0 ) && ( ipconfigCOMPATIBLE_WITH_SINGLE == 0 )
	void vApplicationIPNetworkEventHook_Multi( eIPCallbackEvent_t eNetworkEvent,
										 NetworkEndPoint_t * pxEndPoint )
#else
	void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent )
#endif
{
	static BaseType_t xTasksAlreadyCreated = pdFALSE;

	/* If the network has just come up...*/
	if( eNetworkEvent == eNetworkUp )
	{
		uint32_t ulGatewayAddress = 0U;
		#if ( ipconfigMULTI_INTERFACE != 0 ) && ( ipconfigCOMPATIBLE_WITH_SINGLE == 0 )
			{
				#if ( ipconfigUSE_IPv6 != 0 )
					if( pxEndPoint->bits.bIPv6 == pdFALSE )
				#endif
				{
					ulGatewayAddress = pxEndPoint->ipv4_settings.ulGatewayAddress;
				}
			}
		#else
			{
				FreeRTOS_GetAddressConfiguration( NULL, NULL, &ulGatewayAddress, NULL );
			}
		#endif /* if ( ipconfigMULTI_INTERFACE != 0 ) && ( ipconfigCOMPATIBLE_WITH_SINGLE == 0 ) */

		/* Create the tasks that use the IP stack if they have not already been
		 * created. */
		if( ( ulGatewayAddress != 0UL ) && ( xTasksAlreadyCreated == pdFALSE ) )
		{
			xTasksAlreadyCreated = pdTRUE;

			/* Let the server work task 'prvServerWorkTask' now it can now create the servers. */
			xTaskNotifyGive( xServerWorkTaskHandle );

			xDoStartupTasks = pdTRUE;

			#if ( USE_PLUS_FAT != 0 )
				doMountCard = 1;
			#endif
		}

		#if ( ipconfigMULTI_INTERFACE == 0 ) || ( ipconfigCOMPATIBLE_WITH_SINGLE != 0 )
			{
				uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
				char pcBuffer[ 16 ];

				/* Print out the network configuration, which may have come from a DHCP
				 * server. */
				FreeRTOS_GetAddressConfiguration( &ulIPAddress, &ulNetMask, &ulGatewayAddress, &ulDNSServerAddress );
				FreeRTOS_inet_ntoa( ulIPAddress, pcBuffer );
				FreeRTOS_printf( ( "IP Address: %s\n", pcBuffer ) );

				FreeRTOS_inet_ntoa( ulNetMask, pcBuffer );
				FreeRTOS_printf( ( "Subnet Mask: %s\n", pcBuffer ) );

				FreeRTOS_inet_ntoa( ulGatewayAddress, pcBuffer );
				FreeRTOS_printf( ( "Gateway Address: %s\n", pcBuffer ) );

				FreeRTOS_inet_ntoa( ulDNSServerAddress, pcBuffer );
				FreeRTOS_printf( ( "DNS Server Address: %s\n", pcBuffer ) );
			}
		#else /* if ( ipconfigMULTI_INTERFACE == 0 ) || ( ipconfigCOMPATIBLE_WITH_SINGLE != 0 ) */
			{
				/* Print out the network configuration, which may have come from a DHCP
				 * server. */
				showEndPoint( pxEndPoint );
			}
		#endif /* ipconfigMULTI_INTERFACE */
		{
			static BaseType_t xHasStarted = pdFALSE;

			if( xHasStarted == pdFALSE )
			{
				xHasStarted = pdTRUE;
				xTaskCreate( vUDPTest, "vUDPTest", mainUDP_SERVER_STACK_SIZE, NULL, mainUDP_SERVER_TASK_PRIORITY, NULL );
			}
		}
	}
}
/*-----------------------------------------------------------*/

#if ( configUSE_MALLOC_FAILED_HOOK != 1 )
	#error The function below will never be called.
#endif

void vApplicationMallocFailedHook( void )
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	 * free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	 * internally by FreeRTOS API functions that create tasks, queues, software
	 * timers, and semaphores.  The size of the FreeRTOS heap is set by the
	 * configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

	/* Force an assert. */
	configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask,
									char * pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	 * configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	 * function is called if a stack overflow is detected. */

	/* Force an assert. */
	configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/

static UBaseType_t uxRand16( void )
{
	const uint32_t ulMultiplier = 0x015a4e35UL, ulIncrement = 1UL;

	/* Utility function to generate a pseudo random number. */

	ulNextRand = ( ulMultiplier * ulNextRand ) + ulIncrement;
	return( ( int ) ( ulNextRand >> 16UL ) & 0x7fffuL );
}
/*-----------------------------------------------------------*/

UBaseType_t uxRand( void )
{
	uint32_t ulResult;

	if( xRandom32( &ulResult ) == pdFAIL )
	{
		do
		{
			UBaseType_t v1 = uxRand16() & 0x7fffuL;
			UBaseType_t v2 = uxRand16() & 0x7fffuL;
			UBaseType_t v3 = uxRand16() & 0x03;
			ulResult = v1 | ( v2 << 15 ) | ( v3 << 30 );
		} while( ulResult == 0uL );
	}

	return ulResult;
}
/*-----------------------------------------------------------*/

static void prvSRand( UBaseType_t ulSeed )
{
	/* Utility function to seed the pseudo random number generator. */
	ulNextRand = ulSeed;
}
/*-----------------------------------------------------------*/

#if ( ipconfigMULTI_INTERFACE != 0 )
	extern BaseType_t xApplicationDNSQueryHook_Multi( NetworkEndPoint_t * pxEndPoint,
                                                      const char * pcName );
#else
	extern BaseType_t xApplicationDNSQueryHook( const char * pcName );
#endif

/* These are volatile to try and prevent the compiler/linker optimising them
 * away as the variables never actually get used.  If the debugger won't show the
 * values of the variables, make them global my moving their declaration outside
 * of this function. */
volatile uint32_t r0;
volatile uint32_t r1;
volatile uint32_t r2;
volatile uint32_t r3;
volatile uint32_t r12;
volatile uint32_t lr;  /* Link register. */
volatile uint32_t pc;  /* Program counter. */
volatile uint32_t psr; /* Program status register. */

struct xREGISTER_STACK
{
	uint32_t spare0[ 8 ];
	uint32_t r0;
	uint32_t r1;
	uint32_t r2;
	uint32_t r3;
	uint32_t r12;
	uint32_t lr;  /* Link register. */
	uint32_t pc;  /* Program counter. */
	uint32_t psr; /* Program status register. */
	uint32_t spare1[ 8 ];
};

volatile struct xREGISTER_STACK * pxRegisterStack = NULL;

void prvGetRegistersFromStack( uint32_t * pulFaultStackAddress )
{
	pxRegisterStack = ( struct xREGISTER_STACK * ) ( pulFaultStackAddress - ARRAY_SIZE( pxRegisterStack->spare0 ) );
	r0 = pulFaultStackAddress[ 0 ];
	r1 = pulFaultStackAddress[ 1 ];
	r2 = pulFaultStackAddress[ 2 ];
	r3 = pulFaultStackAddress[ 3 ];

	r12 = pulFaultStackAddress[ 4 ];
	lr = pulFaultStackAddress[ 5 ];
	pc = pulFaultStackAddress[ 6 ];
	psr = pulFaultStackAddress[ 7 ];

	/* When the following line is hit, the variables contain the register values. */
	for( ; ; )
	{
	}
}
/*-----------------------------------------------------------*/

/*void HardFault_Handler( void ) __attribute__( ( naked ) ); */
void HardFault_Handler( void )
{
	__asm volatile
	(
		" tst lr, #4                                                \n"
		" ite eq                                                    \n"
		" mrseq r0, msp                                             \n"
		" mrsne r0, psp                                             \n"
		" ldr r1, [r0, #24]                                         \n"
		" bl prvGetRegistersFromStack                               \n"
	);
}
/*-----------------------------------------------------------*/

#if defined( __IAR_SYSTEMS_ICC__ )
	uint8_t heapMemory[ 100000 ];

	#define HEAP_START    heapMemory[ 0 ]
	#define HEAP_END      heapMemory[ sizeof heapMemory ]
#else
	extern uint8_t __bss_end__, _estack, _Min_Stack_Size;
	#define HEAP_START    __bss_end__
	#define HEAP_END      _estack
#endif

volatile uint32_t ulHeapSize;
volatile uint8_t * pucHeapStart;

static void heapInit()
{
	uint32_t ulStackSize = ( uint32_t ) &( _Min_Stack_Size );

	pucHeapStart = ( uint8_t * ) ( ( ( ( uint32_t ) &HEAP_START ) + 7 ) & ~0x07ul );

	ulHeapSize = ( uint32_t ) ( &HEAP_END - &HEAP_START );
	ulHeapSize &= ~0x07ul;
	ulHeapSize -= ulStackSize;

	HeapRegion_t xHeapRegions[] =
	{
		{ ( unsigned char * ) pucHeapStart, ulHeapSize },
		{ NULL,                             0          }
	};

	vPortDefineHeapRegions( xHeapRegions );
}
/*-----------------------------------------------------------*/

/* In some cases, a library call will use malloc()/free(),
 * redefine them to use pvPortMalloc()/vPortFree(). */

void * malloc( size_t size )
{
	return pvPortMalloc( size );
}
/*-----------------------------------------------------------*/

void free( void * ptr )
{
	vPortFree( ptr );
}
/*-----------------------------------------------------------*/

extern void vOutputChar( const char cChar,
						 const TickType_t xTicksToWait );

void vOutputChar( const char cChar,
				  const TickType_t xTicksToWait )
{
	( void ) cChar;
	( void ) xTicksToWait;
}
/*-----------------------------------------------------------*/

volatile int fail_count;
BaseType_t xApplicationMemoryPermissions( uint32_t aAddress )
{
/*
 * Return 1 for readable, 2 for writeable, 3 for both.
 * Function must be provided by the application.
 */
	BaseType_t xReturn = 0;

#define FLASH_CODE         0x08000000U
#define FLASH_CODE_SIZE    ( 1024U * 1024U )
#define RAM0               0x10000000U
#define RAM0_SIZE          ( 64U * 1024U )
#define RAM                0x20000000U
#define RAM_SIZE           ( 128U * 1024U )

	if( ( aAddress >= FLASH_CODE ) && ( aAddress < FLASH_CODE + FLASH_CODE_SIZE ) )
	{
		xReturn = 1;
	}
	else if( ( aAddress >= RAM ) && ( aAddress < RAM + RAM_SIZE ) )
	{
		xReturn = 3;
	}
	else if( ( aAddress >= RAM0 ) && ( aAddress < RAM0 + RAM0_SIZE ) )
	{
		xReturn = 3;
	}
	else
	{
		fail_count++;
	}

/*	FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 1024K */
/*	RAM0 (xrw)      : ORIGIN = 0x10000000, LENGTH = 64K */
/*	RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 128K */
/*	MEMORY_B1 (rx)  : ORIGIN = 0x60000000, LENGTH = 0K */
	return xReturn;
}
/*-----------------------------------------------------------*/

const char * pcApplicationHostnameHook( void )
{
	/* Assign the name "FreeRTOS" to this network node.  This function will be
	 * called during the DHCP: the machine will be registered with an IP address
	 * plus this name. */
	return mainHOST_NAME;
}
/*-----------------------------------------------------------*/

#if ( ipconfigMULTI_INTERFACE != 0 ) && ( ipconfigUSE_IPv6 != 0 ) && ( TESTING_PATCH == 0 )
	static BaseType_t setEndPoint( NetworkEndPoint_t * pxEndPoint )
	{
		NetworkEndPoint_t * px;
		BaseType_t xDone = pdFALSE;
		BaseType_t bDNS_IPv6 = ( pxEndPoint->usDNSType == dnsTYPE_AAAA_HOST ) ? 1 : 0;

		FreeRTOS_printf( ( "Wanted v%c got v%c\n", bDNS_IPv6 ? '6' : '4', pxEndPoint->bits.bIPv6 ? '6' : '4' ) );

		if( ( pxEndPoint->usDNSType == dnsTYPE_ANY_HOST ) ||
			( ( pxEndPoint->usDNSType == dnsTYPE_AAAA_HOST ) == ( pxEndPoint->bits.bIPv6 != 0U ) ) )
		{
			xDone = pdTRUE;
		}
		else
		{
			for( px = FreeRTOS_FirstEndPoint( pxEndPoint->pxNetworkInterface );
				 px != NULL;
				 px = FreeRTOS_NextEndPoint( pxEndPoint->pxNetworkInterface, px ) )
			{
				BaseType_t bIPv6 = ENDPOINT_IS_IPv6( px );

				if( bIPv6 == bDNS_IPv6 )
				{
					if( bIPv6 != 0 )
					{
						memcpy( pxEndPoint->ipv6_settings.xIPAddress.ucBytes, px->ipv6_settings.xIPAddress.ucBytes, ipSIZE_OF_IPv6_ADDRESS );
					}
					else
					{
						pxEndPoint->ipv4_settings.ulIPAddress = px->ipv4_settings.ulIPAddress;
					}

					pxEndPoint->bits.bIPv6 = bDNS_IPv6;
					xDone = pdTRUE;
					break;
				}
			}
		}

		if( pxEndPoint->bits.bIPv6 != 0 )
		{
			FreeRTOS_printf( ( "%s address %pip\n", xDone ? "Success" : "Failed", pxEndPoint->ipv6_settings.xIPAddress.ucBytes ) );
		}
		else
		{
			FreeRTOS_printf( ( "%s address %xip\n", xDone ? "Success" : "Failed", ( unsigned ) FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulIPAddress ) ) );
		}

		return xDone;
	}
#endif /* ( ipconfigMULTI_INTERFACE != 0 ) */

/*-----------------------------------------------------------*/

#if ( ipconfigMULTI_INTERFACE != 0 )
	BaseType_t xApplicationDNSQueryHook_Multi( NetworkEndPoint_t * pxEndPoint,
										 const char * pcName )
#else
	BaseType_t xApplicationDNSQueryHook( const char * pcName )
#endif
{
	BaseType_t xReturn;

	/* Determine if a name lookup is for this node.  Two names are given
	 * to this node: that returned by pcApplicationHostnameHook() and that set
	 * by mainDEVICE_NICK_NAME. */
	const char * serviceName = ( strstr( pcName, ".local" ) != NULL ) ? "mDNS" : "LLMNR";

	if( strncasecmp( pcName, "bong", 4 ) == 0 )
	{
		#if ( ipconfigUSE_IPv6 != 0 )
			int ip6Preferred = ( pcName[ 4 ] == '6' ) ? 6 : 4;
			#if ( TESTING_PATCH == 0 )
				xReturn = ( pxEndPoint->usDNSType != dnsTYPE_A_HOST ) == ( ip6Preferred == 6 );
			#else
				xReturn = ( pxEndPoint->bits.bIPv6 != 0 ) == ( ip6Preferred == 6 );
			#endif
		#else
			xReturn = pdPASS;
		#endif
	}
	else if( strcasecmp( pcName, pcApplicationHostnameHook() ) == 0 )
	{
		xReturn = pdPASS;
	}
	else if( strcasecmp( pcName, "stm.local" ) == 0 )
	{
		xReturn = pdPASS;
	}
	else if( strcasecmp( pcName, mainDEVICE_NICK_NAME ) == 0 )
	{
		xReturn = pdPASS;
	}
	else
	{
		xReturn = pdFAIL;
	}

	#if ( ipconfigMULTI_INTERFACE != 0 ) && ( ipconfigUSE_IPv6 != 0 ) && ( TESTING_PATCH == 0 )
		if( xReturn == pdTRUE )
		{
			xReturn = setEndPoint( pxEndPoint );
		}
	#endif
	{
		#if ( ipconfigMULTI_INTERFACE != 0 ) && ( ipconfigUSE_IPv6 != 0 )
			FreeRTOS_printf( ( "%s query '%s' = %d IPv%c\n", serviceName, pcName, ( int ) xReturn, pxEndPoint->bits.bIPv6 ? '6' : '4' ) );
		#else
			FreeRTOS_printf( ( "%s query '%s' = %d IPv4 only\n", serviceName, pcName, ( int ) xReturn ) );
		#endif
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

/*static uint32_t ulListTime; */
static uint64_t ullHiresTime;
uint32_t ulGetRunTimeCounterValue( void )
{
	return ( uint32_t ) ( ullGetHighResolutionTime() - ullHiresTime );
}
/*-----------------------------------------------------------*/

/* 'xTaskClearCounters' was defined in an earlier version of tasks.c */
/*extern*/ BaseType_t xTaskClearCounters;
void vShowTaskTable( BaseType_t aDoClear )
{
	TaskStatus_t * pxTaskStatusArray;
	volatile UBaseType_t uxArraySize, x;
	uint64_t ullTotalRunTime;
	uint32_t ulStatsAsPermille;
	uint32_t ulStackSize;

	/* Take a snapshot of the number of tasks in case it changes while this */
	/* function is executing. */
	uxArraySize = uxTaskGetNumberOfTasks();

	/* Allocate a TaskStatus_t structure for each task.  An array could be */
	/* allocated statically at compile time. */
	pxTaskStatusArray = pvPortMalloc( uxArraySize * sizeof( TaskStatus_t ) );

	FreeRTOS_printf( ( "Task name    Prio    Stack    Time(uS) Perc \n" ) );

	if( pxTaskStatusArray != NULL )
	{
		/* Generate raw status information about each task. */
		uint32_t ulDummy;
		xTaskClearCounters = aDoClear;
		uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulDummy );

		ullTotalRunTime = ullGetHighResolutionTime() - ullHiresTime;

		/* For percentage calculations. */
		ullTotalRunTime /= 1000UL;

		/* Avoid divide by zero errors. */
		if( ullTotalRunTime > 0ull )
		{
			/* For each populated position in the pxTaskStatusArray array, */
			/* format the raw data as human readable ASCII data */
			for( x = 0; x < uxArraySize; x++ )
			{
				/* What percentage of the total run time has the task used? */
				/* This will always be rounded down to the nearest integer. */
				/* ulTotalRunTimeDiv100 has already been divided by 100. */
				ulStatsAsPermille = pxTaskStatusArray[ x ].ulRunTimeCounter / ullTotalRunTime;

				FreeRTOS_printf( ( "%-14.14s %2lu %8u %8lu  %3lu.%lu %%\n",
								   pxTaskStatusArray[ x ].pcTaskName,
								   pxTaskStatusArray[ x ].uxCurrentPriority,
								   pxTaskStatusArray[ x ].usStackHighWaterMark,
								   pxTaskStatusArray[ x ].ulRunTimeCounter,
								   ulStatsAsPermille / 10,
								   ulStatsAsPermille % 10 ) );
			}
		}

		/* The array is no longer needed, free the memory it consumes. */
		vPortFree( pxTaskStatusArray );
	}

	ulStackSize = ( uint32_t ) &( _Min_Stack_Size );
	FreeRTOS_printf( ( "Heap: min/cur/max: %lu %lu %lu stack %lu\n",
					   ( uint32_t ) xPortGetMinimumEverFreeHeapSize(),
					   ( uint32_t ) xPortGetFreeHeapSize(),
					   ( uint32_t ) ulHeapSize,
					   ( uint32_t ) ulStackSize ) );

	if( aDoClear != pdFALSE )
	{
/*		ulListTime = xTaskGetTickCount(); */
		ullHiresTime = ullGetHighResolutionTime();
	}
}
/*-----------------------------------------------------------*/

/* The application should supply the following time-function.
 * It must return the number of seconds that have passed since
 * 1/1/1970. */
uint32_t ulApplicationTimeHook( void )
{
	return 1660624704U + xTaskGetTickCount() / 1000U;
}
/*-----------------------------------------------------------*/

#define MEMCPY_SIZE    2048

/*
 * 2015-02-25 14:26:56.532 1:18:21    3.428.000 [IP-task   ] Copy internal 12679 external 8347
 *
 * 12679 * 2048 * 10 = 259,665,920
 * 8347 * 2048 * 10 = 170,946,560
 * 259,665,920 / 168000000 = 1.5456304761904761904761904761905
 */


#if defined( __GNUC__ ) /*!< GCC Compiler */

	volatile int unknown_calls = 0;

	struct timeval;

	int _gettimeofday( struct timeval * pcTimeValue,
					   void * pvTimeZone )
	{
		( void ) pcTimeValue;
		( void ) pvTimeZone;
		unknown_calls++;

		return 0;
	}
	/*-----------------------------------------------------------*/

	#if GNUC_NEED_SBRK
		void * _sbrk( ptrdiff_t __incr )
		{
			( void ) __incr;
			unknown_calls++;

			return 0;
		}
		/*-----------------------------------------------------------*/
	#endif
#endif /* if defined( __GNUC__ ) */
/*-----------------------------------------------------------*/

#ifndef SRAM1_BASE
	#define SRAM1_BASE    ( ( uint32_t ) 0x20000000 )    /*!< SRAM1(112 KB) base address in the alias region                             */
#endif
#ifndef SRAM2_BASE
	#define SRAM2_BASE    ( ( uint32_t ) 0x2001C000 )    /*!< SRAM2(16 KB) base address in the alias region                              */
#endif
#ifndef SRAM3_BASE
	#define SRAM3_BASE    ( ( uint32_t ) 0x20020000 )    /*!< SRAM3(64 KB) base address in the alias region                              */
#endif

typedef struct xMemoryArea
{
	uint32_t ulStart;
	uint32_t ulSize;
} MemoryArea_t;

#define sizeKB    1024ul

MemoryArea_t xMemoryAreas[] =
{
	{ ulStart : SRAM1_BASE, ulSize : 112 * sizeKB },
	{ ulStart : SRAM2_BASE, ulSize : 16 * sizeKB  },
	{ ulStart : SRAM3_BASE, ulSize : 64 * sizeKB  },
};

static volatile int errors[ 3 ] = { 0, 0, 0 };

/*
 * static void memory_tests()
 * {
 *  int index;
 *  char buffer[ 16 ];
 *  for( index = 0; index < ARRAY_SIZE(xMemoryAreas); index++ )
 *  {
 *  int offset;
 *  unsigned char *source = ( unsigned char * ) ( xMemoryAreas[ index ].ulStart + 8192 );
 *
 *      for( offset = 0; offset < sizeof( buffer ); offset++ )
 *      {
 *          buffer[ offset ] = source[ offset ];
 *      }
 *      for( offset = 0; offset < sizeof( buffer ); offset++ )
 *      {
 *          source[ offset ] = buffer[ offset ] + 1;
 *      }
 *      for( offset = 0; offset < sizeof( buffer ); offset++ )
 *      {
 *          if( source[ offset ] != buffer[ offset ] + 1 )
 *          {
 *              errors[ index ]++;
 *          }
 *      }
 *      // Restore original contents.
 *      for( offset = 0; offset < sizeof( buffer ); offset++ )
 *      {
 *          source[ offset ] = buffer[ offset ];
 *      }
 *  }
 * }
 */

/* Add legacy definition */
#define  GPIO_Speed_2MHz      GPIO_SPEED_LOW
#define  GPIO_Speed_25MHz     GPIO_SPEED_MEDIUM
#define  GPIO_Speed_50MHz     GPIO_SPEED_FAST
#define  GPIO_Speed_100MHz    GPIO_SPEED_HIGH

/*_HT_ Please consider keeping the function below called HAL_ETH_MspInit().
 * It will be called from "stm32f4xx_hal_eth.c".
 * It is defined there as weak only.
 * My board didn't work without the settings made below.
 */
/* This is a callback function. */

void HAL_ETH_MspInit( ETH_HandleTypeDef * heth )
{
	GPIO_InitTypeDef GPIO_InitStruct;

	if( heth->Instance == ETH )
	{
		__ETH_CLK_ENABLE();         /* defined as __HAL_RCC_ETH_CLK_ENABLE. */
		__ETHMACRX_CLK_ENABLE();    /* defined as __HAL_RCC_ETHMACRX_CLK_ENABLE. */
		__ETHMACTX_CLK_ENABLE();    /* defined as __HAL_RCC_ETHMACTX_CLK_ENABLE. */

		/* Enable GPIOs clocks */
/*	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB | */
/*						   RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOI | */
/*						   RCC_AHB1Periph_GPIOG | RCC_AHB1Periph_GPIOH | */
/*						   RCC_AHB1Periph_GPIOD | RCC_AHB1Periph_GPIOE | */
/*						   RCC_AHB1Periph_GPIOF, ENABLE); */

/*	/ * Enable SYSCFG clock * / */
/*	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE); */

		/* Configure MCO (PA8) */
		GPIO_InitStruct.Pin = GPIO_PIN_8;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF11_ETH; /*GPIO_AF0_MCO; */
		HAL_GPIO_Init( GPIOA, &GPIO_InitStruct );

		/* MII/RMII Media interface selection --------------------------------------*/
		#if ( ipconfigUSE_RMII != 0 )
			/* Mode RMII with STM324xx-EVAL */
			/*SYSCFG_ETH_MediaInterfaceConfig(SYSCFG_ETH_MediaInterface_RMII); */
		#else
			/* Mode MII with STM324xx-EVAL  */
			#ifdef PHY_CLOCK_MCO
				/* Output HSE clock (25MHz) on MCO pin (PA8) to clock the PHY */
				RCC_MCO1Config( RCC_MCO1Source_HSE, RCC_MCO1Div_1 );
			#endif /* PHY_CLOCK_MCO */
			/*SYSCFG_ETH_MediaInterfaceConfig(SYSCFG_ETH_MediaInterface_MII); */
		#endif

		/* Ethernet pins configuration ************************************************/

		/*
		 *                                  STM3240G-EVAL
		 *                                  DP83848CVV	LAN8720
		 * ETH_MDIO -------------------------> PA2		==	PA2
		 * ETH_MDC --------------------------> PC1		==	PC1
		 * ETH_PPS_OUT ----------------------> PB5			x
		 * ETH_MII_CRS ----------------------> PH2			x
		 * ETH_MII_COL ----------------------> PH3			x
		 * ETH_MII_RX_ER --------------------> PI10		x
		 * ETH_MII_RXD2 ---------------------> PH6			x
		 * ETH_MII_RXD3 ---------------------> PH7			x
		 * ETH_MII_TX_CLK -------------------> PC3			x
		 * ETH_MII_TXD2 ---------------------> PC2			x
		 * ETH_MII_TXD3 ---------------------> PB8			x
		 * ETH_MII_RX_CLK/ETH_RMII_REF_CLK---> PA1		==	PA1
		 * ETH_MII_RX_DV/ETH_RMII_CRS_DV ----> PA7		==	PA7
		 * ETH_MII_RXD0/ETH_RMII_RXD0 -------> PC4		==	PC4
		 * ETH_MII_RXD1/ETH_RMII_RXD1 -------> PC5		==	PC5
		 * ETH_MII_TX_EN/ETH_RMII_TX_EN -----> PG11	<>	PB11
		 * ETH_MII_TXD0/ETH_RMII_TXD0 -------> PG13	<>	PB12
		 * ETH_MII_TXD1/ETH_RMII_TXD1 -------> PG14	<>	PB13
		 */

		/* Configure PA1, PA2 and PA7 */
		GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
		HAL_GPIO_Init( GPIOA, &GPIO_InitStruct );

		#if ( USE_STM324xG_EVAL != 0 )
			{
				/* Configure PB5 and PB8 */
				GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_8;
				GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
				GPIO_InitStruct.Pull = GPIO_NOPULL;
				GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
				GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
				HAL_GPIO_Init( GPIOB, &GPIO_InitStruct );

				/* Configure PC1, PC2, PC3, PC4 and PC5 */
				GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
				GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
				GPIO_InitStruct.Pull = GPIO_NOPULL;
				GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
				GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
				HAL_GPIO_Init( GPIOC, &GPIO_InitStruct );
			}
		#else /* if ( USE_STM324xG_EVAL != 0 ) */
			{
				/* Configure PC1, PC4 and PC5 */
				GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
				GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
				GPIO_InitStruct.Pull = GPIO_NOPULL;
				GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
				GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
				HAL_GPIO_Init( GPIOC, &GPIO_InitStruct );
			}
		#endif /* USE_STM324xG_EVAL */


		/* Configure PC1, PC2, PC3, PC4 and PC5 */
		#if ( USE_STM324xG_EVAL != 0 )
			{
				/* Configure PG11, PG13 and PG14 */
				GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14;
				GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
				GPIO_InitStruct.Pull = GPIO_NOPULL;
				GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
				GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
				HAL_GPIO_Init( GPIOG, &GPIO_InitStruct );

				/* Configure PH2, PH3, PH6, PH7 */
				GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_6 | GPIO_PIN_7;
				GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
				GPIO_InitStruct.Pull = GPIO_NOPULL;
				GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
				GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
				HAL_GPIO_Init( GPIOH, &GPIO_InitStruct );

				/* Configure PI10 */
				GPIO_InitStruct.Pin = GPIO_PIN_10;
				GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
				GPIO_InitStruct.Pull = GPIO_NOPULL;
				GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
				GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
				HAL_GPIO_Init( GPIOI, &GPIO_InitStruct );
			}
		#else /* if ( USE_STM324xG_EVAL != 0 ) */
			{
				/* Configure PB11, PB12 and PB13 */
				GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
				GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
				GPIO_InitStruct.Pull = GPIO_NOPULL;
				GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
				GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
				HAL_GPIO_Init( GPIOB, &GPIO_InitStruct );
			}
		#endif /* USE_STM324xG_EVAL */
		HAL_NVIC_SetPriority( ETH_IRQn, ipconfigMAC_INTERRUPT_PRIORITY, 0 );
		HAL_NVIC_EnableIRQ( ETH_IRQn );
	} /* if( heth->Instance == ETH ) */
}
/*-----------------------------------------------------------*/

uint32_t ulApplicationGetNextSequenceNumber( uint32_t ulSourceAddress,
											 uint16_t usSourcePort,
											 uint32_t ulDestinationAddress,
											 uint16_t usDestinationPort )
{
	( void ) ulSourceAddress;
	( void ) usSourcePort;
	( void ) ulDestinationAddress;
	( void ) usDestinationPort;
	uint32_t ulReturn;
	xApplicationGetRandomNumber( &( ulReturn ) );
	return ulReturn;
}

static uint8_t msg_1[] =
{
	0x9c, 0x5c, 0x8e, 0x38, 0x06, 0x6c, 0x74, 0xb5, 0x7e, 0xf0, 0x47, 0xee, 0x08, 0x00, 0x45, 0x08,
	0x00, 0xb7, 0x00, 0x00, 0x40, 0x00, 0x33, 0x06, 0x7e, 0x6c, 0xd4, 0x66, 0x31, 0xb9, 0xc0, 0xa8,
	0x02, 0x05, 0x8e, 0x0c, 0x36, 0x23, 0x7d, 0x3d, 0x51, 0x68, 0x44, 0xa3, 0x8b, 0x06, 0x50, 0x18,
	0x04, 0x01, 0xde, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x14, 0x00, 0x64, 0x31, 0x3a, 0x65,
	0x69, 0x31, 0x65, 0x31, 0x3a, 0x6d, 0x64, 0x31, 0x31, 0x3a, 0x75, 0x74, 0x5f, 0x6d, 0x65, 0x74,
	0x61, 0x64, 0x61, 0x74, 0x61, 0x69, 0x33, 0x65, 0x36, 0x3a, 0x75, 0x74, 0x5f, 0x70, 0x65, 0x78,
	0x69, 0x31, 0x65, 0x65, 0x31, 0x33, 0x3a, 0x6d, 0x65, 0x74, 0x61, 0x64, 0x61, 0x74, 0x61, 0x5f,
	0x73, 0x69, 0x7a, 0x65, 0x69, 0x33, 0x37, 0x37, 0x30, 0x36, 0x65, 0x31, 0x3a, 0x70, 0x69, 0x33,
	0x36, 0x33, 0x36, 0x34, 0x65, 0x34, 0x3a, 0x72, 0x65, 0x71, 0x71, 0x69, 0x35, 0x31, 0x32, 0x65,
	0x31, 0x31, 0x3a, 0x75, 0x70, 0x6c, 0x6f, 0x61, 0x64, 0x5f, 0x6f, 0x6e, 0x6c, 0x79, 0x69, 0x31,
	0x65, 0x31, 0x3a, 0x76, 0x31, 0x37, 0x3a, 0x54, 0x72, 0x61, 0x6e, 0x73, 0x6d, 0x69, 0x73, 0x73,
	0x69, 0x6f, 0x6e, 0x20, 0x33, 0x2e, 0x30, 0x30, 0x65, 0x00, 0x00, 0x00, 0x01, 0x0e, 0x00, 0x00,
	0x00, 0x03, 0x09, 0x8e, 0x0c

/*	0x54, 0x55, 0x58, 0x10, 0x00, 0x37, 0x00, 0xe0,   0x4c, 0x36, 0x01, 0x3b, 0x08, 0x00, 0x45, 0x00, */
/*	0x00, 0x34, 0x49, 0xa7, 0x40, 0x00, 0x80, 0x06,   0x00, 0x00, 0xc0, 0xa8, 0x01, 0x0a, 0xc0, 0xa8, */
/*	0x01, 0x05, 0xd6, 0xc5, 0x27, 0x10, 0x0c, 0x5b,   0xa2, 0x4b, 0x00, 0x00, 0x00, 0x00, 0x80, 0x02, */
/*	0xfa, 0xf0, 0x83, 0x86, 0x00, 0x00, 0x02, 0x04,   0x05, 0xb4, 0x01, 0x03, 0x03, 0x08, 0x01, 0x01, */
/*	0x04, 0x02 */
};

static uint8_t msg_2[] =
{
	0x9c, 0x5c, 0x8e, 0x38, 0x06, 0x6c, 0x74, 0xb5, 0x7e, 0xf0, 0x47, 0xee, 0x08, 0x00, 0x45, 0x08,
	0x00, 0x36, 0xe5, 0x04, 0x00, 0x00, 0x72, 0x11, 0x1f, 0xe9, 0xc6, 0x35, 0xba, 0xde, 0xc0, 0xa8,
	0x02, 0x05, 0xfc, 0x18, 0xa6, 0x70, 0x00, 0x22, 0xd6, 0xe4, 0x21, 0x01, 0x20, 0x95, 0xc7, 0x01,
	0xd2, 0x0c, 0x8e, 0xc7, 0x29, 0x4b, 0x00, 0x01, 0x00, 0x13, 0x2e, 0x09, 0xeb, 0x62, 0x00, 0x04,
	0x96, 0xfe, 0xff, 0x3f
/*	0x00, 0xe0, 0x4c, 0x36, 0x01, 0x3b, 0x54, 0x55,   0x58, 0x10, 0x00, 0x37, 0x08, 0x00, 0x45, 0x00, */
/*	0x00, 0x2c, 0x00, 0x04, 0x00, 0x00, 0x80, 0x06,   0x2d, 0xbd, 0xc0, 0xa8, 0x01, 0x05, 0xc0, 0xa8, */
/*	0x01, 0x0a, 0x27, 0x10, 0xd6, 0xc5, 0x29, 0xac,   0xd1, 0x2d, 0x0c, 0x5b, 0xa2, 0x4c, 0x60, 0x12, */
/*	0x05, 0x78, 0x44, 0x43, 0x00, 0x00, 0x02, 0x04,   0x05, 0x78, 0x00, 0x00 */
};

static uint8_t msg_3[] =
{
	0x74, 0xb5, 0x7e, 0xf0, 0x47, 0xee, 0x9c, 0x5c, 0x8e, 0x38, 0x06, 0x6c, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x89, 0x6e, 0x30, 0x40, 0x00, 0x80, 0x06, 0x82, 0xad, 0xc0, 0xa8, 0x02, 0x05, 0x7b, 0xd7,
	0xcb, 0x0c, 0x36, 0x28, 0xf6, 0xd8, 0x37, 0x91, 0x7e, 0xa4, 0xa0, 0x10, 0xf6, 0xfa, 0x50, 0x18,
	0x04, 0x02, 0xe5, 0xd8, 0x00, 0x00, 0xe9, 0xcd, 0x32, 0x03, 0x06, 0xf3, 0x66, 0x17, 0x14, 0x95,
	0x40, 0xf7, 0xdc, 0x61, 0xa3, 0x57, 0x58, 0x2f, 0x19, 0x33, 0x8c, 0x00, 0x3d, 0x26, 0x98, 0x35,
	0x14, 0x00, 0xc8, 0x39, 0x23, 0x31, 0x62, 0xc0, 0x57, 0x93, 0x8f, 0x88, 0x10, 0x82, 0x6e, 0x7d,
	0xfe, 0x0c, 0x2f, 0x5d, 0xee, 0x6b, 0x01, 0xb8, 0x50, 0xe5, 0x3d, 0x55, 0xe6, 0x23, 0xc6, 0xc2,
	0x30, 0x18, 0x0e, 0x8f, 0x00, 0x72, 0xd5, 0xb3, 0x16, 0xa0, 0xf0, 0xd3, 0x49, 0x3a, 0x2a, 0x9a,
	0x40, 0x57, 0x82, 0xd9, 0x08, 0x7f, 0x97, 0xbb, 0xbf, 0xa0, 0xbc, 0x2f, 0x89, 0x60, 0x77, 0x76,
	0xd7, 0xbf, 0x7c, 0x37, 0x84, 0xc6, 0xa7
/*	0x54, 0x55, 0x58, 0x10, 0x00, 0x37, 0x00, 0xe0,   0x4c, 0x36, 0x01, 0x3b, 0x08, 0x00, 0x45, 0x00, */
/*	0x00, 0x34, 0x49, 0xa8, 0x40, 0x00, 0x80, 0x06,   0x00, 0x00, 0xc0, 0xa8, 0x01, 0x0a, 0xc0, 0xa8, */
/*	0x01, 0x05, 0xd6, 0xc5, 0x27, 0x10, 0x0c, 0x5b,   0xa2, 0x4b, 0x00, 0x00, 0x00, 0x00, 0x80, 0x02, */
/*	0xfa, 0xf0, 0x83, 0x86, 0x00, 0x00, 0x02, 0x04,   0x05, 0xb4, 0x01, 0x03, 0x03, 0x08, 0x01, 0x01, */
/*	0x04, 0x02 */
};

static uint8_t msg_4[] =
{
	0x9c, 0x5c, 0x8e, 0x38, 0x06, 0x6c, 0x74, 0xb5, 0x7e, 0xf0, 0x47, 0xee, 0x08, 0x00, 0x45, 0x00,
	0x01, 0x5b, 0x80, 0x2e, 0x00, 0x00, 0x75, 0x11, 0x84, 0x7a, 0xc9, 0x69, 0xb3, 0xd2, 0xc0, 0xa8,
	0x02, 0x05, 0xe6, 0xc8, 0xa6, 0x70, 0x01, 0x47, 0x29, 0x76, 0x64, 0x32, 0x3a, 0x69, 0x70, 0x36,
	0x3a, 0xb4, 0xf9, 0x29, 0x76, 0x18, 0xf5, 0x31, 0x3a, 0x72, 0x64, 0x32, 0x3a, 0x69, 0x64, 0x32,
	0x30, 0x3a, 0x4e, 0x73, 0x65, 0xe3, 0x6b, 0x24, 0xba, 0xaa, 0xf5, 0x32, 0xcf, 0xe6, 0x0a, 0xd5,
	0x7d, 0x5b, 0x72, 0x8a, 0xda, 0x18, 0x35, 0x3a, 0x6e, 0x6f, 0x64, 0x65, 0x73, 0x32, 0x30, 0x38,
	0x3a, 0x4c, 0xd0, 0x7b, 0x5f, 0x42, 0x0e, 0x90, 0xc4, 0x34, 0x9e, 0x5e, 0x92, 0xb7, 0x05, 0x7a,
	0x5e, 0xc7, 0x23, 0x73, 0xbd, 0x5f, 0x1c, 0x30, 0x1c, 0xc8, 0xd5, 0x4c, 0x8e, 0x4e, 0x4e, 0x5c,
	0x9f, 0x5c, 0x53, 0x20, 0xab, 0x97, 0xdc, 0xa5, 0xe3, 0x08, 0xfa, 0x43, 0xda, 0x2d, 0xf0, 0x4b,
	0x47, 0xbf, 0x67, 0xf7, 0x8a, 0x4c, 0x58, 0x3b, 0x75, 0xff, 0x58, 0x16, 0x76, 0xeb, 0x6a, 0x68,
	0x23, 0xcc, 0x3c, 0xf1, 0x9c, 0x2b, 0x90, 0x61, 0x12, 0x97, 0xcb, 0x4e, 0x68, 0x23, 0x27, 0x4c,
	0x03, 0x84, 0x2b, 0xb0, 0xb8, 0xa9, 0xc7, 0x5a, 0x24, 0xc1, 0xa2, 0x20, 0x82, 0x7d, 0xc4, 0xdc,
	0xe7, 0x3a, 0x7c, 0xd5, 0xe3, 0x88, 0x4f, 0xc8, 0xd5, 0x4d, 0xf5, 0x35, 0x72, 0xa4, 0x51, 0x23,
	0xed, 0x91, 0x85, 0xe2, 0x3e, 0x3a, 0xa6, 0xdc, 0x10, 0x10, 0xfe, 0x99, 0xec, 0x05, 0x87, 0xa5,
	0xa0, 0x1a, 0xe1, 0x4d, 0x9d, 0x4f, 0xab, 0x2d, 0x49, 0x91, 0x81, 0x3d, 0x9b, 0xf8, 0x2e, 0x55,
	0x02, 0x2a, 0xa4, 0xac, 0xb5, 0x49, 0xf4, 0x2a, 0x02, 0xdd, 0xfd, 0x1a, 0xe1, 0x4d, 0x48, 0x1e,
	0xf8, 0x9e, 0x82, 0xe1, 0x10, 0xbd, 0x55, 0x19, 0x61, 0x47, 0x7b, 0xd7, 0xd0, 0x23, 0x87, 0x49,
	0xf2, 0xc3, 0x9a, 0xd6, 0xcb, 0x1d, 0xc7, 0x4d, 0x32, 0x2a, 0xdd, 0xcb, 0xb3, 0xe5, 0x4f, 0x9f,
	0xda, 0xa7, 0x90, 0x1c, 0xe8, 0xad, 0x00, 0x9d, 0xd3, 0x96, 0xc5, 0x25, 0x30, 0x59, 0xce, 0x48,
	0x28, 0x35, 0x3a, 0x74, 0x6f, 0x6b, 0x65, 0x6e, 0x32, 0x30, 0x3a, 0x02, 0x3f, 0xb7, 0x6f, 0xd8,
	0x4d, 0x98, 0x75, 0x04, 0xbd, 0xe3, 0xd2, 0x09, 0x36, 0x11, 0x03, 0x84, 0x4a, 0x14, 0x38, 0x65,
	0x31, 0x3a, 0x74, 0x34, 0x3a, 0x4b, 0x01, 0x00, 0x00, 0x31, 0x3a, 0x76, 0x34, 0x3a, 0x55, 0x54,
	0xb2, 0xde, 0x31, 0x3a, 0x79, 0x31, 0x3a, 0x72, 0x65
/*	0x00, 0xe0, 0x4c, 0x36, 0x01, 0x3b, 0x54, 0x55,   0x58, 0x10, 0x00, 0x37, 0x08, 0x00, 0x45, 0x00, */
/*	0x00, 0x2c, 0x00, 0x05, 0x00, 0x00, 0x80, 0x06,   0x2d, 0xbc, 0xc0, 0xa8, 0x01, 0x05, 0xc0, 0xa8, */
/*	0x01, 0x0a, 0x27, 0x10, 0xd6, 0xc5, 0x29, 0xac,   0xd1, 0x2d, 0x0c, 0x5b, 0xa2, 0x4c, 0x60, 0x12, */
/*	0x05, 0x78, 0x44, 0x43, 0x00, 0x00, 0x02, 0x04,   0x05, 0x78, 0x00, 0x00 */
};

uint8_t ucTCPv6Packet[] =
{
	0x9c, 0x5c, 0x8e, 0x38, 0x06, 0x6c, 0x00, 0x11, 0x22, 0x33, 0x44, 0x60, 0x86, 0xdd, 0x60, 0x0a,
	0x7e, 0xf3, 0x00, 0x18, 0x06, 0x80, 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x70, 0x07, 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x68, 0x16,
	0x5e, 0x9b, 0x80, 0xa0, 0x9e, 0xdb, 0x00, 0x17, 0x08, 0x28, 0xb2, 0x4f, 0xe6, 0xbd, 0x6f, 0xf6,
	0xa5, 0xc3, 0x60, 0x10, 0x05, 0xa0, 0x88, 0x4f, 0x00, 0x00, 0x02, 0x04, 0x05
};

const struct SPacket packets[] =
{
	{ msg_1, sizeof msg_1 },
	{ msg_2, sizeof msg_2 },
	{ msg_3, sizeof msg_3 },
	{ msg_4, sizeof msg_4 },
};

#if ( ipconfigUSE_CALLBACKS != 0 )
	static BaseType_t xOnUdpReceive( Socket_t xSocket,
									 void * pvData,
									 size_t xLength,
									 const struct freertos_sockaddr * pxFrom,
									 const struct freertos_sockaddr * pxDest )
	{
		( void ) xSocket;
		( void ) pvData;
		( void ) xLength;
		( void ) pxFrom;
		( void ) pxDest;
		#if ( ipconfigUSE_IPv6 != 0 )
			const struct freertos_sockaddr * pxFrom6 = ( const struct freertos_sockaddr * ) pxFrom;
/*const struct freertos_sockaddr *pxDest6 = ( const struct freertos_sockaddr * ) pxDest; */
		#endif
		#if ( ipconfigUSE_IPv6 != 0 )
			if( pxFrom6->sin_family == FREERTOS_AF_INET6 )
			{
/*		FreeRTOS_printf( ( "xOnUdpReceive_6: %d bytes\n",  ( int ) xLength ) ); */
/*		FreeRTOS_printf( ( "xOnUdpReceive_6: from %pip\n", pxFrom6->sin_address.xIP_IPv6.ucBytes ) ); */
/*		FreeRTOS_printf( ( "xOnUdpReceive_6: to   %pip\n", pxDest6->sin_address.xIP_IPv6.ucBytes ) ); */
			}
			else
		#endif
		{
/*		FreeRTOS_printf( ( "xOnUdpReceive_4: %d bytes\n", ( int ) xLength ) ); */
/*		FreeRTOS_printf( ( "xOnUdpReceive_4: from %lxip\n", FreeRTOS_ntohl( pxFrom->sin_addr ) ) ); */
/*		FreeRTOS_printf( ( "xOnUdpReceive_4: to   %lxip\n", FreeRTOS_ntohl( pxDest->sin_addr ) ) ); */
		}

		/* Returning 0 means: not yet consumed. */
		return 0;
	}
#endif /* if ( ipconfigUSE_CALLBACKS != 0 ) */

void vUDPTest( void * pvParameters )
{
	Socket_t xUDPSocket;
	struct freertos_sockaddr xBindAddress;
	char pcBuffer[ 128 ];
	const TickType_t x1000ms = 1000UL / portTICK_PERIOD_MS;
	int port_number = 50001;    /* 30718; */
	socklen_t xAddressLength;

	( void ) pvParameters;

	/* Create the socket. */
	xUDPSocket = FreeRTOS_socket( FREERTOS_AF_INET,
								  FREERTOS_SOCK_DGRAM, /*FREERTOS_SOCK_DGRAM for UDP.*/
								  FREERTOS_IPPROTO_UDP );

	/* Check the socket was created. */
	configASSERT( xUDPSocket != FREERTOS_INVALID_SOCKET );

	xBindAddress.sin_address.ulIP_IPv4 = FreeRTOS_GetIPAddress();
	xBindAddress.sin_port = FreeRTOS_htons( port_number );
	xBindAddress.sin_family = FREERTOS_AF_INET4;
	int rc = FreeRTOS_bind( xUDPSocket, &xBindAddress, sizeof xAddressLength );
	FreeRTOS_printf( ( "FreeRTOS_bind %u rc = %d", FreeRTOS_ntohs( xBindAddress.sin_port ), rc ) );

	if( rc != 0 )
	{
		vTaskDelete( NULL );
		configASSERT( 0 );
	}

	FreeRTOS_setsockopt( xUDPSocket, 0, FREERTOS_SO_RCVTIMEO, &x1000ms, sizeof( x1000ms ) );

	for( ; ; )
	{
		int rc_recv, rc_send;
		struct freertos_sockaddr xFromAddress;
		socklen_t len = sizeof xFromAddress;

		rc_recv = FreeRTOS_recvfrom( xUDPSocket, pcBuffer, sizeof pcBuffer, 0, &( xFromAddress ), &len );

		if( rc_recv > 0 )
		{
			uint8_t * pucBuffer = ( uint8_t * ) pcBuffer;
			char pcAddressBuffer[ 40 ];

			rc_send = FreeRTOS_sendto( xUDPSocket,
									   pcBuffer,
									   rc_recv,
									   0,
									   &( xFromAddress ),
									   sizeof xFromAddress );
			#if ( ipconfigUSE_IPv6 != 0 )
				if( xFromAddress.sin_family == ( uint8_t ) FREERTOS_AF_INET6 )
				{
					FreeRTOS_inet_ntop( FREERTOS_AF_INET6, ( void * ) xFromAddress.sin_address.xIP_IPv6.ucBytes, pcAddressBuffer, sizeof pcAddressBuffer );
				}
				else
			#endif
			{
				FreeRTOS_inet_ntop( FREERTOS_AF_INET4, ( void * ) &( xFromAddress.sin_address.ulIP_IPv4 ), pcAddressBuffer, sizeof pcAddressBuffer );
			}

			FreeRTOS_printf( ( "Received %s port %u: %d bytes: %02x %02x %02x %02x Send %d\n",
							   pcAddressBuffer,
							   FreeRTOS_htons( xFromAddress.sin_port ),
							   rc_recv,
							   pucBuffer[ 0 ],
							   pucBuffer[ 1 ],
							   pucBuffer[ 2 ],
							   pucBuffer[ 3 ],
							   rc_send ) );
		}
	}
}

static void setUDPCheckSum( Socket_t xSocket,
							BaseType_t iUseChecksum )
{
	if( iUseChecksum == pdFALSE )
	{
		/* Turn the UDP checksum creation off for outgoing UDP packets. */
		FreeRTOS_setsockopt( xSocket,                  /* The socket being modified. */
							 0,                        /* Not used. */
							 FREERTOS_SO_UDPCKSUM_OUT, /* Setting checksum on/off. */
							 NULL,                     /* NULL means off. */
							 0 );                      /* Not used. */
	}
	else
	{
		/* The checksum is used by default, so there is nothing to do here.
		 * If the checksum was off it could be turned on again using an option
		 * value other than NULL, for example ( ( void * ) 1 ). */
		/* Turn the UDP checksum creation off for outgoing UDP packets. */
		FreeRTOS_setsockopt( xSocket,                  /* The socket being modified. */
							 0,                        /* Not used. */
							 FREERTOS_SO_UDPCKSUM_OUT, /* Setting checksum on/off. */
							 ( void * ) 1U,            /* non NULL means on. */
							 0 );                      /* Not used. */
	}
}


static uint8_t ucGoodDnsResponse[] =
{
	0xd7, 0x66, 0x81, 0x80, 0x00, 0x01, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x61, 0x33, 0x37,
	0x62, 0x78, 0x76, 0x31, 0x63, 0x62, 0x64, 0x61, 0x33, 0x6a, 0x67, 0x03, 0x69, 0x6f, 0x74, 0x09,
	0x75, 0x73, 0x2d, 0x77, 0x65, 0x73, 0x74, 0x2d, 0x32, 0x09, 0x61, 0x6d, 0x61, 0x7a, 0x6f, 0x6e,
	0x61, 0x77, 0x73, 0x03, 0x63, 0x6f, 0x6d, 0x00, 0x00, 0x01, 0x00, 0x01, 0xc0, 0x0c, 0x00, 0x05,
	0x00, 0x01, 0x00, 0x00, 0x01, 0x2c, 0x00, 0x1e, 0x0c, 0x69, 0x6f, 0x74, 0x6d, 0x6f, 0x6f, 0x6e,
	0x72, 0x61, 0x6b, 0x65, 0x72, 0x09, 0x75, 0x73, 0x2d, 0x77, 0x65, 0x73, 0x74, 0x2d, 0x32, 0x04,
	0x70, 0x72, 0x6f, 0x64, 0xc0, 0x1b, 0xc0, 0x48, 0x00, 0x05, 0x00, 0x01, 0x00, 0x00, 0x00, 0xec,
	0x00, 0x45, 0x09, 0x64, 0x75, 0x61, 0x6c, 0x73, 0x74, 0x61, 0x63, 0x6b, 0x2a, 0x69, 0x6f, 0x74,
	0x6d, 0x6f, 0x6f, 0x6e, 0x72, 0x61, 0x6b, 0x65, 0x72, 0x2d, 0x75, 0x2d, 0x65, 0x6c, 0x62, 0x2d,
	0x31, 0x77, 0x38, 0x71, 0x6e, 0x77, 0x31, 0x33, 0x33, 0x36, 0x7a, 0x71, 0x2d, 0x31, 0x31, 0x38,
	0x36, 0x33, 0x34, 0x38, 0x30, 0x39, 0x32, 0x09, 0x75, 0x73, 0x2d, 0x77, 0x65, 0x73, 0x74, 0x2d,
	0x32, 0x03, 0x65, 0x6c, 0x62, 0xc0, 0x29, 0xc0, 0x72, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x23, 0x00, 0x04, 0x22, 0xd3, 0x41, 0xdb, 0xc0, 0x72, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x23, 0x00, 0x04, 0x22, 0xd3, 0x53, 0xe4, 0xc0, 0x72, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x23, 0x00, 0x04, 0x22, 0xd3, 0xb6, 0x17, 0xc0, 0x72, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x23, 0x00, 0x04, 0x22, 0xd6, 0xf5, 0xf0, 0xc0, 0x72, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x23, 0x00, 0x04, 0x22, 0xd7, 0xe6, 0xa4, 0xc0, 0x72, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x23, 0x00, 0x04, 0x36, 0x95, 0x5e, 0x45
};
/*-----------------------------------------------------------*/

typedef union
{
	uint16_t u16;
	uint8_t u8[ 2 ];
} _Union16;

static void crc_test()
{
	int trial;
	int do_swap;

	_Union16 testWord;

	testWord.u8[ 0 ] = ( uint8_t ) 0U;
	testWord.u8[ 1 ] = ( uint8_t ) 1U;
	FreeRTOS_printf( ( "%s endian\n", ( testWord.u16 == 0x01 ) ? "BIG" : "LITTLE" ) );
	uint8_t * pucCopy = ( uint8_t * ) pvPortMalloc( sizeof ucGoodDnsResponse + 8 );

	for( trial = 0; trial < 2; trial++ )
	{
		do_swap = trial;
		int uxIndex;
		uint16_t usSums[ 4 ];

		for( uxIndex = 0; uxIndex < ARRAY_SIZE( usSums ); uxIndex++ )
		{
			memcpy( pucCopy + uxIndex, ucGoodDnsResponse, sizeof ucGoodDnsResponse );
			usSums[ uxIndex ] = usGenerateChecksum( 0x1234U, pucCopy + uxIndex, sizeof ucGoodDnsResponse );
		}

		FreeRTOS_printf( ( "Swap = %d CRC = %04X %04X %04X %04X\n",
						   do_swap, usSums[ 0 ], usSums[ 1 ], usSums[ 2 ], usSums[ 3 ] ) );
	}

	do_swap = 1;
	vPortFree( pucCopy );
}

#if ( ipconfigMULTI_INTERFACE == 0 )
	extern BaseType_t xNTPHasTime;
	extern uint32_t ulNTPTime;

	struct
	{
		uint32_t ntpTime;
	}
	time_guard;

	int set_time( time_t * pxTime )
	{
		( void ) pxTime;
		time_guard.ntpTime = ulNTPTime - xTaskGetTickCount() / configTICK_RATE_HZ;
		return 0;
	}

	time_t get_time( time_t * puxTime )
	{
		if( xNTPHasTime != pdFALSE )
		{
			TickType_t passed = xTaskGetTickCount() / configTICK_RATE_HZ;

			*( puxTime ) = time_guard.ntpTime + passed;
		}
		else
		{
			*( puxTime ) = 0U;
		}

		return 1;
	}
#endif

__attribute__( ( weak ) ) void vListInsertGeneric( List_t * const pxList,
												   ListItem_t * const pxNewListItem,
												   MiniListItem_t * const pxWhere )
{
	/* Insert a new list item into pxList, it does not sort the list,
	 * but it puts the item just before xListEnd, so it will be the last item
	 * returned by listGET_HEAD_ENTRY() */
	pxNewListItem->pxNext = ( struct xLIST_ITEM * configLIST_VOLATILE ) pxWhere;
	pxNewListItem->pxPrevious = pxWhere->pxPrevious;
	pxWhere->pxPrevious->pxNext = pxNewListItem;
	pxWhere->pxPrevious = pxNewListItem;

	/* Remember which list the item is in. */
	listLIST_ITEM_CONTAINER( pxNewListItem ) = ( struct xLIST * configLIST_VOLATILE ) pxList;

	( pxList->uxNumberOfItems )++;
}

int logPrintf( const char * apFmt,
			   ... ) __attribute__( ( format( __printf__, 1, 2 ) ) );                       /* Delayed write to serial device, non-blocking */
int logPrintf( const char * apFmt,
			   ... )
{
	char buffer[ 128 ];
	va_list args;

	va_start( args, apFmt );
	vsnprintf( buffer, sizeof buffer, apFmt, args );
	va_end( args );
	FreeRTOS_printf( ( "%s", buffer ) );
	return 0;
}

void printf_test()
{
	uint32_t ulValue = 0u;

/*		logPrintf("%u", ulValue); // Zynq: argument 2 has type 'uint32_t' {aka 'long unsigned int'} */
	logPrintf( "%lu", ulValue );   /* <== good */

	uint16_t usValue = 0u;
	logPrintf( "%u", usValue );   /* <== good */
/*		logPrintf("%lu", usValue);	// Zynq argument 2 has type 'int' */

	uint8_t ucValue = 0u;
	logPrintf( "%u", ucValue );   /* <== good */
/*		logPrintf("%lu", ucValue); // argument 2 has type 'int' */

/*		uint64_t ullValue = 0u; */
/*		logPrintf("%u", ullValue);  // argument 2 has type 'uint64_t' {aka 'long long unsigned int'} */
/*		logPrintf("%lu", ullValue); // <== good */

	unsigned uValue = 0u;
	logPrintf( "%u", uValue );   /* <== good */
/*		logPrintf("%lu", uValue); // argument 2 has type 'unsigned int' */

	size_t uxSize = 0u;
	logPrintf( "%u", uxSize );   /* <== good */
/*		logPrintf("%lu", uxSize); // argument 2 has type 'unsigned int' */
}

#undef ipconfigUSE_TCP
#undef ipconfigETHERNET_MINIMUM_PACKET_BYTES

#define ipconfigUSE_TCP                          0
#define ipconfigETHERNET_MINIMUM_PACKET_BYTES    0

#if ipconfigUSE_TCP == 1
	#define baMINIMAL_BUFFER_SIZE                sizeof( TCPPacket_t )
#else
	#define baMINIMAL_BUFFER_SIZE                sizeof( ARPPacket_t )
#endif /* ipconfigUSE_TCP == 1 */

#if defined( ipconfigETHERNET_MINIMUM_PACKET_BYTES )

/* Creating a static assert that can be used with sizeof()
 * expressions, but lint doesn't like it.
 * If the expression is not as expected, the compiler will
 * detect a division-by-zero in an enum expression. */
	#ifndef _lint
		#define ASSERT_CONCAT_( a, b )    a ## b
		#define ASSERT_CONCAT( a, b )     ASSERT_CONCAT_( a, b )
		#define STATIC_ASSERT( e ) \
	; enum { ASSERT_CONCAT( assert_line_, __LINE__ ) = 1 / ( !!( e ) ) }

		STATIC_ASSERT( ipconfigETHERNET_MINIMUM_PACKET_BYTES <= baMINIMAL_BUFFER_SIZE );
	#endif
#endif /* if defined( ipconfigETHERNET_MINIMUM_PACKET_BYTES ) */

/* Send SNMP messages to a particular UC3 device. */
static void snmp_test( void )
{
	static int nextPacket = -1;

	if( ++nextPacket >= ARRAY_SIZE( xSMNPPackets ) )
	{
		nextPacket = 0;
	}

	struct SPacket * pxPacket = &( xSMNPPackets[ nextPacket ] );
	int count = 0;

/*	pxPacket->pucBytes */
/*	pxPacket->uxLength */
	for( count = 0; count < 2; count++ )
	{
		NetworkBufferDescriptor_t * pxNetworkBuffer = pxGetNetworkBufferWithDescriptor( BUFFER_FROM_WHERE_CALL( "main(1).c" ) pxPacket->uxLength, 1000U );

		if( pxNetworkBuffer == NULL )
		{
			FreeRTOS_printf( ( "pxGetNetworkBufferWithDescriptor failed\n" ) );
		}
		else
		{
			memcpy( pxNetworkBuffer->pucEthernetBuffer, pxPacket->pucBytes, pxPacket->uxLength );
			pxNetworkBuffer->xDataLength = pxPacket->uxLength;
			uint32_t ulSenderNew = 0U;
			uint32_t ulTargetNew = 0U;
			ProtocolPacket_t * packet = ( ProtocolPacket_t * ) pxNetworkBuffer->pucEthernetBuffer;

			{
				BaseType_t xDoCalculate = pdFALSE;
				uint16_t usFrameType = packet->xUDPPacket.xEthernetHeader.usFrameType;
				uint8_t ucProtocol = packet->xUDPPacket.xIPHeader.ucProtocol;
				uint32_t ulSender = 0U;
				uint32_t ulTarget = 0U;

				if( usFrameType == ipIPv4_FRAME_TYPE )
				{
					ulSender = packet->xUDPPacket.xIPHeader.ulSourceIPAddress;
					ulTarget = packet->xUDPPacket.xIPHeader.ulDestinationIPAddress;
					ulSenderNew = ulSender;

					/* 192.168.2.228 */
					if( count == 0 )
					{
						ulSenderNew = FreeRTOS_inet_addr_quick( 192, 168, 2, 20 );
						ulTargetNew = FreeRTOS_inet_addr_quick( 192, 168, 2, 228 );
					}
					else
					{
						ulSenderNew = FreeRTOS_inet_addr_quick( 192, 168, 2, 20 );
						ulTargetNew = FreeRTOS_inet_addr_quick( 192, 168, 2, 5 );
					}

					#warning disabled xARPWaitResolution()
					/*xARPWaitResolution( ulTargetNew, 200U ); */
					uint32_t ulIPAddress = ulTargetNew;
					MACAddress_t xMACAddress;
					#if ( ipconfigMULTI_INTERFACE != 0 )
						struct xNetworkEndPoint * pxEndPoint;
					#endif

					if( eARPGetCacheEntry( &( ulIPAddress ), &( xMACAddress )
										   #if ( ipconfigMULTI_INTERFACE != 0 )
											   , &( pxEndPoint )
										   #endif
										   ) == eARPCacheHit )
					{
						memcpy( packet->xUDPPacket.xEthernetHeader.xDestinationAddress.ucBytes,
								xMACAddress.ucBytes,
								sizeof( packet->xUDPPacket.xEthernetHeader.xDestinationAddress.ucBytes ) );
					}

					if( ( ulSenderNew != ulSender ) || ( ulTargetNew != ulTarget ) )
					{
						packet->xUDPPacket.xIPHeader.ulSourceIPAddress = ulSenderNew;
						packet->xUDPPacket.xIPHeader.ulDestinationIPAddress = ulTargetNew;
						xDoCalculate = pdTRUE;
					}

					if( ucProtocol == ipPROTOCOL_UDP )
					{
					}
				}

				if( xDoCalculate )
				{
					IPHeader_t * pxIPHeader;
					TCPPacket_t * pxTCPPacket;
					pxTCPPacket = ( TCPPacket_t * ) pxNetworkBuffer->pucEthernetBuffer;
					pxIPHeader = &pxTCPPacket->xIPHeader;


					pxIPHeader->usHeaderChecksum = 0x00U;
					pxIPHeader->usHeaderChecksum = usGenerateChecksum( 0U, ( uint8_t * ) &( pxIPHeader->ucVersionHeaderLength ), ipSIZE_OF_IPv4_HEADER );
					pxIPHeader->usHeaderChecksum = ~FreeRTOS_htons( pxIPHeader->usHeaderChecksum );

					/* calculate the TCP checksum for an outgoing packet. */
					( void ) usGenerateProtocolChecksum( ( uint8_t * ) pxTCPPacket, pxPacket->uxLength, pdTRUE );

					/* A calculated checksum of 0 must be inverted as 0 means the checksum
					 * is disabled. */
					if( pxTCPPacket->xTCPHeader.usChecksum == 0U )
					{
						pxTCPPacket->xTCPHeader.usChecksum = 0xffffU;
					}
				}
			}

			IPStackEvent_t xSendEvent;

			xSendEvent.eEventType = eNetworkTxEvent;
			xSendEvent.pvData = ( void * ) pxNetworkBuffer;
			#if ( ipconfigMULTI_INTERFACE != 0 )
				pxNetworkBuffer->pxInterface = FreeRTOS_FirstNetworkInterface();
			#endif

			if( xSendEventStructToIPTask( &xSendEvent, 0U ) == pdPASS )
			{
/*
 *              uint8_t *pucBytes = packet->xUDPPacket.xEthernetHeader.xDestinationAddress.ucBytes;
 *              FreeRTOS_printf( ( "Packet %d sent to %xip @ %02x:%02x:%02x:%02x:%02x:%02x\n",
 *                  nextPacket,
 *                  FreeRTOS_ntohl( ulTargetNew ),
 *                  pucBytes[ 0 ],
 *                  pucBytes[ 1 ],
 *                  pucBytes[ 2 ],
 *                  pucBytes[ 3 ],
 *                  pucBytes[ 4 ],
 *                  pucBytes[ 5 ] ) );
 */
			}
			else
			{
				vReleaseNetworkBufferAndDescriptor( pxNetworkBuffer );
			}
		} /* if( pxNetworkBuffer != NULL ) */
	}     /* for */
}

void Init_LEDs()
{
	__HAL_RCC_GPIOD_CLK_ENABLE();
	GPIO_InitTypeDef BoardLEDs;
	BoardLEDs.Mode = GPIO_MODE_OUTPUT_PP;
	BoardLEDs.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
	HAL_GPIO_Init( GPIOD, &BoardLEDs );
}

void Set_LED_Number( BaseType_t xNumber,
					 BaseType_t xValue )
{
	uint32_t ulMask;

	switch( xNumber )
	{
		case 0:
			ulMask = LED_MASK_GREEN;
			break;

		case 1:
			ulMask = LED_MASK_ORANGE;
			break;

		case 2:
			ulMask = LED_MASK_RED;
			break;

		case 3:
			ulMask = LED_MASK_BLUE;
			break;

		default:
			return;
	}

	Set_LED_Mask( ulMask, xValue );
}

void Set_LED_Mask( uint32_t ulMask,
				   BaseType_t xValue )
{
	HAL_GPIO_WritePin( GPIOD, ulMask, ( xValue != 0 ) ? GPIO_PIN_SET : GPIO_PIN_RESET );
}

/* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application must provide an
 * implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
 * used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer,
									StackType_t ** ppxIdleTaskStackBuffer,
									uint32_t * pulIdleTaskStackSize )
{
/* If the buffers to be provided to the Idle task are declared inside this
 * function then they must be declared static - otherwise they will be allocated on
 * the stack and so not exists after this function exits. */
	static StaticTask_t xIdleTaskTCB;
	static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

	/* Pass out a pointer to the StaticTask_t structure in which the Idle task's
	 * state will be stored. */
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

	/* Pass out the array that will be used as the Idle task's stack. */
	*ppxIdleTaskStackBuffer = uxIdleTaskStack;

	/* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
	 * Note that, as the array is necessarily of type StackType_t,
	 * configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*-----------------------------------------------------------*/


static const uint8_t ucICMPPacket[] =
{
	0x12, 0x11, 0x22, 0x33, 0x44, 0x22, 0xa2, 0x4c, 0xc5, 0x84, 0x8e, 0x6c, 0x08, 0x00, 0x45, 0x00, /*	.."3D".L...l..E. */
	0x05, 0x30, 0x27, 0x4e, 0x40, 0x00, 0x40, 0x01, 0x76, 0x5a, 0xc0, 0xa8, 0x0b, 0x64, 0xc0, 0xa8, /*	.0'N@.@.vZ...d.. */
	0x0b, 0x70, 0x08, 0x00, 0x3c, 0x7e, 0x29, 0x12, 0x50, 0xb1, 0xa0, 0x40, 0x76, 0x3a, 0x5b, 0x41, /*	.p..<~).P..@v:[A */
	0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, /*	BCDEFGHIJKLMNOPQ */
	0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, /*	RSTUVWXYZABCDEFG */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, /*	HIJKLMNOPQRSTUVW */
	0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, /*	XYZABCDEFGHIJKLM */
	0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, /*	NOPQRSTUVWXYZABC */
	0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, /*	DEFGHIJKLMNOPQRS */
	0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, /*	TUVWXYZABCDEFGHI */
	0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, /*	JKLMNOPQRSTUVWXY */
	0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, /*	ZABCDEFGHIJKLMNO */
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, /*	PQRSTUVWXYZABCDE */
	0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, /*	FGHIJKLMNOPQRSTU */
	0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, /*	VWXYZABCDEFGHIJK */
	0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, /*	LMNOPQRSTUVWXYZA */
	0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, /*	BCDEFGHIJKLMNOPQ */
	0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, /*	RSTUVWXYZABCDEFG */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, /*	HIJKLMNOPQRSTUVW */
	0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, /*	XYZABCDEFGHIJKLM */
	0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, /*	NOPQRSTUVWXYZABC */
	0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, /*	DEFGHIJKLMNOPQRS */
	0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, /*	TUVWXYZABCDEFGHI */
	0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, /*	JKLMNOPQRSTUVWXY */
	0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, /*	ZABCDEFGHIJKLMNO */
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, /*	PQRSTUVWXYZABCDE */
	0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, /*	FGHIJKLMNOPQRSTU */
	0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, /*	VWXYZABCDEFGHIJK */
	0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, /*	LMNOPQRSTUVWXYZA */
	0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, /*	BCDEFGHIJKLMNOPQ */
	0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, /*	RSTUVWXYZABCDEFG */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, /*	HIJKLMNOPQRSTUVW */
	0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, /*	XYZABCDEFGHIJKLM */
	0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, /*	NOPQRSTUVWXYZABC */
	0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, /*	DEFGHIJKLMNOPQRS */
	0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, /*	TUVWXYZABCDEFGHI */
	0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, /*	JKLMNOPQRSTUVWXY */
	0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, /*	ZABCDEFGHIJKLMNO */
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, /*	PQRSTUVWXYZABCDE */
	0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, /*	FGHIJKLMNOPQRSTU */
	0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, /*	VWXYZABCDEFGHIJK */
	0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, /*	LMNOPQRSTUVWXYZA */
	0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, /*	BCDEFGHIJKLMNOPQ */
	0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, /*	RSTUVWXYZABCDEFG */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, /*	HIJKLMNOPQRSTUVW */
	0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, /*	XYZABCDEFGHIJKLM */
	0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, /*	NOPQRSTUVWXYZABC */
	0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, /*	DEFGHIJKLMNOPQRS */
	0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, /*	TUVWXYZABCDEFGHI */
	0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, /*	JKLMNOPQRSTUVWXY */
	0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, /*	ZABCDEFGHIJKLMNO */
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, /*	PQRSTUVWXYZABCDE */
	0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, /*	FGHIJKLMNOPQRSTU */
	0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, /*	VWXYZABCDEFGHIJK */
	0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, /*	LMNOPQRSTUVWXYZA */
	0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, /*	BCDEFGHIJKLMNOPQ */
	0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, /*	RSTUVWXYZABCDEFG */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, /*	HIJKLMNOPQRSTUVW */
	0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, /*	XYZABCDEFGHIJKLM */
	0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, /*	NOPQRSTUVWXYZABC */
	0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, /*	DEFGHIJKLMNOPQRS */
	0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, /*	TUVWXYZABCDEFGHI */
	0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, /*	JKLMNOPQRSTUVWXY */
	0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, /*	ZABCDEFGHIJKLMNO */
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, /*	PQRSTUVWXYZABCDE */
	0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, /*	FGHIJKLMNOPQRSTU */
	0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, /*	VWXYZABCDEFGHIJK */
	0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, /*	LMNOPQRSTUVWXYZA */
	0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, /*	BCDEFGHIJKLMNOPQ */
	0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, /*	RSTUVWXYZABCDEFG */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, /*	HIJKLMNOPQRSTUVW */
	0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, /*	XYZABCDEFGHIJKLM */
	0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, /*	NOPQRSTUVWXYZABC */
	0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, /*	DEFGHIJKLMNOPQRS */
	0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, /*	TUVWXYZABCDEFGHI */
	0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, /*	JKLMNOPQRSTUVWXY */
	0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, /*	ZABCDEFGHIJKLMNO */
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, /*	PQRSTUVWXYZABCDE */
	0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, /*	FGHIJKLMNOPQRSTU */
	0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, /*	VWXYZABCDEFGHIJK */
	0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, /*	LMNOPQRSTUVWXYZA */
	0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, /*	BCDEFGHIJKLMNOPQ */
	0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, /*	RSTUVWXYZABCDEFG */
	0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x5d, 0x00              /*	HIJKLMNOPQRS]. */
};

int trap_rx_event;
#if ( ipconfigUSE_IPv6 != 0 )
	void ICMPEchoTest( size_t uxOptionCount,
					   BaseType_t xSendRequest )
	{
		const size_t uxOptionsLength = ( 4U * uxOptionCount );
		const size_t uxRequestedSizeBytes = sizeof ucICMPPacket + uxOptionsLength;
		const size_t uxIPLength = ipSIZE_OF_ETH_HEADER + ipSIZE_OF_IPv4_HEADER;
		const size_t uxProtLength = sizeof ucICMPPacket - uxIPLength;
		const uint8_t * pucSource = ucICMPPacket;
		uint8_t * pucTarget;
		static const uint8_t macLaptop[ ipMAC_ADDRESS_LENGTH_BYTES ] = { 0x9c, 0x5c, 0x8e, 0x38, 0x06, 0x6c };
		static const uint8_t macMe[ ipMAC_ADDRESS_LENGTH_BYTES ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };
		IPStackEvent_t xIPStackEvent;
		NetworkBufferDescriptor_t * pxNetworkBuffer;
		uint16_t usLength;

		pxNetworkBuffer = pxGetNetworkBufferWithDescriptor( uxRequestedSizeBytes, 1000U );

		if( pxNetworkBuffer == NULL )
		{
			FreeRTOS_printf( ( "pxGetNetworkBufferWithDescriptor: failed\n" ) );
			return;
		}

		pucTarget = pxNetworkBuffer->pucEthernetBuffer;

		memcpy( pucTarget, pucSource, uxIPLength );
		pucSource = pucSource + uxIPLength;
		pucTarget = pucTarget + uxIPLength;

		if( uxOptionsLength != 0U )
		{
			memset( pucTarget, 0, uxOptionsLength );
			pucTarget = pucTarget + uxOptionsLength;
		}

		memcpy( pucTarget, pucSource, uxProtLength );

		pxNetworkBuffer->xDataLength = uxRequestedSizeBytes;

		ICMPPacket_t * xICMPPacket = ( ICMPPacket_t * ) pxNetworkBuffer->pucEthernetBuffer;

		if( xSendRequest == pdTRUE )
		{
			xICMPPacket->xIPHeader.ulSourceIPAddress = FreeRTOS_inet_addr_quick( 192, 168, 2, 114 );
			xICMPPacket->xIPHeader.ulDestinationIPAddress = FreeRTOS_inet_addr_quick( 192, 168, 2, 5 );
			memcpy( xICMPPacket->xEthernetHeader.xSourceAddress.ucBytes, macMe, ipMAC_ADDRESS_LENGTH_BYTES );
			memcpy( xICMPPacket->xEthernetHeader.xDestinationAddress.ucBytes, macLaptop, ipMAC_ADDRESS_LENGTH_BYTES );
		}
		else
		{
			xICMPPacket->xIPHeader.ulSourceIPAddress = FreeRTOS_inet_addr_quick( 192, 168, 2, 5 );
			xICMPPacket->xIPHeader.ulDestinationIPAddress = FreeRTOS_inet_addr_quick( 192, 168, 2, 114 );
			memcpy( xICMPPacket->xEthernetHeader.xSourceAddress.ucBytes, macLaptop, ipMAC_ADDRESS_LENGTH_BYTES );
			memcpy( xICMPPacket->xEthernetHeader.xDestinationAddress.ucBytes, macMe, ipMAC_ADDRESS_LENGTH_BYTES );
		}

		xICMPPacket->xIPHeader.ucVersionHeaderLength = 0x45 + ( uxOptionCount & 0x0fU );

		usLength = FreeRTOS_ntohs( xICMPPacket->xIPHeader.usLength );
		usLength += uxOptionsLength;
		xICMPPacket->xIPHeader.usLength = FreeRTOS_htons( usLength );

		xICMPPacket->xIPHeader.usHeaderChecksum = 0x00U;
		xICMPPacket->xIPHeader.usHeaderChecksum = usGenerateChecksum( 0U, ( uint8_t * ) &( xICMPPacket->xIPHeader.ucVersionHeaderLength ), ipSIZE_OF_IPv4_HEADER );
		xICMPPacket->xIPHeader.usHeaderChecksum = ~FreeRTOS_htons( xICMPPacket->xIPHeader.usHeaderChecksum );

		usGenerateProtocolChecksum( ( uint8_t * ) pxNetworkBuffer->pucEthernetBuffer, uxRequestedSizeBytes, pdTRUE );

		pxNetworkBuffer->pxEndPoint = &( xEndPoints[ 0 ] );
		pxNetworkBuffer->pxInterface = pxNetworkBuffer->pxEndPoint->pxNetworkInterface;

		trap_rx_event = 1;

		xIPStackEvent.pvData = ( void * ) pxNetworkBuffer;
		xIPStackEvent.eEventType = ( xSendRequest != 0 ) ? eNetworkTxEvent : eNetworkRxEvent;

		xSendEventStructToIPTask( &( xIPStackEvent ), ( TickType_t ) 1000U );

		vTaskDelay( 100U );
	}
#endif /* ( ipconfigUSE_IPv6 != 0 ) */


#if ( ipconfigUSE_IPv6 != 0 )

	#ifndef ipIPv6_EXT_HEADER_HOP_BY_HOP
		#define ipIPv6_EXT_HEADER_HOP_BY_HOP             0U
		#define ipIPv6_EXT_HEADER_DESTINATION_OPTIONS    60U
		#define ipIPv6_EXT_HEADER_ROUTING_HEADER         43U
		#define ipIPv6_EXT_HEADER_FRAGMENT_HEADER        44U
		#define ipIPv6_EXT_HEADER_AUTHEN_HEADER          51U
		#define ipIPv6_EXT_HEADER_SECURE_PAYLOAD         50U
		/* Destination options may follow here in case there are no routing options. */
		#define ipIPv6_EXT_HEADER_MOBILITY_HEADER        135U
	#endif

	/* IPv6 Extension Headers test */
	void ICMPEchoTest_v6( size_t uxOptionCount,
						  BaseType_t xSendRequest )
	{
		const size_t uxOptionsLength = ( 8U * uxOptionCount );
		const size_t uxEchoSize = 32U;

		/* 14 + 40 + (4 * 8) + 8 + 32 = 126 */
		const size_t uxRequestedSizeBytes =
			ipSIZE_OF_ETH_HEADER + ipSIZE_OF_IPv6_HEADER + uxOptionsLength +
			sizeof( ICMPEcho_IPv6_t ) + uxEchoSize;

		const size_t uxIPLength = ipSIZE_OF_ETH_HEADER + ipSIZE_OF_IPv6_HEADER;
		uint8_t * pucTarget;
		static const uint8_t macLaptop[ ipMAC_ADDRESS_LENGTH_BYTES ] = { 0x9c, 0x5c, 0x8e, 0x38, 0x06, 0x6c };
		static const uint8_t macMe[ ipMAC_ADDRESS_LENGTH_BYTES ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };
		IPStackEvent_t xIPStackEvent;
		NetworkBufferDescriptor_t * pxNetworkBuffer;
		IPPacket_IPv6_t * pxIPPacket;
		static uint16_t usIdentifier = 1;
		static uint16_t usSequenceNumber = 100;
		ICMPEcho_IPv6_t * pxICMPHeader;
		uint16_t usPayloadLength;
		uint32_t uxPayloadLength;
		char * pcUserMessage;

		pxNetworkBuffer = pxGetNetworkBufferWithDescriptor( uxRequestedSizeBytes, 1000U );

		FreeRTOS_printf( ( "uxIPLength: %u  uxOptionsLength: %u Total: %u\n",
						   ( unsigned ) uxIPLength, ( unsigned ) uxOptionsLength, ( unsigned ) uxRequestedSizeBytes ) );

		if( pxNetworkBuffer == NULL )
		{
			FreeRTOS_printf( ( "pxGetNetworkBufferWithDescriptor: failed\n" ) );
			return;
		}

		pxIPPacket = ( IPPacket_IPv6_t * ) pxNetworkBuffer->pucEthernetBuffer;

		( void ) memset( pxNetworkBuffer->pucEthernetBuffer, 0, pxNetworkBuffer->xDataLength );

		/* pucTarget points to the first byte after the IP-header. */
		pucTarget = &( pxNetworkBuffer->pucEthernetBuffer[ uxIPLength ] );

		pxIPPacket->xIPHeader.ucVersionTrafficClass = ipTYPE_IPv6; /* 0x60 */
		pxIPPacket->xIPHeader.ucTrafficClassFlow = 0U;
		pxIPPacket->xIPHeader.usFlowLabel = 0U;                    /**< Flow label.                             2 +  2 =  4 */

		uxPayloadLength = uxEchoSize + sizeof( ICMPEcho_IPv6_t );
		usPayloadLength = ( uint16_t ) uxPayloadLength;
		pxIPPacket->xIPHeader.usPayloadLength = FreeRTOS_htons( usPayloadLength );
		pxIPPacket->xIPHeader.ucNextHeader = ipPROTOCOL_ICMP_IPv6;
		pxIPPacket->xIPHeader.ucHopLimit = 128;                 /**< Replaces the time to live from IPv4.    7 +  1 =  8 */

		if( uxOptionsLength != 0U )
		{
			size_t uxOptionIndex;

			pxIPPacket->xIPHeader.ucNextHeader = ipIPv6_EXT_HEADER_HOP_BY_HOP;

			for( uxOptionIndex = uxOptionCount; uxOptionIndex > 0U; uxOptionIndex-- )
			{
				uint8_t ucNextHeader = 0U;

				switch( uxOptionIndex )
				{
					case 4:
						ucNextHeader = ipIPv6_EXT_HEADER_DESTINATION_OPTIONS;
						break;

					case 3:
						ucNextHeader = ipIPv6_EXT_HEADER_ROUTING_HEADER;
						break;

					case 2:
						ucNextHeader = ipIPv6_EXT_HEADER_FRAGMENT_HEADER;
						break;

					case 1:
						ucNextHeader = ipPROTOCOL_ICMP_IPv6;
						break;
				}

				pucTarget[ 0 ] = ucNextHeader;
				pucTarget[ 1 ] = 0;

				pucTarget = pucTarget + 8;
			}
		}

		pxICMPHeader = ( ICMPEcho_IPv6_t * ) pucTarget;
		pxICMPHeader->usIdentifier = FreeRTOS_htons( usIdentifier );
		pxICMPHeader->usSequenceNumber = FreeRTOS_htons( usSequenceNumber );
		usIdentifier++;
		usSequenceNumber++;
		pxICMPHeader->ucTypeOfMessage = ipICMP_PING_REQUEST_IPv6;
		pxICMPHeader->ucTypeOfService = 0;

		pcUserMessage = ( char * ) &( pucTarget[ sizeof( ICMPEcho_IPv6_t ) ] );
		memcpy( pcUserMessage, "abcdefghijklmnopqrstuvwabcdefghi", uxEchoSize );

		/*pxNetworkBuffer->xDataLength += uxRequestedSizeBytes; */

		usPayloadLength = FreeRTOS_ntohs( pxIPPacket->xIPHeader.usPayloadLength );
		usPayloadLength += uxOptionsLength;
		pxIPPacket->xIPHeader.usPayloadLength = FreeRTOS_htons( usPayloadLength );

		if( xSendRequest == pdTRUE )
		{
			FreeRTOS_inet_pton6( "fe80::7007", pxIPPacket->xIPHeader.xSourceAddress.ucBytes );
			FreeRTOS_inet_pton6( "fe80::6816:5e9b:80a0:9edb", pxIPPacket->xIPHeader.xDestinationAddress.ucBytes );
			memcpy( pxIPPacket->xEthernetHeader.xSourceAddress.ucBytes, macMe, ipMAC_ADDRESS_LENGTH_BYTES );
			memcpy( pxIPPacket->xEthernetHeader.xDestinationAddress.ucBytes, macLaptop, ipMAC_ADDRESS_LENGTH_BYTES );
		}
		else
		{
			FreeRTOS_inet_pton6( "fe80::7007", pxIPPacket->xIPHeader.xDestinationAddress.ucBytes );
			FreeRTOS_inet_pton6( "fe80::6816:5e9b:80a0:9edb", pxIPPacket->xIPHeader.xSourceAddress.ucBytes );
			memcpy( pxIPPacket->xEthernetHeader.xSourceAddress.ucBytes, macLaptop, ipMAC_ADDRESS_LENGTH_BYTES );
			memcpy( pxIPPacket->xEthernetHeader.xDestinationAddress.ucBytes, macMe, ipMAC_ADDRESS_LENGTH_BYTES );
		}

		pxIPPacket->xEthernetHeader.usFrameType = ipIPv6_FRAME_TYPE;

		#if ( ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM == 0 )
			{
				usGenerateProtocolChecksum( ( uint8_t * ) pxNetworkBuffer->pucEthernetBuffer, uxRequestedSizeBytes, pdTRUE );
			}
		#endif

		NetworkEndPoint_t * pxMyEndPoint = &( xEndPoints[ 1 ] );
		pxNetworkBuffer->pxEndPoint = pxMyEndPoint;
		pxNetworkBuffer->pxInterface = pxMyEndPoint->pxNetworkInterface;

		trap_rx_event = 1;

		xIPStackEvent.pvData = ( void * ) pxNetworkBuffer;
		xIPStackEvent.eEventType = ( xSendRequest == pdTRUE ) ? eNetworkTxEvent : eNetworkRxEvent;

		xSendEventStructToIPTask( &( xIPStackEvent ), ( TickType_t ) 1000U );

		vTaskDelay( 100U );
	}
#endif /* ( ipconfigUSE_IPv6 != 0 ) */

void dumpHexData( const unsigned char * apBuf,
				  unsigned aOffset,
				  int aLen )
{
	const unsigned char * source = apBuf;
	char hexLine[ 129 ];
	int hexLen = 0;
	char ascLine[ 19 ];
	int ascLen = 0;
	char hasNL = pdTRUE;

	for( int index = 0; index < aLen; )
	{
		if( ( index % 16 ) == 0 )
		{
			char empty = pdTRUE;

			for( int i = 0; i < 16; i++ )
			{
				if( source[ i ] != 0 )
				{
					empty = pdFALSE;
					break;
				}
			}

			if( ( index >= 512 ) && empty )
			{
				index += 16;
				source += 16;

				if( !hasNL )
				{
					FreeRTOS_printf( ( "-\n" ) );
					hasNL = pdTRUE;
					/*flushTcpLogging (); */
				}

				continue;
			}
		}

		hasNL = pdFALSE;
		unsigned char ch = *( source++ );
		hexLen += snprintf( hexLine + hexLen, sizeof hexLine - hexLen, "%02X ", ch );

		if( ( ch >= 32 ) && ( ch < 128 ) )
		{
			ascLen += snprintf( ascLine + ascLen, sizeof ascLine - ascLen, "%c", ch );
		}
		else
		{
			ascLen += snprintf( ascLine + ascLen, sizeof ascLine - ascLen, "." );
		}

		if( ( ( index + 1 ) % 16 ) == 0 )
		{
			FreeRTOS_printf( ( "%06X  %-50.50s  %s\n", aOffset + index - 15, hexLine, ascLine ) );
			hexLine[ 0 ] = '\0';
			hexLen = 0;
			ascLine[ 0 ] = '\0';
			ascLen = 0;
			/*flushTcpLogging (); */
		}

		index++;
	}
}

#if ( HAS_ARP_TEST != 0 )
	typedef struct xTimer
	{
		TickType_t uxStartTime, uxDeltaTime;
	} Timer_t;

	static void timer_start( Timer_t * pxTime )
	{
		pxTime->uxStartTime = xTaskGetTickCount();
	}
/*-----------------------------------------------------------*/

	static TickType_t timer_end( Timer_t * pxTime )
	{
		pxTime->uxDeltaTime = xTaskGetTickCount() - pxTime->uxStartTime;
		return pxTime->uxDeltaTime;
	}
/*-----------------------------------------------------------*/

	static void flush_logging( void )
	{
		BaseType_t xIndex;

		for( xIndex = 0; xIndex < 3; xIndex++ )
		{
			vTaskDelay( 100U );
		}
	}
/*-----------------------------------------------------------*/

	extern ARPCacheRow_t xARPCache[ ipconfigARP_CACHE_ENTRIES ];
	static uint32_t ulUnknownIP = FreeRTOS_inet_addr_quick( 192, 168, 2, 254 );
	static uint32_t ulKnownIP = FreeRTOS_inet_addr_quick( 192, 168, 2, 18 );
	static uint32_t ulRemoteIP = FreeRTOS_inet_addr_quick( 80, 80, 2, 18 );
	static uint8_t pucKnownMAC[ ipMAC_ADDRESS_LENGTH_BYTES ] = { 0x9c, 0x5c, 0x8e, 0x38, 0x06, 0x6c };

	extern BaseType_t xCacheLookupCount;

	static void fill_arp()
	{
		BaseType_t xIndex;
		uint32_t ulIPAddress = FreeRTOS_inet_addr_quick( 10, 1, 1, 1 );

		memset( xARPCache, 0, sizeof xARPCache );

		for( xIndex = 0; xIndex < ARRAY_SIZE( xARPCache ); xIndex++ )
		{
			xARPCache[ xIndex ].ucAge = 10U;
			xARPCache[ xIndex ].ucValid = ( uint8_t ) pdTRUE;

			if( xIndex == 31 )
			{
				xARPCache[ xIndex ].ulIPAddress = ulKnownIP;     /**< The IP address of an ARP cache entry. */
				memcpy( xARPCache[ xIndex ].xMACAddress.ucBytes, pucKnownMAC, ipMAC_ADDRESS_LENGTH_BYTES );
			}
			else if( xIndex == ARRAY_SIZE( xARPCache ) - 1 )
			{
				memset( &( xARPCache[ xIndex ] ), 0, sizeof xARPCache[ 0 ] );
			}
			else
			{
				xARPCache[ xIndex ].ulIPAddress = ulIPAddress;     /**< The IP address of an ARP cache entry. */
				ulIPAddress++;
			}
		}
	}
/*-----------------------------------------------------------*/

	void arp_test( void )
	{
		BaseType_t xIndex;
		BaseType_t xType;
		BaseType_t xLoopCount = 10000;
		MACAddress_t xMACAddress;
		Timer_t xTime;

		fill_arp();

		for( xType = 0; xType < 3; xType++ )
		{
			uint32_t ulIPToLookUp = 0U;
			const char * pcTypeName = "?";

			switch( xType )
			{
				case 0:
					ulIPToLookUp = ulKnownIP;
					pcTypeName = "Known";
					break;

				case 1:
					ulIPToLookUp = ulUnknownIP;
					pcTypeName = "Unknown";
					break;

				case 2:
					ulIPToLookUp = ulRemoteIP;
					pcTypeName = "Remote";
					break;
			}

			{
				timer_start( &xTime );

				for( xIndex = 0; xIndex < xLoopCount; xIndex++ )
				{
					uint32_t ulIPAddress = ulIPToLookUp;
					xCacheLookupCount = 0;
					eARPGetCacheEntry( &( ulIPAddress ), &( xMACAddress ) );
				}

				timer_end( &xTime );
			}
			uint32_t ulAverage = ( uint32_t ) ( ( ( uint64_t ) 1000ULL * xTime.uxDeltaTime ) / ( xLoopCount ) );
			FreeRTOS_printf( ( "Looking up %-7.7s %d times: %u or %u us (%d)\n",
							   pcTypeName,
							   ( int ) xLoopCount,
							   ( unsigned ) xTime.uxDeltaTime,
							   ( unsigned ) ulAverage,
							   ( int ) xCacheLookupCount ) );
			flush_logging();
		}
	}
/*-----------------------------------------------------------*/

#endif /* ( HAS_ARP_TEST != 0 ) */

BaseType_t xPacketBouncedBack( uint8_t * pck )
{
	( void ) pck;
	return pdFALSE;
}


#if ( ipconfigMULTI_INTERFACE != 0 )

    void show_single_addressinfo( const char * pcFormat,
                                  const struct freertos_addrinfo * pxAddress )
    {
        char cBuffer[ 40 ];
        const uint8_t * pucAddress;

        #if ( ipconfigUSE_IPv6 != 0 )
            if( pxAddress->ai_family == FREERTOS_AF_INET6 )
            {
                pucAddress = pxAddress->ai_addr->sin_address.xIP_IPv6.ucBytes;
            }
            else
        #endif /* ( ipconfigUSE_IPv6 != 0 ) */
        {
            pucAddress = ( const uint8_t * ) &( pxAddress->ai_addr->sin_address.ulIP_IPv4 );
        }

        ( void ) FreeRTOS_inet_ntop( pxAddress->ai_family, ( const void * ) pucAddress, cBuffer, sizeof( cBuffer ) );

        if( pcFormat != NULL )
        {
            FreeRTOS_printf( ( pcFormat, cBuffer ) );
        }
        else
        {
            FreeRTOS_printf( ( "Address: %s\n", cBuffer ) );
        }
    }
/*-----------------------------------------------------------*/

/**
 * @brief For testing purposes: print a list of DNS replies.
 * @param[in] pxAddress: The first reply received ( or NULL )
 */
    void show_addressinfo( const struct freertos_addrinfo * pxAddress )
    {
        const struct freertos_addrinfo * ptr = pxAddress;
        BaseType_t xIndex = 0;

        while( ptr != NULL )
        {
            show_single_addressinfo( "Found Address: %s", ptr );

            ptr = ptr->ai_next;
        }

        /* In case the function 'FreeRTOS_printf()` is not implemented. */
        ( void ) xIndex;
    }

    #if ( ipconfigCOMPATIBLE_WITH_SINGLE == 1 ) && ( ipAFTER_INTEGRATION == 1 )
        void FreeRTOS_GetAddressConfiguration( uint32_t * pulIPAddress,
                                               uint32_t * pulNetMask,
                                               uint32_t * pulGatewayAddress,
                                               uint32_t * pulDNSServerAddress )
        {
            struct xNetworkEndPoint * pxEndPoint = FreeRTOS_FirstEndPoint( NULL );

            FreeRTOS_GetEndPointConfiguration( pulIPAddress, pulNetMask, pulGatewayAddress, pulDNSServerAddress, pxEndPoint );
        }
    #endif /* ( ipconfigCOMPATIBLE_WITH_SINGLE ) */

#endif /* if ( ipconfigMULTI_INTERFACE != 0 ) */

void start_RNG( void )
{
    uint32_t ulSeed = 0x5a5a5a5a;
	/* Enable the clock for the RNG. */
	__HAL_RCC_RNG_CLK_ENABLE();
	RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;
	RNG->CR |= RNG_CR_RNGEN;

	/* Set the Instance pointer. */
	hrng.Instance = RNG;
	/* Initialise it. */
	HAL_RNG_Init( &hrng );
	/* Get a random number. */
	HAL_RNG_GenerateRandomNumber( &hrng, &ulSeed );
	/* And pass it to the rand() function. */
/*		vSRand( ulSeed ); */
/*		hrng.Instance = RNG; */
/*		HAL_RNG_Init( &hrng ); */
	xRandom32( &ulSeed );
	prvSRand( ulSeed );
	ulInitialSeed = ulSeed;
}


/*
 *  A set of stdio functions.
 */
int	vfprintf_r (struct _reent *apReent, FILE *apFile, const char *apFmt, va_list args)
{
	(void)apReent;
	(void)apFile;
	(void)apFmt;
	(void)args;
	return 0;
}

int	fclose_r (struct _reent *apReent, FILE *apFile)
{
	(void)apReent;
	(void)apFile;
	return 0;
}

_ssize_t _write_r (struct _reent *apReent, int aHandle, const void *ap, size_t aSize)
{
	(void)apReent;
	(void)aHandle;
	(void)ap;
	(void)aSize;
	return 0;
}

int _close_r (struct _reent *apReent, int aHandle)
{
	(void)apReent;
	(void)aHandle;
	return 0;
}

_ssize_t _read_r (struct _reent *apReent, int aHandle, void *apPtr, size_t aSize)
{
	(void)apReent;
	(void)aHandle;
	(void)apPtr;
	(void)aSize;
	return 0;
}

_off_t _lseek_r (struct _reent *apReent, int aHandle, _off_t aOffset, int aWhence)
{
	(void)apReent;
	(void)aHandle;
	(void)aOffset;
	(void)aWhence;
	return 0;
}

int	fclose (FILE *apFile)
{
	(void)apFile;
	configASSERT( 0 );
	return 0;
}

int	_fclose_r (struct _reent *apReent, FILE *apFile)
{
	(void)apReent;
	(void)apFile;
	configASSERT( 0 );
	return 0;
}

void exit(int status)
{
	configASSERT( 0 );
	( void ) status;
	for( ;; )
	{
	}
}

void __libc_fini_array (void)
{
}

#include <sys/stat.h>

int _stat(const char fname, struct stat *pstat)
{
	( void ) fname;
	( void ) pstat;
	configASSERT( 0 );
	return 0;
}

int _fstat_r(struct _reent *ptr, int fd, struct stat *pstat)
{
	( void ) ptr;
	( void ) fd;
	( void ) pstat;
	configASSERT( 0 );
	return 0;
}

int _isatty_r(struct _reent *ptr, int fd)
{
	( void ) ptr;
	( void ) fd;
	configASSERT( 0 );
	return 0;
}
