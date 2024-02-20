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


#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "FreeRTOS.h"
#include "task.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"

#include "ddos_testing.h"

#include "eventLogging.h"

#define DDOS_TASK_PRIORITY        ( tskIDLE_PRIORITY + 1 )

#define DDOS_TASK_STACK_SIZE      4196
#define DDOS_PAYLOAD_LENGTH       512

#define SERVER_TASK_STACK_SIZE    4196
#define SERVER_TASK_PRIORITY      512

static TaskHandle_t xDDoSTaskHandle = NULL;
static TaskHandle_t xWakeupTashHandle = NULL;
static TickType_t uxDDoSSleepTime = 10;
static BaseType_t xDDoSUDPPortNumber = 7;
static BaseType_t xDDoSTCPPortNumber = 32002;
Socket_t xUDPSocket;
Socket_t xTCPClientSocket;
struct freertos_sockaddr xDDoSFloodAddress;
const TickType_t x1000ms = pdMS_TO_TICKS( 1000UL );
static BaseType_t xDDoSRunning = pdFALSE;
static BaseType_t xDDoSDoLog = pdFALSE;
static TickType_t uxStartTime;
static TickType_t uxDDoSDuration;
static BaseType_t xTCPWantConnect;
static BaseType_t xTCPWantDisconnect;
static BaseType_t xTCPNowConnected;
static uint32_t uxTCPSendCount = 2 * 1024 * 1024;
static uint32_t ulUDPSendCount;
static uint32_t ulDDoSPayloadLength;
static uint32_t ulUDPRecvCount;

uint32_t rxPacketCount;
uint32_t rxPacketLength;

/* Target addresss for TCP client. */
static uint32_t ulTargetTCPIPAddress = FreeRTOS_inet_addr_quick( 192, 168, 2, 2 );

/* Target addresss for UDP DDoD attack. */
static uint32_t ulTargetUDPIPAddress = FreeRTOS_inet_addr_quick( 192, 168, 2, 2 );

static void vTCPWork( void );
struct freertos_sockaddr xEchoServerAddress;


/* No need to call the following functions: */
static void vStartDDoSTask( void );
static void vGetDDoDLogging( void );
static void vSetDDoDRunState( BaseType_t xNewState );

#ifndef USE_UDP_CALLNACK
    #define USE_UDP_CALLNACK    1
#endif

#if ( ipconfigUSE_CALLBACKS != 0 ) && ( USE_UDP_CALLNACK != 0 )
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
        ulUDPRecvCount++;
        {
            NetworkBufferDescriptor_t * pxBuffer = pxUDPPayloadBuffer_to_NetworkBuffer( pvData );

            eventLogAdd( "xOnUdpReceive %p", pxBuffer );
        }
        return 1;
    }
#else /* if ( ipconfigUSE_CALLBACKS != 0 ) && ( USE_UDP_CALLNACK != 0 ) */
    static void vDDoSReceive()
    {
        struct freertos_sockaddr from_address;
        socklen_t xSourceAddressLength = sizeof( from_address );
        BaseType_t xRc;

/*        #error Here */
        xRc = FreeRTOS_recvfrom( xUDPSocket,
                                 ( char * ) NULL,
                                 1500,
                                 FREERTOS_MSG_DONTWAIT,
                                 &( from_address ),
                                 &( xSourceAddressLength ) );

        if( xRc > 0 )
        {
            ulUDPRecvCount++;
        }
    }
#endif /* if ( ipconfigUSE_CALLBACKS != 0 ) && ( USE_UDP_CALLNACK != 0 ) */

