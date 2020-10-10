/*
	FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
	All rights reserved
*/

/*
 * Instructions for using this project are provided on:
 * http://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/examples_FreeRTOS_simulator.html
 */


/* Standard includes. */
#include <stdio.h>
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
#include "FreeRTOS_DHCP.h"
#include "FreeRTOS_DNS.h"
#include "NetworkBufferManagement.h"
#include "FreeRTOS_ARP.h"
#include "FreeRTOS_IP_Private.h"

#if( ipconfigMULTI_INTERFACE != 0 )
	#include "FreeRTOS_Routing.h"
	#if( ipconfigUSE_IPv6 != 0 )
		#include "FreeRTOS_ND.h"
	#endif
#endif
#include "FreeRTOS_tcp_server.h"
#include "NetworkInterface.h"

#if( USE_PLUS_FAT != 0 )
	/* FreeRTOS+FAT includes. */
	#include "ff_headers.h"
	#include "ff_stdio.h"
	#include "ff_ramdisk.h"
	#include "ff_sddisk.h"
#endif

/* Demo application includes. */
#include "hr_gettime.h"

#include "UDPLoggingPrintf.h"

#include "eventLogging.h"

#if( USE_IPERF != 0 )
	#include "iperf_task.h"
#endif


#if( USE_TELNET != 0 )
	#include "telnet.h"
#endif

#if( ipconfigTCP_IP_SANITY != 0 )
	#warning ipconfigTCP_IP_SANITY is defined
#endif

/* ST includes. */
#include "stm32f4xx_hal.h"

BaseType_t xRandom32( uint32_t *pulValue );

#include "plus_echo_server.h"

//#include "cmsis_os.h"
/* FTP and HTTP servers execute in the TCP server work task. */
#define mainTCP_SERVER_TASK_PRIORITY	( tskIDLE_PRIORITY + 3 )
#define	mainTCP_SERVER_STACK_SIZE		2048

#define	mainFAT_TEST_STACK_SIZE			2048

#define mainRUN_STDIO_TESTS				0
#define mainHAS_RAMDISK					0
#define mainHAS_SDCARD					1
#define mainSD_CARD_DISK_NAME			"/"
#define mainSD_CARD_TESTING_DIRECTORY	"/fattest"

/* Remove this define if you don't want to include the +FAT test code. */
#define mainHAS_FAT_TEST				USE_PLUS_FAT

/* Set the following constants to 1 to include the relevant server, or 0 to
exclude the relevant server. */
#define mainCREATE_FTP_SERVER			USE_PLUS_FAT
#define mainCREATE_HTTP_SERVER 			USE_PLUS_FAT


/* Define names that will be used for DNS, LLMNR and NBNS searches. */
#define mainHOST_NAME					"stm"
/* http://stm32f4/index.html */
#define mainDEVICE_NICK_NAME			"stm32f4"

int PING_COUNT_MAX = 10;

extern void vApplicationPingReplyHook( ePingReplyStatus_t eStatus, uint16_t usIdentifier );
#if( ipconfigUSE_IPv6 != 0 )
	static IPv6_Address_t xPing6IPAddress;
	volatile BaseType_t xPing6Count = -1;
#endif
uint32_t ulPingIPAddress;
volatile BaseType_t xPing4Count = -1;
volatile BaseType_t xPingReady;
static int pingLogging = pdFALSE;

extern const char *pcApplicationHostnameHook( void );

static SemaphoreHandle_t xServerSemaphore;


/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc3;

DAC_HandleTypeDef hdac;

DCMI_HandleTypeDef hdcmi;

I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

#define USE_USART3_LOGGING 0

#if( USE_USART3_LOGGING )
	UART_HandleTypeDef huart3;
#endif

RNG_HandleTypeDef hrng;

NOR_HandleTypeDef hnor1;
SRAM_HandleTypeDef hsram2;
SRAM_HandleTypeDef hsram3;
//osThreadId defaultTaskHandle;

uint8_t retSD;    /* Return value for SD */
char SD_Path[4];  /* SD logical drive path */

int verboseLevel;

typedef enum {
	eIPv4 = 0x01,
	eIPv6 = 0x02,
} eIPVersion;

eIPVersion allowIPVersion = eIPv4 | eIPv6;

#if( USE_PLUS_FAT != 0 )
FF_Disk_t *pxSDDisk;
#endif

#if( mainHAS_RAMDISK != 0 )
	static FF_Disk_t *pxRAMDisk;
#endif

#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
static TCPServer_t *pxTCPServer = NULL;
#endif

#ifndef ARRAY_SIZE
#	define	ARRAY_SIZE( x )	( int )( sizeof( x ) / sizeof( x )[ 0 ] )
#endif

/*
 * Just seeds the simple pseudo random number generator.
 */
static void prvSRand( UBaseType_t ulSeed );
uint32_t ulInitialSeed;

/* Private function prototypes -----------------------------------------------*/

/*
 * Creates a RAM disk, then creates files on the RAM disk.  The files can then
 * be viewed via the FTP server and the command line interface.
 */
static void prvCreateDiskAndExampleFiles( void );

extern void vStdioWithCWDTest( const char *pcMountPath );
extern void vCreateAndVerifyExampleFiles( const char *pcMountPath );

static void checksum_test();

#if( ipconfigUSE_CALLBACKS != 0 )
static BaseType_t xOnUdpReceive( Socket_t xSocket, void * pvData, size_t xLength,
	const struct freertos_sockaddr *pxFrom, const struct freertos_sockaddr *pxDest );
#endif

/*
 * The task that runs the FTP and HTTP servers.
 */
static void prvServerWorkTask( void *pvParameters );

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC3_Init(void);
static void MX_DAC_Init(void);
static void MX_DCMI_Init(void);
static void MX_FSMC_Init(void);
static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
#if( USE_USART3_LOGGING )
	static void MX_USART3_UART_Init(void);
#endif

static void heapInit( void );

void vShowTaskTable( BaseType_t aDoClear );
#if( USE_IPERF != 0 )
	void vIPerfInstall( void );
#endif

/* The default IP and MAC address used by the demo.  The address configuration
defined here will be used if ipconfigUSE_DHCP is 0, or if ipconfigUSE_DHCP is
1 but a DHCP server could not be contacted.  See the online documentation for
more information. */
static const uint8_t ucIPAddress[ 4 ] = { configIP_ADDR0, configIP_ADDR1, configIP_ADDR2, configIP_ADDR3 };
static const uint8_t ucNetMask[ 4 ] = { configNET_MASK0, configNET_MASK1, configNET_MASK2, configNET_MASK3 };
static const uint8_t ucGatewayAddress[ 4 ] = { configGATEWAY_ADDR0, configGATEWAY_ADDR1, configGATEWAY_ADDR2, configGATEWAY_ADDR3 };
static const uint8_t ucDNSServerAddress[ 4 ] = { configDNS_SERVER_ADDR0, configDNS_SERVER_ADDR1, configDNS_SERVER_ADDR2, configDNS_SERVER_ADDR3 };

uint32_t ulServerIPAddress;

/* Default MAC address configuration.  The demo creates a virtual network
connection that uses this MAC address by accessing the raw Ethernet data
to and from a real network connection on the host PC.  See the
configNETWORK_INTERFACE_TO_USE definition for information on how to configure
the real network connection to use. */
#if( ipconfigMULTI_INTERFACE == 0 )
	const uint8_t ucMACAddress[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };
#else
	static const uint8_t ucIPAddress2[ 4 ] = { 172, 16, 0, 100 };
	static const uint8_t ucNetMask2[ 4 ] = { 255, 255, 0, 0 };
	static const uint8_t ucGatewayAddress2[ 4 ] = { 0, 0, 0, 0};
	static const uint8_t ucDNSServerAddress2[ 4 ] = { 0, 0, 0, 0 };

	static const uint8_t ucIPAddress3[ 4 ] = { 127, 0, 0, 1 };	// 127.0.0.1
	static const uint8_t ucNetMask3[ 4 ] = { 255, 0, 0, 0 };
	static const uint8_t ucGatewayAddress3[ 4 ] = { 0, 0, 0, 0};
	static const uint8_t ucDNSServerAddress3[ 4 ] = { 0, 0, 0, 0 };

	static const uint8_t ucMACAddress1[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };
	static const uint8_t ucMACAddress2[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 + 1 };
	static const uint8_t ucMACAddress3[ 6 ] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x7f };
#endif /* ipconfigMULTI_INTERFACE */

/* Use by the pseudo random number generator. */
static UBaseType_t ulNextRand;

/* Handle of the task that runs the FTP and HTTP servers. */
static TaskHandle_t xServerWorkTaskHandle = NULL;

#if( ipconfigMULTI_INTERFACE != 0 )
	NetworkInterface_t *pxSTM32Fxx_FillInterfaceDescriptor( BaseType_t xEMACIndex, NetworkInterface_t *pxInterface );
	NetworkInterface_t *pxLoopback_FillInterfaceDescriptor( BaseType_t xEMACIndex, NetworkInterface_t *pxInterface );
#endif /* ipconfigMULTI_INTERFACE */
/*-----------------------------------------------------------*/

#if( ipconfigMULTI_INTERFACE != 0 )
NetworkInterface_t *pxSTMF40_IPInit(
	BaseType_t xEMACIndex, NetworkInterface_t *pxInterface );
#endif

#if( ipconfigMULTI_INTERFACE != 0 )
	static NetworkInterface_t xInterfaces[ 2 ];
	static NetworkEndPoint_t xEndPoints[ 4 ];
#endif /* ipconfigMULTI_INTERFACE */

