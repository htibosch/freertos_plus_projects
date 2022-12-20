/*
 * FreeRTOS+TCP V2.3.1
 * Copyright (C) 2020 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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

#if ( ipconfigMULTI_INTERFACE != 0 )
    #include "FreeRTOS_Routing.h"
    #if ( ipconfigUSE_IPv6 != 0 )
        #include "FreeRTOS_ND.h"
    #endif
#endif

#if ( ipconfigUSE_HTTP != 0 ) || ( ipconfigUSE_FTP != 0 )
    #include "FreeRTOS_tcp_server.h"
#endif

#if ( USE_IPERF != 0 )
    #include "iperf_task.h"
#endif

#include "pcap_live_player.h"

#if ( USE_PLUS_FAT != 0 )
    /* FreeRTOS+FAT includes. */
    #include "ff_headers.h"
    #include "ff_stdio.h"
    #include "ff_ramdisk.h"
    #include "ff_sddisk.h"
    #include "ff_format.h"
#endif

#if ( ffconfigMKDIR_RECURSIVE != 0 )
    #define MKDIR_FALSE    , pdFALSE
#else
    #define MKDIR_FALSE
#endif

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

#include "http_client_test.h"

#include "plus_tcp_demo_cli.h"

#if ( USE_NTOP_TEST != 0 )
    #include "inet_pton_ntop_tests.h"
#endif

#if ( USE_TELNET != 0 )
    #include "telnet.h"
#endif

#if ( USE_TCP_TESTER != 0 )
    #include "tcp_tester.h"
#endif

#include "ddos_testing.h"

#ifndef BUFFER_FROM_WHERE_CALL
    #define BUFFER_FROM_WHERE_CALL( FileName )
#endif

#if( ipconfigMAX_ARP_AGE != 150 )
	#warning Sure about this?
#endif

#ifndef dnsTYPE_ANY_HOST
	#define dnsTYPE_ANY_HOST	0xffU
#endif

#define	mainUDP_SERVER_STACK_SIZE		2048
#define	mainUDP_SERVER_TASK_PRIORITY	2
static void vUDPTest( void *pvParameters );

static void tcp_client( void );

#if( ipconfigTCP_KEEP_ALIVE != 1 )
	#warning ipconfigTCP_KEEP_ALIVE is false
#endif

extern int prvFFErrorToErrno( FF_Error_t xError );
extern BaseType_t xMountFailIgnore;
extern BaseType_t xDiskPartition;

/* FTP and HTTP servers execute in the TCP server work task. */
#define mainTCP_SERVER_TASK_PRIORITY    ( tskIDLE_PRIORITY + 4 )
#define mainTCP_SERVER_STACK_SIZE       2048

#define mainFAT_TEST_STACK_SIZE         2048

/* The number and size of sectors that will make up the RAM disk. */

#define mainRAM_DISK_SECTOR_SIZE     ffconfigRAM_SECTOR_SIZE
#define mainRAM_DISK_SECTORS         ( ( 5UL * 1024UL * 1024UL ) / mainRAM_DISK_SECTOR_SIZE )
/*#define mainRAM_DISK_SECTORS			( 150 ) */
#define mainIO_MANAGER_CACHE_SIZE    ( 15UL * mainRAM_DISK_SECTOR_SIZE )

/* Where the SD and RAM disks are mounted.  As set here the SD card disk is the
 * root directory, and the RAM disk appears as a directory off the root. */
#define mainSD_CARD_DISK_NAME        "/"
#define mainRAM_DISK_NAME            "/ram"

#define mainHAS_RAMDISK              1
#define mainHAS_SDCARD               1

static int doMountDisks = pdFALSE;

#define mainHAS_FAT_TEST          1

/* Set the following constants to 1 to include the relevant server, or 0 to
 * exclude the relevant server. */
#define mainCREATE_FTP_SERVER     1
#define mainCREATE_HTTP_SERVER    1

#define NTP_STACK_SIZE            256
#define NTP_PRIORITY              ( tskIDLE_PRIORITY + 3 )

/* The LED to toggle and the rate at which the LED will be toggled. */
#define mainLED_TOGGLE_PERIOD     pdMS_TO_TICKS( 250 )
#define mainLED                   0

/* Define names that will be used for DNS, LLMNR and NBNS searches. */
#define mainHOST_NAME             "RTOSDemo"
#define mainDEVICE_NICK_NAME      "zynq"

#ifndef ARRAY_SIZE
    #define ARRAY_SIZE( x )    ( int ) ( sizeof( x ) / sizeof( x )[ 0 ] )
#endif

/*-----------------------------------------------------------*/

#if ( ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM != 1 )
    #warning ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM must be '1'
#endif

#if ( ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM != 1 )
    #warning ipconfigDRIVER_INCLUDED_RX_IP_CHECKSUM must be '1'
#endif

static unsigned mustSendARP;

#if ( ipconfigMULTI_INTERFACE == 1 )
	extern void show_single_addressinfo( const char *pcFormat, const struct freertos_addrinfo * pxAddress );
	extern void show_addressinfo( const struct freertos_addrinfo * pxAddress );
#endif

BaseType_t xDoStartupTasks = pdFALSE;

void logHeapUsage( void );

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

extern void vStdioWithCWDTest( const char * pcMountPath );
extern void vCreateAndVerifyExampleFiles( const char * pcMountPath );

#if( ipconfigUSE_CALLBACKS != 0 )
static BaseType_t xOnUdpReceive( Socket_t xSocket, void * pvData, size_t xLength,
	const struct freertos_sockaddr *pxFrom, const struct freertos_sockaddr *pxDest );
#endif

/*
 * The task that runs the FTP and HTTP servers.
 */
static void prvServerWorkTask( void * pvParameters );
static void prvFTPTask( void * pvParameters );
static BaseType_t vHandleOtherCommand( Socket_t xSocket,
                                       char * pcBuffer );

void copy_file( const char * pcTarget,
                const char * pcSource );

/*
 * Callback function for the timer that toggles an LED.
 */
#if configUSE_TIMERS != 0
    static void prvLEDTimerCallback( TimerHandle_t xTimer );
#endif

#if ( ipconfigUSE_WOLFSSL != 0 )
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
 * defined here will be used if ipconfigUSE_DHCP is 0, or if ipconfigUSE_DHCP is
 * 1 but a DHCP server could not be contacted.  See the online documentation for
 * more information. */
#if ( ipconfigMULTI_INTERFACE == 1 )
    static const uint8_t ucIPAddress[ 4 ] = { configIP_ADDR0, configIP_ADDR1, configIP_ADDR2, configIP_ADDR3 };
#endif
/*static const uint8_t ucNetMask[ 4 ] = { configNET_MASK0, configNET_MASK1, configNET_MASK2, configNET_MASK3 }; */
/*static const uint8_t ucGatewayAddress[ 4 ] = { configGATEWAY_ADDR0, configGATEWAY_ADDR1, configGATEWAY_ADDR2, configGATEWAY_ADDR3 }; */
/*static const uint8_t ucDNSServerAddress[ 4 ] = { configDNS_SERVER_ADDR0, configDNS_SERVER_ADDR1, configDNS_SERVER_ADDR2, configDNS_SERVER_ADDR3 }; */
/*static const uint8_t ucDNSServerAddress[ 4 ] = {   8,   8,   8,   8 }; */
/*static const uint8_t ucDNSServerAddress[ 4 ] = { 192, 168,   2,   1 }; */
/*static const uint8_t ucDNSServerAddress[ 4 ] = { 118,  98,  44, 100 }; */

/* Default MAC address configuration.  The demo creates a virtual network
 * connection that uses this MAC address by accessing the raw Ethernet data
 * to and from a real network connection on the host PC.  See the
 * configNETWORK_INTERFACE_TO_USE definition for information on how to configure
 * the real network connection to use. */
#if ( ipconfigMULTI_INTERFACE == 0 )
    const uint8_t ucMACAddress[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };
#else
/*	static const uint8_t ucIPAddress2[ 4 ] = { 172, 16, 0, 100 }; */
/*	static const uint8_t ucNetMask2[ 4 ] = { 255, 255, 0, 0 }; */
/*	static const uint8_t ucGatewayAddress2[ 4 ] = { 0, 0, 0, 0}; */
/*	static const uint8_t ucDNSServerAddress2[ 4 ] = { 0, 0, 0, 0 }; */
/*	static const uint8_t ucMACAddress[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 }; */
    static const uint8_t ucMACAddress1[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 + 1 };
#endif /* ipconfigMULTI_INTERFACE */

/* Use by the pseudo random number generator. */
static UBaseType_t ulNextRand;

/* Handle of the task that runs the FTP and HTTP servers. */
static TaskHandle_t xServerWorkTaskHandle = NULL;
static TaskHandle_t xFTPTaskHandle = NULL;

static StreamBufferHandle_t streamBuffer;
static TaskHandle_t xStreamBufferTaskHandle = NULL;
static void prvStreamBufferTask( void * parameters );

/* FreeRTOS+FAT disks for the SD card and RAM disk. */
static FF_Disk_t * pxSDDisks[ ffconfigMAX_PARTITIONS ];
FF_Disk_t * pxRAMDisk;

int verboseLevel = 0;

/*#warning Just for testing */

#define dhcpCLIENT_HARDWARE_ADDRESS_LENGTH    16
#define dhcpSERVER_HOST_NAME_LENGTH           64
#define dhcpBOOT_FILE_NAME_LENGTH             128

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
extern void clearDHCPMessage( DHCPMessage_t * pxDHCPMessage );

extern void memset_test( void );

void memset_test()
{
/*	DHCPMessage_t *pxDHCPMessage; */
/*	uint8_t msg_space[ sizeof *pxDHCPMessage + 4 ]; */
/*	int offset; */
/*	for( offset = 0; offset < 4; offset++ ) */
/*	{ */
/*		pxDHCPMessage = ( DHCPMessage_t * ) ( msg_space + offset ); */
/*		clearDHCPMessage( pxDHCPMessage ); */
/*	} */
}

extern int use_event_logging;

#if ( ipconfigMULTI_INTERFACE != 0 )
    NetworkInterface_t * pxZynq_FillInterfaceDescriptor( BaseType_t xEMACIndex,
                                                         NetworkInterface_t * pxInterface );
#endif /* ipconfigMULTI_INTERFACE */
/*-----------------------------------------------------------*/

#if ( ipconfigMULTI_INTERFACE != 0 )
    static NetworkInterface_t xInterfaces[ 2 ];
    static NetworkEndPoint_t xEndPoints[ 4 ];
#endif /* ipconfigMULTI_INTERFACE */