static void prvDDoSTask( void * pvparameter )
{
    char pcBuffer[ DDOS_PAYLOAD_LENGTH ];
    struct freertos_sockaddr xBindAddress;

    ulUDPSendCount = 0;
    ulDDoSPayloadLength = DDOS_PAYLOAD_LENGTH;

    ( void ) pvparameter;
    memset( pcBuffer, 'x', sizeof pcBuffer );
    /* Create the socket. */
    xUDPSocket = FreeRTOS_socket( FREERTOS_AF_INET,
                                  FREERTOS_SOCK_DGRAM,
                                  FREERTOS_IPPROTO_UDP );

    /* Check the socket was created. */
    configASSERT( xSocketValid( xUDPSocket ) == pdTRUE );

    xBindAddress.sin_len = sizeof( xBindAddress );
    xBindAddress.sin_address.ulIP_IPv4 = FreeRTOS_GetIPAddress();
    xBindAddress.sin_port = FreeRTOS_htons( xDDoSUDPPortNumber );
    xBindAddress.sin_family = FREERTOS_AF_INET4;
    FreeRTOS_bind( xUDPSocket, &xBindAddress, sizeof( xBindAddress ) );
    FreeRTOS_setsockopt( xUDPSocket, 0, FREERTOS_SO_SNDTIMEO, &x1000ms, sizeof( x1000ms ) );

    #if ( ipconfigUSE_CALLBACKS != 0 ) && ( USE_UDP_CALLNACK != 0 )
        {
            F_TCP_UDP_Handler_t xHandler;
            memset( &xHandler, 0, sizeof( xHandler ) );
            xHandler.pOnUdpReceive = xOnUdpReceive;
            FreeRTOS_setsockopt( xUDPSocket, 0, FREERTOS_SO_UDP_RECV_HANDLER, ( void * ) &( xHandler ), sizeof( xHandler ) );
        }
    #endif

    FreeRTOS_printf( ( "prvDDoSTask started\n" ) );

    for( ; ; )
    {
        if( xDDoSRunning != 0 )
        {
            int32_t rc = FreeRTOS_sendto( xUDPSocket,
                                          pcBuffer,
                                          ulDDoSPayloadLength,
                                          0,
                                          &( xDDoSFloodAddress ),
                                          sizeof( xDDoSFloodAddress ) );

            if( rc >= 0 )
            {
                ulUDPSendCount++;
                TickType_t xNow = xTaskGetTickCount();

                if( xNow > uxStartTime + uxDDoSDuration )
                {
                    FreeRTOS_printf( ( "prvDDoSTask ready, %u packets sent to %xip\n",
                                       ( unsigned ) ulUDPSendCount, ( unsigned ) FreeRTOS_ntohl( ulTargetUDPIPAddress ) ) );
                    xDDoSRunning = pdFALSE;
                }
            }
            else
            {
                FreeRTOS_printf( ( "FreeRTOS_sendto: sendto failed: %d\n", ( int ) rc ) );
                xDDoSRunning = pdFALSE;
            }
        }

        if( xDDoSDoLog != pdFALSE )
        {
            xDDoSDoLog = pdFALSE;
            FreeRTOS_printf( ( "DDoS: sent %u packets\n", ( unsigned ) ulUDPSendCount ) );
            ulUDPSendCount = 0U;
        }

        if( xDDoSRunning == 0 )
        {
            vTaskDelay( 100U );
        }
        else if( uxDDoSSleepTime != 0U )
        {
            vTaskDelay( uxDDoSSleepTime );
        }

        vTCPWork();
    }
}
/*-----------------------------------------------------------*/

static void vStartDDoSTask( void )
{
    /* Custom task for testing. */
    uxStartTime = xTaskGetTickCount();

    if( xDDoSTaskHandle == NULL )
    {
        xTaskCreate( prvDDoSTask, "DDoSTask", DDOS_TASK_STACK_SIZE, NULL, DDOS_TASK_PRIORITY, &xDDoSTaskHandle );
    }
}
/*-----------------------------------------------------------*/

static BaseType_t xGetParameters( char ** ppcCommand )
{
    BaseType_t xReturn = pdFALSE;
    char * pcCommand = *( ppcCommand );

    if( ( pcCommand != NULL ) && ( *pcCommand != 0 ) )
    {
        while( isspace( ( uint8_t ) *pcCommand ) != 0 )
        {
            pcCommand++;
        }

        if( *pcCommand != 0 )
        {
            xReturn = pdTRUE;
            *( ppcCommand ) = pcCommand;
        }
    }

    return xReturn;
}

