/*
 * FreeRTOS Kernel V10.3.0
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
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

/******************************************************************************
 *
 * See http://www.FreeRTOS.org/RTOS-Xilinx-UltraScale_MPSoC_64-bit.html for
 * additional information on this demo.
 *
 * NOTE 1:  This project provides two demo applications.  A simple blinky
 * style project, and a more comprehensive test and demo application.  The
 * mainSELECTED_APPLICATION setting in main.c is used to select between the two.
 * See the notes on using mainSELECTED_APPLICATION where it is defined below.
 *
 * NOTE 2:  This file only contains the source code that is not specific to
 * either the simply blinky or full demos - this includes initialisation code
 * and callback functions.
 *
 * NOTE 3:  This project builds the FreeRTOS source code, so is expecting the
 * BSP project to be configured as a 'standalone' bsp project rather than a
 * 'FreeRTOS' bsp project.  However the BSP project MUST still be build with
 * the FREERTOS_BSP symbol defined (-DFREERTOS_BSP must be added to the
 * command line in the BSP configuration).
 */

/* Standard includes. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* Scheduler include files. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* Xilinx includes. */
#include "platform.h"
#include "xttcps.h"
#include "xscugic.h"

#include "xuartps_hw.h"

#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_DNS.h"
#include "FreeRTOS_DHCP.h"

//void xil_printf( const char8 pucFmt, ... );

/* mainSELECTED_APPLICATION is used to select between two demo applications,
 * as described at the top of this file.
 *
 * When mainSELECTED_APPLICATION is set to 0 the simple blinky example will
 * be run.
 *
 * When mainSELECTED_APPLICATION is set to 1 the comprehensive test and demo
 * application will be run.
 */
#define mainSELECTED_APPLICATION	1

/*-----------------------------------------------------------*/

/*
 * Configure the hardware as necessary to run this demo.
 */
static void prvSetupHardware( void );

/*
 * See the comments at the top of this file and above the
 * mainSELECTED_APPLICATION definition.
 */
#if ( mainSELECTED_APPLICATION == 0 )
	extern void main_blinky( void );
#elif ( mainSELECTED_APPLICATION == 1 )
	extern void main_full( void );
#else
	#error Invalid mainSELECTED_APPLICATION setting.  See the comments at the top of this file and above the mainSELECTED_APPLICATION definition.
#endif

/* Prototypes for the standard FreeRTOS callback/hook functions implemented
within this file. */
void vApplicationMallocFailedHook( void );
void vApplicationIdleHook( void );
void vApplicationStackOverflowHook( TaskHandle_t pxTask, char *pcTaskName );
void vApplicationTickHook( void );

#define mainSERVER_TASK_PRIORITY   2
void prvServerTask( void *pvParameter );

const uint8_t ucMACAddress[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };

/*-----------------------------------------------------------*/

/* The interrupt controller is initialised in this file, and made available to
other modules. */
XScuGic xInterruptController;

/*-----------------------------------------------------------*/

int main( void )
{
	/* See http://www.FreeRTOS.org/RTOS-Xilinx-UltraScale_MPSoC_64-bit.html for
	additional information on this demo. */

	/* Configure the hardware ready to run the demo. */
	prvSetupHardware();

	/* The mainSELECTED_APPLICATION setting is described at the top
	of this file. */
	/* Create the task that performs the 'check' functionality,	as described at
	the top of this file. */
	xTaskCreate( prvServerTask, "Server", 4 * configMINIMAL_STACK_SIZE, NULL, mainSERVER_TASK_PRIORITY, NULL );

	const uint8_t ucIPAddress[ 4 ]        = { 192, 168,   2, 114 };
	const uint8_t ucNetMask[ 4 ]          = { 255, 255, 255, 255 };
	const uint8_t ucGatewayAddress[ 4 ]   = { 192, 168,   2,   1 };
	const uint8_t ucDNSServerAddress[ 4 ] = { 118,  98,  44, 100 };

	//xNetworkInterfaceInitialise();

	xil_printf( "Calling FreeRTOS_IPInit\n" );

	FreeRTOS_IPInit( ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress );

	xil_printf( "Starting scheduler\n" );

			/* Start the scheduler. */
	vTaskStartScheduler();

/*
*
	#if( mainSELECTED_APPLICATION == 0 )
	{
		main_blinky();
	}
	#elif( mainSELECTED_APPLICATION == 1 )
	{
		main_full();
	}
	#endif
*/
	/* Don't expect to reach here. */
	return 0;
}
/*-----------------------------------------------------------*/
void uart_init(u32 BaseAddress);

