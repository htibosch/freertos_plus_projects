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
#include "FreeRTOS_TCP_server.h"
#include "FreeRTOS_DHCP.h"
#include "FreeRTOS_DNS.h"
#include "NetworkInterface.h"
#include "NetworkBufferManagement.h"
#include "FreeRTOS_ARP.h"
#if( ipconfigMULTI_INTERFACE != 0 )
	#include "FreeRTOS_Routing.h"
	#include "FreeRTOS_ND.h"
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

#if( USE_TELNET != 0 )
	#include "telnet.h"
#endif

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

#if( ipconfigMULTI_INTERFACE != 0 )
	static void showEndPoint( NetworkEndPoint_t *pxEndPoint );
#endif

extern int prvFFErrorToErrno( FF_Error_t xError );
extern BaseType_t xMountFailIgnore;
extern BaseType_t xDiskPartition;

/* FTP and HTTP servers execute in the TCP server work task. */
#define mainTCP_SERVER_TASK_PRIORITY	( tskIDLE_PRIORITY + 4 )
#define	mainTCP_SERVER_STACK_SIZE		2048

#define	mainFAT_TEST_STACK_SIZE			2048

/* The number and size of sectors that will make up the RAM disk. */
#define mainRAM_DISK_SECTOR_SIZE		512UL /* Currently fixed! */
#define mainRAM_DISK_SECTORS			( ( 45UL * 1024UL * 1024UL ) / mainRAM_DISK_SECTOR_SIZE )
	//#define mainRAM_DISK_SECTORS			( 150 )
#define mainIO_MANAGER_CACHE_SIZE		( 15UL * mainRAM_DISK_SECTOR_SIZE )

/* Where the SD and RAM disks are mounted.  As set here the SD card disk is the
root directory, and the RAM disk appears as a directory off the root. */
#define mainSD_CARD_DISK_NAME			"/"
#define mainRAM_DISK_NAME				"/ram"

#define mainHAS_RAMDISK					1
#define mainHAS_SDCARD					1

static int doMountDisks = pdFALSE;

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

#define	mainUDP_SERVER_STACK_SIZE		2048
#define	mainUDP_SERVER_TASK_PRIORITY	2
static void vUDPTest( void *pvParameters );

/*-----------------------------------------------------------*/

#if( ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM != 1 )
	#warning ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM must be '1'
#endif

#if( ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM != 1 )
	#warning ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM must be '1'
#endif

#if( USE_TOM != 0 )
	int pingReplyCount = 0;
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
static void prvServerWorkTask( void *pvParameters );

void copy_file( const char *pcTarget, const char *pcSource );

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

static void sd_write_test( int aNumber );
static void sd_read_test( int aNumber );

void memcpy_test( void );

/* The default IP and MAC address used by the demo.  The address configuration
defined here will be used if ipconfigUSE_DHCP is 0, or if ipconfigUSE_DHCP is
1 but a DHCP server could not be contacted.  See the online documentation for
more information. */
static const uint8_t ucIPAddress[ 4 ] = { configIP_ADDR0, configIP_ADDR1, configIP_ADDR2, configIP_ADDR3 };
static const uint8_t ucNetMask[ 4 ] = { configNET_MASK0, configNET_MASK1, configNET_MASK2, configNET_MASK3 };
static const uint8_t ucGatewayAddress[ 4 ] = { configGATEWAY_ADDR0, configGATEWAY_ADDR1, configGATEWAY_ADDR2, configGATEWAY_ADDR3 };
//static const uint8_t ucDNSServerAddress[ 4 ] = { configDNS_SERVER_ADDR0, configDNS_SERVER_ADDR1, configDNS_SERVER_ADDR2, configDNS_SERVER_ADDR3 };
static const uint8_t ucDNSServerAddress[ 4 ] = {   8,   8,   8,   8 };
//static const uint8_t ucDNSServerAddress[ 4 ] = { 192, 168,   2,   1 };
//static const uint8_t ucDNSServerAddress[ 4 ] = { 118,  98,  44, 100 };

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
	static const uint8_t ucMACAddress[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };
	static const uint8_t ucMACAddress1[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 + 1 };
#endif /* ipconfigMULTI_INTERFACE */

/* Use by the pseudo random number generator. */
static UBaseType_t ulNextRand;

/* Handle of the task that runs the FTP and HTTP servers. */
static TaskHandle_t xServerWorkTaskHandle = NULL;

static StreamBufferHandle_t streamBuffer;
static TaskHandle_t xStreamBufferTaskHandle = NULL;
static void prvStreamBufferTask(void *parameters);

/* FreeRTOS+FAT disks for the SD card and RAM disk. */
static FF_Disk_t *pxSDDisk;
static FF_Disk_t *pxSDDisks[2];
FF_Disk_t *pxRAMDisk;

int verboseLevel = 0;

//#warning Just for testing

#define dhcpCLIENT_HARDWARE_ADDRESS_LENGTH		16
#define dhcpSERVER_HOST_NAME_LENGTH				64
#define dhcpBOOT_FILE_NAME_LENGTH 				128

#include "pack_struct_start.h"
struct xDHCPMessage
{
	uint8_t ucOpcode;
	uint8_t ucAddressType;
	uint8_t ucAddressLength;
	uint8_t ucHops;
	uint32_t ulTransactionID;
	uint16_t usElapsedTime;
	uint16_t usFlags;
	uint32_t ulClientIPAddress_ciaddr;
	uint32_t ulYourIPAddress_yiaddr;
	uint32_t ulServerIPAddress_siaddr;
	uint32_t ulRelayAgentIPAddress_giaddr;
	uint8_t ucClientHardwareAddress[ dhcpCLIENT_HARDWARE_ADDRESS_LENGTH ];
	uint8_t ucServerHostName[ dhcpSERVER_HOST_NAME_LENGTH ];
	uint8_t ucBootFileName[ dhcpBOOT_FILE_NAME_LENGTH ];
	uint32_t ulDHCPCookie;
	uint8_t ucFirstOptionByte;
}
#include "pack_struct_end.h"
typedef struct xDHCPMessage DHCPMessage_t;
extern void clearDHCPMessage(DHCPMessage_t *pxDHCPMessage);

extern void memset_test(void);

void memset_test()
{
//	DHCPMessage_t *pxDHCPMessage;
//	uint8_t msg_space[ sizeof *pxDHCPMessage + 4 ];
//	int offset;
//	for( offset = 0; offset < 4; offset++ )
//	{
//		pxDHCPMessage = ( DHCPMessage_t * ) ( msg_space + offset );
//		clearDHCPMessage( pxDHCPMessage );
//	}
}

extern int use_event_logging;

#if( ipconfigMULTI_INTERFACE != 0 )
	NetworkInterface_t *pxZynq_FillInterfaceDescriptor( BaseType_t xEMACIndex, NetworkInterface_t *pxInterface );
#endif /* ipconfigMULTI_INTERFACE */
/*-----------------------------------------------------------*/