void vDDoSCommand( char * pcCommand )
{
    unsigned ulCode, ulSleep_time, ulDuration;
    unsigned uIPAddress[ 4 ];

    if( xGetParameters( &( pcCommand ) ) != 0 )
    {
        BaseType_t doUsage = pdTRUE;

        ulSleep_time = 1u;
        ulDuration = 1000u;
        /* ddos  192.168.2.5  1  0  1000 */
        int rc = sscanf( pcCommand, "%d.%d.%d.%d %d %d %d",
                         uIPAddress + 0,
                         uIPAddress + 1,
                         uIPAddress + 2,
                         uIPAddress + 3,
                         &( ulCode ),
                         &( ulSleep_time ),
                         &( ulDuration ) );

        if( ( ulCode != 0 ) && ( rc > 5 ) )
        {
            ulTargetUDPIPAddress =
                ( ( uIPAddress[ 0 ] & 0xFFu ) << 24 ) |
                ( ( uIPAddress[ 1 ] & 0xFFu ) << 16 ) |
                ( ( uIPAddress[ 2 ] & 0xFFu ) << 8 ) |
                ( ( uIPAddress[ 3 ] & 0xFFu ) );
            ulTargetUDPIPAddress = FreeRTOS_ntohl( ulTargetUDPIPAddress );

            if( ( ulCode == 1 ) && ( rc >= 3 ) )
            {
                /* Send a UDP packet every xx ms. */
                uxDDoSSleepTime = pdMS_TO_TICKS( ulSleep_time );
                uxDDoSDuration = pdMS_TO_TICKS( ulDuration );
                FreeRTOS_printf( ( "Start UDP attack to %xip with %lu ms delays for %lu ms\n",
                                   ( unsigned ) FreeRTOS_ntohl( ulTargetUDPIPAddress ),
                                   ( unsigned long ) uxDDoSSleepTime,
                                   ( unsigned long ) uxDDoSDuration ) );

                #warning disabled xARPWaitResolution()
                /*xARPWaitResolution( ulTargetUDPIPAddress, pdMS_TO_TICKS( 500U ) ); */
                xDDoSFloodAddress.sin_len = sizeof( xDDoSFloodAddress );
                xDDoSFloodAddress.sin_address.ulIP_IPv4 = ulTargetUDPIPAddress;
                xDDoSFloodAddress.sin_port = FreeRTOS_htons( xDDoSUDPPortNumber );
                xDDoSFloodAddress.sin_family = FREERTOS_AF_INET;

                vStartDDoSTask();
                vSetDDoDRunState( pdTRUE );
                doUsage = pdFALSE;
            }
            else if( ulCode == 0 )
            {
                vSetDDoDRunState( pdFALSE );
                doUsage = pdFALSE;
            }
            else
            {
                vGetDDoDLogging();
                doUsage = pdFALSE;
            }
        }

        if( doUsage != 0 )
        {
            FreeRTOS_printf( ( "usage: ddos <IP-address> <0/1> <sleep-time> <duration>\n" ) );
        }
    }
    else
    {
        vGetDDoDLogging();
    }
}
/*-----------------------------------------------------------*/

#if ( ipconfigNETWORK_BUFFER_DEBUGGING != 0 )
    static uint32_t ulSuccessPercentage = 100u;
    static TickType_t uxStartTime;
    static TickType_t uxBufferDuration;
    static uint32_t ulBuffersRefused = 0;
    static uint32_t ulBuffersPassed = 0;
    BaseType_t xAllocBufferSucceed( void )
    {
        uint32_t ulValue;
        uint32_t ulReturn = pdTRUE;

        if( ulSuccessPercentage < 100u )
        {
            TickType_t uxNow = xTaskGetTickCount();
            TickType_t uxDiff = uxNow - uxStartTime;

            if( uxDiff > uxBufferDuration )
            {
                ulSuccessPercentage = 100u;
                FreeRTOS_printf( ( "Buffer allocation now 100 perc after %lu/%lu refusals\n",
                                   ulBuffersRefused, ulBuffersRefused + ulBuffersPassed ) );
            }
            else
            {
                if( xApplicationGetRandomNumber( &( ulValue ) ) )
                {
                    ulValue = ( ulValue % 100u );
                    ulReturn = ( ulValue < ulSuccessPercentage ) ? pdTRUE : pdFALSE;

                    if( ulReturn == pdTRUE )
                    {
                        ulBuffersPassed++;
                    }
                    else
                    {
                        ulBuffersRefused++;
                    }
                }
            }
        }

        return ulReturn;
    }
#endif /* if ( ipconfigNETWORK_BUFFER_DEBUGGING != 0 ) */
/*-----------------------------------------------------------*/

/*
 * buffer 80 10000    Let 80% of the buffer allocation succeed for 10 seconds
 * buffer 0 20000     Let all buffer allocations fail for 20 seconds
 */
void vBufferCommand( char * pcCommand )
{
    #if ( ipconfigNETWORK_BUFFER_DEBUGGING != 0 )
        if( xGetParameters( &( pcCommand ) ) != 0 )
        {
            unsigned uPercentage, uDuration;
            int rc = sscanf( pcCommand, "%u %u", &( uPercentage ), &( uDuration ) );

            if( rc >= 2 )
            {
                if( uPercentage <= 100 )
                {
                    {
                        ulSuccessPercentage = 100u;
                        FreeRTOS_printf( ( "Allocation success %u perc for %u msec\n", uPercentage, uDuration ) );
                        vTaskDelay( 120 );
                    }

                    ulBuffersRefused = 0;
                    ulBuffersPassed = 0;
                    ulSuccessPercentage = uPercentage;
                    uxBufferDuration = uDuration;
                    uxStartTime = xTaskGetTickCount();
                }
                else
                {
                    rc = 0;
                }
            }

            if( rc < 2 )
            {
                FreeRTOS_printf( ( "Usage: buffer <percentage> <msec>.\n" ) );
            }
        }
    #else /* if ( ipconfigNETWORK_BUFFER_DEBUGGING != 0 ) */
        FreeRTOS_printf( ( "ipconfigNETWORK_BUFFER_DEBUGGING is not defined.\n" ) );
    #endif /* if ( ipconfigNETWORK_BUFFER_DEBUGGING != 0 ) */
}
/*-----------------------------------------------------------*/