static void prvSetupHardware( void )
{
BaseType_t xStatus;
XScuGic_Config *pxGICConfig;

	/* Ensure no interrupts execute while the scheduler is in an inconsistent
	state.  Interrupts are automatically enabled when the scheduler is
	started. */
	portDISABLE_INTERRUPTS();

	/* Obtain the configuration of the GIC. */
	pxGICConfig = XScuGic_LookupConfig( XPAR_SCUGIC_SINGLE_DEVICE_ID );

	/* Sanity check the FreeRTOSConfig.h settings are correct for the
	hardware. */
	configASSERT( pxGICConfig );
	configASSERT( pxGICConfig->CpuBaseAddress == ( configINTERRUPT_CONTROLLER_BASE_ADDRESS + configINTERRUPT_CONTROLLER_CPU_INTERFACE_OFFSET ) );
	configASSERT( pxGICConfig->DistBaseAddress == configINTERRUPT_CONTROLLER_BASE_ADDRESS );

	/* Install a default handler for each GIC interrupt. */
	xStatus = XScuGic_CfgInitialize( &xInterruptController, pxGICConfig, pxGICConfig->CpuBaseAddress );
	configASSERT( xStatus == XST_SUCCESS );
	( void ) xStatus; /* Remove compiler warning if configASSERT() is not defined. */
	#ifdef STDOUT_BASEADDRESS
	{
	//XUartPs_ResetHw( STDOUT_BASEADDRESS );
	////XUartPs_ResetHw( STDOUT_BASEADDRESS );
		xil_printf( " Starting the program\n" );
	}
	#endif
}
/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook( void )
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */
	taskDISABLE_INTERRUPTS();
	for( ;; );
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

void vApplicationIdleHook( void )
{
volatile size_t xFreeHeapSpace;

	/* This is just a trivial example of an idle hook.  It is called on each
	cycle of the idle task.  It must *NOT* attempt to block.  In this case the
	idle task just queries the amount of FreeRTOS heap that remains.  See the
	memory management section on the http://www.FreeRTOS.org web site for memory
	management options.  If there is a lot of heap memory free then the
	configTOTAL_HEAP_SIZE value in FreeRTOSConfig.h can be reduced to free up
	RAM. */
	xFreeHeapSpace = xPortGetFreeHeapSize();

	/* Remove compiler warning about xFreeHeapSpace being set but never used. */
	( void ) xFreeHeapSpace;
}
/*-----------------------------------------------------------*/

void vApplicationTickHook( void )
{
	#if( mainSELECTED_APPLICATION == 1 )
	{
		/* Only the comprehensive demo actually uses the tick hook. */
//		extern void vFullDemoTickHook( void );
//		vFullDemoTickHook();
	}
	#endif
}
/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION is set to 1, so the application must provide an
implementation of vApplicationGetIdleTaskMemory() to provide the memory that is
used by the Idle task. */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
/* If the buffers to be provided to the Idle task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xIdleTaskTCB;
static StackType_t uxIdleTaskStack[ configMINIMAL_STACK_SIZE ];

	/* Pass out a pointer to the StaticTask_t structure in which the Idle task's
	state will be stored. */
	*ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

	/* Pass out the array that will be used as the Idle task's stack. */
	*ppxIdleTaskStackBuffer = uxIdleTaskStack;

	/* Pass out the size of the array pointed to by *ppxIdleTaskStackBuffer.
	Note that, as the array is necessarily of type StackType_t,
	configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
/*-----------------------------------------------------------*/

