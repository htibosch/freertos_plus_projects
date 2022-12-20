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

/**
 * @file plus_tcp_demo_cli.c
 * @brief This module will handle a set of commands that help with integration testing.
 *        It is used for integration tests, both IPv4 and IPv6.
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

#include "plus_tcp_demo_cli.h"
#include "http_client_test.h"

#if ( ipconfigUSE_NTP_DEMO != 0 )
    #include "NTPDemo.h"
#endif

#if ( USE_IPERF != 0 )
    #include "iperf_task.h"
#endif

#if ( ipconfigMULTI_INTERFACE != 0 )
    extern void show_single_addressinfo( const char * pcFormat,
                                         const struct freertos_addrinfo * pxAddress );
    extern void show_addressinfo( const struct freertos_addrinfo * pxAddress );
#endif

char * __ctype_ptr__ = ( char * ) _ctype_;

int verboseLevel;

static uint32_t ulWorkCount, ulLastWorkCount;

extern SemaphoreHandle_t xServerSemaphore;

extern uint64_t ullGetHighResolutionTime( void );

/*
 * uint64_t ullGetHighResolutionTime( void )
 * {
 *  // In case you don't have a usec timer function.
 *  return xTaskGetTickCount();
 * }
 */

#define PING_TIMEOUT    100U

typedef struct xCommandOptions
{
    BaseType_t xDoClear;
    BaseType_t xIPVersion; /* Zero means: do not change version, otherwise 4 or 6. */
    BaseType_t xAsynchronous;
    BaseType_t xLogging;
} CommandOptions_t;

size_t uxGetOptions( CommandOptions_t * pxOptions,
                     const char ** ppcCommand );

int PING_COUNT_MAX = 10;

static TaskHandle_t xServerWorkTaskHandle = NULL;

extern void vApplicationPingReplyHook( ePingReplyStatus_t eStatus,
                                       uint16_t usIdentifier );
#if ( ipconfigUSE_IPv6 != 0 )
    static IPv6_Address_t xPing6IPAddress;
    volatile BaseType_t xPing6Count = -1;
#endif
uint32_t ulPingIPAddress;
size_t uxPingSize = 42U;
volatile BaseType_t xPing4Count = -1;
volatile BaseType_t xPingReady;
static TickType_t uxPingTimes[ 2 ];

static int pingLogging = pdFALSE;

static void handle_dnsq( char * pcBuffer );
static void handle_ping( char * pcBuffer );
#if ( ipconfigUSE_NTP_DEMO != 0 )
    static void handle_ntp( char * pcBuffer );
#endif
static void handle_rand( char * pcBuffer );
static void handle_http( char * pcBuffer );
static void handle_whatismyipaddress( char * pcBuffer );
static void handle_ifconfig( char * pcBuffer );
static void handle_gw( char * pcBuffer );
static void handle_help( char * pcBuffer );

static void clear_caches( void );

static volatile BaseType_t xDNSCount = 0;

#if ( ipconfigMULTI_INTERFACE != 0 )
    static struct freertos_addrinfo * pxDNSLookup( char * pcHost,
                                                   BaseType_t xIPVersion,
                                                   BaseType_t xAsynchronous,
                                                   BaseType_t xDoClear );
#endif

#if ( ipconfigUSE_IPv6 == 0 )
    /* In the old days, an IP-address was just a number. */
    static void vDNSEvent( const char * pcName,
                           void * pvSearchID,
                           uint32_t ulIPAddress );
#else
    /* freertos_addrinfo can contain eithe a IPv4 or a IPv6 address. */
    static void vDNSEvent( const char * pcName,
                           void * pvSearchID,
                           struct freertos_addrinfo * pxAddrInfo );
#endif

/*-----------------------------------------------------------*/

typedef void ( * pfhandler ) ( char * /*pcBuffer */ );

struct xCommandCouple
{
    const char * pcCommand;
    size_t uxCommandLength;
    pfhandler pHandler;
    const char * pcHelp;
};

static struct xCommandCouple xCommands[] =
{
    { "ping",       4U,  handle_ping,              "Look up a host and ping it 10 times."                       },
    { "dnsq",       4U,  handle_dnsq,              "Look up a host using DNS, mDNS or LLMNR."                   },
    { "rand",       4U,  handle_rand,              "Call the randomiser and print the resulting number.\n"      },
    { "http",       4U,  handle_http,              "Connect to port 80 of a host and download \"index.html\"\n" },
    { "whatismyip", 10U, handle_whatismyipaddress, "Print my IP-address\n"                                      },
    { "gw",         2U,  handle_gw,                "Show the configured gateway address\n"                      },
    { "ifconfig",   8U,  handle_ifconfig,          "Show a few network parameters\n"                            },
    { "help",       4U,  handle_help,              "Show this help\n"                                           },
    #if ( ipconfigUSE_NTP_DEMO != 0 )
        { "ntp",        3U,  handle_ntp,               "Contact an NTP server and ask the time.\n"                  },
    #endif
};

static BaseType_t can_handle( char * pcBuffer,
                              const char * pcCommand,
                              size_t uxLength,
                              pfhandler phandler )
{
    BaseType_t xReturn = pdFALSE;

    if( strncmp( pcBuffer, pcCommand, uxLength ) == 0 )
    {
        phandler( pcBuffer + uxLength );
        xReturn = pdTRUE;
    }

    return xReturn;
}

/**
 *
 * @brief Create a task that runs the CLI.
 * @return zero when the command was recognised and handled.
 */
