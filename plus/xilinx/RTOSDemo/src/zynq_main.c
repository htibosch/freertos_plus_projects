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

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_tcp_server.h"
#include "FreeRTOS_DHCP.h"
#include "NetworkBufferManagement.h"
#include "NetworkInterface.h"
#if( ipconfigMULTI_INTERFACE != 0 )
	#include "FreeRTOS_Routing.h"
#endif

#include "iperf_task.h"

/* FreeRTOS+FAT includes. */
#include "ff_headers.h"
#include "ff_stdio.h"
#include "ff_ramdisk.h"
#include "ff_sddisk.h"

/* Demo application includes. */
#include "hr_gettime.h"
#include "partest.h"
#include "UDPLoggingPrintf.h"


#include "eventLogging.h"

/* Xilinx includes. */
#include "xil_cache.h"
#include "xil_misc_psreset_api.h"
#include "xqspips_hw.h"
#include "xuartps.h"

#include "NTPDemo.h"
#include "date_and_time.h"

#if( USE_TAMAS != 0 )
	#include "tamas_main.h"
#endif

#if( USE_IP_DROP_SELECTIVE_PORT != 0 )
	void showDrop( void );
	void setDrop(int aRxDrop, int aTxDrop);
#endif

#if( USE_TCP_TESTER != 0 )
	#include "tcp_tester.h"
#endif

/* FTP and HTTP servers execute in the TCP server work task. */
#define mainTCP_SERVER_TASK_PRIORITY	( tskIDLE_PRIORITY + 4 )
#define	mainTCP_SERVER_STACK_SIZE		2048

#define	mainFAT_TEST_STACK_SIZE			2048

/* The number and size of sectors that will make up the RAM disk. */
#define mainRAM_DISK_SECTOR_SIZE		512UL /* Currently fixed! */
#define mainRAM_DISK_SECTORS			( ( 45UL * 1024UL * 1024UL ) / mainRAM_DISK_SECTOR_SIZE )
#define mainIO_MANAGER_CACHE_SIZE		( 15UL * mainRAM_DISK_SECTOR_SIZE )

/* Where the SD and RAM disks are mounted.  As set here the SD card disk is the
root directory, and the RAM disk appears as a directory off the root. */
#define mainSD_CARD_DISK_NAME			"/"
#define mainRAM_DISK_NAME				"/ram"

#define mainHAS_RAMDISK					1
#define mainHAS_SDCARD					1

#define mainHAS_FAT_TEST				1

/* Set the following constants to 1 to include the relevant server, or 0 to
exclude the relevant server. */
#define mainCREATE_FTP_SERVER			1
#define mainCREATE_HTTP_SERVER 			1

#define NTP_STACK_SIZE					256
#define NTP_PRIORITY					( tskIDLE_PRIORITY + 3 )

/* The LED to toggle and the rate at which the LED will be toggled. */
#define mainLED_TOGGLE_PERIOD			pdMS_TO_TICKS( 250 )
#define mainLED							0

/* Define names that will be used for DNS, LLMNR and NBNS searches. */
#define mainHOST_NAME					"RTOSDemo"
#define mainDEVICE_NICK_NAME			"zynq"

#ifndef	ARRAY_SIZE
	#define	ARRAY_SIZE( x )	( int )( sizeof( x ) / sizeof( x )[ 0 ] )
#endif
/*-----------------------------------------------------------*/

#if( ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM != 1 )
	#warning ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM must be '1'
#endif

#if( ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM != 1 )
	#warning ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM must be '1'
#endif

/*
 * Just seeds the simple pseudo random number generator.
 */
static void prvSRand( UBaseType_t ulSeed );

/*
 * Miscellaneous initialisation including preparing the logging and seeding the
 * random number generator.
 */
static void prvMiscInitialisation( void );

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
#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
	static void prvServerWorkTask( void *pvParameters );
#endif

/*
 * Callback function for the timer that toggles an LED.
 */
#if configUSE_TIMERS != 0
	static void prvLEDTimerCallback( TimerHandle_t xTimer );
#endif

#if( ipconfigUSE_WOLFSSL != 0 )
	extern int vStartWolfServerTask( void );
#endif
/*
 * The Xilinx projects use a BSP that do not allow the start up code to be
 * altered easily.  Therefore the vector table used by FreeRTOS is defined in
 * FreeRTOS_asm_vectors.S, which is part of this project.  Switch to use the
 * FreeRTOS interrupt vector table.
 */
extern void vPortInstallFreeRTOSVectorTable( void );

void sendICMP( void );

void vShowTaskTable( BaseType_t aDoClear );

