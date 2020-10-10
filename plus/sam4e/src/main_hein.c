/*
	FreeRTOS V8.2.1 - Copyright (C) 2015 Real Time Engineers Ltd.
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
#include <time.h>

/* FreeRTOS includes. */
#include <FreeRTOS.h>
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

#if defined ( __ICCARM__ ) /*!< IAR Compiler */
	#define _NO_DEFINITIONS_IN_HEADER_FILES		1
#endif

//#if defined ( __GCC__ ) /*!< GCC Compiler */
//	#include <sys/unistd.h>
//#endif

#include "board.h"


/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_DHCP.h"
#include "FreeRTOS_tcp_server.h"

/* FreeRTOS+FAT includes. */
#include "ff_headers.h"
#include "ff_stdio.h"
#include "ff_ramdisk.h"
#include "ff_sddisk.h"

/* Demo application includes. */
#include "hr_gettime.h"

#include "UDPLoggingPrintf.h"

//#include "eventLogging.h"

#include "conf_board.h"

#include <asf.h>

/* FTP and HTTP servers execute in the TCP server work task. */
#define mainTCP_SERVER_TASK_PRIORITY	( tskIDLE_PRIORITY + 3 )
#define	mainTCP_SERVER_STACK_SIZE		2048

#define	mainFAT_TEST_STACK_SIZE			2048

#define mainCOMMAND_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )

#define mainSD_CARD_DISK_NAME			"/"

#define mainHAS_RAMDISK					0
#define mainHAS_SDCARD					1

#define mainHAS_FAT_TEST				1
#define mainFAT_TEST__TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )

/* Set the following constants to 1 to include the relevant server, or 0 to
exclude the relevant server. */
#define mainCREATE_FTP_SERVER			1
#define mainCREATE_HTTP_SERVER 			1



/* Define names that will be used for DNS, LLMNR and NBNS searches. */
#define mainHOST_NAME					"sam4e"
#define mainDEVICE_NICK_NAME			"sam4expro"

int verboseLevel;

FF_Disk_t *pxSDDisk;
#if( mainHAS_RAMDISK != 0 )
	static FF_Disk_t *pxRAMDisk;
#endif

#ifndef ARRAY_SIZE
#	define	ARRAY_SIZE( x )	( int )( sizeof( x ) / sizeof( x )[ 0 ] )
#endif

/* Private function prototypes -----------------------------------------------*/

/*
 * Creates a RAM disk, then creates files on the RAM disk.  The files can then
 * be viewed via the FTP server and the command line interface.
 */
static void prvCreateDiskAndExampleFiles( void );

void vStdioWithCWDTest( const char *pcMountPath );
void vCreateAndVerifyExampleFiles( const char *pcMountPath );

void vApplicationMallocFailedHook( void );
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName );
BaseType_t xApplicationDNSQueryHook( const char *pcName );

/*
 * The task that runs the FTP and HTTP servers.
 */
#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
	static void prvServerWorkTask( void *pvParameters );
	static void prvCommandTask( void *pvParameters );
#endif

void vShowTaskTable( BaseType_t aDoClear );

/* The default IP and MAC address used by the demo.  The address configuration
defined here will be used if ipconfigUSE_DHCP is 0, or if ipconfigUSE_DHCP is
1 but a DHCP server could not be contacted.  See the online documentation for
more information. */
static const uint8_t ucIPAddress[ 4 ] = { configIP_ADDR0, configIP_ADDR1, configIP_ADDR2, configIP_ADDR3 };
static const uint8_t ucNetMask[ 4 ] = { configNET_MASK0, configNET_MASK1, configNET_MASK2, configNET_MASK3 };
static const uint8_t ucGatewayAddress[ 4 ] = { configGATEWAY_ADDR0, configGATEWAY_ADDR1, configGATEWAY_ADDR2, configGATEWAY_ADDR3 };
static const uint8_t ucDNSServerAddress[ 4 ] = { configDNS_SERVER_ADDR0, configDNS_SERVER_ADDR1, configDNS_SERVER_ADDR2, configDNS_SERVER_ADDR3 };

