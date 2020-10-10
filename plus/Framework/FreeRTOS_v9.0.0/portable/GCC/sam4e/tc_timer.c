/*
    FreeRTOS V8.0.0:rc2 - Copyright (C) 2014 Real Time Engineers Ltd.
    All rights reserved

    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    ***************************************************************************
     *                                                                       *
     *    FreeRTOS provides completely free yet professionally developed,    *
     *    robust, strictly quality controlled, supported, and cross          *
     *    platform software that has become a de facto standard.             *
     *                                                                       *
     *    Help yourself get started quickly and support the FreeRTOS         *
     *    project by purchasing a FreeRTOS tutorial book, reference          *
     *    manual, or both from: http://www.FreeRTOS.org/Documentation        *
     *                                                                       *
     *    Thank you!                                                         *
     *                                                                       *
    ***************************************************************************

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>!AND MODIFIED BY!<< the FreeRTOS exception.

    >>! NOTE: The modification to the GPL is included to allow you to distribute
    >>! a combined work that includes FreeRTOS without being obliged to provide
    >>! the source code for proprietary components outside of the FreeRTOS
    >>! kernel.

    FreeRTOS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE.  Full license text is available from the following
    link: http://www.freertos.org/a00114.html

    1 tab == 4 spaces!

    ***************************************************************************
     *                                                                       *
     *    Having a problem?  Start by reading the FAQ "My application does   *
     *    not run, what could be wrong?"                                     *
     *                                                                       *
     *    http://www.FreeRTOS.org/FAQHelp.html                               *
     *                                                                       *
    ***************************************************************************

    http://www.FreeRTOS.org - Documentation, books, training, latest versions,
    license and Real Time Engineers Ltd. contact details.

    http://www.FreeRTOS.org/plus - A selection of FreeRTOS ecosystem products,
    including FreeRTOS+Trace - an indispensable productivity tool, a DOS
    compatible FAT file system, and our tiny thread aware UDP/IP stack.

    http://www.OpenRTOS.com - Real Time Engineers ltd license FreeRTOS to High
    Integrity Systems to sell under the OpenRTOS brand.  Low cost OpenRTOS
    licenses offer ticketed support, indemnification and middleware.

    http://www.SafeRTOS.com - High Integrity Systems also provide a safety
    engineered and independently SIL3 certified version for use in safety and
    mission critical applications that require provable dependability.

    1 tab == 4 spaces!
*/

/*-----------------------------------------------------------
 * Implementation of functions defined in portable.h for the ARM CM3 port.
 *----------------------------------------------------------*/

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "sysclk.h"
#include "pmc.h"
#include "UDPLoggingPrintf.h"

#if( configUSE_TICKLESS_IDLE == 0 )

#	warning No need to include this module

#else

#if( portSLEEP_ALT_INTERRUPT == 0 )
#	warning No need to include this module
#endif

#include <tc.h>

#define portNVIC_SYSTICK_CTRL_REG			( * ( ( volatile uint32_t * ) 0xe000e010 ) )
#define portNVIC_SYSTICK_LOAD_REG			( * ( ( volatile uint32_t * ) 0xe000e014 ) )
#define portNVIC_SYSTICK_CURRENT_VALUE_REG	( * ( ( volatile uint32_t * ) 0xe000e018 ) )

#define portNVIC_SYSTICK_INT_BIT			( 1UL << 1UL )
#define portNVIC_SYSTICK_ENABLE_BIT			( 1UL << 0UL )
#define portNVIC_SYSTICK_COUNT_FLAG_BIT		( 1UL << 16UL )

#ifndef configSYSTICK_CLOCK_HZ
	#define configSYSTICK_CLOCK_HZ configCPU_CLOCK_HZ
	/* Ensure the SysTick is clocked at the same frequency as the core. */
	#define portNVIC_SYSTICK_CLK_BIT	( 1UL << 2UL )
#else
	/* The way the SysTick is clocked is not modified in case it is not the same
	as the core. */
	#define portNVIC_SYSTICK_CLK_BIT	( 0 )
#endif

#ifndef ARRAY_SIZE
#	define	ARRAY_SIZE( x )	( int )( sizeof( x ) / sizeof( x )[ 0 ] )
#endif

#define TC_WPMR_WPKEY_VALUE TC_WPMR_WPKEY((uint32_t)0x54494D)

/* Select a TC timer: TC0..TC2 */
#define SYSTICK_TC		TC1
/* Select a TC channel: 0..2 */
#define SYSTICK_CH		0

static uint32_t ulTimerCountsForOneTick = 0;
static uint32_t xMaximumPossibleSuppressedTicks = 0;
uint32_t ulStoppedTimerCompensation = 0;

extern void systick_set_led( BaseType_t xValue );