int main( void )
{
uint32_t ulSeed = 0x5a5a5a5a;

	/* MCU Configuration----------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* Configure the system clock */
	//SystemClock_Config();

	heapInit( );

#if USE_LOG_EVENT
	iEventLogInit();
	eventLogAdd("Main");
#endif

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_ADC3_Init();
	MX_DAC_Init();
	MX_DCMI_Init();
	MX_FSMC_Init();
	MX_I2C1_Init();
	MX_RTC_Init();
	#if( USE_USART3_LOGGING )
	{
		MX_USART3_UART_Init();
	}
	#endif

	/* Timer2 initialization function.
	ullGetHighResolutionTime() will be used to get the running time in uS. */
	vStartHighResolutionTimer();

	hrng.Instance = RNG;
	HAL_RNG_Init( &hrng );
	xRandom32( &ulSeed );
	prvSRand( ulSeed );
	ulInitialSeed = ulSeed;

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
	pxSTM32Fxx_FillInterfaceDescriptor( 0, &( xInterfaces[0] ) );
//	FreeRTOS_AddNetworkInterface( &( xInterfaces[0] ) );

	/*
	 * End-point-1  // private + public
	 *     Network: 192.168.2.x/24
	 *     IPv4   : 192.168.2.12
	 *     Gateway: 192.168.2.1 ( NAT router )
	 */
	NetworkEndPoint_t *pxEndPoint_0 = &( xEndPoints[ 0 ] );
	NetworkEndPoint_t *pxEndPoint_1 = &( xEndPoints[ 1 ] );
	NetworkEndPoint_t *pxEndPoint_2 = &( xEndPoints[ 2 ] );
	NetworkEndPoint_t *pxEndPoint_3 = &( xEndPoints[ 3 ] );
	{
		//FreeRTOS_FillEndPoint( &( xInterfaces[0] ), pxEndPoint_0, ucIPAddress2, ucNetMask2, ucGatewayAddress2, ucDNSServerAddress2, ucMACAddress1 );

		FreeRTOS_FillEndPoint( &( xInterfaces[0] ), pxEndPoint_1, ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress1 );
		pxEndPoint_1->bits.bWantDHCP = pdTRUE;
	}


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

			FreeRTOS_inet_pton6( "fe80::", xPrefix.ucBytes );
			FreeRTOS_inet_pton6( "2001:470:ec54::", xPrefix.ucBytes );

			FreeRTOS_CreateIPv6Address( &xIPAddress, &xPrefix, 64, pdTRUE );

			FreeRTOS_FillEndPoint_IPv6( &( xInterfaces[0] ), 
										pxEndPoint_2,
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

			FreeRTOS_inet_pton6( "2001:470:ec54::", xPrefix.ucBytes );

			FreeRTOS_CreateIPv6Address( &xIPAddress, &xPrefix, 64, pdTRUE );
			FreeRTOS_inet_pton6( "fe80::9355:69c7:585a:afe7", xGateWay.ucBytes );

			FreeRTOS_FillEndPoint_IPv6( &( xInterfaces[0] ), 
										pxEndPoint_3,
										&( xIPAddress ),
										&( xPrefix ),
										64uL,			/* Prefix length. */
										&( xGateWay ),
										NULL,			/* pxDNSServerAddress: Not used yet. */
										ucMACAddress1 );
		}
	}
	#endif

//	FreeRTOS_IPStart();
#endif /* ipconfigMULTI_INTERFACE */

	/* Create the task that handles the FTP and HTTP servers.  This will
	initialise the file system then wait for a notification from the network
	event hook before creating the servers.  The task is created at the idle
	priority, and sets itself to mainTCP_SERVER_TASK_PRIORITY after the file
	system has initialised. */
	xTaskCreate( prvServerWorkTask, "SvrWork", mainTCP_SERVER_STACK_SIZE, NULL, tskIDLE_PRIORITY, &xServerWorkTaskHandle );

	/* Start the RTOS scheduler. */
	FreeRTOS_debug_printf( ("vTaskStartScheduler\n") );
eventLogAdd("vTaskStartScheduler");
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

BaseType_t xRandom32( uint32_t *pulValue )
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
#if( mainHAS_RAMDISK != 0 )
	static uint8_t ucRAMDisk[ mainRAM_DISK_SECTORS * mainRAM_DISK_SECTOR_SIZE ];
#endif

	verboseLevel = 0;
	#if( mainHAS_RAMDISK != 0 )
	{
		FreeRTOS_printf( ( "Create RAM-disk\n" ) );
		/* Create the RAM disk. */
		pxRAMDisk = FF_RAMDiskInit( mainRAM_DISK_NAME, ucRAMDisk, mainRAM_DISK_SECTORS, mainIO_MANAGER_CACHE_SIZE );
		configASSERT( pxRAMDisk );

		/* Print out information on the RAM disk. */
		FF_RAMDiskShowPartition( pxRAMDisk );
	}
	#endif	/* mainHAS_RAMDISK */
	#if( mainHAS_SDCARD != 0 ) && ( USE_PLUS_FAT != 0 )
	{
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
			ff_mkdir( mainSD_CARD_TESTING_DIRECTORY );

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

#if( USE_PLUS_FAT != 0 )
	static int iFATRunning = 0;

	static void prvFileSystemAccessTask( void *pvParameters )
	{
	const char *pcBasePath = ( const char * ) pvParameters;

		while( iFATRunning != 0 )
		{
			ff_deltree( pcBasePath );
			ff_mkdir( pcBasePath );
			vCreateAndVerifyExampleFiles( pcBasePath );
			vStdioWithCWDTest( pcBasePath );
		}
		FreeRTOS_printf( ( "%s: done\n", pcBasePath ) );
		vTaskDelete( NULL );
	}
}
#endif	/* USE_PLUS_FAT */

void onDNSEvent( const char * pcName, void *pvSearchID, uint32_t ulIPAddress )
{
	FreeRTOS_printf( ( "onDNSEvent: found '%s' on %lxip\n", pcName, FreeRTOS_ntohl( ulIPAddress ) ) );
}

#if( USE_TELNET != 0 )
	Telnet_t *pxTelnetHandle = NULL;

	void vUDPLoggingHook( const char *pcMessage, BaseType_t xLength )
	{
		if (pxTelnetHandle != NULL && pxTelnetHandle->xClients != NULL) {
			xTelnetSend (pxTelnetHandle, NULL, pcMessage, xLength );
		}
		#if( USE_USART3_LOGGING != 0 )
		//HAL_USART_Transmit(&huart3, (uint8_t *)pcMessage, xLength, 100);
		HAL_UART_Transmit(&huart3, (uint8_t *)pcMessage, xLength, 100);
		#endif
	}
#endif

#if( ipconfigUSE_DHCP_HOOK != 0 )
eDHCPCallbackAnswer_t xApplicationDHCPHook( eDHCPCallbackPhase_t eDHCPPhase, uint32_t ulIPAddress )
{
	const char *name = "Unknown";
	switch( eDHCPPhase )
	{
	case eDHCPPhasePreDiscover:	name = "Discover"; break;	/* Driver is about to send a DHCP discovery. */
	case eDHCPPhasePreRequest:	name = "Request"; break;	/* Driver is about to request DHCP an IP address. */
	}
	FreeRTOS_printf( ( "DHCP %s address %lxip\n", name, FreeRTOS_ntohl( ulIPAddress ) ) );
	return eDHCPContinue;
}
#endif	/* ipconfigUSE_DHCP_HOOK */

