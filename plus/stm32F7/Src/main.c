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

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_TCP_server.h"
#include "FreeRTOS_DHCP.h"
#include "tcp_mem_stats.h"
#include "NetworkBufferManagement.h"
#include "NetworkInterface.h"
#include "FreeRTOS_IP_Private.h"
#if( ipconfigMULTI_INTERFACE != 0 )
	#include "FreeRTOS_Routing.h"
	#include "FreeRTOS_ND.h"
#endif

/* FreeRTOS+FAT includes. */
#include "ff_headers.h"
#include "ff_stdio.h"
#include "ff_ramdisk.h"
#include "ff_sddisk.h"

#define EXTERN_C  extern

#if USE_LOG_EVENT
	#include "eventLogging.h"
#endif

/* Demo application includes. */
#include "hr_gettime.h"

#include "UDPLoggingPrintf.h"

#if( USE_IPERF != 0 )
	#include "iperf_task.h"
#endif

//#define __STM32_HAL_LEGACY   1
#include "date_and_time.h"

/* ST includes. */
#include "stm32f7xx_hal.h"
#include "stm32fxx_hal_eth.h"

#if( USE_TCP_DEMO_CLI != 0 )
	#include "plus_tcp_demo_cli.h"
#endif

#if( HAS_AUDIO != 0 )
	#include "playerRecorder.h"
#endif

/* SDRAM on STM32F746g discovery_sdram */
/* See Framework/mysource/stm32746g_discovery_sdram.c */
#define SDRAM_START  0xC0000000U
#define SDRAM_SIZE  ( 8U * 1024U * 1024U )

/* The following function is called before FreeRTOS has started.
 * It loops for a certain amount of msec. */
EXTERN_C void BM_Delay( uint32_t ulDelayMS );


//#if defined( MBEDTLS_PLATFORM_TIME_TYPE_MACRO )
//	#error 1
//#else
//	#error 2
//#endif
//
//#if defined( MBEDTLS_PLATFORM_TIME_ALT )
//	#error 3
//#else
//	#error 4
//#endif
//
//#if defined( MBEDTLS_PLATFORM_TIME_MACRO )
//	#error 5
//#else
//	#error 6
//#endif

#if( ipconfigMULTI_INTERFACE != 0 )
	void showEndPoint( NetworkEndPoint_t *pxEndPoint );
#else
	#ifndef ipSIZE_OF_IPv4_ADDRESS
		#define ipSIZE_OF_IPv4_ADDRESS		4
	#endif
	#define FREERTOS_AF_INET4			FREERTOS_AF_INET
#endif

#if( HAS_ECHO_TEST != 0 )
	/* The CLI command "plus" will start an echo server. */
	#include "plus_echo_server.h"
#endif

//#include "cmsis_os.h"
/* FTP and HTTP servers execute in the TCP server work task. */
#define mainTCP_SERVER_TASK_PRIORITY	( tskIDLE_PRIORITY + 3 )
#define	mainTCP_SERVER_STACK_SIZE		2048
#define	mainFAT_SERVER_STACK_SIZE		( 1024U )

/* Some macros related to +FAT testing. */
#define	mainFAT_TEST_STACK_SIZE			2048
#define mainRUN_STDIO_TESTS				0
#define mainHAS_RAMDISK					0
#define mainRAM_DISK_NAME               "/ram"
#define mainHAS_SDCARD					1
#define mainSD_CARD_DISK_NAME			"/"
#define mainSD_CARD_TESTING_DIRECTORY	"/fattest"

/* Remove this define if you don't want to include the +FAT test code. */
#define mainHAS_FAT_TEST				1

/* Set the following constants to 1 to include the relevant server, or 0 to
exclude the relevant server. */
#define mainCREATE_FTP_SERVER			1
#define mainCREATE_HTTP_SERVER 			1

/* The function ff_mkdir() may have a second parameter which allows
 * recursive creation of directories. */
#if( ffconfigMKDIR_RECURSIVE != 0 )
	#define MKDIR_RECURSE  , pdFALSE
#else
	#define MKDIR_RECURSE
#endif


/* Define names that will be used for DNS, LLMNR and NBNS searches. */
#define mainHOST_NAME					"stm"
#define mainDEVICE_NICK_NAME			"stm32f7"


#ifdef HAL_WWDG_MODULE_ENABLED
	#warning Why enable HAL_WWDG_MODULE_ENABLED
#endif

#if( ipconfigUSE_DNS_CACHE == 0 )
	#warning Please enable ipconfigUSE_DNS_CACHE
#endif

#ifndef GPIO_SPEED_LOW
    /* Not all versions of the HAL have these macros defines. */
	#define  GPIO_SPEED_LOW         ((uint32_t)0x00000000)  /*!< Low speed     */
	#define  GPIO_SPEED_MEDIUM      ((uint32_t)0x00000001)  /*!< Medium speed  */
	#define  GPIO_SPEED_FAST        ((uint32_t)0x00000002)  /*!< Fast speed    */
	#define  GPIO_SPEED_HIGH        ((uint32_t)0x00000003)  /*!< High speed    */
#endif

#define logPrintf  lUDPLoggingPrintf

/* Random Number Generator. */
static RNG_HandleTypeDef hrng;

int verboseLevel;

typedef enum {
	eIPv4 = 0x01,
	eIPv6 = 0x02,
} eIPVersion;

/* This variable is allowed for testing mDNS/LLMNR requests
 * in all combinations. */
eIPVersion allowIPVersion = ( eIPVersion ) ( ( uint8_t ) eIPv4 | ( uint8_t ) eIPv6 );

FF_Disk_t *pxSDDisk;
static FF_Disk_t *pxRAMDisk;

static TCPServer_t *pxTCPServer = NULL;

enum EStartupPhase {
    EStartupWait,
	EStartupStart,
	EStartupStarted
};

static volatile enum EStartupPhase xStartServices = EStartupWait;

#ifndef ARRAY_SIZE
#	define	ARRAY_SIZE( x )	( int )( sizeof( x ) / sizeof( x )[ 0 ] )
#endif

SemaphoreHandle_t xServerSemaphore;

/* Private function prototypes -----------------------------------------------*/

/*
 * Creates a RAM disk, then creates files on the RAM disk.  The files can then
 * be viewed via the FTP server and the command line interface.
 */
static void prvCreateDiskAndExampleFiles( void );

EXTERN_C void vStdioWithCWDTest( const char *pcMountPath );
EXTERN_C void vCreateAndVerifyExampleFiles( const char *pcMountPath );

static void check_disk_presence(void);

/*
 * The task that runs the FTP and HTTP servers.
 */
static void prvServerWorkTask( void *pvParameters );
static BaseType_t vHandleOtherCommand( Socket_t xSocket, char *pcBuffer );

/*
 * Configure the hardware as necessary to run this demo.
 */
static void prvSetupHardware( void );

/*
 * Configure the system clock for maximum speed.
 */
static void prvSystemClockConfig( void );

static void heapInit( void );

void vShowTaskTable( BaseType_t aDoClear );
void vIPerfInstall( void );

void tcpServerStart();
void tcpServerWork();

uint32_t ulServerIPAddress;

/* Default MAC address configuration.  The demo creates a virtual network
connection that uses this MAC address by accessing the raw Ethernet data
to and from a real network connection on the host PC.  See the
configNETWORK_INTERFACE_TO_USE definition for information on how to configure
the real network connection to use. */
#if( ipconfigMULTI_INTERFACE == 0 )
	const uint8_t ucMACAddress[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };
#else
	static const uint8_t ucMACAddress1[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };
#endif /* ipconfigMULTI_INTERFACE */

/* Handle of the task that runs the FTP and HTTP servers. */
static TaskHandle_t xServerWorkTaskHandle = NULL;
static TaskHandle_t xFTPWorkTaskHandle = NULL;
static void prvFTPTask (void *pvParameters );

#if( ipconfigMULTI_INTERFACE != 0 )
	NetworkInterface_t *pxSTM32Fxx_FillInterfaceDescriptor( BaseType_t xEMACIndex, NetworkInterface_t *pxInterface );
	NetworkInterface_t *pxLoopback_FillInterfaceDescriptor( BaseType_t xEMACIndex, NetworkInterface_t *pxInterface );
#endif /* ipconfigMULTI_INTERFACE */
/*-----------------------------------------------------------*/

#if( ipconfigMULTI_INTERFACE != 0 )
	static NetworkInterface_t xInterfaces[ 2 ];
	static NetworkEndPoint_t xEndPoints[ 4 ];
#endif /* ipconfigMULTI_INTERFACE */

/* Prototypes for the standard FreeRTOS callback/hook functions implemented
within this file. */
EXTERN_C void vApplicationMallocFailedHook( void );
EXTERN_C void vApplicationIdleHook( void );
EXTERN_C void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName );
EXTERN_C void vApplicationTickHook( void );

/*-----------------------------------------------------------*/