static void vPortSleepClockStart( uint32_t ulTicks );
static uint32_t vPortSleepRemaining( void );
static uint32_t vPortSleepInit( void );

/* Atmel's notation is a bit misleading here:
 * TC3_Handler is the interrupt handler for TC1, channel 0
 */
void TC3_Handler ()
{
	/* The timer has expired, disable it as we don't need it anymore */
	if (SYSTICK_TC->TC_CHANNEL[ SYSTICK_CH ].TC_SR & TC_SR_CPCS) {
		SYSTICK_TC->TC_CHANNEL[ SYSTICK_CH ].TC_CCR = TC_CCR_CLKDIS;
		SYSTICK_TC->TC_CHANNEL[ SYSTICK_CH ].TC_IDR = TC_IER_CPCS;
	}
}

/*
* vPortSleepClockStart(uint32_t ulTicks)
* Starts a timer for a given number of ulTicks
* ulTicks is expressed in number of clock cycles (configSYSTICK_CLOCK_HZ)
*/
static void vPortSleepClockStart( uint32_t ulTicks )
{
TcChannel *tc_channel = &( SYSTICK_TC->TC_CHANNEL[ SYSTICK_CH ] );
	/* Disable TC clock. */
	tc_channel->TC_CCR = TC_CCR_CLKDIS;

	/* Set the Reload Counter */
	tc_channel->TC_RC  = ulTicks;
	/* Enable it and issue a software start */
	tc_channel->TC_CCR = TC_CCR_CLKEN | TC_CCR_SWTRG;
	/* Enable the CP interrupt */
	tc_channel->TC_IER = TC_IER_CPCS;
}
/*
* vPortSleepRemaining( )
* This function should return the number of clock ticks left
* Or zero if the alternative clock had expired
*/
static uint32_t vPortSleepRemaining( )
{
	/* Read the Clock Value: it starts at RC and runs down to zero */
	uint32_t ulTicks = SYSTICK_TC->TC_CHANNEL[ SYSTICK_CH ].TC_CV;
	if( ( SYSTICK_TC->TC_CHANNEL[ SYSTICK_CH ].TC_SR & TC_SR_CLKSTA ) == 0 )
	{
		/* The timer has finished and the interrupt has occured,
		 * disabling the timer */
		ulTicks = 0ul;
	}
	return ulTicks;
}

#if( portSLEEP_STATS != 0 )
	#include "hr_gettime.h"
	static uint64_t last_time;
	unsigned switch_count;
	unsigned switch_complete;
	uint64_t sleep_time;
	uint64_t longest_sleep;
	uint64_t wake_time;
#endif

#if( portSLEEP_STATS != 0 )
	typedef struct xSleepEvent
	{
		uint32_t ulActualSleepUs;
		TickType_t xExpectedIdleTime;
		TickType_t xTickCount;
		uint32_t ulReloadValue;
		uint32_t ulCompletedSysTickDecrements;
		uint32_t ulCompleteTickPeriods;
	} SleepEvent_t;

	static int iSleepIndex;
	static SleepEvent_t xSleepEvents[ 8 ];

	extern volatile TickType_t xTickCount;
#endif

static uint8_t bHasInit = 0;

static uint32_t vPortSleepInit( )
{
TcChannel *tc_channel = &( SYSTICK_TC->TC_CHANNEL[ SYSTICK_CH ] );

	ulTimerCountsForOneTick = ( configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ );
	xMaximumPossibleSuppressedTicks = ~0UL / ulTimerCountsForOneTick;
	ulStoppedTimerCompensation = 45;

	pmc_enable_periph_clk(ID_TC1);

	bHasInit = 1;
	/* Disable TC interrupts for this channel */
	tc_channel->TC_IDR = ~0U;
	/* Clear status register */
	tc_channel->TC_SR;
	/* Set mode */
	tc_channel->TC_CMR = TC_CMR_TCCLKS_TIMER_CLOCK1 | TC_CMR_WAVSEL_UP_RC | TC_CMR_WAVE;
	/* Use the undivided master clock */
	tc_channel->TC_EMR = TC_EMR_NODIVCLK;
	NVIC_EnableIRQ( ( IRQn_Type ) TC3_IRQn );
}

#if( portSLEEP_STATS != 0 )
	void show_timer_stats()
	{
	int index;

		lUDPLoggingPrintf( "Now    Expe    Reload     Compl  Compl       Real\n" );
		for( index = 0; index < ARRAY_SIZE( xSleepEvents ); index++ )
		{
			lUDPLoggingPrintf( "%6lu %4lu %9lu %9lu %6lu  %9u\n",
				xSleepEvents[ index ].xTickCount,
				xSleepEvents[ index ].xExpectedIdleTime,
				xSleepEvents[ index ].ulReloadValue,
				xSleepEvents[ index ].ulCompletedSysTickDecrements,
				xSleepEvents[ index ].ulCompleteTickPeriods,
				xSleepEvents[ index ].ulActualSleepUs );
		}
	}