BaseType_t xHandleTestingCommand( char * pcBuffer,
                                  size_t uxBufferSize )
{
    BaseType_t xReturn = pdFALSE;

    ( void ) uxBufferSize;

    if( ulLastWorkCount == ulWorkCount )
    {
        FreeRTOS_printf( ( "xHandleTestingCommand: the function xHandleTesting() was not called\n" ) );
    }
    else
    {
        ulLastWorkCount = ulWorkCount;
    }

    if( xServerWorkTaskHandle == NULL )
    {
        xServerWorkTaskHandle = xTaskGetCurrentTaskHandle();
    }

    if( strncmp( pcBuffer, "ver", 3 ) == 0 )
    {
        int level;

        if( sscanf( pcBuffer + 3, "%d", &level ) == 1 )
        {
            verboseLevel = level;
        }

        FreeRTOS_printf( ( "Verbose level %d\n", verboseLevel ) );
        xReturn = pdTRUE;
    }
    else
    {
        BaseType_t xIndex;

        for( xIndex = 0; xIndex < ARRAY_SIZE( xCommands ); xIndex++ )
        {
            struct xCommandCouple * pxCommand = &( xCommands[ xIndex ] );

            if( can_handle( pcBuffer, pxCommand->pcCommand, pxCommand->uxCommandLength, pxCommand->pHandler ) == pdTRUE )
            {
                xReturn = pdTRUE;
                break;
            }
        }
    }

    return xReturn;
}
/*-----------------------------------------------------------*/

#if ( ipconfigMULTI_INTERFACE != 0 )
    static void handle_ifconfig( char * pcBuffer )
    {
        NetworkInterface_t * pxInterface;
        NetworkEndPoint_t * pxEndPoint;

        ( void ) pcBuffer;

        for( pxInterface = FreeRTOS_FirstNetworkInterface();
             pxInterface != NULL;
             pxInterface = FreeRTOS_NextNetworkInterface( pxInterface ) )
        {
            FreeRTOS_printf( ( "Interface %s\n", pxInterface->pcName ) );

            for( pxEndPoint = FreeRTOS_FirstEndPoint( pxInterface );
                 pxEndPoint != NULL;
                 pxEndPoint = FreeRTOS_NextEndPoint( pxInterface, pxEndPoint ) )
            {
                showEndPoint( pxEndPoint );
            }
        }
    }
#else /* if ( ipconfigMULTI_INTERFACE != 0 ) */
    static void handle_ifconfig( char * pcBuffer )
    {
        ( void ) pcBuffer;
        FreeRTOS_printf( ( "IP-address %xip\n", ( unsigned ) FreeRTOS_ntohl( FreeRTOS_GetIPAddress() ) ) );
        FreeRTOS_printf( ( "Netmask    %xip\n", ( unsigned ) FreeRTOS_ntohl( FreeRTOS_GetNetmask() ) ) );
        FreeRTOS_printf( ( "Gateway    %xip\n", ( unsigned ) FreeRTOS_ntohl( FreeRTOS_GetGatewayAddress() ) ) );
        FreeRTOS_printf( ( "DNS        %xip\n", ( unsigned ) FreeRTOS_ntohl( FreeRTOS_GetDNSServerAddress() ) ) );
    }
#endif /* ( ipconfigMULTI_INTERFACE != 0 ) */
/*-----------------------------------------------------------*/

#if ( ipconfigMULTI_INTERFACE != 0 )
    static void handle_dnsq( char * pcBuffer )
    {
        CommandOptions_t xOptions;
        char * ptr = pcBuffer;

        uxGetOptions( &( xOptions ), ( const char ** ) &( ptr ) );

        while( isspace( ( ( uint8_t ) *ptr ) ) )
        {
            ptr++;
        }

        if( *ptr )
        {
            struct freertos_addrinfo * pxResult;

            pxResult = pxDNSLookup( ptr, xOptions.xIPVersion, xOptions.xAsynchronous, xOptions.xDoClear );

            if( pxResult != NULL )
            {
                FreeRTOS_freeaddrinfo( pxResult );
            }
        }
        else
        {
            FreeRTOS_printf( ( "Usage: dnsq <name>\n" ) );
        }
    }
/*-----------------------------------------------------------*/