EXTERN_C int main( void );
int main( void )
{
uint32_t ulSeed;

	SCB_EnableICache();
	SCB_EnableDCache();

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* Configure the hardware ready to run the demo. */
	prvSetupHardware();

	/* Timer2 initialization function.
	ullGetHighResolutionTime() will be used to get the running time in uS. */
	vStartHighResolutionTimer();

	/* Initialise a heap in SRAM. */
	heapInit( );

#if USE_LOG_EVENT
	iEventLogInit();
#endif

	{
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
	}

	hrng.Instance = RNG;
	HAL_RNG_Init( &hrng );

	/* Initialise the network interface.

	***NOTE*** Tasks that use the network are created in the network event hook
	when the network is connected and ready for use (see the definition of
	vApplicationIPNetworkEventHook() below).  The address values passed in here
	are used if ipconfigUSE_DHCP is set to 0, or if ipconfigUSE_DHCP is set to 1
	but a DHCP server cannot be	contacted. */

#if( ipconfigMULTI_INTERFACE == 0 )
	/* Call it later in a task. */
//	FreeRTOS_IPInit( ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress );
#else
	pxSTM32Fxx_FillInterfaceDescriptor( 0, &( xInterfaces[ 0 ] ) );

	/*
	 * End-point-1  // private + public
	 *     Network: 192.168.2.x/24
	 *     IPv4   : 192.168.2.12
	 *     Gateway: 192.168.2.1 ( NAT router )
	 */
	NetworkEndPoint_t *pxEndPoint_0 = &( xEndPoints[ 0 ] );
	NetworkEndPoint_t *pxEndPoint_1 = &( xEndPoints[ 1 ] );
	NetworkEndPoint_t *pxEndPoint_2 = &( xEndPoints[ 2 ] );
//	NetworkEndPoint_t *7 = &( xEndPoints[ 3 ] );
	{
		const uint8_t ucIPAddress[ 4 ]        = { 192, 168,   2, 127 };
		const uint8_t ucNetMask[ 4 ]          = { 255, 255, 255, 0   };
		const uint8_t ucGatewayAddress[ 4 ]   = { 192, 168,   2,   1 };
		const uint8_t ucDNSServerAddress[ 4 ] = { 118,  98,  44, 100 };

		FreeRTOS_FillEndPoint( &( xInterfaces[0] ), pxEndPoint_0, ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress1 );
		#if( ipconfigUSE_DHCP != 0 )
		{
			pxEndPoint_0->bits.bWantDHCP = pdTRUE;	//	pdTRUE;
		}
		#endif	/* ( ipconfigUSE_DHCP != 0 ) */
	}
#warning please include this end-point again.
//	{
//		const uint8_t ucIPAddress[]        = { 172,  16,   0, 114 };
//		const uint8_t ucNetMask[]          = { 255, 255,   0,   0};
//		const uint8_t ucGatewayAddress[]   = {   0,   0,   0,   0};
//		const uint8_t ucDNSServerAddress[] = {   0,   0,   0,   0};
//		FreeRTOS_FillEndPoint( &( xInterfaces[0] ), pxEndPoint_3, ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress1 );
//	}


	#if( ipconfigUSE_IPv6 != 0 )
	{
		/*
		 * End-point-2  // private
		 *     Network: fe80::/10 (link-local)
		 *     IPv6   : fe80::d80e:95cc:3154:b76a/128
		 *     Gateway: -
		*/
		{
			IPv6_Address_t xIPAddress;
			IPv6_Address_t xPrefix;
			// a573:8::5000:0:d8ff:120
			// a8ff:120::593b:200:d8af:20

			FreeRTOS_inet_pton6( "fe80::", xPrefix.ucBytes );
//			FreeRTOS_inet_pton6( "2001:470:ec54::", xPrefix.ucBytes );

//			Take a random IP-address
//			FreeRTOS_CreateIPv6Address( &xIPAddress, &xPrefix, 64, pdTRUE );

//			Take a fixed IP-address, which is easier for testing.
			//FreeRTOS_inet_pton6( "fe80::b865:6c00:6615:6e50", xIPAddress.ucBytes );
			FreeRTOS_inet_pton6( "fe80::7007", xIPAddress.ucBytes );

			FreeRTOS_FillEndPoint_IPv6( &( xInterfaces[0] ), 
										pxEndPoint_1,
										&( xIPAddress ),
										&( xPrefix ),
										64uL,			/* Prefix length. */
										NULL,			/* No gateway */
										NULL,			/* pxDNSServerAddress: Not used yet. */
										ucMACAddress1 );
		}
		/*
		 * End-point-3  // public
		 *     Network: 2001:470:ec54::/64
		 *     IPv6   : 2001:470:ec54::4514:89d5:4589:8b79/128
		 *     Gateway: fe80::9355:69c7:585a:afe7  // obtained from Router Advertisement
		*/
		{
			IPv6_Address_t xIPAddress;
			IPv6_Address_t xPrefix;
			IPv6_Address_t xGateWay;
			IPv6_Address_t xDNSServer;

			FreeRTOS_inet_pton6( "2001:470:ec54::",           xPrefix.ucBytes );
			FreeRTOS_inet_pton6( "2001:4860:4860::8888",      xDNSServer.ucBytes );			

			FreeRTOS_CreateIPv6Address( &xIPAddress, &xPrefix, 64, pdTRUE );
			FreeRTOS_inet_pton6( "fe80::9355:69c7:585a:afe7", xGateWay.ucBytes );

			FreeRTOS_FillEndPoint_IPv6( &( xInterfaces[0] ), 
										pxEndPoint_2,
										&( xIPAddress ),
										&( xPrefix ),
										64uL,				/* Prefix length. */
										&( xGateWay ),
										&( xDNSServer ),	/* pxDNSServerAddress: Not used yet. */
										ucMACAddress1 );
			#if( ipconfigUSE_RA != 0 )
			{
				pxEndPoint_2->bits.bWantRA = pdTRUE_UNSIGNED;
			}
			#endif /* #if( ipconfigUSE_RA != 0 ) */
			#if( ipconfigUSE_DHCPv6 != 0 )
		{
				pxEndPoint_2->bits.bWantDHCP = pdTRUE;
			}
			#endif	/* ( ipconfigUSE_DHCP != 0 ) */
		}
	}
	#endif

//	FreeRTOS_IPStart();
#endif /* ipconfigMULTI_INTERFACE */

#if( ipconfigMULTI_INTERFACE == 0 )
	const uint8_t ucIPAddress[ 4 ]        = { 192, 168,   2, 127 };
	const uint8_t ucNetMask[ 4 ]          = { 255, 255, 255,   0 };
	const uint8_t ucGatewayAddress[ 4 ]   = { 192, 168,   2,   1 };
	const uint8_t ucDNSServerAddress[ 4 ] = { 118,  98,  44, 100 };
	FreeRTOS_IPInit( ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress );
//	const uint8_t ucZeros[ 4 ] = { 0, 0, 0, 0 };
//	FreeRTOS_IPInit( ucZeros, ucZeros, ucZeros, ucZeros, ucMACAddress );
#else
	FreeRTOS_IPStart();
#endif
	/* Create the task that handles the FTP and HTTP servers.  This will
	initialise the file system then wait for a notification from the network
	event hook before creating the servers.  The task is created at the idle
	priority, and sets itself to mainTCP_SERVER_TASK_PRIORITY after the file
	system has initialised. */
	xTaskCreate( prvServerWorkTask, "SvrWork", mainTCP_SERVER_STACK_SIZE, NULL, tskIDLE_PRIORITY + 1, &xServerWorkTaskHandle );

	/* Start the RTOS scheduler. */
	FreeRTOS_debug_printf( ("vTaskStartScheduler\n") );
	vTaskStartScheduler();

	/* If all is well, the scheduler will now be running, and the following
	line will never be reached.  If the following line does execute, then
	there was insufficient FreeRTOS heap memory available for the idle and/or
	timer tasks	to be created.  See the memory management section on the
	FreeRTOS web site for more details.  http://www.freertos.org/a00111.html */
	for( ;; )
	{
	}
}
/*-----------------------------------------------------------*/

void HAL_RNG_MspInit(RNG_HandleTypeDef *hrng)
{
	( void ) hrng;
//	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;
	__HAL_RCC_RNG_CLK_ENABLE();

//	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
//	PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
//	HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
	/* Enable RNG clock source */
	RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;
	
	/* RNG Peripheral enable */
	RNG->CR |= RNG_CR_RNGEN;

	/*Select PLLQ output as RNG clock source */
//	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RNG;
//	PeriphClkInitStruct.RngClockSelection = RCC_RNGCLKSOURCE_PLL;
//	HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);
}

BaseType_t xApplicationGetRandomNumber( uint32_t *pulValue )
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

static void prvCreateDiskAndExampleFiles( void )
{
	verboseLevel = 0;
	#if( mainHAS_RAMDISK != 0 )
	{
		static uint8_t * pucRAMDiskSpace = NULL;
		if( pucRAMDiskSpace == NULL )
		{
			#define mainRAM_DISK_SECTOR_SIZE	512U
			pucRAMDiskSpace = ( uint8_t * ) malloc( mainRAM_DISK_SECTORS * mainRAM_DISK_SECTOR_SIZE );
			if( pucRAMDiskSpace != NULL )
			{
				FreeRTOS_printf( ( "Create RAM-disk\n" ) );
				/* Create the RAM disk. */
				pxRAMDisk = FF_RAMDiskInit( ( char * ) mainRAM_DISK_NAME, pucRAMDiskSpace, mainRAM_DISK_SECTORS, mainIO_MANAGER_CACHE_SIZE );
				configASSERT( pxRAMDisk );

				/* Print out information on the RAM disk. */
				FF_RAMDiskShowPartition( pxRAMDisk );
			}
		}
	}
	#endif	/* mainHAS_RAMDISK */
	#if( mainHAS_SDCARD != 0 )
	if( pxSDDisk == NULL ) {
		FreeRTOS_printf( ( "Mount SD-card\n" ) );
		/* Create the SD card disk. */
		pxSDDisk = FF_SDDiskInit( mainSD_CARD_DISK_NAME );
		FreeRTOS_printf( ( "Mount SD-card done\n" ) );
		#if( mainRUN_STDIO_TESTS == 1 )
		if( pxSDDisk != NULL )
		{
			/* Remove the base directory again, ready for another loop. */
			ff_deltree( mainSD_CARD_TESTING_DIRECTORY );

			/* Make sure that the testing directory exists. */
			ff_mkdir( mainSD_CARD_TESTING_DIRECTORY MKDIR_RECURSE );

			/* Create a few example files on the disk.  These are not deleted again. */
			vCreateAndVerifyExampleFiles( mainSD_CARD_TESTING_DIRECTORY );

			/* A few sanity checks only - can only be called after 
			vCreateAndVerifyExampleFiles().  NOTE:  The tests take a relatively long 
			time to execute and the FTP and HTTP servers will not start until the 
			tests have completed. */

			vStdioWithCWDTest( mainSD_CARD_TESTING_DIRECTORY );
		}
		#endif

	}
	#endif	/* mainHAS_SDCARD */
}
/*-----------------------------------------------------------*/

#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )

	static int iFATRunning = 0;

	static void prvFileSystemAccessTask( void *pvParameters )
	{
	const char *pcBasePath = ( const char * ) pvParameters;

		while( iFATRunning != 0 )
		{
			ff_deltree( pcBasePath );
			ff_mkdir( pcBasePath MKDIR_RECURSE );
			vCreateAndVerifyExampleFiles( pcBasePath );
			vStdioWithCWDTest( pcBasePath );
		}
		FreeRTOS_printf( ( "%s: done\n", pcBasePath ) );
		vTaskDelete( NULL );
	}

#if( ipconfigUSE_DHCP_HOOK != 0 )
	eDHCPCallbackAnswer_t xApplicationDHCPHook( eDHCPCallbackPhase_t eDHCPPhase, uint32_t ulIPAddress )
	{
	eDHCPCallbackAnswer_t eAnswer = eDHCPContinue;

		const char *name = "Unknown";
		switch( eDHCPPhase )
		{
		case eDHCPPhasePreDiscover:
			{
				name = "Discover";			/* Driver is about to send a DHCP discovery. */
	//#warning Testing here
	//			eAnswer = eDHCPUseDefaults;
			}
			break;
		case eDHCPPhasePreRequest:
			{
				name = "Request";			/* Driver is about to request DHCP an IP address. */
	//#warning Testing here
	//			eAnswer = eDHCPUseDefaults;
			}
			break;
		}
		FreeRTOS_printf( ( "DHCP %s address %lxip\n", name, FreeRTOS_ntohl( ulIPAddress ) ) );
		return eAnswer;
	}
#endif	/* ipconfigUSE_DHCP_HOOK */

	void dnsCallback( const char *pcName, void *pvSearchID, uint32_t ulIPAddress)
	{
		FreeRTOS_printf( ( "DNS callback: %s with ID %4d found on %xip\n",
			pcName,
			( unsigned ) pvSearchID,
			( unsigned ) FreeRTOS_ntohl( ulIPAddress ) ) );
	}

