/*
	FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
	All rights reserved

	VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

	This file is part of the FreeRTOS distribution.

	FreeRTOS is free software; you can redistribute it and/or modify it under
	the terms of the GNU General Public License (version 2) as published by the
	Free Software Foundation >>!AND MODIFIED BY!<< the FreeRTOS exception.

	***************************************************************************
	>>!   NOTE: The modification to the GPL is included to allow you to     !<<
	>>!   distribute a combined work that includes FreeRTOS without being   !<<
	>>!   obliged to provide the source code for proprietary components     !<<
	>>!   outside of the FreeRTOS kernel.                                   !<<
	***************************************************************************

	FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
	WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE.  Full license text is available on the following
	link: http://www.freertos.org/a00114.html

	***************************************************************************
	 *                                                                       *
	 *    FreeRTOS provides completely free yet professionally developed,    *
	 *    robust, strictly quality controlled, supported, and cross          *
	 *    platform software that is more than just the market leader, it     *
	 *    is the industry's de facto standard.                               *
	 *                                                                       *
	 *    Help yourself get started quickly while simultaneously helping     *
	 *    to support the FreeRTOS project by purchasing a FreeRTOS           *
	 *    tutorial book, reference manual, or both:                          *
	 *    http://www.FreeRTOS.org/Documentation                              *
	 *                                                                       *
	***************************************************************************

	http://www.FreeRTOS.org/FAQHelp.html - Having a problem?  Start by reading
	the FAQ page "My application does not run, what could be wrong?".  Have you
	defined configASSERT()?

	http://www.FreeRTOS.org/support - In return for receiving this top quality
	embedded software for free we request you assist our global community by
	participating in the support forum.

	http://www.FreeRTOS.org/training - Investing in training allows your team to
	be as productive as possible as early as possible.  Now you can receive
	FreeRTOS training directly from Richard Barry, CEO of Real Time Engineers
	Ltd, and the world's leading authority on the world's leading RTOS.

	http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
	including FreeRTOS+Trace - an indispensable productivity tool, a DOS
	compatible FAT file system, and our tiny thread aware UDP/IP stack.

	http://www.FreeRTOS.org/labs - Where new FreeRTOS products go to incubate.
	Come and try FreeRTOS+TCP, our new open source TCP/IP stack for FreeRTOS.

	http://www.OpenRTOS.com - Real Time Engineers ltd. license FreeRTOS to High
	Integrity Systems ltd. to sell under the OpenRTOS brand.  Low cost OpenRTOS
	licenses offer ticketed support, indemnification and commercial middleware.

	http://www.SafeRTOS.com - High Integrity Systems also provide a safety
	engineered and independently SIL3 certified version for use in safety and
	mission critical applications that require provable dependability.

	1 tab == 4 spaces!
*/

/*
 * Instructions for using this project are provided on:
 * http://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/examples_FreeRTOS_simulator.html
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
#if( ipconfigMULTI_INTERFACE != 0 )
	#include "FreeRTOS_Routing.h"
#endif


/* FreeRTOS+FAT includes. */
#include "ff_headers.h"
#include "ff_stdio.h"
#include "ff_ramdisk.h"
#include "ff_sddisk.h"

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

/* ST includes. */
#include "stm32f7xx_hal.h"
#include "stm32fxx_hal_eth.h"

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
#define mainHAS_FAT_TEST				1

/* Set the following constants to 1 to include the relevant server, or 0 to
exclude the relevant server. */
#define mainCREATE_FTP_SERVER			1
#define mainCREATE_HTTP_SERVER 			1



/* Define names that will be used for DNS, LLMNR and NBNS searches. */
#define mainHOST_NAME					"stm"
#define mainDEVICE_NICK_NAME			"stm32f4"


#ifdef HAL_WWDG_MODULE_ENABLED
	#warning Why enable HAL_WWDG_MODULE_ENABLED
#endif

#ifndef GPIO_SPEED_LOW
	#define  GPIO_SPEED_LOW         ((uint32_t)0x00000000)  /*!< Low speed     */
	#define  GPIO_SPEED_MEDIUM      ((uint32_t)0x00000001)  /*!< Medium speed  */
	#define  GPIO_SPEED_FAST        ((uint32_t)0x00000002)  /*!< Fast speed    */
	#define  GPIO_SPEED_HIGH        ((uint32_t)0x00000003)  /*!< High speed    */
#endif

RNG_HandleTypeDef hrng;

int verboseLevel;

FF_Disk_t *pxSDDisk;
#if( mainHAS_RAMDISK != 0 )
	static FF_Disk_t *pxRAMDisk;
#endif

extern int zero_divisor;
static TCPServer_t *pxTCPServer = NULL;

#ifndef ARRAY_SIZE
#	define	ARRAY_SIZE( x )	( int )( sizeof( x ) / sizeof( x )[ 0 ] )
#endif

/*
 * Just seeds the simple pseudo random number generator.
 */
static void prvSRand( UBaseType_t ulSeed );