#else /* if ( ipconfigMULTI_INTERFACE != 0 ) */

    static void handle_dnsq( char * pcBuffer )
    {
        CommandOptions_t xOptions;
        char * ptr = pcBuffer;

        uxGetOptions( &( xOptions ), ( const char ** ) &( ptr ) );

        while( *ptr && !isspace( *ptr ) )
        {
            ptr++;
        }

        while( isspace( ( ( uint8_t ) *ptr ) ) )
        {
            ptr++;
        }

        unsigned tmout = 4000;
        static unsigned searchID;

        if( *ptr )
        {
            uint32_t ip;

            for( char * target = ptr; *target; target++ )
            {
                if( isspace( *target ) )
                {
                    *target = '\0';
                    break;
                }
            }

            if( xOptions.xDoClear )
            {
                #if ( ipconfigUSE_DNS_CACHE != 0 )
                    {
                        FreeRTOS_dnsclear();
                        FreeRTOS_printf( ( "Clear DNS cache and ARP\n" ) );
                    }
                #endif /* ipconfigUSE_DNS_CACHE */
                #if ( ipconfigMULTI_INTERFACE != 0 )
                    FreeRTOS_ClearARP( NULL );
                #else
                    FreeRTOS_ClearARP();
                #endif
                FreeRTOS_printf( ( "Clear ARP cache\n" ) );
            }

            FreeRTOS_printf( ( "DNS query: '%s'\n", ptr ) );
            {
                uint32_t ulGatewayAddress;
                FreeRTOS_GetAddressConfiguration( NULL, NULL, &( ulGatewayAddress ), NULL );

                if( xIsIPInARPCache( ulGatewayAddress ) == pdFALSE )
                {
                    xARPWaitResolution( ulGatewayAddress, pdMS_TO_TICKS( 5000U ) );
                }
            }
            #if ( ipconfigDNS_USE_CALLBACKS != 0 )
                if( xOptions.xAsynchronous != 0 )
                {
                    BaseType_t iCount;
                    TickType_t uxWaitTime = 1000U;
                    xDNSCount = 0;
                    ip = FreeRTOS_gethostbyname_a( ptr, vDNSEvent, ( void * ) ++searchID, tmout );

                    for( iCount = 0; iCount < 10; iCount++ )
                    {
                        ulTaskNotifyTake( pdTRUE, uxWaitTime );

                        if( xDNSCount != 0 )
                        {
                            break;
                        }
                    }
                }
                else
            #endif /* if ( ipconfigDNS_USE_CALLBACKS != 0 ) */
            {
                #if ( ipconfigDNS_USE_CALLBACKS == 0 )
                    if( xOptions.xAsynchronous != 0 )
                    {
                        FreeRTOS_printf( ( "Asynchronous DNS requested but not installed.\n" ) );
                    }
                #endif
                ip = FreeRTOS_gethostbyname( ptr );
            }

            FreeRTOS_printf( ( "%s : %xip\n", ptr, ( unsigned ) FreeRTOS_ntohl( ip ) ) );
            #if ( ipconfigUSE_DNS_CACHE == 0 )
                {
                    FreeRTOS_printf( ( "DNS caching not enabled\n" ) );
                }
            #else
                {
                    uint32_t ulFirstIPAddress = 0U;
                    BaseType_t xIndex;

                    for( xIndex = 0; xIndex < ( BaseType_t ) ipconfigDNS_CACHE_ENTRIES; xIndex++ )
                    {
                        /* Note: 'FreeRTOS_dnslookup' is only defined when
                         * 'ipconfigUSE_DNS_CACHE' is enabled. */
                        uint32_t ulThisIPAddress = FreeRTOS_dnslookup( ptr );

                        if( xIndex == 0 )
                        {
                            ulFirstIPAddress = ulThisIPAddress;
                        }
                        else if( ulFirstIPAddress == ulThisIPAddress )
                        {
                            break;
                        }

                        FreeRTOS_printf( ( "Cache[%d]: %xip\n", ( int ) xIndex, ( unsigned ) FreeRTOS_ntohl( ulThisIPAddress ) ) );
                    }
                }
            #endif /* ( ipconfigUSE_DNS_CACHE == 0 ) */
        }
        else
        {
            FreeRTOS_printf( ( "Usage: dnsquery <name>\n" ) );
        }
    }
#endif /* ( ipconfigMULTI_INTERFACE != 0 ) */
/*-----------------------------------------------------------*/

static void handle_rand( char * pcBuffer )
{
    uint32_t ulNumber = 0x5a5a5a5a;
    BaseType_t rc = xApplicationGetRandomNumber( &ulNumber );

    ( void ) pcBuffer;

    if( rc == pdPASS )
    {
        char buffer[ 33 ];
        int index;
        uint32_t ulMask = 0x80000000uL;

        for( index = 0; index < 32; index++ )
        {
            buffer[ index ] = ( ( ulNumber & ulMask ) != 0 ) ? '1' : '0';
            ulMask >>= 1;
        }

        buffer[ index ] = '\0';
        FreeRTOS_printf( ( "Random %08lx (%s)\n", ulNumber, buffer ) );
    }
    else
    {
        FreeRTOS_printf( ( "Random failed\n" ) );
    }
}
/*-----------------------------------------------------------*/

size_t uxGetOptions( CommandOptions_t * pxOptions,
                     const char ** ppcCommand )
{
    size_t uxLength = 0U;
    const char * pcCommand = *ppcCommand;

    memset( pxOptions, 0, sizeof( *pxOptions ) );
    pxOptions->xIPVersion = 4;

    while( ( pcCommand[ uxLength ] != 0 ) && ( !isspace( ( uint8_t ) pcCommand[ uxLength ] ) ) )
    {
        switch( pcCommand[ uxLength ] )
        {
            case 'a':
                pxOptions->xAsynchronous = pdTRUE;
                break;

            case 'c':
                pxOptions->xDoClear = pdTRUE;
                break;

            case 'v':
                pxOptions->xLogging = pdTRUE;
                break;

            case '4':
                pxOptions->xIPVersion = 4;
                break;

            case '6':
                pxOptions->xIPVersion = 6;
                break;
        }

        uxLength++;
    }

    if( uxLength > 0U )
    {
        *ppcCommand = &( pcCommand[ uxLength ] );
    }

    return uxLength;
}

#if ( ipconfigUSE_NTP_DEMO != 0 )
    static void handle_ntp( char * pcBuffer )
    {
        CommandOptions_t xOptions;
        char * ptr = pcBuffer;

        uxGetOptions( &( xOptions ), ( const char ** ) &( ptr ) );

        if( xOptions.xDoClear )
        {
            clear_caches();
            vNTPClearCache();
        }

        vNTPSetNTPType( xOptions.xIPVersion, xOptions.xAsynchronous, xOptions.xLogging );
        /* vStartNTPTask() may be called multiple times. */
        vStartNTPTask( configMINIMAL_STACK_SIZE * 12, tskIDLE_PRIORITY + 1 );
    }
#endif /* if ( ipconfigUSE_NTP_DEMO != 0 ) */