#if 1
static void prvServerWorkTask( void *pvParameters )
{
#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
	const TickType_t xInitialBlockTime = pdMS_TO_TICKS( 200UL );
	BaseType_t xWasPresent, xIsPresent;
#endif
BaseType_t xHadSocket = pdFALSE;

eventLogAdd("prvServerWorkTask");

/* A structure that defines the servers to be created.  Which servers are
included in the structure depends on the mainCREATE_HTTP_SERVER and
mainCREATE_FTP_SERVER settings at the top of this file. */
#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
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
#endif

	/* Remove compiler warning about unused parameter. */
	( void ) pvParameters;

#if( ipconfigMULTI_INTERFACE == 0 )
	FreeRTOS_IPInit( ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress );
#else
	FreeRTOS_IPStart();
#endif

	#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
	{
	/* Create the disk used by the FTP and HTTP servers. */
		FreeRTOS_printf( ( "Creating files\n" ) );
		prvCreateDiskAndExampleFiles();
		xIsPresent = xWasPresent = FF_SDDiskDetect( pxSDDisk );
		FreeRTOS_printf( ( "FF_SDDiskDetect returns -> %ld\n", xIsPresent  ) );
	}
	#endif

	/* If the CLI is included in the build then register commands that allow
	the file system to be accessed. */
	#if( mainCREATE_UDP_CLI_TASKS == 1 )
	{
		vRegisterFileSystemCLICommands();
	}
	#endif /* mainCREATE_UDP_CLI_TASKS */



	/* The priority of this task can be raised now the disk has been
	initialised. */
	vTaskPrioritySet( NULL, mainTCP_SERVER_TASK_PRIORITY );

	/* Wait until the network is up before creating the servers.  The
	notification is given from the network event hook. */
	ulTaskNotifyTake( pdTRUE, portMAX_DELAY );


	FreeRTOS_printf( ( "prvServerWorkTask starts running\n" ) );

	/* Create the servers defined by the xServerConfiguration array above. */
#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
	pxTCPServer = FreeRTOS_CreateTCPServer( xServerConfiguration, sizeof( xServerConfiguration ) / sizeof( xServerConfiguration[ 0 ] ) );
	configASSERT( pxTCPServer );
#endif

	#if( CONFIG_USE_LWIP != 0 )
	{
		vLWIP_Init();
	}
	#endif
	xServerSemaphore = xSemaphoreCreateBinary();
	configASSERT( xServerSemaphore != NULL );

	#if( USE_TELNET != 0 )
	Telnet_t xTelnet;
	memset( &xTelnet, '\0', sizeof xTelnet );
	#endif

	for( ;; )
	{
	TickType_t xReceiveTimeOut = pdMS_TO_TICKS( 200 );

		xSemaphoreTake( xServerSemaphore, xReceiveTimeOut );
		if( xPingReady )
		{
			xPingReady = pdFALSE;
			if( xPing4Count >= 0 && xPing4Count < PING_COUNT_MAX )
			{
				FreeRTOS_SendPingRequest( ulPingIPAddress, 10, 10 );
			}
		#if( ipconfigUSE_IPv6 != 0 )
			if( xPing6Count >= 0 && xPing6Count < PING_COUNT_MAX )
			{
				FreeRTOS_SendPingRequestIPv6( &xPing6IPAddress, 10, 10 );
			}
		#endif
		}
		#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
		{
			xIsPresent = FF_SDDiskDetect( pxSDDisk );
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
	//				FreeRTOS_printf( ( "SD-card still %d\n", xIsPresent ) );
			}
			FreeRTOS_TCPServerWork( pxTCPServer, xInitialBlockTime );
		}
		#endif /* mainCREATE_FTP_SERVER == 1 || mainCREATE_HTTP_SERVER == 1 */

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

		if( xSocket != NULL)
		{
		char cBuffer[ 64 ];
		BaseType_t xCount;

			if( xHadSocket == pdFALSE )
			{
				xHadSocket = pdTRUE;
				FreeRTOS_printf( ( "prvCommandTask started\n" ) );
				/* xServerSemaphore will be given to when there is data for xSocket
				and also as soon as there is USB/CDC data. */
				FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_SET_SEMAPHORE, ( void * ) &xServerSemaphore, sizeof( xServerSemaphore ) );
				//FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof( xReceiveTimeOut ) );
				#if( ipconfigUSE_CALLBACKS != 0 )
				{
					F_TCP_UDP_Handler_t xHandler;
					memset( &xHandler, '\0', sizeof ( xHandler ) );
					xHandler.pOnUdpReceive = xOnUdpReceive;
					FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_UDP_RECV_HANDLER, ( void * ) &xHandler, sizeof( xHandler ) );
				}
				#endif
				#if( USE_TELNET != 0 )
				{
					xTelnetCreate( &xTelnet, TELNET_PORT_NUMBER );
					if( xTelnet.xParentSocket != 0 )
					{
						FreeRTOS_setsockopt( xTelnet.xParentSocket, 0, FREERTOS_SO_SET_SEMAPHORE, ( void * ) &xServerSemaphore, sizeof( xServerSemaphore ) );
						pxTelnetHandle = &xTelnet;
					}
				}
				#endif
			}
			xCount = FreeRTOS_recvfrom( xSocket, ( void * )cBuffer, sizeof( cBuffer ) - 1, FREERTOS_MSG_DONTWAIT,
				&xSourceAddress, &xSourceAddressLength );
			#if( USE_TELNET != 0 )
			{
				if( ( xCount <= 0 ) && ( xTelnet.xParentSocket != NULL ) )
				{
					struct freertos_sockaddr fromAddr;
					xCount = xTelnetRecv( &xTelnet, &fromAddr, cBuffer, sizeof( cBuffer ) - 1 );
					if( xCount > 0 )
					{
						xTelnetSend( &xTelnet, &fromAddr, cBuffer, xCount );
					}
				}
			}
			#endif
			if( xCount > 0 )
			{
				cBuffer[ xCount ] = '\0';
				FreeRTOS_printf( ( ">> %s\n", cBuffer ) );
				if( strncmp( cBuffer, "ver", 3 ) == 0 )
				{
				int level;

					if( sscanf( cBuffer + 3, "%d", &level ) == 1 )
					{
						verboseLevel = level;
					}
					FreeRTOS_printf( ( "Verbose level %d  port %d IP %lxip seed %08lx\n",
						verboseLevel,
						FreeRTOS_ntohs( xSourceAddress.sin_port ),
						FreeRTOS_ntohl( xSourceAddress.sin_addr ),
						ulInitialSeed ) );
				} else if( strncmp( cBuffer, "rand", 4 ) == 0 ) {
					uint32_t ulNumber = 0x5a5a5a5a;
					BaseType_t rc = xRandom32( &ulNumber );
					if (rc == pdPASS )
					{
						char buffer[33];
						int index;
						uint32_t ulMask = 0x80000000uL;
						for( index = 0; index < 32; index++ )
						{
							buffer[index] = ((ulNumber & ulMask) != 0) ? '1' : '0';;
							ulMask >>= 1;
						}
						buffer[index] = '\0';
						FreeRTOS_printf( ("Random %08lx (%s)\n", ulNumber, buffer ) );
					}
					else
					{
						FreeRTOS_printf( ("Random failed\n" ) );
					}
			#if( ipconfigUSE_IPv6 != 0 )
				} else if( strncmp( cBuffer, "whatismyip", 4 ) == 0 ) {
					NetworkEndPoint_t *pxEndPoint;

					for( pxEndPoint = FreeRTOS_FirstEndPoint( NULL );
						pxEndPoint != NULL;
						pxEndPoint = FreeRTOS_NextEndPoint( NULL, pxEndPoint ) )
					{
						if( pxEndPoint->bits.bIPv6 )
						{
							FreeRTOS_printf( ( "%pip\n", pxEndPoint->ipv6.xIPAddress.ucBytes ) );
						}
					}
			#endif
				} else if( strncmp( cBuffer, "udp", 3 ) == 0 ) {
					const char pcMessage[] = "1234567890";
					const char *ptr = cBuffer + 3;
					while (*ptr && !isspace (*ptr)) ptr++;
					while (isspace (*ptr)) ptr++;
					if (*ptr) {
					} else {
						ptr = pcMessage;
					}

					#if( ipconfigUSE_IPv6 != 0 )
					{
						const char *ip_address[] = {
							"fe80::9355:69c7:585a:afe7",	/* raspberry ff02::1:ff5a:afe7, 33:33:ff:5a:af:e7 */
							"fe80::6816:5e9b:80a0:9edb",	/* laptop Hein */
						};
						struct freertos_sockaddr6 xAddress;
						memset( &xAddress, '\0', sizeof xAddress );
						xAddress.sin_len = sizeof( xAddress );		/* length of this structure. */
						xAddress.sin_family = FREERTOS_AF_INET6;
						xAddress.sin_port = FreeRTOS_htons( configUDP_LOGGING_PORT_REMOTE );
						xAddress.sin_flowinfo = 0;	/* IPv6 flow information. */
						FreeRTOS_inet_pton6( ip_address[ 1 ], xAddress.sin_addrv6.ucBytes );
						FreeRTOS_sendto( xSocket, ptr, strlen( ptr ), 0, ( const struct freertos_sockaddr * ) &( xAddress ), sizeof( xAddress ) );
					}
					#endif

					{
						struct freertos_sockaddr xAddress_IPv4;
						xAddress_IPv4.sin_family = FREERTOS_AF_INET4;
						xAddress_IPv4.sin_len = sizeof( xAddress_IPv4 );		/* length of this structure. */
						xAddress_IPv4.sin_port = FreeRTOS_htons( configUDP_LOGGING_PORT_REMOTE );
						xAddress_IPv4.sin_addr = FreeRTOS_inet_addr_quick( 192, 168, 2, 5 );

						FreeRTOS_sendto( xSocket, ptr, strlen( ptr ), 0, &( xAddress_IPv4 ), sizeof( xAddress_IPv4 ) );
					}
			#if( ipconfigUSE_IPv6 != 0 )
				} else if( strncmp( cBuffer, "address", 7 ) == 0 ) {
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
			#endif
			#if( ipconfigUSE_IPv6 != 0 )
				} else if( strncmp( cBuffer, "test2", 5 ) == 0 ) {
					IPv6_Address_t xLeft, xRight;
					struct SPair {
						const char *pcLeft;
						const char *pcRight;
						int prefixLength;
					};
					struct SPair pairs[] = {
						{  "2001:470:ec54::",  "2001:470:ec54::", 64 },
						{  "fe80::",           "fe80::"         , 10 },
						{  "fe80::",           "feBF::"         , 10 },
						{  "fe80::",           "feff::"         , 10 },
					};
					int index;
					for( index = 0; index < ARRAY_SIZE( pairs ); index++) {
					BaseType_t xResult;
					
						FreeRTOS_inet_pton6( pairs[index].pcLeft, xLeft.ucBytes );
						FreeRTOS_inet_pton6( pairs[index].pcRight, xRight.ucBytes );
						xResult = xCompareIPv6_Address( &xLeft, &xRight, 10 );
						FreeRTOS_printf( ( "Compare %pip with %pip = %s\n", xLeft.ucBytes, xRight.ucBytes, xResult ? "differ" : "same" ) );
					}
			#endif /* ( ipconfigUSE_IPv6 != 0 ) */
				} else if( strncmp( cBuffer, "test", 4 ) == 0 ) {
					extern BaseType_t call_nd_test;
					call_nd_test++;
					FreeRTOS_printf( ("call_nd_test=%ld\n", call_nd_test ) );
				} else if( strncmp( cBuffer, "dnsq", 4 ) == 0 ) {
					struct freertos_addrinfo *pxResult = NULL;
					struct freertos_addrinfo xHints;
					#if( ipconfigUSE_LLMNR == 0 )
					{
						FreeRTOS_printf( ( "Warning: LLMNR not enabled\n" ) );
					}
					#endif
					char *ptr = cBuffer + 4;
					UBaseType_t uxHostType = dnsTYPE_A_HOST;
					xHints.ai_family = FREERTOS_AF_INET4;
					for (; *ptr && !isspace (*ptr); ptr++) {
						if( ptr[0] == '6' ) {
							uxHostType = dnsTYPE_AAAA_HOST;
							xHints.ai_family = FREERTOS_AF_INET6;
						} else if( ptr[0] == 'c' ) {
							#if( ipconfigUSE_DNS_CACHE != 0 )
							{
								FreeRTOS_dnsclear();
							}
							#endif /* ipconfigUSE_DNS_CACHE */
							FreeRTOS_ClearARP();
						}
					}
					while (isspace (*ptr)) ptr++;
					if (*ptr) {
						#if( ipconfigDNS_USE_CALLBACKS != 0 )
						void *pvSearchID = ( void *)ipconfigRAND32();
							uint32_t ulIPAddress = FreeRTOS_getaddrinfo_a(
								ptr,		/* The node. */
								NULL,		/* const char *pcService: ignored for now. */
								&xHints,	/* If not NULL: preferences. */
								&pxResult,	/* An allocated struct, containing the results. */
								onDNSEvent,
								pvSearchID,
								5000);

							FreeRTOS_printf( ( "dns query%d: '%s'\n", uxHostType == dnsTYPE_AAAA_HOST ? 6 : 4, ptr ) );
						#else
							uint32_t ulIPAddress = FreeRTOS_getaddrinfo(
								ptr,			/* The node. */
								NULL,			/* const char *pcService: ignored for now. */
								&xHints,		/* If not NULL: preferences. */
								&pxResult );	/* An allocated struct, containing the results. */
							if( ulIPAddress == 0uL )
							{
								FreeRTOS_printf( ( "dns query%d: '%s' No results\n", uxHostType == dnsTYPE_AAAA_HOST ? 6 : 4, ptr ) );
							}
							else
							{
							#if( ipconfigUSE_IPv6 != 0 )
								if( uxHostType == dnsTYPE_AAAA_HOST )
								{
								struct freertos_sockaddr6 *pxAddr6;
									pxAddr6 = ( struct freertos_sockaddr6 * ) pxResult->ai_addr;

									FreeRTOS_printf( ( "dns query%d: '%s' = %pip\n", uxHostType == dnsTYPE_AAAA_HOST ? 6 : 4, ptr, pxAddr6->sin_addrv6.ucBytes ) );
								}
								else
							#endif
								{
									FreeRTOS_printf( ( "dns query%d: '%s' = %lxip\n", uxHostType == dnsTYPE_AAAA_HOST ? 6 : 4, ptr, FreeRTOS_ntohl( ulIPAddress ) ) );
								}
							#endif
								FreeRTOS_freeaddrinfo( pxResult );
							}
					} else {
						FreeRTOS_printf( ( "Usage: dnsquery <name>\n" ) );
					}
					{
						vTaskDelay( 333 );
					#if( ipconfigUSE_IPv6 != 0 )
						IPv6_Address_t xAddress_IPv6;
					#endif /* ( ipconfigUSE_IPv6 != 0 ) */
						uint32_t ulIpAddress;
					#if( ipconfigUSE_IPv6 != 0 )
						memset( xAddress_IPv6.ucBytes, '\0', sizeof( xAddress_IPv6.ucBytes ) );
						if( uxHostType == dnsTYPE_AAAA_HOST )
						{
							FreeRTOS_dnslookup6( ptr, &( xAddress_IPv6 ) );
							FreeRTOS_printf( ( "Lookup6 '%s' = %pip\n", ptr, xAddress_IPv6.ucBytes ) );
						}
						else
					#endif /* ( ipconfigUSE_IPv6 != 0 ) */
						{
							ulIpAddress = FreeRTOS_dnslookup( ptr );
							FreeRTOS_printf( ( "Lookup4 '%s' = %lxip\n", ptr, FreeRTOS_ntohl( ulIpAddress ) ) );
						}
					}
				} else if( strncmp( cBuffer, "check", 5 ) == 0 ) {
					checksum_test();
			#if( ipconfigUSE_IPv6 != 0 )
				} else if( strncmp( cBuffer, "ping6", 5 ) == 0 ) {
					BaseType_t xDoClear = pdFALSE;
					BaseType_t xResult = pdTRUE;
					BaseType_t isName = (strchr(cBuffer + 5, ':') == NULL);
					IPv6_Address_t xAddress_IPv6;
					char *ptr = cBuffer + 5;
					pingLogging = pdFALSE;
					while (*ptr && !isspace(*ptr)) {
						if (*ptr == 'c')
							xDoClear = pdTRUE;
						if (*ptr == 'v')
							pingLogging = pdTRUE;
						ptr++;
					}

					while (isspace( *ptr )) ptr++;
					if (*ptr == '\0') {
						ptr = "fe80::6816:5e9b:80a0:9edb";
					}
					if( xDoClear )
					{
						FreeRTOS_ClearND();
						FreeRTOS_dnsclear();
						FreeRTOS_printf( ( "Clearing DNS/ND cache\n" ) );
					}
					if (isName) {
						FreeRTOS_printf( ( "ping6: looking up name '%s'\n", ptr ) );
//						xResult = FreeRTOS_gethostbyname( ptr, dnsTYPE_AAAA_HOST );
						xResult = FreeRTOS_gethostbyname( ptr );
						xResult = FreeRTOS_dnslookup6( ptr, &( xAddress_IPv6 ) );
						if ( xResult )
						{
							ptr = cBuffer + 5;
							sprintf(ptr, "%pip", xAddress_IPv6.ucBytes );
							FreeRTOS_printf( ( "ping6 to '%s'\n", ptr ) );
						}
						else
						{
							FreeRTOS_printf( ( "ping6: '%s' not found\n", ptr ) );
						}
					}

					if ( xResult )
					{
						FreeRTOS_inet_pton6( ptr, xPing6IPAddress.ucBytes );
						xPing4Count = -1;
						xPing6Count = 0;
						FreeRTOS_SendPingRequestIPv6( &xPing6IPAddress, 10, 10 );
						if( xDoClear )
						{
							FreeRTOS_SendPingRequestIPv6( &xPing6IPAddress, 10, 10 );
						}
					}
			#endif
				} else if( strncmp( cBuffer, "ping", 4 ) == 0 ) {
					BaseType_t xDoClear = pdFALSE;
					uint32_t ulIPAddress;
					char *ptr = cBuffer + 4;

					pingLogging = pdFALSE;
					PING_COUNT_MAX = 10;

					while (*ptr && !isspace(*ptr)) {
						if (*ptr == 'c')
							xDoClear = pdTRUE;
						if (*ptr == 'v')
							pingLogging = pdTRUE;
						ptr++;
					}
					while (isspace(*ptr)) ptr++;

					#if( ipconfigMULTI_INTERFACE != 0 )
					{
						ulIPAddress = FreeRTOS_inet_addr_quick( ucIPAddress[0], ucIPAddress[1], ucIPAddress[2], ucIPAddress[3] );
					}
					#else
					{
						FreeRTOS_GetAddressConfiguration( &ulIPAddress, NULL, NULL, NULL );
					}
					#endif

					if (*ptr) {
						char *rest = strchr(ptr, ' ');
						if (rest) {
							*(rest++) = '\0';
						}
						ulIPAddress = FreeRTOS_inet_addr( ptr );
						while (*ptr && !isspace(*ptr)) ptr++;
						unsigned count;
						if (rest != NULL && sscanf(rest, "%u", &count) > 0) {
							PING_COUNT_MAX = count;
						}
					}
					FreeRTOS_printf( ( "ping to %lxip\n", FreeRTOS_htonl(ulIPAddress) ) );

					ulPingIPAddress = ulIPAddress;
					xPing4Count = 0;
				#if( ipconfigUSE_IPv6 != 0 )
					xPing6Count = -1;
				#endif /* ( ipconfigUSE_IPv6 != 0 ) */
					xPingReady = pdFALSE;
					if( xDoClear )
					{
						FreeRTOS_ClearARP();
						FreeRTOS_printf( ( "Clearing ARP cache\n" ) );
					}

					FreeRTOS_SendPingRequest( ulIPAddress, 10, 10 );
				} else if( strncmp( cBuffer, "plus", 4 ) == 0 )
				{
					int value = 1;
					char *ptr = cBuffer+4;
					while (*ptr && !isdigit(*ptr)) ptr++;
					if (!ulServerIPAddress)
						ulServerIPAddress = FreeRTOS_inet_addr_quick( ucIPAddress[0], ucIPAddress[1], ucIPAddress[2], ucIPAddress[3] );
					if (*ptr) {
						int value2;
						char ch;
						int rc = sscanf (ptr, "%d%c%d", &value, &ch, &value2);
						if (rc >= 3) {
							ulServerIPAddress = FreeRTOS_inet_addr( ptr );
							value = 1;
						} else if (rc < 1) {
							value = 0;
						}
					}
					plus_echo_start (value);
					FreeRTOS_printf( ( "Plus testing has %s for %lxip\n", value ? "started" : "stopped", ulServerIPAddress ) );
				} else if( strncmp( cBuffer, "netstat", 7 ) == 0 ) {
					FreeRTOS_netstat();
				} else if( strncmp( cBuffer, "allow", 5 ) == 0 ) {
					unsigned value = 0;
					char *ptr = cBuffer+5;
					while (*ptr) {
						if (*ptr == '4') {
							value |= eIPv4;
						}
						if (*ptr == '6') {
							value |= eIPv6;
						}
						ptr++;
					}
					if (value) {
						allowIPVersion = (eIPVersion)value;
					}
					FreeRTOS_printf( ( "Allow%s%s\n",
						(allowIPVersion & eIPv4) ? " IPv4" : "",
						(allowIPVersion & eIPv6) ? " IPv6" : "" ) );
				}
				#if( USE_LOG_EVENT != 0 )
				{
					if( strncmp( cBuffer, "event", 4 ) == 0 )
					{
						if(cBuffer[ 5 ] == 'c')
						{
							int rc = iEventLogClear();
							lUDPLoggingPrintf( "cleared %d events\n", rc );
						}
						else
						{
							eventLogDump();
						}
					}
				}
				#endif /* USE_LOG_EVENT */
				if( strncmp( cBuffer, "list", 4 ) == 0 )
				{
					vShowTaskTable( cBuffer[ 4 ] == 'c' );
				}
				#if( mainHAS_RAMDISK != 0 )
				{
					if( strncmp( cBuffer, "dump", 4 ) == 0 )
					{
						FF_RAMDiskStoreImage( pxRAMDisk, "ram_disk.img" );
					}
				}
				#endif	/* mainHAS_RAMDISK */
				#if( USE_IPERF != 0 )
				{
					if( strncmp( cBuffer, "iperf", 5 ) == 0 )
					{
						vIPerfInstall();
					}
				}
				#endif	/* USE_IPERF */
				#if( USE_PLUS_FAT != 0 )
				if( strncmp( cBuffer, "format", 7 ) == 0 )
				{
					if( pxSDDisk != 0 )
					{
						FF_SDDiskFormat( pxSDDisk, 0 );
					}
				}
				#endif
				#if( mainHAS_FAT_TEST != 0 )
				{
					if( strncmp( cBuffer, "fattest", 7 ) == 0 )
					{
					int level = 0;
					const char pcMountPath[] = "/test";

						if( sscanf( cBuffer + 7, "%d", &level ) == 1 )
						{
							verboseLevel = level;
						}
						FreeRTOS_printf( ( "FAT test %d\n", level ) );
						ff_mkdir( pcMountPath );
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
							ff_mkdir( pcBaseDirectoryNames[ x ] );

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
					}	/* if( strncmp( cBuffer, "fattest", 7 ) == 0 ) */
				}
				#endif /* mainHAS_FAT_TEST */
			}	/* if( xCount > 0 ) */
		}	/* if( xSocket != NULL) */
	}
}
/*-----------------------------------------------------------*/
#endif

