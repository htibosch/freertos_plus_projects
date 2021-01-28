/*
    Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
    Copyright (C) 2012 - 2018 Xilinx, Inc. All Rights Reserved.

    Permission is hereby granted, free of charge, to any person obtaining a copy of
    this software and associated documentation files (the "Software"), to deal in
    the Software without restriction, including without limitation the rights to
    use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
    the Software, and to permit persons to whom the Software is furnished to do so,
    subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software. If you wish to use our Amazon
    FreeRTOS name, please do so in a fair use way that does not cause confusion.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
    FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
    COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
    IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    http://www.FreeRTOS.org
    http://aws.amazon.com/freertos


    1 tab == 4 spaces!
*/

/* Standard includes. */
#include <stdio.h>
#include <time.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"
#include "semphr.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "NetworkBufferManagement.h"
#include "FreeRTOS_IP_Private.h"
#include "xilinx_ultrascale/x_emacpsif.h"

/* Xilinx includes. */
#include "xil_printf.h"
#include "xparameters.h"

#include "iperf_task.h"
#include "UDPLoggingPrintf.h"
#include "hr_gettime.h"

/* Define names that will be used for SDN, LLMNR and NBNS searches. */
#define mainHOST_NAME					"zynqMP"

static const uint8_t ucIPAddress[ 4 ] =       { 192, 168,   2, 124 };
static const uint8_t ucNetMask[ 4 ] =         { 255, 255, 255,   0 };
static const uint8_t ucGatewayAddress[ 4 ] =  { 192, 168,   2,   1 };

///* The following is the address of an OpenDNS server. */
static const uint8_t ucDNSServerAddress[ 4 ] = { 192, 168, 80, 1 };


BaseType_t verboseLevel = 0;

/*
 * Just seeds the simple pseudo random number generator.
 */
static void prvSRand(UBaseType_t ulSeed);

/*
 * Miscellaneous initialisation including preparing the logging and seeding the
 * random number generator.
 */
//static void prvMiscInitialisation(void);

/* Set the following constant to pdTRUE to log using the method indicated by the
 name of the constant, or pdFALSE to not log using the method indicated by the
 name of the constant.  Options include to standard out (xLogToStdout), to a disk
 file (xLogToFile), and to a UDP port (xLogToUDP).  If xLogToUDP is set to pdTRUE
 then UDP messages are sent to the IP address configured as the echo server
 address (see the configECHO_SERVER_ADDR0 definitions in FreeRTOSConfig.h) and
 the port number set by configPRINT_PORT in FreeRTOSConfig.h. */
const BaseType_t xLogToStdout = pdTRUE, xLogToFile = pdFALSE, xLogToUDP =
pdFALSE;

#if( ipconfigUSE_CALLBACKS != 0 )
static BaseType_t xOnUdpReceive( Socket_t xSocket, void * pvData, size_t xLength,
	const struct freertos_sockaddr *pxFrom, const struct freertos_sockaddr *pxDest );
#endif

/* Use by the pseudo random number generator. */
static UBaseType_t ulNextRand;
const uint8_t ucMACAddress[ 6 ] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x56 };

static SemaphoreHandle_t xServerSemaphore;

static BaseType_t run_command_line( TickType_t uxSleepTime );

void initTask(void * dummy)
{
	FreeRTOS_debug_printf( ( "\rFreeRTOS_IPInit\r\n" ) );
	FreeRTOS_IPInit (ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress);

	xServerSemaphore = xSemaphoreCreateBinary();
	configASSERT( xServerSemaphore != NULL );

	for( ;; )
	{
	TickType_t uxTimeout = pdMS_TO_TICKS( 200U );

		run_command_line( uxTimeout );
	}
//	vTaskDelete(NULL);
}

int main(void) {

	/* ***NOTE*** Tasks that use the network are created in the network event hook
	when the network is connected and ready for use (see the definition of
	vApplicationIPNetworkEventHook() below).  The address values passed in here
	are used if ipconfigUSE_DHCP is set to 0, or if ipconfigUSE_DHCP is set to 1
	but a DHCP server cannot be	contacted. */

	xil_printf("---------------------------------\r\n");

	/* Start the RTOS scheduler. */
	FreeRTOS_debug_printf(("\rvTaskStartScheduler\r\n"));
	xil_printf("---------------------------------\r\n");

	xTaskCreate(initTask, "Init", 1024, NULL, 7, NULL);

	/* Start the tasks and timer running. */
	vTaskStartScheduler();

	/* If all is well, the scheduler will now be running, and the following line
	 will never be reached.  If the following line does execute, then there was
	 insufficient FreeRTOS heap memory available for the idle and/or timer tasks
	 to be created.  See the memory management section on the FreeRTOS web site
	 for more details. */
	for (;;){
		}

}