#if ( ipconfigUSE_IPv6 != 0 )
    static void handle_whatismyipaddress( char * pcBuffer )
    {
        NetworkEndPoint_t * pxEndPoint;

        ( void ) pcBuffer;
        FreeRTOS_printf( ( "Showing all end-points\n" ) );

        for( pxEndPoint = FreeRTOS_FirstEndPoint( NULL );
             pxEndPoint != NULL;
             pxEndPoint = FreeRTOS_NextEndPoint( NULL, pxEndPoint ) )
        {
            #if ( ipconfigUSE_IPv6 != 0 )
                if( pxEndPoint->bits.bIPv6 )
                {
                    FreeRTOS_printf( ( "IPv6: %pip on '%s'\n", pxEndPoint->ipv6_settings.xIPAddress.ucBytes, pxEndPoint->pxNetworkInterface->pcName ) );
                }
                else
            #endif
            {
                FreeRTOS_printf( ( "IPv4: %xip on '%s'\n", ( unsigned ) FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulIPAddress ), pxEndPoint->pxNetworkInterface->pcName ) );
            }
        }
    }
#else /* if ( ipconfigUSE_IPv6 != 0 ) */
    static void handle_whatismyipaddress( char * pcBuffer )
    {
        ( void ) pcBuffer;
        FreeRTOS_printf( ( "IPv4: %xip\n", ( unsigned ) FreeRTOS_ntohl( FreeRTOS_GetIPAddress() ) ) );
    }
#endif /* if ( ipconfigUSE_IPv6 != 0 ) */

static void handle_help( char * pcBuffer )
{
    BaseType_t xIndex;

    ( void ) pcBuffer;
    FreeRTOS_printf( ( "Available commands:\n" ) );

    for( xIndex = 0; xIndex < ARRAY_SIZE( xCommands ); xIndex++ )
    {
        FreeRTOS_printf( ( "%-11.11s: %s\n", xCommands[ xIndex ].pcCommand, xCommands[ xIndex ].pcHelp ) );
    }
}

#if ( ipconfigMULTI_INTERFACE != 0 )
    static void handle_gw( char * pcBuffer )
    {
        NetworkEndPoint_t * pxEndPoint;

        ( void ) pcBuffer;
        FreeRTOS_printf( ( "Showing all gateways\n" ) );

        for( pxEndPoint = FreeRTOS_FirstEndPoint( NULL );
             pxEndPoint != NULL;
             pxEndPoint = FreeRTOS_NextEndPoint( NULL, pxEndPoint ) )
        {
            #if ( ipconfigUSE_IPv6 != 0 )
                if( pxEndPoint->bits.bIPv6 )
                {
                    if( memcmp( pxEndPoint->ipv6_settings.xGatewayAddress.ucBytes, in6addr_any.ucBytes, ipSIZE_OF_IPv6_ADDRESS ) != 0 )
                    {
                        FreeRTOS_printf( ( "IPv6: %pip on '%s'\n", pxEndPoint->ipv6_settings.xGatewayAddress.ucBytes, pxEndPoint->pxNetworkInterface->pcName ) );
                    }
                }
                else
            #endif
            {
                if( pxEndPoint->ipv4_settings.ulGatewayAddress != 0U )
                {
                    FreeRTOS_printf( ( "IPv4: %xip on '%s'\n", ( unsigned ) FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulGatewayAddress ), pxEndPoint->pxNetworkInterface->pcName ) );
                }
            }
        }
    }
#else /* if ( ipconfigMULTI_INTERFACE != 0 ) */
    static void handle_gw( char * pcBuffer )
    {
        ( void ) pcBuffer;
        FreeRTOS_printf( ( "Gateway: %xip\n", ( unsigned ) FreeRTOS_ntohl( FreeRTOS_GetGatewayAddress() ) ) );
    }
#endif /* if ( ipconfigMULTI_INTERFACE != 0 ) */

#if ( ipconfigMULTI_INTERFACE != 0 )
    static void handle_ping( char * pcBuffer )
    {
        struct freertos_addrinfo * pxDNSResult = NULL;
        char * ptr = pcBuffer;
        CommandOptions_t xOptions;

        uxGetOptions( &( xOptions ), ( const char ** ) &( ptr ) );

        pingLogging = xOptions.xLogging;

        while( isspace( ( ( uint8_t ) *ptr ) ) )
        {
            ptr++;
        }

        char * pcHostname = ptr;

        if( *pcHostname == '\0' )
        {
            #if ( ipconfigUSE_IPv6 != 0 )
                if( xOptions.xIPVersion == 6 )
                {
                    pcHostname = "fe80::6816:5e9b:80a0:9edb";
                }
                else
            #endif
            {
                pcHostname = "192.168.2.1";
            }
        }

        #if ( ipconfigUSE_IPv6 != 0 )
            if( strchr( pcHostname, ':' ) != NULL )
            {
                IPv6_Address_t xAddress_IPv6;

                /* ulIPAddress does not represent an IPv4 address here. It becomes non-zero when the look-up succeeds. */
                if( FreeRTOS_inet_pton6( pcHostname, xAddress_IPv6.ucBytes ) != 0 )
                {
                    xOptions.xIPVersion = 6;
                }
            }
            else if( strchr( pcHostname, '.' ) != NULL )
            {
                uint32_t ulIPAddress;

                ulIPAddress = FreeRTOS_inet_addr( pcHostname );

                if( ( ulIPAddress != 0 ) && ( ulIPAddress != ~0uL ) )
                {
                    xOptions.xIPVersion = 4;
                }
            }
        #endif /* if ( ipconfigUSE_IPv6 != 0 ) */
        FreeRTOS_printf( ( "ping%d: looking up name '%s'\n", ( int ) xOptions.xIPVersion, pcHostname ) );

        if( xOptions.xDoClear )
        {
            clear_caches();
        }

        FreeRTOS_printf( ( "Calling pxDNSLookup\n" ) );
        pxDNSResult = pxDNSLookup( pcHostname, xOptions.xIPVersion, xOptions.xAsynchronous, xOptions.xDoClear );

        if( pxDNSResult != NULL )
        {
            #if ( ipconfigUSE_IPv6 != 0 )
                if( xOptions.xIPVersion == 6 )
                {
                    FreeRTOS_printf( ( "ping6 to '%s' (%pip)\n", pcHostname, pxDNSResult->xPrivateStorage.sockaddr6.sin_addrv6.ucBytes ) );
                    xPing4Count = -1;
                    xPing6Count = 0;
                    memcpy( xPing6IPAddress.ucBytes, pxDNSResult->xPrivateStorage.sockaddr6.sin_addrv6.ucBytes, ipSIZE_OF_IPv6_ADDRESS );
                    FreeRTOS_SendPingRequestIPv6( &xPing6IPAddress, uxPingSize, PING_TIMEOUT );
                    uxPingTimes[ 0 ] = ( TickType_t ) ullGetHighResolutionTime();
                }
                else
            #endif /* if ( ipconfigUSE_IPv6 != 0 ) */
            {
                FreeRTOS_printf( ( "ping4 to '%s' (%xip)\n", pcHostname, ( unsigned ) FreeRTOS_ntohl( pxDNSResult->ai_addr->sin_addr ) ) );
                xPing4Count = 0;
                #if ( ipconfigUSE_IPv6 != 0 )
                    xPing6Count = -1;
                #endif
                ulPingIPAddress = pxDNSResult->ai_addr->sin_addr;
                xARPWaitResolution( ulPingIPAddress, pdMS_TO_TICKS( 5000U ) );
                FreeRTOS_SendPingRequest( ulPingIPAddress, uxPingSize, PING_TIMEOUT );
                uxPingTimes[ 0 ] = ( TickType_t ) ullGetHighResolutionTime();
            }
        }
        else
        {
            FreeRTOS_printf( ( "ping -%d: '%s' not found\n", ( int ) xOptions.xIPVersion, ptr ) );
        }
    }