/* Default MAC address configuration.  The demo creates a virtual network
connection that uses this MAC address by accessing the raw Ethernet data
to and from a real network connection on the host PC.  See the
configNETWORK_INTERFACE_TO_USE definition for information on how to configure
the real network connection to use. */
const uint8_t ucMACAddress[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };

/* Use by the pseudo random number generator. */
static UBaseType_t ulNextRand;

/* Handle of the task that runs the FTP and HTTP servers. */
static TaskHandle_t xServerWorkTaskHandle = NULL, xCommandTaskHandle = NULL;

extern char _estack;
#define HEAP_START		_estack
#define RAM_LENGTH		0x00020000	// 128 KB of internal SRAM
#define RAM_START		0x20000000	// at 0x20000000

volatile uint32_t ulHeapSize;
volatile uint8_t *pucHeapStart;

#if( BOARD == SAM4E_XPLAINED_PRO )
	/* The SAM4E Xplained board has 2 banks of external SRAM, each one 512KB. */
	#define EXTERNAL_SRAM_SIZE		( 512ul * 1024ul )
#endif /* SAM4E_XPLAINED_PRO */

/**
 * \brief Configure the console UART.
 */
static void configure_console(void);

extern void vApplicationIdleHook(void);
extern void vApplicationTickHook(void);

extern void xPortSysTickHandler(void);

int tickCount = 0;
/**
 * \brief Handler for Sytem Tick interrupt.
 */
//void SysTick_Handler(void)
//{
//	xPortSysTickHandler();
//	tickCount++;
//}

static void heapInit( void );

static void showMemories( void );
/**
 *  \brief FreeRTOS Real Time Kernel example entry point.
 *
 *  \return Unused (ANSI-C compatibility).
 */
int main(void)
{
	/* Initilise the SAM system */
	sysclk_init();
	board_init();
	heapInit();

	pmc_enable_periph_clk(ID_GMAC);
	pmc_enable_periph_clk(ID_TC0);
	pmc_enable_periph_clk(ID_TC3);

	pmc_enable_periph_clk(ID_SMC);

#ifdef CONF_BOARD_TWI0
	pmc_enable_periph_clk(ID_TWI0);
#endif
#ifdef CONF_BOARD_TWI1
	pmc_enable_periph_clk(ID_TWI1);
#endif
	pmc_enable_periph_clk(ID_PIOA);
	pmc_enable_periph_clk(ID_PIOB);

	/* Start a TC timer to get a hi-precision time. */
	vStartHighResolutionTimer();

//	/* Initialize the console uart */
//	configure_console();

	/* Output demo information. */
	lUDPLoggingPrintf("Started %s, compiled %s %s\n", BOARD_NAME, __DATE__, __TIME__);

	/* Initialise the network interface.

	***NOTE*** Tasks that use the network are created in the network event hook
	when the network is connected and ready for use (see the definition of
	vApplicationIPNetworkEventHook() below).  The address values passed in here
	are used if ipconfigUSE_DHCP is set to 0, or if ipconfigUSE_DHCP is set to 1
	but a DHCP server cannot be	contacted. */
	FreeRTOS_IPInit( ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress );

	#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
	{
		/* Create the task that handles the FTP and HTTP servers.  This will
		initialise the file system then wait for a notification from the network
		event hook before creating the servers.  The task is created at the idle
		priority, and sets itself to mainTCP_SERVER_TASK_PRIORITY after the file
		system has initialised. */
		xTaskCreate( prvServerWorkTask, "SvrWork", mainTCP_SERVER_STACK_SIZE, NULL, tskIDLE_PRIORITY, &xServerWorkTaskHandle );
		xTaskCreate( prvCommandTask, "Command", mainTCP_SERVER_STACK_SIZE, NULL, tskIDLE_PRIORITY, &xCommandTaskHandle );
	}
	#endif

	/* Start the scheduler. */
	vTaskStartScheduler();

	/* Will only get here if there was insufficient memory to create the idle task. */
	return 0;
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
	#if( mainHAS_SDCARD != 0 )
	{
		FreeRTOS_printf( ( "Mount SD-card\n" ) );
		/* Create the SD card disk. */
		pxSDDisk = FF_SDDiskInit( mainSD_CARD_DISK_NAME );
		FreeRTOS_printf( ( "Mount SD-card done\n" ) );
	}
	#endif	/* mainHAS_SDCARD */

//	/* Create a few example files on the disk.  These are not deleted again. */
//	vCreateAndVerifyExampleFiles( mainRAM_DISK_NAME );
//	vStdioWithCWDTest( mainRAM_DISK_NAME );
}
/*-----------------------------------------------------------*/