/*-----------------------------------------------------------*/

void vAssertCalled(const char *pcFile, uint32_t ulLine) {
	volatile uint32_t ulBlockVariable = 0UL;
	volatile char *pcFileName = (volatile char *) pcFile;
	volatile uint32_t ulLineNumber = ulLine;

	(void) pcFileName;
	(void) ulLineNumber;

	FreeRTOS_debug_printf(( "vAssertCalled( %s, %ld\n", pcFile, ulLine ));

	/* Setting ulBlockVariable to a non-zero value in the debugger will allow
	 this function to be exited. */
	taskDISABLE_INTERRUPTS();
	{
		while (ulBlockVariable == 0UL) {
			__asm volatile( "NOP" );
		}
	}
	taskENABLE_INTERRUPTS();
}
/*-----------------------------------------------------------*/

/* Called by FreeRTOS+TCP when the network connects or disconnects.  Disconnect
 events are only received if implemented in the MAC driver. */
void vApplicationIPNetworkEventHook(eIPCallbackEvent_t eNetworkEvent) {
	uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
	char cBuffer[16];
	static BaseType_t xTasksAlreadyCreated = pdFALSE;

	/* If the network has just come up...*/
	if (eNetworkEvent == eNetworkUp) {
		/* Create the tasks that use the IP stack if they have not already been
		 created. */
		xil_printf ("Network is UP\r\n");
		        /* Print out the network configuration, which may have come from a DHCP server. */
		        FreeRTOS_GetAddressConfiguration( &ulIPAddress, &ulNetMask, &ulGatewayAddress, &ulDNSServerAddress );

		        FreeRTOS_inet_ntoa( ulIPAddress, cBuffer );             xil_printf( "IP Address: %s\r\n", cBuffer );
		        FreeRTOS_inet_ntoa( ulNetMask, cBuffer );               xil_printf( "Subnet Mask: %s\r\n", cBuffer );
		        FreeRTOS_inet_ntoa( ulGatewayAddress, cBuffer );        xil_printf( "Gateway Address: %s\r\n", cBuffer );
		        FreeRTOS_inet_ntoa( ulDNSServerAddress, cBuffer );      xil_printf( "DNS Server Address: %s\r\n", cBuffer );
		        xil_printf("Static MAC Address: %02x:%02x:%02x:%02x:%02x:%02x\n", ucMACAddress[0],ucMACAddress[1],ucMACAddress[2],ucMACAddress[3],ucMACAddress[4],ucMACAddress[5]);

		if (xTasksAlreadyCreated == pdFALSE) {
			vIPerfInstall();
			vUDPLoggingTaskCreate();
			xTasksAlreadyCreated = pdTRUE;
		}

	}
}
/*-----------------------------------------------------------*/
UBaseType_t uxRand(void) {
	const uint32_t ulMultiplier = 0x015a4e35UL, ulIncrement = 1UL;

	/* Utility function to generate a pseudo random number. */

	ulNextRand = (ulMultiplier * ulNextRand) + ulIncrement;
	return ((int) (ulNextRand >> 16UL) & 0x7fffUL);
}
/*-----------------------------------------------------------*/

static void prvSRand(UBaseType_t ulSeed) {
	/* Utility function to seed the pseudo random number generator. */
	ulNextRand = ulSeed;
}
/*-----------------------------------------------------------*/

//static void prvMiscInitialisation(void)
//{
//	time_t xTimeNow;

//	/* Seed the random number generator. */
//	time(&xTimeNow);
//	FreeRTOS_debug_printf(( "Seed for randomiser: %lu\n", xTimeNow ));
//	prvSRand((uint32_t) xTimeNow);
////	FreeRTOS_debug_printf(
////			( "Random numbers: %08X %08X %08X %08X\n", ipconfigRAND32(), ipconfigRAND32(), ipconfigRAND32(), ipconfigRAND32() ));
//
//}
/*-----------------------------------------------------------*/