uint32_t echoServerIPAddress()
{
	return ulServerIPAddress;
}
/*-----------------------------------------------------------*/

uint32_t HAL_GetTick()
{
	if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
	{
		static uint32_t ulCounter;
		ulCounter++;
		return ulCounter / 1000;
	}
	else
	{
		return (uint32_t)xTaskGetTickCount();
	}
}

void vApplicationIdleHook( void )
{
}
/*-----------------------------------------------------------*/

volatile const char *pcAssertedFileName;
volatile int iAssertedErrno;
volatile uint32_t ulAssertedLine;
#if( USE_PLUS_FAT != 0 )
volatile FF_Error_t xAssertedFF_Error;
#endif
void vAssertCalled( const char *pcFile, uint32_t ulLine )
{
volatile uint32_t ulBlockVariable = 0UL;

	ulAssertedLine = ulLine;
#if( USE_PLUS_FAT != 0 )
	iAssertedErrno = stdioGET_ERRNO();
	xAssertedFF_Error = stdioGET_FF_ERROR( );
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
	this function to be exited. */
	taskDISABLE_INTERRUPTS();
	{
		while( ulBlockVariable == 0UL )
		{
			__asm volatile( "NOP" );
		}
	}
	taskENABLE_INTERRUPTS();
}
/*-----------------------------------------------------------*/