/*-----------------------------------------------------------*/

#else /* ipconfigMULTI_INTERFACE != 0 */

    static void handle_ping( char * pcBuffer )
    {
        uint32_t ulIPAddress;
        char * ptr = pcBuffer;

        PING_COUNT_MAX = 10;

        CommandOptions_t xOptions;

        uxGetOptions( &( xOptions ), ( const char ** ) &( ptr ) );

        pingLogging = xOptions.xLogging;

        while( isspace( *ptr ) )
        {
            ptr++;
        }

        FreeRTOS_GetAddressConfiguration( &ulIPAddress, NULL, NULL, NULL );

        if( *ptr != 0 )
        {
            char * rest = strchr( ptr, ' ' );

            if( rest )
            {
                *( rest++ ) = '\0';
            }

            ulIPAddress = FreeRTOS_inet_addr( ptr );

            while( *ptr && !isspace( *ptr ) )
            {
                ptr++;
            }

            unsigned count;

            if( ( rest != NULL ) && ( sscanf( rest, "%u", &count ) > 0 ) )
            {
                PING_COUNT_MAX = count;
            }
        }

        FreeRTOS_printf( ( "ping to %xip\n", ( unsigned ) FreeRTOS_htonl( ulIPAddress ) ) );

        ulPingIPAddress = ulIPAddress;
        xPing4Count = 0;
        xPingReady = pdFALSE;

        if( xOptions.xDoClear )
        {
            FreeRTOS_ClearARP();
            FreeRTOS_printf( ( "Clearing ARP cache\n" ) );
        }

        FreeRTOS_SendPingRequest( ulIPAddress, uxPingSize, PING_TIMEOUT );
        uxPingTimes[ 0 ] = ( TickType_t ) ullGetHighResolutionTime();
    }
#endif /* ( ipconfigMULTI_INTERFACE != 0 ) */
/*-----------------------------------------------------------*/

static void handle_http( char * pcBuffer )
{
    char * ptr = pcBuffer;
    CommandOptions_t xOptions;

    uxGetOptions( &( xOptions ), ( const char ** ) &( ptr ) );

    while( isspace( ( uint8_t ) *ptr ) )
    {
        ptr++;
    }

    if( *ptr != 0 )
    {
        char * pcHost = ptr;
        char * pcFileName;

        while( ( *ptr != 0 ) && ( !isspace( ( uint8_t ) *ptr ) ) )
        {
            ptr++;
        }

        if( isspace( ( uint8_t ) *ptr ) )
        {
            ptr[ 0 ] = 0;
            ptr++;
        }

        while( isspace( ( uint8_t ) *ptr ) )
        {
            ptr++;
        }

        pcFileName = ptr;

        if( xOptions.xDoClear )
        {
            clear_caches();
        }

        vStartHTTPClientTest( configMINIMAL_STACK_SIZE * 8, tskIDLE_PRIORITY + 1 );
        wakeupHTTPClient( 0U, pcHost, pcFileName, xOptions.xIPVersion );
    }
    else
    {
        FreeRTOS_printf( ( "Usage: http <hostname>\n" ) );
    }
}
/*-----------------------------------------------------------*/

void vApplicationPingReplyHook( ePingReplyStatus_t eStatus,
                                uint16_t usIdentifier )
{
    uxPingTimes[ 1 ] = ( TickType_t ) ullGetHighResolutionTime();

    if( ( xPing4Count >= 0 ) && ( xPing4Count < PING_COUNT_MAX ) )
    {
        xPing4Count++;

        if( pingLogging || ( xPing4Count == PING_COUNT_MAX ) )
        {
            FreeRTOS_printf( ( "Received reply %d: status %d ID %04x\n", ( int ) xPing4Count, ( int ) eStatus, usIdentifier ) );
        }
    }

    #if ( ipconfigUSE_IPv6 != 0 )
        if( ( xPing6Count >= 0 ) && ( xPing6Count < PING_COUNT_MAX ) )
        {
            xPing6Count++;

            if( pingLogging || ( xPing6Count == PING_COUNT_MAX ) )
            {
                FreeRTOS_printf( ( "Received reply %d: status %d ID %04x\n", ( int ) xPing6Count, ( int ) eStatus, usIdentifier ) );
            }
        }
    #endif
    xPingReady = pdTRUE;

    if( xServerSemaphore != NULL )
    {
        xSemaphoreGive( xServerSemaphore );
    }
}
/*-----------------------------------------------------------*/

