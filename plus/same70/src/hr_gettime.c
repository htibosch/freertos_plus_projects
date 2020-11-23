/*
    FreeRTOS V8.2.3 - Copyright (C) 2015 Real Time Engineers Ltd.
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

/* High resolution timer functions - used for logging only. */

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

/* Atmel includes. */
#include "board.h"
#include "tc.h"
#include "hr_gettime.h"
#include "asf.h"

/* The TC interrupt will become active as little as possible: every half
minute. */
//#define hrSECONDS_PER_INTERRUPT		30ul
#define hrSECONDS_PER_INTERRUPT		10ul
#define hrMICROSECONDS_PER_SECOND	1000000ull

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x)	(int)(sizeof(x)/sizeof(x)[0])
#endif

/*-----------------------------------------------------------*/

/* Timer interrupt handler. */
void TC0_Handler ( void );

/*-----------------------------------------------------------*/

/* The timer channel to use. */
static const uint32_t ulTCChannel = 0;

/* Interrupt count. */
static uint32_t ulInterruptCount;

/*-----------------------------------------------------------*/

void tc_set_emr(Tc *p_tc, uint32_t ul_channel, uint32_t ul_mode)
{
	if (ul_channel >= ARRAY_SIZE(p_tc->TC_CHANNEL))
		return;
	// Set Extended Mode Register
	p_tc->TC_CHANNEL[ul_channel].TC_EMR = ul_mode;
}

#define CHANNEL_0		0
#define CHANNEL_1		1
#define CHANNEL_2		2

volatile Tc *global_timer;

void vStartHighResolutionTimer( void )
{
uint32_t cpu_hz;

global_timer = TC0;

	cpu_hz = sysclk_get_cpu_hz();

	{
		volatile unsigned i;
		volatile uint32_t counter;
		uint32_t value;

		tc_set_block_mode(TC0, 0x08);
		/* Timer initialisation. */
		/* Channel 0 : MCK/32 or 150000000 / 32 = 4,687,500 */
		tc_init( TC0, CHANNEL_0, TC_CMR_TCCLKS_TIMER_CLOCK1 | TC_CMR_WAVSEL_UP_RC | TC_CMR_WAVE );
		tc_set_emr( TC0, CHANNEL_0, TC_EMR_NODIVCLK );
		tc_write_ra( TC0, CHANNEL_0, 0xffff );
//		tc_write_rc( TC0, CHANNEL_0, 0xffff );
		tc_start( TC0, CHANNEL_0 );

	//4687500 / 65536 = 71.52557373046875
	//256*256=65536

		/* Channel 1 : Clock selected: XC0  at 71.52557373046875 Hz */
		tc_init( TC0, CHANNEL_1, TC_CMR_TCCLKS_XC0 | TC_CMR_WAVSEL_UP_RC | TC_CMR_WAVE );
	//	tc_set_emr( TC0, CHANNEL_1, TC_EMR_NODIVCLK );
		tc_write_ra( TC0, CHANNEL_1, 0xffff );
//		tc_write_rc( TC0, CHANNEL_1, 0xffff );
		tc_start( TC0, CHANNEL_1 );

//		for (counter = 0; counter < 0x80000000; counter++) {
//			__NOP();
//		}
		for (i = 0; i < 4; i++) {
			if (i == 1)
				continue;
			value = (i << 0) | (i << 2) | (i << 4);

			tc_stop( TC0, CHANNEL_0 );
			tc_stop( TC0, CHANNEL_1 );
			tc_set_block_mode(TC0, value);
			tc_start( TC0, CHANNEL_0 );
			tc_start( TC0, CHANNEL_1 );

			for (counter = 0; counter < 10; counter++) {
				__NOP();
			}
			counter = 0;
		}
	}
	
//	NVIC_SetPriority( TC0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY );
//	NVIC_EnableIRQ( ( IRQn_Type ) TC0_IRQn );

	/* Enable this TC timer channel. */
//	tc_enable_interrupt( TC0, CHANNEL_1, TC_IER_CPCS );
}
/*-----------------------------------------------------------*/

void TC0_Handler( void )
{
	if( ( TC0->TC_CHANNEL[ CHANNEL_1 ].TC_SR & TC_SR_CPCS ) != 0 )
	{
		ulInterruptCount++;
	}
}
/*-----------------------------------------------------------*/

uint64_t ullGetHighResolutionTime( void )
{
static uint32_t ulCPUTicksPer_us = 0ul;
uint32_t ulCounts[2];
uint32_t ulSlowCount;
uint64_t ullReturn;


	if( ulCPUTicksPer_us == 0ul )
	{
		/* Calculate the number of CPU ticks per second. */
		ulCPUTicksPer_us = ( uint32_t ) ( ( sysclk_get_cpu_hz() + ( hrMICROSECONDS_PER_SECOND / 2 ) ) / hrMICROSECONDS_PER_SECOND );
	}

	for( ;; )
	{
		ulCounts[ 0 ] = tc_read_cv( TC0, ulTCChannel );
		ulSlowCount = ulInterruptCount;
		ulCounts[ 1 ] = tc_read_cv( TC0, ulTCChannel );

		if( ulCounts[ 1 ] >= ulCounts[ 0 ] )
		{
			/* TC0_Handler() has not occurred in between. */
			break;
		}
	}
	ullReturn = ( uint64_t )ulSlowCount * hrSECONDS_PER_INTERRUPT * hrMICROSECONDS_PER_SECOND + ( ulCounts[ 1 ] / ulCPUTicksPer_us );

	return ullReturn;
}

void vHRStop( void )
{
	/* Disable this TC timer channel. */
	tc_disable_interrupt( TC0, ulTCChannel, TC_IER_CPCS );
	NVIC_DisableIRQ( GMAC_IRQn );
}

void vHRStart( void )
{
	/* Enable this TC timer channel. */
	tc_enable_interrupt( TC0, ulTCChannel, TC_IER_CPCS );
	NVIC_EnableIRQ( GMAC_IRQn );
}
