/*
	FreeRTOS V9.0.0 - Copyright (C) 2016 Real Time Engineers Ltd.
	All rights reserved

	VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

	This file is part of the FreeRTOS distribution.

	FreeRTOS is free software; you can redistribute it and/or modify it under
	the terms of the GNU General Public License (version 2) as published by the
	Free Software Foundation >>>> AND MODIFIED BY <<<< the FreeRTOS exception.

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
#include <ctype.h>

/* FreeRTOS includes. */
#include <FreeRTOS.h>
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "semphr.h"

#include "board.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_DHCP.h"
#include "NetworkBufferManagement.h"
#include "FreeRTOS_ARP.h"
#include "FreeRTOS_DNS.h"
#if( ipconfigUSE_IPv6 != 0 )
	#include "FreeRTOS_ND.h"
#endif
#include "FreeRTOS_TCP_server.h"
#include "NetworkInterface.h"

#if( USE_LOG_EVENT != 0 )
	#include "eventLogging.h"
#endif

#if( ipconfigMULTI_INTERFACE != 0 )
	#include "FreeRTOS_Routing.h"
#endif

#if( USE_FREERTOS_FAT != 0 )
	/* FreeRTOS+FAT includes. */
	#include "ff_headers.h"
	#include "ff_stdio.h"
	#include "ff_ramdisk.h"
	#include "ff_sddisk.h"
#endif

/* Demo application includes. */
#include "hr_gettime.h"

#include "UDPLoggingPrintf.h"

#include "conf_board.h"
#include "telnet.h"

#if( USE_IPERF != 0 )
	#include "iperf_task.h"
#endif

#if( USE_USB_CDC != 0 )
	static bool main_b_cdc_enable = false;
	#include "usb_protocol_cdc.h"
	#include "device/udi_cdc.h"
	#include "udc.h"
#endif

#include <asf.h>
#include "gmac_SAM.h"
#include "sdramc.h"

#include "plus_echo_server.h"

/* FTP and HTTP servers execute in the TCP server work task. */
#define mainTCP_SERVER_TASK_PRIORITY	( tskIDLE_PRIORITY + 3 )
#define	mainTCP_SERVER_STACK_SIZE		2048

#define	mainFAT_TEST_STACK_SIZE			2048

#define mainCOMMAND_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )

#define mainSD_CARD_DISK_NAME			"/"

#define mainHAS_RAMDISK					0
#if( USE_FREERTOS_FAT != 0 )
	#define mainHAS_SDCARD				1
	#define mainHAS_FAT_TEST			1
	#define mainFAT_TEST__TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )
	/* Set the following constants to 1 to include the relevant server, or 0 to
	exclude the relevant server. */
	#define mainCREATE_FTP_SERVER		1
	#define mainCREATE_HTTP_SERVER 		1
#else
	#define mainHAS_SDCARD				0
	#define mainHAS_FAT_TEST			0
	#define mainCREATE_FTP_SERVER		0
	#define mainCREATE_HTTP_SERVER 		0
#endif



/* Define names that will be used for DNS, LLMNR and NBNS searches. */
#define mainHOST_NAME					"sam4e"
#define mainDEVICE_NICK_NAME			"sam4expro"

/* The LED toggled by the Rx task. */
#define mainTASK_LED						( LED0_GPIO )

#define STACK_WEBSERVER_TASK  ( 512 + 256 )
#define	PRIO_WEBSERVER_TASK     2

static SemaphoreHandle_t xServerSemaphore;

int verboseLevel;

#if( USE_FREERTOS_FAT != 0 )
#error sure?
	FF_Disk_t *pxSDDisk;
	#if( mainHAS_RAMDISK != 0 )
		static FF_Disk_t *pxRAMDisk;
	#endif
	int FF_SDDiskTwoPartitionFormat(void);
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
#if( ipconfigMULTI_INTERFACE != 0 )
	BaseType_t xApplicationDNSQueryHook( NetworkEndPoint_t *pxEndPoint, const char *pcName );
#else
	BaseType_t xApplicationDNSQueryHook( const char *pcName );
#endif

/*
 * The task that runs the FTP and HTTP servers.
 */
#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
	static void prvServerWorkTask( void *pvParameters );
#endif

static void prvCommandTask( void *pvParameters );

void vShowTaskTable( BaseType_t aDoClear );

#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
	void file_create_test ( void );
#endif

#if !defined( CONF_BOARD_ENABLE_CACHE )
	#warning Caching is off
#elif defined( CONF_BOARD_ENABLE_CACHE )
	#warning Caching is on
#endif

#define	TST_MEM_SPEED_TEST	1

/*-----------------------------------------------------------*/
#if( TST_MEM_SPEED_TEST != 0 )
	static void memory_speed_test ( void );
#endif

/* The default IP and MAC address used by the demo.  The address configuration
defined here will be used if ipconfigUSE_DHCP is 0, or if ipconfigUSE_DHCP is
1 but a DHCP server could not be contacted.  See the online documentation for
more information. */
static const uint8_t ucIPAddress[ 4 ] = { configIP_ADDR0, configIP_ADDR1, configIP_ADDR2, configIP_ADDR3 };
static const uint8_t ucNetMask[ 4 ] = { configNET_MASK0, configNET_MASK1, configNET_MASK2, configNET_MASK3 };
static const uint8_t ucGatewayAddress[ 4 ] = { configGATEWAY_ADDR0, configGATEWAY_ADDR1, configGATEWAY_ADDR2, configGATEWAY_ADDR3 };
static const uint8_t ucDNSServerAddress[ 4 ] = { configDNS_SERVER_ADDR0, configDNS_SERVER_ADDR1, configDNS_SERVER_ADDR2, configDNS_SERVER_ADDR3 };

#if( ipconfigMULTI_INTERFACE == 1 )
	static const uint8_t ucIPAddress2[ 4 ] = { configIP_ADDR0, configIP_ADDR1, configIP_ADDR2, configIP_ADDR3 };	// 192.168.2.124
	static const uint8_t ucNetMask2[ 4 ] = { configNET_MASK0, configNET_MASK1, configNET_MASK2, configNET_MASK3 };
	static const uint8_t ucGatewayAddress2[ 4 ] = { configGATEWAY_ADDR0, configGATEWAY_ADDR1, configGATEWAY_ADDR2, configGATEWAY_ADDR3 };
	static const uint8_t ucDNSServerAddress2[ 4 ] = { configDNS_SERVER_ADDR0, configDNS_SERVER_ADDR1, configDNS_SERVER_ADDR2, configDNS_SERVER_ADDR3 };

	static const uint8_t ucIPAddress3[ 4 ] = { 127, 0, 0, 1 };	// 127.0.0.1
	static const uint8_t ucNetMask3[ 4 ] = { 255, 0, 0, 0 };
	static const uint8_t ucGatewayAddress3[ 4 ] = { 0, 0, 0, 0};
	static const uint8_t ucDNSServerAddress3[ 4 ] = { 0, 0, 0, 0 };
#endif

uint32_t ulServerIPAddress;

/* Default MAC address configuration.  The demo creates a virtual network
connection that uses this MAC address by accessing the raw Ethernet data
to and from a real network connection on the host PC.  See the
configNETWORK_INTERFACE_TO_USE definition for information on how to configure
the real network connection to use. */
#if( ipconfigMULTI_INTERFACE == 0 )
	const uint8_t ucMACAddress[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };
#else
	/* 00:11:22:33:44:49 */
	static const uint8_t ucMACAddress[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };
	/* 00:11:22:33:44:4A */
	static const uint8_t ucMACAddress2[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 + 1 };
	static const uint8_t ucMACAddress3[ 6 ] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x7f };
#endif /* ipconfigMULTI_INTERFACE */

