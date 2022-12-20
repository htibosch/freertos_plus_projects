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

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/*-----------------------------------------------------------
 * Application specific definitions.
 *
 * These definitions should be adjusted for your particular hardware and
 * application requirements.
 *
 * THESE PARAMETERS ARE DESCRIBED WITHIN THE 'CONFIGURATION' SECTION OF THE
 * FreeRTOS API DOCUMENTATION AVAILABLE ON THE FreeRTOS.org WEB SITE.
 *
 * See http://www.freertos.org/a00110.html.
 *----------------------------------------------------------*/

/* USER CODE BEGIN Includes */
/* Section where include file can be added */
/* USER CODE END Includes */

/* Ensure stdint is only used by the compiler, and not the assembler. */
#if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
    #include <stdint.h>
    extern uint32_t SystemCoreClock;
#endif

#define configSUPPORT_STATIC_ALLOCATION         1

#define configUSE_PREEMPTION					1
#define configUSE_IDLE_HOOK						0
#define configUSE_TICK_HOOK						0
#define configCPU_CLOCK_HZ						( SystemCoreClock )
#define configUSE_PORT_OPTIMISED_TASK_SELECTION	1
#define configTICK_RATE_HZ						( 1000U )
#define configMAX_PRIORITIES					( 7 )
#define configMINIMAL_STACK_SIZE				( ( uint16_t ) 128 )
#define configMAX_TASK_NAME_LEN					( 16 )
#define configUSE_TRACE_FACILITY				1
#define configUSE_STATS_FORMATTING_FUNCTIONS	0
#define configUSE_16_BIT_TICKS					0
#define configIDLE_SHOULD_YIELD					1
#define configUSE_MUTEXES						1
#define configQUEUE_REGISTRY_SIZE				8
#define configCHECK_FOR_STACK_OVERFLOW			0
#define configUSE_RECURSIVE_MUTEXES				1
#define configUSE_MALLOC_FAILED_HOOK			1
#define configUSE_COUNTING_SEMAPHORES			1
#define configGENERATE_RUN_TIME_STATS			0

#define configUSE_TASK_NOTIFICATIONS			1
#define configNUM_THREAD_LOCAL_STORAGE_POINTERS	3 /* FreeRTOS+FAT requires 3 pointers if a CWD is supported. */

/* Co-routine definitions. */
#define configUSE_CO_ROUTINES					0
#define configMAX_CO_ROUTINE_PRIORITIES			( 2 )

/* Software timer definitions. */
#define configUSE_TIMERS						0
#define configTIMER_TASK_PRIORITY				( configMAX_PRIORITIES - 1 )
#define configTIMER_QUEUE_LENGTH				5
#define configTIMER_TASK_STACK_DEPTH			( configMINIMAL_STACK_SIZE * 2 )

/* Set the following definitions to 1 to include the API function, or zero
to exclude the API function. */
#define INCLUDE_vTaskPrioritySet				1
#define INCLUDE_uxTaskPriorityGet				1
#define INCLUDE_vTaskDelete						1
#define INCLUDE_vTaskCleanUpResources			0
#define INCLUDE_vTaskSuspend					1
#define INCLUDE_vTaskDelayUntil					0
#define INCLUDE_vTaskDelay						1
#define INCLUDE_xTimerPendFunctionCall			0
#define INCLUDE_eTaskGetState					1
#define INCLUDE_pcTaskGetTaskName				1
#define INCLUDE_xTaskGetSchedulerState			1

extern int lUDPLoggingPrintf( const char *pcFormatString, ... ) __attribute__ ((format (__printf__, 1, 2)));

/*
#define configPRINTF( X )	( void ) lUDPLoggingPrintf X
*/

/*
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()

#if( configGENERATE_RUN_TIME_STATS != 0 )
	extern uint32_t ulGetRunTimeCounterValue(void);
	#define portGET_RUN_TIME_COUNTER_VALUE()			ulGetRunTimeCounterValue()
#endif
*/

/* Cortex-M specific definitions. */
#ifdef __NVIC_PRIO_BITS
 /* __BVIC_PRIO_BITS will be specified when CMSIS is being used. */
 #define configPRIO_BITS		__NVIC_PRIO_BITS
#else
 #define configPRIO_BITS		4	/* 15 priority levels */
#endif

/* The lowest interrupt priority that can be used in a call to a "set priority"
function. */
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY		15

/* The highest interrupt priority that can be used by any interrupt service
routine that makes calls to interrupt safe FreeRTOS API functions.  DO NOT CALL
INTERRUPT SAFE FREERTOS API FUNCTIONS FROM ANY INTERRUPT THAT HAS A HIGHER
PRIORITY THAN THIS! (higher priorities are lower numeric values. */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

/* Interrupt priorities used by the kernel port layer itself.  These are generic
to all Cortex-M ports, and do not rely on any particular library functions. */
#define configKERNEL_INTERRUPT_PRIORITY  \
		( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )
/* !!!! configMAX_SYSCALL_INTERRUPT_PRIORITY must not be set to zero !!!!
See http://www.FreeRTOS.org/RTOS-Cortex-M3-M4.html. */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY  \
		( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS) )

#define  USE_FULL_ASSERT	    0 // 1
#define  configASSERT_DEFINED	1

	#if( USE_FULL_ASSERT != 0 )
		/* Normal assert() semantics without relying on the provision of an assert.h
		header file. */
		void vAssertCalled( const char *pcFile, uint32_t ulLine );
		#define configASSERT( x ) if( ( x ) == 0 ) vAssertCalled( __FILE__, __LINE__ );
	#else
		#define configASSERT( x ) do {} while( 0 )
	#endif

/* USER CODE BEGIN 1 */


/* USER CODE END 1 */

/* Definitions that map the FreeRTOS port interrupt handlers to their CMSIS
standard names. */
#define vPortSVCHandler		SVC_Handler
#define xPortPendSVHandler	PendSV_Handler
/* IMPORTANT: This define MUST be commented when used with STM32Cube firmware,
              to prevent overwriting SysTick_Handler defined within STM32Cube HAL */
/* #define xPortSysTickHandler SysTick_Handler */


/* Default MAC address configuration.  The demo creates a virtual network
connection that uses this MAC address by accessing the raw Ethernet/WiFi data
to and from a real network connection on the host PC.  See the
configNETWORK_INTERFACE_TO_USE definition above for information on how to
configure the real network connection to use. */


#define configMAC_ADDR0		0x00
#define configMAC_ADDR1		0x11
#define configMAC_ADDR2		0x22
#define configMAC_ADDR3		0x33
#define configMAC_ADDR4		0x44
#define configMAC_ADDR5		0x62

/* Default IP address configuration.  Used in case ipconfigUSE_DHCP is set to 0, or
ipconfigUSE_DHCP is set to 1 but a DHCP server cannot be contacted. */
#define configIP_ADDR0		192
#define configIP_ADDR1		168
#define configIP_ADDR2		2
#define configIP_ADDR3		90

/* Default gateway IP address configuration.  Used in case ipconfigUSE_DHCP is
set to 0, or ipconfigUSE_DHCP is set to 1 but a DHCP server cannot be contacted. */
#define configGATEWAY_ADDR0	192
#define configGATEWAY_ADDR1	168
#define configGATEWAY_ADDR2	2
#define configGATEWAY_ADDR3	1

/* Default DNS server configuration.  OpenDNS addresses are 208.67.222.222 and
208.67.220.220.  Used in ipconfigUSE_DHCP is set to 0, or ipconfigUSE_DHCP is set
to 1 but a DHCP server cannot be contacted.*/
#define configDNS_SERVER_ADDR0    10
#define configDNS_SERVER_ADDR1    128
#define configDNS_SERVER_ADDR2    128
#define configDNS_SERVER_ADDR3    4
//#define configDNS_SERVER_ADDR0    118
//#define configDNS_SERVER_ADDR1    98
//#define configDNS_SERVER_ADDR2    44
//#define configDNS_SERVER_ADDR3    100

/* Default netmask configuration.  Used in case ipconfigUSE_DHCP is set to 0,
or ipconfigUSE_DHCP is set to 1 but a DHCP server cannot be contacted. */
#define configNET_MASK0		255
#define configNET_MASK1		255
#define configNET_MASK2		255
#define configNET_MASK3		0

/* The address of an echo server that will be used by the two demo echo client
tasks.
http://www.freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/examples_FreeRTOS_simulator.html */
#define configECHO_SERVER_ADDR0	192
#define configECHO_SERVER_ADDR1 168
#define configECHO_SERVER_ADDR2 2
#define configECHO_SERVER_ADDR3 3


#define configHTTP_ROOT "/web_source"


/* USER CODE BEGIN Defines */
/* Section where parameter definitions can be added (for instance, to override default ones in FreeRTOS.h) */
/* USER CODE END Defines */

/* The STM3240G-EVAL board uses pin H13 for card detection. */
#define configSD_DETECT_PIN				GPIO_PIN_13
#define configSD_DETECT_GPIO_PORT		GPIOH
//#define sdDETECT_GPIO_CLK		RCC_AHB1Periph_GPIOH

extern uint32_t ulSeconds, ulMsec;

#define traceINCREASE_TICK_COUNT( xTicksToJump ) \
	{ \
		ulMsec += xTicksToJump; \
		if( ulMsec >= 1000 ) \
		{ \
			ulSeconds += ( ulMsec / 1000ul ); \
			ulMsec = ( ulMsec % 1000ul ); \
		} \
	}


#define traceTASK_INCREMENT_TICK( xTickCount ) \
	if( uxSchedulerSuspended == ( UBaseType_t ) pdFALSE ) \
	{ \
		if( ++ulMsec >= 1000 ) \
		{ \
			ulMsec = 0; \
			ulSeconds++; \
		} \
	}

#endif /* FREERTOS_CONFIG_H */