static SemaphoreHandle_t xServerSemaphore;

/* Private function prototypes -----------------------------------------------*/

/*
 * Creates a RAM disk, then creates files on the RAM disk.  The files can then
 * be viewed via the FTP server and the command line interface.
 */
static void prvCreateDiskAndExampleFiles( void );

extern void vStdioWithCWDTest( const char *pcMountPath );
extern void vCreateAndVerifyExampleFiles( const char *pcMountPath );

/*
 * The task that runs the FTP and HTTP servers.
 */

static void prvServerWorkTask( void *pvParameters );

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

void realloc_test();

void tcpServerStart();
void tcpServerWork();


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
	static const uint8_t ucIPAddress2[ 4 ] = { 127, 0, 0, 1 };
	static const uint8_t ucNetMask2[ 4 ] = { 255, 0, 0, 0 };
	static const uint8_t ucGatewayAddress2[ 4 ] = { 0, 0, 0, 0};
	static const uint8_t ucDNSServerAddress2[ 4 ] = { 0, 0, 0, 0 };

	static const uint8_t ucMACAddress[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };
	static const uint8_t ucMACAddress2[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 + 1 };
#endif /* ipconfigMULTI_INTERFACE */

/* Use by the pseudo random number generator. */
static UBaseType_t ulNextRand;

/* Handle of the task that runs the FTP and HTTP servers. */
static TaskHandle_t xServerWorkTaskHandle = NULL;
static TaskHandle_t xTCPTaskHandle = NULL;

#if( ipconfigMULTI_INTERFACE != 0 )
	NetworkInterface_t *pxSTMF40_FillInterfaceDescriptor( BaseType_t xEMACIndex, NetworkInterface_t *pxInterface );
	NetworkInterface_t *pxLoopback_FillInterfaceDescriptor( BaseType_t xEMACIndex, NetworkInterface_t *pxInterface );
#endif /* ipconfigMULTI_INTERFACE */
/*-----------------------------------------------------------*/

#if( ipconfigMULTI_INTERFACE != 0 )
NetworkInterface_t *pxSTMF40_IPInit(
	BaseType_t xEMACIndex, NetworkInterface_t *pxInterface );
#endif

/* Prototypes for the standard FreeRTOS callback/hook functions implemented
within this file. */
void vApplicationMallocFailedHook( void );
void vApplicationIdleHook( void );
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName );
void vApplicationTickHook( void );

/*-----------------------------------------------------------*/