void xHandleTesting()
{
    /* A counter to check if the application calls this work function.
     * It should be called regularly from the application.
     * he application will block on 'xServerSemaphore'. */
    ulWorkCount++;

    if( xPingReady )
    {
        TickType_t uxTimeTicks = uxPingTimes[ 1 ] - uxPingTimes[ 0 ];
        #if ( ipconfigUSE_IPv6 != 0 )
            FreeRTOS_printf( ( "xPingReady %d xPing4 %d xPing6 %d  Delta %u\n", ( int ) xPingReady, ( int ) xPing4Count, ( int ) xPing6Count, ( unsigned ) uxTimeTicks ) );
        #else
            FreeRTOS_printf( ( "xPingReady %d xPing4 %d  Delta %u\n", ( int ) xPingReady, ( int ) xPing4Count, ( unsigned ) uxTimeTicks ) );
        #endif
        xPingReady = pdFALSE;

        if( ( xPing4Count >= 0 ) && ( xPing4Count < PING_COUNT_MAX ) )
        {
            /* 10 bytes, 100 clock ticks. */
            FreeRTOS_SendPingRequest( ulPingIPAddress, uxPingSize, PING_TIMEOUT );
            uxPingTimes[ 0 ] = ( TickType_t ) ullGetHighResolutionTime();
        }

        #if ( ipconfigUSE_IPv6 != 0 )
            if( ( xPing6Count >= 0 ) && ( xPing6Count < PING_COUNT_MAX ) )
            {
                FreeRTOS_SendPingRequestIPv6( &xPing6IPAddress, uxPingSize, PING_TIMEOUT );
                uxPingTimes[ 0 ] = ( TickType_t ) ullGetHighResolutionTime();
            }
        #endif
    }
}
/*-----------------------------------------------------------*/