#if( ipconfigMULTI_INTERFACE != 0 )
	NetworkInterface_t *pxMicrel_FillInterfaceDescriptor( BaseType_t xEMACIndex, NetworkInterface_t *pxInterface );
	NetworkInterface_t *pxSAM_GMAC_FillInterfaceDescriptor( BaseType_t xEMACIndex, NetworkInterface_t *pxInterface );
	NetworkInterface_t *pxLoopback_FillInterfaceDescriptor( BaseType_t xEMACIndex, NetworkInterface_t *pxInterface );
#endif /* ipconfigMULTI_INTERFACE */

/* Use by the pseudo random number generator. */
static UBaseType_t ulNextRand;

/* Handle of the task that runs the FTP and HTTP servers. */
static TaskHandle_t xServerWorkTaskHandle = NULL, xCommandTaskHandle = NULL;

volatile uint32_t ulHeapSize;
volatile uint8_t *pucHeapStart;

#if( BOARD == SAM4E_XPLAINED_PRO )
	/* The SAM4E Xplained board has 2 banks of external SRAM, each one 512KB. */
	#define EXTERNAL_SRAM_SIZE		( 512ul * 1024ul )
#endif /* SAM4E_XPLAINED_PRO */

/**
 * \brief Configure the console UART.
 */
//static void configure_console(void);

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

volatile unsigned short idle_cycle_count;

static void heapInit( void );

static void vEthernetInit( void );

static void showMemories( void );
/**
 *  \brief FreeRTOS Real Time Kernel example entry point.
 *
 *  \return Unused (ANSI-C compatibility).
 */
int main( void )
{
#if( ipconfigMULTI_INTERFACE != 0 )
	static NetworkInterface_t xInterfaces[ 3 ];
	static NetworkEndPoint_t xEndPoints[ 3 ];
#endif /* ipconfigMULTI_INTERFACE */

	/* Initialise the SAM system */
	sysclk_init();
	board_init();
	heapInit();

	#if( USE_LOG_EVENT != 0 )
	{
		iEventLogInit();
	}
	#endif
//	pmc_enable_periph_clk(ID_SMC);

//	/* Enable SDRAMC peripheral clock */
//	pmc_enable_periph_clk(ID_SDRAMC);
//
//	/* Complete SDRAM configuration */
//	sdramc_init((sdramc_memory_dev_t *)&SDRAM_ISSI_IS42S16100E,
//			sysclk_get_cpu_hz());
//	sdram_enable_unaligned_support();
//
//	{
//		uint32_t *sdram = (uint32_t *)SDRAM_BASE_ADDRESS;
//		sdram[0] = ~0ul;
//		sdram[1] = 0ul;
//		sdram[2] = ~0ul;
//		sdram[3] = 0ul;
//	}

	pmc_enable_periph_clk(ID_GMAC);
	pmc_enable_periph_clk(ID_TC0);
	pmc_enable_periph_clk(ID_TC1);
	pmc_enable_periph_clk(ID_TC2);
	pmc_enable_periph_clk(ID_TC3);

#ifdef CONF_BOARD_TWI0
	pmc_enable_periph_clk(ID_TWI0);
#endif
#ifdef CONF_BOARD_TWI1
	pmc_enable_periph_clk(ID_TWI1);
#endif
	pmc_enable_periph_clk(ID_PIOA);
	pmc_enable_periph_clk(ID_PIOB);

//	pmc_enable_periph_clk(ID_SPI);
	/* Start a TC timer to get a hi-precision time. */
	vStartHighResolutionTimer();

	vEthernetInit();

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
#if( ipconfigMULTI_INTERFACE == 0 )
	FreeRTOS_IPInit( ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress );
#else

#define NIC_SWAPPED 0

#if 1
	#if( NIC_SWAPPED == 0 )
		pxSAM_GMAC_FillInterfaceDescriptor( 0, &( xInterfaces[ 0 ] ) );
	#else	
		pxMicrel_FillInterfaceDescriptor( 0, &( xInterfaces[ 0 ] ) );
	#endif

	/* 172.16.0.100 */
	FreeRTOS_AddNetworkInterface( &( xInterfaces[ 0 ] ) );
	FreeRTOS_FillEndPoint( &( xEndPoints[ 0 ] ), ucIPAddress2, ucNetMask2, ucGatewayAddress2, ucDNSServerAddress2, ucMACAddress2 );
	#if( ipconfigUSE_IPv6 != 0 )
		{
		BaseType_t xIndex;
			// ff02::1:ff66:4a80
			// ping 192.168.2.124
			// ping fe80::8d11:cd9b:8b66:4a80
			// MAC address = 00:11:22:33:44:49
			unsigned short pusAddress[] = { 0xfe80, 0x0000, 0x0000, 0x0000, 0x8d11, 0xcd9b, 0x8b66, 0x4a80 };
			for( xIndex = 0; xIndex < ARRAY_SIZE( pusAddress ); xIndex++ )
			{
				xEndPoints[ 0 ].ulIPAddress_IPv6.usShorts[ xIndex ] = FreeRTOS_htons( pusAddress[ xIndex ] );
			}
			xEndPoints[ 0 ].bits.bIPv6 = pdTRUE_UNSIGNED;
			// The multicast MAC address for "80fe::118d:9bcd:668b:784a" will be "33:33:ff:8b:78:4a"
		}
	#endif
	FreeRTOS_AddEndPoint( &( xInterfaces[ 0 ] ), &( xEndPoints[ 0 ] ) );
	// You can modify fields:
	xEndPoints[ 0 ].bits.bIsDefault = pdTRUE_UNSIGNED;
	xEndPoints[ 0 ].bits.bWantDHCP = pdFALSE_UNSIGNED;
#endif /* 1 */

/*
#if( NIC_SWAPPED == 0 )
	pxMicrel_FillInterfaceDescriptor( 1, &( xInterfaces[ 1 ] ) );
#else	
	pxSAM_GMAC_FillInterfaceDescriptor( 1, &( xInterfaces[ 1 ] ) );
#endif

	// 192.168.2.110
	FreeRTOS_AddNetworkInterface( &( xInterfaces[ 1 ] ) );
	FreeRTOS_FillEndPoint( &( xEndPoints[ 1 ] ), ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress );

	#if( ipconfigUSE_IPv6 != 0 )
		{
		BaseType_t xIndex;
			// ping 172.16.0.100
			// ping fe80::8d11:cd9b:8b66:4a81
			// MAC address = 00:11:22:33:44:4A
			unsigned short pusAddress[] = { 0xfe80, 0x0000, 0x0000, 0x0000, 0x8d11, 0xcd9b, 0x8b66, 0x4a81 };
			for( xIndex = 0; xIndex < ARRAY_SIZE( pusAddress ); xIndex++ )
			{
				xEndPoints[ 1 ].ulIPAddress_IPv6.usShorts[ xIndex ] = FreeRTOS_htons( pusAddress[ xIndex ] );
			}
			xEndPoints[ 1 ].bits.bIPv6 = pdTRUE_UNSIGNED;
		}
	#endif
	FreeRTOS_AddEndPoint( &( xInterfaces[ 1 ] ), &( xEndPoints[ 1 ] ) );
	// You can modify fields:
	xEndPoints[ 1 ].bits.bIsDefault = pdTRUE_UNSIGNED;
	xEndPoints[ 1 ].bits.bWantDHCP = pdTRUE_UNSIGNED;
*/

	pxLoopback_FillInterfaceDescriptor( 0, &( xInterfaces[ 2 ] ) );
	FreeRTOS_AddNetworkInterface( &( xInterfaces[ 2 ] ) );
	FreeRTOS_FillEndPoint( &( xEndPoints[ 2 ] ), ucIPAddress3, ucNetMask3, ucGatewayAddress3, ucDNSServerAddress3, ucMACAddress3 );
	FreeRTOS_AddEndPoint( &( xInterfaces[ 2 ] ), &( xEndPoints[ 2 ] ) );

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

	xTaskCreate( prvCommandTask, "Command", mainTCP_SERVER_STACK_SIZE, NULL, tskIDLE_PRIORITY, &xCommandTaskHandle );

	/* Start the scheduler. */
	vTaskStartScheduler();

	/* Will only get here if there was insufficient memory to create the idle task. */
	return 0;
}