/* configUSE_STATIC_ALLOCATION and configUSE_TIMERS are both set to 1, so the
application must provide an implementation of vApplicationGetTimerTaskMemory()
to provide the memory that is used by the Timer service task. */
void vApplicationGetTimerTaskMemory( StaticTask_t **ppxTimerTaskTCBBuffer, StackType_t **ppxTimerTaskStackBuffer, uint32_t *pulTimerTaskStackSize )
{
/* If the buffers to be provided to the Timer task are declared inside this
function then they must be declared static - otherwise they will be allocated on
the stack and so not exists after this function exits. */
static StaticTask_t xTimerTaskTCB;
static StackType_t uxTimerTaskStack[ configTIMER_TASK_STACK_DEPTH ];

	/* Pass out a pointer to the StaticTask_t structure in which the Timer
	task's state will be stored. */
	*ppxTimerTaskTCBBuffer = &xTimerTaskTCB;

	/* Pass out the array that will be used as the Timer task's stack. */
	*ppxTimerTaskStackBuffer = uxTimerTaskStack;

	/* Pass out the size of the array pointed to by *ppxTimerTaskStackBuffer.
	Note that, as the array is necessarily of type StackType_t,
	configMINIMAL_STACK_SIZE is specified in words, not bytes. */
	*pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
/*-----------------------------------------------------------*/

volatile char name[ 256 ];
volatile uint32_t line;
void vMainAssertCalled( const char *pcFileName, uint32_t ulLineNumber )
{
	strcpy( ( char * ) name, pcFileName );
	line = ulLineNumber;

	xil_printf( "ASSERT!  Line %lu of file %s\r\n", ulLineNumber, pcFileName );
	taskENTER_CRITICAL();
	for( ;; );
}

void *____memset(void *str, int c, size_t n)
{
size_t x;
uint8_t *puc = ( uint8_t * ) str;

	for( x = 0; x < c; x++ )
	{
		puc[ x ] = ( uint8_t ) c;
	}

	return str;
}

void *memset(void *str, int c, size_t n)
{
size_t x;
uint8_t *puc = ( uint8_t * ) str;

	for( x = 0; x < c; x++ )
	{
		puc[ x ] = ( uint8_t ) c;
	}

	return str;
}

void prvServerTask( void *pvParameter )
{
	for( ;; )
	{
		xil_printf( "Still working\n" );
		vTaskDelay( 2000 );
	}
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

int lUDPLoggingPrintf( const char8 *pcFormatString, ... )
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

	return 1;
}

#if( ipconfigUSE_DHCP_HOOK != 0 )
eDHCPCallbackAnswer_t xApplicationDHCPHook( eDHCPCallbackPhase_t eDHCPPhase, uint32_t ulIPAddress )
{
eDHCPCallbackAnswer_t eAnswer = eDHCPContinue;

	const char *name = "Unknown";
	switch( eDHCPPhase )
	{
	case eDHCPPhasePreDiscover:
		{
			name = "Discover";			/* Driver is about to send a DHCP discovery. */
//#warning Testing here
//			eAnswer = eDHCPUseDefaults;
		}
		break;
	case eDHCPPhasePreRequest:
		{
			name = "Request";			/* Driver is about to request DHCP an IP address. */
//#warning Testing here
//			eAnswer = eDHCPUseDefaults;
		}
		break;
	}
	FreeRTOS_printf( ( "DHCP %s address %lxip\n", name, FreeRTOS_ntohl( ulIPAddress ) ) );
	return eAnswer;
}
#endif	/* ipconfigUSE_DHCP_HOOK */

void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent )
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
			//vUDPLoggingTaskCreate();

			//bosman_open_listen_socket();

			/* Let the server work task 'prvServerWorkTask' now it can now create the servers. */
			//xTaskNotifyGive( xServerWorkTaskHandle );

			xTasksAlreadyCreated = pdTRUE;
		}

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
		//xTaskCreate( vUDPTest, "vUDPTest", mainUDP_SERVER_STACK_SIZE, NULL, mainUDP_SERVER_TASK_PRIORITY, NULL );
	}
}
/*-----------------------------------------------------------*/

BaseType_t xApplicationDNSQueryHook( const char *pcName )
{
	BaseType_t xReturn = pdFAIL;

	/* Determine if a name lookup is for this node.  Two names are given
	to this node: that returned by pcApplicationHostnameHook() and that set
	by mainDEVICE_NICK_NAME. */
	if( strcasecmp( pcName, pcApplicationHostnameHook() ) == 0 )
	{
		xReturn = pdPASS;
	}
	return xReturn;
}

BaseType_t xApplicationGetRandomNumber( uint32_t *pulValue )
{
uint32_t ulValue;

	ulValue =
			( ( ( uint32_t ) rand() ) << 30 ) |
			( ( ( uint32_t ) rand() ) << 15 ) |
			( ( uint32_t ) rand() );
	*pulValue = ulValue;

	return pdPASS;
}

#if ( ipconfigSUPPORT_OUTGOING_PINGS == 1 )
	void vApplicationPingReplyHook( ePingReplyStatus_t eStatus, uint16_t usIdentifier )
	{
		FreeRTOS_printf( ( "Ping replied\n" ) );
	}
#endif


#if( ipconfigDHCP_REGISTER_HOSTNAME == 1 )

	/* DHCP has an option for clients to register their hostname.  It doesn't
	have much use, except that a device can be found in a router along with its
	name. If this option is used the callback below must be provided by the
	application	writer to return a const string, denoting the device's name. */
	/* Typically this function is defined in a user module. */
	const char *pcApplicationHostnameHook( void )
	{
		return "ultrascale";
	}

#endif /* ipconfigDHCP_REGISTER_HOSTNAME */

uint32_t ulApplicationGetNextSequenceNumber(
    uint32_t ulSourceAddress,
    uint16_t usSourcePort,
    uint32_t ulDestinationAddress,
    uint16_t usDestinationPort )
{
	uint32_t ulReturn;
	( void ) ulSourceAddress;
	( void ) usSourcePort;
	( void ) ulDestinationAddress;
	( void ) usDestinationPort;
	xApplicationGetRandomNumber( &ulReturn );

	return ulReturn;
}