#if ( ipconfigMULTI_INTERFACE != 0 )
    static struct freertos_addrinfo * pxDNSLookup( char * pcHost,
                                                   BaseType_t xIPVersion,
                                                   BaseType_t xAsynchronous,
                                                   BaseType_t xDoClear )
    {
        #if ( ipconfigMULTI_INTERFACE != 0 )
            struct freertos_addrinfo xHints;
            struct freertos_addrinfo * pxResult = NULL;

            memset( &( xHints ), 0, sizeof( xHints ) );

            if( xIPVersion == 6 )
            {
                xHints.ai_family = FREERTOS_AF_INET6;
            }
            else
            {
                xHints.ai_family = FREERTOS_AF_INET;
            }
        #endif /* if ( ipconfigMULTI_INTERFACE != 0 ) */
        FreeRTOS_printf( ( "pxDNSLookup: '%s' IPv%d %s DNS-clear = %s\n",
                           pcHost, ( int ) xIPVersion, ( xAsynchronous != 0 ) ? " Async" : "Sync", ( xDoClear != 0 ) ? "true" : "false" ) );

        if( xDoClear )
        {
            #if ( ipconfigUSE_DNS_CACHE != 0 )
                {
                    FreeRTOS_dnsclear();
                    FreeRTOS_printf( ( "Clear DNS cache\n" ) );
                }
            #endif /* ipconfigUSE_DNS_CACHE */
            #if ( ipconfigMULTI_INTERFACE != 0 )
                FreeRTOS_ClearARP( NULL );
            #else
                FreeRTOS_ClearARP();
            #endif
            FreeRTOS_printf( ( "Clear ARP cache\n" ) );
        }

        xDNSCount = 0;
        #if ( ipconfigMULTI_INTERFACE != 0 )
            #if ( ipconfigDNS_USE_CALLBACKS != 0 )
                if( xAsynchronous != 0 )
                {
                    uint32_t ulReturn;
                    xApplicationGetRandomNumber( &( ulReturn ) );
                    void * pvSearchID = ( void * ) ulReturn;

                    BaseType_t rc = FreeRTOS_getaddrinfo_a(
                        pcHost,    /* The node. */
                        NULL,      /* const char *pcService: ignored for now. */
                        &xHints,   /* If not NULL: preferences. */
                        &pxResult, /* An allocated struct, containing the results. */
                        vDNSEvent,
                        pvSearchID,
                        5000 );

                    FreeRTOS_printf( ( "dns query%d: '%s' = %d\n", ( int ) xIPVersion, pcHost, ( int ) rc ) );

                    if( pxResult != NULL )
                    {
                        show_addressinfo( pxResult );
                    }
                }
                else
            #endif /* if ( ipconfigDNS_USE_CALLBACKS != 0 ) */
            {
                #if ( ipconfigDNS_USE_CALLBACKS == 0 )
                    if( xAsynchronous != 0 )
                    {
                        FreeRTOS_printf( ( "ipconfigDNS_USE_CALLBACKS is not defined\n" ) );
                    }
                #endif
                BaseType_t rc = FreeRTOS_getaddrinfo(
                    pcHost,      /* The node. */
                    NULL,        /* const char *pcService: ignored for now. */
                    &xHints,     /* If not NULL: preferences. */
                    &pxResult ); /* An allocated struct, containing the results. */
                FreeRTOS_printf( ( "FreeRTOS_getaddrinfo: rc %d\n", ( int ) rc ) );

                if( pxResult != NULL )
                {
                    show_addressinfo( pxResult );
                }

                if( rc != 0 )
                {
                    FreeRTOS_printf( ( "dns query%d: '%s' No results\n", ( int ) xIPVersion, pcHost ) );
                }
                else
                {
                    #if ( ipconfigUSE_IPv6 != 0 )
                        if( xIPVersion == 6 )
                        {
                            struct freertos_sockaddr6 * pxAddr6;
                            pxAddr6 = ( struct freertos_sockaddr6 * ) pxResult->ai_addr;

                            FreeRTOS_printf( ( "dns query%d: '%s' = %pip rc = %d\n", ( int ) xIPVersion, pcHost, pxAddr6->sin_addrv6.ucBytes, ( int ) rc ) );
                        }
                        else
                    #endif /* ipconfigUSE_IPv6 */
                    {
                        uint32_t luIPAddress = pxResult->ai_addr->sin_addr;
                        FreeRTOS_printf( ( "dns query%d: '%s' = %lxip rc = %d\n", ( int ) xIPVersion, pcHost, FreeRTOS_ntohl( luIPAddress ), ( int ) rc ) );
                    }
                }
            }
        #endif /* ipconfigMULTI_INTERFACE */

        #if ( ipconfigDNS_USE_CALLBACKS != 0 ) && ( ipconfigMULTI_INTERFACE != 0 )
            if( ( pxResult == NULL ) && ( xAsynchronous != 0 ) )
            {
                #if ( ipconfigUSE_IPv6 != 0 )
                    IPv6_Address_t xAddress_IPv6;
                #endif /* ( ipconfigUSE_IPv6 != 0 ) */
                uint32_t ulIpAddress;
                int iCount;

                for( iCount = 0; iCount < 10; iCount++ )
                {
                    ulTaskNotifyTake( pdTRUE, 1000 );

                    if( xDNSCount != 0 )
                    {
                        break;
                    }
                }

                vTaskDelay( 333 );
                pxResult = ( struct freertos_addrinfo * ) pvPortMalloc( sizeof( *pxResult ) );

                if( pxResult != NULL )
                {
                    memset( pxResult, '\0', sizeof( *pxResult ) );
                    pxResult->ai_canonname = pxResult->xPrivateStorage.ucName;
                    strncpy( pxResult->xPrivateStorage.ucName, pcHost, sizeof( pxResult->xPrivateStorage.ucName ) );

                    #if ( ipconfigUSE_IPv6 != 0 )
                        {
                            pxResult->ai_addr = ( sockaddr4_t * ) &( pxResult->xPrivateStorage.sockaddr6 );
                        }
                    #else
                        {
                            pxResult->ai_addr = &( pxResult->xPrivateStorage.sockaddr4 );
                        }
                    #endif

                    #if ( ipconfigUSE_IPv6 != 0 )
                        memset( xAddress_IPv6.ucBytes, '\0', sizeof( xAddress_IPv6.ucBytes ) );

                        if( xIPVersion == 6 )
                        {
                            #if ( ipconfigUSE_DNS_CACHE == 1 ) && ( ipconfigUSE_IPv6 != 0 )
                                FreeRTOS_dnslookup6( pcHost, &( xAddress_IPv6 ) );
                                FreeRTOS_printf( ( "Lookup6 '%s' = %pip\n", pcHost, xAddress_IPv6.ucBytes ) );
                            #endif
                            pxResult->ai_family = FREERTOS_AF_INET6;
                            pxResult->ai_addrlen = ipSIZE_OF_IPv6_ADDRESS;
                            memcpy( pxResult->xPrivateStorage.sockaddr6.sin_addrv6.ucBytes, xAddress_IPv6.ucBytes, ipSIZE_OF_IPv6_ADDRESS );
                        }
                        else
                    #endif /* ( ipconfigUSE_IPv6 != 0 ) */
                    {
                        #if ( ipconfigUSE_DNS_CACHE != 0 )
                            ulIpAddress = FreeRTOS_dnslookup( pcHost );
                            FreeRTOS_printf( ( "Lookup4 '%s' = %lxip\n", pcHost, FreeRTOS_ntohl( ulIpAddress ) ) );
                            pxResult->ai_addr->sin_addr = ulIpAddress;
                            pxResult->ai_family = FREERTOS_AF_INET4;
                            pxResult->ai_addrlen = ipSIZE_OF_IPv4_ADDRESS;
                        #endif
                    }
                }
            }
        #endif /* if ( ipconfigDNS_USE_CALLBACKS != 0 ) && ( ipconfigMULTI_INTERFACE != 0 ) */
        #if ( ipconfigMULTI_INTERFACE != 0 )
            /* Don't forget to call FreeRTOS_freeaddrinfo() */
            return pxResult;
        #else
            return 0;
        #endif
    }
#endif /* ( ipconfigMULTI_INTERFACE != 0 ) */
/*-----------------------------------------------------------*/

#if ( ipconfigUSE_IPv6 != 0 )
    static void vDNSEvent( const char * pcName,
                           void * pvSearchID,
                           struct freertos_addrinfo * pxAddrInfo )
    {
        ( void ) pvSearchID;
        const struct freertos_sockaddr6 * pxAddress6 = ( const struct freertos_sockaddr6 * ) pxAddrInfo->ai_addr;

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
                FreeRTOS_printf( ( "vDNSEvent/v6: '%s' timed out\n", pcName ) );
            }
            else
            {
                FreeRTOS_printf( ( "vDNSEvent/v6: found '%s' on %pip\n", pcName, pxAddress6->sin_addrv6.ucBytes ) );
            }
        }
        else
        {
            struct freertos_sockaddr * pxAddress4 = ( struct freertos_sockaddr * ) pxAddress6;

            if( pxAddress4->sin_addr == 0uL )
            {
                FreeRTOS_printf( ( "vDNSEvent/v4: '%s' timed out\n", pcName ) );
            }
            else
            {
                FreeRTOS_printf( ( "vDNSEvent/v4: found '%s' on %lxip\n", pcName, FreeRTOS_ntohl( pxAddress4->sin_addr ) ) );
            }
        }

        if( xServerWorkTaskHandle != NULL )
        {
            xDNSCount++;
            xTaskNotifyGive( xServerWorkTaskHandle );
        }
    }