#if( ipconfigUSE_PCAP != 0 )
	#include "win_pcap.h"

	/* SPcap can gather Ethernet packets in RAM and write them to disk. */
	SPcap myPCAP;
	static BaseType_t pcapRunning = pdFALSE;
	static char pcPCAPFname[ 80 ];
	TickType_t xPCAPStartTime;
	unsigned xPCAPMaxTime;
	#define PCAP_SIZE    ( 1024U )

	static void pcap_check( void )
	{
		if( pcapRunning )
		{
			TickType_t xNow = xTaskGetTickCount ();
			TickType_t xDiff = ( xNow - xPCAPStartTime ) / configTICK_RATE_HZ;
			BaseType_t xTimeIsUp = ( ( xPCAPMaxTime > 0U ) && ( xDiff > xPCAPMaxTime ) ) ? 1 : 0;
			if( xPCAPStartTime == 1 )
			{
				xTimeIsUp = 1;
			}
			if( xTimeIsUp != 0 )
			{
				FreeRTOS_printf( ( "PCAP time is up\n" ) );
			}
			if( ( xTimeIsUp != 0 ) || ( pcap_has_space( &( myPCAP ) ) < 2048 ) )
			{
				pcap_toFile( &( myPCAP ), pcPCAPFname );
				pcap_clear( &( myPCAP ) );
				pcapRunning = pdFALSE;
				FreeRTOS_printf( ( "PCAP ready with '%s'\n", pcPCAPFname ) );
			}
		}
	}

	void pcap_store( uint8_t * pucBytes, size_t uxLength )
	{
		if( pcapRunning )
		{
			IPPacket_t * pxIPPacket = ( IPPacket_t * ) pucBytes;
			IPHeader_t * pxIPHeader = &( pxIPPacket->xIPHeader );
			uint8_t ucProtocol = pxIPPacket->xIPHeader.ucProtocol;
			if( pxIPPacket->xIPHeader.ulSourceIPAddress == FreeRTOS_GetIPAddress())
			{
				switch( ucProtocol )
				{
				case ipPROTOCOL_ICMP:
				case ipPROTOCOL_UDP:
				case ipPROTOCOL_TCP:
                    pxIPHeader->usHeaderChecksum = 0x00U;
                    pxIPHeader->usHeaderChecksum = usGenerateChecksum( 0U, ( uint8_t * ) &( pxIPHeader->ucVersionHeaderLength ), ipSIZE_OF_IPv4_HEADER );
                    pxIPHeader->usHeaderChecksum = ~FreeRTOS_htons( pxIPHeader->usHeaderChecksum );

					usGenerateProtocolChecksum( pucBytes, uxLength, pdTRUE );
					break;
				}
			}
			pcap_write( &myPCAP, pucBytes, ( int ) uxLength );
		}
	}

	static void pcap_command( char *pcBuffer )
	{
		char *ptr = pcBuffer;
		while( isspace( *ptr ) ) ptr++;
		if( *ptr == 0 )
		{
			FreeRTOS_printf( ( "Usage: pcap /test.pcap 1024 100\n" ) );
			FreeRTOS_printf( ( "Which records 1024 KB\n" ) );
			FreeRTOS_printf( ( "Lasting at most 100 seconds\n" ) );
			return;
		}
		if( strncmp( ptr, "stop", 4 )  == 0 )
		{
			if( pcapRunning )
			{
				FreeRTOS_printf( ( "Trying to stop PCAP\n" ) );
				xPCAPStartTime = 1U;
				pcap_check();
			}
			else
			{
				FreeRTOS_printf( ( "PCAP was not running\n" ) );
			}
		}
		else if( pcapRunning )
		{
			FreeRTOS_printf( ( "PCAP is still running\n" ) );
		}
		else
		{
			xPCAPMaxTime = 0U;
			const char *fname = ptr;
			unsigned length = PCAP_SIZE;
			while( *ptr && !isspace( *ptr ) )
			{
				ptr++;
			}
			while( isspace( *ptr ) )
			{
				*ptr = 0;
				ptr++;
			}
			if( isdigit( *ptr ) )
			{
				if( sscanf( ptr, "%u", &length) > 0 )
				{
					while( *ptr && !isspace( *ptr ) )
					{
						ptr++;
					}
					while( isspace( *ptr ) ) ptr++;
					sscanf( ptr, "%u", &xPCAPMaxTime);
				}
			}
			snprintf(pcPCAPFname, sizeof pcPCAPFname, "%s", fname);

			if( pcap_init( &( myPCAP ), length * 1024U ) == pdTRUE )
			{
				xPCAPStartTime = xTaskGetTickCount ();
				pcapRunning = pdTRUE;
				FreeRTOS_printf( ( "PCAP recording %u KB to '%s' for %u sec\n",
					length * 1024U, pcPCAPFname, xPCAPMaxTime ) );
			}
		}
	}

#endif

	static void prvServerWorkTask( void *pvParameters )
	{
	const TickType_t xInitialBlockTime = pdMS_TO_TICKS( 10UL );

	/* A structure that defines the servers to be created.  Which servers are
	included in the structure depends on the mainCREATE_HTTP_SERVER and
	mainCREATE_FTP_SERVER settings at the top of this file. */
	static const struct xSERVER_CONFIG xServerConfiguration[] =
	{
		#if( mainCREATE_HTTP_SERVER == 1 )
				/* Server type,		port number,	backlog, 	root dir. */
				{ eSERVER_HTTP, 	80, 			12, 		configHTTP_ROOT },
		#endif

		#if( mainCREATE_FTP_SERVER == 1 )
				/* Server type,		port number,	backlog, 	root dir. */
				{ eSERVER_FTP,  	21, 			12, 		"" }
		#endif
	};

	BaseType_t xHadSocket = pdFALSE;
	BaseType_t xHasBlocked = pdFALSE;

		/* Remove compiler warning about unused parameter. */
		( void ) pvParameters;

		/* Wait until the network is up before creating the servers.  The
		notification is given from the network event hook. */
		ulTaskNotifyTake( pdTRUE, portMAX_DELAY );

		FreeRTOS_printf( ( "Creating files\n" ) );

		/* Create the disk used by the FTP and HTTP servers. */
		prvCreateDiskAndExampleFiles();
		BaseType_t xIsPresent = FF_SDDiskDetect( pxSDDisk );
		FreeRTOS_printf( ( "FF_SDDiskDetect returns -> %d\n", ( int ) xIsPresent  ) );

		/* The priority of this task can be raised now the disk has been
		initialised. */
		vTaskPrioritySet( NULL, mainTCP_SERVER_TASK_PRIORITY );

		/* Wait until the network is up before creating the servers.  The
		notification is given from the network event hook. */
//		ulTaskNotifyTake( pdTRUE, portMAX_DELAY );

		/* Create the servers defined by the xServerConfiguration array above. */
		pxTCPServer = FreeRTOS_CreateTCPServer( xServerConfiguration, sizeof( xServerConfiguration ) / sizeof( xServerConfiguration[ 0 ] ) );
		configASSERT( pxTCPServer );

		#if( CONFIG_USE_LWIP != 0 )
		{
			/* Earlier testing with a dual-stack. */
			vLWIP_Init();
		}
		#endif
		xServerSemaphore = xSemaphoreCreateBinary();
		char pcBuffer[ 64 ];

		for( ;; )
		{
			if (xStartServices == EStartupStart) {
				xStartServices = EStartupStarted;
				strcpy( pcBuffer, "ntp" );
				xHandleTestingCommand( pcBuffer, sizeof pcBuffer );
				xTaskCreate( prvFTPTask, "FTP-task", mainFAT_SERVER_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, &xFTPWorkTaskHandle );
			}
			/* tcpServerWork() is yet another test. */
			tcpServerWork();

			xHasBlocked = pdFALSE;

			#if( configUSE_TIMERS == 0 )
			{
				/* As there is not Timer task, toggle the LED 'manually'. */
//				vParTestToggleLED( mainLED );
				HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_6);
			}
			#endif
			xSocket_t xSocket = xLoggingGetSocket();
			struct freertos_sockaddr xSourceAddress;
			socklen_t xSourceAddressLength = sizeof( xSourceAddress );

			if( !xHasBlocked )
			{
				xSemaphoreTake( xServerSemaphore, 10 );
			}

			if( xSocket != NULL)
			{
			BaseType_t xCount;

			TickType_t xReceiveTimeOut = pdMS_TO_TICKS( 0 );

				#if( ipconfigUSE_PCAP != 0 )
				{
					pcap_check();
				}
				#endif

				#if( USE_TCP_DEMO_CLI != 0 )
				{
					xHandleTesting();
				}
				#endif
				if( xHadSocket == pdFALSE )
				{
					xHadSocket = pdTRUE;
					FreeRTOS_printf( ( "prvCommandTask started. Multi = %d IPv6 = %d\n",
						ipconfigMULTI_INTERFACE,
						ipconfigUSE_IPv6) );

					FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof( xReceiveTimeOut ) );
				}
//				xCount = FreeRTOS_recvfrom( xSocket, ( void * )pcBuffer, sizeof( pcBuffer ) - 1,
//						xHasBlocked ? 0 : FREERTOS_MSG_DONTWAIT, &xSourceAddress, &xSourceAddressLength );
				xCount = FreeRTOS_recvfrom( xSocket, ( void * )pcBuffer, sizeof( pcBuffer ) - 1,
						0, &xSourceAddress, &xSourceAddressLength );
				if( xCount > 0 )
				{
					pcBuffer[ xCount ] = '\0';
					if (pcBuffer[0] == 'f' && isdigit (pcBuffer[1]) && pcBuffer[2] == ' ') {
						// Ignore messages like "f4 dnsq"
						if (pcBuffer[1] != '7')
							continue;
						xCount -= 3;
						// "f7 ver"
						memmove(pcBuffer, pcBuffer+3, xCount + 1);
					}

					#if( USE_TCP_DEMO_CLI != 0 )
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
				}	/* if( xCount > 0 ) */
			}	/* if( xSocket != NULL) */
		}
	}

#endif /* ( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) ) */
/*-----------------------------------------------------------*/

static void check_disk_presence()
{
    /* Check if the SD-card was inserted or extracted. */
	static BaseType_t xWasPresent = pdFALSE;
	BaseType_t xIsPresent = FF_SDDiskDetect( pxSDDisk );
	if (xWasPresent != xIsPresent )
	{
		if( xIsPresent == pdFALSE )
		{
			FreeRTOS_printf( ( "SD-card now removed (%ld -> %ld)\n", xWasPresent, xIsPresent ) );
			if( ( pxSDDisk != NULL ) && ( pxSDDisk->pxIOManager != NULL ) )
			{
				/* Invalidate all open file handles so they
				will get closed by the application. */
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
//				if( xResult > 0 )
//				{
//					FF_SDDiskShowPartition( pxSDDisk );
//				}
			}
			else
			{
				prvCreateDiskAndExampleFiles();
			}
		}
		xWasPresent = xIsPresent;
	}
}

typedef struct xParameters
{
	int parameters[16];
	const char * texts[16];
	int txtCount;
	int parCount;
	int ipCount;
} SParameters;