int main( void )
{
#if( ipconfigMULTI_INTERFACE != 0 )
	static NetworkInterface_t xInterfaces[ 2 ];
	static NetworkEndPoint_t xEndPoints[ 2 ];
#endif /* ipconfigMULTI_INTERFACE */

uint32_t ulSeed;

	SCB_EnableDCache();
	/* Disable DCache for STM32F7 family */
//	SCB_DisableDCache();

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* Configure the hardware ready to run the demo. */
	prvSetupHardware();

	heapInit( );

	/* Timer2 initialization function.
	ullGetHighResolutionTime() will be used to get the running time in uS. */
	vStartHighResolutionTimer();

//	hrng.Instance = RNG;
//	HAL_RNG_Init( &hrng );
//	HAL_RNG_GenerateRandomNumber( &hrng, &ulSeed );
//	prvSRand( ulSeed );

#if USE_LOG_EVENT
	iEventLogInit();
#endif

//	HAL_RNG_Init( &hrng );
//
//	hrng.Instance = RNG;
//	if(HAL_RNG_GenerateRandomNumber( &hrng, &ulSeed ) == HAL_OK )
//	{
//		prvSRand( ulSeed );
//	}

	/* Initialise the network interface.

	***NOTE*** Tasks that use the network are created in the network event hook
	when the network is connected and ready for use (see the definition of
	vApplicationIPNetworkEventHook() below).  The address values passed in here
	are used if ipconfigUSE_DHCP is set to 0, or if ipconfigUSE_DHCP is set to 1
	but a DHCP server cannot be	contacted. */

#if( ipconfigMULTI_INTERFACE == 0 )
	FreeRTOS_IPInit( ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress );
#else
	pxSTMF40_FillInterfaceDescriptor( 0, &( xInterfaces[ 0 ] ) );
	pxLoopback_FillInterfaceDescriptor( 0, &( xInterfaces[ 1 ] ) );

	FreeRTOS_FillEndPoint( &( xEndPoints[ 0 ] ), ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress );
	FreeRTOS_FillEndPoint( &( xEndPoints[ 1 ] ), ucIPAddress2, ucNetMask2, ucGatewayAddress2, ucDNSServerAddress2, ucMACAddress2 );

	#if( ipconfigUSE_IPv6 != 0 )
	{
	BaseType_t xIndex;
		//                                80fe       0       0       0    118d    9bcd    668b    784a
		// 0000   ff 02 00 00 00 00 00 00 00 00 00 01 ff 66 4a 78  .............fJx
//		unsigned short pusAddress[] = { 0xfe80, 0x0000, 0x0000, 0x0000, 0x8d11, 0xcd9b, 0x8b66, 0x4a78 };
		// fe80::8d11:cd9b:8b66:4a80
		unsigned short pusAddress[] = { 0xfe80, 0x0000, 0x0000, 0x0000, 0x8d11, 0xcd9b, 0x8b66, 0x4a80 };
		for( xIndex = 0; xIndex < ARRAY_SIZE( pusAddress ); xIndex++ )
		{
			xEndPoints[ 0 ].ulIPAddress_IPv6.usShorts[ xIndex ] = FreeRTOS_htons( pusAddress[ xIndex ] );
		}
		xEndPoints[ 0 ].bits.bIPv6 = pdTRUE_UNSIGNED;
	}
	#endif /* ipconfigUSE_IPv6 */
	FreeRTOS_AddEndPoint( &( xInterfaces[ 0 ] ), &( xEndPoints[ 0 ] ) );
	FreeRTOS_AddEndPoint( &( xInterfaces[ 1 ] ), &( xEndPoints[ 1 ] ) );

	/* You can modify fields: */
	xEndPoints[ 0 ].bits.bIsDefault = pdTRUE;
	xEndPoints[ 0 ].bits.bWantDHCP = pdFALSE;

	FreeRTOS_IPStart();
#endif /* ipconfigMULTI_INTERFACE */

	#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
	{
		/* Create the task that handles the FTP and HTTP servers.  This will
		initialise the file system then wait for a notification from the network
		event hook before creating the servers.  The task is created at the idle
		priority, and sets itself to mainTCP_SERVER_TASK_PRIORITY after the file
		system has initialised. */
		xTaskCreate( prvServerWorkTask, "SvrWork", mainTCP_SERVER_STACK_SIZE, NULL, tskIDLE_PRIORITY, &xServerWorkTaskHandle );
	}
	#endif

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
	#if( mainHAS_SDCARD != 0 )
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

#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )

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

	void dnsCallback( const char *pcName, void *pvSearchID, uint32_t ulIPAddress)
	{
		FreeRTOS_printf( ( "DNS callback: %s with ID %04x found on %lxip\n", pcName, pvSearchID, FreeRTOS_ntohl( ulIPAddress ) ) );
	}

	static void prvServerWorkTask( void *pvParameters )
	{
	const TickType_t xInitialBlockTime = pdMS_TO_TICKS( 200UL );
	BaseType_t xWasPresent, xIsPresent;

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
		xIsPresent = xWasPresent = FF_SDDiskDetect( pxSDDisk );
		FreeRTOS_printf( ( "FF_SDDiskDetect returns -> %ld\n", xIsPresent  ) );

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
//		ulTaskNotifyTake( pdTRUE, portMAX_DELAY );

		/* Create the servers defined by the xServerConfiguration array above. */
		pxTCPServer = FreeRTOS_CreateTCPServer( xServerConfiguration, sizeof( xServerConfiguration ) / sizeof( xServerConfiguration[ 0 ] ) );
		configASSERT( pxTCPServer );

		#if( CONFIG_USE_LWIP != 0 )
		{
			vLWIP_Init();
		}
		#endif
		xServerSemaphore = xSemaphoreCreateBinary();

		for( ;; )
		{
			tcpServerWork();
			if( pxSDDisk != NULL )
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
				xHasBlocked = pdTRUE;
			}
			else
			{
				xHasBlocked = pdFALSE;
			}

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

			xSemaphoreTake( xServerSemaphore, 10 );

			if( xSocket != NULL)
			{
			char cBuffer[ 64 ];
			BaseType_t xCount;

			TickType_t xReceiveTimeOut = pdMS_TO_TICKS( 0 );

				if( xHadSocket == pdFALSE )
				{
					xHadSocket = pdTRUE;
					FreeRTOS_printf( ( "prvCommandTask started. Multi = %d IPv6 = %d\n",
						ipconfigMULTI_INTERFACE,
						ipconfigUSE_IPv6) );

					FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof( xReceiveTimeOut ) );
				}
//				xCount = FreeRTOS_recvfrom( xSocket, ( void * )cBuffer, sizeof( cBuffer ) - 1,
//						xHasBlocked ? 0 : FREERTOS_MSG_DONTWAIT, &xSourceAddress, &xSourceAddressLength );
				xCount = FreeRTOS_recvfrom( xSocket, ( void * )cBuffer, sizeof( cBuffer ) - 1,
						0, &xSourceAddress, &xSourceAddressLength );
				if( xCount > 0 )
				{
					cBuffer[ xCount ] = '\0';
					FreeRTOS_printf( ( ">> %s\n", cBuffer ) );
					if( strncmp( cBuffer, "ver", 3 ) == 0 )
					{
					int level;
					TickType_t xCount = xTaskGetTickCount();
						if( sscanf( cBuffer + 3, "%d", &level ) == 1 )
						{
							verboseLevel = level;
						}
						FreeRTOS_printf( ( "Verbose level %d time %lu\n", verboseLevel, xCount ) );
					}
					if( strncmp( cBuffer, "tcptest", 7 ) == 0 )
					{
						tcpServerStart();
					}

					if( strncmp( cBuffer, "ping", 4 ) == 0 )
					{
						uint32_t ulIPAddress;
						char *ptr = cBuffer + 4;
//						ulIPAddress = FreeRTOS_inet_addr_quick( 127, 0, 0, 1 );
						#if( ipconfigMULTI_INTERFACE != 0 )
						{
							ulIPAddress = FreeRTOS_inet_addr_quick( ucIPAddress[0], ucIPAddress[1], ucIPAddress[2], ucIPAddress[3] );
						}
						#else
						{
							FreeRTOS_GetAddressConfiguration( &ulIPAddress, NULL, NULL, NULL );
						}
						#endif

						while (isspace(*ptr)) ptr++;
						if (*ptr) {
							ulIPAddress = FreeRTOS_inet_addr( ptr );
						}
						FreeRTOS_printf( ( "ping to %xip\n", FreeRTOS_htonl(ulIPAddress) ) );
						FreeRTOS_SendPingRequest( ulIPAddress, 10, 10 );
					}
					else if( ( strncmp( cBuffer, "memstat", 7 ) == 0 ) || ( strncmp( cBuffer, "mem_stat", 8 ) == 0 ) )
					{
				#if( ipconfigUSE_TCP_MEM_STATS != 0 )
						FreeRTOS_printf( ( "Closing mem_stat\n" ) );
						iptraceMEM_STATS_CLOSE();
				#else
						FreeRTOS_printf( ( "ipconfigUSE_TCP_MEM_STATS was not defined\n" ) );
				#endif	/* ( ipconfigUSE_TCP_MEM_STATS != 0 ) */
					}
					else if( strncmp( cBuffer, "plus", 4 ) == 0 )
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
						FreeRTOS_printf( ( "Plus testing has %s for %xip\n", value ? "started" : "stopped", ulServerIPAddress ) );
					}
					if( strncmp( cBuffer, "realloc", 7 ) == 0 )
					{
						realloc_test();
					}
					if( strncmp( cBuffer, "netstat", 7 ) == 0 )
					{
						FreeRTOS_netstat();
					}
					if( strncmp( cBuffer, "crashnow", 8 ) == 0 )
					{
						zero_divisor = 0xF0937531;
						FreeRTOS_printf( ( "Divide by zero = %d\n", *((unsigned*)zero_divisor) ) );
					}
					#if( USE_LOG_EVENT != 0 )
					{
						if( strncmp( cBuffer, "event", 4 ) == 0 )
						{
							eventLogDump();
						}
					}
					#endif /* USE_LOG_EVENT */
					if( strncmp( cBuffer, "list", 4 ) == 0 )
					{
						vShowTaskTable( cBuffer[ 4 ] == 'c' );
					}
///					if( strncmp( cBuffer, "phytest", 7 ) == 0 )
///					{
///						extern void phy_test();
///						phy_test();
///					}
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
					if( strncmp( cBuffer, "dns", 3 ) == 0 )
					{
						char *ptr = cBuffer + 3;
						while (isspace (*ptr)) ptr++;
						if (*ptr) {
							static uint32_t ulSearchID = 0x0000;
							ulSearchID++;
							FreeRTOS_printf( ( "Looking up '%s'\n", ptr ) );
//							uint32_t ulIPAddress = FreeRTOS_gethostbyname_a( ptr );
//							FreeRTOS_printf( ( "Address = %lxip\n", FreeRTOS_ntohl( ulIPAddress ) ) );
							FreeRTOS_gethostbyname_a( ptr, dnsCallback, (void *)ulSearchID, 5000 );
						}
					}
					if( strncmp( cBuffer, "format", 7 ) == 0 )
					{
						if( pxSDDisk != 0 )
						{
							FF_SDDiskFormat( pxSDDisk, 0 );
						}
					}
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

#endif /* ( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) ) */
/*-----------------------------------------------------------*/

uint32_t echoServerIPAddress()
{
	return ulServerIPAddress;
}
/*-----------------------------------------------------------*/

uint32_t HAL_GetTick(void)
{
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

void vApplicationCardDetectChangeHookFromISR( BaseType_t *pxHigherPriorityTaskWoken );

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
			FreeRTOS_printf( ( "IP-address : %lxip (default %lxip)\n",
				FreeRTOS_ntohl( pxEndPoint->ulIPAddress ), FreeRTOS_ntohl( pxEndPoint->ulDefaultIPAddress ) ) );
			FreeRTOS_printf( ( "Net mask   : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ulNetMask ) ) );
			FreeRTOS_printf( ( "GW         : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ulGatewayAddress ) ) );
			FreeRTOS_printf( ( "DNS        : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ulDNSServerAddresses[ 0 ] ) ) );
			FreeRTOS_printf( ( "Broadcast  : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ulBroadcastAddress ) ) );
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

UBaseType_t uxRand( void )
{
const uint32_t ulMultiplier = 0x015a4e35UL, ulIncrement = 1UL;

	/* Utility function to generate a pseudo random number. */

	ulNextRand = ( ulMultiplier * ulNextRand ) + ulIncrement;
	return( ( int ) ( ulNextRand >> 16UL ) & 0x7fffUL );
}
/*-----------------------------------------------------------*/

static void prvSRand( UBaseType_t ulSeed )
{
	/* Utility function to seed the pseudo random number generator. */
	ulNextRand = ulSeed;
}
/*-----------------------------------------------------------*/

BaseType_t xApplicationGetRandomNumber( uint32_t *pulNumber )
{
	*pulNumber = uxRand();
	return pdTRUE;
}
/*-----------------------------------------------------------*/

extern void vApplicationPingReplyHook( ePingReplyStatus_t eStatus, uint16_t usIdentifier );
extern const char *pcApplicationHostnameHook( void );

#if( ipconfigMULTI_INTERFACE != 0 )
	BaseType_t xApplicationDNSQueryHook( NetworkEndPoint_t *pxEndPoint, const char *pcName );
#else
	BaseType_t xApplicationDNSQueryHook( const char *pcName );
#endif

void vApplicationPingReplyHook( ePingReplyStatus_t eStatus, uint16_t usIdentifier )
{
	FreeRTOS_printf( ( "Ping status %d ID = %04x\n", eStatus, usIdentifier ) ) ;
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
BaseType_t xResult;
#define FLASH_ORIGIN	0x08000000
#define FLASH_LENGTH	( 1024 * 1024 )
#define SRAM_ORIGIN		0x20000000
#define SRAM_LENGTH		( 307 * 1024 )
	if( aAddress >= FLASH_ORIGIN && aAddress < FLASH_ORIGIN + FLASH_LENGTH )
	{
		xResult = 1;
	}
	else if( aAddress >= SRAM_ORIGIN && aAddress < SRAM_ORIGIN + SRAM_LENGTH )
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


#if( ipconfigMULTI_INTERFACE != 0 )
BaseType_t xApplicationDNSQueryHook( NetworkEndPoint_t *pxEndPoint, const char *pcName )
{
BaseType_t xReturn = pdFAIL;

	#if( ipconfigUSE_IPv6 != 0 )
	if( pxEndPoint->bits.bIPv6 == pdFALSE_UNSIGNED )
	{
		FreeRTOS_printf( ( "IP[%s] IPv4request ignored\n", pcName ) );
		return pdFALSE;
	}
	#endif

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
	else if( strcasecmp( pcName, "iface1" ) == 0 )
	{
	NetworkEndPoint_t *pxNewEndPoint;
	uint32_t ulIPAddress =
			FreeRTOS_inet_addr_quick( ucIPAddress[ 0 ], ucIPAddress[ 1 ], ucIPAddress[ 2 ], ucIPAddress[ 3 ] );
		pxNewEndPoint = FreeRTOS_FindEndPointOnNetMask( ulIPAddress, 999 );
FreeRTOS_printf( ( "IP[%s] = %lxip end-point %d\n", pcName, ulIPAddress, pxNewEndPoint != NULL ) );
		if( pxNewEndPoint != NULL )
		{
			memcpy( pxEndPoint, pxNewEndPoint, sizeof *pxEndPoint );
			xReturn = pdPASS;
		}
	}
	else if( strcasecmp( pcName, "iface2" ) == 0 )
	{
	NetworkEndPoint_t *pxNewEndPoint;
	uint32_t ulIPAddress =
			FreeRTOS_inet_addr_quick( ucIPAddress2[ 0 ], ucIPAddress2[ 1 ], ucIPAddress2[ 2 ], ucIPAddress2[ 3 ] );
		pxNewEndPoint = FreeRTOS_FindEndPointOnNetMask( ulIPAddress, 999 );
FreeRTOS_printf( ( "IP[%s] = %lxip end-point %d\n", pcName, ulIPAddress, pxNewEndPoint != NULL ) );
		if( pxNewEndPoint != NULL )
		{
			memcpy( pxEndPoint, pxNewEndPoint, sizeof *pxEndPoint );
			xReturn = pdPASS;
		}
	}
/*
	BaseType_t rc1, rc2;
		rc1 = ( pxEndPoint->ulIPAddress ==
			FreeRTOS_inet_addr_quick( ucIPAddress[ 0 ], ucIPAddress[ 1 ], ucIPAddress[ 2 ], ucIPAddress[ 3 ] ) );
		if( rc1 && strcasecmp( pcName, "iface1" ) == 0 )
		{
			xReturn = pdPASS;
		}
		else
		{
			rc2 = ( pxEndPoint->ulIPAddress ==
				FreeRTOS_inet_addr_quick( ucIPAddress2[ 0 ], ucIPAddress2[ 1 ], ucIPAddress2[ 2 ], ucIPAddress2[ 3 ] ) );
			if( rc2 && strcasecmp( pcName, "iface2" ) == 0 )
			{
				xReturn = pdPASS;
			}
		}
*/

	if( strcasecmp(pcName, "wpad") != 0 )
	{
		#if( ipconfigUSE_IPv6 != 0 )
		{
			FreeRTOS_printf( ( "DNSQuery '%s': return %u for %xip and %pip\n",
				pcName, xReturn, FreeRTOS_ntohl( pxEndPoint->ulIPAddress ), pxEndPoint->ulIPAddress_IPv6.ucBytes ) );
		}
		#else
		{
			FreeRTOS_printf( ( "DNSQuery '%s': return %u for %xip\n",
				pcName, xReturn, FreeRTOS_ntohl( pxEndPoint->ulIPAddress ) ) );
		}
		#endif
	}

	return xReturn;
}
/*-----------------------------------------------------------*/
#else

BaseType_t xApplicationDNSQueryHook( const char *pcName )
{
	BaseType_t rc = strcasecmp( mainHOST_NAME, pcName) == 0 || strcasecmp( mainDEVICE_NICK_NAME, pcName) == 0;
	if (rc || verboseLevel)
		FreeRTOS_printf( ( "Comp '%s' with '%s'/'%s' %ld\n", pcName, mainHOST_NAME, mainDEVICE_NICK_NAME, rc ) );
	return rc;
}
/*-----------------------------------------------------------*/
#endif

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


void vApplicationTickHook( void )
{
//	#if( mainCREATE_SIMPLE_BLINKY_DEMO_ONLY == 0 )
//	{
//		/* The full demo includes a software timer demo/test that requires
//		prodding periodically from the tick interrupt. */
//		vTimerPeriodicISRTests();
//
//		/* Call the periodic queue overwrite from ISR demo. */
//		vQueueOverwritePeriodicISRDemo();
//
//		/* Call the periodic event group from ISR demo. */
//		vPeriodicEventGroupsProcessing();
//
//		/* Call the code that uses a mutex from an ISR. */
//		vInterruptSemaphorePeriodicTest();
//
//		/* Use a queue set from an ISR. */
//		vQueueSetAccessQueueSetFromISR();
//
//		/* Use task notifications from an ISR. */
//		xNotifyTaskFromISR();
//	}
//	#endif
}
/*-----------------------------------------------------------*/

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

size_t uxPortGetSize( void * pvAddres );
void *pvPortRealloc( void *pvAddres, size_t uxNewSize );

typedef struct xAlloc
{
	void *ptr;
	size_t size1;
}
Alloc_t;

void realloc_test()
{
	Alloc_t allocs[ 10 ];
	int index;
	for( index = 0; index < ARRAY_SIZE( allocs ); index++ )
	{
		allocs[ index ].size1 = 2 + rand() % 19;
		allocs[ index ].ptr = pvPortMalloc( allocs[ index ].size1 );
		if( allocs[ index ].ptr )
		{
			memset( allocs[ index ].ptr, 'A' + index, allocs[ index ].size1 );
		}
	}
	FreeRTOS_printf( ( "Allocated: %2d %2d %2d %2d %2d  %2d %2d %2d %2d %2d\n",
		allocs[ 0 ].size1,
		allocs[ 1 ].size1,
		allocs[ 2 ].size1,
		allocs[ 3 ].size1,
		allocs[ 4 ].size1,
		allocs[ 5 ].size1,
		allocs[ 6 ].size1,
		allocs[ 7 ].size1,
		allocs[ 8 ].size1,
		allocs[ 9 ].size1 ) );
	for( index = 0; index < ARRAY_SIZE( allocs ); index++ )
	{
		size_t get_size = uxPortGetSize( allocs[ index ].ptr );
		size_t old_size = allocs[ index ].size1;

		allocs[ index ].size1 = 6 + rand() % 15;
		allocs[ index ].ptr = pvPortRealloc( allocs[ index ].ptr, allocs[ index ].size1 );
		FreeRTOS_printf( ( "%2d: old %2d %2d new %2d contents: '%*.*s'\n",
			index,
			get_size,
			old_size,
			allocs[ index ].size1,
			allocs[ index ].size1,
			allocs[ index ].size1,
			allocs[ index ].ptr ) ) ;
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
	BaseType_t xSize = sizeof(xClient);
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

uint32_t ulApplicationGetNextSequenceNumber(
    uint32_t ulSourceAddress,
    uint16_t usSourcePort,
    uint32_t ulDestinationAddress,
    uint16_t usDestinationPort )
{
	return ipconfigRAND32();
}

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
//void SysTick_Handler(void)
//{
//	HAL_IncTick();
//}

//extern void NMI_Handler( void )                   { for( ;; ) { __NOP(); } }
//extern void HardFault_Handler( void )             { for( ;; ) { __NOP(); } }
//extern void MemManage_Handler( void )             { for( ;; ) { __NOP(); } }
//extern void BusFault_Handler( void )              { for( ;; ) { __NOP(); } }
//extern void UsageFault_Handler( void )            { for( ;; ) { __NOP(); } }
//extern void SVC_Handler( void )                   { for( ;; ) { __NOP(); } }
//extern void DebugMon_Handler( void )              { for( ;; ) { __NOP(); } }
//extern void PendSV_Handler( void )                { for( ;; ) { __NOP(); } }
//extern void SysTick_Handler( void )               { for( ;; ) { __NOP(); } }
extern void WWDG_IRQHandler( void )               { for( ;; ) { __NOP(); } }
extern void PVD_IRQHandler( void )                { for( ;; ) { __NOP(); } }
extern void TAMP_STAMP_IRQHandler( void )         { for( ;; ) { __NOP(); } }
extern void RTC_WKUP_IRQHandler( void )           { for( ;; ) { __NOP(); } }
extern void FLASH_IRQHandler( void )              { for( ;; ) { __NOP(); } }
extern void RCC_IRQHandler( void )                { for( ;; ) { __NOP(); } }
extern void EXTI0_IRQHandler( void )              { for( ;; ) { __NOP(); } }
extern void EXTI1_IRQHandler( void )              { for( ;; ) { __NOP(); } }
extern void EXTI2_IRQHandler( void )              { for( ;; ) { __NOP(); } }
extern void EXTI3_IRQHandler( void )              { for( ;; ) { __NOP(); } }
extern void EXTI4_IRQHandler( void )              { for( ;; ) { __NOP(); } }
extern void DMA1_Stream0_IRQHandler( void )       { for( ;; ) { __NOP(); } }
extern void DMA1_Stream1_IRQHandler( void )       { for( ;; ) { __NOP(); } }
extern void DMA1_Stream2_IRQHandler( void )       { for( ;; ) { __NOP(); } }
extern void DMA1_Stream3_IRQHandler( void )       { for( ;; ) { __NOP(); } }
extern void DMA1_Stream4_IRQHandler( void )       { for( ;; ) { __NOP(); } }
extern void DMA1_Stream5_IRQHandler( void )       { for( ;; ) { __NOP(); } }
extern void DMA1_Stream6_IRQHandler( void )       { for( ;; ) { __NOP(); } }
extern void ADC_IRQHandler( void )                { for( ;; ) { __NOP(); } }
extern void CAN1_TX_IRQHandler( void )            { for( ;; ) { __NOP(); } }
extern void CAN1_RX0_IRQHandler( void )           { for( ;; ) { __NOP(); } }
extern void CAN1_RX1_IRQHandler( void )           { for( ;; ) { __NOP(); } }
extern void CAN1_SCE_IRQHandler( void )           { for( ;; ) { __NOP(); } }
extern void EXTI9_5_IRQHandler( void )            { for( ;; ) { __NOP(); } }
extern void TIM1_BRK_TIM9_IRQHandler( void )      { for( ;; ) { __NOP(); } }
extern void TIM1_UP_TIM10_IRQHandler( void )      { for( ;; ) { __NOP(); } }
extern void TIM1_TRG_COM_TIM11_IRQHandler( void ) { for( ;; ) { __NOP(); } }
extern void TIM1_CC_IRQHandler( void )            { for( ;; ) { __NOP(); } }
//extern void TIM2_IRQHandler( void )               { for( ;; ) { __NOP(); } }
extern void TIM3_IRQHandler( void )               { for( ;; ) { __NOP(); } }
extern void TIM4_IRQHandler( void )               { for( ;; ) { __NOP(); } }
extern void I2C1_EV_IRQHandler( void )            { for( ;; ) { __NOP(); } }
extern void I2C1_ER_IRQHandler( void )            { for( ;; ) { __NOP(); } }
extern void I2C2_EV_IRQHandler( void )            { for( ;; ) { __NOP(); } }
extern void I2C2_ER_IRQHandler( void )            { for( ;; ) { __NOP(); } }
extern void SPI1_IRQHandler( void )               { for( ;; ) { __NOP(); } }
extern void SPI2_IRQHandler( void )               { for( ;; ) { __NOP(); } }
extern void USART1_IRQHandler( void )             { for( ;; ) { __NOP(); } }
extern void USART2_IRQHandler( void )             { for( ;; ) { __NOP(); } }
extern void USART3_IRQHandler( void )             { for( ;; ) { __NOP(); } }
//extern void EXTI15_10_IRQHandler( void )          { for( ;; ) { __NOP(); } }
extern void RTC_Alarm_IRQHandler( void )          { for( ;; ) { __NOP(); } }
extern void OTG_FS_WKUP_IRQHandler( void )        { for( ;; ) { __NOP(); } }
extern void TIM8_BRK_TIM12_IRQHandler( void )     { for( ;; ) { __NOP(); } }
extern void TIM8_UP_TIM13_IRQHandler( void )      { for( ;; ) { __NOP(); } }
extern void TIM8_TRG_COM_TIM14_IRQHandler( void ) { for( ;; ) { __NOP(); } }
extern void TIM8_CC_IRQHandler( void )            { for( ;; ) { __NOP(); } }
extern void DMA1_Stream7_IRQHandler( void )       { for( ;; ) { __NOP(); } }
extern void FMC_IRQHandler( void )                { for( ;; ) { __NOP(); } }
//extern void SDMMC1_IRQHandler( void )             { for( ;; ) { __NOP(); } }
extern void TIM5_IRQHandler( void )               { for( ;; ) { __NOP(); } }
extern void SPI3_IRQHandler( void )               { for( ;; ) { __NOP(); } }
extern void UART4_IRQHandler( void )              { for( ;; ) { __NOP(); } }
extern void UART5_IRQHandler( void )              { for( ;; ) { __NOP(); } }
extern void TIM6_DAC_IRQHandler( void )           { for( ;; ) { __NOP(); } }
extern void TIM7_IRQHandler( void )               { for( ;; ) { __NOP(); } }
extern void DMA2_Stream0_IRQHandler( void )       { for( ;; ) { __NOP(); } }
extern void DMA2_Stream1_IRQHandler( void )       { for( ;; ) { __NOP(); } }
extern void DMA2_Stream2_IRQHandler( void )       { for( ;; ) { __NOP(); } }
//extern void DMA2_Stream3_IRQHandler( void )       { for( ;; ) { __NOP(); } }
extern void DMA2_Stream4_IRQHandler( void )       { for( ;; ) { __NOP(); } }
//extern void DMA2_Stream4_IRQHandler( void )       { for( ;; ) { __NOP(); } }
//extern void ETH_IRQHandler( void )                { for( ;; ) { __NOP(); } }
extern void ETH_WKUP_IRQHandler( void )           { for( ;; ) { __NOP(); } }
extern void CAN2_TX_IRQHandler( void )            { for( ;; ) { __NOP(); } }
extern void CAN2_RX0_IRQHandler( void )           { for( ;; ) { __NOP(); } }
extern void CAN2_RX1_IRQHandler( void )           { for( ;; ) { __NOP(); } }
extern void CAN2_SCE_IRQHandler( void )           { for( ;; ) { __NOP(); } }
extern void OTG_FS_IRQHandler( void )             { for( ;; ) { __NOP(); } }
extern void DMA2_Stream5_IRQHandler( void )       { for( ;; ) { __NOP(); } }
//extern void DMA2_Stream6_IRQHandler( void )       { for( ;; ) { __NOP(); } }
extern void DMA2_Stream7_IRQHandler( void )       { for( ;; ) { __NOP(); } }
extern void USART6_IRQHandler( void )             { for( ;; ) { __NOP(); } }
extern void I2C3_EV_IRQHandler( void )            { for( ;; ) { __NOP(); } }
extern void I2C3_ER_IRQHandler( void )            { for( ;; ) { __NOP(); } }
extern void OTG_HS_EP1_OUT_IRQHandler( void )     { for( ;; ) { __NOP(); } }
extern void OTG_HS_EP1_IN_IRQHandler( void )      { for( ;; ) { __NOP(); } }
extern void OTG_HS_WKUP_IRQHandler( void )        { for( ;; ) { __NOP(); } }
extern void OTG_HS_IRQHandler( void )             { for( ;; ) { __NOP(); } }
extern void DCMI_IRQHandler( void )               { for( ;; ) { __NOP(); } }
extern void RNG_IRQHandler( void )                { for( ;; ) { __NOP(); } }
extern void FPU_IRQHandler( void )                { for( ;; ) { __NOP(); } }
extern void UART7_IRQHandler( void )              { for( ;; ) { __NOP(); } }
extern void UART8_IRQHandler( void )              { for( ;; ) { __NOP(); } }
extern void SPI4_IRQHandler( void )               { for( ;; ) { __NOP(); } }
extern void SPI5_IRQHandler( void )               { for( ;; ) { __NOP(); } }
extern void SPI6_IRQHandler( void )               { for( ;; ) { __NOP(); } }
extern void SAI1_IRQHandler( void )               { for( ;; ) { __NOP(); } }
extern void LTDC_IRQHandler( void )               { for( ;; ) { __NOP(); } }
extern void LTDC_ER_IRQHandler( void )            { for( ;; ) { __NOP(); } }
extern void DMA2D_IRQHandler( void )              { for( ;; ) { __NOP(); } }
extern void SAI2_IRQHandler( void )               { for( ;; ) { __NOP(); } }
extern void QUADSPI_IRQHandler( void )            { for( ;; ) { __NOP(); } }
extern void LPTIM1_IRQHandler( void )             { for( ;; ) { __NOP(); } }
extern void CEC_IRQHandler( void )                { for( ;; ) { __NOP(); } }
extern void I2C4_EV_IRQHandler( void )            { for( ;; ) { __NOP(); } }
extern void I2C4_ER_IRQHandler( void )            { for( ;; ) { __NOP(); } }
extern void SPDIF_RX_IRQHandler( void )           { for( ;; ) { __NOP(); } }