#if( ipconfigUSE_LLMNR != 0 ) || ( ipconfigUSE_NBNS != 0 ) || ( ipconfigDHCP_REGISTER_HOSTNAME == 1 )

const char *pcApplicationHostnameHook(void) {
	/* Assign the name "FreeRTOS" to this network node.  This function will
	 be called during the DHCP: the machine will be registered with an IP
	 address plus this name. */
	return mainHOST_NAME;
}

#endif
/*-----------------------------------------------------------*/

#if( ipconfigUSE_LLMNR != 0 ) || ( ipconfigUSE_NBNS != 0 )

BaseType_t xApplicationDNSQueryHook( const char *pcName )
{
	BaseType_t xReturn;

	/* Determine if a name lookup is for this node.  Two names are given
	 to this node: that returned by pcApplicationHostnameHook() and that set
	 by mainDEVICE_NICK_NAME. */
	if( _stricmp( pcName, pcApplicationHostnameHook() ) == 0 )
	{
		xReturn = pdPASS;
	}
	else if( _stricmp( pcName, mainDEVICE_NICK_NAME ) == 0 )
	{
		xReturn = pdPASS;
	}
	else
	{
		xReturn = pdFAIL;
	}

	return xReturn;
}

#endif

/*
 * Callback that provides the inputs necessary to generate a randomized TCP
 * Initial Sequence Number per RFC 6528.  THIS IS ONLY A DUMMY IMPLEMENTATION
 * THAT RETURNS A PSEUDO RANDOM NUMBER SO IS NOT INTENDED FOR USE IN PRODUCTION
 * SYSTEMS.
 */
extern uint32_t ulApplicationGetNextSequenceNumber(uint32_t ulSourceAddress,
		uint16_t usSourcePort, uint32_t ulDestinationAddress,
		uint16_t usDestinationPort) {
	(void) ulSourceAddress;
	(void) usSourcePort;
	(void) ulDestinationAddress;
	(void) usDestinationPort;

	return uxRand();
}

/*
 * Supply a random number to FreeRTOS+TCP stack.
 * THIS IS ONLY A DUMMY IMPLEMENTATION THAT RETURNS A PSEUDO RANDOM NUMBER
 * SO IS NOT INTENDED FOR USE IN PRODUCTION SYSTEMS.
 */
BaseType_t xApplicationGetRandomNumber(uint32_t* pulNumber) {
	*(pulNumber) = uxRand();
	return pdTRUE;
}

/* Called automatically when a reply to an outgoing ping is received. */
void vApplicationPingReplyHook(ePingReplyStatus_t eStatus,
		uint16_t usIdentifier) {
	char *pcSuccess = "Ping reply received - ";
	char *pcInvalidChecksum =
			"Ping reply received with invalid checksum - ";
	 char *pcInvalidData =
			"Ping reply received with invalid data - ";

	switch (eStatus) {
	case eSuccess:
		FreeRTOS_debug_printf(( pcSuccess ));
		break;

	case eInvalidChecksum:
		FreeRTOS_debug_printf(( pcInvalidChecksum ));
		break;

	case eInvalidData:
		FreeRTOS_debug_printf(( pcInvalidData ));
		break;

	default:
		/* It is not possible to get here as all enums have their own
		 case. */
		break;
	}

//	FreeRTOS_debug_printf(
//			( cMessage, "identifier %d\r\n", ( int ) usIdentifier ));

	/* Prevent compiler warnings in case FreeRTOS_debug_printf() is not defined. */
	(void) usIdentifier;
}
/*-----------------------------------------------------------*/

//#warning _gettimeofday_r is just stubbed out here.
struct timezone;
struct timeval;
int _gettimeofday_r(struct _reent * x, struct timeval *y,
		struct timezone * ptimezone) {
	(void) x;
	(void) y;
	(void) ptimezone;

	return 0;
}

/* configSUPPORT_STATIC_ALLOCATION is set to 1, so the application must provide an
implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer,
                                    StackType_t **ppxIdleTaskStackBuffer,
                                    uint32_t *pulIdleTaskStackSize )
{
/* If the buffers to be provided to the Idle task are declared inside this
function then they must be declared static – otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

    /* Pass out a pointer to the StaticTask_t structure in which the Idle task’s
    state will be stored. */
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

    /* Pass out the array that will be used as the Idle task’s stack. */
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;

    /* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configMINIMAL_STACK_SIZE is specified in words, not bytes. */
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*———————————————————–*/