int main( void )
{
#if( ipconfigMULTI_INTERFACE != 0 )
	static NetworkInterface_t xInterfaces[ 2 ];
	static NetworkEndPoint_t xEndPoints[ 4 ];
	NetworkEndPoint_t *pxEndPoint_0;
	NetworkEndPoint_t *pxEndPoint_1;
	NetworkEndPoint_t *pxEndPoint_2;
	NetworkEndPoint_t *pxEndPoint_3;
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
	#if( ipconfigOLD_MULTI != 0 )
	{
		pxZynq_FillInterfaceDescriptor( 0, &( xInterfaces[ 0 ] ) );
		FreeRTOS_FillEndPoint( &( xEndPoints[ 0 ] ), ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress );
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
		#endif	/* ( ipconfigUSE_IPv6 != 0 ) */
		FreeRTOS_AddEndPoint( &( xInterfaces[ 0 ] ), &( xEndPoints[ 0 ] ) );

	//	pxZynq_FillInterfaceDescriptor( 1, &( xInterfaces[ 1 ] ) );
	//	FreeRTOS_FillEndPoint( &( xEndPoints[ 1 ] ), ucIPAddress2, ucNetMask2, ucGatewayAddress2, ucDNSServerAddress2, ucMACAddress2 );
	//	FreeRTOS_AddEndPoint( &( xInterfaces[ 1 ] ), &( xEndPoints[ 1 ] ) );

		/* You can modify fields: */
		xEndPoints[ 0 ].bits.bIsDefault = pdTRUE_UNSIGNED;
		xEndPoints[ 0 ].bits.bWantDHCP = pdTRUE_UNSIGNED;
	}
	#else
	{
		pxZynq_FillInterfaceDescriptor( 0, &( xInterfaces[ 0 ] ) );

		/*
		 * End-point-1  // private + public
		 *     Network: 192.168.2.x/24
		 *     IPv4   : 192.168.2.12
		 *     Gateway: 192.168.2.1 ( NAT router )
		 */
		pxEndPoint_0 = &( xEndPoints[ 0 ] );
		pxEndPoint_1 = &( xEndPoints[ 1 ] );
		pxEndPoint_2 = &( xEndPoints[ 2 ] );
		pxEndPoint_3 = &( xEndPoints[ 3 ] );
		{
			FreeRTOS_FillEndPoint( &( xInterfaces[0] ), pxEndPoint_0, ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress1 );
			pxEndPoint_0->bits.bWantDHCP = pdTRUE;
	//#warning DHCP disabled

	//		FreeRTOS_FillEndPoint( &( xInterfaces[0] ), &( xEndPoints[ 3 ] ), ucIPAddress2, ucNetMask2, ucGatewayAddress2, ucDNSServerAddress2, ucMACAddress2 );
		}
		{
			const uint8_t ucIPAddress[] = { 172,  16,   0, 114 };
			const uint8_t ucNetMask[]   = { 255, 255,   0,   0};
			const uint8_t ucGatewayAddress[]   = { 0, 0,   0,   0};
			const uint8_t ucDNSServerAddress[]   = { 0, 0,   0,   0};
			FreeRTOS_FillEndPoint( &( xInterfaces[0] ), pxEndPoint_3, ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress1 );
		}
	}
	#endif

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
//			FreeRTOS_inet_pton6( "2001:470:ec54::", xPrefix.ucBytes );

			FreeRTOS_CreateIPv6Address( &xIPAddress, &xPrefix, 64, pdTRUE );

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

			FreeRTOS_inet_pton6( "2001:470:ec54::", xPrefix.ucBytes );
			FreeRTOS_inet_pton6( "2001:4860:4860::8888", xDNSServer.ucBytes );			

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
			#endif	/* ( ipconfigUSE_RA != 0 ) */
		}
	}
	#endif

	FreeRTOS_IPStart();