/*
 * server 32002  Create an echoserver on port 32002
 */
static unsigned uServerPortNumber = 32002;
static TaskHandle_t xServerTaskHandle = NULL;
static void prvTCPServTask( void * pvParameter );
void vTCPServerCommand( char * pcCommand )
{
    if( xGetParameters( &( pcCommand ) ) != 0 )
    {
        int rc = sscanf( pcCommand, "%u", &( uServerPortNumber ) );

        if( xServerTaskHandle == NULL )
        {
            if( rc >= 1 )
            {
                xTaskCreate( prvTCPServTask, "ServerTask", SERVER_TASK_STACK_SIZE, NULL, SERVER_TASK_PRIORITY, &xServerTaskHandle );
            }
            else
            {
                FreeRTOS_printf( ( "prvTCPServTask: missing port.\n" ) );
            }
        }
    }
}
/*-----------------------------------------------------------*/

#warning xSocketValid not yet present
/*BaseType_t xSocketValid( Socket_t xSocket ) */
/*{ */
/*    BaseType_t xReturnValue = pdFALSE; */
/* */
/*    / * */
/*     * There are two values which can indicate an invalid socket: */
/*     * FREERTOS_INVALID_SOCKET and NULL.  In order to compare against */
/*     * both values, the code cannot be compliant with rule 11.4, */
/*     * hence the Coverity suppression statement below. */
/*     * / */
/*    / * coverity[misra_c_2012_rule_11_4_violation] * / */
/*    if( ( xSocket != FREERTOS_INVALID_SOCKET ) && ( xSocket != NULL ) ) */
/*    { */
/*        xReturnValue = pdTRUE; */
/*    } */
/* */
/*    return xReturnValue; */
/*} */