void systick_set_led( BaseType_t xValue )
{
	ioport_set_pin_level( LED_0_PIN, xValue );
}

void systick_toggle_led( void )
{
	ioport_toggle_pin_level(LED_0_PIN);
}

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

	#if( ( configUSE_TICKLESS_IDLE == 1 ) && ( portSLEEP_STATS == 1 ) )
		void show_timer_stats( void );
	#endif	
	static void prvCommandTask( void *pvParameters )
	{
		xSocket_t xSocket;
		BaseType_t xHadSocket = pdFALSE;

		/* Wait until the network is up before creating the servers.  The
		notification is given from the network event hook. */
		ulTaskNotifyTake( pdTRUE, portMAX_DELAY );

		/* The priority of this task can be raised now the disk has been
		initialised. */
		vTaskPrioritySet( NULL, mainCOMMAND_TASK_PRIORITY );

		for( ;; )
		{
			xSocket = xLoggingGetSocket();

			if( xSocket == NULL)
			{
				vTaskDelay( 1000 );
			}
			else
			{
			char cBuffer[ 64 ];
			BaseType_t xCount;
			struct freertos_sockaddr xSourceAddress;
			socklen_t xSourceAddressLength = sizeof( xSourceAddress );
			TickType_t xReceiveTimeOut = pdMS_TO_TICKS( 30000 );

				if( xHadSocket == pdFALSE )
				{
					xHadSocket = pdTRUE;
					FreeRTOS_printf( ( "prvCommandTask started\n" ) );

					FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof( xReceiveTimeOut ) );
				}
				xCount = FreeRTOS_recvfrom( xSocket, ( void * )cBuffer, sizeof( cBuffer ) - 1, 0,
					&xSourceAddress, &xSourceAddressLength );

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
						lUDPLoggingPrintf( "Verbose level %d\n", verboseLevel );
						#if( ( configUSE_TICKLESS_IDLE == 1 ) && ( portSLEEP_STATS == 1 ) )
						{
							show_timer_stats();
						}
						#endif	/* configUSE_TICKLESS_IDLE */
					}

					if( strncmp( cBuffer, "netstat", 7 ) == 0 )
					{
						FreeRTOS_netstat();
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
					#if( mainHAS_RAMDISK != 0 )
					{
						if( strncmp( cBuffer, "dump", 4 ) == 0 )
						{
							FF_RAMDiskStoreImage( pxRAMDisk, "ram_disk.img" );
						}
					}
					#endif	/* mainHAS_RAMDISK */
					if( strncmp( cBuffer, "format", 7 ) == 0 )
					{
						if( pxSDDisk != 0 )
						{
							FF_SDDiskFormat( pxSDDisk, 0 );
						}
					}
					#if( mainHAS_FAT_TEST != 0 )
					{
						/* Do the famous +FAT test from "ff_stdio_tests_with_cwd.c", started
						as "fattest <N>" where <N> is the number of concurrent tasks. */
						if( strncmp( cBuffer, "fattest", 7 ) == 0 )
						{
						int level = 0;
						const char pcMountPath[] = "/test";
//						const char pcMountPath[] = "/ram";

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

	static void prvServerWorkTask( void *pvParameters )
	{
	TCPServer_t *pxTCPServer = NULL;
	const TickType_t xInitialBlockTime = pdMS_TO_TICKS( 30000UL );

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

		/* Remove compiler warning about unused parameter. */
		( void ) pvParameters;

		FreeRTOS_printf( ( "Creating files\n" ) );

		/* Create the disk used by the FTP and HTTP servers. */
		prvCreateDiskAndExampleFiles();

		/* The priority of this task can be raised now the disk has been
		initialised. */
		vTaskPrioritySet( NULL, mainTCP_SERVER_TASK_PRIORITY );

		/* If the CLI is included in the build then register commands that allow
		the file system to be accessed. */
		#if( mainCREATE_UDP_CLI_TASKS == 1 )
		{
			vRegisterFileSystemCLICommands();
		}
		#endif /* mainCREATE_UDP_CLI_TASKS */


		/* Wait until the network is up before creating the servers.  The
		notification is given from the network event hook. */
		ulTaskNotifyTake( pdTRUE, portMAX_DELAY );

		/* Create the servers defined by the xServerConfiguration array above. */
		pxTCPServer = FreeRTOS_CreateTCPServer( xServerConfiguration, sizeof( xServerConfiguration ) / sizeof( xServerConfiguration[ 0 ] ) );
		configASSERT( pxTCPServer );

		#if( CONFIG_USE_LWIP != 0 )
		{
			vLWIP_Init();
		}
		#endif

		for( ;; )
		{
			FreeRTOS_TCPServerWork( pxTCPServer, xInitialBlockTime );
		}
	}

#endif /* ( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) ) */
/*-----------------------------------------------------------*/

void vApplicationIdleHook( void )
{
}
/*-----------------------------------------------------------*/

volatile const char *pcAssertedFileName;
volatile int iAssertedErrno;
volatile uint32_t ulAssertedLine;
volatile FF_Error_t xAssertedFF_Error;

void vAssertCalled( const char *pcFile, uint32_t ulLine )
{
volatile uint32_t ulBlockVariable = 0UL;

	ulAssertedLine = ulLine;
	iAssertedErrno = stdioGET_ERRNO();
	xAssertedFF_Error = stdioGET_FF_ERROR( );
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
//	FF_RAMDiskStoreImage( pxRAMDisk, "/ram_disk_image.img" );
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



/**
 * \brief This function is called by FreeRTOS each tick
 */
extern void vApplicationTickHook(void)
{
}

/*-----------------------------------------------------------*/

/* Called by FreeRTOS+TCP when the network connects or disconnects.  Disconnect
events are only received if implemented in the MAC driver. */
void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent )
{
uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
char cBuffer[ 16 ];
static BaseType_t xTasksAlreadyCreated = pdFALSE;

FreeRTOS_printf( ( "vApplicationIPNetworkEventHook: event %ld\n", eNetworkEvent ) );

	showMemories();
	/* If the network has just come up...*/
	if( eNetworkEvent == eNetworkUp )
	{
		/* Create the tasks that use the IP stack if they have not already been
		created. */
		if( xTasksAlreadyCreated == pdFALSE )
		{
			/* Tasks that use the TCP/IP stack can be created here. */

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
				/* See TBD.
				Let the server work task now it can now create the servers. */
				xTaskNotifyGive( xServerWorkTaskHandle );
				xTaskNotifyGive( xCommandTaskHandle );
			}
			#endif

			/* Start a new task to fetch logging lines and send them out. */
			vUDPLoggingTaskCreate();

			xTasksAlreadyCreated = pdTRUE;
		}

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
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	vAssertCalled( __FILE__, __LINE__ );
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName )
{
	( void ) pcTaskName;
	( void ) pxTask;

	/* Run time stack overflow checking is performed if
	configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
	function is called if a stack overflow is detected. */
	taskDISABLE_INTERRUPTS();
	for( ;; );
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

void vApplicationPingReplyHook( ePingReplyStatus_t eStatus, uint16_t usIdentifier )
{
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

BaseType_t xApplicationDNSQueryHook( const char *pcName )
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
	else
	{
		xReturn = pdFAIL;
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

static uint64_t ullHiresTime;
uint32_t ulGetRunTimeCounterValue( void )
{
	return ( uint32_t ) ( ullGetHighResolutionTime() - ullHiresTime );
}
/*-----------------------------------------------------------*/

void vShowTaskTable( BaseType_t aDoClear )
{
TaskStatus_t *pxTaskStatusArray;
volatile UBaseType_t uxArraySize, x;
uint32_t ulTotalRunTime, ulStatsAsPermille;

	// Take a snapshot of the number of tasks in case it changes while this
	// function is executing.
	uxArraySize = uxTaskGetNumberOfTasks();

	// Allocate a TaskStatus_t structure for each task.  An array could be
	// allocated statically at compile time.
	pxTaskStatusArray = pvPortMalloc( uxArraySize * sizeof( TaskStatus_t ) );

	FreeRTOS_printf( ( "Task name    Prio    Stack    Ticks   Switch   Perc \n" ) );

	if( pxTaskStatusArray != NULL )
	{
		// Generate raw status information about each task.
		uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalRunTime, aDoClear );

		// For percentage calculations.
		ulTotalRunTime /= 1000UL;

		// Avoid divide by zero errors.
		{
			// For each populated position in the pxTaskStatusArray array,
			// format the raw data as human readable ASCII data
			for( x = 0; x < uxArraySize; x++ )
			{
				// What percentage of the total run time has the task used?
				// This will always be rounded down to the nearest integer.
				// ulTotalRunTimeDiv100 has already been divided by 100.
				ulStatsAsPermille = 0;
				if( ulTotalRunTime != 0ul )
				{
					ulStatsAsPermille = pxTaskStatusArray[ x ].ulRunTimeCounter / ulTotalRunTime;
				}

				FreeRTOS_printf( ( "%-14.14s %2u %8lu %8lu %7lu %3lu.%lu %%\n",
					pxTaskStatusArray[ x ].pcTaskName,
					pxTaskStatusArray[ x ].uxCurrentPriority,
					pxTaskStatusArray[ x ].usStackHighWaterMark,
					pxTaskStatusArray[ x ].ulRunTimeCounter,
					pxTaskStatusArray[ x ].ulSwitchCounter,
					ulStatsAsPermille / 10,
					ulStatsAsPermille % 10) );
			}
		}

		// The array is no longer needed, free the memory it consumes.
		vPortFree( pxTaskStatusArray );
	}
	FreeRTOS_printf( ( "Heap: min/cur/max: %lu %lu %lu\n",
			( uint32_t )xPortGetMinimumEverFreeHeapSize(),
			( uint32_t )xPortGetFreeHeapSize(),
			( uint32_t )ulHeapSize ) );


	if( aDoClear != pdFALSE )
	{
//		ulListTime = xTaskGetTickCount();
		ullHiresTime = ullGetHighResolutionTime();
	}
}
/*-----------------------------------------------------------*/


/**
 * \brief Configure the console UART.
 */
__attribute__ ((unused)) static void configure_console(void)
{
	const usart_serial_options_t uart_serial_options = {
		.baudrate = CONF_UART_BAUDRATE,
#if (defined CONF_UART_CHAR_LENGTH)
		.charlength = CONF_UART_CHAR_LENGTH,
#endif
		.paritytype = CONF_UART_PARITY,
#if (defined CONF_UART_STOP_BITS)
		.stopbits = CONF_UART_STOP_BITS,
#endif
	};

	/* Configure console UART. */
	stdio_serial_init(CONF_UART, &uart_serial_options);

	/* Specify that stdout should not be buffered. */
#if defined(__GNUC__)
	setbuf(stdout, NULL);
#else
	/* Already the case in IAR's Normal DLIB default configuration: printf()
	 * emits one character at a time.
	 */
#endif
}


#if( ipconfigDHCP_USES_USER_HOOK != 0 )
	BaseType_t xApplicationDHCPUserHook( eDHCPCallbackQuestion_t eQuestion,
		uint32_t ulIPAddress, uint32_t ulNetMask )
	{
	BaseType_t xResult = ( BaseType_t ) eDHCPContinue;
	static BaseType_t xUseDHCP = pdTRUE;

		switch( eQuestion )
		{
		case eDHCPOffer:		/* Driver is about to ask for a DHCP offer. */
			/* Returning eDHCPContinue. */
			break;
		case eDHCPRequest:		/* Driver is about to request DHCP an IP address. */
			if( xUseDHCP == pdFALSE )
			{
				xResult = ( BaseType_t ) eDHCPStopNoChanges;
			}
			else
			{
				xResult = ( BaseType_t ) eDHCPContinue;
			}
			break;
		}
		lUDPLoggingPrintf( "DHCP %s: use = %d: %s\n",
			( eQuestion == eDHCPOffer ) ? "offer" : "request",
			xUseDHCP,
			xResult == ( BaseType_t ) eDHCPContinue ? "Cont" : "Stop" );
		return xResult;
	}
#endif	/* ipconfigDHCP_USES_USER_HOOK */


time_t FreeRTOS_time( time_t *pxTime )
{
	return xTaskGetTickCount() / 1000 + 1425522441ul;
}
/*-----------------------------------------------------------*/

static void checkMemories( HeapRegion_t *pxHeapRegions )
{
BaseType_t xIndex;
	for( xIndex = 0; pxHeapRegions[ xIndex ].pucStartAddress != NULL; xIndex++ )
	{
	volatile uint32_t *ulpMem, *ulpLast;

		ulpMem = ( volatile uint32_t * ) pxHeapRegions[ xIndex ].pucStartAddress;
		ulpLast = ulpMem + pxHeapRegions[ xIndex ].xSizeInBytes / sizeof( *ulpMem );
		while( ulpMem < ulpLast )
		{
		volatile uint32_t ulOldValue;
			ulOldValue = *ulpMem;
			*ulpMem  = ~ulOldValue;
			configASSERT( *ulpMem == ~ulOldValue );
			ulpMem++;
		}
	}
}

static void heapInit()
{
	pucHeapStart = ( uint8_t * ) ( ( ( ( uint32_t ) &HEAP_START ) + 7 ) & ~0x07ul );

	char *heapEnd = ( char * ) ( RAM_START + RAM_LENGTH );

	ulHeapSize = ( uint32_t ) ( heapEnd - &HEAP_START );
	ulHeapSize &= ~0x07ul;
	ulHeapSize -= 1024;

	HeapRegion_t xHeapRegions[] = {
		{ ( unsigned char *) pucHeapStart, ulHeapSize },
#if( BOARD == SAM4E_XPLAINED_PRO )
		{ ( unsigned char *) SRAM_BASE_ADDRESS, EXTERNAL_SRAM_SIZE },
		{ ( unsigned char *) SRAM_BASE_ADDRESS_2ND, EXTERNAL_SRAM_SIZE },
#endif /* SAM4E_XPLAINED_PRO */
		{ NULL, 0 }
 	};
	checkMemories( xHeapRegions );
	vPortDefineHeapRegions( xHeapRegions );

}

static void showMemories()
{
	lUDPLoggingPrintf("Memory: %lu + %lu + %lu = %lu Now free %lu lowest %lu\n",
		ulHeapSize,
		EXTERNAL_SRAM_SIZE,
		EXTERNAL_SRAM_SIZE,
		ulHeapSize + EXTERNAL_SRAM_SIZE + EXTERNAL_SRAM_SIZE,
		xPortGetFreeHeapSize(),
		xPortGetMinimumEverFreeHeapSize() );
}
/*-----------------------------------------------------------*/

int _write (int fd, char* buf, int nbytes)
{
	(void)fd;
	(void)buf;
	(void)nbytes;
	return 0;
}
/*-----------------------------------------------------------*/

int _read (int fd, char* buf, int nbytes)
{
	(void)fd;
	(void)buf;
	(void)nbytes;
	return 0;
}
/*-----------------------------------------------------------*/