#endif /* ipconfigMULTI_INTERFACE */

	memset_test();

	/* Custom task for testing. */
	xTaskCreate( prvServerWorkTask, "SvrWork", mainTCP_SERVER_STACK_SIZE, NULL, 2, &xServerWorkTaskHandle );

	/* Create a timer that just toggles an LED to show something is alive. */
	#if configUSE_TIMERS != 0
	{
//		xLEDTimer = xTimerCreate( "LED", mainLED_TOGGLE_PERIOD, pdTRUE, NULL, prvLEDTimerCallback );
//		configASSERT( xLEDTimer );
//		xTimerStart( xLEDTimer, 0 );
	}
	#endif
	#if( USE_LOG_EVENT != 0 )
	{
		eventLogAdd("Program started");
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
		xMountFailIgnore = 1;
		pxSDDisk = FF_SDDiskInit( mainSD_CARD_DISK_NAME );
		xMountFailIgnore = 0;
		if( pxSDDisk != NULL )
		{
			FF_IOManager_t *pxIOManager = sddisk_ioman( pxSDDisk );
			uint64_t ullFreeBytes =
				( uint64_t ) pxIOManager->xPartition.ulFreeClusterCount * pxIOManager->xPartition.ulSectorsPerCluster * 512ull;
			FreeRTOS_printf( ( "Volume %-12.12s\n",
				pxIOManager->xPartition.pcVolumeLabel ) );
			FreeRTOS_printf( ( "Free clusters %lu total clusters %lu Free %lu KB\n",
				pxIOManager->xPartition.ulFreeClusterCount,
				pxIOManager->xPartition.ulNumClusters,
				( uint32_t ) ( ullFreeBytes / 1024ull ) ) );
#if( ffconfigMAX_PARTITIONS == 1 )
	#warning Only 1 partition defined
#else
			for (int partition = 1; partition <= ARRAY_SIZE( pxSDDisks ); partition++) {
				char name[32];
				xDiskPartition = partition;
				snprintf(name, sizeof name, "/part_%d", partition);
				pxSDDisks[partition-1] = FF_SDDiskInit( name );
			}
#endif
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

#if( ipconfigUSE_IPv6 != 0 )
volatile int xDNSCount;
	void onDNSEvent( const char * pcName, void *pvSearchID, struct freertos_sockaddr6 *pxAddress6 )
	{
		if( pxAddress6->sin_family == FREERTOS_AF_INET6 )
		{
		BaseType_t xIsEmpty = pdTRUE, xIndex;

			for( xIndex = 0; xIndex < ( BaseType_t ) ARRAY_SIZE( pxAddress6->sin_addrv6.ucBytes ); xIndex++ )
			{
				if( pxAddress6->sin_addrv6.ucBytes[ xIndex ] != ( uint8_t ) 0u )
				{
					xIsEmpty = pdFALSE;
					break;
				}
			}
			if( xIsEmpty )
			{
				FreeRTOS_printf( ( "onDNSEvent/v6: '%s' timed out\n", pcName ) );
			}
			else
			{
				FreeRTOS_printf( ( "onDNSEvent/v6: found '%s' on %pip\n", pcName, pxAddress6->sin_addrv6.ucBytes ) );
			}
		}
		else
		{
		struct freertos_sockaddr *pxAddress4 = ( struct freertos_sockaddr * ) pxAddress6;

			if( pxAddress4->sin_addr == 0uL )
			{
				FreeRTOS_printf( ( "onDNSEvent/v4: '%s' timed out\n", pcName ) );
			}
			else
			{
				FreeRTOS_printf( ( "onDNSEvent/v4: found '%s' on %lxip\n", pcName, FreeRTOS_ntohl( pxAddress4->sin_addr ) ) );
			}
		}
		if( xServerWorkTaskHandle != NULL )
		{
			xDNSCount++;
			xTaskNotifyGive( xServerWorkTaskHandle );
		}
	}
#else
	void onDNSEvent ( const char * pcName, void * pvSearchID, uint32_t ulIPAddress )
	{
		FreeRTOS_printf( ( "DNS callback: %s with ID %04x found on %lxip\n", pcName, pvSearchID, FreeRTOS_ntohl( ulIPAddress ) ) );
	}
#endif

#if( USE_TELNET != 0 )
	Telnet_t *pxTelnetHandle = NULL;

	void vUDPLoggingHook( const char *pcMessage, BaseType_t xLength )
	{
		if (pxTelnetHandle != NULL && pxTelnetHandle->xClients != NULL) {
			xTelnetSend (pxTelnetHandle, NULL, pcMessage, xLength );
		}
		/*	XUartPs_Send( &uartInstance, ( u8 * ) pcMessage, ( u32 ) xLength ); */
	}
#endif

#if( ipconfigMULTI_INTERFACE == 0 )
	void dnsFound (const char *pcName, void *pvSearchID, uint32_t aIp)
	{
		FreeRTOS_printf( ( "DNS: '%s' found at %lxip\n", pcName, FreeRTOS_ntohl( aIp ) ) );
	}
#endif

extern void plus_echo_client_thread( void *parameters );
extern void vStartSimple2TCPServerTasks( uint16_t usStackSize, UBaseType_t uxPriority );

	static void prvServerWorkTask( void *pvParameters )
	{
	TCPServer_t *pxTCPServer = NULL;
	const TickType_t xInitialBlockTime = pdMS_TO_TICKS( 20UL );

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

		SemaphoreHandle_t xServerSemaphore;
		xServerSemaphore = xSemaphoreCreateBinary();
		configASSERT( xServerSemaphore != NULL );

		/* Remove compiler warning about unused parameter. */
		( void ) pvParameters;

		#if( configUSE_TASK_FPU_SUPPORT == 1 )
		{
			portTASK_USES_FLOATING_POINT();
		}
		#endif /* configUSE_TASK_FPU_SUPPORT == 1 */

		FreeRTOS_printf( ( "Creating files\n" ) );

		/* Create the RAM disk used by the FTP and HTTP servers. */
		//prvCreateDiskAndExampleFiles();

/*

		#if( ipconfigUSE_WOLFSSL != 0 )
		{
			if( ( pxSDDisk != NULL ) || ( pxRAMDisk != NULL ) )
			{
				FreeRTOS_printf( ( "Calling vStartWolfServerTask\n" ) );
				vStartWolfServerTask();
			}
		}
		#endif // ipconfigUSE_WOLFSSL
*/
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

		vStartSimple2TCPServerTasks( 1024u, 2u );

		//xTaskCreate (plus_echo_client_thread, "echo_client", STACK_WEBSERVER_TASK, NULL, PRIO_WEBSERVER_TASK, NULL);
		#if( USE_TELNET != 0 )
		Telnet_t xTelnet;
		memset( &xTelnet, '\0', sizeof xTelnet );
		#endif

		for( ;; )
		{
		TickType_t xReceiveTimeOut = pdMS_TO_TICKS( 200 );
			if( pxTCPServer )
			{
				FreeRTOS_TCPServerWork( pxTCPServer, xInitialBlockTime );
			}

			xSemaphoreTake( xServerSemaphore, xReceiveTimeOut );

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
			static BaseType_t xHadSocket = pdFALSE;
			TickType_t xReceiveTimeOut = pdMS_TO_TICKS( 2 );

				if( xHadSocket == pdFALSE )
				{
					xHadSocket = pdTRUE;
					FreeRTOS_printf( ( "prvCommandTask started\n" ) );

					FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof( xReceiveTimeOut ) );
					FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_SET_SEMAPHORE, ( void * ) &xServerSemaphore, sizeof( xServerSemaphore ) );

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
				if (doMountDisks) {
					doMountDisks = pdFALSE;
					prvCreateDiskAndExampleFiles();
				}

				xCount = FreeRTOS_recvfrom( xSocket, ( void * )cBuffer, sizeof( cBuffer ) - 1, pxTCPServer ? FREERTOS_MSG_DONTWAIT : 0,
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
					if( strncmp( cBuffer, "ver", 3 ) == 0 ) {
					int level;
					extern unsigned link_speed;

						if( sscanf( cBuffer + 3, "%d", &level ) == 1 ) {
							verboseLevel = level;
						}
						FreeRTOS_printf( ( "Verbose level %d link_speed %u\n", verboseLevel, link_speed ) );
						#if( ipconfigCACHE_COUNTING != 0 )
						{
							if (pxRAMDisk && pxRAMDisk->pxIOManager) {
								FreeRTOS_printf( ( "RAM: Buffers %d / %d / %d\n", 
									pxRAMDisk->pxIOManager->bufferUseCount,
									pxRAMDisk->pxIOManager->bufferValidCount,
									pxRAMDisk->pxIOManager->bufferCount ) );
							}
							if (pxSDDisk && pxSDDisk->pxIOManager) {
								FreeRTOS_printf( ( "SDC: Buffers %d / %d / %d\n", 
									pxSDDisk->pxIOManager->bufferUseCount,
									pxRAMDisk->pxIOManager->bufferValidCount,
									pxSDDisk->pxIOManager->bufferCount ) );
							}
						}
						#endif
//					} else if( strncmp( cBuffer, "delay", 5 ) == 0 ) {
//extern TickType_t ack_reception_delay;
//						unsigned value;
//						if( sscanf( cBuffer + 5, "%u", &value ) == 1 ) {
//							ack_reception_delay = value;
//						}
//						FreeRTOS_printf( ( "ack_reception_delay = %u\n", ack_reception_delay ) );
					} else if( strncmp( cBuffer, "memtest", 7 ) == 0 ) {
						memcpy_test ();
					} else if (strncasecmp (cBuffer, "fcopy", 5) == 0) {
						copy_file( "/scratch.txt", "/file_1mb.txt" );
					} else if (strncasecmp (cBuffer, "partition", 9) == 0) {
						if (pxSDDisk != NULL) {
							FF_PartitionParameters_t partitions;
							uint32_t ulNumberOfSectors = pxSDDisk->ulNumberOfSectors;
							memset( &partitions, 0x00, sizeof( partitions ) );
							partitions.ulSectorCount = ulNumberOfSectors;
							// ulHiddenSectors is the distance between the start of the partition and
							// the BPR ( Partition Boot Record ).
							// It may not be zero.
							partitions.ulHiddenSectors = 8;
							partitions.ulInterSpace = 0;
							if (ffconfigMAX_PARTITIONS == 1)
							{
								partitions.xPrimaryCount = 1;
							}
							else
							{
								partitions.xPrimaryCount = 2;	// That is correct
							}
							partitions.eSizeType = eSizeIsPercent;

							int iPercentagePerPartition = 100 / ffconfigMAX_PARTITIONS;
							if (iPercentagePerPartition > 33)
							{
								iPercentagePerPartition = 33;
							}
							for (size_t i = 0; i < ffconfigMAX_PARTITIONS; i++)
							{
								partitions.xSizes[ i ] = iPercentagePerPartition;
							}

							FF_Error_t partition_err = FF_Partition( pxSDDisk, &partitions);
							FF_FlushCache( pxSDDisk->pxIOManager );

							FreeRTOS_printf( ( "FF_Partition: 0x%08lX (errno %d) Number of sectors %lu\n", partition_err, prvFFErrorToErrno( partition_err ), ulNumberOfSectors ) );
							if( !FF_isERR( partition_err ) ) {
								for (size_t partNr = 0; partNr < ffconfigMAX_PARTITIONS; partNr++)
								{
									FF_Error_t err_format = FF_Format( pxSDDisk, partNr, pdFALSE, pdFALSE );
									FreeRTOS_printf( ( "FF_Format: 0x%08lX (errno %d)\n", err_format, prvFFErrorToErrno(err_format) ) );
									if (FF_isERR( err_format )) {
										break;
									}
								}
							}
						}
					} else if (strncasecmp (cBuffer, "format", 6) == 0) {
						int partition = 0;
						char *ptr = cBuffer + 6;
						while (isspace (*ptr)) ptr++;
						if (sscanf (ptr, "%d", &partition) == 1 &&
							partition >= 0 &&
							partition < ffconfigMAX_PARTITIONS) {
							FF_Disk_t *disk = NULL;
							switch (partition) {
								case 0: disk = pxSDDisk; break;
								case 1:
								case 2: disk = pxSDDisks[partition-1]; break;
							}
							if (disk == NULL) {
								FF_PRINTF("No handle for partition %d\n", partition);
							} else {
								FF_SDDiskUnmount( disk );
								{
									/* Format the drive */
									int xError = FF_Format( disk, partition, pdFALSE, pdFALSE);  // Try FAT32 with large clusters
									if( FF_isERR( xError ) )
									{
										FF_PRINTF( "FF_SDDiskFormat: %s\n", (const char*)FF_GetErrMessage( xError ) );
									}
									else
									{
										FF_PRINTF( "FF_SDDiskFormat: OK, now remounting\n" );
										disk->xStatus.bPartitionNumber = partition;
										xError = FF_SDDiskMount( disk );
										FF_PRINTF( "FF_SDDiskFormat: rc %08x\n", ( unsigned )xError );
										if( FF_isERR( xError ) == pdFALSE )
										{
											FF_SDDiskShowPartition( disk );
										}
									}
								}
							}
						}
					} else if (strncasecmp (cBuffer, "memset", 6) == 0) {
						memset_test();
					} else if (strncasecmp (cBuffer, "ff_free", 7) == 0) {
						ff_chdir( "/ram" );
						ff_mkdir( "mydir" );
						ff_chdir( "mydir" );
						ff_free_CWD_space();
					} else if( strncmp( cBuffer, "msgs", 4 ) == 0 ) {
						if (streamBuffer == NULL) {
							//streamBuffer = xStreamBufferCreate( 256, 1 /* xTriggerLevelBytes */ );
							streamBuffer = xMessageBufferCreate( 256 );
							FreeRTOS_printf( ( "Created buffer\n" ) );
						}

						if (xStreamBufferTaskHandle == NULL) {
							/* Custom task for testing. */
							xTaskCreate( prvStreamBufferTask, "StreamBuf", mainTCP_SERVER_STACK_SIZE, NULL, 2, &xStreamBufferTaskHandle );
							FreeRTOS_printf( ( "Created task\n" ) );
						}

						char *tosend = cBuffer + 4;
						int len = strlen (tosend);
						if (len) {
							size_t rc = xStreamBufferSend ( streamBuffer, tosend, len, 1000 );
//							size_t rc = xMessageBufferSend( streamBuffer, tosend, len, 1000 );
							FreeRTOS_printf( ( "Sending data: '%s' len = %d (%lu)\n", tosend, len, rc ) );
						}
					} else if( strncmp( cBuffer, "dnss", 4 ) == 0 ) {
						char *ptr = cBuffer + 4;
						while (isspace(*ptr)) ptr++;
						if (*ptr) {
							unsigned u32[4];
							char ch;
							int rc = sscanf (ptr, "%u%c%u%c%u%c%u",
								u32 + 0,
								&ch,
								u32 + 1,
								&ch,
								u32 + 2,
								&ch,
								u32 + 3);
							if (rc >= 7) {
								uint32_t address = FreeRTOS_inet_addr_quick( u32[ 0 ], u32[ 1 ], u32[ 2 ], u32[ 3 ] );
								FreeRTOS_printf( ( "DNS server at %xip\n", FreeRTOS_ntohl ( address ) ) );
								/* pxEndPoint, pulIPAddress, pulNetMask, pulGatewayAddress, pulDNSServerAddress*/
								#if( ipconfigMULTI_INTERFACE != 0 )
								{
									FreeRTOS_SetAddressConfiguration( NULL, NULL, NULL, &( address ), NULL );
								}
								#else
								{
									FreeRTOS_SetAddressConfiguration( NULL, NULL, NULL, &( address ) );
								}
								#endif	/* ( ipconfigMULTI_INTERFACE != 0 ) */
							}
						}
						uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
						FreeRTOS_GetAddressConfiguration( &ulIPAddress, &ulNetMask, &ulGatewayAddress, &ulDNSServerAddress
						#if( ipconfigMULTI_INTERFACE != 0 )
							, NULL
						#endif /* ( ipconfigMULTI_INTERFACE != 0 ) */
						);
						FreeRTOS_printf( ( "IP-address = %lxip\n", FreeRTOS_ntohl( ulIPAddress ) ) );
						FreeRTOS_printf( ( "Net mask   = %lxip\n", FreeRTOS_ntohl( ulNetMask ) ) );
						FreeRTOS_printf( ( "Gateway    = %lxip\n", FreeRTOS_ntohl( ulGatewayAddress ) ) );
						FreeRTOS_printf( ( "DNS server = %lxip\n", FreeRTOS_ntohl( ulDNSServerAddress ) ) );
				#if( ipconfigMULTI_INTERFACE == 0 )
					} else if( strncmp( cBuffer, "dnsq", 4 ) == 0 ) {
						char *ptr = cBuffer + 4;
						while (*ptr && !isspace (*ptr)) ptr++;
						while (isspace (*ptr)) ptr++;
						unsigned tmout = 4000;
						static unsigned searchID;
						if (*ptr) {
							for (char *target = ptr; *target; target++) {
								if (isspace (*target)) {
									*target = '\0';
									break;
								}
							}
							FreeRTOS_printf( ("DNS query: '%s'\n", ptr ) );
	#if( ipconfigDNS_USE_CALLBACKS != 0 )
							uint32_t ip = FreeRTOS_gethostbyname_a (ptr, dnsFound, (void*)++searchID, tmout);
	#else
							uint32_t ip = FreeRTOS_gethostbyname (ptr);
	#endif
							if (ip)
								FreeRTOS_printf( ("%s : %lxip\n", ptr, FreeRTOS_ntohl( ip ) ) );
						} else {
							FreeRTOS_printf( ("Usage: dnsquery <name>\n" ) );
						}
						#if( ipconfigUSE_DNS_CACHE != 0 )
						{
							FreeRTOS_dnsclear();
						}
						#endif /* ipconfigUSE_DNS_CACHE */
				#endif	/* ( ipconfigMULTI_INTERFACE == 0 ) */
#if( ipconfigUSE_WOLFSSL != 0 )
					} else if (strncasecmp (cBuffer, "wolfssl", 4) == 0) {
						if( ( pxSDDisk != NULL ) || ( pxRAMDisk != NULL ) )
						{
							FreeRTOS_printf( ( "Calling vStartWolfServerTask\n" ) );
							vStartWolfServerTask();
						}
#endif
					} else if (strncasecmp (cBuffer, "dns", 3) == 0) {
						char *ptr = cBuffer + 3;
						while (isspace (*ptr)) ptr++;
						if (*ptr) {
							static uint32_t ulSearchID = 0x0000;
							ulSearchID++;
							FreeRTOS_printf( ( "Looking up '%s'\n", ptr ) );
//							uint32_t ulIPAddress = FreeRTOS_gethostbyname_a( ptr );
//							FreeRTOS_printf( ( "Address = %lxip\n", FreeRTOS_ntohl( ulIPAddress ) ) );
							FreeRTOS_gethostbyname_a( ptr, onDNSEvent, (void *)ulSearchID, 5000 );
						}
#if( USE_IP_DROP_SELECTIVE_PORT != 0 ) && ( ipconfigMULTI_INTERFACE != 0 )
					} else if (strncasecmp (cBuffer, "droprx", 6) == 0) {
						setDrop( 21, 0 );
					} else if (strncasecmp (cBuffer, "droptx", 6) == 0) {
						setDrop( 0, 21 );
					} else if (strncasecmp (cBuffer, "droptx", 4) == 0) {
						setDrop( 0, 0 );
#endif
#if( USE_TOM != 0 )
					} else if( strncmp( cBuffer, "ping", 4 ) == 0 ) {
						const char *ptr = cBuffer + 4;
						uint32_t ipAddress = 0;
						unsigned ad0, ad1, ad2, ad3;
						while( isspace( ( int ) *ptr ) ) ptr++;
						if (*ptr) {
							BaseType_t Result;
							if (sscanf (ptr, "%u.%u.%u.%u", &ad0, &ad1, &ad2, &ad3) >= 4) {
								ipAddress =
									( ( ad0 & 0xff ) << 24 ) |
									( ( ad1 & 0xff ) << 16 ) |
									( ( ad2 & 0xff ) <<  8 ) |
									( ( ad3 & 0xff ) <<  0 );

								Result = FreeRTOS_SendPingRequest( FreeRTOS_htonl( ipAddress ), 11, 100 );
								FreeRTOS_printf( ( "Send ping to %lxip: rc = %ld replies = %d\n", ipAddress, Result, 
									pingReplyCount ) );
							}
						}
						if (!ipAddress) {
							FreeRTOS_printf( ( "Usage: ping <IP address>\n" ) );
						}
#endif
#if( USE_IPERF != 0 )
					} else if( strncmp( cBuffer, "iperf", 5 ) == 0 ) {
					extern BaseType_t xTCPWindowLoggingLevel;
						xTCPWindowLoggingLevel = 1;
						vIPerfInstall();
#endif	/* ( USE_IPERF != 0 ) */
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
					} else if( memcmp( cBuffer, "sdwrite", 6 ) == 0 ) {
						int number = -1;
						char *ptr = cBuffer + 6;
						while( *ptr && !isdigit( ( int ) ( *ptr ) ) ) ptr++;
						if( isdigit( ( int ) ( *ptr ) ) )
						{
							sscanf( ptr, "%d", &number );
						}
						sd_write_test( number );
					} else if( memcmp( cBuffer, "sdread", 6 ) == 0 ) {
						int number = -1;
						char *ptr = cBuffer + 6;
						while( *ptr && !isdigit( ( int ) ( *ptr ) ) ) ptr++;
						if( isdigit( ( int ) ( *ptr ) ) )
						{
							sscanf( ptr, "%d", &number );
						}
						sd_read_test( number );
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
				#if( ipconfigMULTI_INTERFACE != 0 )
					} else if( strncmp( cBuffer, "ifconfig", 4 ) == 0 ) {
						NetworkInterface_t *pxInterface;
						NetworkEndPoint_t *pxEndPoint;

						for( pxInterface = FreeRTOS_FirstNetworkInterface();
							pxInterface != NULL;
							pxInterface = FreeRTOS_NextNetworkInterface( pxInterface ) )
						{
							for( pxEndPoint = FreeRTOS_FirstEndPoint( pxInterface );
								pxEndPoint != NULL;
								pxEndPoint = FreeRTOS_NextEndPoint( pxInterface, pxEndPoint ) )
							{
								showEndPoint( pxEndPoint );
							}
						}
				#endif	/* ipconfigMULTI_INTERFACE */
					} else if( strncmp( cBuffer, "netstat", 7 ) == 0 ) {
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

			/* See TBD.
			Let the server work task now it can now create the servers. */
			xTaskNotifyGive( xServerWorkTaskHandle );

			/* Start a new task to fetch logging lines and send them out. */
			vUDPLoggingTaskCreate();

			xTasksAlreadyCreated = pdTRUE;
			doMountDisks = pdTRUE;
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
				FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulIPAddress ), FreeRTOS_ntohl( pxEndPoint->ipv4_defaults.ulIPAddress ) ) );
		#if( ipconfigOLD_MULTI == 0 )
			FreeRTOS_printf( ( "Net mask   : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_defaults.ulNetMask ) ) );
			FreeRTOS_printf( ( "GW         : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_defaults.ulGatewayAddress ) ) );
			FreeRTOS_printf( ( "DNS        : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_defaults.ulDNSServerAddresses[ 0 ] ) ) );
			FreeRTOS_printf( ( "Broadcast  : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_defaults.ulBroadcastAddress ) ) );
			FreeRTOS_printf( ( "MAC address: %02x-%02x-%02x-%02x-%02x-%02x\n",
				pxEndPoint->xMACAddress.ucBytes[ 0 ],
				pxEndPoint->xMACAddress.ucBytes[ 1 ],
				pxEndPoint->xMACAddress.ucBytes[ 2 ],
				pxEndPoint->xMACAddress.ucBytes[ 3 ],
				pxEndPoint->xMACAddress.ucBytes[ 4 ],
				pxEndPoint->xMACAddress.ucBytes[ 5 ] ) );
		#endif	/* ( ipconfigOLD_MULTI == 0 ) */
			FreeRTOS_printf( ( " \n" ) );
		}
		#endif /* ipconfigMULTI_INTERFACE */
		//vIPerfInstall();
		//xTaskCreate( vUDPTest, "vUDPTest", mainUDP_SERVER_STACK_SIZE, NULL, mainUDP_SERVER_TASK_PRIORITY, NULL );
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

BaseType_t xApplicationGetRandomNumber( uint32_t *pulNumber )
{
	uint32_t ulR1 = uxRand() & 0x7fff;
	uint32_t ulR2 = uxRand() & 0x7fff;
	uint32_t ulR3 = uxRand() & 0x7fff;
	*pulNumber = (ulR1 << 17) | (ulR2 << 2) | (ulR3 & 0x03uL);
	return pdTRUE;
}

BaseType_t xRandom32( uint32_t *pulValue )
{
	*pulValue = uxRand();
	return pdPASS;
}

uint32_t ulApplicationGetNextSequenceNumber(
    uint32_t ulSourceAddress,
    uint16_t usSourcePort,
    uint32_t ulDestinationAddress,
    uint16_t usDestinationPort )
{
	uint32_t ulValue = uxRand();
	ulValue |= ((uint32_t)uxRand()) << 16;
	return ulValue;
}

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

#if( ipconfigMULTI_INTERFACE != 0 )
BaseType_t xApplicationDNSQueryHook( struct xNetworkEndPoint *pxEndPoint, const char *pcName )
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
		unsigned number = 0;
		if( sscanf( pcName + 3, "%u", &number) > 0 )
		{
			if( number == 172 )
			{
				#if( ipconfigOLD_MULTI == 0 )
				{
					pxEndPoint->ipv4_settings.ulIPAddress = FreeRTOS_inet_addr_quick( 172,  16,   0, 114 );
				}
				#else
				{
					pxEndPoint->ulIPAddress = FreeRTOS_inet_addr_quick( 172,  16,   0, 114 );
				}
				#endif
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
					#if( ipconfigOLD_MULTI == 0 )
					{
						pxEndPoint->ipv4_settings.ulIPAddress = px->ipv4_settings.ulIPAddress;
					}
					#else
					{
						pxEndPoint->ulIPAddress = px->ulIPAddress;
					}
					#endif
				}
				else
				{
					#if( ipconfigOLD_MULTI != 0 )
					{
						pxEndPoint->ulIPAddress = FreeRTOS_inet_addr_quick( 192,  168,   2, 114 );
					}
					#else
					{
						pxEndPoint->ipv4_settings.ulIPAddress = FreeRTOS_inet_addr_quick( 192,  168,   2, 114 );
					}
					#endif
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

void _fini ()
{
}

void _init ()
{
}
static uint64_t ullHiresTime;
uint32_t ulGetRunTimeCounterValue( void )
{
	return ( uint32_t ) ( ullGetHighResolutionTime() - ullHiresTime );
}

BaseType_t xTaskClearCounters;

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
	FreeRTOS_printf( ( "Heap: min/cur/max: %lu %lu %lu\n",
			( uint32_t )xPortGetMinimumEverFreeHeapSize(),
			( uint32_t )xPortGetFreeHeapSize(),
			( uint32_t )configTOTAL_HEAP_SIZE ) );
	if( aDoClear != pdFALSE )
	{
//		ulListTime = xTaskGetTickCount();
		ullHiresTime = ullGetHighResolutionTime();
	}
}
/*-----------------------------------------------------------*/


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
		lUDPLoggingPrintf("Answered ping, status:%lu (0==ok) id:%u\n", status, id);
		pingReplyCount++;
	}
#endif

#if( ipconfigUSE_WOLFSSL == 0 )
	void vHandleButtonClick( BaseType_t xButton, BaseType_t *pxHigherPriorityTaskWoken )
	{
	}
#endif

#if( ipconfigUSE_DHCP_HOOK != 0 )
//	static int s_StaticIPIsEnabled, s_DHCPIsEnabled;
//    eDHCPCallbackAnswer_t xApplicationDHCPHook(eDHCPCallbackPhase_t eDHCPPhase, uint32_t ulIPAddress)
//    {
//        eDHCPCallbackAnswer_t xResult = eDHCPContinue;
//
//        switch(eDHCPPhase)
//        {
//            case eDHCPPhasePreRequest:
//                // Driver is about to ask for a DHCP offer.
//            case eDHCPPhasePreDiscover:
//                // Driver is about to request DHCP an IP address.
//                if(s_StaticIPIsEnabled == pdTRUE)
//                {
//                    xResult = eDHCPUseDefaults;
//                }
//                else if(s_DHCPIsEnabled == pdFALSE)
//                {
//                    xResult = eDHCPStopNoChanges;
//                }
//                break;
//        }
//
//        // Return the result.
//        return(xResult);
//    }

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


#if( ipconfigUSE_IPv6 != 0 )

static const unsigned char targetMultiCastAddr[ 16 ] = {
	0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0x66, 0x4a, 0x75
};

static const unsigned char targetAddress[ 16 ] = {
	0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8d, 0x11, 0xcd, 0x9b, 0x8b, 0x66, 0x4a, 0x75
};

static const unsigned char targetMAC[ 6 ] = {
	0x33, 0x33, 0xff, 0x66, 0x4a, 0x75
};

void sendICMP()
{
NetworkBufferDescriptor_t *pxNetworkBuffer;
ICMPPacket_IPv6_t *pxICMPPacket;
ICMPHeader_IPv6_t *ICMPHeader_IPv6;
NetworkEndPoint_t *pxEndPoint;
size_t xRequestedSizeBytes = sizeof( packet_template );
BaseType_t xICMPSize = sizeof( ICMPHeader_IPv6_t );
BaseType_t xDataLength = ( size_t ) ( ipSIZE_OF_ETH_HEADER + ipSIZE_OF_IPv6_HEADER + xICMPSize );

pxNetworkBuffer = pxGetNetworkBufferWithDescriptor( xRequestedSizeBytes, 1000 );

	memcpy( pxNetworkBuffer->pucEthernetBuffer, packet_template, sizeof( packet_template ) );
	pxICMPPacket = ( ICMPPacket_IPv6_t * )pxNetworkBuffer->pucEthernetBuffer;
	ICMPHeader_IPv6 = ( ICMPHeader_IPv6_t * )&( pxICMPPacket->xICMPHeader_IPv6 );
	pxEndPoint = FreeRTOS_FindEndPointOnNetMask_IPv6( NULL );

	ICMPHeader_IPv6->ucTypeOfMessage = ipICMP_NEIGHBOR_SOLICITATION_IPv6;
	ICMPHeader_IPv6->ucTypeOfService = 0;
	ICMPHeader_IPv6->usChecksum = 0;
	ICMPHeader_IPv6->ulReserved = 0;
	memcpy( ICMPHeader_IPv6->xIPv6_Address.ucBytes, targetAddress, sizeof( targetAddress ) );

	memcpy( pxICMPPacket->xIPHeader.xDestinationAddress.ucBytes, targetMultiCastAddr, sizeof( IPv6_Address_t ) );
	memcpy( pxICMPPacket->xIPHeader.xSourceAddress.ucBytes, pxEndPoint->ipv6_settings.xIPAddress.ucBytes, sizeof( IPv6_Address_t ) );
	pxICMPPacket->xIPHeader.usPayloadLength = FreeRTOS_htons( xICMPSize );
	ICMPHeader_IPv6->ucOptionBytes[ 0 ] = 1;
	ICMPHeader_IPv6->ucOptionBytes[ 1 ] = 1;
//	memcpy( ICMPHeader_IPv6->ucOptionBytes + 2, pxEndPoint->xMACAddress.ucBytes, sizeof( pxEndPoint->xMACAddress ) );
	memcpy( ICMPHeader_IPv6->ucOptionBytes + 2, pxEndPoint->xMACAddress.ucBytes, sizeof( pxEndPoint->xMACAddress ) );

	memcpy( pxICMPPacket->xEthernetHeader.xSourceAddress.ucBytes, targetMAC, sizeof targetMAC );

	pxICMPPacket->xIPHeader.ucHopLimit = 255;

//				#if( ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM == 0 )
	{
		/* calculate the UDP checksum for outgoing package */
		usGenerateProtocolChecksum( ( uint8_t* ) pxNetworkBuffer->pucEthernetBuffer, xDataLength, pdTRUE );
	}
//				#endif

	/* Important: tell NIC driver how many bytes must be sent */
	pxNetworkBuffer->xDataLength = xDataLength;
	pxNetworkBuffer->pxEndPoint = pxEndPoint;

	/* This function will fill in the eth addresses and send the packet */
	vReturnEthernetFrame( pxNetworkBuffer, pdFALSE );
}
#endif /* ipconfigUSE_IPv6 */

#define WRITE_BUF_SIZE	( 4 * ( 1024 * 1024 ) )
#define WRITE_BUF_COUNT	( 2 )

static void sd_write_test( int aNumber )
{
static int curIndex = 0;
unsigned *buffer;
char fname[ 128];
int i, index;
FF_FILE *fp;
unsigned long long times[ 20 ];
uint64_t sum = 0u;
	if( aNumber >= 0 )
	{
		curIndex = aNumber;
	}

	buffer = ( unsigned * ) pvPortMalloc( WRITE_BUF_SIZE );
	if (buffer == NULL )
	{
		FreeRTOS_printf( ( "malloc failed\n" ) );
		return;
	}
	memset( buffer, 0xE3, WRITE_BUF_SIZE );
	curIndex++;
	sprintf( fname, "/test%04d.txt", curIndex );

	fp = ff_fopen( fname, "w" );
	if (fp == NULL )
	{
		FreeRTOS_printf( ( "ff_fopen %s failed\n", fname ) );
		return;
	}
times[ 0 ] = ullGetHighResolutionTime( );
	for( index = 0; index < WRITE_BUF_COUNT; index++ )
	{
	int rc;
		rc = ff_fwrite( buffer, 1, WRITE_BUF_SIZE, fp );
times[ index + 1 ] = ullGetHighResolutionTime( );
		if( rc <= 0 )
		{
			break;
		}
		sum += ( uint64_t ) WRITE_BUF_SIZE;
	}
	ff_fclose( fp );

	for( i = 0; i < index; i++ )
	{
		FreeRTOS_printf( ( "Write %u took %lu\n",
			WRITE_BUF_SIZE,
			( unsigned long ) ( times[ i + 1 ] - times[ i ] ) ) );
	}
	if( index )
	{
	uint64_t total_time = times[ index ] - times[ 0 ];
	unsigned avg = ( unsigned ) ( ( sum * 1024ull ) / total_time );
		FreeRTOS_printf( ( "Write %s %u took %lu avg %lu\n",
			fname,
			( unsigned long ) sum,
			( unsigned long ) total_time,
			avg ) );
	}
	vPortFree( buffer );
}

static void sd_read_test( int aNumber )
{
static int curIndex = 0;
unsigned *buffer;
char fname[ 128];
int i, index;
FF_FILE *fp;
unsigned long long times[ 20 ];
uint64_t sum = 0u;
	if( aNumber >= 0 )
	{
		curIndex = aNumber;
	}

	buffer = ( unsigned * ) pvPortMalloc( WRITE_BUF_SIZE );
	if (buffer == NULL )
	{
		FreeRTOS_printf( ( "malloc failed\n" ) );
		return;
	}
	memset( buffer, 0xE3, WRITE_BUF_SIZE );
	curIndex++;
	sprintf( fname, "/test%04d.txt", curIndex );

	fp = ff_fopen( fname, "r" );
	if (fp == NULL )
	{
		FreeRTOS_printf( ( "ff_fopen %s failed\n", fname ) );
		return;
	}
times[ 0 ] = ullGetHighResolutionTime( );
	for( index = 0; index < WRITE_BUF_COUNT; index++ )
	{
	int rc;
		rc = ff_fread( buffer, 1, WRITE_BUF_SIZE, fp );
times[ index + 1 ] = ullGetHighResolutionTime( );
		if( rc <= 0 )
		{
			break;
		}
		sum += ( uint64_t ) WRITE_BUF_SIZE;
	}
	ff_fclose( fp );

	for( i = 0; i < index; i++ )
	{
		FreeRTOS_printf( ( "Write %u took %lu\n",
			WRITE_BUF_SIZE,
			( unsigned long ) ( times[ i + 1 ] - times[ i ] ) ) );
	}
	if( index )
	{
	uint64_t total_time = times[ index ] - times[ 0 ];
	unsigned avg = ( unsigned ) ( ( sum * 1024ull ) / total_time );
		FreeRTOS_printf( ( "Write %s %u took %lu avg %lu\n",
			fname,
			( unsigned long ) sum,
			( unsigned long ) total_time,
			avg ) );
	}
	vPortFree( buffer );
}

#define	LOGGING_MARK_BYTES	( 100ul * 1024ul * 1024ul )

void copy_file( const char *pcTarget, const char *pcSource )
{
FF_FILE *infile, *outfile;
unsigned *buffer;

	infile = ff_fopen( pcSource, "rb" );
	if( infile == NULL )
	{
		FreeRTOS_printf( ( "fopen %s failed\n", pcSource ) );
	}
	else
	{
		outfile = ff_fopen( pcTarget, "ab" );
		if( infile == NULL )
		{
			FreeRTOS_printf( ( "create %s failed\n", pcTarget ) );
		}
		else
		{
			buffer = ( unsigned * ) pvPortMalloc( WRITE_BUF_SIZE );
			if( buffer == NULL )
			{
				FreeRTOS_printf( ( "malloc failed\n" ) );
			}
			else
			{
			size_t remain = infile->ulFileSize;
			size_t sum = 0ul, total_sum = 0ul;
			size_t curSize;

				FreeRTOS_printf( ( "Open %s: %lX bytes\n", pcSource, remain ) );

				curSize = ff_filelength( infile );
				FreeRTOS_printf( ( "Size %s: %lX bytes\n", pcSource, curSize ) );
				curSize = ff_filelength( outfile );
				FreeRTOS_printf( ( "Size %s: %lX bytes\n", pcTarget, curSize ) );

				while( remain )
				{
				size_t toCopy = remain;
				int nwritten, nread;
					if( toCopy > WRITE_BUF_SIZE )
					{
						toCopy = WRITE_BUF_SIZE;
					}
					nread = ff_fread( buffer, 1, toCopy, infile );
					if( nread != toCopy )
					{
						FreeRTOS_printf( ( "Reading %s: %d in stead of %d bytes (%d remain)\n", pcSource, nread, toCopy, remain ) );
						break;
					}
					nwritten = ff_fwrite( buffer, 1, toCopy, outfile );
					if( nwritten != toCopy )
					{
						FreeRTOS_printf( ( "Writing %s: %d in stead of %d bytes (%d remain)\n", pcTarget, nwritten, toCopy, remain ) );
						break;
					}
					sum += toCopy;
					total_sum += toCopy;
					remain -= toCopy;
					if( ( sum >= LOGGING_MARK_BYTES ) || ( remain == 0 ) )
					{
						sum -= LOGGING_MARK_BYTES;
						FreeRTOS_printf( ( "Done %lu / %lu MB\n", total_sum / ( 1024lu * 1024lu ), infile->ulFileSize  / ( 1024lu * 1024lu ) ) );
					}
				}
				vPortFree( buffer );
			}
			ff_fclose( outfile );
		}
		ff_fclose( infile );
	}
}

#define MEMCPY_BLOCK_SIZE		0x10000
#define MEMCPY_EXTA_SIZE		128

struct SMEmcpyData {
	char target_pre [ MEMCPY_EXTA_SIZE  ];
	char target_data[ MEMCPY_BLOCK_SIZE + 16 ];
	char target_post[ MEMCPY_EXTA_SIZE  ];

	char source_pre [ MEMCPY_EXTA_SIZE  ];
	char source_data[ MEMCPY_BLOCK_SIZE + 16 ];
	char source_post[ MEMCPY_EXTA_SIZE  ];
};

typedef void * ( * FMemcpy ) ( void *pvDest, const void *pvSource, size_t ulBytes );
typedef void * ( * FMemset ) ( void *pvDest, int iChar, size_t ulBytes );

void *x_memcpy( void *pvDest, const void *pvSource, size_t ulBytes )
{
	return pvDest;
}
void *x_memset(void *pvDest, int iValue, size_t ulBytes)
{
	return pvDest;
}

#define logPrintf	lUDPLoggingPrintf

#define 	LOOP_COUNT		100

void memcpy_test()
{
int target_offset;
int source_offset;
int algorithm;
struct SMEmcpyData *pxBlock;
char *target;
char *source;
uint64_t ullStartTime;
uint32_t ulDelta;
uint32_t ulTimes[ 2 ][ 4 ][ 4 ];
uint32_t ulSetTimes[ 2 ][ 4 ];
//int time_index = 0;
int index;
uint64_t copy_size = LOOP_COUNT * MEMCPY_BLOCK_SIZE;

	memset( ulTimes, '\0', sizeof ulTimes );
	
	pxBlock = ( struct SMEmcpyData * ) pvPortMalloc( sizeof *pxBlock );
	if( pxBlock == NULL )
	{
		logPrintf( "Failed to allocate %lu bytes\n", sizeof *pxBlock );
		return;
	}
	FMemcpy memcpy_func;
	for( algorithm = 0; algorithm < 2; algorithm++ )
	{
		memcpy_func = algorithm == 0 ? memcpy : x_memcpy;
		for( target_offset = 0; target_offset < 4; target_offset++ ) {
			for( source_offset = 0; source_offset < 4; source_offset++ ) {
				target = pxBlock->target_data + target_offset;
				source = pxBlock->source_data + source_offset;
				ullStartTime = ullGetHighResolutionTime();
				for( index = 0; index < LOOP_COUNT; index++ )
				{
					memcpy_func( target, source, MEMCPY_BLOCK_SIZE );
				}
				ulDelta = ( uint32_t ) ( ullGetHighResolutionTime() - ullStartTime );
				ulTimes[ algorithm ][ target_offset ][ source_offset ] = ulDelta;
			}
		}
	}
	FMemset memset_func;
	for( algorithm = 0; algorithm < 2; algorithm++ )
	{
		memset_func = algorithm == 0 ? memset : x_memset;
		for( target_offset = 0; target_offset < 4; target_offset++ ) {
			target = pxBlock->target_data + target_offset;
			ullStartTime = ullGetHighResolutionTime();
			for( index = 0; index < LOOP_COUNT; index++ )
			{
				memset_func( target, '\xff', MEMCPY_BLOCK_SIZE );
			}
			ulDelta = ( uint32_t ) ( ullGetHighResolutionTime() - ullStartTime );
			ulSetTimes[ algorithm ][ target_offset ] = ulDelta;
		}
	}
	logPrintf( "copy_size - %lu ( %lu bits)\n",
		( uint32_t )copy_size, ( uint32_t )(8ull * copy_size) );
	for( target_offset = 0; target_offset < 4; target_offset++ )
	{
		for( source_offset = 0; source_offset < 4; source_offset++ )
		{
			uint32_t ulTime1 = ulTimes[ 0 ][ target_offset ][ source_offset ];
			uint32_t ulTime2 = ulTimes[ 1 ][ target_offset ][ source_offset ];

			uint64_t avg1 = ( copy_size * 1000000ull ) / ulTime1;
			uint64_t avg2 = ( copy_size * 1000000ull ) / ulTime2;

			uint32_t mb1 = ( uint32_t ) ( ( avg1 + 500000ull ) / 1000000ull );
			uint32_t mb2 = ( uint32_t ) ( ( avg2 + 500000ull ) / 1000000ull );

			logPrintf( "Offset[%d,%d] = memcpy %3lu.%03lu ms (%5lu MB/s) x_memcpy %3lu.%03lu ms  (%5lu MB/s)\n",
				target_offset,
				source_offset,
				ulTime1 / 1000ul, ulTime1 % 1000ul,
				mb1,
				ulTime2 / 1000ul, ulTime2 % 1000ul,
				mb2);
		}
	}
	for( target_offset = 0; target_offset < 4; target_offset++ )
	{
		uint32_t ulTime1 = ulSetTimes[ 0 ][ target_offset ];
		uint32_t ulTime2 = ulSetTimes[ 1 ][ target_offset ];

		uint64_t avg1 = ( copy_size * 1000000ull ) / ulTime1;
		uint64_t avg2 = ( copy_size * 1000000ull ) / ulTime2;

		uint32_t mb1 = ( uint32_t ) ( ( avg1 + 500000ull ) / 1000000ull );
		uint32_t mb2 = ( uint32_t ) ( ( avg2 + 500000ull ) / 1000000ull );

		logPrintf( "Offset[%d] = memset %3lu.%03lu ms (%5lu MB/s) x_memset %3lu.%03lu ms  (%5lu MB/s)\n",
			target_offset,
			ulTime1 / 1000ul, ulTime1 % 1000ul,
			mb1,
			ulTime2 / 1000ul, ulTime2 % 1000ul,
			mb2);
	}
	vPortFree( ( void * ) pxBlock );
}

#if( ipconfigMULTI_INTERFACE != 0 )
static void showEndPoint( NetworkEndPoint_t *pxEndPoint )
{
#if( ipconfigUSE_IPv6 != 0 )
	if( pxEndPoint->bits.bIPv6 )
	{
	IPv6_Address_t xPrefix;
//			pxEndPoint->ipv6_settings.xIPAddress;			/* The actual IPv4 address. Will be 0 as long as end-point is still down. */
//			pxEndPoint->ipv6_settings.uxPrefixLength;
//			pxEndPoint->ipv6_defaults.xIPAddress;
//			pxEndPoint->ipv6_settings.xGatewayAddress;
//			pxEndPoint->ipv6_settings.xDNSServerAddresses[ 2 ];	/* Not yet in use. */

		/* Extract the prefix from the IP-address */
		FreeRTOS_CreateIPv6Address( &( xPrefix ), &( pxEndPoint->ipv6_settings.xIPAddress ), pxEndPoint->ipv6_settings.uxPrefixLength, pdFALSE );

		FreeRTOS_printf( ( "IP-address : %pip\n", pxEndPoint->ipv6_settings.xIPAddress.ucBytes ) );
		if( memcmp( pxEndPoint->ipv6_defaults.xIPAddress.ucBytes, pxEndPoint->ipv6_settings.xIPAddress.ucBytes, ipSIZE_OF_IPv6_ADDRESS ) != 0 )
		{
			FreeRTOS_printf( ( "Default IP : %pip\n", pxEndPoint->ipv6_defaults.xIPAddress.ucBytes ) );
		}
		FreeRTOS_printf( ( "End-point  : up = %s\n", pxEndPoint->bits.bEndPointUp ? "yes" : "no" ) );
		FreeRTOS_printf( ( "Prefix     : %pip/%d\n", xPrefix.ucBytes, ( int ) pxEndPoint->ipv6_settings.uxPrefixLength ) );
		FreeRTOS_printf( ( "GW         : %pip\n", pxEndPoint->ipv6_settings.xGatewayAddress.ucBytes ) );
		FreeRTOS_printf( ( "DNS        : %pip\n", pxEndPoint->ipv6_settings.xDNSServerAddresses[ 0 ].ucBytes ) );
	}
	else
#endif	/* ( ipconfigUSE_IPv6 != 0 ) */
	{
		#if( ipconfigOLD_MULTI == 0 )
		{
			FreeRTOS_printf( ( "IP-address : %lxip\n",
				FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulIPAddress ) ) );
			if( pxEndPoint->ipv4_settings.ulIPAddress != pxEndPoint->ipv4_defaults.ulIPAddress )
			{
				FreeRTOS_printf( ( "Default IP : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_defaults.ulIPAddress ) ) );
			}
			FreeRTOS_printf( ( "End-point  : up = %s dhcp = %d\n",
				pxEndPoint->bits.bEndPointUp ? "yes" : "no",
				pxEndPoint->bits.bWantDHCP ) );
			FreeRTOS_printf( ( "Net mask   : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulNetMask ) ) );
			FreeRTOS_printf( ( "GW         : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulGatewayAddress ) ) );
			FreeRTOS_printf( ( "DNS        : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulDNSServerAddresses[ 0 ] ) ) );
			FreeRTOS_printf( ( "Broadcast  : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulBroadcastAddress ) ) );
		}
		#else
		{
			FreeRTOS_printf( ( "IP-address : %lxip\n",
				FreeRTOS_ntohl( pxEndPoint->ulIPAddress ) ) );
			if( pxEndPoint->ulIPAddress != pxEndPoint->ulDefaultIPAddress )
			{
				FreeRTOS_printf( ( "Default IP : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ulDefaultIPAddress ) ) );
			}
			FreeRTOS_printf( ( "End-point  : up = %s dhcp = %d\n",
				pxEndPoint->bits.bEndPointUp ? "yes" : "no",
				pxEndPoint->bits.bWantDHCP ) );
			FreeRTOS_printf( ( "Net mask   : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ulNetMask ) ) );
			FreeRTOS_printf( ( "GW         : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ulGatewayAddress ) ) );
			FreeRTOS_printf( ( "DNS        : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ulDNSServerAddresses[ 0 ] ) ) );
			FreeRTOS_printf( ( "Broadcast  : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ulBroadcastAddress ) ) );
		}
		#endif
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
#endif	/* ipconfigMULTI_INTERFACE */

//#if( ipconfigMULTI_INTERFACE == 0 )
#if 0
	BaseType_t xCheckLoopback( NetworkBufferDescriptor_t * const pxDescriptor, BaseType_t bReleaseAfterSend )
	{
	BaseType_t xResult = pdFALSE;
	NetworkBufferDescriptor_t * pxUseDescriptor = pxDescriptor;
	IPPacket_t *pxIPPacket = ( IPPacket_t * ) pxUseDescriptor->pucEthernetBuffer;
	NetworkEndPoint_t *pxEndPoint;
	MACAddress_t xMACAddress;
		/* This function will check if the target IP-address belongs to this device.
		 * If so, the packet will be passed to the IP-stack, who will answer it.
		 * The function is to be called within the function xNetworkInterfaceOutput().
		 */
		pxEndPoint = FreeRTOS_FindEndPointOnIP_IPv4( 0uL, 0uL );	/* Find the firt IPv4 end-point */
		if( pxEndPoint != NULL )
		{
			memcpy( xMACAddress.ucBytes, pxEndPoint->xMACAddress.ucBytes, sizeof xMACAddress.ucBytes );
		}
		else
		{
			memset( xMACAddress.ucBytes, 0, sizeof xMACAddress.ucBytes );
		}
		if( ( pxIPPacket->xEthernetHeader.usFrameType == ipIPv4_FRAME_TYPE ) &&
			( memcmp( pxIPPacket->xEthernetHeader.xDestinationAddress.ucBytes, xMACAddress.ucBytes, ipMAC_ADDRESS_LENGTH_BYTES ) == 0 ) )
		{
			xResult = pdTRUE;
			if( bReleaseAfterSend == pdFALSE )
			{
				/* Driver is not allowed to transfer the ownership
				of descriptor,  so make a copy of it */
				pxUseDescriptor =
					pxDuplicateNetworkBufferWithDescriptor( pxDescriptor, pxDescriptor->xDataLength );
			}
			if( pxUseDescriptor != NULL )
			{
			IPStackEvent_t xRxEvent;

				xRxEvent.eEventType = eNetworkRxEvent;
				xRxEvent.pvData = ( void * ) pxUseDescriptor;
				if( xSendEventStructToIPTask( &xRxEvent, 0u ) != pdTRUE )
				{
					vReleaseNetworkBufferAndDescriptor( pxUseDescriptor );
					iptraceETHERNET_RX_EVENT_LOST();
					FreeRTOS_printf( ( "prvEMACRxPoll: Can not queue return packet!\n" ) );
				}
				
			}
		}
		return xResult;
	}
#endif
/*-----------------------------------------------------------*/

static void prvStreamBufferTask(void *parameters)
{
	char pcBuffer[9];
	FreeRTOS_printf( ( "prvStreamBufferTask started\n" ) );
	for( ;; )
	{
		if( streamBuffer == NULL )
		{
			vTaskDelay(5000);
		}
		else
		{
			size_t rc = xStreamBufferReceive( streamBuffer, pcBuffer, sizeof pcBuffer - 1, 5000 );
			if (rc > 0) {
				pcBuffer[rc] = '\0';
				FreeRTOS_printf( ( "prvStreamBufferTask: received '%s' rc = %lu\n", pcBuffer, rc ) );
			}
		}
	}
}

static void vUDPTest( void *pvParameters )
{
Socket_t xUDPSocket;
struct freertos_sockaddr xAddress;
char pcBuffer[ 128 ];
const TickType_t x1000ms = 1000UL / portTICK_PERIOD_MS;
int port_number = 30718;
socklen_t xAddressLength;

	( void ) pvParameters;

	/* Create the socket. */
	xUDPSocket = FreeRTOS_socket( FREERTOS_AF_INET,
										FREERTOS_SOCK_DGRAM,/*FREERTOS_SOCK_DGRAM for UDP.*/
										FREERTOS_IPPROTO_UDP );

	/* Check the socket was created. */
	configASSERT( xUDPSocket != FREERTOS_INVALID_SOCKET );

	xAddress.sin_addr = FreeRTOS_GetIPAddress();
	xAddress.sin_port = FreeRTOS_htons( port_number );
	FreeRTOS_bind( xUDPSocket, &xAddress, sizeof xAddressLength );

	FreeRTOS_setsockopt( xUDPSocket, 0, FREERTOS_SO_RCVTIMEO, &x1000ms, sizeof( x1000ms ) );

	for( ;; )
	{
	int rc;
	socklen_t len = sizeof xAddress;
		rc = FreeRTOS_recvfrom( xUDPSocket, pcBuffer, sizeof pcBuffer, 0, &xAddress,  &len );
		if (rc > 0) {
			FreeRTOS_printf( ( "Received from %d: %d bytes\n", FreeRTOS_htons(xAddress.sin_port), rc ) );
		}
	}
}