void prvTCPServTask( void * pvParameter )
{
    Socket_t xTCPServerSocket, xTCPClientSocket;
    struct freertos_sockaddr xBindAddress;
    TickType_t xReceiveTimeOut = pdMS_TO_TICKS( 10000U );
    TickType_t xSendTimeOut = pdMS_TO_TICKS( 10000U );
    char pcRecvBuffer[ 1460 ];

    #if ( ipconfigUSE_TCP_WIN == 1 )
        WinProperties_t xWinProps;
    #endif
    ( void ) pvParameter;
    FreeRTOS_printf( ( "prvTCPServTask: started.\n" ) );

    do
    {
        /* Create a TCP socket. */
        xTCPServerSocket = FreeRTOS_socket( FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP );

        #warning xSocketValid not yet present

        if( xSocketValid( xTCPServerSocket ) != pdTRUE )
        {
            FreeRTOS_printf( ( "prvTCPServTask: FreeRTOS_socket failed: %p\n", xTCPServerSocket ) );
            xTCPServerSocket = NULL;
            break;
        }

        FreeRTOS_printf( ( "prvTCPServTask: created a TCP server socket.\n" ) );

        memset( &xBindAddress, '\0', sizeof xBindAddress );
        xBindAddress.sin_len = sizeof( xBindAddress );
        xBindAddress.sin_address.ulIP_IPv4 = 0U;
        xBindAddress.sin_port = FreeRTOS_htons( uServerPortNumber );
        xBindAddress.sin_family = FREERTOS_AF_INET;

        int rc = FreeRTOS_bind( xTCPServerSocket, &xBindAddress, sizeof xBindAddress );

        if( rc != 0 )
        {
            FreeRTOS_printf( ( "prvTCPServTask: FreeRTOS_bind failed with %d\n", rc ) );
            break;
        }

        /* Set a time out so a missing reply does not cause the task to block
         * indefinitely. */
        FreeRTOS_setsockopt( xTCPServerSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof( xReceiveTimeOut ) );
        FreeRTOS_setsockopt( xTCPServerSocket, 0, FREERTOS_SO_SNDTIMEO, &xSendTimeOut, sizeof( xSendTimeOut ) );

        #if ( ipconfigUSE_TCP_WIN == 1 )
            {
                /* Fill in the buffer and window sizes that will be used by the socket. */
                xWinProps.lTxBufSize = 6 * ipconfigTCP_MSS;
                xWinProps.lTxWinSize = 4;
                xWinProps.lRxBufSize = 6 * ipconfigTCP_MSS;
                xWinProps.lRxWinSize = 4;

                /* Set the window and buffer sizes. */
                FreeRTOS_setsockopt( xTCPServerSocket, 0, FREERTOS_SO_WIN_PROPERTIES, ( void * ) &xWinProps, sizeof( xWinProps ) );
            }
        #endif /* ipconfigUSE_TCP_WIN */
        rc = FreeRTOS_listen( xTCPServerSocket, 1 );

        if( rc != 0 )
        {
            FreeRTOS_printf( ( "prvTCPServTask: listen fails with %d\n", rc ) );
            break;
        }

        for( ; ; )
        {
            struct freertos_sockaddr xRemoteAddress;
            socklen_t uxAddressLength = sizeof( xBindAddress );
            xTCPClientSocket = FreeRTOS_accept( xTCPServerSocket,
                                                &( xBindAddress ),
                                                &( uxAddressLength ) );

            if( xSocketValid( xTCPClientSocket ) == pdTRUE )
            {
                #if ( ipconfigUSE_IPv6 != 0 )
                    FreeRTOS_GetRemoteAddress( xTCPClientSocket, ( struct freertos_sockaddr * ) &xRemoteAddress );
                #else
                    FreeRTOS_GetRemoteAddress( xTCPClientSocket, &xRemoteAddress );
                #endif
                FreeRTOS_printf( ( "prvTCPServTask: FreeRTOS_accept from %lxip port %u\n",
                                   FreeRTOS_ntohl( xRemoteAddress.sin_address.ulIP_IPv4 ),
                                   FreeRTOS_ntohs( xRemoteAddress.sin_port ) ) );

                for( ; ; )
                {
                    /*char *pcBuffer; */
                    int rcRecv = FreeRTOS_recv( xTCPClientSocket,
                                                pcRecvBuffer,
                                                sizeof pcRecvBuffer,
                                                0 );

/*FreeRTOS_printf( ( "rcRecv = %d\n", rcRecv ) ); */
                    if( ( rcRecv < 0 ) && ( rcRecv != -pdFREERTOS_ERRNO_EAGAIN ) )
                    {
                        FreeRTOS_printf( ( "prvTCPServTask: recv returns %d\n", rcRecv ) );
                        break;
                    }

                    if( rcRecv > 0 )
                    {
                        int rcSend = FreeRTOS_send( xTCPClientSocket,
                                                    pcRecvBuffer,
                                                    ( size_t ) rcRecv,
                                                    0 );

/*FreeRTOS_printf( ( "rcSend = %d\n", rcSend ) ); */
                        if( ( rcSend < 0 ) && ( rcSend != -pdFREERTOS_ERRNO_EAGAIN ) )
                        {
                            char pcBuf[ 20 ];
                            FreeRTOS_printf( ( "prvTCPServTask: send returns %d (%s)\n",
                                               rcSend,
                                               FreeRTOS_strerror_r( rcSend, pcBuf, sizeof pcBuf ) ) );
                            break;
                        }

                        if( rcSend != rcRecv )
                        {
                            FreeRTOS_printf( ( "prvTCPServTask: send returns %d not %d\n",
                                               rcSend, rcRecv ) );
                            break;
                        }
                    }
                }

                FreeRTOS_closesocket( xTCPClientSocket );
                xTCPClientSocket = NULL;
            }
        }
    }
    while( ipFALSE_BOOL );

    FreeRTOS_printf( ( "prvTCPServTask: kill this task.\n" ) );
    vTaskDelete( NULL );
}