int main( void )
{
//	configASSERT( ulPortCriticalNesting == 9999);

    #if configUSE_TIMERS != 0
        TimerHandle_t xLEDTimer;
    #endif

    /* Miscellaneous initialisation including preparing the logging and seeding
     * the random number generator. */
    prvMiscInitialisation();

    /* Initialise the network interface.
     *
     ***NOTE*** Tasks that use the network are created in the network event hook
     * when the network is connected and ready for use (see the definition of
     * vApplicationIPNetworkEventHook() below).  The address values passed in here
     * are used if ipconfigUSE_DHCP is set to 0, or if ipconfigUSE_DHCP is set to 1
     * but a DHCP server cannot be	contacted. */

    #if ( ipconfigMULTI_INTERFACE == 0 )
        const uint8_t ucIPAddress[ 4 ]        = { 192, 168, 2, 127 };
        const uint8_t ucNetMask[ 4 ]          = { 255, 255, 255, 0 };
        const uint8_t ucGatewayAddress[ 4 ]   = { 192, 168, 2, 1 };
        const uint8_t ucDNSServerAddress[ 4 ] = { 118, 98, 44, 100 };

		#if( ipconfigUSE_DHCP != 0 )
			uint32_t ulPreferredIPAddress = FreeRTOS_inet_addr_quick( 192, 168, 2, 127 );
			vDHCPSetPreferredIPAddress( ulPreferredIPAddress );
		#endif

        FreeRTOS_IPInit( ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress );
    #else
        pxZynq_FillInterfaceDescriptor( 0, &( xInterfaces[ 0 ] ) );

        /*
         * End-point-1  // private + public
         *     Network: 192.168.2.x/24
         *     IPv4   : 192.168.2.12
         *     Gateway: 192.168.2.1 ( NAT router )
         */
        NetworkEndPoint_t * pxEndPoint_0 = &( xEndPoints[ 0 ] );
        #if ( ipconfigUSE_IPv6 != 0 )
            NetworkEndPoint_t * pxEndPoint_1 = &( xEndPoints[ 1 ] );
            NetworkEndPoint_t * pxEndPoint_2 = &( xEndPoints[ 2 ] );
        #endif /* ( ipconfigUSE_IPv6 != 0 ) */
/*	NetworkEndPoint_t *pxEndPoint_3 = &( xEndPoints[ 3 ] ); */
        {
            const uint8_t ucIPAddress[ 4 ] = { 192, 168, 2, 127 };
            const uint8_t ucNetMask[ 4 ] = { 255, 255, 255, 0 };
            const uint8_t ucGatewayAddress[ 4 ] = { 192, 168, 2, 1 };
            const uint8_t ucDNSServerAddress[ 4 ] = { 118, 98, 44, 100 };
            #if ( ipconfigENDPOINT_DNS_ADDRESS_COUNT < 3 )
            #error This setup needs at least 3 DNS addresses
            #endif
            FreeRTOS_FillEndPoint( &( xInterfaces[ 0 ] ), pxEndPoint_0, ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress1 );

            FreeRTOS_inet_pton( FREERTOS_AF_INET, "118.98.44.100", &( pxEndPoint_0->ipv4_defaults.ulDNSServerAddresses[ 0 ] ) );
            FreeRTOS_inet_pton( FREERTOS_AF_INET, "180.250.245.182", &( pxEndPoint_0->ipv4_defaults.ulDNSServerAddresses[ 1 ] ) );
            FreeRTOS_inet_pton( FREERTOS_AF_INET, "180.250.245.178", &( pxEndPoint_0->ipv4_defaults.ulDNSServerAddresses[ 2 ] ) );
            memcpy( pxEndPoint_0->ipv4_settings.ulDNSServerAddresses, pxEndPoint_0->ipv4_defaults.ulDNSServerAddresses, sizeof pxEndPoint_0->ipv4_settings.ulDNSServerAddresses );

            #if ( ipconfigUSE_DHCP != 0 )
                {
//                    pxEndPoint_0->bits.bWantDHCP = pdTRUE;
                }
            #endif /* ( ipconfigUSE_DHCP != 0 ) */
        }
/*#warning please include this end-point again. */
/*	{ */
/*		const uint8_t ucIPAddress[]        = { 172,  16,   0, 114 }; */
/*		const uint8_t ucNetMask[]          = { 255, 255,   0,   0}; */
/*		const uint8_t ucGatewayAddress[]   = {   0,   0,   0,   0}; */
/*		const uint8_t ucDNSServerAddress[] = {   0,   0,   0,   0}; */
/*		FreeRTOS_FillEndPoint( &( xInterfaces[0] ), pxEndPoint_3, ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress1 ); */
/*	} */


        #if ( ipconfigUSE_IPv6 != 0 )
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
                    /* a573:8::5000:0:d8ff:120 */
                    /* a8ff:120::593b:200:d8af:20 */

                    FreeRTOS_inet_pton6( "fe80::", xPrefix.ucBytes );
/*			FreeRTOS_inet_pton6( "2001:470:ec54::", xPrefix.ucBytes ); */

/*			Take a random IP-address */
/*			FreeRTOS_CreateIPv6Address( &xIPAddress, &xPrefix, 64, pdTRUE ); */

/*			Take a fixed IP-address, which is easier for testing. */
                    /*FreeRTOS_inet_pton6( "fe80::b865:6c00:6615:6e50", xIPAddress.ucBytes ); */
                    FreeRTOS_inet_pton6( "fe80::7009", xIPAddress.ucBytes );

                    FreeRTOS_FillEndPoint_IPv6( &( xInterfaces[ 0 ] ),
                                                pxEndPoint_1,
                                                &( xIPAddress ),
                                                &( xPrefix ),
                                                64uL, /* Prefix length. */
                                                NULL, /* No gateway */
                                                NULL, /* pxDNSServerAddress: Not used yet. */
                                                ucMACAddress1 );
                }

                /*
                 * End-point-3  // public
                 *     Network: 2001:470:ec54::/64
                 *     IPv6   : 2001:470:ec54::4514:89d5:4589:8b79/128
                 *     Gateway: fe80::ba27:ebff:fe5a:d751  // obtained from Router Advertisement
                 */
                {
                    IPv6_Address_t xIPAddress;
                    IPv6_Address_t xPrefix;
                    IPv6_Address_t xGateWay;
                    IPv6_Address_t xDNSServer;

                    FreeRTOS_inet_pton6( "2001:470:ec54::", xPrefix.ucBytes );
                    FreeRTOS_inet_pton6( "2001:4860:4860::8888", xDNSServer.ucBytes );

                    FreeRTOS_CreateIPv6Address( &xIPAddress, &xPrefix, 64, pdTRUE );
                    FreeRTOS_inet_pton6( "fe80::ba27:ebff:fe5a:d751", xGateWay.ucBytes );

                    FreeRTOS_FillEndPoint_IPv6( &( xInterfaces[ 0 ] ),
                                                pxEndPoint_2,
                                                &( xIPAddress ),
                                                &( xPrefix ),
                                                64uL,            /* Prefix length. */
                                                &( xGateWay ),
                                                &( xDNSServer ), /* pxDNSServerAddress: Not used yet. */
                                                ucMACAddress1 );
                    #if ( ipconfigUSE_RA != 0 )
                        {
                            pxEndPoint_2->bits.bWantRA = pdTRUE_UNSIGNED;
                        }
                    #endif /* #if( ipconfigUSE_RA != 0 ) */
                    #if ( ipconfigUSE_DHCPv6 != 0 )
                        {
							pxEndPoint_2->bits.bWantDHCP = pdTRUE_UNSIGNED;
                        }
                    #endif /* ( ipconfigUSE_DHCP != 0 ) */
                }
            }
        #endif /* if ( ipconfigUSE_IPv6 != 0 ) */

        FreeRTOS_IPStart();
    #endif /* ipconfigMULTI_INTERFACE */

    memset_test();

    /* Custom task for testing. */
    xTaskCreate( prvServerWorkTask, "SvrWork", mainTCP_SERVER_STACK_SIZE, NULL, 2, &xServerWorkTaskHandle );
    xTaskCreate( prvFTPTask,        "FTPTask", mainTCP_SERVER_STACK_SIZE, NULL, 2, &xFTPTaskHandle );

    /* Create a timer that just toggles an LED to show something is alive. */
    #if configUSE_TIMERS != 0
        {
/*		xLEDTimer = xTimerCreate( "LED", mainLED_TOGGLE_PERIOD, pdTRUE, NULL, prvLEDTimerCallback ); */
/*		configASSERT( xLEDTimer ); */
/*		xTimerStart( xLEDTimer, 0 ); */
        }
    #endif
    #if ( USE_LOG_EVENT != 0 )
        {
			iEventLogInit();
            eventLogAdd( "Program started" );
        }
    #endif

    /* Start the RTOS scheduler. */
    FreeRTOS_debug_printf( ( "vTaskStartScheduler\n" ) );
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

static void prvCreateDiskAndExampleFiles( void )
{
    #if ( mainHAS_RAMDISK != 0 )
        static uint8_t ucRAMDisk[ mainRAM_DISK_SECTORS * mainRAM_DISK_SECTOR_SIZE ];
    #endif

    verboseLevel = 0;
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
    #if ( mainHAS_SDCARD != 0 )
        {
            FreeRTOS_printf( ( "Mount SD-card\n" ) );
            /* Create the SD card disk. */
            xMountFailIgnore = 1;
            pxSDDisks[ 0 ] = FF_SDDiskInit( mainSD_CARD_DISK_NAME );
            xMountFailIgnore = 0;

            if( pxSDDisks[ 0 ] != NULL )
            {
                FF_IOManager_t * pxIOManager = sddisk_ioman( pxSDDisks[ 0 ] );
                uint64_t ullFreeBytes =
                    ( uint64_t ) pxIOManager->xPartition.ulFreeClusterCount * pxIOManager->xPartition.ulSectorsPerCluster * 512ull;
                FreeRTOS_printf( ( "Volume %-12.12s\n",
                                   pxIOManager->xPartition.pcVolumeLabel ) );
                FreeRTOS_printf( ( "Free clusters %u total clusters %u Free %u KB\n",
                                   ( unsigned ) pxIOManager->xPartition.ulFreeClusterCount,
                                   ( unsigned ) pxIOManager->xPartition.ulNumClusters,
                                   ( unsigned ) ( ullFreeBytes / 1024ull ) ) );
                #if ( ffconfigMAX_PARTITIONS == 1 )
                    /*#warning Only 1 partition defined */
                #else
                    for( int partition = 1; partition < ARRAY_SIZE( pxSDDisks ); partition++ )
                    {
                        char name[ 32 ];
                        xDiskPartition = partition;
                        snprintf( name, sizeof name, "/part_%d", partition );

                        if( xDiskPartition >= 2 )
                        {
                            FreeRTOS_printf( ( "Part-%u\n", xDiskPartition ) );
                        }

                        pxSDDisks[ partition ] = FF_SDDiskInit( name );
                    }
                #endif /* if ( ffconfigMAX_PARTITIONS == 1 ) */
            }

            FreeRTOS_printf( ( "Mount SD-card done\n" ) );
        }
    #endif /* mainHAS_SDCARD */

/*	/ * Create a few example files on the disk.  These are not deleted again. * / */
/*	vCreateAndVerifyExampleFiles( mainRAM_DISK_NAME ); */
/*	vStdioWithCWDTest( mainRAM_DISK_NAME ); */
}
/*-----------------------------------------------------------*/

/*#if defined(CONFIG_USE_LWIP) */
/*	#undef CONFIG_USE_LWIP */
/*	#define CONFIG_USE_LWIP    0 */
/*#endif */

#if ( CONFIG_USE_LWIP != 0 )

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

    void lwip_init( void );

#endif /* CONFIG_USE_LWIP */