static int analyseCommand(SParameters *apPars, char *pcBuffer)
{
	const char *ptr = pcBuffer;
	apPars->txtCount = 0;
	apPars->parCount = 0;
	apPars->ipCount = 0;

	memset(apPars->parameters, '\0', sizeof apPars->parameters);
	memset(apPars->texts, '\0', sizeof apPars->texts);

	while (*ptr && !isdigit (*ptr) && *ptr != '-') ptr++;

	while (apPars->parCount < ARRAY_SIZE(apPars->parameters))
	{
		char isHex = pdFALSE;
		char negative = pdFALSE;
		while (*ptr) {
			if (ptr[0] == '0' && ptr[1] == 'x' && isxdigit (ptr[2])) {
				ptr += 2;
				isHex = pdTRUE;
				break;
			}
			if (ptr[0] == '-' && isdigit (ptr[1])) {
				ptr += 1;
				negative = pdTRUE;
				break;
			}
			if (isdigit (*ptr))
				break;
			ptr++;
		}
		if (!*ptr)
			break;
		if (isHex) {
			sscanf (ptr, "%x", &apPars->parameters[apPars->parCount]);
			while (isxdigit (*ptr)) ptr++;
		} else {
			unsigned n;
			sscanf (ptr, "%u", &n);
			if (apPars->ipCount) {
				*((unsigned*)apPars->parameters+apPars->parCount) =
					(((unsigned)apPars->parameters[apPars->parCount]) << 8) | n;
			} else if (negative) {
				apPars->parameters[apPars->parCount] = - ((int)n);
			} else {
				apPars->parameters[apPars->parCount] = n;
			}
			while (isdigit (*ptr)) ptr++;
			if (*ptr == '.') {
				apPars->parCount--;
				apPars->ipCount++;
			} else {
				apPars->ipCount = 0;
			}
		}
		apPars->parCount++;
	}

	for (char *token = strtok(pcBuffer, " ");
		 token != NULL;
		 token = strtok(NULL, " ")) {

		apPars->texts[apPars->txtCount++] = token;
		if (apPars->txtCount >= ARRAY_SIZE(apPars->texts))
			break;
	}
	while (apPars->txtCount < ARRAY_SIZE(apPars->texts)) {
		static const char zeroString[1] = { 0 };
		apPars->texts[apPars->txtCount++] = zeroString;
	}
	return apPars->parCount;
}

static BaseType_t vHandleOtherCommand( Socket_t xSocket, char *pcBuffer )
{
	/* Assume that the command will be handled. */
    BaseType_t xReturn = pdTRUE;

	( void ) xSocket;

	SParameters pars;
	analyseCommand(&pars, pcBuffer);

	if( strncmp( pcBuffer, "ver", 3 ) == 0 )
	{
	int level;
	TickType_t xCount = xTaskGetTickCount();
		if( sscanf( pcBuffer + 3, "%d", &level ) == 1 )
		{
			verboseLevel = level;
		}
		FreeRTOS_printf( ( "Verbose level %d time %lu\n", verboseLevel, xCount ) );
	}
#if( HAS_AUDIO != 0 )
	else if( strncmp( pcBuffer, "audio", 5 ) == 0 )
	{
		static TaskHandle_t xTaskHandle = NULL;
		extern unsigned audioTick;
		if (xTaskHandle == NULL)
		{
			void audioTask (void * pvParameter);
			xTaskCreate( audioTask, "AudioTask", 2048, NULL, tskIDLE_PRIORITY + 1, &xTaskHandle );
			logPrintf("Audio task %sstarted\n", (xTaskHandle == NULL) ? "not " : "");
		}
		unsigned seconds = 10;
		const char *command = pars.txtCount >= 2 ? pars.texts[1] : "test";
		const char *fname   = pars.txtCount >= 3 ? pars.texts[2] : "/temp/file_01.wav";
		const char *secs    = pars.txtCount >= 4 ? pars.texts[3] : "10";
		sscanf (secs, "%u", &seconds);
		if( strncmp( command, "record", 6 ) == 0 )
		{
			audioRecord (fname, seconds);
			logPrintf("Record to %s for %u secs\n", fname, seconds);
		}
		else if( strncmp( command, "play", 6 ) == 0 )
		{
			audioPlay (fname, seconds);
			logPrintf("Play from %s for %u secs\n", fname, seconds);
		}
		else if( strncmp( command, "test", 4 ) == 0 )
		{
			audioTick++;
		}
	}
#endif
	else if( memcmp( pcBuffer, "mem", 3 ) == 0 )
	{
		uint32_t now = xPortGetFreeHeapSize();
		uint32_t total = 0; /*xPortGetOrigHeapSize( ); */
		uint32_t perc = total ? ( ( 100 * now ) / total ) : 100;
		FreeRTOS_printf( ( "mem Low %u, Current %lu / %lu (%lu perc free)\n",
						   xPortGetMinimumEverFreeHeapSize(),
						   now, total, perc ) );
	}
	else if( memcmp( pcBuffer, "pars", 4 ) == 0 )
	{
		logPrintf("pars[%d] '%d' '%d' '%d' '%d'\n",
			pars.parCount,
			pars.parameters[0],
			pars.parameters[1],
			pars.parameters[2],
			pars.parameters[3]);
		logPrintf("texts[%d] '%s' '%s' '%s' '%s'\n",
			pars.txtCount,
			pars.texts[0],
			pars.texts[1],
			pars.texts[2],
			pars.texts[3]);
	}
#if( ipconfigUSE_IPv6 != 0 )
	else if( strncmp( pcBuffer, "sizes", 5 ) == 0 )
	{
		FreeRTOS_printf( ( "ipconfigNETWORK_MTU %u IPv6_HEADER %u IPv4_HEADER %u ipSIZE_OF_TCP_HEADER %u\n",
			( unsigned ) ipconfigNETWORK_MTU,
			( unsigned ) ipSIZE_OF_IPv6_HEADER,
			( unsigned ) ipSIZE_OF_IPv4_HEADER,
			( unsigned ) ipSIZE_OF_TCP_HEADER ) );
		#define ipconfigTCP_MSS_6	   ( ipconfigNETWORK_MTU - ( ipSIZE_OF_IPv6_HEADER + ipSIZE_OF_TCP_HEADER ) )
		#define ipconfigTCP_MSS_4	   ( ipconfigNETWORK_MTU - ( ipSIZE_OF_IPv4_HEADER + ipSIZE_OF_TCP_HEADER ) )
		FreeRTOS_printf( ( "ipconfigTCP_MSS = %u and %u\n", ipconfigTCP_MSS_4, ipconfigTCP_MSS_6 ) );
	}
#endif
	else if( strncmp( pcBuffer, "tcptest", 7 ) == 0 )
	{
		tcpServerStart();
	}
#if( USE_MBEDTLS != 0 )
	else if( strncmp( pcBuffer, "ssltest", 7 ) == 0 )
	{
		extern int ssl_main( int argc, char *argv[] );
		char * argv[] =
		{
			"ssl_main",
			"server_port=443",
			"request_page=/",
			"ca_path=/etc/certs",
			"server_name=google.com",
			"dtls=0",
		};
		ssl_main( ARRAY_SIZE( argv ), argv );
	}
#endif
	else if( ( strncmp( pcBuffer, "memstat", 7 ) == 0 ) || ( strncmp( pcBuffer, "mem_stat", 8 ) == 0 ) )
	{
#if( ipconfigUSE_TCP_MEM_STATS != 0 )
		FreeRTOS_printf( ( "Closing mem_stat\n" ) );
		iptraceMEM_STATS_CLOSE();
#else
		FreeRTOS_printf( ( "ipconfigUSE_TCP_MEM_STATS was not defined\n" ) );
#endif	/* ( ipconfigUSE_TCP_MEM_STATS != 0 ) */
	}
//	else if( strncmp( pcBuffer, "mymalloc", 8 ) == 0 )
//	{
//		uint8_t * mem = ( uint8_t * ) myMalloc( 64, pdFALSE );
//		if( mem )
//		{
//			free( mem );
//		}
//		logPrintf( "maMalloc = %p\n", mem );
//	}
	else if( strncmp( pcBuffer, "time", 4 ) == 0 )
	{
		time_t currentTime;
		extern time_t time ( time_t *puxTime );

		time( &( currentTime ) );
		FreeRTOS_printf( ( "Current time %u TickCount %u HR-time %u\n",
			( unsigned ) currentTime,
			( unsigned ) xTaskGetTickCount(),
			( unsigned ) ullGetHighResolutionTime() ) );
	}
#if( HAS_ECHO_TEST != 0 )
	else if( strncmp( pcBuffer, "plus abort", 10 ) == 0 )
	{
		FreeRTOS_printf( ( "Change uxServerAbort from %d\n", uxServerAbort ) );
		uxServerAbort = pdTRUE;
	}
	else if( strncmp( pcBuffer, "plus", 4 ) == 0 )
	{
		int iDoStart = 1;
		char *ptr = pcBuffer+4;
		while (*ptr && !isdigit(*ptr)) ptr++;
		if (!ulServerIPAddress)
			ulServerIPAddress = FreeRTOS_GetIPAddress();
		if (*ptr) {
			int value2;
			char ch;
			char *tail = ptr;
			int rc = sscanf (ptr, "%d%c%d", &iDoStart, &ch, &value2);

			while( *tail && !isspace( *tail ) ) tail++;
			if( isspace( *tail ) )
			{
				*(tail++) = 0;
				while( isspace( *tail ) ) tail++;
			}

			if (rc >= 3) {
				ulServerIPAddress = FreeRTOS_inet_addr( ptr );
				iDoStart = 1;
			} else if (rc < 1) {
				iDoStart = 0;
			}
			if( *tail )
			{
				unsigned uValue;
				if( sscanf( tail, "%u", &uValue) >= 1 )
				{
					FreeRTOS_printf( ( "Exchange %u bytes\n", uValue ) );
					uxClientSendCount = uValue;
				}
			}
		}
		plus_echo_start (iDoStart);
		FreeRTOS_printf( ( "Plus testing has %s for %xip\n", iDoStart ? "started" : "stopped",
			( unsigned ) FreeRTOS_ntohl( ulServerIPAddress ) ) );
	}
#endif /* ( HAS_ECHO_TEST != 0 ) */
	else if( strncmp( pcBuffer, "netstat", 7 ) == 0 )
	{
		FreeRTOS_netstat();
	}
#if( ipconfigUSE_PCAP != 0 )
    else if( strncmp( pcBuffer, "pcap", 4 ) == 0 )
	{
		FreeRTOS_printf( ( "PCAP: %s\n", pcBuffer ) );
		pcap_command( pcBuffer + 4 );
	}
#endif
	#if( USE_LOG_EVENT != 0 )
	else if( strncmp( pcBuffer, "event", 4 ) == 0 )
	{
		eventLogDump();
	}
	#endif /* USE_LOG_EVENT */
	else if( strncmp( pcBuffer, "list", 4 ) == 0 )
	{
		vShowTaskTable( pcBuffer[ 4 ] == 'c' );
	}
//	if( strncmp( pcBuffer, "phytest", 7 ) == 0 )
//	{
//		extern void phy_test();
//		phy_test();
//	}
	#if( mainHAS_RAMDISK != 0 )
//	else if( strncmp( pcBuffer, "dump", 4 ) == 0 )
//	{
//		FF_RAMDiskStoreImage( pxRAMDisk, "ram_disk.img" );
//	}
	#endif	/* mainHAS_RAMDISK */
	#if( USE_IPERF != 0 )
	else if( strncmp( pcBuffer, "iperf", 5 ) == 0 )
	{
		vIPerfInstall();
	}
	#endif	/* USE_IPERF */
	else if( strncmp( pcBuffer, "format", 6 ) == 0 )
	{
		if( pxSDDisk != 0 )
		{
			FF_SDDiskFormat( pxSDDisk, 0 );
		}
	}
	#if( mainHAS_FAT_TEST != 0 )
	else if( strncmp( pcBuffer, "fattest", 7 ) == 0 )
	{
	int level = 0;
	const char pcMountPath[] = "/test";

		if( sscanf( pcBuffer + 7, "%d", &level ) == 1 )
		{
			verboseLevel = level;
		}
		FreeRTOS_printf( ( "FAT test %d\n", level ) );
		ff_mkdir( pcMountPath MKDIR_RECURSE );
		if( ( level != 0 ) && ( iFATRunning == 0 ) )
		{
		int x;

			iFATRunning = 1;
			if( level < 1 ) level = 1;
			if( level > 10 ) level = 10;
			for( x = 0; x < level; x++ )
			{
			char pcName[ 16 ];
			static char pcBaseDirectoryNames[ 10 ][ 16 ];
			sprintf( pcName, "FAT_%02d", x + 1 );
			snprintf( pcBaseDirectoryNames[ x ], sizeof pcBaseDirectoryNames[ x ], "%s/%d",
					pcMountPath, x + 1 );
			ff_mkdir( pcBaseDirectoryNames[ x ] MKDIR_RECURSE );

#define mainFAT_TEST__TASK_PRIORITY	( tskIDLE_PRIORITY + 1 )
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
	}	/* if( strncmp( pcBuffer, "fattest", 7 ) == 0 ) */
	#endif /* mainHAS_FAT_TEST */
	else
	{
		xReturn = pdFALSE;
	}

	return xReturn;
}

uint32_t echoServerIPAddress()
{
	return ulServerIPAddress;
}
/*-----------------------------------------------------------*/

uint32_t HAL_GetTick(void)
{
	/* This timer can be used before the scheduler has started.
	 * The high-resolution timer returns the time since startup
	 * in micro-seconds. */
	uint32_t ulCurTime = ( ullGetHighResolutionTime() / 1000ull );
	return ulCurTime;
}


static void prvSetupHardware( void )
{
	GPIO_InitTypeDef  GPIO_InitStruct;

	/* Configure Flash prefetch and Instruction cache through ART accelerator. */
	#if( ART_ACCLERATOR_ENABLE != 0 )
	{
		__HAL_FLASH_ART_ENABLE();
	}
	#endif /* ART_ACCLERATOR_ENABLE */

	/* Set Interrupt Group Priority */
	HAL_NVIC_SetPriorityGrouping( NVIC_PRIORITYGROUP_4 );

	/* Init the low level hardware. */
	HAL_MspInit();

	/* Configure the System clock to have a frequency of 200 MHz */
	prvSystemClockConfig();

	/* Enable GPIOB  Clock (to be able to program the configuration
	registers) and configure for LED output. */
	/* GPIO Ports Clock Enable */
	__GPIOE_CLK_ENABLE();
	__GPIOB_CLK_ENABLE();
	__GPIOG_CLK_ENABLE();
	__GPIOD_CLK_ENABLE();
	__GPIOC_CLK_ENABLE();
	__GPIOA_CLK_ENABLE();
	__GPIOI_CLK_ENABLE();
	__GPIOH_CLK_ENABLE();
	__GPIOF_CLK_ENABLE();

	__HAL_RCC_GPIOF_CLK_ENABLE();

	GPIO_InitStruct.Pin = GPIO_PIN_10;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
	HAL_GPIO_Init( GPIOF, &GPIO_InitStruct );

	/* MCO2 : Pin PC9 */
	HAL_RCC_MCOConfig( RCC_MCO2, RCC_MCO2SOURCE_SYSCLK, RCC_MCODIV_1 );
}
/*-----------------------------------------------------------*/

void BM_Delay( uint32_t ulDelayMS )
{
	/* BSP_SDRAM_Init() initialises the SDRAM driver. 
	 * It is called far before the scheduler is running.
	 * This function will use the HR-timer to measure time. */
	uint32_t ulStartTime = HAL_GetTick();
	for( ;; )
	{
		uint32_t ulNow = HAL_GetTick();
		if( ( ulNow - ulStartTime ) > ulDelayMS )
		{
			break;
		}
	}
}
/*-----------------------------------------------------------*/

static void prvSystemClockConfig( void )
{
	/* The system Clock is configured as follow :
		System Clock source            = PLL (HSE)
		SYSCLK(Hz)                     = 200000000
		HCLK(Hz)                       = 200000000
		AHB Prescaler                  = 1
		APB1 Prescaler                 = 4
		APB2 Prescaler                 = 2
		HSE Frequency(Hz)              = 25000000
		PLL_M                          = 25
		PLL_N                          = 400
		PLL_P                          = 2
		PLL_Q                          = 7
		VDD(V)                         = 3.3
		Main regulator output voltage  = Scale1 mode
		Flash Latency(WS)              = 7 */
	RCC_ClkInitTypeDef RCC_ClkInitStruct;
	RCC_OscInitTypeDef RCC_OscInitStruct;

	/* Enable HSE Oscillator and activate PLL with HSE as source */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.HSIState = RCC_HSI_OFF;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 25;
	RCC_OscInitStruct.PLL.PLLN = 400;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 7;
	HAL_RCC_OscConfig(&RCC_OscInitStruct);

	/* Select PLL as system clock source and configure the HCLK, PCLK1 and PCLK2
	clocks dividers */
	RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2);
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
	configASSERT( HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_7) == HAL_OK );
}
/*-----------------------------------------------------------*/