#else /* if ( ipconfigUSE_IPv6 != 0 ) */
    static void vDNSEvent( const char * pcName,
                           void * pvSearchID,
                           uint32_t ulIPAddress )
    {
        ( void ) pvSearchID;

        FreeRTOS_printf( ( "vDNSEvent: found '%s' on %xip\n", pcName, ( unsigned ) FreeRTOS_ntohl( ulIPAddress ) ) );

        if( xServerWorkTaskHandle != NULL )
        {
            xDNSCount++;
            xTaskNotifyGive( xServerWorkTaskHandle );
        }
    }
#endif /* if ( ipconfigUSE_IPv6 != 0 ) */
/*-----------------------------------------------------------*/

#if ( ipconfigMULTI_INTERFACE != 0 )
    void showEndPoint( NetworkEndPoint_t * pxEndPoint )
    {
        int bWantDHCP, bWantRA;
        const char * pcMethodName;
        size_t uxDNSIndex;

        #if ( ipconfigUSE_DHCP != 0 )
            bWantDHCP = pxEndPoint->bits.bWantDHCP;
        #else
            bWantDHCP = 0;
        #endif
        #if ( ipconfigUSE_RA != 0 )
            bWantRA = pxEndPoint->bits.bWantRA;
        #else
            bWantRA = 0;
        #endif /* ( ipconfigUSE_RA != 0 ) */

        if( bWantDHCP != 0 )
        {
            pcMethodName = "DHCP";
        }
        else if( bWantRA != 0 )
        {
            pcMethodName = "RA";
        }
        else
        {
            pcMethodName = "static";
        }

        #if ( ipconfigUSE_IPv6 != 0 )
            if( pxEndPoint->bits.bIPv6 )
            {
                IPv6_Address_t xPrefix;

                /* Extract the prefix from the IP-address */
                FreeRTOS_CreateIPv6Address( &( xPrefix ), &( pxEndPoint->ipv6_settings.xIPAddress ), pxEndPoint->ipv6_settings.uxPrefixLength, pdFALSE );

                FreeRTOS_printf( ( "IP-address : %pip\n", pxEndPoint->ipv6_settings.xIPAddress.ucBytes ) );

                if( memcmp( pxEndPoint->ipv6_defaults.xIPAddress.ucBytes, pxEndPoint->ipv6_settings.xIPAddress.ucBytes, ipSIZE_OF_IPv6_ADDRESS ) != 0 )
                {
                    FreeRTOS_printf( ( "Default IP : %pip\n", pxEndPoint->ipv6_defaults.xIPAddress.ucBytes ) );
                }

                FreeRTOS_printf( ( "End-point  : up = %s method %s\n", ( pxEndPoint->bits.bEndPointUp != 0 ) ? "yes" : "no", pcMethodName ) );
                FreeRTOS_printf( ( "Prefix     : %pip/%d\n", xPrefix.ucBytes, ( int ) pxEndPoint->ipv6_settings.uxPrefixLength ) );
                FreeRTOS_printf( ( "GW         : %pip\n", pxEndPoint->ipv6_settings.xGatewayAddress.ucBytes ) );

                for( uxDNSIndex = 0U; uxDNSIndex < ipconfigENDPOINT_DNS_ADDRESS_COUNT; uxDNSIndex++ )
                {
                    FreeRTOS_printf( ( "DNS-%u      : %pip\n", uxDNSIndex, pxEndPoint->ipv6_settings.xDNSServerAddresses[ uxDNSIndex ].ucBytes ) );
                }
            }
            else
        #endif /* ( ipconfigUSE_IPv6 != 0 ) */
        {
            FreeRTOS_printf( ( "IP-address : %lxip\n",
                               FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulIPAddress ) ) );

            if( pxEndPoint->ipv4_settings.ulIPAddress != pxEndPoint->ipv4_defaults.ulIPAddress )
            {
                FreeRTOS_printf( ( "Default IP : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_defaults.ulIPAddress ) ) );
            }

            FreeRTOS_printf( ( "End-point  : up = %s method %s\n", pxEndPoint->bits.bEndPointUp ? "yes" : "no", pcMethodName ) );

            FreeRTOS_printf( ( "Net mask   : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulNetMask ) ) );
            FreeRTOS_printf( ( "GW         : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulGatewayAddress ) ) );

            for( uxDNSIndex = 0U; uxDNSIndex < ipconfigENDPOINT_DNS_ADDRESS_COUNT; uxDNSIndex++ )
            {
                FreeRTOS_printf( ( "DNS-%u      : %xip\n", uxDNSIndex, ( unsigned ) FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulDNSServerAddresses[ uxDNSIndex ] ) ) );
            }

            FreeRTOS_printf( ( "Broadcast  : %lxip\n", FreeRTOS_ntohl( pxEndPoint->ipv4_settings.ulBroadcastAddress ) ) );
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
/*-----------------------------------------------------------*/

static void clear_caches()
{
    #if ( ipconfigUSE_DNS_CACHE != 0 )
        {
            FreeRTOS_dnsclear();
        }
    #endif /* ipconfigUSE_DNS_CACHE */
    #if ( ipconfigMULTI_INTERFACE != 0 )
        FreeRTOS_ClearARP( NULL );
    #else
        FreeRTOS_ClearARP();
    #endif
    FreeRTOS_printf( ( "Cleared caches.\n" ) );
}
/*-----------------------------------------------------------*/