#endif

void vPortSuppressTicksAndSleep( TickType_t xExpectedIdleTime )
{
uint32_t ulReloadValue, ulCompletedSysTickDecrements, ulCompleteTickPeriods;
#if( portSLEEP_STATS != 0 )
	uint64_t cur_time;
#endif

	if( bHasInit == pdFALSE )
	{
		bHasInit = pdTRUE;
		vPortSleepInit( );
	}
	/* Make sure the SysTick reload value does not overflow the counter. */
	if( xExpectedIdleTime > xMaximumPossibleSuppressedTicks )
		xExpectedIdleTime = xMaximumPossibleSuppressedTicks;

	portNVIC_SYSTICK_CTRL_REG &= ~portNVIC_SYSTICK_ENABLE_BIT;
	
	ulReloadValue = portNVIC_SYSTICK_CURRENT_VALUE_REG + ( ulTimerCountsForOneTick * ( xExpectedIdleTime - 1UL ) );
	if( ulReloadValue > ulStoppedTimerCompensation )
	{
		ulReloadValue -= ulStoppedTimerCompensation;
	}

	systick_set_led( pdTRUE );

	/* Enter a critical section but don't use the taskENTER_CRITICAL()
	method as that will mask interrupts that should exit sleep mode. */
	__asm volatile( "cpsid i" );

#if( portSLEEP_STATS != 0 )
	{
		/* Measure the wakeup time */
		cur_time = ullGetHighResolutionTime();
		if (last_time) {
			uint64_t diff = cur_time - last_time;
			wake_time += diff;
		}
		last_time = cur_time;	
	}
#endif

	vPortSleepClockStart( ulReloadValue );
	{
		__asm volatile( "dsb" );
		__asm volatile( "wfi" );
		__asm volatile( "isb" );
	}
	/* Re-enable interrupts - see comments above the cpsid instruction()
	above. */
	__asm volatile( "cpsie i" );


	ulCompletedSysTickDecrements = vPortSleepRemaining ();

	systick_set_led( pdFALSE );

	if ( ulCompletedSysTickDecrements == 0 )
	{
		ulCompleteTickPeriods = xExpectedIdleTime;
		portNVIC_SYSTICK_LOAD_REG = ( ulTimerCountsForOneTick - 1UL );
	}
	else
	{
		ulCompleteTickPeriods = ulCompletedSysTickDecrements / ulTimerCountsForOneTick;
		portNVIC_SYSTICK_LOAD_REG = ( ulCompletedSysTickDecrements % ulTimerCountsForOneTick ) -1;
	}

	#if( portSLEEP_STATS != 0 )
	{
		xSleepEvents[ iSleepIndex ].ulActualSleepUs = ( uint32_t ) ( ullGetHighResolutionTime() - cur_time );
		xSleepEvents[ iSleepIndex ].xTickCount = xTickCount;
		xSleepEvents[ iSleepIndex ].xExpectedIdleTime = xExpectedIdleTime;
		xSleepEvents[ iSleepIndex ].ulReloadValue = ulReloadValue;
		xSleepEvents[ iSleepIndex ].ulCompletedSysTickDecrements = ulCompletedSysTickDecrements;
		xSleepEvents[ iSleepIndex ].ulCompleteTickPeriods = ulCompleteTickPeriods;
		if( ++iSleepIndex >= ARRAY_SIZE( xSleepEvents ) )
		{
			iSleepIndex = 0;
		}
	}
	#endif	/* portSLEEP_STATS */

	portNVIC_SYSTICK_CURRENT_VALUE_REG = 0UL;
	portENTER_CRITICAL();
	{
		portNVIC_SYSTICK_CTRL_REG |= portNVIC_SYSTICK_ENABLE_BIT;
		vTaskStepTick( ulCompleteTickPeriods );
		portNVIC_SYSTICK_LOAD_REG = ulTimerCountsForOneTick - 1UL;
	}
	portEXIT_CRITICAL();

#if( portSLEEP_STATS != 0 )
	if( xExpectedIdleTime > 0 ) {
		/* Measure the sleeping time */
		uint64_t cur_time = ullGetHighResolutionTime();
		if (last_time) {
			uint64_t diff = cur_time - last_time;
			sleep_time += diff;
			if (diff > longest_sleep)
				longest_sleep = diff;
		}
		switch_count++;
		if ( ulCompletedSysTickDecrements == 0 )
			switch_complete++;
		last_time = cur_time;	
	}
#endif
}

#endif /* ( configUSE_TICKLESS_IDLE == 1 ) */