/* The following hook-function will be called from "STM32F4xx\ff_sddisk.c"
in case the GPIO contact of the card-detect has changed. */

EXTERN_C void vApplicationCardDetectChangeHookFromISR( BaseType_t *pxHigherPriorityTaskWoken );

void vApplicationCardDetectChangeHookFromISR( BaseType_t *pxHigherPriorityTaskWoken )
{
	/* This routine will be called on every change of the Card-detect
	GPIO pin.  The TCP server is probably waiting for in event in a
	select() statement.  Wake it up.*/
	if( pxTCPServer != NULL )
	{
		FreeRTOS_TCPServerSignalFromISR( pxTCPServer, pxHigherPriorityTaskWoken );
	}
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

	volatile const char *assert_fname = NULL;
	volatile int assert_line = 0;
	volatile int assert_may_proceed = 0;

	void assert_failed(uint8_t* pucFilename, uint32_t ulLineNr)
	{
		assert_fname = ( const char * ) pucFilename;
		assert_line = ( int ) ulLineNr;
		taskDISABLE_INTERRUPTS();
		while( assert_may_proceed == pdFALSE )
		{
		}
		taskENABLE_INTERRUPTS();

	}
#endif
/*-----------------------------------------------------------*/

static BaseType_t xMatchDNS( const char * pcObject, const char * pcMyName )
{
taskENTER_CRITICAL();
	BaseType_t rc = -1;
	size_t uxLength = strlen( pcMyName );
	const char *pcDot = strchr( pcObject, '.' );
#if( ipconfigUSE_MDNS == 1 )
	if( pcDot != NULL )
	{
		if( strcasecmp( pcDot, ".local" ) == 0 )
		{
			uxLength -= 6U;
			rc = strncasecmp( pcMyName, pcObject, uxLength );
		}
	}
	else
#endif
	{
		rc = strncasecmp( pcMyName, pcObject, uxLength );
	}
	/*
	 * "zynq" matches "zynq.local".
	 */
taskEXIT_CRITICAL();
	return rc;
}

#if( ipconfigMULTI_INTERFACE != 0 )
#warning Multi
	EXTERN_C BaseType_t xApplicationDNSQueryHook( NetworkEndPoint_t *pxEndPoint, const char *pcName )
#else
#warning Single
	EXTERN_C BaseType_t xApplicationDNSQueryHook( const char *pcName )
#endif
{
BaseType_t xReturn;

	/* Determine if a name lookup is for this node.  Two names are given
	to this node: that returned by pcApplicationHostnameHook() and that set
	by mainDEVICE_NICK_NAME. */
	if( xMatchDNS( pcName, pcApplicationHostnameHook() ) == 0 )  /* "stm" */
	{
		xReturn = pdPASS;
	}
	else if( xMatchDNS( pcName, mainDEVICE_NICK_NAME ) == 0 )  /* "stm32f7" */
	{
		xReturn = pdPASS;
	}
	else if( xMatchDNS( pcName, "hs2011" ) == 0 )
	{
		xReturn = pdPASS;
	}
	else if( xMatchDNS( pcName, "aap" ) == 0 )
	{
#if( ipconfigUSE_IPv6 != 0 )
		xReturn = pxEndPoint->bits.bIPv6 != 0;
#else
		xReturn = pdPASS;
#endif
	}
	else if( xMatchDNS( pcName, "kat" ) == 0 )
	{
#if( ipconfigUSE_IPv6 != 0 )
		xReturn = pxEndPoint->bits.bIPv6 == 0;
		unsigned number = 0;
		if( sscanf( pcName + 3, "%u", &number) > 0 )
		{
			if( number == 172 )
			{
				pxEndPoint->ipv4_settings.ulIPAddress = FreeRTOS_inet_addr_quick( 172,  16,   0, 114 );
			}
			else
			if( number == 192 )
			{
			NetworkEndPoint_t *px;

				for( px = FreeRTOS_FirstEndPoint( NULL );
					px != NULL;
					px = FreeRTOS_NextEndPoint( NULL, px ) )
				{
					if( px->bits.bIPv6 == pdFALSE_UNSIGNED )
					{
						break;
					}
				}
				if( px != NULL )
				{
					pxEndPoint->ipv4_settings.ulIPAddress = px->ipv4_settings.ulIPAddress;
				}
				else
				{
					pxEndPoint->ipv4_settings.ulIPAddress = FreeRTOS_inet_addr_quick( 192,  168,   2, 114 );
				}
			}
		}
#else
		xReturn = pdPASS;
#endif
	}
	else
	{
		xReturn = pdFAIL;
	}
	#if( ipconfigUSE_IPv6 != 0 )
	{
		if( ( allowIPVersion & eIPv4 ) == 0 && !pxEndPoint->bits.bIPv6 )
		{
			xReturn = pdFAIL;
		}
		if( ( allowIPVersion & eIPv6 ) == 0 && pxEndPoint->bits.bIPv6 )
		{
			xReturn = pdFAIL;
		}
	}
	#endif	/* ipconfigUSE_IPv6 */
	if( xReturn )
	{
	#if( ipconfigMULTI_INTERFACE != 0 ) && ( ipconfigUSE_IPv6 != 0 )
		FreeRTOS_printf( ( "LLMNR query '%s' = %d IPv%c\n", pcName, (int)xReturn, pxEndPoint->bits.bIPv6 ? '6' : '4' ) );
	#else
		FreeRTOS_printf( ( "LLMNR query '%s' = %d IPv4 only\n", pcName, (int)xReturn ) );
	#endif
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

/* Called by FreeRTOS+TCP when the network connects or disconnects.  Disconnect
events are only received if implemented in the MAC driver. */
#if( ipconfigMULTI_INTERFACE != 0 )
	void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent, NetworkEndPoint_t *pxEndPoint )
#else
	void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent )
#endif
{
static BaseType_t xTasksAlreadyCreated = pdFALSE;

	/* If the network has just come up...*/
	if( eNetworkEvent == eNetworkUp )
	{
		/* Create the tasks that use the IP stack if they have not already been
		created. */
		if( xTasksAlreadyCreated == pdFALSE )
		{
			/* Tasks that use the TCP/IP stack can be created here. */

			/* Start a new task to fetch logging lines and send them out. */
			vUDPLoggingTaskCreate();

			#if( mainCREATE_TCP_ECHO_TASKS_SINGLE == 1 )
			{
				/* See http://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/TCP_Echo_Clients.html */
				vStartTCPEchoClientTasks_SingleTasks( configMINIMAL_STACK_SIZE * 4, tskIDLE_PRIORITY + 1 );
			}
			#endif

			#if( mainCREATE_SIMPLE_TCP_ECHO_SERVER == 1 )
			{
				/* See http://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/TCP_Echo_Server.html */
				vStartSimpleTCPServerTasks( configMINIMAL_STACK_SIZE * 4, tskIDLE_PRIORITY + 1 );
			}
			#endif

			#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
			{
				/* Let the server work task now it can now create the servers. */
				xTaskNotifyGive( xServerWorkTaskHandle );
			}
			#endif

			xTasksAlreadyCreated = pdTRUE;
			xStartServices = EStartupStart;
		}

		#if( ipconfigMULTI_INTERFACE == 0 )
		{
		uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
		char pcBuffer[ 16 ];
			/* Print out the network configuration, which may have come from a DHCP
			server. */
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
		#else
		{
			/* Print out the network configuration, which may have come from a DHCP
			server. */
			FreeRTOS_printf( ( "IP-address : %lxip (default %lxip)\n",
				FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulIPAddress ), FreeRTOS_ntohl( pxEndPoint->ipv4_defaults.ulIPAddress ) ) );
			FreeRTOS_printf( ( "Net mask   : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulNetMask ) ) );
			FreeRTOS_printf( ( "GW         : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulGatewayAddress ) ) );
			FreeRTOS_printf( ( "DNS        : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulDNSServerAddresses[ 0 ] ) ) );
			FreeRTOS_printf( ( "Broadcast  : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulBroadcastAddress ) ) );
			FreeRTOS_printf( ( "MAC address: %02x-%02x-%02x-%02x-%02x-%02x\n",
				pxEndPoint->xMACAddress.ucBytes[ 0 ],
				pxEndPoint->xMACAddress.ucBytes[ 1 ],
				pxEndPoint->xMACAddress.ucBytes[ 2 ],
				pxEndPoint->xMACAddress.ucBytes[ 3 ],
				pxEndPoint->xMACAddress.ucBytes[ 4 ],
				pxEndPoint->xMACAddress.ucBytes[ 5 ] ) );
			FreeRTOS_printf( ( " \n" ) );
		}
		#endif /* ipconfigMULTI_INTERFACE */
	}
}
/*-----------------------------------------------------------*/

EXTERN_C void vApplicationMallocFailedHook( void );
void vApplicationMallocFailedHook( void )
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

	/* Force an assert. */
	configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */

	/* Force an assert. */
	configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/

#if( USE_TCP_DEMO_CLI == 0 )
	extern void vApplicationPingReplyHook( ePingReplyStatus_t eStatus, uint16_t usIdentifier );
#endif

extern const char *pcApplicationHostnameHook( void );

#if( ipconfigMULTI_INTERFACE != 0 )
	BaseType_t xApplicationDNSQueryHook( NetworkEndPoint_t *pxEndPoint, const char *pcName );
#else
	BaseType_t xApplicationDNSQueryHook( const char *pcName );
#endif

#if( USE_TCP_DEMO_CLI == 0 )
	void vApplicationPingReplyHook( ePingReplyStatus_t eStatus, uint16_t usIdentifier )
	{
		FreeRTOS_printf( ( "Ping status %d ID = %4d\n", eStatus, usIdentifier ) ) ;
	}
#endif
/*-----------------------------------------------------------*/

struct xREGISTER_STACK {
	uint32_t spare0[ 8 ];
	uint32_t r0;
	uint32_t r1;
	uint32_t r2;
	uint32_t r3;
	uint32_t r12;
	uint32_t lr; /* Link register. */
	uint32_t pc; /* Program counter. */
	uint32_t psr;/* Program status register. */
	uint32_t spare1[ 8 ];
};

volatile struct xREGISTER_STACK *pxRegisterStack = NULL;

EXTERN_C void prvGetRegistersFromStack( uint32_t *pulFaultStackAddress )
{
	pxRegisterStack = ( struct xREGISTER_STACK *) ( pulFaultStackAddress - ARRAY_SIZE( pxRegisterStack->spare0 ) );

	/* When the following line is hit, the variables contain the register values. */
	taskENTER_CRITICAL();
	{
		/* Set ul to a non-zero value using the debugger to step out of this
		function. */
		while( ipFALSE_BOOL )
		{
			__NOP();
		}
	}
	taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

//void HardFault_Handler( void ) __attribute__( ( naked ) );
EXTERN_C void HardFault_Handler(void)
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


EXTERN_C void vAssertCalled( const char *pcFile, uint32_t ulLine );

void vAssertCalled( const char *pcFile, uint32_t ulLine )
{
volatile unsigned long ul = 0;

	( void ) pcFile;
	( void ) ulLine;

	taskENTER_CRITICAL();
	{
		/* Set ul to a non-zero value using the debugger to step out of this
		function. */
		while( ul == 0 )
		{
			__NOP();
		}
	}
	taskEXIT_CRITICAL();
}
/*-----------------------------------------------------------*/

#if defined(__IAR_SYSTEMS_ICC__)
	uint8_t heapMemory[100000];
	
	#define HEAP_START		heapMemory[0]
	#define HEAP_END		heapMemory[sizeof heapMemory]
#else
	extern uint8_t __bss_end__, _estack, _Min_Stack_Size;
	#define HEAP_START		__bss_end__
	#define HEAP_END		_estack
#endif

volatile uint32_t ulHeapSize;
volatile uint8_t *pucHeapStart;

static void heapInit( )
{
uint32_t ulStackSize = (uint32_t)&(_Min_Stack_Size);

	pucHeapStart = ( uint8_t * ) ( ( ( ( uint32_t ) &HEAP_START ) + 7 ) & ~0x07ul );

	ulHeapSize = ( uint32_t ) ( &HEAP_END - &HEAP_START );
	ulHeapSize &= ~0x07ul;
	ulHeapSize -= ulStackSize;

	HeapRegion_t xHeapRegions[] = {
		{ ( unsigned char *) pucHeapStart, ulHeapSize },
		{ NULL, 0 }
 	};

	vPortDefineHeapRegions( xHeapRegions );
}
/*-----------------------------------------------------------*/

/* In some cases, a library call will use malloc()/free(),
redefine them to use pvPortMalloc()/vPortFree(). */

EXTERN_C void *malloc(size_t size)
{
	return pvPortMalloc(size);
}
/*-----------------------------------------------------------*/

EXTERN_C void free(void *ptr)
{
	vPortFree(ptr);
}
/*-----------------------------------------------------------*/

EXTERN_C void vOutputChar( const char cChar, const TickType_t xTicksToWait  );
void vOutputChar( const char cChar, const TickType_t xTicksToWait  )
{
}
/*-----------------------------------------------------------*/

//RAM (xrw)        : ORIGIN = 0x20000000, LENGTH = 307K
//Memory_B1(xrw)   : ORIGIN = 0x20010000, LENGTH = 0x80
//Memory_B2(xrw)   : ORIGIN = 0x20010080, LENGTH = 0x80
//Memory_B3(xrw)   : ORIGIN = 0x2004C000, LENGTH = 0x17d0 
//Memory_B4(xrw)   : ORIGIN = 0x2004D7D0, LENGTH = 0x17d0

EXTERN_C BaseType_t xApplicationMemoryPermissions( uint32_t aAddress );
BaseType_t xApplicationMemoryPermissions( uint32_t aAddress )
{
BaseType_t xResult;
#define FLASH_ORIGIN	0x08000000
#define FLASH_LENGTH	( 1024 * 1024 )
#define SRAM_ORIGIN		0x20000000
#define SRAM_LENGTH		( 400 * 1024 )//( 307 * 1024 )
// max = 4CC00
//  0x2004febc
	if( aAddress >= FLASH_ORIGIN && aAddress < FLASH_ORIGIN + FLASH_LENGTH )
	{
		xResult = 1;
	}
	else if( ( aAddress >= SRAM_ORIGIN && aAddress < SRAM_ORIGIN + SRAM_LENGTH ) ||
			 ( aAddress >= SDRAM_START && aAddress < SDRAM_START + SDRAM_SIZE ) )

	{
		xResult = 3;
	}
	else
	{
		xResult = 0;
	}

	return xResult;
}
/*-----------------------------------------------------------*/

const char *pcApplicationHostnameHook( void )
{
	/* Assign the name "FreeRTOS" to this network node.  This function will be
	called during the DHCP: the machine will be registered with an IP address
	plus this name. */
	return mainHOST_NAME;
}
/*-----------------------------------------------------------*/

extern void vConfigureTimerForRunTimeStats( void );
extern uint32_t ulGetRunTimeCounterValue( void );

void vConfigureTimerForRunTimeStats( void )
{
}

static uint64_t ullHiresTime;
uint32_t ulGetRunTimeCounterValue( void )
{
	return ( uint32_t ) ( ullGetHighResolutionTime() - ullHiresTime );
}

/*extern*/ BaseType_t xTaskClearCounters;
void vShowTaskTable( BaseType_t aDoClear )
{
TaskStatus_t *pxTaskStatusArray;
volatile UBaseType_t uxArraySize, x;
uint64_t ullTotalRunTime;
uint32_t ulStatsAsPermille;
uint32_t ulStackSize;

	// Take a snapshot of the number of tasks in case it changes while this
	// function is executing.
	uxArraySize = uxTaskGetNumberOfTasks();

	// Allocate a TaskStatus_t structure for each task.  An array could be
	// allocated statically at compile time.
	pxTaskStatusArray = ( TaskStatus_t * ) pvPortMalloc( uxArraySize * sizeof( TaskStatus_t ) );

	FreeRTOS_printf( ( "Task name    Prio    Stack    Time(uS) Perc \n" ) );

	if( pxTaskStatusArray != NULL )
	{
		// Generate raw status information about each task.
		uint32_t ulDummy;
		xTaskClearCounters = aDoClear;
		uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulDummy );

		ullTotalRunTime = ullGetHighResolutionTime() - ullHiresTime;

		// For percentage calculations.
		ullTotalRunTime /= 1000UL;

		// Avoid divide by zero errors.
		if( ullTotalRunTime > 0ull )
		{
			// For each populated position in the pxTaskStatusArray array,
			// format the raw data as human readable ASCII data
			for( x = 0; x < uxArraySize; x++ )
			{
				// What percentage of the total run time has the task used?
				// This will always be rounded down to the nearest integer.
				// ulTotalRunTimeDiv100 has already been divided by 100.
				ulStatsAsPermille = pxTaskStatusArray[ x ].ulRunTimeCounter / ullTotalRunTime;

				FreeRTOS_printf( ( "%-14.14s %2lu %8u %8lu  %3lu.%lu %%\n",
					pxTaskStatusArray[ x ].pcTaskName,
					pxTaskStatusArray[ x ].uxCurrentPriority,
					pxTaskStatusArray[ x ].usStackHighWaterMark,
					pxTaskStatusArray[ x ].ulRunTimeCounter,
					ulStatsAsPermille / 10,
					ulStatsAsPermille % 10) );
			}
		}

		// The array is no longer needed, free the memory it consumes.
		vPortFree( pxTaskStatusArray );
	}
	ulStackSize = (uint32_t)&(_Min_Stack_Size);
	FreeRTOS_printf( ( "Heap: min/cur/max: %lu %lu %lu stack %lu\n",
			( uint32_t )xPortGetMinimumEverFreeHeapSize(),
			( uint32_t )xPortGetFreeHeapSize(),
			( uint32_t )ulHeapSize,
			( uint32_t )ulStackSize ) );
	if( aDoClear != pdFALSE )
	{
//		ulListTime = xTaskGetTickCount();
		ullHiresTime = ullGetHighResolutionTime();
	}
}
/*-----------------------------------------------------------*/

#if defined ( __GNUC__ ) /*!< GCC Compiler */

	volatile int unknown_calls = 0;

	struct timeval;

	int _gettimeofday (struct timeval *pcTimeValue, void *pvTimeZone)
	{
		unknown_calls++;

		return 0;
	}
	/*-----------------------------------------------------------*/

	#if GNUC_NEED_SBRK
		EXTERN_C void *_sbrk( ptrdiff_t __incr);
		void *_sbrk( ptrdiff_t __incr)
		{
			unknown_calls++;
	
			return 0;
		}
		/*-----------------------------------------------------------*/
	#endif
#endif


void vApplicationTickHook( void )
{
}
/*-----------------------------------------------------------*/

EXTERN_C void _init();
void _init()
{
}


/*_HT_ Please consider keeping the function below called HAL_ETH_MspInit().
It will be called from "stm32f4xx_hal_eth.c".
It is defined there as weak only.
My board didn't work without the settings made below.
*/
/* This is a callback function. */
void HAL_ETH_MspInit( ETH_HandleTypeDef* heth )
{
    GPIO_InitTypeDef GPIO_InitStructure;
    if (heth->Instance == ETH) {

        /* Enable GPIOs clocks */
        __HAL_RCC_GPIOA_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOG_CLK_ENABLE();

        /** ETH GPIO Configuration
          RMII_REF_CLK ----------------------> PA1
          RMII_MDIO -------------------------> PA2
          RMII_MDC --------------------------> PC1
          RMII_MII_CRS_DV -------------------> PA7
          RMII_MII_RXD0 ---------------------> PC4
          RMII_MII_RXD1 ---------------------> PC5
          RMII_MII_RXER ---------------------> PG2
          RMII_MII_TX_EN --------------------> PG11
          RMII_MII_TXD0 ---------------------> PG13
          RMII_MII_TXD1 ---------------------> PG14
         */
        /* Configure PA1, PA2 and PA7 */

        GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
        GPIO_InitStructure.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStructure.Pull = GPIO_NOPULL; 
        GPIO_InitStructure.Alternate = GPIO_AF11_ETH;
        GPIO_InitStructure.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);

        /* Configure PC1, PC4 and PC5 */
        GPIO_InitStructure.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStructure);

        /* Configure PG2, PG11, PG13 and PG14 */
        GPIO_InitStructure.Pin =  GPIO_PIN_2 | GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14;
        HAL_GPIO_Init(GPIOG, &GPIO_InitStructure);

        /* Enable the Ethernet global Interrupt */
        HAL_NVIC_SetPriority(ETH_IRQn, 0x7, 0);
        HAL_NVIC_EnableIRQ(ETH_IRQn);
        
        /* Enable ETHERNET clock  */
        __HAL_RCC_ETH_CLK_ENABLE();
    }
}

#ifdef DEBUG
	#undef DEBUG
#endif

#define DEBUG(X)			lUDPLoggingPrintf X

#define tcpechoPORT_NUMBER	7

Socket_t xListeningSocket = NULL;
Socket_t xChildSocket = NULL;

void tcpServerStart()
{
	BaseType_t xReceiveTimeOut = 10;
	struct freertos_sockaddr xBindAddress;
#if( ipconfigUSE_TCP_WIN == 1 )
	WinProperties_t xWinProps;

	/* Fill in the buffer and window sizes that will be used by the socket. */
	xWinProps.lTxBufSize = 2920;
	xWinProps.lTxWinSize = 2;
	xWinProps.lRxBufSize = 2920;
	xWinProps.lRxWinSize = 2;
#endif /* ipconfigUSE_TCP_WIN */

	if (xListeningSocket != NULL) {
		FreeRTOS_printf( ( "Socket already created\n" ) );
		return;
	}
	xListeningSocket = FreeRTOS_socket( FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP );
	configASSERT( xListeningSocket != FREERTOS_INVALID_SOCKET );

	/* Set a time out so accept() will just wait for a connection. */
	FreeRTOS_setsockopt( xListeningSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof( xReceiveTimeOut ) );

	/* Set the window and buffer sizes. */
	#if( ipconfigUSE_TCP_WIN == 1 )
	{
		FreeRTOS_setsockopt( xListeningSocket, 0, FREERTOS_SO_WIN_PROPERTIES, ( void * ) &xWinProps, sizeof( xWinProps ) );
	}
	#endif /* ipconfigUSE_TCP_WIN */

	FreeRTOS_setsockopt( xListeningSocket, 0, FREERTOS_SO_SET_SEMAPHORE, ( void * ) &xServerSemaphore, sizeof( xServerSemaphore ) );

	/* Bind the socket to the port that the client task will send to, then
	listen for incoming connections. */
	xBindAddress.sin_port = FreeRTOS_htons( tcpechoPORT_NUMBER );
	FreeRTOS_bind( xListeningSocket, &xBindAddress, sizeof( xBindAddress ) );
	FreeRTOS_listen( xListeningSocket, 2 );
	FreeRTOS_printf( ( "Socket now created\n" ) );
}

void tcpServerWork()
{
	struct freertos_sockaddr xClient;
	socklen_t xSize = sizeof(xClient);
	if( xChildSocket != NULL ) {
		unsigned char pucRxBuffer[ 1400 ];
		BaseType_t lBytes = FreeRTOS_recv( xChildSocket, pucRxBuffer, ipconfigTCP_MSS, 0 );

		/* If data was received, echo it back. */
		if( lBytes >= 0 )
		{
			BaseType_t lSent = FreeRTOS_send( xChildSocket, pucRxBuffer, lBytes, 0 );

			if( ( lSent < 0 ) && ( lSent != pdFREERTOS_ERRNO_EAGAIN ) )
			{
				/* Socket closed? */
				FreeRTOS_closesocket( xChildSocket );
				xChildSocket = NULL;
			}
		}
		else if( lBytes < 0 && lBytes != -pdFREERTOS_ERRNO_EAGAIN )
		{
			/* Socket closed? */
			FreeRTOS_closesocket( xChildSocket );
			xChildSocket = NULL;
			FreeRTOS_printf( ( "Connection closed\n" ) );
		}
	} else if( xListeningSocket != NULL ) {
		xChildSocket = FreeRTOS_accept( xListeningSocket, &xClient, &xSize );
		if( xChildSocket != FREERTOS_INVALID_SOCKET && xChildSocket != NULL )
		{
			FreeRTOS_printf( ( "A new client comes in\n" ) );
		}
		else
		{
			xChildSocket = NULL;
		}
	}
}

static void prvFTPTask (void *pvParameters )
{
	for (;; )
	{
		if( ( pxSDDisk != NULL ) || ( pxRAMDisk != NULL ) )
		{
			#if( ipconfigUSE_HTTP != 0 ) || ( ipconfigUSE_FTP != 0 )
			{
				const TickType_t xInitialBlockTime = pdMS_TO_TICKS( 100U );
				FreeRTOS_TCPServerWork( pxTCPServer, xInitialBlockTime );
			}
			#endif
		}
		else
		{
			vTaskDelay (10);
		}
	}
}

uint32_t ulApplicationGetNextSequenceNumber(
    uint32_t ulSourceAddress,
    uint16_t usSourcePort,
    uint32_t ulDestinationAddress,
    uint16_t usDestinationPort )
{
	uint32_t ulReturn;
	xApplicationGetRandomNumber( &( ulReturn ) );
	return ulReturn;
}

EXTERN_C void xPortSysTickHandler( void );
EXTERN_C void SysTick_Handler(void);
void SysTick_Handler(void)
{
	/* USER CODE BEGIN SysTick_IRQn 0 */

	/* USER CODE END SysTick_IRQn 0 */
	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
		xPortSysTickHandler( );
	}
	HAL_IncTick();
	/* USER CODE BEGIN SysTick_IRQn 1 */

	/* USER CODE END SysTick_IRQn 1 */
}

//EXTERN_C void NMI_Handler( void )                   { for( ;; ) { __NOP(); } }
//EXTERN_C void HardFault_Handler( void )             { for( ;; ) { __NOP(); } }
//EXTERN_C void MemManage_Handler( void )             { for( ;; ) { __NOP(); } }
//EXTERN_C void BusFault_Handler( void )              { for( ;; ) { __NOP(); } }
//EXTERN_C void UsageFault_Handler( void )            { for( ;; ) { __NOP(); } }
//EXTERN_C void SVC_Handler( void )                   { for( ;; ) { __NOP(); } }
//EXTERN_C void DebugMon_Handler( void )              { for( ;; ) { __NOP(); } }
//EXTERN_C void PendSV_Handler( void )                { for( ;; ) { __NOP(); } }
//EXTERN_C void SysTick_Handler( void )               { for( ;; ) { __NOP(); } }
EXTERN_C void WWDG_IRQHandler( void )               { for( ;; ) { __NOP(); } }
EXTERN_C void PVD_IRQHandler( void )                { for( ;; ) { __NOP(); } }
EXTERN_C void TAMP_STAMP_IRQHandler( void )         { for( ;; ) { __NOP(); } }
EXTERN_C void RTC_WKUP_IRQHandler( void )           { for( ;; ) { __NOP(); } }
EXTERN_C void FLASH_IRQHandler( void )              { for( ;; ) { __NOP(); } }
EXTERN_C void RCC_IRQHandler( void )                { for( ;; ) { __NOP(); } }
EXTERN_C void EXTI0_IRQHandler( void )              { for( ;; ) { __NOP(); } }
EXTERN_C void EXTI1_IRQHandler( void )              { for( ;; ) { __NOP(); } }
EXTERN_C void EXTI2_IRQHandler( void )              { for( ;; ) { __NOP(); } }
EXTERN_C void EXTI3_IRQHandler( void )              { for( ;; ) { __NOP(); } }
EXTERN_C void EXTI4_IRQHandler( void )              { for( ;; ) { __NOP(); } }
EXTERN_C void DMA1_Stream0_IRQHandler( void )       { for( ;; ) { __NOP(); } }
EXTERN_C void DMA1_Stream1_IRQHandler( void )       { for( ;; ) { __NOP(); } }
EXTERN_C void DMA1_Stream2_IRQHandler( void )       { for( ;; ) { __NOP(); } }
EXTERN_C void DMA1_Stream3_IRQHandler( void )       { for( ;; ) { __NOP(); } }
EXTERN_C void DMA1_Stream4_IRQHandler( void )       { for( ;; ) { __NOP(); } }
EXTERN_C void DMA1_Stream5_IRQHandler( void )       { for( ;; ) { __NOP(); } }
EXTERN_C void DMA1_Stream6_IRQHandler( void )       { for( ;; ) { __NOP(); } }
EXTERN_C void ADC_IRQHandler( void )                { for( ;; ) { __NOP(); } }
EXTERN_C void CAN1_TX_IRQHandler( void )            { for( ;; ) { __NOP(); } }
EXTERN_C void CAN1_RX0_IRQHandler( void )           { for( ;; ) { __NOP(); } }
EXTERN_C void CAN1_RX1_IRQHandler( void )           { for( ;; ) { __NOP(); } }
EXTERN_C void CAN1_SCE_IRQHandler( void )           { for( ;; ) { __NOP(); } }
EXTERN_C void EXTI9_5_IRQHandler( void )            { for( ;; ) { __NOP(); } }
EXTERN_C void TIM1_BRK_TIM9_IRQHandler( void )      { for( ;; ) { __NOP(); } }
EXTERN_C void TIM1_UP_TIM10_IRQHandler( void )      { for( ;; ) { __NOP(); } }
EXTERN_C void TIM1_TRG_COM_TIM11_IRQHandler( void ) { for( ;; ) { __NOP(); } }
EXTERN_C void TIM1_CC_IRQHandler( void )            { for( ;; ) { __NOP(); } }
//EXTERN_C void TIM2_IRQHandler( void )               { for( ;; ) { __NOP(); } }
EXTERN_C void TIM3_IRQHandler( void )               { for( ;; ) { __NOP(); } }
EXTERN_C void TIM4_IRQHandler( void )               { for( ;; ) { __NOP(); } }
EXTERN_C void I2C1_EV_IRQHandler( void )            { for( ;; ) { __NOP(); } }
EXTERN_C void I2C1_ER_IRQHandler( void )            { for( ;; ) { __NOP(); } }
EXTERN_C void I2C2_EV_IRQHandler( void )            { for( ;; ) { __NOP(); } }
EXTERN_C void I2C2_ER_IRQHandler( void )            { for( ;; ) { __NOP(); } }
EXTERN_C void SPI1_IRQHandler( void )               { for( ;; ) { __NOP(); } }
EXTERN_C void SPI2_IRQHandler( void )               { for( ;; ) { __NOP(); } }
EXTERN_C void USART1_IRQHandler( void )             { for( ;; ) { __NOP(); } }
EXTERN_C void USART2_IRQHandler( void )             { for( ;; ) { __NOP(); } }
EXTERN_C void USART3_IRQHandler( void )             { for( ;; ) { __NOP(); } }
//EXTERN_C void EXTI15_10_IRQHandler( void )          { for( ;; ) { __NOP(); } }
EXTERN_C void RTC_Alarm_IRQHandler( void )          { for( ;; ) { __NOP(); } }
EXTERN_C void OTG_FS_WKUP_IRQHandler( void )        { for( ;; ) { __NOP(); } }
EXTERN_C void TIM8_BRK_TIM12_IRQHandler( void )     { for( ;; ) { __NOP(); } }
EXTERN_C void TIM8_UP_TIM13_IRQHandler( void )      { for( ;; ) { __NOP(); } }
EXTERN_C void TIM8_TRG_COM_TIM14_IRQHandler( void ) { for( ;; ) { __NOP(); } }
EXTERN_C void TIM8_CC_IRQHandler( void )            { for( ;; ) { __NOP(); } }
EXTERN_C void DMA1_Stream7_IRQHandler( void )       { for( ;; ) { __NOP(); } }
EXTERN_C void FMC_IRQHandler( void )                { for( ;; ) { __NOP(); } }
//EXTERN_C void SDMMC1_IRQHandler( void )             { for( ;; ) { __NOP(); } }
EXTERN_C void TIM5_IRQHandler( void )               { for( ;; ) { __NOP(); } }
EXTERN_C void SPI3_IRQHandler( void )               { for( ;; ) { __NOP(); } }
EXTERN_C void UART4_IRQHandler( void )              { for( ;; ) { __NOP(); } }
EXTERN_C void UART5_IRQHandler( void )              { for( ;; ) { __NOP(); } }
EXTERN_C void TIM6_DAC_IRQHandler( void )           { for( ;; ) { __NOP(); } }
EXTERN_C void TIM7_IRQHandler( void )               { for( ;; ) { __NOP(); } }
EXTERN_C void DMA2_Stream0_IRQHandler( void )       { for( ;; ) { __NOP(); } }
EXTERN_C void DMA2_Stream1_IRQHandler( void )       { for( ;; ) { __NOP(); } }
EXTERN_C void DMA2_Stream2_IRQHandler( void )       { for( ;; ) { __NOP(); } }
//EXTERN_C void DMA2_Stream3_IRQHandler( void )       { for( ;; ) { __NOP(); } }
#if( HAS_AUDIO == 0 )
	EXTERN_C void DMA2_Stream4_IRQHandler( void )       { for( ;; ) { __NOP(); } }
#endif
//EXTERN_C void DMA2_Stream4_IRQHandler( void )       { for( ;; ) { __NOP(); } }
//EXTERN_C void ETH_IRQHandler( void )                { for( ;; ) { __NOP(); } }
EXTERN_C void ETH_WKUP_IRQHandler( void )           { for( ;; ) { __NOP(); } }
EXTERN_C void CAN2_TX_IRQHandler( void )            { for( ;; ) { __NOP(); } }
EXTERN_C void CAN2_RX0_IRQHandler( void )           { for( ;; ) { __NOP(); } }
EXTERN_C void CAN2_RX1_IRQHandler( void )           { for( ;; ) { __NOP(); } }
EXTERN_C void CAN2_SCE_IRQHandler( void )           { for( ;; ) { __NOP(); } }
EXTERN_C void OTG_FS_IRQHandler( void )             { for( ;; ) { __NOP(); } }
EXTERN_C void DMA2_Stream5_IRQHandler( void )       { for( ;; ) { __NOP(); } }
//EXTERN_C void DMA2_Stream6_IRQHandler( void )       { for( ;; ) { __NOP(); } }
#if( HAS_AUDIO == 0 )
	EXTERN_C void DMA2_Stream7_IRQHandler( void )       { for( ;; ) { __NOP(); } }
#endif
EXTERN_C void USART6_IRQHandler( void )             { for( ;; ) { __NOP(); } }
EXTERN_C void I2C3_EV_IRQHandler( void )            { for( ;; ) { __NOP(); } }
EXTERN_C void I2C3_ER_IRQHandler( void )            { for( ;; ) { __NOP(); } }
EXTERN_C void OTG_HS_EP1_OUT_IRQHandler( void )     { for( ;; ) { __NOP(); } }
EXTERN_C void OTG_HS_EP1_IN_IRQHandler( void )      { for( ;; ) { __NOP(); } }
EXTERN_C void OTG_HS_WKUP_IRQHandler( void )        { for( ;; ) { __NOP(); } }
EXTERN_C void OTG_HS_IRQHandler( void )             { for( ;; ) { __NOP(); } }
EXTERN_C void DCMI_IRQHandler( void )               { for( ;; ) { __NOP(); } }
EXTERN_C void RNG_IRQHandler( void )                { for( ;; ) { __NOP(); } }
EXTERN_C void FPU_IRQHandler( void )                { for( ;; ) { __NOP(); } }
EXTERN_C void UART7_IRQHandler( void )              { for( ;; ) { __NOP(); } }
EXTERN_C void UART8_IRQHandler( void )              { for( ;; ) { __NOP(); } }
EXTERN_C void SPI4_IRQHandler( void )               { for( ;; ) { __NOP(); } }
EXTERN_C void SPI5_IRQHandler( void )               { for( ;; ) { __NOP(); } }
EXTERN_C void SPI6_IRQHandler( void )               { for( ;; ) { __NOP(); } }
EXTERN_C void SAI1_IRQHandler( void )               { for( ;; ) { __NOP(); } }
EXTERN_C void LTDC_IRQHandler( void )               { for( ;; ) { __NOP(); } }
EXTERN_C void LTDC_ER_IRQHandler( void )            { for( ;; ) { __NOP(); } }
EXTERN_C void DMA2D_IRQHandler( void )              { for( ;; ) { __NOP(); } }
EXTERN_C void SAI2_IRQHandler( void )               { for( ;; ) { __NOP(); } }
EXTERN_C void QUADSPI_IRQHandler( void )            { for( ;; ) { __NOP(); } }
EXTERN_C void LPTIM1_IRQHandler( void )             { for( ;; ) { __NOP(); } }
EXTERN_C void CEC_IRQHandler( void )                { for( ;; ) { __NOP(); } }
EXTERN_C void I2C4_EV_IRQHandler( void )            { for( ;; ) { __NOP(); } }
EXTERN_C void I2C4_ER_IRQHandler( void )            { for( ;; ) { __NOP(); } }
EXTERN_C void SPDIF_RX_IRQHandler( void )           { for( ;; ) { __NOP(); } }

EXTERN_C void vListInsertGeneric( List_t * const pxList,
								ListItem_t * const pxNewListItem,
								MiniListItem_t * const pxWhere );
__attribute__((weak)) void vListInsertGeneric( List_t * const pxList,
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

void Set_LED_Number( BaseType_t xNumber, BaseType_t xValue )
{
}

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
	configASSERT( 0 );
	return 0;
}

#include "E:\Home\amazon-freertos\ipv6\amazon-freertos\libraries\freertos_plus\standard\freertos_plus_tcp\tools\tcp_utilities\tcp_dump_packets.c"

extern time_t time ( time_t *puxTime );
time_t time ( time_t *puxTime )
{
	time_t rc = FreeRTOS_time( puxTime );
	return rc;
}

void sdcard_test(void);
void sdcard_test(void)
{
}

void HAL_SD_MspInit( SD_HandleTypeDef * hsd )
{
    ( void ) hsd;
    /* NOTE : This function Should not be modified, when the callback is needed,
     *        the HAL_SD_MspInit could be implemented in the user file
     */
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