BaseType_t xApplicationGetRandomNumber( uint32_t *pulNumber )
{
	uint32_t ulR1 = uxRand() & 0x7fff;
	uint32_t ulR2 = uxRand() & 0x7fff;
	uint32_t ulR3 = uxRand() & 0x7fff;
	*pulNumber = (ulR1 << 17) | (ulR2 << 2) | (ulR3 & 0x03uL);
	return pdTRUE;
}

static void vEthernetInit()
{
	pio_set_output(PIN_GMAC_RESET_PIO, PIN_GMAC_RESET_MASK, 1,  false, true);
	pio_set_input(PIN_GMAC_INT_PIO, PIN_GMAC_INT_MASK, PIO_PULLUP);
	pio_set_peripheral(PIN_GMAC_PIO, PIN_GMAC_PERIPH, PIN_GMAC_MASK);

	/* Configure GMAC runtime clock */
	gmac_set_mdc_clock(GMAC, sysclk_get_cpu_hz());
	/* Enable GMAC clock */
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
//	ff_mkdir( "/fattest" );
///	vCreateAndVerifyExampleFiles( "/fattest" );
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

#if( USE_USB_CDC != 0 )

static TickType_t xLastLogTime;
static UBaseType_t uxCDCCount;

static void handle_cdc_input()
{
	for( ;; )
	{
	char buffer[129];

		int count = udi_cdc_get_nb_received_data();
		if(count == 0)
			break;

		if (count > (int)(sizeof buffer - 1)) {
			count = sizeof buffer - 1;
		}
		int rc = udi_cdc_read_buf((void*)buffer, count);
		if (rc > 0) {
//			buffer[count] = '\0';
//			lUDPLoggingPrintf("CDC %d: '%s'\n", count, buffer);
		//	udi_cdc_write_buf((void*)buffer, count);
			uxCDCCount += count;
		}
	}
	if( uxCDCCount != 0 )
	{
		TickType_t xNow = xTaskGetTickCount();

		if( ( xNow - xLastLogTime ) > 1000 )
		{
		UBaseType_t xCount = uxCDCCount;
		uxCDCCount = 0;

			lUDPLoggingPrintf("Received %lu bytes\n", xCount );
			xLastLogTime = xNow;
		}
	}
}
#endif


static int iFATRunning = 0;

extern void phy_test(void);

	Telnet_t xTelnet;
	void vUDPLoggingHook( const char *pcMessage, BaseType_t xLength )
	{
	}

	#if( USE_FREERTOS_FAT != 0 )
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
	#endif

	#if( ( configUSE_TICKLESS_IDLE == 1 ) && ( portSLEEP_STATS == 1 ) )
		void show_timer_stats( void );
	#endif

#ifndef ipconfigUSE_IPv6
	#define ipconfigUSE_IPv6	0
#endif

void dnsCallback( const char *pcName, void *pvSearchID, uint32_t ulIPAddress)
{
	FreeRTOS_printf( ( "DNS callback: %s with ID %04x found on %lxip\n", pcName, pvSearchID, FreeRTOS_ntohl( ulIPAddress ) ) );
}


//	void vMCIEventShow( void );
static void prvCommandTask( void *pvParameters )
{
	xSocket_t xSocket;
	BaseType_t xHadSocket = pdFALSE;
	BaseType_t xLastCount = 0;
	char cLastBuffer[ 16 ];
	char cBuffer[ 128 ];

	xServerSemaphore = xSemaphoreCreateBinary();
	configASSERT( xServerSemaphore != NULL );

#if( USE_USB_CDC != 0 )
	// Start USB stack to authorize VBus monitoring
	udc_start();
#endif

	/* Wait until the network is up before creating the servers.  The
	notification is given from the network event hook. */
	ulTaskNotifyTake( pdTRUE, portMAX_DELAY );

	/* The priority of this task can be raised now the disk has been
	initialised. */
	vTaskPrioritySet( NULL, mainCOMMAND_TASK_PRIORITY );

	xTelnetCreate( &( xTelnet ), 23 );

	for( ;; )
	{
	TickType_t xReceiveTimeOut = pdMS_TO_TICKS( 200 );

		xSemaphoreTake( xServerSemaphore, xReceiveTimeOut );
//{
//	static int counter = 0;
//	if (++counter == 4) {
//		ioport_toggle_pin_level( mainTASK_LED );
//		counter = 0;
//	}
//}
		xSocket = xLoggingGetSocket();
		#if( USE_USB_CDC != 0 )
		{
			handle_cdc_input();
		}
		#endif

		if( xSocket != NULL)
		{
		BaseType_t xCount;
		struct freertos_sockaddr xSourceAddress;
		socklen_t xSourceAddressLength = sizeof( xSourceAddress );

			if( xHadSocket == pdFALSE )
			{
				xHadSocket = pdTRUE;
				FreeRTOS_printf( ( "prvCommandTask started. Multi = %d IPv6 = %d\n",
					ipconfigMULTI_INTERFACE,
					ipconfigUSE_IPv6 ) );
				/* xServerSemaphore will be given to when there is data for xSocket
				and also as soon as there is USB/CDC data. */
				FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_SET_SEMAPHORE, ( void * ) &xServerSemaphore, sizeof( xServerSemaphore ) );
				//FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof( xReceiveTimeOut ) );
			}
			xCount = xTelnetRecv( &xTelnet, &xSourceAddress, cBuffer, sizeof( cBuffer ) - 1 );
			if( xCount == 0 )
			{
				xCount = FreeRTOS_recvfrom( xSocket, ( void * )cBuffer, sizeof( cBuffer )-1, FREERTOS_MSG_DONTWAIT,
					&xSourceAddress, &xSourceAddressLength );
			}

			if( ( xCount >= 3 ) && ( xCount <= 5 ) && ( cBuffer[ 0 ] == 0x1b ) && ( cBuffer[ 1 ] == 0x5b ) && ( cBuffer[ 2 ] == 0x41 ) )
			{
				xCount = xLastCount;
				memcpy( cBuffer, cLastBuffer, sizeof cLastBuffer );
			}
			else if( xCount > 0 )
			{
				xLastCount = xCount;
				memcpy( cLastBuffer, cBuffer, sizeof cLastBuffer );
			}

			if( ( xCount > 0 ) && ( cBuffer[ 0 ] >= 32 ) && ( cBuffer[ 0 ] < 0x7F ) )
			{
				cBuffer[ xCount ] = '\0';
				FreeRTOS_printf( ( ">> %s\n", cBuffer ) );
ioport_toggle_pin_level( mainTASK_LED );
				if( strncmp( cBuffer, "ver", 3 ) == 0 )
				{
				int level;
				extern int tx_release_count[ 4 ];

					if( sscanf( cBuffer + 3, "%d", &level ) == 1 )
					{
						verboseLevel = level;
					}
					lUDPLoggingPrintf( "Verbose level %d (%d, %d, %d, %d)\n", verboseLevel, tx_release_count[0], tx_release_count[1], tx_release_count[2], tx_release_count[3] );
					#if( ( configUSE_TICKLESS_IDLE == 1 ) && ( portSLEEP_STATS == 1 ) )
					{
						show_timer_stats();
					}
					#endif	/* configUSE_TICKLESS_IDLE */
				}
				if (strncasecmp (cBuffer, "clock", 5) == 0) {
					FreeRTOS_printf( ( "sysclk_get_main_hz = %lu sysclk_get_peripheral_hz %lu\n",
						sysclk_get_main_hz(),
						sysclk_get_peripheral_hz() ) );
				}
				if (strncasecmp (cBuffer, "dns", 3) == 0) {
					char *ptr = cBuffer + 3;
					while (isspace (*ptr)) ptr++;
					if (*ptr) {
						static uint32_t ulSearchID = 0x0000;
						ulSearchID++;
						FreeRTOS_printf( ( "Looking up '%s'\n", ptr ) );
						#if( ipconfigDNS_USE_CALLBACKS != 0 )
//							uint32_t ulIPAddress = FreeRTOS_gethostbyname_a( ptr );
//							FreeRTOS_printf( ( "Address = %lxip\n", FreeRTOS_ntohl( ulIPAddress ) ) );
						FreeRTOS_gethostbyname_a( ptr, dnsCallback, (void *)ulSearchID, 5000 );
						#endif
					}
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
				if( strncmp( cBuffer, "plus", 4 ) == 0 )
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
				#if( USE_USB_CDC != 0 )
				{
					if( strncmp( cBuffer, "cdc", 3 ) == 0 )
					{
						handle_cdc_input();
						// Transfer UART RX fifo to CDC TX
						if (!udi_cdc_is_tx_ready()) {
							// Fifo full
							//udi_cdc_signal_overrun();
							//ui_com_overflow();
							lUDPLoggingPrintf("CDC overflow\n");
						} else {
							char *ptr = cBuffer + 3;
							int length = strlen (ptr);
							lUDPLoggingPrintf("CDC send '%s'\n", ptr);
							if (length) {
								udi_cdc_write_buf((void*)ptr, length);
							}
						}
					}
				}
				#endif
				#if( USE_IPERF != 0 )
				{
					if( strncmp( cBuffer, "iperf", 5 ) == 0 )
					{
						vIPerfInstall();
					}
				}
				#endif

				#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
				{
					if( strncmp( cBuffer, "filecreate", 10 ) == 0 )
					{
						file_create_test ();
					}
				}
				#endif
				#if( TST_MEM_SPEED_TEST != 0 )
				{
					if( strncmp( cBuffer, "memtest", 7 ) == 0 )
					{
						memory_speed_test ();
					}
				}
				#endif /* TST_MEM_SPEED_TEST */
				if( strncmp( cBuffer, "mem", 3 ) == 0 )
				{
					showMemories();
				}

				if( strncmp( cBuffer, "netstat", 7 ) == 0 )
				{
					FreeRTOS_netstat();
				}
//					if( strncmp( cBuffer, "stat", 4 ) == 0 )
//					{
//						vMCIEventShow();
//					}
				#if( USE_LOG_EVENT != 0 )
				{
					if( strncmp( cBuffer, "event", 5 ) == 0 )
					{
						if( cBuffer[ 5 ] == 'c' )
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
				#if( USE_FREERTOS_FAT != 0 )
				{
					if( strncmp( cBuffer, "format", 6 ) == 0 )
					{
						if( pxSDDisk != 0 )
						{
//							FF_SDDiskTwoPartitionFormat();
							FF_SDDiskFormat( pxSDDisk, 0 );
						}
					}
				}
				#endif
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
/*-----------------------------------------------------------*/

uint32_t echoServerIPAddress()
{
	return ulServerIPAddress;
}
/*-----------------------------------------------------------*/

#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )

	TCPServer_t *pxTCPServer = NULL;
	static void prvServerWorkTask( void *pvParameters )
	{
	const TickType_t xInitialBlockTime = pdMS_TO_TICKS( 30000UL );

	/* A structure that defines the servers to be created.  Which servers are
	included in the structure depends on the mainCREATE_HTTP_SERVER and
	mainCREATE_FTP_SERVER settings at the top of this file. */
	static const struct xSERVER_CONFIG xServerConfiguration[] =
	{
		#if( mainCREATE_HTTP_SERVER == 1 )
#warning Has HTTP server
				/* Server type,		port number,	backlog, 	root dir. */
				{ eSERVER_HTTP, 	80, 			12, 		configHTTP_ROOT },
		#endif

		#if( mainCREATE_FTP_SERVER == 1 )
#warning Has FTP server
				/* Server type,		port number,	backlog, 	root dir. */
				{ eSERVER_FTP,  	21, 			12, 		"" }
		#endif
	};

		/* Remove compiler warning about unused parameter. */
		( void ) pvParameters;

		FreeRTOS_printf( ( "prvServerWorkTask: Creating files\n" ) );

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

		FreeRTOS_printf( ( "prvServerWorkTask: starting loop\n" ) );
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
#if( USE_FREERTOS_FAT != 0 )
	volatile FF_Error_t xAssertedFF_Error;
	volatile int iAssertedErrno;
#endif
volatile uint32_t ulAssertedLine;

void vAssertCalled( const char *pcFile, uint32_t ulLine )
{
volatile uint32_t ulBlockVariable = 0UL;

	ulAssertedLine = ulLine;
	#if( USE_FREERTOS_FAT != 0 )
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
//	FreeRTOS_printf( ( "vAssertCalled( %s, %ld\n", pcFile, ulLine ) );

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

/* Called by FreeRTOS+TCP when the network connects or disconnects.  Disconnect
events are only received if implemented in the MAC driver. */
#if( ipconfigMULTI_INTERFACE != 0 )
	void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent, NetworkEndPoint_t *pxEndPoint )
#else
	void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent )
#endif
{
uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
char cBuffer[ 16 ];
static BaseType_t xTasksAlreadyCreated = pdFALSE;

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
			}
			#endif

			/* Wake-up prvCommandTask() */
			xTaskNotifyGive( xCommandTaskHandle );

			/* Start a new task to fetch logging lines and send them out. */
			vUDPLoggingTaskCreate();

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
	FreeRTOS_printf( ( "prvSRand(0x%08lX)\n", ulSeed ) );
	/* Utility function to seed the pseudo random number generator. */
	ulNextRand = ulSeed;
}
/*-----------------------------------------------------------*/

void vApplicationPingReplyHook( ePingReplyStatus_t eStatus, uint16_t usIdentifier )
{
	FreeRTOS_printf( ( "Received reply: status %d ID %04x\n", eStatus, usIdentifier ) );
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
#else
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
	else
	{
		xReturn = pdFAIL;
	}

	return xReturn;
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
/*-----------------------------------------------------------*/

extern BaseType_t xTaskClearCounters;
void vShowTaskTable( BaseType_t aDoClear )
{
TaskStatus_t *pxTaskStatusArray;
volatile UBaseType_t uxArraySize, x;
uint64_t ullTotalRunTime;
uint32_t ulStatsAsPermille;

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
		{
			// For each populated position in the pxTaskStatusArray array,
			// format the raw data as human readable ASCII data
			for( x = 0; x < uxArraySize; x++ )
			{
				// What percentage of the total run time has the task used?
				// This will always be rounded down to the nearest integer.
				// ulTotalRunTimeDiv100 has already been divided by 100.
				if( ullTotalRunTime != 0ul )
				{
					ulStatsAsPermille = pxTaskStatusArray[ x ].ulRunTimeCounter / ullTotalRunTime;
				}
				else
				{
					ulStatsAsPermille = 0ul;
				}

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
	FreeRTOS_printf( ( "Heap: min/cur/max: %lu %lu %lu uxArraySize %lu\n",
			( uint32_t )xPortGetMinimumEverFreeHeapSize(),
			( uint32_t )xPortGetFreeHeapSize(),
			( uint32_t )ulHeapSize,
			( uint32_t )uxArraySize ) );
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
//__attribute__ ((unused)) static void configure_console(void)
//{
//	const usart_serial_options_t uart_serial_options = {
//		.baudrate = CONF_UART_BAUDRATE,
//#if (defined CONF_UART_CHAR_LENGTH)
//		.charlength = CONF_UART_CHAR_LENGTH,
//#endif
//		.paritytype = CONF_UART_PARITY,
//#if (defined CONF_UART_STOP_BITS)
//		.stopbits = CONF_UART_STOP_BITS,
//#endif
//	};
//
//	/* Configure console UART. */
//	stdio_serial_init(CONF_UART, &uart_serial_options);
//
//	/* Specify that stdout should not be buffered. */
//#if defined(__GNUC__)
//	setbuf(stdout, NULL);
//#else
//	/* Already the case in IAR's Normal DLIB default configuration: printf()
//	 * emits one character at a time.
//	 */
//#endif
//}


#if( ipconfigUSE_DHCP_HOOK != 0 )
	eDHCPCallbackAnswer_t xApplicationDHCPHook( eDHCPCallbackPhase_t eDHCPPhase, uint32_t ulIPAddress )
	{
	eDHCPCallbackAnswer_t xResult = eDHCPContinue;
	static BaseType_t xUseDHCP = pdTRUE;

		switch( eDHCPPhase )
		{
		case eDHCPPhasePreDiscover:		/* Driver is about to ask for a DHCP offer. */
			/* Returning eDHCPContinue. */
			break;
		case eDHCPPhasePreRequest:		/* Driver is about to request DHCP an IP address. */
			if( xUseDHCP == pdFALSE )
			{
				xResult = eDHCPStopNoChanges;
			}
			else
			{
				xResult = eDHCPContinue;
			}
			break;
		}
		lUDPLoggingPrintf( "DHCP %s: use = %d: %s\n",
			( eDHCPPhase == eDHCPPhasePreDiscover ) ? "pre-discover" : "pre-request",
			xUseDHCP,
			xResult == eDHCPContinue ? "Continue" : "Stop" );
		return xResult;
	}
#endif	/* ipconfigUSE_DHCP_HOOK */


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

//static uint32_t ulMallocSpace[ ( 64 * 1024 ) / sizeof( uint32_t ) ];

extern char _estack;
#define HEAP_START		_estack

static void heapInit()
{
	pucHeapStart = ( uint8_t * ) ( ( ( ( uint32_t ) &HEAP_START ) + 7 ) & ~0x07ul );

	char *heapEnd = ( char * ) ( SRAM_END_ADDRESS + 1 );

	ulHeapSize = ( uint32_t ) ( heapEnd - &HEAP_START );
	ulHeapSize &= ~0x07ul;
	ulHeapSize -= 1024;

	HeapRegion_t xHeapRegions[] = {
		{ ( unsigned char *) pucHeapStart, ulHeapSize },
		{ NULL, 0 }
 	};
	//checkMemories( xHeapRegions );
	vPortDefineHeapRegions( xHeapRegions );
}

void *malloc( size_t uxSize )
{
	return pvPortMalloc( uxSize );
}

void free( void *pvMem )
{
	vPortFree( pvMem );
}

void *_malloc_r( struct _reent * pxReent, size_t uxSize )
{
	return pvPortMalloc( uxSize );
}

void _free_r( struct _reent * pxReent, void *pvMem )
{
	vPortFree( pvMem );
}

void *_calloc_r( struct _reent * pxReent, size_t uxCount, size_t uxSize )
{
	size_t uxTotal = uxCount * uxSize;
	void *pvResult = pvPortMalloc( uxTotal );
	if( pvResult != NULL )
	{
		memset( pvResult, '\0', sizeof uxTotal );
	}
	return pvResult;
}


static void showMemories()
{
	lUDPLoggingPrintf("Memory: %lu + %lu + %lu = %lu Now free %lu lowest %lu\n",
		ulHeapSize,
		SDRAM_SIZE,
		SDRAM_SIZE,
		ulHeapSize + SDRAM_SIZE + SDRAM_SIZE,
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

void vOutputChar( const char cChar, const TickType_t xTicksToWait  )
{

}

BaseType_t xApplicationMemoryPermissions( uint32_t aAddress )
{
//	#define FLASH_BASE_ADDRESS	0x00400000
//	#define ROM_BASE_ADDRESS	0x00800000
//	#define SRAM_BASE_ADDRESS	0x20400000
//	#define SDRAM_BASE_ADDRESS	0x60000000
//	
//	#define FLASH_SIZE			( 2ul * _MBYTE_ )
//	#define ROM_SIZE   			( 4ul * _MBYTE_ )
//	#define SRAM_SIZE			( 384 * _KBYTE_ )
//	#define SDRAM_SIZE 			( 16ul * _MBYTE_ )

	if( ( aAddress >= SRAM_BASE_ADDRESS ) && ( aAddress < SRAM_BASE_ADDRESS + SRAM_SIZE ) )
		return 3;
	if( ( aAddress >= SDRAM_BASE_ADDRESS ) && ( aAddress < SDRAM_BASE_ADDRESS + SDRAM_SIZE ) )
		return 3;
	if( ( aAddress >= FLASH_BASE_ADDRESS ) && ( aAddress < FLASH_BASE_ADDRESS + FLASH_SIZE ) )
		return 1;
	if( ( aAddress >= ROM_BASE_ADDRESS ) && ( aAddress < ROM_BASE_ADDRESS + ROM_SIZE ) )
		return 1;
	return 0;
}

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
	taskDISABLE_INTERRUPTS();

	/* When the following line is hit, the variables contain the register values. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void HardFault_Handler( void ) __attribute__( ( naked ) );
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

#if TST_MEM_SPEED_TEST
struct __attribute__ ((__packed__)) SNonAligned {
	unsigned short begin;
	unsigned char a1[4];
	unsigned char a2[4];
	unsigned char a3[4];
	unsigned char a4[4];
	unsigned short end;
};

struct __attribute__ ((__packed__)) SAligned {
	unsigned char a1[4];
	unsigned char a2[4];
	unsigned char a3[4];
	unsigned char a4[4];
	unsigned short begin;
	unsigned short end;
};

__attribute__((__noinline__)) static unsigned write_speed_test (volatile unsigned *apPtr, const int aCount, const int aSize)
{
int loop;
unsigned sum = 0;
	for( loop = aCount; loop--; )
	{
		volatile unsigned *source = apPtr;
		int index;
		for( index = aSize - 8; index--; )
		{
			source[0] = loop;
			source[1] = loop;
			source[2] = loop;
			source[3] = loop;
			source[4] = loop;
			source[5] = loop;
			source[6] = loop;
			source[7] = loop;
			source++;
		}
	}
	return sum;
}

__attribute__((__noinline__)) static unsigned read_speed_test (volatile unsigned *apPtr, int aCount, const int aSize)
{
	while( aCount-- )
	{
	int i;
		for (i = 0; i < 8; i++) {
			volatile unsigned *source = apPtr;
			int index;
			for( index = aSize - 8; index; )
			{
				unsigned tmp;
				asm volatile(
					"	ldr     %[TMP], [%1, #0]\n"
					"	ldr     %[TMP], [%1, #4]\n"
					"	ldr     %[TMP], [%1, #8]\n"
					"	ldr     %[TMP], [%1, #12]\n"
					"	ldr     %[TMP], [%1, #16]\n"
					"	ldr     %[TMP], [%1, #20]\n"
					"	ldr     %[TMP], [%1, #24]\n"
					"	ldr     %[TMP], [%1, #28]\n"
					: [TMP]"=&r"(tmp), [M_PTR]"+r"(source)
					:// [M_PTR]"m"(*source)
					: "cc");
				source += 8;
				index -= 8;
			}
		}
	}
	return apPtr[0]+apPtr[1]+apPtr[2]+apPtr[3]+apPtr[4]+apPtr[5]+apPtr[6]+apPtr[7];
}

__attribute__((__noinline__)) static unsigned write_sdram_test32 (volatile unsigned *apPtr, const int aCount, const int aSize)
{
unsigned sum = 0;
int loop;
	for (loop = aCount; loop--; ) {
		volatile unsigned *source = apPtr;
		unsigned toassign = loop;
		int index;
		for (index = aSize-8; index >= 8; ) {
			source[0] = toassign; toassign <<= 1;
			source[1] = toassign; toassign <<= 1;
			source[2] = toassign; toassign <<= 1;
			source[3] = toassign; toassign <<= 1;
			source[4] = toassign; toassign ^= 0xa55aa55a;
			source[5] = toassign; toassign <<= 1;
			source[6] = toassign; toassign <<= 1;
			source[7] = toassign;
			source += 8;
			index -= 8;
			toassign ^= index;
		}
		source = apPtr;
		toassign = loop;
		for (index = aSize-8; index >= 8; ) {
			if (source[0] != toassign) sum++; toassign <<= 1;
			if (source[1] != toassign) sum++; toassign <<= 1;
			if (source[2] != toassign) sum++; toassign <<= 1;
			if (source[3] != toassign) sum++; toassign <<= 1;
			if (source[4] != toassign) sum++; toassign ^= 0xa55aa55a;
			if (source[5] != toassign) sum++; toassign <<= 1;
			if (source[6] != toassign) sum++; toassign <<= 1;
			if (source[7] != toassign) sum++;
			source += 8;
			index -= 8;
			toassign ^= index;
		}
	}
	return sum;
}

__attribute__((__noinline__)) static unsigned write_sdram_test16 (volatile unsigned short *apPtr, const int aCount, const int aSize)
{
	unsigned sum = 0;
	int loop;
	for (loop = aCount; loop--; ) {
		volatile unsigned short *source = apPtr;
		unsigned short toassign = loop;
		int index;
		for (index = aSize-8; index >= 8; ) {
			source[0] = toassign; toassign <<= 1;
			source[1] = toassign; toassign <<= 1;
			source[2] = toassign; toassign <<= 1;
			source[3] = toassign; toassign <<= 1;
			source[4] = toassign; toassign ^= 0xa55aa55a;
			source[5] = toassign; toassign <<= 1;
			source[6] = toassign; toassign <<= 1;
			source[7] = toassign;
			source += 8;
			index -= 8;
			toassign ^= index;
		}
		source = apPtr;
		toassign = loop;
		for (index = aSize-8; index >= 8; ) {
			if (source[0] != toassign) sum++; toassign <<= 1;
			if (source[1] != toassign) sum++; toassign <<= 1;
			if (source[2] != toassign) sum++; toassign <<= 1;
			if (source[3] != toassign) sum++; toassign <<= 1;
			if (source[4] != toassign) sum++; toassign ^= 0xa55aa55a;
			if (source[5] != toassign) sum++; toassign <<= 1;
			if (source[6] != toassign) sum++; toassign <<= 1;
			if (source[7] != toassign) sum++;
			source += 8;
			index -= 8;
			toassign ^= index;
		}
	}
	return sum;
}

__attribute__((__noinline__)) static unsigned write_sdram_test8 (volatile unsigned char *apPtr, const int aCount, const int aSize)
{
	unsigned sum = 0;
	int loop;
	for (loop = aCount; loop--; ) {
		volatile unsigned char *source = apPtr;
		unsigned char toassign = loop;
		int index;
		for (index = aSize-8; index >= 8; ) {
			source[0] = toassign; toassign <<= 1;
			source[1] = toassign; toassign <<= 1;
			source[2] = toassign; toassign <<= 1;
			source[3] = toassign; toassign <<= 1;
			source[4] = toassign; toassign ^= 0x5a;
			source[5] = toassign; toassign <<= 1;
			source[6] = toassign; toassign <<= 1;
			source[7] = toassign;
			source += 8;
			index -= 8;
			toassign ^= index;
		}
		source = apPtr;
		toassign = loop;
		for (index = aSize-8; index >= 8; ) {
			if (source[0] != toassign) sum++; toassign <<= 1;
			if (source[1] != toassign) sum++; toassign <<= 1;
			if (source[2] != toassign) sum++; toassign <<= 1;
			if (source[3] != toassign) sum++; toassign <<= 1;
			if (source[4] != toassign) sum++; toassign ^= 0x5a;
			if (source[5] != toassign) sum++; toassign <<= 1;
			if (source[6] != toassign) sum++; toassign <<= 1;
			if (source[7] != toassign) sum++;
			source += 8;
			index -= 8;
			toassign ^= index;
		}
	}
	return sum;
}

const volatile unsigned *flashMemory = (unsigned*)IFLASH_ADDR;

union xPointer {
	uint8_t *u8;
	uint16_t *u16;
	uint32_t *u32;
	uint32_t uint32;
};

void *fast_memcpy( void *pvDest, const void *pvSource, size_t ulBytes );
void *fast_memcpy( void *pvDest, const void *pvSource, size_t ulBytes )
{
union xPointer pxDestination;
union xPointer pxSource;
union xPointer pxLastSource;
uint32_t ulAlignBits;

	pxDestination.u8 = ( uint8_t * ) pvDest;
	pxSource.u8 = ( uint8_t * ) pvSource;
	pxLastSource.u8 = pxSource.u8 + ulBytes;

	ulAlignBits = ( pxDestination.uint32 & 0x03 ) ^ ( pxSource.uint32 & 0x03 );

	if( ( ulAlignBits & 0x01 ) == 0 )
	{
		if( ( ( pxSource.uint32 & 1 ) != 0 ) && ( pxSource.u8 < pxLastSource.u8 ) )
		{
			*( pxDestination.u8++ ) = *( pxSource.u8++) ;
		}
		/* 16-bit aligned here */
		if( ( ulAlignBits & 0x02 ) != 0 )
		{
			uint32_t extra = pxLastSource.uint32 & 0x01ul;

			pxLastSource.uint32 &= ~0x01ul;

			while( pxSource.u16 < pxLastSource.u16 )
			{
				*( pxDestination.u16++ ) = *( pxSource.u16++) ;
			}

			pxLastSource.uint32 |= extra;
		}
		else
		{
			int iCount;
			uint32_t extra;

			if( ( ( pxSource.uint32 & 2 ) != 0 ) && ( pxSource.u8 < pxLastSource.u8 - 1 ) )
			{
				*( pxDestination.u16++ ) = *( pxSource.u16++) ;
			}
			// 32-bit aligned
			extra = pxLastSource.uint32 & 0x03ul;

			pxLastSource.uint32 &= ~0x03ul;
			iCount = pxLastSource.u32 - pxSource.u32;
			while( iCount > 8 )
			{
				/* Copy 32 bytes */
				/* Normally it doesn't make sense to make this list much longer because
				the indexes will get too big, and therefore longer instructions are needed. */
				pxDestination.u32[ 0 ] = pxSource.u32[ 0 ];
				pxDestination.u32[ 1 ] = pxSource.u32[ 1 ];
				pxDestination.u32[ 2 ] = pxSource.u32[ 2 ];
				pxDestination.u32[ 3 ] = pxSource.u32[ 3 ];
				pxDestination.u32[ 4 ] = pxSource.u32[ 4 ];
				pxDestination.u32[ 5 ] = pxSource.u32[ 5 ];
				pxDestination.u32[ 6 ] = pxSource.u32[ 6 ];
				pxDestination.u32[ 7 ] = pxSource.u32[ 7 ];
				pxDestination.u32 += 8;
				pxSource.u32 += 8;
				iCount -= 8;
			}

			while( pxSource.u32 < pxLastSource.u32 )
			{
				*( pxDestination.u32++ ) = *( pxSource.u32++) ;
			}

			pxLastSource.uint32 |= extra;
		}
	}
	else
	{
		/* This it the worst alignment, e.g. 0x80000 and 0xA0001,
		only 8-bits copying is possible. */
		int iCount = pxLastSource.u8 - pxSource.u8;
		while( iCount > 8 )
		{
			/* Copy 8 bytes the hard way */
			pxDestination.u8[ 0 ] = pxSource.u8[ 0 ];
			pxDestination.u8[ 1 ] = pxSource.u8[ 1 ];
			pxDestination.u8[ 2 ] = pxSource.u8[ 2 ];
			pxDestination.u8[ 3 ] = pxSource.u8[ 3 ];
			pxDestination.u8[ 4 ] = pxSource.u8[ 4 ];
			pxDestination.u8[ 5 ] = pxSource.u8[ 5 ];
			pxDestination.u8[ 6 ] = pxSource.u8[ 6 ];
			pxDestination.u8[ 7 ] = pxSource.u8[ 7 ];
			pxDestination.u8 += 8;
			pxSource.u8 += 8;
			iCount -= 8;
		}
	}
	while( pxSource.u8 < pxLastSource.u8 )
	{
		*( pxDestination.u8++ ) = *( pxSource.u8++ );
	}
	return pvDest;
}

static uint64_t ullSysStart = 0ull;
static unsigned startSysCount()
{
	ullSysStart = ullGetHighResolutionTime();
}

static unsigned getSysCount()
{
	return ( uint32_t ) ( ullGetHighResolutionTime() - ullSysStart );
}

static char ioBuffer[2048];
static void memory_speed_test ()
{
	unsigned times[10];
	unsigned *sdramBuf = (unsigned*)pvPortMalloc (sizeof ioBuffer);
	const int mallocCount = sizeof ioBuffer / sizeof *sdramBuf;
	const int loopCount = 1000;
	const size_t memcpy_size = sizeof ioBuffer - 8;

	if (!sdramBuf) {
		FreeRTOS_printf( ( "memory_speed_test: pvPortMalloc fails\n" ) );
		return;
	}
	int byteCount = (mallocCount-8) * loopCount * 8;
	FreeRTOS_printf( ("memory_speed_test: %d words at %X and %X for %u bytes\n", mallocCount, (unsigned)ioBuffer, (unsigned)sdramBuf, byteCount ) );

	unsigned sum1, sum2, sum3, sum4, sum5;
	const int memcpy_count = 64;

	startSysCount();
	{
	int i;
	volatile int j;
		portDISABLE_INTERRUPTS();
/*
1000 * 512 
memory_speed_test: 512 words at 5020 and D000A078 for 4032000 bytes
Internal RAM: Wd 113060 (34842 KB/s) Rd 135643 (29041 KB/s) sum = 0
External RAM: Wd 236173 (16679 KB/s) Rd 549105 (7173 KB/s) sum = 0

SDRAM read:  7173*1024 = 7345152 
67737600 / 7345152 = 9.2

SDRAM write: 16679*1024 = 17079296
67737600 / 17079296 = 4.0

IRAM read:   34842*1024 = 35678208
67737600 / 35678208 = 1.9

IRAM write:  29041*1024 = 29737984
67737600 / 29737984 = 2.3

SDRAM errCount = 0, 0, 0
*/
		times[0] = getSysCount ();
		sum1 = write_speed_test ((volatile unsigned*)ioBuffer, loopCount, mallocCount);
		times[1] = getSysCount ();
		sum2 = write_speed_test ((volatile unsigned*)sdramBuf, loopCount, mallocCount);

		times[2] = getSysCount ();
		sum3 = read_speed_test ((volatile unsigned*)ioBuffer, loopCount, mallocCount);
		times[3] = getSysCount ();
		sum4 = read_speed_test ((volatile unsigned*)sdramBuf, loopCount, mallocCount);

		times[4] = getSysCount ();
		sum5 = read_speed_test ((volatile unsigned*)flashMemory, loopCount, mallocCount);

		times[5] = getSysCount ();

		for(j = memcpy_count; j--; ) {
			fast_memcpy( ioBuffer, ioBuffer+4, memcpy_size );
		}
		times[6] = getSysCount ();
		for(j = memcpy_count; j--; ) {
			fast_memcpy( sdramBuf, sdramBuf+4, memcpy_size );
		}
		times[7] = getSysCount ();
		for(j = memcpy_count; j--; ) {
			memcpy( ioBuffer, ioBuffer+4, memcpy_size );
		}
		times[8] = getSysCount ();
		for(j = memcpy_count; j--; ) {
			memcpy( sdramBuf, sdramBuf+4, memcpy_size );
		}
		times[9] = getSysCount ();

		portENABLE_INTERRUPTS();
	}
	const unsigned tpu = 1;
	unsigned speeds[5];
	speeds[0] = (1000 * byteCount)/((times[1] - times[0])/tpu);
	speeds[1] = (1000 * byteCount)/((times[3] - times[2])/tpu);
	speeds[2] = (1000 * byteCount)/((times[2] - times[1])/tpu);
	speeds[3] = (1000 * byteCount)/((times[4] - times[3])/tpu);
	speeds[4] = (1000 * byteCount)/((times[5] - times[4])/tpu);

	FreeRTOS_printf( ("Internal FLS: Rd %u uS (%5u KB/s) sum = %X\n",                    (times[5] - times[4])/tpu, speeds[4], sum5 ) );
	FreeRTOS_printf( ("Internal RAM: Rd %u uS (%5u KB/s) Wd %u uS (%5u KB/s) sum = %X/%X\n", (times[3] - times[2])/tpu, speeds[1], (times[1] - times[0])/tpu, speeds[0], sum1, sum2 ) );
	FreeRTOS_printf( ("External RAM: Rd %u uS (%5u KB/s) Wd %u uS (%5u KB/s) sum = %X/%X\n", (times[4] - times[3])/tpu, speeds[3], (times[2] - times[1])/tpu, speeds[2], sum3, sum4 ) );
	unsigned errCount32 = write_sdram_test32 ((volatile unsigned *)sdramBuf, loopCount, mallocCount);
	unsigned errCount16 = write_sdram_test16 ((volatile unsigned short *)sdramBuf, loopCount, mallocCount*2);
	unsigned errCount8 = write_sdram_test8 ((volatile unsigned char *)sdramBuf, loopCount, mallocCount*4);
	FreeRTOS_printf( ("SDRAM errCount = %u, %u, %u\n", errCount32, errCount16, errCount8 ) );

	{
		unsigned delta, speed;
		const unsigned count = memcpy_count * memcpy_size;
		int i;

		const char *names[4] = {
			"fast_cpy  IRAM",
			"fast_cpy SDRAM",
			"norm_cpy  IRAM",
			"norm_cpy SDRAM" };
		for (i = 0; i < 4; i++) {

			delta = times[i + 6] - times[i + 5];
			speed = ( 1000 * count ) / ( delta / tpu );

			FreeRTOS_printf( ("%s: %u (%u * %u = %u) speed %5u\n",
				names[i], delta, memcpy_count, mallocCount, count, speed ) );
		}
	}
	vPortFree (sdramBuf);
}
#endif // TST_MEM_SPEED_TEST

int strnicmp( const char *pcLeft, const char *pcRight, int iCount )
{
	return strncasecmp( pcLeft, pcRight, iCount );
}

#if( USE_FREERTOS_FAT != 0 )
// 96 bytes
static const char contents[] = "12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345";

void file_create_test ( void )
{
int i;
TickType_t t1, t2;
TickType_t diff;


//	t1 = xTaskGetTickCount();
//	ff_deltree( "/filetest" );
//	diff = xTaskGetTickCount() - t1;
//	FreeRTOS_printf( ( "deltree /filetest: %lu ms\n", diff ) );


	ff_mkdir( "/filetest" );

	for( i = 1; i <= 250; i++ )
	{
	char fname[ 80 ];

		t1 = xTaskGetTickCount();
	
		snprintf( fname, sizeof fname, "/filetest/file%04d.txt", i );
		FF_FILE *fp = ff_fopen( fname, "a" );
		if (fp == NULL )
		{
			FreeRTOS_printf( ( "fopen %s failed: %5d\n", fname, stdioGET_ERRNO() ) );
			break;
		}
		t2 = xTaskGetTickCount();
		ff_fwrite( contents, 1, sizeof contents, fp );
		ff_fclose (fp );
		diff = xTaskGetTickCount() - t2;
		t2 -= t1;
		FreeRTOS_printf( ( "%d %lu %lu\n", i, t2, diff ) );
	}
}
#endif /* ( USE_FREERTOS_FAT != 0 ) */
 
#if( USE_USB_CDC != 0 )

	bool main_cdc_enable(int port)
	{
		main_b_cdc_enable = true;
		// Open communication
		//uart_open(port);
		return true;
	}

	void main_cdc_disable(int port)
	{
		main_b_cdc_enable = false;
		// Close communication
		//uart_close(port);
	}

	void main_cdc_set_dtr(int port, bool b_enable)
	{
		if (b_enable) {
			// Host terminal has open COM
			//ui_com_open(port);
		}else{
			// Host terminal has close COM
			//ui_com_close(port);
		}
	}

	void main_suspend_action(void)
	{
		//ui_powerdown();
	}

	void main_resume_action(void)
	{
		//ui_wakeup();
	}

	void main_sof_action(void)
	{
		if (!main_b_cdc_enable)
			return;
		//ui_process(udd_get_frame_number());
	}
	void set_coding(uint8_t port, usb_cdc_line_coding_t * cfg)
	{
	}
#endif

/* HT Notify the owner of this stream */
void cdc_data_received(int iCount)
{
BaseType_t xHigherPriorityTaskWoken = pdFALSE;

	if( xServerSemaphore != NULL )
	{
		xSemaphoreGiveFromISR(xServerSemaphore, &xHigherPriorityTaskWoken );
		if( xHigherPriorityTaskWoken != pdFALSE )
		{
			portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
		}
	}
}

uint32_t ulApplicationGetNextSequenceNumber( uint32_t ulSourceAddress,
													uint16_t usSourcePort,
													uint32_t ulDestinationAddress,
													uint16_t usDestinationPort )
{
	return uxRand();
}

/*
void _init ()
{
}
*/

/*
extern int32_t prvFFWrite( uint8_t *pucBuffer, uint32_t ulSectorNumber, uint32_t ulSectorCount, FF_Disk_t *pxDisk );
extern int32_t prvFFRead( uint8_t *pucBuffer, uint32_t ulSectorNumber, uint32_t ulSectorCount, FF_Disk_t *pxDisk );
extern SemaphoreHandle_t xPlusFATMutex;

static FF_IOManager_t * CreateIoManager(FF_Disk_t * p_disk)
{
FF_CreationParameters_t xParameters;

	memset( &xParameters, '\0', sizeof( xParameters ) );
	xParameters.pxDisk = p_disk;
	xParameters.pucCacheMemory = NULL;
	xParameters.ulMemorySize = 3072;	// 1024;
	xParameters.ulSectorSize = 512;
	xParameters.fnWriteBlocks = prvFFWrite;
	xParameters.fnReadBlocks = prvFFRead;
	xParameters.xBlockDeviceIsReentrant = pdFALSE;
	if( xPlusFATMutex == NULL )
	{
		xPlusFATMutex = xSemaphoreCreateRecursiveMutex();
	}

	configASSERT( xPlusFATMutex != NULL );

	xParameters.pvSemaphore = ( void * ) xPlusFATMutex;

	FF_Error_t err;
	FF_IOManager_t * p_ioman = FF_CreateIOManger( &xParameters, &err );

	if( FF_isERR( err ) )
	{
		FreeRTOS_printf( ( "FF_CreateIOManger: 0x%08lX\n", err ) );

		if( p_ioman != NULL )
		{
			ffconfigFREE( p_ioman );
		}
	    return NULL;
	}
	return p_ioman;
}


// The signature of a FF_Disk_t.
#define sdSIGNATURE 			0x41404342UL

// Using pointers to FF_Disk_t
static FF_Disk_t *pxDisks[ffconfigMAX_PARTITIONS];
static FF_Disk_t xSecondDisk;

int FF_SDDiskTwoPartitionFormat(void)
{
int iPercentagePerPartition;
uint32_t ulNumberOfSectors;

	//
	// pxSDDisk is a pointer to a FF_Disk_t, from the demo code.
	// It is re-used here for formatting/mounting the first partition.
	//

	if( pxSDDisk != NULL )
	{
		ulNumberOfSectors = pxSDDisk->ulNumberOfSectors;
	}
	else
	{
		ulNumberOfSectors = 0;
	}

	if( ulNumberOfSectors == 0ul )
	{
		FreeRTOS_printf( ( "Disk was not yet mounted ?\n" ) );
		return 0;
	}
	pxDisks[ 0 ] = pxSDDisk;
	pxDisks[ 1 ] = &( xSecondDisk );

	for (size_t i = 0; i < ffconfigMAX_PARTITIONS; i++)
	{
		// Check if an I/O handler has already been created.
		if( pxDisks[ i ]->pxIOManager == NULL )
		{
			pxDisks[ i ]->ulNumberOfSectors = ulNumberOfSectors;
			pxDisks[ i ]->pxIOManager = CreateIoManager( pxDisks[ i ] );
			pxDisks[ i ]->xStatus.bIsInitialised = pdTRUE;
			pxDisks[ i ]->xStatus.bPartitionNumber = i;
			pxDisks[ i ]->ulSignature = sdSIGNATURE;
			if( pxDisks[ i ]->pxIOManager == NULL )
			{
				FreeRTOS_printf( ( "Disk %d has no manager\n", i ) );
				return -1;
			}
		}
	}

	FF_PartitionParameters_t partitions;
	memset( &partitions, 0x00, sizeof( partitions ) );
	partitions.ulSectorCount = ulNumberOfSectors;
	// ulHiddenSectors is the distance between the start of the partition and
	// the BPR ( Partition Boot Record ).
	// It may not be zero.
	partitions.ulHiddenSectors = 8;
	partitions.ulInterSpace = 0;
	partitions.xPrimaryCount = 2;	// That is correct
	partitions.eSizeType = eSizeIsPercent;

	iPercentagePerPartition = 100 / ffconfigMAX_PARTITIONS;
	for (size_t i = 0; i < ffconfigMAX_PARTITIONS; i++)
	{
		partitions.xSizes[ i ] = iPercentagePerPartition;
	}

	FF_Error_t partition_err = FF_Partition( pxDisks[ 0 ], &partitions);
	FF_FlushCache( pxDisks[ 0 ]->pxIOManager );

	FreeRTOS_printf( ( "FF_Partition: 0x%08lX Number of sectors %lu\n", partition_err, ulNumberOfSectors ) );

	if( FF_isERR( partition_err ) )
	{
		return -1;
	}

	for (size_t i = 0; i < ffconfigMAX_PARTITIONS; i++)
	{
		if( pxDisks[ i ]->xStatus.bIsMounted )
		{
			FF_Unmount( pxDisks[ i ] );
		}
	    FF_Error_t format_err = FF_Format( pxDisks[ i ], i, pdFALSE, pdFALSE);
		FF_FlushCache( pxDisks[ i ]->pxIOManager );
		FreeRTOS_printf( ( "FF_Format[ %lu ]: 0x%08lX\n", i, format_err ) );
	    if( FF_isERR( format_err ) )
		{
			return -1;
		}
	}
	for (size_t i = 0; i < ffconfigMAX_PARTITIONS; i++)
	{
	char pcName[12];

		FF_Error_t mount_err = FF_Mount( pxDisks[ i ], pxDisks[ i ]->xStatus.bPartitionNumber );
		FreeRTOS_printf( ( "FF_Mount[ %lu ]: 0x%08lX\n", i, mount_err) );
		if( FF_isERR( mount_err ) )
		{
			return -1;
		}
		{
			if( i == 0 )
			{
				// Has probably been added already.
				snprintf( pcName, sizeof pcName, "/" );
			}
			else
			{
				snprintf( pcName, sizeof pcName, "/part%d", i );
			}
			FF_FS_Add( pcName, pxDisks[ i ] );
		}
	}
	return 0;
}
*/