/* configSUPPORT_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer,
                                     StackType_t **ppxTimerTaskStackBuffer,
                                     uint32_t *pulTimerTaskStackSize )
{
/* If the buffers to be provided to the Timer task are declared inside this
function then they must be declared static – otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

    /* Pass out a pointer to the StaticTask_t structure in which the Timer
    task’s state will be stored. */
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

    /* Pass out the array that will be used as the Timer task’s stack. */
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;

    /* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
    Note that, as the array is necessarily of type StackType_t,
    configTIMER_TASK_STACK_DEPTH is specified in words, not bytes. */
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

static SemaphoreHandle_t xPrintSemaphore;

BaseType_t logging_enter( void )
{
	if( xTaskGetSchedulerState( ) == taskSCHEDULER_RUNNING )
	{
		if( xPrintSemaphore == NULL )
		{
			xPrintSemaphore = xSemaphoreCreateBinary();
			if( xPrintSemaphore != NULL )
			{
				xSemaphoreGive( xPrintSemaphore );
			}

		}
		if( xPrintSemaphore != NULL )
		{
			if( xSemaphoreTake( xPrintSemaphore, 200U ) == pdFAIL )
			{
				return pdPASS;
			}
			else
			{
				return pdFAIL;
			}
		}
	}
	return pdPASS;
}

void logging_exit( void )
{
	if( xPrintSemaphore != NULL )
	{
		xSemaphoreGive( xPrintSemaphore );
	}
}

int serialLogging( const char8 *pcFormatString, ... )
{
static char pcBuffer[ 512 ];
int iIndex, iLength;

	logging_enter();

	va_list args;
	va_start (args, ( const char * ) pcFormatString);
	iLength = vsnprintf (pcBuffer, sizeof pcBuffer, ( const char * ) pcFormatString, args );
	va_end (args);

	for( iIndex = 0; iIndex < iLength; iIndex++ )
	{
		XUartPs_SendByte( STDOUT_BASEADDRESS, pcBuffer[ iIndex ] );
	}
	logging_exit();

	lUDPLoggingPrintf( "%s", pcBuffer );

	return 1;
}

#define USE_ZERO_COPY	1

static BaseType_t run_command_line( TickType_t uxSleepTime )
{
char  pcBuffer[ 92 ];
BaseType_t xCount;
struct freertos_sockaddr xSourceAddress;
socklen_t xSourceAddressLength = sizeof( xSourceAddress );
xSocket_t xSocket = xLoggingGetSocket();
static BaseType_t xHadSocket = pdFALSE;
static NetworkBufferDescriptor_t *pxDescriptor = NULL;

	xSemaphoreTake( xServerSemaphore, uxSleepTime );

if( xSocket == NULL )
	{
		return 0;
	}
	if( xHadSocket == pdFALSE )
	{
	TickType_t xReceiveTimeOut = 0U;

		xHadSocket = pdTRUE;
		FreeRTOS_printf( ( "prvCommandTask started\n" ) );
		/* xServerSemaphore will be given to when there is data for xSocket
		and also as soon as there is USB/CDC data. */
		FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_SET_SEMAPHORE, ( void * ) &xServerSemaphore, sizeof( xServerSemaphore ) );
		FreeRTOS_setsockopt( xSocket, 0, FREERTOS_SO_RCVTIMEO, &xReceiveTimeOut, sizeof( xReceiveTimeOut ) );
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
		#if( USE_ECHO_TASK != 0 )
		{
			vStartTCPEchoClientTasks_SingleTasks( configMINIMAL_STACK_SIZE * 4, tskIDLE_PRIORITY + 1 );
		}
		#endif /* ( USE_ECHO_TASK != 0 ) */
	}