/** System Clock Configuration
*/
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct;
	RCC_ClkInitTypeDef RCC_ClkInitStruct;
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct;

	__PWR_CLK_ENABLE();

	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.LSIState = RCC_LSI_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 25;
	RCC_OscInitStruct.PLL.PLLN = 336;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 7;
	HAL_RCC_OscConfig(&RCC_OscInitStruct);

	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1
							  |RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
	HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);

	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
	PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
	HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

	HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSE, RCC_MCODIV_1);
}
/*-----------------------------------------------------------*/

/* ADC3 init function */
void MX_ADC3_Init(void)
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
	HAL_ADC_Init(&hadc3);

	/**Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
	*/
	sConfig.Channel = ADC_CHANNEL_7;
	sConfig.Rank = 1;
	sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
	HAL_ADC_ConfigChannel(&hadc3, &sConfig);
}
/*-----------------------------------------------------------*/

/* DAC init function */
void MX_DAC_Init(void)
{
	DAC_ChannelConfTypeDef sConfig;

	/**DAC Initialization
	*/
	hdac.Instance = DAC;
	HAL_DAC_Init(&hdac);

	/**DAC channel OUT1 config
	*/
	sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
	sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
	HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1);
}
/*-----------------------------------------------------------*/

/* DCMI init function */
void MX_DCMI_Init(void)
{
	hdcmi.Instance = DCMI;
	hdcmi.Init.SynchroMode = DCMI_SYNCHRO_HARDWARE;
	hdcmi.Init.PCKPolarity = DCMI_PCKPOLARITY_FALLING;
	hdcmi.Init.VSPolarity = DCMI_VSPOLARITY_LOW;
	hdcmi.Init.HSPolarity = DCMI_HSPOLARITY_LOW;
	hdcmi.Init.CaptureRate = DCMI_CR_ALL_FRAME;
	hdcmi.Init.ExtendedDataMode = DCMI_EXTEND_DATA_8B;
	hdcmi.Init.JPEGMode = DCMI_JPEG_DISABLE;
	HAL_DCMI_Init(&hdcmi);
}
/*-----------------------------------------------------------*/

/* I2C1 init function */
void MX_I2C1_Init(void)
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
	HAL_I2C_Init(&hi2c1);
}
/*-----------------------------------------------------------*/

/* RTC init function */
void MX_RTC_Init(void)
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
	HAL_RTC_Init(&hrtc);

	sTime.Hours = 0;
	sTime.Minutes = 0;
	sTime.Seconds = 0;
	sTime.SubSeconds = 0;
	sTime.TimeFormat = RTC_HOURFORMAT12_AM;
	sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sTime.StoreOperation = RTC_STOREOPERATION_RESET;
	HAL_RTC_SetTime(&hrtc, &sTime, FORMAT_BCD);

	sDate.WeekDay = RTC_WEEKDAY_MONDAY;
	sDate.Month = RTC_MONTH_JANUARY;
	sDate.Date = 1;
	sDate.Year = 0;
	HAL_RTC_SetDate(&hrtc, &sDate, FORMAT_BCD);

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
	HAL_RTC_SetAlarm(&hrtc, &sAlarm, FORMAT_BCD);
}
/*-----------------------------------------------------------*/

/* USART3 init function */
#if( USE_USART3_LOGGING )
	void MX_USART3_UART_Init(void)
	{
		huart3.Instance = USART3;
		huart3.Init.BaudRate = 115200;
		huart3.Init.WordLength = UART_WORDLENGTH_8B;
		huart3.Init.StopBits = UART_STOPBITS_1;
		huart3.Init.Parity = UART_PARITY_NONE;
		huart3.Init.Mode = UART_MODE_TX_RX;
		huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
		huart3.Init.OverSampling = UART_OVERSAMPLING_16;
		HAL_UART_Init(&huart3);
	}
#endif
/*-----------------------------------------------------------*/

/** Configure pins
	PE2   ------> SYS_TRACECLK
	PB5   ------> USB_OTG_HS_ULPI_D7
	PE5   ------> SYS_TRACED2
	PE6   ------> SYS_TRACED3
	PA12   ------> USB_OTG_FS_DP
	PI3   ------> I2S2_SD
	PA11   ------> USB_OTG_FS_DM
	PA10   ------> USB_OTG_FS_ID
	PI11   ------> USB_OTG_HS_ULPI_DIR
	PI0   ------> I2S2_WS
	PA9   ------> USB_OTG_FS_VBUS
	PA8   ------> RCC_MCO_1
	PH4   ------> USB_OTG_HS_ULPI_NXT
	PG7   ------> USART6_CK
	PC0   ------> USB_OTG_HS_ULPI_STP
	PA5   ------> USB_OTG_HS_ULPI_CK
	PB12   ------> USB_OTG_HS_ULPI_D5
	PB13   ------> USB_OTG_HS_ULPI_D6
	PA3   ------> USB_OTG_HS_ULPI_D0
	PB1   ------> USB_OTG_HS_ULPI_D2
	PB0   ------> USB_OTG_HS_ULPI_D1
	PB10   ------> USB_OTG_HS_ULPI_D3
	PB11   ------> USB_OTG_HS_ULPI_D4
*/
void MX_GPIO_Init(void)
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
//	GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_5|GPIO_PIN_6;
//	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
//	GPIO_InitStruct.Pull = GPIO_NOPULL;
//	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
//	GPIO_InitStruct.Alternate = GPIO_AF0_TRACE;
//	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

	/*Configure GPIO pins : PB5 PB12 PB13 PB1
						   PB0 PB10 PB11 */
	GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_1
						  |GPIO_PIN_0|GPIO_PIN_10|GPIO_PIN_11;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pin : PG15 */
	GPIO_InitStruct.Pin = GPIO_PIN_15;
	GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

	/*Configure GPIO pins : PG12 PG8 PG6 */
	GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_8|GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

	/*Configure GPIO pins : PA12 PA11 PA10 */
	GPIO_InitStruct.Pin = GPIO_PIN_12|GPIO_PIN_11|GPIO_PIN_10;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : PI3 PI0 */
	GPIO_InitStruct.Pin = GPIO_PIN_3|GPIO_PIN_0;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
	HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

	/*Configure GPIO pin : PI2 */
	GPIO_InitStruct.Pin = GPIO_PIN_2;
	GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

	/*Configure GPIO pin : PI9 */
	GPIO_InitStruct.Pin = GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

	/*Configure GPIO pins : PH15 PH5 */
	GPIO_InitStruct.Pin = GPIO_PIN_15|GPIO_PIN_5;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

	/*Configure GPIO pin : PI11 */
	GPIO_InitStruct.Pin = GPIO_PIN_11;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
	HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);

	/*Configure GPIO pin : PH13 */
	GPIO_InitStruct.Pin = GPIO_PIN_13;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

	/*Configure GPIO pin : PA9 */
	GPIO_InitStruct.Pin = GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : PA8 */
	GPIO_InitStruct.Pin = GPIO_PIN_8;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : PC7 */
	GPIO_InitStruct.Pin = GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pin : PH4 */
	GPIO_InitStruct.Pin = GPIO_PIN_4;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
	HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

	/*Configure GPIO pin : PG7 */
	GPIO_InitStruct.Pin = GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
	HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

	/*Configure GPIO pin : PF7 */
	GPIO_InitStruct.Pin = GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_LOW;
	HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

	/*Configure GPIO pin : PF6 */
	GPIO_InitStruct.Pin = GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

	/*Configure GPIO pin : PC0 */
	GPIO_InitStruct.Pin = GPIO_PIN_0;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pin : PB2 */
	GPIO_InitStruct.Pin = GPIO_PIN_2;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pins : PA5 PA3 */
	GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_3;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF10_OTG_HS;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : PF11 */
	GPIO_InitStruct.Pin = GPIO_PIN_11;
	GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

	/*Configure GPIO pin : PB14 */
	GPIO_InitStruct.Pin = GPIO_PIN_14;
	GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}