void vTCPClientCommand( char * pcCommand )
{
    uint32_t ulIPAddress;
    BaseType_t xNeedUsage = pdTRUE;

    if( xTCPNowConnected != 0 )
    {
        FreeRTOS_printf( ( "Still sending data...\n" ) );
        xTCPWantDisconnect = pdTRUE;
        xNeedUsage = pdFALSE;
    }
    else
    {
        if( ( pcCommand != NULL ) && ( *pcCommand != 0 ) )
        {
            unsigned int uIPAddress[ 4 ];
            unsigned int ulByteCount;
            int rc = sscanf( pcCommand, "%u.%u.%u.%u %u",
                             &( uIPAddress[ 0 ] ),
                             &( uIPAddress[ 1 ] ),
                             &( uIPAddress[ 2 ] ),
                             &( uIPAddress[ 3 ] ),
                             &( ulByteCount ) );

            if( ( ulByteCount >= 1024 ) && ( rc >= 5 ) )
            {
                ulIPAddress =
                    ( ( uIPAddress[ 0 ] & 0xFFu ) << 24 ) |
                    ( ( uIPAddress[ 1 ] & 0xFFu ) << 16 ) |
                    ( ( uIPAddress[ 2 ] & 0xFFu ) << 8 ) |
                    ( ( uIPAddress[ 3 ] & 0xFFu ) );
                ulTargetTCPIPAddress = FreeRTOS_ntohl( ulIPAddress );

                uxTCPSendCount = ulByteCount;
                uxDDoSSleepTime = 100U;
                uxDDoSDuration = 0U;
                vStartDDoSTask();

                if( xTCPWantConnect == 0 )
                {
                    xTCPWantConnect = pdTRUE;
                    FreeRTOS_printf( ( "Setting xTCPWantConnect %xip exchange %lu bytes\n",
                                       ( unsigned ) FreeRTOS_ntohl( ulTargetTCPIPAddress ),
                                       ( unsigned long ) uxTCPSendCount ) );
                }
                else
                {
                    FreeRTOS_printf( ( "xTCPWantConnect still set...\n" ) );
                }

                xNeedUsage = pdFALSE;
            }
        }
    }

    if( xNeedUsage != 0 )
    {
        FreeRTOS_printf( ( "client <IP-address> <byte-count>\n" ) );
    }
}

static void vGetDDoDLogging( void )
{
    xDDoSDoLog = pdTRUE;
}
/*-----------------------------------------------------------*/

static void vSetDDoDRunState( BaseType_t xNewState )
{
    const char * word;

    if( xDDoSRunning == xNewState )
    {
        word = "already";
    }
    else
    {
        /* Custom task for testing. */
        uxStartTime = xTaskGetTickCount();
        xDDoSRunning = xNewState;
        ulUDPSendCount = 0U;
        word = "now";
    }

    FreeRTOS_printf( ( "DDoS task %s %s\n",
                       word, ( xNewState != 0 ) ? "running" : "stopped" ) );
}
/*-----------------------------------------------------------*/

static size_t uxBytesReceived = 0U;
static size_t uxBytesSent = 0U;

static BaseType_t xOnTcpReceive( Socket_t xSocket,
                                 void * pData,
                                 size_t xLength )
{
    ( void ) xSocket;
    ( void ) pData;
    uxBytesReceived += xLength;

    TaskHandle_t xHandle = xWakeupTashHandle;

    if( xHandle != NULL )
    {
        xTaskNotifyGive( xHandle );
    }

    return 1;
}
/*-----------------------------------------------------------*/