//	xCount = FreeRTOS_recvfrom( xSocket, pcBuffer, sizeof( pcBuffer ) - 1, FREERTOS_MSG_DONTWAIT, &xSourceAddress, &xSourceAddressLength );
	#if( USE_ZERO_COPY )
	{
		if( pxDescriptor != NULL )
		{
			vReleaseNetworkBufferAndDescriptor( pxDescriptor );
			pxDescriptor = NULL;
		}
		char  *ppcBuffer;
		xCount = FreeRTOS_recvfrom( xSocket, ( void * )&ppcBuffer, sizeof( pcBuffer ) - 1, FREERTOS_MSG_DONTWAIT | FREERTOS_ZERO_COPY, &xSourceAddress, &xSourceAddressLength );
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
	}
	#else
	{
		xCount = FreeRTOS_recvfrom( xSocket, ( void * ) pcBuffer, sizeof( pcBuffer ) - 1, FREERTOS_MSG_DONTWAIT,
			&( xSourceAddress ), &( xSourceAddressLength ) );
	}
	#endif
	if( xCount <= 0 )
	{
		return 0;
	}
	pcBuffer[ xCount ] = 0;
	if( strncmp( pcBuffer, "ver", 4 ) == 0 )
	{
		lUDPLoggingPrintf( "Verbose level %d\n", verboseLevel );
	}
	else if( strncmp( pcBuffer, "hrtime", 6 ) == 0 )
	{
		static uint64_t lastTime = 0ULL;
		uint64_t curTime = ullGetHighResolutionTime();
		uint64_t difTime = curTime - lastTime;
		lUDPLoggingPrintf( "hr_time %lu\n", ( uint32_t ) difTime );
		lastTime = curTime;
	}
	else if( memcmp( pcBuffer, "mem", 3 ) == 0 )
	{
		uint32_t now = xPortGetFreeHeapSize( );
		uint32_t total = 0;//xPortGetOrigHeapSize( );
		uint32_t perc = total ? ( ( 100 * now ) / total ) : 100;
		lUDPLoggingPrintf("mem Low %u, Current %lu / %lu (%lu perc free)\n",
			xPortGetMinimumEverFreeHeapSize( ),
			now, total, perc );
	}
#if( USE_LOG_EVENT != 0 )
	else if( strncmp( pcBuffer, "event", 4 ) == 0 )
	{
		if(pcBuffer[ 5 ] == 'c')
		{
			int rc = iEventLogClear();
			lUDPLoggingPrintf( "cleared %d events\n", rc );
		}
		else
		{
			eventLogDump();
		}
	}
#endif /* USE_LOG_EVENT */
	else
	{
		FreeRTOS_printf( ( "'%s' is not recognized as a valid command\n", pcBuffer ) );
	}
	return xCount;
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
			FreeRTOS_printf( ( "xOnUdpReceive_6: %d bytes\n",  ( int ) xLength ) );
			FreeRTOS_printf( ( "xOnUdpReceive_6: from %pip\n", pxFrom6->sin_addrv6.ucBytes ) );
			FreeRTOS_printf( ( "xOnUdpReceive_6: to   %pip\n", pxDest6->sin_addrv6.ucBytes ) );
		}
		else
		#endif
		{
			FreeRTOS_printf( ( "xOnUdpReceive_4: %d bytes\n", ( int ) xLength ) );
			FreeRTOS_printf( ( "xOnUdpReceive_4: from %lxip\n", FreeRTOS_ntohl( pxFrom->sin_addr ) ) );
			FreeRTOS_printf( ( "xOnUdpReceive_4: to   %lxip\n", FreeRTOS_ntohl( pxDest->sin_addr ) ) );
			
		}
		/* Returning 0 means: not yet consumed. */
		return 0;
	}
#endif	/* ( ipconfigUSE_CALLBACKS != 0 ) */

#if( ipconfigTCP_IP_SANITY != 0 )
	#warning Please undefine ipconfigTCP_IP_SANITY later on
#endif

#if defined( __OPTIMIZE )
	#warning Code is optimised
#else
	#warning Code is NOT optimised
#endif

	int logPrintf (const char *apFmt, ...) __attribute__ ((format (__printf__, 1, 2)));		/* Delayed write to serial device, non-blocking */
	void printf_test()
	{
	    uint32_t ulValue = 0u;

	    logPrintf("%u", ulValue); // <== good
	    logPrintf("%lu", ulValue);

	    uint16_t usValue = 0u;
	    logPrintf("%u", usValue); // <== good
	    logPrintf("%lu", usValue);

	    uint8_t ucValue = 0u;
	    logPrintf("%u", ucValue); // <== good
	    logPrintf("%lu", ucValue);

	    uint64_t ullValue = 0u;
	    logPrintf("%u", ullValue);
	    logPrintf("%lu", ullValue); // <== good

	    unsigned uValue = 0u;
	    logPrintf("%u", uValue); // <== good
	    logPrintf("%lu", uValue);
	}