/* The default IP and MAC address used by the demo.  The address configuration
defined here will be used if ipconfigUSE_DHCP is 0, or if ipconfigUSE_DHCP is
1 but a DHCP server could not be contacted.  See the online documentation for
more information. */
static const uint8_t ucIPAddress[ 4 ] = { configIP_ADDR0, configIP_ADDR1, configIP_ADDR2, configIP_ADDR3 };
static const uint8_t ucNetMask[ 4 ] = { configNET_MASK0, configNET_MASK1, configNET_MASK2, configNET_MASK3 };
static const uint8_t ucGatewayAddress[ 4 ] = { configGATEWAY_ADDR0, configGATEWAY_ADDR1, configGATEWAY_ADDR2, configGATEWAY_ADDR3 };
static const uint8_t ucDNSServerAddress[ 4 ] = { configDNS_SERVER_ADDR0, configDNS_SERVER_ADDR1, configDNS_SERVER_ADDR2, configDNS_SERVER_ADDR3 };

static const uint8_t ucIPAddress2[ 4 ] = { 172, 16, 0, 100 };
static const uint8_t ucNetMask2[ 4 ] = { 255, 255, 0, 0 };
static const uint8_t ucGatewayAddress2[ 4 ] = { 0, 0, 0, 0};
static const uint8_t ucDNSServerAddress2[ 4 ] = { 0, 0, 0, 0 };

/* Default MAC address configuration.  The demo creates a virtual network
connection that uses this MAC address by accessing the raw Ethernet data
to and from a real network connection on the host PC.  See the
configNETWORK_INTERFACE_TO_USE definition for information on how to configure
the real network connection to use. */
#if( ipconfigMULTI_INTERFACE == 0 )
	const uint8_t ucMACAddress[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };
#else
	static const uint8_t ucMACAddress[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };
	static const uint8_t ucMACAddress2[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 + 1 };
#endif /* ipconfigMULTI_INTERFACE */

/* Use by the pseudo random number generator. */
static UBaseType_t ulNextRand;

/* Handle of the task that runs the FTP and HTTP servers. */
static TaskHandle_t xServerWorkTaskHandle = NULL;

/* FreeRTOS+FAT disks for the SD card and RAM disk. */
static FF_Disk_t *pxSDDisk;
FF_Disk_t *pxRAMDisk;

int verboseLevel = 0;

extern int use_event_logging;

#if( ipconfigMULTI_INTERFACE != 0 )
	NetworkInterface_t *pxZynq_FillInterfaceDescriptor( BaseType_t xEMACIndex, NetworkInterface_t *pxInterface );
	NetworkInterface_t *pxSAM4e_GMAC__FillInterfaceDescriptor( BaseType_t xEMACIndex, NetworkInterface_t *pxInterface );
#endif /* ipconfigMULTI_INTERFACE */
/*-----------------------------------------------------------*/

int main( void )
{
#if( ipconfigMULTI_INTERFACE != 0 )
	static NetworkInterface_t xInterfaces[ 2 ];
	static NetworkEndPoint_t xEndPoints[ 2 ];
#endif /* ipconfigMULTI_INTERFACE */

#if configUSE_TIMERS != 0
	TimerHandle_t xLEDTimer;
#endif
	/* Miscellaneous initialisation including preparing the logging and seeding
	the random number generator. */
	prvMiscInitialisation();

	/* Initialise the network interface.

	***NOTE*** Tasks that use the network are created in the network event hook
	when the network is connected and ready for use (see the definition of
	vApplicationIPNetworkEventHook() below).  The address values passed in here
	are used if ipconfigUSE_DHCP is set to 0, or if ipconfigUSE_DHCP is set to 1
	but a DHCP server cannot be	contacted. */

#if( ipconfigMULTI_INTERFACE == 0 )
	FreeRTOS_IPInit( ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress );
#else
	pxZynq_FillInterfaceDescriptor( 0, &( xInterfaces[ 0 ] ) );
	FreeRTOS_FillEndPoint( &( xEndPoints[ 0 ] ), ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress );
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
	FreeRTOS_AddEndPoint( &( xInterfaces[ 0 ] ), &( xEndPoints[ 0 ] ) );

//	pxZynq_FillInterfaceDescriptor( 1, &( xInterfaces[ 1 ] ) );
//	FreeRTOS_FillEndPoint( &( xEndPoints[ 1 ] ), ucIPAddress2, ucNetMask2, ucGatewayAddress2, ucDNSServerAddress2, ucMACAddress2 );
//	FreeRTOS_AddEndPoint( &( xInterfaces[ 1 ] ), &( xEndPoints[ 1 ] ) );

	/* You can modify fields: */
	xEndPoints[ 0 ].bits.bIsDefault = pdTRUE_UNSIGNED;
	xEndPoints[ 0 ].bits.bWantDHCP = pdTRUE_UNSIGNED;

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

	/* Create a timer that just toggles an LED to show something is alive. */
	#if configUSE_TIMERS != 0
	{
		xLEDTimer = xTimerCreate( "LED", mainLED_TOGGLE_PERIOD, pdTRUE, NULL, prvLEDTimerCallback );
		configASSERT( xLEDTimer );
		xTimerStart( xLEDTimer, 0 );
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
		if( pxSDDisk != NULL )
		{
			FF_IOManager_t *pxIOManager = sddisk_ioman( pxSDDisk );
			uint64_t ullFreeBytes =
				( uint64_t ) pxIOManager->xPartition.ulFreeClusterCount * pxIOManager->xPartition.ulSectorsPerCluster * 512ull;
			FreeRTOS_printf( ( "Volume %-12.12s: Free clusters %lu total clusters %lu\n",
				pxIOManager->xPartition.pcVolumeLabel,
				pxIOManager->xPartition.ulFreeClusterCount,
				pxIOManager->xPartition.ulNumClusters,
				( uint32_t ) ( ullFreeBytes / 1024ull ) ) );
		}
		FreeRTOS_printf( ( "Mount SD-card done\n" ) );
	}
	#endif	/* mainHAS_SDCARD */

//	/* Create a few example files on the disk.  These are not deleted again. */
//	vCreateAndVerifyExampleFiles( mainRAM_DISK_NAME );
//	vStdioWithCWDTest( mainRAM_DISK_NAME );
}
/*-----------------------------------------------------------*/