static void vTCPWork()
{
    #if ( ipconfigUSE_TCP_WIN == 1 )
        WinProperties_t xWinProps;
    #endif
    char pcBuffer[ 1440 ];

    #if ( ipconfigUSE_CALLBACKS == 0 ) || ( USE_UDP_CALLNACK == 0 )
        vDDoSReceive();
    #endif

    while( xTCPWantConnect != 0 )
    {
        TickType_t xReceiveTimeOut = pdMS_TO_TICKS( 10000U );
        TickType_t xSendTimeOut = pdMS_TO_TICKS( 10000U );
        struct freertos_sockaddr xBindAddress;
        struct freertos_sockaddr xLocalAddress;
        BaseType_t rc;

        xTCPNowConnected = pdTRUE;
        xTCPWantDisconnect = pdFALSE;

        xTCPWantConnect = 0;
        uxBytesReceived = 0U;
        uxBytesSent = 0U;

        if( xSocketValid( xTCPClientSocket ) == pdTRUE )
        {
            FreeRTOS_closesocket( xTCPClientSocket );
        }

        xTCPClientSocket = NULL;
        xEchoServerAddress.sin_len = sizeof( xEchoServerAddress );
        xEchoServerAddress.sin_address.ulIP_IPv4 = ulTargetTCPIPAddress;
        xEchoServerAddress.sin_port = FreeRTOS_htons( xDDoSTCPPortNumber );
        xEchoServerAddress.sin_family = FREERTOS_AF_INET;

        FreeRTOS_printf( ( "Creating new socket\n" ) );

        /* Create a TCP socket. */
        xTCPClientSocket = FreeRTOS_socket( FREERTOS_AF_INET, FREERTOS_SOCK_STREAM, FREERTOS_IPPROTO_TCP );

        if( xSocketValid( xTCPClientSocket ) == pdFALSE )
        {
            FreeRTOS_printf( ( "vTCPWork: FreeRTOS_socket failed: %p\n", xTCPClientSocket ) );
            xTCPClientSocket = NULL;
            break;
        }

        FreeRTOS_printf( ( "vTCPWork: created a TCP client socket.\n" ) );

        memset( &xBindAddress, '\0', sizeof xBindAddress );
        xBindAddress.sin_len = sizeof( xBindAddress );
        xBindAddress.sin_address.ulIP_IPv4 = 0U;
        xBindAddress.sin_port = 0U;
        xBindAddress.sin_family = FREERTOS_AF_INET;

        rc = FreeRTOS_bind( xTCPClientSocket, &xBindAddress, sizeof xBindAddress );

        if( rc != 0 )
        {
            FreeRTOS_printf( ( "vTCPWork: FreeRTOS_bind failed with %ld\n", ( long ) rc ) );
            break;
        }

        /* Set a time out so a missing reply does not cause the task to block
         * indefinitely. */
        FreeRTOS_setsockopt( xTCPClientSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof( xReceiveTimeOut ) );
        FreeRTOS_setsockopt( xTCPClientSocket, 0, FREERTOS_SO_SNDTIMEO, &xSendTimeOut, sizeof( xSendTimeOut ) );

        #if ( ipconfigUSE_TCP_WIN == 1 )
            {
                /* Fill in the buffer and window sizes that will be used by the socket. */
                xWinProps.lTxBufSize = 6 * ipconfigTCP_MSS;
                xWinProps.lTxWinSize = 4;
                xWinProps.lRxBufSize = 6 * ipconfigTCP_MSS;
                xWinProps.lRxWinSize = 4;

                /* Set the window and buffer sizes. */
                FreeRTOS_setsockopt( xTCPClientSocket, 0, FREERTOS_SO_WIN_PROPERTIES, ( void * ) &xWinProps, sizeof( xWinProps ) );
            }
        #endif /* ipconfigUSE_TCP_WIN */

        #if ( ipconfigUSE_IPv6 != 0 )
            FreeRTOS_GetLocalAddress( xTCPClientSocket, ( struct freertos_sockaddr * ) &xLocalAddress );
        #else
            FreeRTOS_GetLocalAddress( xTCPClientSocket, &xLocalAddress );
        #endif
        /* Connect to the echo server. */
        rc = FreeRTOS_connect( xTCPClientSocket, &xEchoServerAddress, sizeof( xEchoServerAddress ) );

        FreeRTOS_printf( ( "vTCPWork: FreeRTOS_connect to %xip:%u to %xip:%u: rc %ld\n",
                           ( unsigned ) FreeRTOS_ntohl( xLocalAddress.sin_address.ulIP_IPv4 ),
                           FreeRTOS_ntohs( xLocalAddress.sin_port ),
                           ( unsigned ) FreeRTOS_ntohl( xEchoServerAddress.sin_address.ulIP_IPv4 ),
                           FreeRTOS_ntohs( xEchoServerAddress.sin_port ),
                           ( long ) rc ) );

        if( rc != 0 )
        {
            break;
        }

        #if ( ipconfigUSE_CALLBACKS != 0 )
            {
                F_TCP_UDP_Handler_t xHandler;
                memset( &xHandler, '\0', sizeof( xHandler ) );
                xHandler.pxOnTCPReceive = xOnTcpReceive;
                FreeRTOS_setsockopt( xTCPClientSocket, 0, FREERTOS_SO_TCP_RECV_HANDLER, ( void * ) &xHandler, sizeof( xHandler ) );
            }
        #endif

        memset( pcBuffer, 'x', sizeof( pcBuffer ) );

        size_t uxBytesToSend = uxTCPSendCount;
        TickType_t uxStartTime = xTaskGetTickCount();
        TickType_t uxMeasureTime = xTaskGetTickCount();
        TickType_t ulMaxBlockTime = pdMS_TO_TICKS( 10UL );
        uint32_t ulBytes[ 2 ] = { 0U, 0U };

        while( uxBytesToSend > 0 )
        {
            size_t uxSendSize = uxBytesToSend;

            if( uxSendSize > sizeof( pcBuffer ) )
            {
                uxSendSize = sizeof( pcBuffer );
            }

            BaseType_t rc;

            rc = FreeRTOS_recv( xTCPClientSocket, /* The socket being received from. */
                                NULL,             /* The buffer into which the received data will be written. */
                                1460,             /* The size of the buffer provided to receive the data. */
                                FREERTOS_MSG_DONTWAIT );

            if( ( rc < 0 ) && ( rc != -pdFREERTOS_ERRNO_EAGAIN ) )
            {
                FreeRTOS_printf( ( "FreeRTOS_recv returns %ld\n", ( long ) rc ) );
                break;
            }

            rc = FreeRTOS_send( xTCPClientSocket,
                                pcBuffer,
                                uxSendSize,
                                0 );

            #if ( ipconfigUSE_CALLBACKS == 0 ) || ( USE_UDP_CALLNACK == 0 )
                vDDoSReceive();
            #endif

            if( xTCPWantDisconnect != 0 )
            {
                xTCPWantDisconnect = pdFALSE;
                FreeRTOS_printf( ( "User stop\n" ) );
                break;
            }

            if( rc < ( BaseType_t ) uxSendSize )
            {
                FreeRTOS_printf( ( "vTCPWork: FreeRTOS_send returns %ld in stead of %u after sending %u bytes.\n",
                                   ( long ) rc, ( unsigned ) uxSendSize, ( unsigned ) uxBytesSent ) );

                if( ( rc < 0 ) && ( rc != -pdFREERTOS_ERRNO_ENOSPC ) )
                {
                    break;
                }
            }

            if( rc > 0 )
            {
                uxBytesToSend -= rc;

                TickType_t uxNow = xTaskGetTickCount();
                TickType_t uxPassed = uxNow - uxMeasureTime;
                uxBytesSent += rc;
                ulBytes[ 0 ] += rc;

                if( uxPassed >= 1000U )
                {
                    uint32_t ulRecv = uxBytesReceived - ulBytes[ 1 ];
                    uint32_t ulBitCount = 8U * ( ulBytes[ 0 ] + ulRecv );

                    FreeRTOS_printf( ( "vTCPWork: %u + %u bytes = %u bits  DDoS packets %u\n",
                                       ( unsigned ) ulBytes[ 0 ],
                                       ( unsigned ) ulRecv,
                                       ( unsigned ) ulBitCount,
                                       ( unsigned ) ulUDPRecvCount ) );
                    uxMeasureTime = uxNow;
                    ulBytes[ 0 ] = 0U;
                    ulBytes[ 1 ] = uxBytesReceived;
                }
            }

            size_t uxReceived = uxBytesReceived;
            #if ( ipconfigUSE_TCP_WIN == 1 )
                size_t uxDiff = ( uxSendSize > 0U ) ? ( ( size_t ) xWinProps.lRxWinSize ) : 0U;
            #else
                size_t uxDiff = ( uxSendSize > 0U ) ? ( ( size_t ) ipconfigTCP_MSS ) : 0U;
            #endif

            for( ; ; )
            {
                if( uxBytesReceived + uxDiff < uxBytesSent )
                {
                    xWakeupTashHandle = xDDoSTaskHandle;
                    /* This task will be woken up after receiving more TCP data. */
                    ulTaskNotifyTake( pdFALSE, ulMaxBlockTime );
                    xWakeupTashHandle = NULL;
                }

                if( uxBytesReceived == uxBytesSent )
                {
                    /* Received all bytes. */
                    break;
                }

                if( uxReceived == uxBytesReceived )
                {
                    /* No more bytes received for 10 ms. */
                    break;
                }

                uxReceived = uxBytesReceived;
            }
        }

        FreeRTOS_printf( ( " uxBytesReceived = %u uxBytesSent = %u\n", uxBytesReceived, uxBytesSent ) );
        FreeRTOS_printf( ( "All data exchanged, starting shutdown\n" ) );
        FreeRTOS_shutdown( xTCPClientSocket, FREERTOS_SHUT_RDWR );

        /* Expect FreeRTOS_recv() to return an error once the shutdown is
         * complete. */
        TickType_t uxEndTime = xTaskGetTickCount();
        TickType_t uxDiff = uxEndTime - uxStartTime;
        FreeRTOS_printf( ( "Exchanged %lu bytes in %lu msec\n",
                           ( unsigned long ) uxBytesSent,
                           ( unsigned long ) uxDiff ) );

        TickType_t xTimeOnEntering = xTaskGetTickCount();

        do
        {
            BaseType_t xReturned = FreeRTOS_recv( xTCPClientSocket,   /* The socket being received from. */
                                                  pcBuffer,           /* The buffer into which the received data will be written. */
                                                  sizeof( pcBuffer ), /* The size of the buffer provided to receive the data. */
                                                  0 );

            if( xReturned < 0 )
            {
                FreeRTOS_printf( ( "Shutdown is confirmed by peer\n" ) );
                break;
            }
        } while( ( xTaskGetTickCount() - xTimeOnEntering ) < xReceiveTimeOut );
    } /* while( xTCPWantConnect != 0 ) */

    xTCPNowConnected = pdFALSE;

    if( xTCPClientSocket != NULL )
    {
        FreeRTOS_closesocket( xTCPClientSocket );
        xTCPClientSocket = NULL;
    }
}