#if ( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) )

    #define STACK_WEBSERVER_TASK    ( 512 + 256 )
    #define PRIO_WEBSERVER_TASK     2

    void FreeRTOS_netstat( void );

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

    #if ( CONFIG_USE_LWIP != 0 )

        unsigned char ucLWIP_Mac_Address[] = { 0x00, 0x0a, 0x35, 0x00, 0x01, 0x02 };

        static err_t xemacpsif_output( struct netif * netif,
                                       struct pbuf * p,
                                       struct ip_addr * ipaddr )
        {
            /* resolve hardware address, then send (or queue) packet */
            return etharp_output( netif, p, ipaddr );
        }

        static err_t low_level_output( struct netif * netif,
                                       struct pbuf * p )
        {
            NetworkBufferDescriptor_t * pxBuffer;
            const unsigned char * pcPacket = ( const unsigned char * ) p->payload;

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

        err_t xemacpsif_init( struct netif * netif )
        {
/* Define those to better describe your network interface. */
        #define IFNAME0    't'
        #define IFNAME1    'e'
            netif->name[ 0 ] = IFNAME0;
            netif->name[ 1 ] = IFNAME1;
            netif->output = xemacpsif_output;
            netif->linkoutput = low_level_output;

            return ERR_OK;
        }

        struct netif server_netif;
        extern volatile struct netif * curr_netif;
        static void vLWIP_Init( void )
        {
            struct netif * netif = &server_netif;
            struct ip_addr ipaddr, netmask, gw;
            err_t rc;

            memset( &server_netif, '\0', sizeof server_netif );
            lwip_raw_init();
            lwip_init();

            /* initliaze IP addresses to be used */
            IP4_ADDR( &ipaddr, 192, 168, 2, 10 );
            IP4_ADDR( &netmask, 255, 255, 255, 0 );
            IP4_ADDR( &gw, 192, 168, 2, 1 );

            /*
             * struct netif *netif_add(struct netif *netif, ip_addr_t *ipaddr, ip_addr_t *netmask,
             * ip_addr_t *gw, void *state, netif_init_fn init, netif_input_fn input);
             */

            /* Add network interface to the netif_list, and set it as default */
            struct netif * rc_netif = netif_add( netif, &ipaddr, &netmask, &gw,
                                                 ( void * ) ucLWIP_Mac_Address,
                                                 xemacpsif_init, tcpip_input );

            if( rc_netif == NULL )
            {
                FreeRTOS_printf( ( "Error adding N/W interface\r\n" ) );
            }
            else
            {
                netif_set_default( netif );

                /* specify that the network if is up */
                netif_set_up( netif );
                netif->flags |= NETIF_FLAG_ETHERNET | NETIF_FLAG_ETHARP | NETIF_FLAG_UP | NETIF_FLAG_BROADCAST;
                memset( netif->hwaddr, '\0', sizeof netif->hwaddr );
                memcpy( netif->hwaddr, ucLWIP_Mac_Address, 6 );
                netif->hwaddr_len = ETHARP_HWADDR_LEN;

                curr_netif = netif;

                xTaskCreate( lwip_echo_application_thread, "lwip_web", STACK_WEBSERVER_TASK, NULL, PRIO_WEBSERVER_TASK, NULL );
                xTaskCreate( plus_echo_application_thread, "plus_web", STACK_WEBSERVER_TASK, NULL, PRIO_WEBSERVER_TASK, NULL );
            }
        }
    #endif /* CONFIG_USE_LWIP */

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

    #if ( USE_TELNET != 0 )
        Telnet_t * pxTelnetHandle = NULL;

        void vUDPLoggingHook( const char * pcMessage,
                              BaseType_t xLength )
        {
            if( ( pxTelnetHandle != NULL ) && ( pxTelnetHandle->xClients != NULL ) )
            {
                xTelnetSend( pxTelnetHandle, NULL, pcMessage, xLength );
            }

            /*	XUartPs_Send( &uartInstance, ( u8 * ) pcMessage, ( u32 ) xLength ); */
        }
    #endif /* if ( USE_TELNET != 0 ) */

    extern void plus_echo_client_thread( void * parameters );
    extern void vStartSimple2TCPServerTasks( uint16_t usStackSize,
                                             UBaseType_t uxPriority );

    NetworkBufferDescriptor_t * pxDescriptor = NULL;
    SemaphoreHandle_t xServerSemaphore;
	TickType_t uxNeedSleep = 0;
    static void prvFTPTask( void *pvParameter )
	{
        /* A structure that defines the servers to be created.  Which servers are
         * included in the structure depends on the mainCREATE_HTTP_SERVER and
         * mainCREATE_FTP_SERVER settings at the top of this file. */
        TCPServer_t * pxTCPServer = NULL;
        const TickType_t xInitialBlockTime = pdMS_TO_TICKS( 2U );
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
		( void ) pvParameter;
        ulTaskNotifyTake( pdTRUE, portMAX_DELAY );

/* Create the servers defined by the xServerConfiguration array above. */
        pxTCPServer = FreeRTOS_CreateTCPServer( xServerConfiguration, sizeof( xServerConfiguration ) / sizeof( xServerConfiguration[ 0 ] ) );
        configASSERT( pxTCPServer );

		for( ;; )
		{
            if( pxTCPServer )
            {
                FreeRTOS_TCPServerWork( pxTCPServer, xInitialBlockTime );
				if( uxNeedSleep )
				{
					TickType_t ux = pdMS_TO_TICKS( uxNeedSleep );
					FreeRTOS_printf( ( "Start sleep %u ms\n", uxNeedSleep ) );
					uxNeedSleep = 0U;
					vTaskDelay( ux );
					FreeRTOS_printf( ( "Done\n" ) );
				}
            }
			else
			{
				vTaskDelay( 2000 );
			}
		}
	}

#if( ipconfigUSE_PCAP != 0 )
	#include "win_pcap.h"
	
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
		if( strncmp( pcBuffer, "stop", 4 )  == 0 )
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

		if( pcapRunning )
		{
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

    static void prvServerWorkTask( void * pvParameters )
    {
        {
            /* May 3, 2012 */
            time_t xTime = 1620027433U;
            FreeRTOS_settime( &( xTime ) );
        }

        xServerSemaphore = xSemaphoreCreateBinary();
        configASSERT( xServerSemaphore != NULL );

        /* Remove compiler warning about unused parameter. */
        ( void ) pvParameters;

        #if ( configUSE_TASK_FPU_SUPPORT == 1 )
            {
                portTASK_USES_FLOATING_POINT();
            }
        #endif /* configUSE_TASK_FPU_SUPPORT == 1 */

        FreeRTOS_printf( ( "Creating files\n" ) );

        /* Create the RAM disk used by the FTP and HTTP servers. */
        /*prvCreateDiskAndExampleFiles(); */

/*
 *
 #if( ipconfigUSE_WOLFSSL != 0 )
 *  {
 *      if( ( pxSDDisks[ 0 ] != NULL ) || ( pxRAMDisk != NULL ) )
 *      {
 *          FreeRTOS_printf( ( "Calling vStartWolfServerTask\n" ) );
 *          vStartWolfServerTask();
 *      }
 *  }
 #endif // ipconfigUSE_WOLFSSL
 */

        /* The priority of this task can be raised now the disk has been
         * initialised. */
        vTaskPrioritySet( NULL, mainTCP_SERVER_TASK_PRIORITY );

        /* If the CLI is included in the build then register commands that allow
         * the file system to be accessed. */
        #if ( mainCREATE_UDP_CLI_TASKS == 1 )
            {
                vRegisterFileSystemCLICommands();
            }
        #endif /* mainCREATE_UDP_CLI_TASKS */

        vParTestInitialise();

        /* Wait until the network is up before creating the servers.  The
         * notification is given from the network event hook. */
        ulTaskNotifyTake( pdTRUE, portMAX_DELAY );

        #if ( CONFIG_USE_LWIP != 0 )
            {
                vLWIP_Init();
            }
        #endif
        #if ( USE_TCP_TESTER != 0 )
            {
                tcp_test_start();
            }
        #endif

        vStartSimple2TCPServerTasks( 1024u, 2u );

        /*xTaskCreate (plus_echo_client_thread, "echo_client", STACK_WEBSERVER_TASK, NULL, PRIO_WEBSERVER_TASK, NULL); */
        #if ( USE_TELNET != 0 )
            Telnet_t xTelnet;
            memset( &xTelnet, '\0', sizeof xTelnet );
        #endif

        for( ; ; )
        {
            char pcBuffer[ 129 ];
            BaseType_t xCount;
            static BaseType_t xHadSocket = pdFALSE;
            TickType_t xReceiveTimeOut = pdMS_TO_TICKS( 0 );

            xSemaphoreTake( xServerSemaphore, pdMS_TO_TICKS( 200 ) );

            if( xDoStartupTasks != pdFALSE )
            {
                xDoStartupTasks = pdFALSE;

                /* Tasks that use the TCP/IP stack can be created here. */
                /* Start a new task to fetch logging lines and send them out. */
                vUDPLoggingTaskCreate();
            }

            #if ( configUSE_TIMERS == 0 )
                {
                    /* As there is not Timer task, toggle the LED 'manually'. */
                    vParTestToggleLED( mainLED );
                }
            #endif
            xSocket_t xSocket = xLoggingGetSocket();
            struct freertos_sockaddr xSourceAddress;
            socklen_t xSourceAddressLength = sizeof( xSourceAddress );

            #if ( CONFIG_USE_LWIP != 0 )
                {
                    curr_netif = &( server_netif );
                }
            #endif

            #if USE_TCP_DEMO_CLI
                {
                    /* Let the CLI task do its regular work. */
                    xHandleTesting();
                }
            #endif

            if( xSocket == NULL )
            {
                continue;
            }

			pcap_check();

			//if( mustSendARP > 0 )
			{
				static BaseType_t xLast = 0; 
				BaseType_t xNew = ParTestReadButton();
				if( xLast != xNew )
				{
					xLast = xNew;
					FreeRTOS_printf( ( "Send ARP\n" ) );
					vARPSendGratuitous();
				}
				mustSendARP--;
			}
            if( xHadSocket == pdFALSE )
            {
                xHadSocket = pdTRUE;
                FreeRTOS_printf( ( "prvCommandTask started\n" ) );

                FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof( xReceiveTimeOut ) );
                FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_SET_SEMAPHORE, ( void * ) &xServerSemaphore, sizeof( xServerSemaphore ) );

                #if ( USE_TELNET != 0 )
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

            if( doMountDisks )
            {
                doMountDisks = pdFALSE;
                prvCreateDiskAndExampleFiles();
            }

            if( pxDescriptor != NULL )
            {
                vReleaseNetworkBufferAndDescriptor( BUFFER_FROM_WHERE_CALL( "main.c" ) pxDescriptor );
                pxDescriptor = NULL;
            }

			logHeapUsage();

            char * ppcBuffer;
            xCount = FreeRTOS_recvfrom( xSocket, ( void * ) &ppcBuffer, sizeof( pcBuffer ) - 1, FREERTOS_ZERO_COPY, &xSourceAddress, &xSourceAddressLength );

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

            #if ( USE_TELNET != 0 )
                {
                    if( ( xCount <= 0 ) && ( xTelnet.xParentSocket != NULL ) )
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

				if (pcBuffer[0] == 'f' && isdigit (pcBuffer[1]) && pcBuffer[2] == ' ') {
					// Ignore messages like "f4 dnsq"
					if (pcBuffer[1] != '9')
						continue;
					xCount -= 3;
					// "f9 ver"
					memmove(pcBuffer, pcBuffer+3, xCount + 1);
				}

                /* Strip any line terminators. */
                while( ( xCount > 0 ) && ( ( pcBuffer[ xCount - 1 ] == 13 ) || ( pcBuffer[ xCount - 1 ] == 10 ) ) )
                {
                    pcBuffer[ --xCount ] = 0;
                }

                if( xHandleTestingCommand( pcBuffer, sizeof( pcBuffer ) ) == pdTRUE )
                {
                    /* Command has been handled. */
                }
                else if( vHandleOtherCommand( xSocket, pcBuffer ) == pdTRUE )
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
#endif /* if ( ( mainCREATE_FTP_SERVER == 1 ) || ( mainCREATE_HTTP_SERVER == 1 ) ) */

static BaseType_t vHandleOtherCommand( Socket_t xSocket,
                                       char * pcBuffer )
{
    /* Assume that the command will be handled. */
    BaseType_t xReturn = pdTRUE;

    ( void ) xSocket;

    if( strncmp( pcBuffer, "zynq ddos", 9 ) == 0 )
    {
        char * ptr = pcBuffer + 9;

        while( *ptr && isspace( *ptr ) )
        {
            ptr++;
        }

        if( *ptr != 0 )
        {
            vDDoSCommand( ptr );
        }
    }
	else if( strncmp( pcBuffer, "arpclear", 8 ) == 0 )
	{
		FreeRTOS_printf( ( "Clearing ARP table\n" ) );
		#if ( ipconfigMULTI_INTERFACE != 0 )
			FreeRTOS_ClearARP( NULL );
		#else
			FreeRTOS_ClearARP( );
		#endif
	}
	else if( strncmp( pcBuffer, "arp", 3 ) == 0 )
	{
		FreeRTOS_PrintARPCache();
	}
	else if( strncmp( pcBuffer, "tcp_client", 10 ) == 0 )
	{
		FreeRTOS_printf( ( "Start tcp_client\n" ) );
		tcp_client();
	}

    #if ( USE_NTOP_TEST != 0 )
        else if( strncmp( pcBuffer, "ntop", 4 ) == 0 )
        {
            FreeRTOS_printf( ( "Starting ntop test\n" ) );
            inet_pton_ntop_tests();
        }
    #endif

    else if( strncmp( pcBuffer, "zynq client", 11 ) == 0 )
    {
        char * ptr = pcBuffer + 11;

        while( *ptr && isspace( *ptr ) )
        {
            ptr++;
        }

        if( *ptr != 0 )
        {
            vTCPClientCommand( ptr );
        }
    }

    else if( strncmp( pcBuffer, "TESTUDP", 7 ) == 0 )
    {
        if( pxDescriptor != NULL )
        {
            const uint8_t * puc = pxDescriptor->pucEthernetBuffer;
            UDPPacket_t * pxPacket = ( UDPPacket_t * ) puc;
            uint16_t usIPChecksum = FreeRTOS_ntohs( pxPacket->xIPHeader.usHeaderChecksum );
            uint16_t usProtChecksum = FreeRTOS_ntohs( pxPacket->xUDPHeader.usChecksum );
            FreeRTOS_printf( ( "Xilinx : Checksum IP %04X PR %04X [ %xip -> %xip ] (%s)\n",
                               usIPChecksum,
                               usProtChecksum,
                               FreeRTOS_ntohl( pxPacket->xIPHeader.ulSourceIPAddress ),
                               FreeRTOS_ntohl( pxPacket->xIPHeader.ulDestinationIPAddress ),
                               pcBuffer ) );
        }
        else
        {
            FreeRTOS_printf( ( "Xilinx : recv packet '%s'\n", pcBuffer ) );
        }
    }

    else if( strncmp( pcBuffer, "playpcap", 8 ) == 0 )
    {
        static const char pcap_fname[] = "/pcdata_tail.pcap";
        const char * fname = pcap_fname;
        char * ptr = pcBuffer + 8;

        while( isspace( *ptr ) )
        {
            ptr++;
        }

        if( *ptr )
        {
            fname = ptr;
        }

        /*static const char pcap_fname[] = "/pcdata_test.pcap"; */
        vPCAPLivePlay( fname );
    }

#if( ipconfigUSE_PCAP != 0 )
    else if( strncmp( pcBuffer, "pcap", 4 ) == 0 )
	{
		FreeRTOS_printf( ( "PCAP: %s\n", pcBuffer ) );
		pcap_command( pcBuffer + 4 );
	}
#endif

    else if( strncmp( pcBuffer, "xarp", 4 ) == 0 )
    {
        vARPSendGratuitous();
    }
    else if( strncmp( pcBuffer, "memtest", 7 ) == 0 )
    {
        memcpy_test();
    }
    #if ( ipconfigUSE_TCP_NET_STAT != 0 )
        else if( strncasecmp( pcBuffer, "defender", 8 ) == 0 )
        {
            MetricsType_t xMetrics;
            vGetMetrics( &xMetrics );
            vShowMetrics( &xMetrics );
        }
    #endif
    else if( strncasecmp( pcBuffer, "fcopy", 5 ) == 0 )
    {
        copy_file( "/scratch.txt", "/file_1mb.txt" );
    }

    else if( strncasecmp( pcBuffer, "fseek", 5 ) == 0 )
    {
        extern void seek_test( unsigned size );
        unsigned size = 4096;

        if( isdigit( pcBuffer[ 5 ] ) )
        {
            sscanf( pcBuffer, "%u", &( size ) );
            seek_test( size );
        }
        else
        {
            seek_test( 1U * 1024U );
            seek_test( 2U * 1024U );
            seek_test( 4U * 1024U );
            seek_test( 8U * 1024U );
            seek_test( 16U * 1024U );
        }
    }

    else if( strncasecmp( pcBuffer, "partition", 9 ) == 0 )
    {
        if( pxSDDisks[ 0 ] != NULL )
        {
            FF_PartitionParameters_t partitions;
            uint32_t ulNumberOfSectors = pxSDDisks[ 0 ]->ulNumberOfSectors;
            memset( &partitions, 0x00, sizeof( partitions ) );
            partitions.ulSectorCount = ulNumberOfSectors;
            /* ulHiddenSectors is the distance between the start of the partition and */
            /* the BPR ( Partition Boot Record ). */
            /* It may not be zero. */
            partitions.ulHiddenSectors = 8;
            partitions.ulInterSpace = 0;
            partitions.xPrimaryCount = ffconfigMAX_PARTITIONS < 4 ? ffconfigMAX_PARTITIONS : 3;

            partitions.eSizeType = eSizeIsPercent;

            int iPercentagePerPartition = 100 / ffconfigMAX_PARTITIONS;

            if( iPercentagePerPartition > 33 )
            {
                iPercentagePerPartition = 33;
            }

            for( size_t i = 0; i < ffconfigMAX_PARTITIONS; i++ )
            {
                partitions.xSizes[ i ] = iPercentagePerPartition;
            }

            FF_Error_t partition_err = FF_Partition( pxSDDisks[ 0 ], &partitions );
            FF_FlushCache( pxSDDisks[ 0 ]->pxIOManager );

            FreeRTOS_printf( ( "FF_Partition: 0x%08lX (errno %d) Number of sectors %lu\n", partition_err, prvFFErrorToErrno( partition_err ), ulNumberOfSectors ) );

            if( !FF_isERR( partition_err ) )
            {
                for( size_t partNr = 0; partNr < ffconfigMAX_PARTITIONS; partNr++ )
                {
                    char partName[ 12 ];
                    snprintf( partName, sizeof partName, "part_%d     ", partNr );
                    FF_Error_t err_format = FF_FormatDisk( pxSDDisks[ 0 ], partNr, pdFALSE, pdFALSE, partName );
                    FreeRTOS_printf( ( "FF_FormatDisk[%d]: 0x%08lX (errno %d)\n", partNr, err_format, prvFFErrorToErrno( err_format ) ) );

                    if( FF_isERR( err_format ) )
                    {
                        FreeRTOS_printf( ( "Stopping.\n" ) );
                        break;
                    }
                }
            }
        }
    }

    else if( strncasecmp( pcBuffer, "format", 6 ) == 0 )
    {
        int partition = 0;
        char * ptr = pcBuffer + 6;

        while( isspace( *ptr ) )
        {
            ptr++;
        }

        if( ( sscanf( ptr, "%d", &partition ) == 1 ) &&
            ( partition >= 0 ) &&
            ( partition < ffconfigMAX_PARTITIONS ) )
        {
            FF_Disk_t * disk = NULL;

            if( ( partition >= 0 ) && ( partition < ARRAY_SIZE( pxSDDisks ) ) )
            {
                disk = pxSDDisks[ partition ];
            }

            if( disk == NULL )
            {
                FF_PRINTF( "No handle for partition %d\n", partition );
            }
            else
            {
                FF_SDDiskUnmount( disk );
                {
                    /* Format the drive */
                    int xError = FF_Format( disk, partition, pdFALSE, pdFALSE ); /* Try FAT32 with large clusters */

                    if( FF_isERR( xError ) )
                    {
                        FF_PRINTF( "FF_SDDiskFormat: %s\n", ( const char * ) FF_GetErrMessage( xError ) );
                    }
                    else
                    {
                        FF_PRINTF( "FF_SDDiskFormat: OK, now remounting\n" );
                        disk->xStatus.bPartitionNumber = partition;
                        xError = FF_SDDiskMount( disk );
                        FF_PRINTF( "FF_SDDiskFormat: rc %08x\n", ( unsigned ) xError );

                        if( FF_isERR( xError ) == pdFALSE )
                        {
                            FF_SDDiskShowPartition( disk );
                        }
                    }
                }
            }
        }
    }

    else if( strncasecmp( pcBuffer, "memset", 6 ) == 0 )
    {
        memset_test();
    }

    else if( strncasecmp( pcBuffer, "ff_free", 7 ) == 0 )
    {
        ff_chdir( "/ram" );
        ff_mkdir( "mydir" MKDIR_FALSE );
        ff_chdir( "mydir" );
        ff_free_CWD_space();
    }

    else if( strncmp( pcBuffer, "msgs", 4 ) == 0 )
    {
        if( streamBuffer == NULL )
        {
            /*streamBuffer = xStreamBufferCreate( 256, 1 / * xTriggerLevelBytes * / ); */
            streamBuffer = xMessageBufferCreate( 256 );
            FreeRTOS_printf( ( "Created buffer\n" ) );
        }

        if( xStreamBufferTaskHandle == NULL )
        {
            /* Custom task for testing. */
            xTaskCreate( prvStreamBufferTask, "StreamBuf", mainTCP_SERVER_STACK_SIZE, NULL, 2, &xStreamBufferTaskHandle );
            FreeRTOS_printf( ( "Created task\n" ) );
        }

        char * tosend = pcBuffer + 4;
        int len = strlen( tosend );

        if( len )
        {
            size_t rc = xStreamBufferSend( streamBuffer, tosend, len, 1000 );
/*							size_t rc = xMessageBufferSend( streamBuffer, tosend, len, 1000 ); */
            FreeRTOS_printf( ( "Sending data: '%s' len = %d (%lu)\n", tosend, len, rc ) );
        }
    }

    else if( strncmp( pcBuffer, "dnss", 4 ) == 0 )
    {
        char * ptr = pcBuffer + 4;

        while( isspace( *ptr ) )
        {
            ptr++;
        }

        if( *ptr )
        {
            unsigned u32[ 4 ];
            char ch;
            int rc = sscanf( ptr, "%u%c%u%c%u%c%u",
                             u32 + 0,
                             &ch,
                             u32 + 1,
                             &ch,
                             u32 + 2,
                             &ch,
                             u32 + 3 );

            if( rc >= 7 )
            {
                uint32_t address = FreeRTOS_inet_addr_quick( u32[ 0 ], u32[ 1 ], u32[ 2 ], u32[ 3 ] );
                FreeRTOS_printf( ( "DNS server at %xip\n", FreeRTOS_ntohl( address ) ) );
                /* pxEndPoint, pulIPAddress, pulNetMask, pulGatewayAddress, pulDNSServerAddress*/
                #if ( ipconfigMULTI_INTERFACE != 0 )
                    {
                        FreeRTOS_SetEndPointConfiguration( NULL, NULL, NULL, &( address ), NULL );
                    }
                #else
                    {
                        FreeRTOS_SetAddressConfiguration( NULL, NULL, NULL, &( address ) );
                    }
                #endif /* ( ipconfigMULTI_INTERFACE != 0 ) */
            }
        }

        uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;

        #if ( ipconfigMULTI_INTERFACE == 0 )
            {
                FreeRTOS_GetAddressConfiguration( &ulIPAddress, &ulNetMask, &ulGatewayAddress, &ulDNSServerAddress );
            }
        #else
            {
                FreeRTOS_GetEndPointConfiguration( &ulIPAddress, &ulNetMask, &ulGatewayAddress, &ulDNSServerAddress, NULL );
            }
        #endif /* ( ipconfigMULTI_INTERFACE != 0 ) */

        FreeRTOS_printf( ( "IP-address = %lxip\n", FreeRTOS_ntohl( ulIPAddress ) ) );
        FreeRTOS_printf( ( "Net mask   = %lxip\n", FreeRTOS_ntohl( ulNetMask ) ) );
        FreeRTOS_printf( ( "Gateway    = %lxip\n", FreeRTOS_ntohl( ulGatewayAddress ) ) );
        FreeRTOS_printf( ( "DNS server = %lxip\n", FreeRTOS_ntohl( ulDNSServerAddress ) ) );
    }

    #if ( ipconfigMULTI_INTERFACE != 0 )
        else if( strncmp( pcBuffer, "dnsl", 4 ) == 0 )
        {
            FreeRTOS_printf( ( "DNS = %xip / %xip / %xip\n",
                               xEndPoints[ 0 ].ipv4_settings.ulDNSServerAddresses[ 0 ],
                               xEndPoints[ 0 ].ipv4_settings.ulDNSServerAddresses[ 1 ],
                               xEndPoints[ 0 ].ipv4_settings.ulDNSServerAddresses[ 2 ] ) );
            FreeRTOS_printf( ( "DNS = %xip / %xip / %xip\n",
                               xEndPoints[ 0 ].ipv4_defaults.ulDNSServerAddresses[ 0 ],
                               xEndPoints[ 0 ].ipv4_defaults.ulDNSServerAddresses[ 1 ],
                               xEndPoints[ 0 ].ipv4_defaults.ulDNSServerAddresses[ 2 ] ) );
        }
    #endif /* if ( ipconfigMULTI_INTERFACE != 0 ) */

    #if ( ipconfigUSE_WOLFSSL != 0 )
        else if( strncasecmp( pcBuffer, "wolfssl", 4 ) == 0 )
        {
            if( ( pxSDDisks[ 0 ] != NULL ) || ( pxRAMDisk != NULL ) )
            {
                FreeRTOS_printf( ( "Calling vStartWolfServerTask\n" ) );
                vStartWolfServerTask();
            }
        }
    #endif

    #if ( USE_IPERF != 0 )
        else if( strncmp( pcBuffer, "iperf", 5 ) == 0 )
        {
            extern BaseType_t xTCPWindowLoggingLevel;
            xTCPWindowLoggingLevel = 1;
            vIPerfInstall();
        }
    #endif /* ( USE_IPERF != 0 ) */
    else if( strncmp( pcBuffer, "pull", 4 ) == 0 )
    {
#define SD_DAT0    42
#define SD_DAT1    43
#define SD_DAT2    44
#define SD_DAT3    45
        uint32_t ulValue, ulOldValues[ 4 ];
        int index, IsLock;
        extern uint32_t ulSDWritePause;

        for( index = 0; index < 4; index++ )
        {
            ulOldValues[ index ] = Xil_In32( ( XSLCR_MIO_PIN_00_ADDR + ( ( SD_DAT0 + index ) * 4U ) ) );
        }

        if( pcBuffer[ 4 ] == '1' )
        {
            ulValue = 0x1280;
        }
        else
        {
            ulValue = 0x0280;
        }

        ulSDWritePause = 0;

        IsLock = XQspiPs_In32( XPAR_XSLCR_0_BASEADDR + SLCR_LOCKSTA );

        if( IsLock )
        {
            XQspiPs_Out32( XPAR_XSLCR_0_BASEADDR + SLCR_UNLOCK, SLCR_UNLOCK_MASK );
        }

        Xil_Out32( ( XSLCR_MIO_PIN_00_ADDR + ( SD_DAT0 * 4U ) ), ulValue );

        if( IsLock )
        {
            XQspiPs_Out32( XPAR_XSLCR_0_BASEADDR + SLCR_LOCK, SLCR_LOCK_MASK );
        }

        /* MIO config DATA0 0280 -> 0680 */
        FreeRTOS_printf( ( "MIO config DATA0 [ %04X %04X %04X %04X ] -> %04X\n",
                           ulOldValues[ 0 ],
                           ulOldValues[ 1 ],
                           ulOldValues[ 2 ],
                           ulOldValues[ 3 ], ulValue ) );
    }
    else if( strncmp( pcBuffer, "switch", 6 ) == 0 )
    {
        vShowSwitchCounters();
    }

    else if( strncmp( pcBuffer, "sleep", 5 ) == 0 )
    {
        unsigned msecs = 1000U;
		char * ptr = pcBuffer + 5;
		while ( isspace( *ptr ) ) { ptr++; }
		if( isdigit( *ptr ) )
		{
			sscanf( ptr, "%u", &msecs ) ;
		}
		uxNeedSleep = msecs;
    }

    else if( memcmp( pcBuffer, "sdwrite", 6 ) == 0 )
    {
        int number = -1;
        char * ptr = pcBuffer + 6;

        while( *ptr && !isdigit( ( int ) ( *ptr ) ) )
        {
            ptr++;
        }

        if( isdigit( ( int ) ( *ptr ) ) )
        {
            sscanf( ptr, "%d", &number );
        }

        sd_write_test( number );
    }

    else if( memcmp( pcBuffer, "sdread", 6 ) == 0 )
    {
        int number = -1;
        char * ptr = pcBuffer + 6;

        while( *ptr && !isdigit( ( int ) ( *ptr ) ) )
        {
            ptr++;
        }

        if( isdigit( ( int ) ( *ptr ) ) )
        {
            sscanf( ptr, "%d", &number );
        }

        sd_read_test( number );
    }

    else if( memcmp( pcBuffer, "mem", 3 ) == 0 )
    {
        uint32_t now = xPortGetFreeHeapSize();
        uint32_t total = 0; /*xPortGetOrigHeapSize( ); */
        uint32_t perc = total ? ( ( 100 * now ) / total ) : 100;

        lUDPLoggingPrintf( "mem Low %u, Current %lu / %lu (%lu perc free)\n",
                           xPortGetMinimumEverFreeHeapSize(),
                           now, total, perc );
    }

    else if( strncmp( pcBuffer, "tx_test", 7 ) == 0 )
    {
        extern void tx_test( void );
        tx_test();
    }

//  else if( strncmp( pcBuffer, "ntp", 3 ) == 0 )
//  {
//      iTimeZone = -480 * 60; /* Timezone in seconds */
//      vStartNTPTask( NTP_STACK_SIZE, NTP_PRIORITY );
//  }
    #if ( ipconfigMULTI_INTERFACE != 0 )
        else if( strncmp( pcBuffer, "ifconfig", 4 ) == 0 )
        {
            NetworkInterface_t * pxInterface;
            NetworkEndPoint_t * pxEndPoint;

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
        }
    #endif /* ipconfigMULTI_INTERFACE */

    else if( strncmp( pcBuffer, "netstat", 7 ) == 0 )
    {
/*					uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress; */
/*						FreeRTOS_GetAddressConfiguration( &ulIPAddress, &ulNetMask, &ulGatewayAddress, &ulDNSServerAddress ); */
/*						lUDPLoggingPrintf("IP %xip  Mask %xip  GW %xip  DNS %xip\n", */
/*							FreeRTOS_htonl( ulIPAddress ), */
/*							FreeRTOS_htonl( ulNetMask ), */
/*							FreeRTOS_htonl( ulGatewayAddress ), */
/*							FreeRTOS_htonl( ulDNSServerAddress ) ); */
        FreeRTOS_netstat();
    }

    #if ( USE_LOG_EVENT != 0 )
        else if( strncmp( pcBuffer, "event", 4 ) == 0 )
        {
            eventLogDump();
        }
    #endif /* USE_LOG_EVENT */

    else if( strncmp( pcBuffer, "list", 4 ) == 0 )
    {
        vShowTaskTable( pcBuffer[ 4 ] == 'c' );
    }

    else if( strncmp( pcBuffer, "format", 7 ) == 0 )
    {
        if( pxSDDisks[ 0 ] != NULL )
        {
            FF_SDDiskFormat( pxSDDisks[ 0 ], 0 );
        }
    }

    #if ( mainHAS_FAT_TEST != 0 )
        else if( strncmp( pcBuffer, "fattest", 7 ) == 0 )
        {
            int level = 0;
/*		const char pcMountPath[] = "/test"; */
            const char pcMountPath[] = "/ram";

            if( sscanf( pcBuffer + 7, "%d", &level ) == 1 )
            {
                verboseLevel = level;
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
}

/*-----------------------------------------------------------*/

#if( configUSE_IDLE_HOOK != 0 )
	void vApplicationIdleHook( void );

	void vApplicationIdleHook( void )
	{
	}
#endif
/*-----------------------------------------------------------*/

volatile const char * pcAssertedFileName;
volatile int iAssertedErrno;
volatile uint32_t ulAssertedLine;
volatile FF_Error_t xAssertedFF_Error;

void vAssertCalled( const char * pcFile,
                    uint32_t ulLine )
{
    volatile uint32_t ulBlockVariable = 0UL;

    ulAssertedLine = ulLine;
    iAssertedErrno = stdioGET_ERRNO();
    xAssertedFF_Error = stdioGET_FF_ERROR();
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
/*	FF_RAMDiskStoreImage( pxRAMDisk, "/ram_disk_image.img" ); */
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

void FreeRTOS_Undefined_Handler( void );
void FreeRTOS_Undefined_Handler( void )
{
    configASSERT( 0 );
}


/* Called by FreeRTOS+TCP when the network connects or disconnects.  Disconnect
 * events are only received if implemented in the MAC driver. */
#if ( ipconfigMULTI_INTERFACE != 0 )
    void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent,
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
        #if ( ipconfigMULTI_INTERFACE != 0 )
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
        #endif /* if ( ipconfigMULTI_INTERFACE != 0 ) */

        /* Create the tasks that use the IP stack if they have not already been
         * created. */
        if( ( ulGatewayAddress != 0UL ) && ( xTasksAlreadyCreated == pdFALSE ) )
        {
            xTasksAlreadyCreated = pdTRUE;

            /* Start a new task to fetch logging lines and send them out. */
            vUDPLoggingTaskCreate();

            /* Let the server work task 'prvServerWorkTask' now it can now create the servers. */
            xTaskNotifyGive( xServerWorkTaskHandle );
			xTaskNotifyGive( xFTPTaskHandle );

            doMountDisks = pdTRUE;
        }

        #if ( ipconfigMULTI_INTERFACE == 0 )
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
        #else /* if ( ipconfigMULTI_INTERFACE == 0 ) */
            {
                showEndPoint( pxEndPoint );
/*			/ * Print out the network configuration, which may have come from a DHCP */
/*			server. * / */
/*			FreeRTOS_printf( ( "IP-address : %lxip (default %lxip)\n", */
/*				FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulIPAddress ), FreeRTOS_ntohl( pxEndPoint->ipv4_defaults.ulIPAddress ) ) ); */
/*		#if( ipconfigOLD_MULTI == 0 ) */
/*			FreeRTOS_printf( ( "Net mask   : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulNetMask ) ) ); */
/*			FreeRTOS_printf( ( "GW         : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulGatewayAddress ) ) ); */
/*			FreeRTOS_printf( ( "DNS        : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulDNSServerAddresses[ 0 ] ) ) ); */
/*			FreeRTOS_printf( ( "Broadcast  : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulBroadcastAddress ) ) ); */
/*			FreeRTOS_printf( ( "MAC address: %02x-%02x-%02x-%02x-%02x-%02x\n", */
/*				pxEndPoint->xMACAddress.ucBytes[ 0 ], */
/*				pxEndPoint->xMACAddress.ucBytes[ 1 ], */
/*				pxEndPoint->xMACAddress.ucBytes[ 2 ], */
/*				pxEndPoint->xMACAddress.ucBytes[ 3 ], */
/*				pxEndPoint->xMACAddress.ucBytes[ 4 ], */
/*				pxEndPoint->xMACAddress.ucBytes[ 5 ] ) ); */
/*		#endif	/ * ( ipconfigOLD_MULTI == 0 ) * / */
/*			FreeRTOS_printf( ( " \n" ) ); */
            }
        #endif /* ipconfigMULTI_INTERFACE */
        /*vIPerfInstall(); */
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

void vApplicationMallocFailedHook( void );
void vApplicationMallocFailedHook( void )
{
    /* Called if a call to pvPortMalloc() fails because there is insufficient
     * free memory available in the FreeRTOS heap.  pvPortMalloc() is called
     * internally by FreeRTOS API functions that create tasks, queues, software
     * timers, and semaphores.  The size of the FreeRTOS heap is set by the
     * configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
    vAssertCalled( __FILE__, __LINE__ );
}
/*-----------------------------------------------------------*/

#if ( configCHECK_FOR_STACK_OVERFLOW != 0 )
    #error projcet not optimised for speed
#endif

void vApplicationStackOverflowHook( TaskHandle_t pxTask,
                                    char * pcTaskName );
void vApplicationStackOverflowHook( TaskHandle_t pxTask,
                                    char * pcTaskName )
{
    volatile char * pcSaveName = ( volatile char * ) pcTaskName;
    volatile TaskHandle_t pxSaveTask = ( volatile TaskHandle_t ) pxTask;

    ( void ) pcTaskName;
    ( void ) pxTask;
    ( void ) pcSaveName;
    ( void ) pxSaveTask;

    /* Run time stack overflow checking is performed if
     * configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
     * function is called if a stack overflow is detected. */
    taskDISABLE_INTERRUPTS();

    for( ; ; )
    {
    }
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

BaseType_t xApplicationGetRandomNumber( uint32_t * pulNumber )
{
    uint32_t ulR1 = uxRand() & 0x7fff;
    uint32_t ulR2 = uxRand() & 0x7fff;
    uint32_t ulR3 = uxRand() & 0x7fff;

    *pulNumber = ( ulR1 << 17 ) | ( ulR2 << 2 ) | ( ulR3 & 0x03uL );
    return pdTRUE;
}

BaseType_t xRandom32( uint32_t * pulValue );
BaseType_t xRandom32( uint32_t * pulValue )
{
    *pulValue = uxRand();
    return pdPASS;
}

uint32_t ulApplicationGetNextSequenceNumber( uint32_t ulSourceAddress,
                                             uint16_t usSourcePort,
                                             uint32_t ulDestinationAddress,
                                             uint16_t usDestinationPort )
{
    ( void ) ulSourceAddress;
    ( void ) usSourcePort;
    ( void ) ulDestinationAddress;
    ( void ) usDestinationPort;

    uint32_t ulValue = uxRand();
    ulValue |= ( ( uint32_t ) uxRand() ) << 16;
    return ulValue;
}

static void prvSRand( UBaseType_t ulSeed )
{
    /* Utility function to seed the pseudo random number generator. */
    ulNextRand = ulSeed;
}
/*-----------------------------------------------------------*/

static XUartPs_Config * uart_cfg;
static XUartPs uartInstance;
static void prvMiscInitialisation( void )
{
    time_t xTimeNow;

    uart_cfg = XUartPs_LookupConfig( XPAR_PS7_UART_1_DEVICE_ID );
    s32 rc = XUartPs_CfgInitialize( &uartInstance, uart_cfg, XPS_UART0_BASEADDR );
    FreeRTOS_printf( ( "XUartPs_CfgInitialize: %ld\n", rc ) );

    /* Seed the random number generator. */
    time( &xTimeNow );
    FreeRTOS_debug_printf( ( "Seed for randomiser: %lu\n", xTimeNow ) );
    prvSRand( ( uint32_t ) xTimeNow );
    FreeRTOS_debug_printf( ( "Random numbers: %08X %08X %08X %08X\n", ipconfigRAND32(), ipconfigRAND32(), ipconfigRAND32(), ipconfigRAND32() ) );

    vPortInstallFreeRTOSVectorTable();
/*	Xil_DCacheEnable(); */
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

#if ( ipconfigUSE_IPv6 != 0 )
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
		FreeRTOS_printf( ( "%s address %xip\n", xDone ? "Success" : "Failed", FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulIPAddress ) ) );
	}
	return xDone;
}
#endif  /* ( ipconfigUSE_IPv6 != 0 ) */

#if( ipconfigUSE_LLMNR != 0 )
	//#warning Has LLMNR
#endif

#if( ipconfigUSE_NBNS != 0 )
	#warning Has NBNS
#endif

#if( ipconfigUSE_MDNS != 0 )
	#warning Has MDNS
#endif

static BaseType_t xMatchDNS( const char * pcObject, const char * pcMyName )
{
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
	return rc;
}

#if ( ipconfigMULTI_INTERFACE != 0 )
    BaseType_t xApplicationDNSQueryHook( NetworkEndPoint_t * pxEndPoint,
                                         const char * pcName )
#else
    BaseType_t xApplicationDNSQueryHook( const char * pcName )
#endif
{
    BaseType_t xReturn;

	/* Determine if a name lookup is for this node.  Two names are given
	to this node: that returned by pcApplicationHostnameHook() and that set
	by mainDEVICE_NICK_NAME. */
	const char *serviceName = ( strstr( pcName, ".local" ) != NULL ) ? "mDNS" : "LLMNR";

	if( xMatchDNS( pcName, "bong" ) == 0 )
	{
	#if( ipconfigUSE_IPv6 != 0 )
		int ip6Preferred = ( pcName[4] == '6' ) ? 6 : 4;
		BaseType_t bDNS_IPv6 = ( pxEndPoint->usDNSType == dnsTYPE_AAAA_HOST ) ? 1 : 0;

		xReturn = ( bDNS_IPv6 != 0 ) == ( ip6Preferred == 6 );
	#else
		xReturn = pdPASS;
	#endif
	}
    else if( xMatchDNS( pcName, pcApplicationHostnameHook() ) == 0 )  /* "RTOSDemo" */
    {
        xReturn = pdPASS;
    }
    else if( xMatchDNS( pcName, mainDEVICE_NICK_NAME ) == 0 )  /* "zynq" */
    {
        xReturn = pdPASS;
    }
    else
    {
        xReturn = pdFAIL;
    }
#if ( ipconfigUSE_IPv6 != 0 )
	if( xReturn == pdTRUE )
	{
		xReturn = setEndPoint( pxEndPoint );
	}
#endif /* ( ipconfigUSE_IPv6 != 0 ) */

{
#if( ipconfigMULTI_INTERFACE != 0 ) && ( ipconfigUSE_IPv6 != 0 )
		FreeRTOS_printf( ( "%s query '%s' = %d IPv%c\n", serviceName, pcName, (int)xReturn, pxEndPoint->bits.bIPv6 ? '6' : '4' ) );
#else
		FreeRTOS_printf( ( "%s query '%s' = %d IPv4 only\n", serviceName, pcName, (int)xReturn ) );
#endif
}

    return xReturn;
}
/*-----------------------------------------------------------*/

/*#warning _gettimeofday_r is just stubbed out here. */
struct timezone;
struct timeval;
int _gettimeofday_r( struct _reent * x,
                     struct timeval * y,
                     struct timezone * ptimezone );
int _gettimeofday_r( struct _reent * x,
                     struct timeval * y,
                     struct timezone * ptimezone )
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
int _write( int fd, char * buf, int nbytes );
int _write( int fd, char * buf, int nbytes )
{
    ( void ) fd;
    ( void ) buf;
    ( void ) nbytes;
    return 0;
}
/*-----------------------------------------------------------*/

off_t lseek( int fd, off_t offset, int whence );
off_t lseek( int fd, off_t offset, int whence )
{
    ( void ) fd;
    ( void ) offset;
    ( void ) whence;
    return( ( off_t ) -1 );
}
/*-----------------------------------------------------------*/

off_t _lseek( int fd, off_t offset, int whence );
off_t _lseek( int fd, off_t offset, int whence )
{
    ( void ) fd;
    ( void ) offset;
    ( void ) whence;
    return( ( off_t ) -1 );
}
/*-----------------------------------------------------------*/

int _read( int fd, char * buf, int nbytes );
int _read( int fd, char * buf, int nbytes )
{
    ( void ) fd;
    ( void ) buf;
    ( void ) nbytes;
    return 0;
}
/*-----------------------------------------------------------*/

u32 prof_pc;

void _profile_init( void );
void _profile_init( void )
{
}
/*-----------------------------------------------------------*/

void _profile_clean( void );
void _profile_clean( void )
{
}
/*-----------------------------------------------------------*/

int _fstat( void );
int _fstat()
{
    return -1;
}

int _isatty( void );
int _isatty()
{
    return 0;
}

void _fini( void );
void _fini()
{
}

void _init( void );
void _init()
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
    TaskStatus_t * pxTaskStatusArray;
    volatile UBaseType_t uxArraySize, x;
    uint64_t ullTotalRunTime;
    uint32_t ulStatsAsPermille;

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

    FreeRTOS_printf( ( "Heap: min/cur/max: %lu %lu %lu\n",
                       ( uint32_t ) xPortGetMinimumEverFreeHeapSize(),
                       ( uint32_t ) xPortGetFreeHeapSize(),
                       ( uint32_t ) configTOTAL_HEAP_SIZE ) );

    if( aDoClear != pdFALSE )
    {
/*		ulListTime = xTaskGetTickCount(); */
        ullHiresTime = ullGetHighResolutionTime();
    }
}
/*-----------------------------------------------------------*/


#define DDR_START    XPAR_PS7_DDR_0_S_AXI_BASEADDR
#define DDR_END      XPAR_PS7_DDR_0_S_AXI_HIGHADDR
BaseType_t xApplicationMemoryPermissions( uint32_t aAddress );
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

void vOutputChar( const char cChar,
                  const TickType_t xTicksToWait );
void vOutputChar( const char cChar,
                  const TickType_t xTicksToWait )
{
    ( void ) cChar;
    ( void ) xTicksToWait;
    /* Do nothing */
}

#if ( ipconfigUSE_WOLFSSL == 0 )
    void vHandleButtonClick( BaseType_t xButton,
                             BaseType_t * pxHigherPriorityTaskWoken );
    void vHandleButtonClick( BaseType_t xButton,
                             BaseType_t * pxHigherPriorityTaskWoken )
    {
        ( void ) xButton;
        ( void ) pxHigherPriorityTaskWoken;
		mustSendARP++;
    }
#endif

#if ( ipconfigUSE_DHCP_HOOK != 0 )

/* The following function must be provided by the user.
 * The user may decide to continue or stop, after receiving a DHCP offer. */
    eDHCPCallbackAnswer_t xApplicationDHCPHook( eDHCPCallbackPhase_t eDHCPPhase,
                                                uint32_t ulIPAddress )
    {
        ( void ) eDHCPPhase;
        ( void ) ulIPAddress;
        return eDHCPContinue;
/*#warning Do not use this for release */
/*		return eDHCPStopNoChanges;*/
    }
#endif /* ipconfigUSE_DHCP_HOOK */

const unsigned char packet_template[] =
{
    0x33, 0x33, 0xff, 0x66, 0x4a, 0x7e, 0x14, 0xda, 0xe9, 0xa3, 0x94, 0x22, 0x86, 0xdd, 0x60, 0x00,
    0x00, 0x00, 0x00, 0x20, 0x3a, 0xff, 0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8d, 0x11,
    0xcd, 0x9b, 0x8b, 0x66, 0x4a, 0x75, 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x01, 0xff, 0x66, 0x4a, 0x7e, 0x87, 0x00, 0x3d, 0xfd, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8d, 0x11, 0xcd, 0x9b, 0x8b, 0x66, 0x4a, 0x7e, 0x01, 0x01,
    0x14, 0xda, 0xe9, 0xa3, 0x94, 0x22
};

#if ( ipconfigUSE_IPv6 != 0 )

    static const unsigned char targetMultiCastAddr[ 16 ] =
    {
        0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xff, 0x66, 0x4a, 0x75
    };

    static const unsigned char targetAddress[ 16 ] =
    {
        0xfe, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8d, 0x11, 0xcd, 0x9b, 0x8b, 0x66, 0x4a, 0x75
    };

    static const unsigned char targetMAC[ 6 ] =
    {
        0x33, 0x33, 0xff, 0x66, 0x4a, 0x75
    };

    void sendICMP()
    {
        NetworkBufferDescriptor_t * pxNetworkBuffer;
        ICMPPacket_IPv6_t * pxICMPPacket;
        ICMPHeader_IPv6_t * ICMPHeader_IPv6;
        NetworkEndPoint_t * pxEndPoint;
        size_t xRequestedSizeBytes = sizeof( packet_template );
        BaseType_t xICMPSize = sizeof( ICMPHeader_IPv6_t );
        BaseType_t xDataLength = ( size_t ) ( ipSIZE_OF_ETH_HEADER + ipSIZE_OF_IPv6_HEADER + xICMPSize );

        pxNetworkBuffer = pxGetNetworkBufferWithDescriptor( xRequestedSizeBytes, 1000 );

        memcpy( pxNetworkBuffer->pucEthernetBuffer, packet_template, sizeof( packet_template ) );
        pxICMPPacket = ( ICMPPacket_IPv6_t * ) pxNetworkBuffer->pucEthernetBuffer;
        ICMPHeader_IPv6 = ( ICMPHeader_IPv6_t * ) &( pxICMPPacket->xICMPHeaderIPv6 );
        pxEndPoint = FreeRTOS_FindEndPointOnNetMask_IPv6( NULL );

        ICMPHeader_IPv6->ucTypeOfMessage = ipICMP_NEIGHBOR_SOLICITATION_IPv6;
        ICMPHeader_IPv6->ucTypeOfService = 0;
        ICMPHeader_IPv6->usChecksum = 0;
        ICMPHeader_IPv6->ulReserved = 0;
        memcpy( ICMPHeader_IPv6->xIPv6Address.ucBytes, targetAddress, sizeof( targetAddress ) );

        memcpy( pxICMPPacket->xIPHeader.xDestinationAddress.ucBytes, targetMultiCastAddr, sizeof( IPv6_Address_t ) );
        memcpy( pxICMPPacket->xIPHeader.xSourceAddress.ucBytes, pxEndPoint->ipv6_settings.xIPAddress.ucBytes, sizeof( IPv6_Address_t ) );
        pxICMPPacket->xIPHeader.usPayloadLength = FreeRTOS_htons( xICMPSize );
        ICMPHeader_IPv6->ucOptionBytes[ 0 ] = 1;
        ICMPHeader_IPv6->ucOptionBytes[ 1 ] = 1;
/*	memcpy( ICMPHeader_IPv6->ucOptionBytes + 2, pxEndPoint->xMACAddress.ucBytes, sizeof( pxEndPoint->xMACAddress ) ); */
        memcpy( ICMPHeader_IPv6->ucOptionBytes + 2, pxEndPoint->xMACAddress.ucBytes, sizeof( pxEndPoint->xMACAddress ) );

        memcpy( pxICMPPacket->xEthernetHeader.xSourceAddress.ucBytes, targetMAC, sizeof targetMAC );

        pxICMPPacket->xIPHeader.ucHopLimit = 255;

/*				#if( ipconfigDRIVER_INCLUDED_TX_IP_CHECKSUM == 0 ) */
        {
            /* calculate the UDP checksum for outgoing package */
            usGenerateProtocolChecksum( ( uint8_t * ) pxNetworkBuffer->pucEthernetBuffer, xDataLength, pdTRUE );
        }
/*				#endif */

        /* Important: tell NIC driver how many bytes must be sent */
        pxNetworkBuffer->xDataLength = xDataLength;
        pxNetworkBuffer->pxEndPoint = pxEndPoint;

        /* This function will fill in the eth addresses and send the packet */
        vReturnEthernetFrame( pxNetworkBuffer, pdFALSE );
    }
#endif /* ipconfigUSE_IPv6 */

#define WRITE_BUF_SIZE     ( 4 * ( 1024 * 1024 ) )
#define WRITE_BUF_COUNT    ( 2 )

static void sd_write_test( int aNumber )
{
    static int curIndex = 0;
    unsigned * buffer;
    char fname[ 128 ];
    int i, index;
    FF_FILE * fp;
    unsigned long long times[ 20 ];
    uint64_t sum = 0u;

    if( aNumber >= 0 )
    {
        curIndex = aNumber;
    }

    buffer = ( unsigned * ) pvPortMalloc( WRITE_BUF_SIZE );

    if( buffer == NULL )
    {
        FreeRTOS_printf( ( "malloc failed\n" ) );
        return;
    }

    memset( buffer, 0xE3, WRITE_BUF_SIZE );
    curIndex++;
    sprintf( fname, "/test%04d.txt", curIndex );

    fp = ff_fopen( fname, "w" );

    if( fp == NULL )
    {
        FreeRTOS_printf( ( "ff_fopen %s failed\n", fname ) );
        return;
    }

    times[ 0 ] = ullGetHighResolutionTime();

    for( index = 0; index < WRITE_BUF_COUNT; index++ )
    {
        int rc;
        rc = ff_fwrite( buffer, 1, WRITE_BUF_SIZE, fp );
        times[ index + 1 ] = ullGetHighResolutionTime();

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
    unsigned * buffer;
    char fname[ 128 ];
    int i, index;
    FF_FILE * fp;
    unsigned long long times[ 20 ];
    uint64_t sum = 0u;

    if( aNumber >= 0 )
    {
        curIndex = aNumber;
    }

    buffer = ( unsigned * ) pvPortMalloc( WRITE_BUF_SIZE );

    if( buffer == NULL )
    {
        FreeRTOS_printf( ( "malloc failed\n" ) );
        return;
    }

    memset( buffer, 0xE3, WRITE_BUF_SIZE );
    curIndex++;
    sprintf( fname, "/test%04d.txt", curIndex );

    fp = ff_fopen( fname, "r" );

    if( fp == NULL )
    {
        FreeRTOS_printf( ( "ff_fopen %s failed\n", fname ) );
        return;
    }

    times[ 0 ] = ullGetHighResolutionTime();

    for( index = 0; index < WRITE_BUF_COUNT; index++ )
    {
        int rc;
        rc = ff_fread( buffer, 1, WRITE_BUF_SIZE, fp );
        times[ index + 1 ] = ullGetHighResolutionTime();

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

#define LOGGING_MARK_BYTES    ( 100ul * 1024ul * 1024ul )

void copy_file( const char * pcTarget,
                const char * pcSource )
{
    FF_FILE * infile, * outfile;
    unsigned * buffer;

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

                    if( toCopy > ( size_t ) WRITE_BUF_SIZE )
                    {
                        toCopy = WRITE_BUF_SIZE;
                    }

                    nread = ff_fread( buffer, 1, toCopy, infile );

                    if( nread != ( int ) toCopy )
                    {
                        FreeRTOS_printf( ( "Reading %s: %d in stead of %d bytes (%d remain)\n", pcSource, nread, toCopy, remain ) );
                        break;
                    }

                    nwritten = ff_fwrite( buffer, 1, toCopy, outfile );

                    if( nwritten != ( int ) toCopy )
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
                        FreeRTOS_printf( ( "Done %lu / %lu MB\n", total_sum / ( 1024lu * 1024lu ), infile->ulFileSize / ( 1024lu * 1024lu ) ) );
                    }
                }

                vPortFree( buffer );
            }

            ff_fclose( outfile );
        }

        ff_fclose( infile );
    }
}

#define MEMCPY_BLOCK_SIZE    0x10000
#define MEMCPY_EXTA_SIZE     128

struct SMEmcpyData
{
    char target_pre[ MEMCPY_EXTA_SIZE ];
    char target_data[ MEMCPY_BLOCK_SIZE + 16 ];
    char target_post[ MEMCPY_EXTA_SIZE ];

    char source_pre[ MEMCPY_EXTA_SIZE ];
    char source_data[ MEMCPY_BLOCK_SIZE + 16 ];
    char source_post[ MEMCPY_EXTA_SIZE ];
};

typedef void * ( * FMemcpy ) ( void * pvDest,
                               const void * pvSource,
                               size_t ulBytes );
typedef void * ( * FMemset ) ( void * pvDest,
                               int iChar,
                               size_t ulBytes );

void * x_memcpy( void * pvDest, const void * pvSource, size_t ulBytes );
void * x_memcpy( void * pvDest, const void * pvSource, size_t ulBytes )
{
    ( void ) pvSource;
    ( void ) ulBytes;
    return pvDest;
}

void * x_memset( void * pvDest, int iValue, size_t ulBytes );
void * x_memset( void * pvDest, int iValue, size_t ulBytes )
{
    ( void ) iValue;
    ( void ) ulBytes;
    return pvDest;
}

#if( ipconfigUSE_CALLBACKS != 0 )
static BaseType_t xOnUdpReceive( Socket_t xSocket, void * pvData, size_t xLength,
	const struct freertos_sockaddr *pxFrom, const struct freertos_sockaddr *pxDest )
{
	( void ) xSocket;
	( void ) pvData;
	( void ) xLength;
	( void ) pxFrom;
	( void ) pxDest;
#if( ipconfigUSE_IPv6 != 0 )
const struct freertos_sockaddr6 *pxFrom6 = ( const struct freertos_sockaddr6 * ) pxFrom;
//const struct freertos_sockaddr6 *pxDest6 = ( const struct freertos_sockaddr6 * ) pxDest;
#endif
	#if( ipconfigUSE_IPv6 != 0 )
	if( pxFrom6->sin_family == FREERTOS_AF_INET6 )
	{
//		FreeRTOS_printf( ( "xOnUdpReceive_6: %d bytes\n",  ( int ) xLength ) );
//		FreeRTOS_printf( ( "xOnUdpReceive_6: from %pip\n", pxFrom6->sin_addrv6.ucBytes ) );
//		FreeRTOS_printf( ( "xOnUdpReceive_6: to   %pip\n", pxDest6->sin_addrv6.ucBytes ) );
	}
	else
	#endif
	{
//		FreeRTOS_printf( ( "xOnUdpReceive_4: %d bytes\n", ( int ) xLength ) );
//		FreeRTOS_printf( ( "xOnUdpReceive_4: from %lxip\n", FreeRTOS_ntohl( pxFrom->sin_addr ) ) );
//		FreeRTOS_printf( ( "xOnUdpReceive_4: to   %lxip\n", FreeRTOS_ntohl( pxDest->sin_addr ) ) );
		
	}
	/* Returning 0 means: not yet consumed. */
	return 0;
}
#endif

void vUDPTest( void *pvParameters )
{
	Socket_t xUDPSocket;
	struct freertos_sockaddr xBindAddress;
	char pcBuffer[ 128 ];
	const TickType_t x1000ms = 1000UL / portTICK_PERIOD_MS;
	int port_number = 50001;	// 30718;
	socklen_t xAddressLength;

	( void ) pvParameters;

	/* Create the socket. */
	xUDPSocket = FreeRTOS_socket( FREERTOS_AF_INET,
								  FREERTOS_SOCK_DGRAM,/*FREERTOS_SOCK_DGRAM for UDP.*/
								  FREERTOS_IPPROTO_UDP );

	/* Check the socket was created. */
	configASSERT( xUDPSocket != FREERTOS_INVALID_SOCKET );

	xBindAddress.sin_addr = FreeRTOS_GetIPAddress();
	xBindAddress.sin_port = FreeRTOS_htons( port_number );
	int rc = FreeRTOS_bind( xUDPSocket, &xBindAddress, sizeof xAddressLength );
	FreeRTOS_printf( ( "FreeRTOS_bind %u rc = %d", FreeRTOS_ntohs( xBindAddress.sin_port ), rc ) );
	if( rc != 0 )
	{
		vTaskDelete( NULL );
		configASSERT( 0 );
	}
	FreeRTOS_setsockopt( xUDPSocket, 0, FREERTOS_SO_RCVTIMEO, &x1000ms, sizeof( x1000ms ) );

	#if( ipconfigUSE_CALLBACKS != 0 )
	{
		F_TCP_UDP_Handler_t xHandler;
		memset( &xHandler, '\0', sizeof ( xHandler ) );
		xHandler.pOnUdpReceive = xOnUdpReceive;
		FreeRTOS_setsockopt( xUDPSocket, 0, FREERTOS_SO_UDP_RECV_HANDLER, ( void * ) &xHandler, sizeof( xHandler ) );
	}
	#endif

	for( ;; )
	{
		int rc_recv, rc_send;
		struct freertos_sockaddr xFromAddress;
		socklen_t len = sizeof xFromAddress;

		rc_recv = FreeRTOS_recvfrom( xUDPSocket, pcBuffer, sizeof pcBuffer, 0, &( xFromAddress ),  &len );
		if (rc_recv > 0) {
			uint8_t *pucBuffer = ( uint8_t * ) pcBuffer;
			char pcAddressBuffer[ 40 ];

			rc_send = FreeRTOS_sendto( xUDPSocket,
                         pcBuffer,
                         rc_recv,
                         0,
                         &( xFromAddress ),
                         sizeof xFromAddress );
		#if ( ipconfigUSE_IPv6 != 0 )
			sockaddr6_t * pxSourceAddressV6 = ( sockaddr6_t * ) &( xFromAddress );
			if( pxSourceAddressV6->sin_family == ( uint8_t ) FREERTOS_AF_INET6 )
			{
				FreeRTOS_inet_ntop( FREERTOS_AF_INET6, ( void * ) pxSourceAddressV6->sin_addrv6.ucBytes, pcAddressBuffer, sizeof pcAddressBuffer );
			}
			else
		#endif
			{
				FreeRTOS_inet_ntop( FREERTOS_AF_INET, ( void * ) &( xFromAddress.sin_addr ), pcAddressBuffer, sizeof pcAddressBuffer );
			}
			FreeRTOS_printf( ( "Received %s port %u: %d bytes: %02x %02x %02x %02x Send %d\n",
							   pcAddressBuffer,
							   FreeRTOS_htons(xFromAddress.sin_port),
							   rc_recv,
							   pucBuffer[ 0 ],
							   pucBuffer[ 1 ],
							   pucBuffer[ 2 ],
							   pucBuffer[ 3 ],
							   rc_send ) );
		}
	}
}

#define logPrintf         lUDPLoggingPrintf

#define     LOOP_COUNT    100

void memcpy_test()
{
    int target_offset;
    int source_offset;
    int algorithm;
    struct SMEmcpyData * pxBlock;
    char * target;
    char * source;
    uint64_t ullStartTime;
    uint32_t ulDelta;
    uint32_t ulTimes[ 2 ][ 4 ][ 4 ];
    uint32_t ulSetTimes[ 2 ][ 4 ];
    /*int time_index = 0; */
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

        for( target_offset = 0; target_offset < 4; target_offset++ )
        {
            for( source_offset = 0; source_offset < 4; source_offset++ )
            {
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

        for( target_offset = 0; target_offset < 4; target_offset++ )
        {
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
               ( uint32_t ) copy_size, ( uint32_t ) ( 8ull * copy_size ) );

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
                       mb2 );
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
                   mb2 );
    }

    vPortFree( ( void * ) pxBlock );
}
/*-----------------------------------------------------------*/

static void prvStreamBufferTask( void * parameters )
{
    char pcBuffer[ 9 ];

    ( void ) parameters;
    FreeRTOS_printf( ( "prvStreamBufferTask started\n" ) );

    for( ; ; )
    {
        if( streamBuffer == NULL )
        {
            vTaskDelay( 5000 );
        }
        else
        {
            size_t rc = xStreamBufferReceive( streamBuffer, pcBuffer, sizeof pcBuffer - 1, 5000 );

            if( rc > 0 )
            {
                pcBuffer[ rc ] = '\0';
                FreeRTOS_printf( ( "prvStreamBufferTask: received '%s' rc = %lu\n", pcBuffer, rc ) );
            }
        }
    }
}
/*-----------------------------------------------------------*/

extern BaseType_t xNTPHasTime;
extern uint32_t ulNTPTime;

struct
{
    uint32_t ntpTime;
}
time_guard;

int stime( time_t * pxTime );
int stime( time_t * pxTime )
{
    ( void ) pxTime;
    time_guard.ntpTime = ulNTPTime - xTaskGetTickCount() / configTICK_RATE_HZ;
    return 0;
}
/*-----------------------------------------------------------*/

int set_time( time_t * pxTime );
//int set_time( time_t * pxTime )
//{
//    return stime( pxTime );
//}
/*-----------------------------------------------------------*/

time_t time( time_t * puxTime )
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
/*-----------------------------------------------------------*/

static void tcp_client()
{
	Socket_t xSocket;
	char packet[] = "ABCDEFGHIJKL\n";
	char buffer[512];
	struct freertos_sockaddr xEchoServerAddress;
	TickType_t xReceiveTimeOut = pdMS_TO_TICKS(5000);
	TickType_t xSendTimeOut = pdMS_TO_TICKS(5000);

	xSocket = FreeRTOS_socket(FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP);
	configASSERT(xSocket != FREERTOS_INVALID_SOCKET);

//	xEchoServerAddress.sin_port = FreeRTOS_htons( 23 );
//	xEchoServerAddress.sin_addr = FreeRTOS_inet_addr_quick( 192, 168, 2, 20 );

	xEchoServerAddress.sin_port = FreeRTOS_htons( 33000 );
	xEchoServerAddress.sin_addr = FreeRTOS_inet_addr_quick( 192, 168, 2, 5 );
	

	FreeRTOS_setsockopt(xSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof(xReceiveTimeOut));
	FreeRTOS_setsockopt(xSocket, 0, FREERTOS_SO_SNDTIMEO, &xSendTimeOut, sizeof(xSendTimeOut));

	if (FreeRTOS_connect(xSocket, &xEchoServerAddress, sizeof(xEchoServerAddress)) == 0)
	{
		BaseType_t xShutdown = pdFALSE;
		FreeRTOS_send(xSocket, packet, sizeof packet, 0);

		for (;;) {
			int rc = FreeRTOS_recv( xSocket, buffer, sizeof buffer, 0 );
			if( rc <= 0 )
			{
				break;
			}
			FreeRTOS_printf( ( "Received %d bytes\n", rc ) );
			if( !xShutdown )
			{
				xShutdown = pdTRUE;
				FreeRTOS_shutdown( xSocket, ~0U );
				FreeRTOS_send(xSocket, "Bye", 3, 0);
			}
		}
	}
	else
	{
		FreeRTOS_printf( ( "Connect failed\n" ) );
	}
	FreeRTOS_closesocket( xSocket );
}

time_t get_time( time_t * puxTime );
//time_t get_time( time_t * puxTime )
//{
//    return time( puxTime );
//}

uint32_t ulApplicationTimeHook( void );
uint32_t ulApplicationTimeHook( void )
{
    return time( NULL );
}

__attribute__ ((weak)) void vListInsertGeneric( List_t * const pxList,
						 ListItem_t * const pxNewListItem,
						 MiniListItem_t * const pxWhere );
__attribute__ ((weak)) void vListInsertGeneric( List_t * const pxList,
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
/*-----------------------------------------------------------*/

/*uint32_t get_time( void ) */
/*{ */
/*	return ( uint32_t ) time( NULL ); */
/*} */

/*-----------------------------------------------------------*/

int logPrintf( const char * apFmt,
               ... ) __attribute__( ( format( __printf__, 1, 2 ) ) );                   /* Delayed write to serial device, non-blocking */
void printf_test( void );
void printf_test()
{
/*	uint32_t ulValue = 0u; */
/* */
/*	logPrintf("%u", ( unsigned ) ulValue); // Zynq: argument 2 has type 'uint32_t' {aka 'long unsigned int'} */
/*	logPrintf("%lu", ulValue); // <== good */
/* */
/*	uint16_t usValue = 0u; */
/*	logPrintf("%u", usValue); // <== good */
/*	logPrintf("%lu", usValue);	// Zynq argument 2 has type 'int' */
/* */
/*	uint8_t ucValue = 0u; */
/*	logPrintf("%u", ucValue); // <== good */
/*	logPrintf("%lu", ucValue); // argument 2 has type 'int' */
/* */
/*	uint64_t ullValue = 0u; */
/*	logPrintf("%u", ullValue);  // argument 2 has type 'uint64_t' {aka 'long long unsigned int'} */
/*	logPrintf("%lu", ullValue); // <== good */
/* */
/*	unsigned uValue = 0u; */
/*	logPrintf("%u", uValue); // <== good */
/*	logPrintf("%lu", uValue); // argument 2 has type 'unsigned int' */
}

#include "xscutimer.h"
#include "xil_exception.h"
#include "xscugic.h"

#define TIMER_DEVICE_ID    XPAR_SCUTIMER_DEVICE_ID
#define INTC_DEVICE_ID     XPAR_SCUGIC_SINGLE_DEVICE_ID

static XScuTimer TimerInstance;

void platform_setup_interrupts( void );
void platform_setup_interrupts()
{
/*	Xil_ExceptionInit(); */
/* */
/*	XScuGic_DeviceInitialize(INTC_DEVICE_ID); */
/* */
/*	/ * */
/*	 * Connect the interrupt controller interrupt handler to the hardware */
/*	 * interrupt handling logic in the processor. */
/*	 * / */
/*	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_IRQ_INT, */
/*			(Xil_ExceptionHandler)XScuGic_DeviceInterruptHandler, */
/*			(void *)INTC_DEVICE_ID); */
/*	/ * */
/*	 * Connect the device driver handler that will be called when an */
/*	 * interrupt for the device occurs, the handler defined above performs */
/*	 * the specific interrupt processing for the device. */
/*	 * / */
/*	XScuGic_RegisterHandler(INTC_BASE_ADDR, TIMER_IRPT_INTR, */
/*					(Xil_ExceptionHandler)timer_callback, */
/*					(void *)&TimerInstance); */
/*	/ * */
/*	 * Enable the interrupt for scu timer. */
/*	 * / */
/*	XScuGic_EnableIntr(INTC_DIST_BASE_ADDR, TIMER_IRPT_INTR); */
}

void platform_enable_interrupts( void );
void platform_enable_interrupts()
{
    /*
     * Enable non-critical exceptions.
     */
    Xil_ExceptionEnableMask( XIL_EXCEPTION_IRQ );
    XScuTimer_EnableInterrupt( &TimerInstance );
    XScuTimer_Start( &TimerInstance );
}

void platform_setup_timer( void );
void platform_setup_timer()
{
    int Status = XST_SUCCESS;
    XScuTimer_Config * ConfigPtr;
    int TimerLoadValue = 0;

    ConfigPtr = XScuTimer_LookupConfig( TIMER_DEVICE_ID );
    Status = XScuTimer_CfgInitialize( &TimerInstance, ConfigPtr,
                                      ConfigPtr->BaseAddr );

    if( Status != XST_SUCCESS )
    {
        FreeRTOS_printf( ( "In %s: Scutimer Cfg initialization failed...\n", __func__ ) );
        return;
    }

    Status = XScuTimer_SelfTest( &TimerInstance );

    if( Status != XST_SUCCESS )
    {
        FreeRTOS_printf( ( "In %s: Scutimer Self test failed...\n", __func__ ) );
        return;
    }

    XScuTimer_EnableAutoReload( &TimerInstance );

    /*
     * Set for 250 milli seconds timeout.
     */
    TimerLoadValue = XPAR_CPU_CORTEXA9_0_CPU_CLK_FREQ_HZ / 8;

    XScuTimer_LoadTimer( &TimerInstance, TimerLoadValue );
}

//#include "E:\temp\printf_format\printf.c"

/*#define ffconfigBYTE_ORDER                pdFREERTOS_LITTLE_ENDIAN */
/*#define ffconfigHAS_CWD                   1 */
/*#define ffconfigLFN_SUPPORT               1 */
/*#define ffconfigSHORTNAME_CASE            1 */
/*#define ffconfigOPTIMISE_UNALIGNED_ACCESS 0 */
/*#define ffconfigCACHE_WRITE_THROUGH       1 */
/*#define ffconfigWRITE_BOTH_FATS           1 */
/*#define ffconfigTIME_SUPPORT              1 */
/*#define ffconfigPATH_CACHE                1 */
/*#define ffconfigPATH_CACHE_DEPTH          5 */
/*#define ffconfigHASH_CACHE                1 */
/*#define ffconfigHASH_CACHE_DEPTH          16 */
/*#define ffconfig64_NUM_SUPPORT            1 */
/*#define ffconfigCWD_THREAD_LOCAL_INDEX    0 */
/*#define ffconfigFAT_CHECK                 1 */
/*#define ffconfigUSE_DELTREE               1 */

static void fill_buffer( char * buffer,
                         char cChar,
                         unsigned clusterSize )
{
    unsigned index, index2;
    char * target = buffer;
    const unsigned LINE_SIZE = 64;

    for( index = 0; index < clusterSize / LINE_SIZE; index++ )
    {
        snprintf( target, 5, "%03u_", index );
        target += 4;

        for( index2 = 4; index2 < ( LINE_SIZE - 2 ); index2++ )
        {
            *( target++ ) = cChar;
        }

        *( target++ ) = 0x0d;
        *( target++ ) = 0x0a;
    }

    FF_PRINTF( "Filled %d bytes\n", ( int ) ( target - buffer ) );
}

void seek_test( unsigned clusterSize );
void seek_test( unsigned clusterSize )
{
/* set a valid cluster size */
    int rc[ 6 ];
    char fname[ 33 ];

    snprintf( fname, sizeof fname, "test_%05u.txt", clusterSize );
    memset( rc, 0, sizeof rc );

    char * buffer = pvPortMalloc( clusterSize );

    if( buffer != NULL )
    {
        fill_buffer( buffer, 'a', clusterSize );
        FF_FILE * file = ff_fopen( fname, "w" );

        if( file != NULL )
        {
/*			rc[ 0 ] = ff_fseek(file, 0, FF_SEEK_SET); */
            rc[ 1 ] = ff_fwrite( buffer, 1, clusterSize, file );
            rc[ 2 ] = ff_fclose( file );

            fill_buffer( buffer, 'b', clusterSize );
            file = ff_fopen( fname, "r+" );

            if( file != NULL )
            {
                rc[ 3 ] = ff_fseek( file, clusterSize, FF_SEEK_SET );
/*				rc[ 3 ] = ff_fseek(file, 0U, FF_SEEK_END); */
                rc[ 4 ] = ff_fwrite( buffer, 1, clusterSize, file );
                rc[ 5 ] = ff_fclose( file );
                FF_PRINTF( "ClusterSize %d Results = %d %d %d %d %d %d\n",
                           clusterSize,
                           rc[ 0 ], rc[ 1 ], rc[ 2 ], rc[ 3 ], rc[ 4 ], rc[ 5 ] );
            }
        }

        vPortFree( buffer );
    }
}


/*
 * vUDPLoggingApplicationHook() will be called for every line of logging.
 */
#define LINE_COUNT     20
#define LINE_LENGTH    128

typedef struct sLogBuffer
{
    char lines[ LINE_COUNT ][ LINE_LENGTH ];
    char head;
} LogBuffer_t;

char * string_lines[ LINE_COUNT ];
LogBuffer_t logBuffer;
static BaseType_t xHasInit;
static BaseType_t xCurrentIndex;

void vUDPLoggingApplicationHook( const char * pcString )
{
    if( xHasInit == pdFALSE )
    {
        BaseType_t index;
        xHasInit = pdTRUE;

        for( index = 0; index < LINE_COUNT; index++ )
        {
            string_lines[ index ] = logBuffer.lines[ index ];
        }

        xCurrentIndex = 0;
    }

    BaseType_t xIndex = xCurrentIndex;

    if( ++xCurrentIndex == LINE_COUNT )
    {
        xCurrentIndex = 0;
    }

    strncpy( logBuffer.lines[ xIndex ], pcString, LINE_LENGTH );
}

static void prvRXWorkTask( void * pvParameter )
{
    ( void ) pvParameter;
    unsigned uCounter = 0;

    for( ; ; )
    {
        FreeRTOS_printf( ( "%4u TX test\n", uCounter++ ) );
        vTaskDelay( pdMS_TO_TICKS( 100U ) );
    }
}

void tx_test( void );
void tx_test()
{
    static TaskHandle_t xTaskHandle = NULL;

    if( xTaskHandle == NULL )
    {
        xTaskCreate( prvRXWorkTask, "tx_test", mainTCP_SERVER_STACK_SIZE, NULL, 2, &xTaskHandle );
        FreeRTOS_printf( ( "tx_test: xTaskHandle %sstarted\n", ( xTaskHandle == NULL ) ? "not " : "" ) );
    }
}

void tcp_set_low_high_water( Socket_t xSocket,
                             size_t uxLittleSpace,   /**< Send a STOP when buffer space drops below X bytes */
                             size_t uxEnoughSpace ); /**< Send a GO when buffer space grows above X bytes */
void tcp_set_low_high_water( Socket_t xSocket,
                             size_t uxLittleSpace,
                             size_t uxEnoughSpace )
{
    LowHighWater_t xLowHighWater;

    memset( &( xLowHighWater ), 0, sizeof( xLowHighWater ) );
    xLowHighWater.uxLittleSpace = uxLittleSpace;
    xLowHighWater.uxEnoughSpace = uxEnoughSpace;

    FreeRTOS_setsockopt( xSocket,
                         0,
                         FREERTOS_SO_SET_LOW_HIGH_WATER,
                         ( void * ) &( xLowHighWater ), sizeof( xLowHighWater ) );
}

//#include "C:\temp\client_test.c"

void logHeapUsage()
{
	size_t uxMinSize = xPortGetMinimumEverFreeHeapSize();
	size_t uxCurSize = xPortGetFreeHeapSize();
	static size_t uxMinLastSize = 0U;

	if( uxMinLastSize == 0U )
	{
		uxMinLastSize = uxMinSize;
	}
	else
	if( uxMinLastSize > uxMinSize )
	{
		size_t uxLost = uxMinLastSize - uxMinSize;
		uxMinLastSize = uxMinSize;
		FreeRTOS_printf( ( "Heap: current %u lowest %u lost %u\n", ( unsigned ) uxCurSize, ( unsigned ) uxMinSize, ( unsigned ) uxLost ) );
	}
}

BaseType_t xDropPacket( BaseType_t xForTransmission );
BaseType_t xDropPacket( BaseType_t xForTransmission )
{
	( void ) xForTransmission;
	return pdFALSE;
/*
	BaseType_t xIndex = ( xForTransmission != 0 ) ? 1 : 0;
	static uint32_t ulCounters[ 2 ] = { 100U, 100U };
	static uint32_t ulSaved = 0U;

	if( ulCounters[ xIndex ] && --ulCounters[ xIndex ] == 0U )
	{
		uint32_t ulMax = xForTransmission ? 100U : 100U;
		uint32_t ulNumber;
		( void ) xApplicationGetRandomNumber( &( ulNumber ) );
		ulCounters[ xIndex ] = 10U + ulNumber % ulMax;

		( void ) xApplicationGetRandomNumber( &( ulNumber ) );
		ulSaved = 1 + (ulNumber % 4);
	}
	
	if( ulSaved )
	{
		ulSaved--;
		return pdTRUE;
	}
	else
	{
		return pdFALSE;
	}
*/
}

void vCheckBroadcast( NetworkBufferDescriptor_t * pxBuffer );
void vCheckBroadcast( NetworkBufferDescriptor_t * pxBuffer )
{
	static volatile UBaseType_t uxCount = 0U;
	static const uint8_t ucBroadcastAddr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	EthernetHeader_t * pxHeader = ( EthernetHeader_t * ) pxBuffer->pucEthernetBuffer;

	if( memcmp( pxHeader->xDestinationAddress.ucBytes, ucBroadcastAddr, sizeof ucBroadcastAddr ) == 0 )
	{
		FreeRTOS_printf( ( "Received a broadcast!\n" ) );
		uxCount++;
	}
}

time_t get_time ( time_t *puxTime )
{
	return FreeRTOS_time( puxTime );
}

int set_time( time_t * puxTime )
{
	FreeRTOS_settime( puxTime );
	return 1;
}