//#if defined(CONFIG_USE_LWIP)
//	#undef CONFIG_USE_LWIP
//	#define CONFIG_USE_LWIP    0
//#endif

#if( CONFIG_USE_LWIP != 0 )

	#include "lwipopts.h"
	#include "xlwipconfig.h"

	#if !NO_SYS
	#ifdef OS_IS_XILKERNEL
	#include "xmk.h"
	#include "sys/process.h"
	#endif
	#endif

	#include "lwip/mem.h"
	#include "lwip/stats.h"
	#include "lwip/sys.h"
	#include "lwip/ip.h"
	#include "lwip/tcp.h"
	#include "lwip/udp.h"
	#include "lwip/tcp_impl.h"

	#include "netif/etharp.h"
	#include "netif/xadapter.h"

	#ifdef XLWIP_CONFIG_INCLUDE_EMACLITE
	#include "netif/xemacliteif.h"
	#endif

	#ifdef XLWIP_CONFIG_INCLUDE_TEMAC
	#include "netif/xlltemacif.h"
	#endif

	#ifdef XLWIP_CONFIG_INCLUDE_AXI_ETHERNET
	#include "netif/xaxiemacif.h"
	#endif

	#ifdef XLWIP_CONFIG_INCLUDE_GEM
	#include "netif/xemacpsif.h"
	#endif

	#if !NO_SYS
	#include "lwip/tcpip.h"
	#endif

	#include "lwip_echo_server.h"
	#include "plus_echo_server.h"

	void lwip_init(void);

#endif	/* CONFIG_USE_LWIP */

#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )

#define STACK_WEBSERVER_TASK  ( 512 + 256 )
#define	PRIO_WEBSERVER_TASK     2

	void FreeRTOS_netstat( void );

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

	#if( CONFIG_USE_LWIP != 0 )

	unsigned char ucLWIP_Mac_Address[] = { 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

	static err_t xemacpsif_output(struct netif *netif, struct pbuf *p,
			struct ip_addr *ipaddr)
	{
		/* resolve hardware address, then send (or queue) packet */
		return etharp_output(netif, p, ipaddr);
	}

	static err_t low_level_output(struct netif *netif, struct pbuf *p)
	{
	NetworkBufferDescriptor_t *pxBuffer;
	const unsigned char *pcPacket = (const unsigned char*)p->payload;

		pxBuffer = pxGetNetworkBufferWithDescriptor( p->len, 10 );
		if( pxBuffer != NULL )
		{
			memcpy( pxBuffer->pucEthernetBuffer, p->payload, p->len );
			xNetworkInterfaceOutput( pxBuffer, pdTRUE );
		}
		if( ( pxBuffer == NULL ) || ( verboseLevel > 0 ) )
		{
			lUDPLoggingPrintf( "xemacpsif_output: %02x:%02x:%02x => %02x:%02x:%02x %s\n",
				pcPacket[ 9 ],
				pcPacket[ 10 ],
				pcPacket[ 11 ],
				pcPacket[ 3 ],
				pcPacket[ 4 ],
				pcPacket[ 5 ],
				pxBuffer ? "OK" : "Error" );
		}
		return ERR_OK;
	}

	err_t xemacpsif_init(struct netif *netif)
	{
/* Define those to better describe your network interface. */
#define IFNAME0 't'
#define IFNAME1 'e'
		netif->name[0] = IFNAME0;
		netif->name[1] = IFNAME1;
		netif->output = xemacpsif_output;
		netif->linkoutput = low_level_output;

		return ERR_OK;
	}

	struct netif server_netif;
	extern volatile struct netif *curr_netif;
	static void vLWIP_Init( void )
	{
	struct netif *netif = &server_netif;
	struct ip_addr ipaddr, netmask, gw;
	err_t rc;

		memset( &server_netif, '\0', sizeof server_netif );
		lwip_raw_init();
		lwip_init();

		/* initliaze IP addresses to be used */
		IP4_ADDR(&ipaddr,  192, 168,   2, 10);
		IP4_ADDR(&netmask, 255, 255, 255,  0);
		IP4_ADDR(&gw,      192, 168,   2,  1);

		/*
		 * struct netif *netif_add(struct netif *netif, ip_addr_t *ipaddr, ip_addr_t *netmask,
		 * ip_addr_t *gw, void *state, netif_init_fn init, netif_input_fn input);
		 */

		/* Add network interface to the netif_list, and set it as default */
		struct netif *rc_netif = netif_add( netif, &ipaddr, &netmask, &gw,
							( void * )ucLWIP_Mac_Address,
							xemacpsif_init, tcpip_input );

		if( rc_netif == NULL )
		{
			FreeRTOS_printf( ("Error adding N/W interface\r\n" ) );
		}
		else
		{
			netif_set_default(netif);

			/* specify that the network if is up */
			netif_set_up(netif);
			netif->flags |= NETIF_FLAG_ETHERNET | NETIF_FLAG_ETHARP | NETIF_FLAG_UP | NETIF_FLAG_BROADCAST;
			memset( netif->hwaddr, '\0', sizeof netif->hwaddr );
			memcpy( netif->hwaddr, ucLWIP_Mac_Address, 6 );
			netif->hwaddr_len = ETHARP_HWADDR_LEN;

			curr_netif = netif;

			xTaskCreate (lwip_echo_application_thread, "lwip_web", STACK_WEBSERVER_TASK, NULL, PRIO_WEBSERVER_TASK, NULL);
			xTaskCreate (plus_echo_application_thread, "plus_web", STACK_WEBSERVER_TASK, NULL, PRIO_WEBSERVER_TASK, NULL);
		}

	}
	#endif	/* CONFIG_USE_LWIP */

void big_nuber_test( void );
TaskSwitch_t xTaskSwitches[ 10 ];
TaskSwitch_t xTaskSwitchCopy[ 10 ];

void vShowSwitchCounters( void );
void vClearSwitchCounters( void );
void vRegisterSwitchCounters( void );

void vShowSwitchCounters()
{
int i;
uint32_t ulSwitchCount = 0ul;

	memcpy( xTaskSwitchCopy, xTaskSwitches, sizeof( xTaskSwitchCopy ) );
	for( i = 0; i < ARRAY_SIZE( xTaskSwitchCopy ); i++ )
	{
		if( xTaskSwitchCopy[ i ].ulSwitchCount != 0 )
		{
			ulSwitchCount += xTaskSwitchCopy[ i ].ulSwitchCount;
			FreeRTOS_printf( ( "%d: %5u (%s)\n", i, xTaskSwitchCopy[ i ].ulSwitchCount, xTaskSwitchCopy[ i ].pcTaskName ) );
		}
	}
	FreeRTOS_printf( ( "vShowSwitchCounters: %u\n", ulSwitchCount ) );
	vClearSwitchCounters();
}

void vClearSwitchCounters( void )
{
int i;

	for( i = 0; i < ARRAY_SIZE( xTaskSwitches ); i++ )
	{
		xTaskSwitches[ i ].ulSwitchCount = 0;
	}
}

extern void plus_echo_client_thread( void *parameters );

	static void prvServerWorkTask( void *pvParameters )
	{
	TCPServer_t *pxTCPServer = NULL;
	const TickType_t xInitialBlockTime = pdMS_TO_TICKS( 200UL );

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

		#if( configUSE_TASK_FPU_SUPPORT == 1 )
		{
			portTASK_USES_FLOATING_POINT();
		}
		#endif /* configUSE_TASK_FPU_SUPPORT == 1 */

		FreeRTOS_printf( ( "Creating files\n" ) );

		/* Create the RAM disk used by the FTP and HTTP servers. */
		prvCreateDiskAndExampleFiles();

		#if( ipconfigUSE_WOLFSSL != 0 )
		{
			if( pxSDDisk != NULL )
			{
				FreeRTOS_printf( ( "Calling vStartWolfServerTask\n" ) );
				vStartWolfServerTask();
			}
		}
		#endif /* ipconfigUSE_WOLFSSL */
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

		vParTestInitialise( );

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
		#if( USE_TCP_TESTER != 0 )
		{
			tcp_test_start();
		}
		#endif

		//xTaskCreate (plus_echo_client_thread, "echo_client", STACK_WEBSERVER_TASK, NULL, PRIO_WEBSERVER_TASK, NULL);

		for( ;; )
		{
			FreeRTOS_TCPServerWork( pxTCPServer, xInitialBlockTime );
			#if( configUSE_TIMERS == 0 )
			{
				/* As there is not Timer task, toggle the LED 'manually'. */
				vParTestToggleLED( mainLED );
			}
			#endif
			xSocket_t xSocket = xLoggingGetSocket();
			struct freertos_sockaddr xSourceAddress;
			socklen_t xSourceAddressLength = sizeof( xSourceAddress );

			#if( CONFIG_USE_LWIP != 0 )
			{
				curr_netif = &( server_netif );
			}
			#endif

			if( xSocket != NULL)
			{
			char cBuffer[ 64 ];
			BaseType_t xCount;

				xCount = FreeRTOS_recvfrom( xSocket, ( void * )cBuffer, sizeof( cBuffer ) - 1, FREERTOS_MSG_DONTWAIT,
					&xSourceAddress, &xSourceAddressLength );
				if( xCount > 0 )
				{
					cBuffer[ xCount ] = '\0';
					FreeRTOS_printf( ( ">> %s\n", cBuffer ) );
					if( strncmp( cBuffer, "ver", 3 ) == 0 ) {
					int level;
					extern unsigned link_speed;

						if( sscanf( cBuffer + 3, "%d", &level ) == 1 ) {
							verboseLevel = level;
						}
						FreeRTOS_printf( ( "Verbose level %d link_speed %u\n", verboseLevel, link_speed ) );
#if( USE_IP_DROP_SELECTIVE_PORT != 0 ) && ( ipconfigMULTI_INTERFACE != 0 )
					} else if (strnicmp (cBuffer, "droprx", 6) == 0) {
						setDrop( 21, 0 );
					} else if (strnicmp (cBuffer, "droptx", 6) == 0) {
						setDrop( 0, 21 );
					} else if (strnicmp (cBuffer, "droptx", 4) == 0) {
						setDrop( 0, 0 );
#endif
//					} else if( strncmp( cBuffer, "iperf", 5 ) == 0 ) {
//						vIPerfInstall();
					} else if( strncmp( cBuffer, "pull", 4 ) == 0 ) {
#define	SD_DAT0		42
#define	SD_DAT1		43
#define	SD_DAT2		44
#define	SD_DAT3		45
						uint32_t ulValue, ulOldValues[4];
						int index, IsLock;
						extern uint32_t ulSDWritePause;
						for( index= 0; index < 4; index++ )
						{
							ulOldValues[index] = Xil_In32( ( XSLCR_MIO_PIN_00_ADDR + ( ( SD_DAT0 + index ) * 4U ) ) );
						}
						if (cBuffer[4] == '1')
						{
							ulValue = 0x1280;
						}
						else
						{
							ulValue = 0x0280;
						}
						ulSDWritePause = 0;

						IsLock = XQspiPs_In32( XPAR_XSLCR_0_BASEADDR + SLCR_LOCKSTA );
						if (IsLock) {
							XQspiPs_Out32( XPAR_XSLCR_0_BASEADDR + SLCR_UNLOCK, SLCR_UNLOCK_MASK );
						}
						Xil_Out32( ( XSLCR_MIO_PIN_00_ADDR + ( SD_DAT0 * 4U ) ), ulValue );

						if (IsLock) {
							XQspiPs_Out32( XPAR_XSLCR_0_BASEADDR + SLCR_LOCK, SLCR_LOCK_MASK);
						}

						// MIO config DATA0 0280 -> 0680
						FreeRTOS_printf( ( "MIO config DATA0 [ %04X %04X %04X %04X ] -> %04X\n",
							ulOldValues[ 0 ],
							ulOldValues[ 1 ],
							ulOldValues[ 2 ],
							ulOldValues[ 3 ], ulValue ) );
					} else if( strncmp( cBuffer, "switch", 6 ) == 0 ) {
						vShowSwitchCounters();
#if( USE_TAMAS != 0 )
					} else if( memcmp( cBuffer, "tamas", 5 ) == 0 ) {
					static int started = pdFALSE;
					
						if( started == pdFALSE ) {
							started = pdTRUE;
							tamas_start();
							use_event_logging = 1;
						}
						if( cBuffer[ 5 ] != '\0' ) {
							console_key = cBuffer[ 5 ];
						}
#endif
					} else if( memcmp( cBuffer, "mem", 3 ) == 0 ) {
						uint32_t now = xPortGetFreeHeapSize( );
						uint32_t total = 0;//xPortGetOrigHeapSize( );
						uint32_t perc = total ? ( ( 100 * now ) / total ) : 100;
						lUDPLoggingPrintf("mem Low %u, Current %lu / %lu (%lu perc free)\n",
							xPortGetMinimumEverFreeHeapSize( ),
							now, total, perc );
					} else if( strncmp( cBuffer, "ntp", 3 ) == 0 ) {
						iTimeZone = -480 * 60;	// Timezone in seconds
						vStartNTPTask( NTP_STACK_SIZE, NTP_PRIORITY );
					} if( strncmp( cBuffer, "netstat", 7 ) == 0 ) {
//					uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
//						FreeRTOS_GetAddressConfiguration( &ulIPAddress, &ulNetMask, &ulGatewayAddress, &ulDNSServerAddress );
//						lUDPLoggingPrintf("IP %xip  Mask %xip  GW %xip  DNS %xip\n",
//							FreeRTOS_htonl( ulIPAddress ),
//							FreeRTOS_htonl( ulNetMask ),
//							FreeRTOS_htonl( ulGatewayAddress ),
//							FreeRTOS_htonl( ulDNSServerAddress ) );
						FreeRTOS_netstat();
					#if( USE_LOG_EVENT != 0 )
					} else if( strncmp( cBuffer, "event", 4 ) == 0 ) {
						eventLogDump();
					#endif /* USE_LOG_EVENT */
					} else if( strncmp( cBuffer, "list", 4 ) == 0 ) {
						vShowTaskTable( cBuffer[ 4 ] == 'c' );
					} else if( strncmp( cBuffer, "format", 7 ) == 0 ) {
						if( pxSDDisk != 0 )
						{
							FF_SDDiskFormat( pxSDDisk, 0 );
						}
					#if( mainHAS_FAT_TEST != 0 )
					} else if( strncmp( cBuffer, "fattest", 7 ) == 0 ) {
						int level = 0;
//						const char pcMountPath[] = "/test";
						const char pcMountPath[] = "/ram";

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
				#endif /* mainHAS_FAT_TEST */
				} /* if( xCount > 0 ) */
			}	/* if( xSocket != NULL) */
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

void FreeRTOS_Undefined_Handler()
{
	configASSERT( 0 );
}


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
			/* See the comments above the definitions of these pre-processor
			macros at the top of this file for a description of the individual
			demo tasks. */

			#if( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )
			{
				/* See TBD.
				Let the server work task now it can now create the servers. */
				xTaskNotifyGive( xServerWorkTaskHandle );
			}
			#endif

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
		//vIPerfInstall();
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
volatile char *pcSaveName = ( volatile char * ) pcTaskName;
volatile TaskHandle_t pxSaveTask = ( volatile TaskHandle_t ) pxTask;
	( void ) pcTaskName;
	( void ) pxTask;
	( void ) pcSaveName;
	( void ) pxSaveTask;

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

static XUartPs_Config *uart_cfg;
static XUartPs uartInstance;
static void prvMiscInitialisation( void )
{
time_t xTimeNow;

	uart_cfg = XUartPs_LookupConfig(XPAR_PS7_UART_1_DEVICE_ID);
	s32 rc = XUartPs_CfgInitialize(&uartInstance, uart_cfg, XPS_UART0_BASEADDR);
	FreeRTOS_printf( ( "XUartPs_CfgInitialize: %ld\n", rc ) );

	/* Seed the random number generator. */
	time( &xTimeNow );
	FreeRTOS_debug_printf( ( "Seed for randomiser: %lu\n", xTimeNow ) );
	prvSRand( ( uint32_t ) xTimeNow );
	FreeRTOS_debug_printf( ( "Random numbers: %08X %08X %08X %08X\n", ipconfigRAND32(), ipconfigRAND32(), ipconfigRAND32(), ipconfigRAND32() ) );

	vPortInstallFreeRTOSVectorTable();
	Xil_DCacheEnable();
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

//#warning _gettimeofday_r is just stubbed out here.
struct timezone;
struct timeval;
int _gettimeofday_r(struct _reent * x, struct timeval *y , struct timezone * ptimezone )
{
	( void ) x;
	( void ) y;
	( void ) ptimezone;

	return 0;
}
/*-----------------------------------------------------------*/

#if configUSE_TIMERS != 0
	static void prvLEDTimerCallback( TimerHandle_t xTimer )
	{
		/* Prevent compiler warnings about the unused parameter. */
		( void ) xTimer;

		/* An LED is toggle to show that something is alive. */
		vParTestToggleLED( mainLED );
	}
#endif
/*-----------------------------------------------------------*/

/* Avoid including unecessary BSP source files. */
int _write (int fd, char* buf, int nbytes)
{
	(void)fd;
	(void)buf;
	(void)nbytes;
	return 0;
}
/*-----------------------------------------------------------*/

off_t lseek(int fd, off_t offset, int whence)
{
	(void)fd;
	(void)offset;
	(void)whence;
	return ((off_t)-1);
}
/*-----------------------------------------------------------*/

off_t _lseek(int fd, off_t offset, int whence)
{
	(void)fd;
	(void)offset;
	(void)whence;
	return ((off_t)-1);
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

u32 prof_pc ;

void _profile_init( void )
{
}
/*-----------------------------------------------------------*/

void _profile_clean( void )
{
}
/*-----------------------------------------------------------*/

int _fstat()
{
	return -1;
}

int _isatty()
{
	return 0;
}

static uint32_t ulListTime;
static uint64_t ulHiresTime;

uint32_t xGetRunTimeCounterValue( void )
{
	return ( uint32_t ) ( ullGetHighResolutionTime() - ulHiresTime );
}

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
		uxArraySize = uxTaskGetSystemState( pxTaskStatusArray, uxArraySize, &ulTotalRunTime );

		// For percentage calculations.
		ulTotalRunTime /= 1000UL;

		// Avoid divide by zero errors.
		if( ulTotalRunTime > 0 )
		{
			// For each populated position in the pxTaskStatusArray array,
			// format the raw data as human readable ASCII data
			for( x = 0; x < uxArraySize; x++ )
			{
				// What percentage of the total run time has the task used?
				// This will always be rounded down to the nearest integer.
				// ulTotalRunTimeDiv100 has already been divided by 100.
				ulStatsAsPermille = pxTaskStatusArray[ x ].ulRunTimeCounter / ulTotalRunTime;

				FreeRTOS_printf( ( "%-14.14s %2u %8lu %8lu %3lu.%lu %%\n",
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
	FreeRTOS_printf( ( "Heap: min/cur/max: %lu %lu %lu\n",
			( uint32_t )xPortGetMinimumEverFreeHeapSize(),
			( uint32_t )xPortGetFreeHeapSize(),
			( uint32_t )configTOTAL_HEAP_SIZE ) );


	if( aDoClear != pdFALSE )
	{
		ulListTime = xTaskGetTickCount();
		ulHiresTime = ullGetHighResolutionTime();
	}
}

#define	DDR_START	XPAR_PS7_DDR_0_S_AXI_BASEADDR
#define	DDR_END		XPAR_PS7_DDR_0_S_AXI_HIGHADDR
BaseType_t xApplicationMemoryPermissions( uint32_t aAddress )
{
BaseType_t xResult;

	if( ( aAddress >= DDR_START ) && ( aAddress < DDR_END ) )
	{
		xResult = 3;
	}
	else
	{
		xResult = 0;
	}

	return xResult;
}

void vOutputChar( const char cChar, const TickType_t xTicksToWait  )
{
	// Do nothing
}

#if( USE_TAMAS == 0 )
	void vApplicationPingReplyHook(ePingReplyStatus_t status, uint16_t id)
	{
		lUDPLoggingPrintf("!!! answered ping, status:%lu (0==ok) id:%u\n", status, id);
	}
#endif

#if( ipconfigUSE_WOLFSSL == 0 )
	void vHandleButtonClick( BaseType_t xButton, BaseType_t *pxHigherPriorityTaskWoken )
	{
	}
#endif

#if( ipconfigUSE_DHCP_HOOK != 0 )
	/* The following function must be provided by the user.
	The user may decide to continue or stop, after receiving a DHCP offer. */
	eDHCPCallbackAnswer_t xApplicationDHCPHook( eDHCPCallbackPhase_t eDHCPPhase, uint32_t ulIPAddress )
	{
		return eDHCPContinue;
/*#warning Do not use this for release */
/*		return eDHCPStopNoChanges;*/
	}
#endif	/* ipconfigUSE_DHCP_HOOK */

const unsigned char packet_template[] = {
	0x33, 0x33, 0xff, 0x66, 0x4a, 0x7e, 0x14, 0xda, 0xe9, 0xa3, 0x94, 0x22, 0x86, 0xdd, 0x60, 0x00,
	0x00, 0x00, 0x00, 0x20, 0x3a, 0xff, 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8d, 0x11,
	0xcd, 0x9b, 0x8b, 0x66, 0x4a, 0x75, 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x01, 0xff, 0x66, 0x4a, 0x7e, 0x87, 0x00, 0x3d, 0xfd, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x80,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8d, 0x11, 0xcd, 0x9b, 0x8b, 0x66, 0x4a, 0x7e, 0x01, 0x01,
	0x14, 0xda, 0xe9, 0xa3, 0x94, 0x22
};

#include "FreeRTOS_IP_Private.h"

static const unsigned char targetMultiCastAddr[ 16 ] = {
	0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0x66, 0x4a, 0x75
};

static const unsigned char targetAddress[ 16 ] = {
	0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8d, 0x11, 0xcd, 0x9b, 0x8b, 0x66, 0x4a, 0x75
};

static const unsigned char targetMAC[ 6 ] = {
	0x33, 0x33, 0xff, 0x66, 0x4a, 0x75
};

#if( ipconfigUSE_IPv6 != 0 )
void sendICMP()
{
NetworkBufferDescriptor_t *pxNetworkBuffer;
ICMPPacket_IPv6_t *pxICMPPacket;
ICMPHeader_IPv6_t *ICMPHeader_IPv6;
NetworkEndPoint_t *pxEndPoint;
size_t xRequestedSizeBytes = sizeof( packet_template );
BaseType_t xICMPSize = sizeof( ICMPHeader_IPv6_t );
BaseType_t xDataLength = ( size_t ) ( ipSIZE_OF_ETH_HEADER + ipSIZE_OF_IP_HEADER_IPv6 + xICMPSize );

pxNetworkBuffer = pxGetNetworkBufferWithDescriptor( xRequestedSizeBytes, 1000 );

	memcpy( pxNetworkBuffer->pucEthernetBuffer, packet_template, sizeof( packet_template ) );
	pxICMPPacket = ( ICMPPacket_IPv6_t * )pxNetworkBuffer->pucEthernetBuffer;
	ICMPHeader_IPv6 = ( ICMPHeader_IPv6_t * )&( pxICMPPacket->xICMPHeader );
	pxEndPoint = FreeRTOS_FindEndPointOnNetMask_IPv6( NULL );

	ICMPHeader_IPv6->ucTypeOfMessage = ipICMP_NEIGHBOR_SOLICITATION_IPv6;
	ICMPHeader_IPv6->ucTypeOfService = 0;
	ICMPHeader_IPv6->usChecksum = 0;
	ICMPHeader_IPv6->ulReserved = 0;
	memcpy( ICMPHeader_IPv6->xIPv6_Address.ucBytes, targetAddress, sizeof( targetAddress ) );

	memcpy( pxICMPPacket->xIPHeader.xDestinationIPv6Address.ucBytes, targetMultiCastAddr, sizeof( IPv6_Address_t ) );
	memcpy( pxICMPPacket->xIPHeader.xSourceIPv6Address.ucBytes, pxEndPoint->ulIPAddress_IPv6.ucBytes, sizeof( IPv6_Address_t ) );
	pxICMPPacket->xIPHeader.usPayloadLength = FreeRTOS_htons( xICMPSize );
	ICMPHeader_IPv6->ucOptions[ 0 ] = 1;
	ICMPHeader_IPv6->ucOptions[ 1 ] = 1;
//	memcpy( ICMPHeader_IPv6->ucOptions + 2, pxEndPoint->xMACAddress.ucBytes, sizeof( pxEndPoint->xMACAddress ) );
	memcpy( ICMPHeader_IPv6->ucOptions + 2, pxEndPoint->xMACAddress.ucBytes, sizeof( pxEndPoint->xMACAddress ) );

	memcpy( pxICMPPacket->xEthernetHeader.xSourceAddress.ucBytes, targetMAC, sizeof targetMAC );

	pxICMPPacket->xIPHeader.ucHopLimit = 255;

//				#if( ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM == 0 )
	{
		/* calculate the UDP checksum for outgoing package */
		usGenerateProtocolChecksum( ( uint8_t* ) pxNetworkBuffer->pucEthernetBuffer, pdTRUE );
	}
//				#endif

	/* Important: tell NIC driver how many bytes must be sent */
	pxNetworkBuffer->xDataLength = xDataLength;
	pxNetworkBuffer->pxEndPoint = pxEndPoint;

	/* This function will fill in the eth addresses and send the packet */
	vReturnEthernetFrame( pxNetworkBuffer, pdFALSE );
}
#endif /* ipconfigUSE_IPv6 */

void vUDPLoggingHook( const char *pcMessage, BaseType_t xLength )
{
/*	XUartPs_Send( &uartInstance, ( u8 * ) pcMessage, ( u32 ) xLength ); */
}

uint32_t ulGetRunTimeCounterValue( void )
{
	return ( uint32_t ) 0uL;
}

void _fini ()
{
}

void _init ()
{
}