/*-----------------------------------------------------------*/

/* The following hook-function will be called from "STM32F4xx\ff_sddisk.c"
in case the GPIO contact of the card-detect has changed. */

void vApplicationCardDetectChangeHookFromISR( BaseType_t *pxHigherPriorityTaskWoken );

void vApplicationCardDetectChangeHookFromISR( BaseType_t *pxHigherPriorityTaskWoken )
{
	/* This routine will be called on every change of the Card-detect
	GPIO pin.  The TCP server is probably waiting for in event in a
	select() statement.  Wake it up.*/
	#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
	if( pxTCPServer != NULL )
	{
		FreeRTOS_TCPServerSignalFromISR( pxTCPServer, pxHigherPriorityTaskWoken );
	}
	#endif
}
/*-----------------------------------------------------------*/

/* FSMC initialization function */
static void MX_FSMC_Init(void)
{
	FSMC_NORSRAM_TimingTypeDef Timing;

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

	HAL_NOR_Init(&hnor1, &Timing, NULL);

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

	HAL_SRAM_Init(&hsram2, &Timing, NULL);

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

	HAL_SRAM_Init(&hsram3, &Timing, NULL);
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

			//bosman_open_listen_socket();

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

			/* Let the server work task 'prvServerWorkTask' now it can now create the servers. */
			xTaskNotifyGive( xServerWorkTaskHandle );

			xTasksAlreadyCreated = pdTRUE;
		}

		#if( ipconfigMULTI_INTERFACE == 0 )
		{
		uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
		char cBuffer[ 16 ];
			/* Print out the network configuration, which may have come from a DHCP
			server. */
			FreeRTOS_GetAddressConfiguration( &ulIPAddress, &ulNetMask, &ulGatewayAddress, &ulDNSServerAddress );
			FreeRTOS_inet_ntoa( ulIPAddress, cBuffer );
			FreeRTOS_printf( ( "IP Address: %s\n", cBuffer ) );

			FreeRTOS_inet_ntoa( ulNetMask, cBuffer );
			FreeRTOS_printf( ( "Subnet Mask: %s\n", cBuffer ) );

			FreeRTOS_inet_ntoa( ulGatewayAddress, cBuffer );
			FreeRTOS_printf( ( "Gateway Address: %s\n", cBuffer ) );

			FreeRTOS_inet_ntoa( ulDNSServerAddress, cBuffer );
			FreeRTOS_printf( ( "DNS Server Address: %s\n", cBuffer ) );
		}
		#else
		{
			/* Print out the network configuration, which may have come from a DHCP
			server. */
		#if( ipconfigUSE_IPv6 != 0 )
			if( pxEndPoint->bits.bIPv6 )
			{
			IPv6_Address_t xPrefix;
//			pxEndPoint->ipv6.xIPAddress;			/* The actual IPv4 address. Will be 0 as long as end-point is still down. */
//			pxEndPoint->ipv6.uxPrefixLength;
//			pxEndPoint->ipv6.xDefaultIPAddress;
//			pxEndPoint->ipv6.xGatewayAddress;
//			pxEndPoint->ipv6.xDNSServerAddresses[ 2 ];	/* Not yet in use. */

				/* Extract the prefix from the IP-address */
				FreeRTOS_CreateIPv6Address( &( xPrefix ), &( pxEndPoint->ipv6.xIPAddress ), pxEndPoint->ipv6.uxPrefixLength, pdFALSE );

				FreeRTOS_printf( ( "IP-address : %pip (default %pip)\n",
					pxEndPoint->ipv6.xIPAddress.ucBytes,
					pxEndPoint->ipv6.xDefaultIPAddress.ucBytes ) );
				FreeRTOS_printf( ( "Prefix     : %pip/%d\n", xPrefix.ucBytes, ( int ) pxEndPoint->ipv6.uxPrefixLength ) );
				FreeRTOS_printf( ( "GW         : %pip\n", pxEndPoint->ipv6.xGatewayAddress.ucBytes ) );
				FreeRTOS_printf( ( "DNS        : %pip\n", pxEndPoint->ipv6.xDNSServerAddresses[ 0 ].ucBytes ) );
			}
			else
		#endif	/* ( ipconfigUSE_IPv6 != 0 ) */
			{
				FreeRTOS_printf( ( "IP-address : %lxip (default %lxip)\n",
					FreeRTOS_ntohl( pxEndPoint->ipv4.ulIPAddress ), FreeRTOS_ntohl( pxEndPoint->ipv4.ulDefaultIPAddress ) ) );
				FreeRTOS_printf( ( "Net mask   : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4.ulNetMask ) ) );
				FreeRTOS_printf( ( "GW         : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4.ulGatewayAddress ) ) );
				FreeRTOS_printf( ( "DNS        : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4.ulDNSServerAddresses[ 0 ] ) ) );
				FreeRTOS_printf( ( "Broadcast  : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4.ulBroadcastAddress ) ) );
			}

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
		do {
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

#if( ipconfigMULTI_INTERFACE != 0 )
	extern BaseType_t xApplicationDNSQueryHook( NetworkEndPoint_t *pxEndPoint, const char *pcName );
#else
	extern BaseType_t xApplicationDNSQueryHook( const char *pcName );
#endif

void vApplicationPingReplyHook( ePingReplyStatus_t eStatus, uint16_t usIdentifier )
{
	if (xPing4Count >= 0 && xPing4Count < PING_COUNT_MAX) {
		xPing4Count++;
		if (pingLogging || xPing4Count == PING_COUNT_MAX)
			FreeRTOS_printf( ( "Received reply %d: status %d ID %04x\n", ( int ) xPing4Count, ( int ) eStatus, usIdentifier ) );
	}
#if( ipconfigUSE_IPv6 != 0 )
	if (xPing6Count >= 0 && xPing6Count < PING_COUNT_MAX) {
		xPing6Count++;
		//if (xPing6Count == PING_COUNT_MAX || xPing6Count == 1)
			FreeRTOS_printf( ( "Received reply %d: status %d ID %04x\n", ( int ) xPing6Count, ( int ) eStatus, usIdentifier ) );
	}
#endif
	xPingReady = pdTRUE;

	if( xServerSemaphore != NULL )
	{
		xSemaphoreGive( xServerSemaphore );
	}
}
/*-----------------------------------------------------------*/

/* These are volatile to try and prevent the compiler/linker optimising them
away as the variables never actually get used.  If the debugger won't show the
values of the variables, make them global my moving their declaration outside
of this function. */
volatile uint32_t r0;
volatile uint32_t r1;
volatile uint32_t r2;
volatile uint32_t r3;
volatile uint32_t r12;
volatile uint32_t lr; /* Link register. */
volatile uint32_t pc; /* Program counter. */
volatile uint32_t psr;/* Program status register. */

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

void prvGetRegistersFromStack( uint32_t *pulFaultStackAddress )
{
	pxRegisterStack = ( struct xREGISTER_STACK *) ( pulFaultStackAddress - ARRAY_SIZE( pxRegisterStack->spare0 ) );
	r0 = pulFaultStackAddress[ 0 ];
	r1 = pulFaultStackAddress[ 1 ];
	r2 = pulFaultStackAddress[ 2 ];
	r3 = pulFaultStackAddress[ 3 ];

	r12 = pulFaultStackAddress[ 4 ];
	lr = pulFaultStackAddress[ 5 ];
	pc = pulFaultStackAddress[ 6 ];
	psr = pulFaultStackAddress[ 7 ];

	/* When the following line is hit, the variables contain the register values. */
	for( ;; );
}
/*-----------------------------------------------------------*/

//void HardFault_Handler( void ) __attribute__( ( naked ) );
void HardFault_Handler(void)
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

void *malloc(size_t size)
{
	return pvPortMalloc(size);
}
/*-----------------------------------------------------------*/

void free(void *ptr)
{
	vPortFree(ptr);
}
/*-----------------------------------------------------------*/

extern void vOutputChar( const char cChar, const TickType_t xTicksToWait  );

void vOutputChar( const char cChar, const TickType_t xTicksToWait  )
{
}
/*-----------------------------------------------------------*/

BaseType_t xApplicationMemoryPermissions( uint32_t aAddress )
{
	return 3;
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


#if( ipconfigMULTI_INTERFACE != 0 )
#warning Multi
	BaseType_t xApplicationDNSQueryHook( NetworkEndPoint_t *pxEndPoint, const char *pcName )
#else
#warning Single
	BaseType_t xApplicationDNSQueryHook( const char *pcName )
#endif
{
BaseType_t xReturn;

	/* Determine if a name lookup is for this node.  Two names are given
	to this node: that returned by pcApplicationHostnameHook() and that set
	by mainDEVICE_NICK_NAME. */
	if( strcasecmp( pcName, pcApplicationHostnameHook() ) == 0 )
	{
		xReturn = pdPASS;
	}
	else if( strcasecmp( pcName, mainDEVICE_NICK_NAME ) == 0 )
	{
		xReturn = pdPASS;
	}
	else if( strcasecmp( pcName, "hs2011" ) == 0 )
	{
		xReturn = pdPASS;
	}
	else if( memcmp( pcName, "aap", 3 ) == 0 )
	{
#if( ipconfigUSE_IPv6 != 0 )
		xReturn = pxEndPoint->bits.bIPv6 != 0;
#else
		xReturn = pdPASS;
#endif
	}
	else if( memcmp( pcName, "kat", 3 ) == 0 )
	{
#if( ipconfigUSE_IPv6 != 0 )
		xReturn = pxEndPoint->bits.bIPv6 == 0;
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
#if( ipconfigMULTI_INTERFACE != 0 ) && ( ipconfigUSE_IPv6 != 0 )
	FreeRTOS_printf( ( "DNS query '%s' = %d IPv%c\n", pcName, (int)xReturn, pxEndPoint->bits.bIPv6 ? '6' : '4' ) );
#else
	FreeRTOS_printf( ( "DNS query '%s' = %d IPv4\n", pcName, (int)xReturn ) );
#endif

	return xReturn;
}
/*-----------------------------------------------------------*/

//static uint32_t ulListTime;
static uint64_t ullHiresTime;
uint32_t ulGetRunTimeCounterValue( void )
{
	return ( uint32_t ) ( ullGetHighResolutionTime() - ullHiresTime );
}

extern BaseType_t xTaskClearCounters;
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
	pxTaskStatusArray = pvPortMalloc( uxArraySize * sizeof( TaskStatus_t ) );

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

#define MEMCPY_SIZE   2048

/*
2015-02-25 14:26:56.532 1:18:21    3.428.000 [IP-task   ] Copy internal 12679 external 8347

12679 * 2048 * 10 = 259,665,920
 8347 * 2048 * 10 = 170,946,560
 259,665,920 / 168000000 = 1.5456304761904761904761904761905
*/


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
		void *_sbrk( ptrdiff_t __incr)
		{
			unknown_calls++;
	
			return 0;
		}
		/*-----------------------------------------------------------*/
	#endif
#endif
/*-----------------------------------------------------------*/

#define SRAM1_BASE            ((uint32_t)0x20000000) /*!< SRAM1(112 KB) base address in the alias region                             */
#define SRAM2_BASE            ((uint32_t)0x2001C000) /*!< SRAM2(16 KB) base address in the alias region                              */
#define SRAM3_BASE            ((uint32_t)0x20020000) /*!< SRAM3(64 KB) base address in the alias region                              */

typedef struct xMemoryArea {
	uint32_t ulStart;
	uint32_t ulSize;
} MemoryArea_t;

#define	sizeKB		1024ul

MemoryArea_t xMemoryAreas[] = {
	{ ulStart : SRAM1_BASE, ulSize : 112 * sizeKB },
	{ ulStart : SRAM2_BASE, ulSize : 16 * sizeKB },
	{ ulStart : SRAM3_BASE, ulSize : 64 * sizeKB },
};

static volatile int errors[ 3 ] = { 0, 0, 0 };

static void memory_tests()
{
	int index;
	char buffer[ 16 ];
	for( index = 0; index < ARRAY_SIZE(xMemoryAreas); index++ )
	{
	int offset;
	unsigned char *source = ( unsigned char * ) ( xMemoryAreas[ index ].ulStart + 8192 );

		for( offset = 0; offset < sizeof( buffer ); offset++ )
		{
			buffer[ offset ] = source[ offset ];
		}
		for( offset = 0; offset < sizeof( buffer ); offset++ )
		{
			source[ offset ] = buffer[ offset ] + 1;
		}
		for( offset = 0; offset < sizeof( buffer ); offset++ )
		{
			if( source[ offset ] != buffer[ offset ] + 1 )
			{
				errors[ index ]++;
			}
		}
		/* Restore original contents. */
		for( offset = 0; offset < sizeof( buffer ); offset++ )
		{
			source[ offset ] = buffer[ offset ];
		}
	}
}

/* Add legacy definition */
#define  GPIO_Speed_2MHz    GPIO_SPEED_LOW
#define  GPIO_Speed_25MHz   GPIO_SPEED_MEDIUM
#define  GPIO_Speed_50MHz   GPIO_SPEED_FAST
#define  GPIO_Speed_100MHz  GPIO_SPEED_HIGH

/*_HT_ Please consider keeping the function below called HAL_ETH_MspInit().
It will be called from "stm32f4xx_hal_eth.c".
It is defined there as weak only.
My board didn't work without the settings made below.
*/
/* This is a callback function. */

void HAL_ETH_MspInit( ETH_HandleTypeDef* heth )
{
	GPIO_InitTypeDef GPIO_InitStruct;

	if( heth->Instance == ETH )
	{
		__ETH_CLK_ENABLE();			/* defined as __HAL_RCC_ETH_CLK_ENABLE. */
		__ETHMACRX_CLK_ENABLE();	/* defined as __HAL_RCC_ETHMACRX_CLK_ENABLE. */
		__ETHMACTX_CLK_ENABLE();	/* defined as __HAL_RCC_ETHMACTX_CLK_ENABLE. */

	/* Enable GPIOs clocks */
//	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB |
//						   RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOI |
//						   RCC_AHB1Periph_GPIOG | RCC_AHB1Periph_GPIOH |
//						   RCC_AHB1Periph_GPIOD | RCC_AHB1Periph_GPIOE |
//						   RCC_AHB1Periph_GPIOF, ENABLE);

//	/* Enable SYSCFG clock */
//	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	/* Configure MCO (PA8) */
	GPIO_InitStruct.Pin = GPIO_PIN_8;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF11_ETH;//GPIO_AF0_MCO;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* MII/RMII Media interface selection --------------------------------------*/
	#if( ipconfigUSE_RMII != 0 )
		/* Mode RMII with STM324xx-EVAL */
		//SYSCFG_ETH_MediaInterfaceConfig(SYSCFG_ETH_MediaInterface_RMII);
	#else
		/* Mode MII with STM324xx-EVAL  */
		#ifdef PHY_CLOCK_MCO
			/* Output HSE clock (25MHz) on MCO pin (PA8) to clock the PHY */
			RCC_MCO1Config(RCC_MCO1Source_HSE, RCC_MCO1Div_1);
		#endif /* PHY_CLOCK_MCO */
		//SYSCFG_ETH_MediaInterfaceConfig(SYSCFG_ETH_MediaInterface_MII);
	#endif

	/* Ethernet pins configuration ************************************************/
	/*
										STM3240G-EVAL
										DP83848CVV	LAN8720
	ETH_MDIO -------------------------> PA2		==	PA2
	ETH_MDC --------------------------> PC1		==	PC1
	ETH_PPS_OUT ----------------------> PB5			x
	ETH_MII_CRS ----------------------> PH2			x
	ETH_MII_COL ----------------------> PH3			x
	ETH_MII_RX_ER --------------------> PI10		x
	ETH_MII_RXD2 ---------------------> PH6			x
	ETH_MII_RXD3 ---------------------> PH7			x
	ETH_MII_TX_CLK -------------------> PC3			x
	ETH_MII_TXD2 ---------------------> PC2			x
	ETH_MII_TXD3 ---------------------> PB8			x
	ETH_MII_RX_CLK/ETH_RMII_REF_CLK---> PA1		==	PA1
	ETH_MII_RX_DV/ETH_RMII_CRS_DV ----> PA7		==	PA7
	ETH_MII_RXD0/ETH_RMII_RXD0 -------> PC4		==	PC4
	ETH_MII_RXD1/ETH_RMII_RXD1 -------> PC5		==	PC5
	ETH_MII_TX_EN/ETH_RMII_TX_EN -----> PG11	<>	PB11
	ETH_MII_TXD0/ETH_RMII_TXD0 -------> PG13	<>	PB12
	ETH_MII_TXD1/ETH_RMII_TXD1 -------> PG14	<>	PB13
	*/

	/* Configure PA1, PA2 and PA7 */
	GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	#if( USE_STM324xG_EVAL != 0 )
	{
		/* Configure PB5 and PB8 */
		GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_8;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

		/* Configure PC1, PC2, PC3, PC4 and PC5 */
		GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
	}
	#else
	{
		/* Configure PC1, PC4 and PC5 */
		GPIO_InitStruct.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
	}
	#endif /* USE_STM324xG_EVAL */


	/* Configure PC1, PC2, PC3, PC4 and PC5 */
	#if( USE_STM324xG_EVAL != 0 )
	{
		/* Configure PG11, PG13 and PG14 */
		GPIO_InitStruct.Pin =  GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
		HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

		/* Configure PH2, PH3, PH6, PH7 */
		GPIO_InitStruct.Pin = GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_6 | GPIO_PIN_7;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
		HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

		/* Configure PI10 */
		GPIO_InitStruct.Pin = GPIO_PIN_10;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
		HAL_GPIO_Init(GPIOI, &GPIO_InitStruct);
	}
	#else
	{
		/* Configure PB11, PB12 and PB13 */
		GPIO_InitStruct.Pin =  GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
		HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
	}
	#endif /* USE_STM324xG_EVAL */
	HAL_NVIC_SetPriority( ETH_IRQn, ipconfigMAC_INTERRUPT_PRIORITY, 0 );
	HAL_NVIC_EnableIRQ( ETH_IRQn );
	} // if( heth->Instance == ETH )
}
/*-----------------------------------------------------------*/

uint32_t ulApplicationGetNextSequenceNumber(
    uint32_t ulSourceAddress,
    uint16_t usSourcePort,
    uint32_t ulDestinationAddress,
    uint16_t usDestinationPort )
{
	return ipconfigRAND32();
}

static uint8_t msg_1[] = {
	0x54, 0x55, 0x58, 0x10, 0x00, 0x37, 0x00, 0xe0,   0x4c, 0x36, 0x01, 0x3b, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x34, 0x49, 0xa7, 0x40, 0x00, 0x80, 0x06,   0x00, 0x00, 0xc0, 0xa8, 0x01, 0x0a, 0xc0, 0xa8,
	0x01, 0x05, 0xd6, 0xc5, 0x27, 0x10, 0x0c, 0x5b,   0xa2, 0x4b, 0x00, 0x00, 0x00, 0x00, 0x80, 0x02,
	0xfa, 0xf0, 0x83, 0x86, 0x00, 0x00, 0x02, 0x04,   0x05, 0xb4, 0x01, 0x03, 0x03, 0x08, 0x01, 0x01,
	0x04, 0x02
};

static uint8_t msg_2[] = {
	0x00, 0xe0, 0x4c, 0x36, 0x01, 0x3b, 0x54, 0x55,   0x58, 0x10, 0x00, 0x37, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x2c, 0x00, 0x04, 0x00, 0x00, 0x80, 0x06,   0x2d, 0xbd, 0xc0, 0xa8, 0x01, 0x05, 0xc0, 0xa8,
	0x01, 0x0a, 0x27, 0x10, 0xd6, 0xc5, 0x29, 0xac,   0xd1, 0x2d, 0x0c, 0x5b, 0xa2, 0x4c, 0x60, 0x12,
	0x05, 0x78, 0x44, 0x43, 0x00, 0x00, 0x02, 0x04,   0x05, 0x78, 0x00, 0x00
};

static uint8_t msg_3[] = {
	0x54, 0x55, 0x58, 0x10, 0x00, 0x37, 0x00, 0xe0,   0x4c, 0x36, 0x01, 0x3b, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x34, 0x49, 0xa8, 0x40, 0x00, 0x80, 0x06,   0x00, 0x00, 0xc0, 0xa8, 0x01, 0x0a, 0xc0, 0xa8,
	0x01, 0x05, 0xd6, 0xc5, 0x27, 0x10, 0x0c, 0x5b,   0xa2, 0x4b, 0x00, 0x00, 0x00, 0x00, 0x80, 0x02,
	0xfa, 0xf0, 0x83, 0x86, 0x00, 0x00, 0x02, 0x04,   0x05, 0xb4, 0x01, 0x03, 0x03, 0x08, 0x01, 0x01,
	0x04, 0x02
};

static uint8_t msg_4[] = {
	0x00, 0xe0, 0x4c, 0x36, 0x01, 0x3b, 0x54, 0x55,   0x58, 0x10, 0x00, 0x37, 0x08, 0x00, 0x45, 0x00,
	0x00, 0x2c, 0x00, 0x05, 0x00, 0x00, 0x80, 0x06,   0x2d, 0xbc, 0xc0, 0xa8, 0x01, 0x05, 0xc0, 0xa8,
	0x01, 0x0a, 0x27, 0x10, 0xd6, 0xc5, 0x29, 0xac,   0xd1, 0x2d, 0x0c, 0x5b, 0xa2, 0x4c, 0x60, 0x12,
	0x05, 0x78, 0x44, 0x43, 0x00, 0x00, 0x02, 0x04,   0x05, 0x78, 0x00, 0x00
};

struct SPacket {
	uint8_t *pucData;
	BaseType_t xCount;
};

const struct SPacket packets[] = {
	{ msg_1, sizeof msg_1 },
	{ msg_2, sizeof msg_2 },
	{ msg_3, sizeof msg_3 },
	{ msg_4, sizeof msg_4 },
};

uint16_t usGenerateProtocolChecksum( const uint8_t * const pucEthernetBuffer, size_t uxBufferLength, BaseType_t xOutgoingPacket );

#if( ipconfigMULTI_INTERFACE == 0 )
	typedef union xPROT_HEADERS
	{
		ICMPHeader_t xICMPHeader;
		UDPHeader_t xUDPHeader;
		TCPHeader_t xTCPHeader;
	} ProtocolHeaders_t;
	#define ipPOINTER_CAST( TYPE, OBJECT )	( ( TYPE ) ( OBJECT ) )
#endif

static void checksum_test()
{
	int i;
	for (i = 0; i < ARRAY_SIZE(packets); i++) {
		uint16_t usChecksum_IP, usChecksum_TCP, usChecksum_IP_2, usChecksum_TCP_2;
		IPHeader_t *pxIPHeader_IPv4;
		ProtocolHeaders_t *pxProtocolHeaders;
		int count = packets[i].xCount;
		BaseType_t xIPHeaderLength;
		const uint8_t *pucEthernetBuffer = packets[i].pucData;
	
		pxIPHeader_IPv4 = ipPOINTER_CAST( IPHeader_t *, &( pucEthernetBuffer[ ipSIZE_OF_ETH_HEADER ] ) );
		xIPHeaderLength = sizeof( uint32_t ) * ( pxIPHeader_IPv4->ucVersionHeaderLength & 0x0F );
		pxProtocolHeaders = ( ProtocolHeaders_t * ) ( pucEthernetBuffer + ipSIZE_OF_ETH_HEADER + xIPHeaderLength );

		usChecksum_IP = pxIPHeader_IPv4->usHeaderChecksum;
		usChecksum_TCP = pxProtocolHeaders->xTCPHeader.usChecksum;

		pxIPHeader_IPv4->usHeaderChecksum = 0x00u;
		pxIPHeader_IPv4->usHeaderChecksum = usGenerateChecksum( 0u, ( uint8_t * ) &( pxIPHeader_IPv4->ucVersionHeaderLength ), xIPHeaderLength );
		pxIPHeader_IPv4->usHeaderChecksum = ~FreeRTOS_htons( pxIPHeader_IPv4->usHeaderChecksum );

		usChecksum_IP_2 = pxIPHeader_IPv4->usHeaderChecksum;

//		pxIPHeader_IPv4->usHeaderChecksum = 0;
//		pxProtocolHeaders->xTCPHeader.usChecksum = 0;

		usGenerateProtocolChecksum (pucEthernetBuffer, count, pdTRUE );

		usChecksum_TCP_2 = pxProtocolHeaders->xTCPHeader.usChecksum;

		FreeRTOS_printf( ( "Checksum %d  %d + %d + %d = %d ( %04x -> %04x ) %04x -> %04x\n",
			i,

			( int ) ipSIZE_OF_ETH_HEADER,
			( int ) xIPHeaderLength,
			( int ) ( FreeRTOS_ntohs( pxIPHeader_IPv4->usLength ) - xIPHeaderLength ),
			( int ) count,

			FreeRTOS_ntohs( usChecksum_IP ),
			FreeRTOS_ntohs( usChecksum_IP_2 ),
			FreeRTOS_ntohs( usChecksum_TCP ),
			FreeRTOS_ntohs( usChecksum_TCP_2 ) ) );
	}
}

#if( ipconfigUSE_CALLBACKS != 0 )
static BaseType_t xOnUdpReceive( Socket_t xSocket, void * pvData, size_t xLength,
	const struct freertos_sockaddr *pxFrom, const struct freertos_sockaddr *pxDest )
{
#if( ipconfigUSE_IPv6 != 0 )
//const struct freertos_sockaddr6 *pxFrom6 = ( const struct freertos_sockaddr6 * ) pxFrom;
//const struct freertos_sockaddr6 *pxDest6 = ( const struct freertos_sockaddr6 * ) pxDest;
#endif
/*
	#if( ipconfigUSE_IPv6 != 0 )
	if( pxFrom6->sin_family == FREERTOS_AF_INET6 )
	{
		FreeRTOS_printf( ( "xOnUdpReceive_6: %d bytes\n",  ( int ) xLength ) );
		FreeRTOS_printf( ( "xOnUdpReceive_6: from %pip\n", pxFrom6->sin_addrv6.ucBytes ) );
		FreeRTOS_printf( ( "xOnUdpReceive_6: to   %pip\n", pxDest6->sin_addrv6.ucBytes ) );
	}
	else
	#endif
	{
		FreeRTOS_printf( ( "xOnUdpReceive_4: %d bytes\n", ( int ) xLength ) );
		FreeRTOS_printf( ( "xOnUdpReceive_4: from %xip\n", FreeRTOS_ntohl( pxFrom->sin_addr ) ) );
		FreeRTOS_printf( ( "xOnUdpReceive_4: to   %xip\n", FreeRTOS_ntohl( pxDest->sin_addr ) ) );
		
	}
*/
	/* Returning 0 means: not yet consumed. */
	return 0;
}
#endif
