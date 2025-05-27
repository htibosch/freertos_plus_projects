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
/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "hr_gettime.h"

#if defined ( __ICCARM__ ) /*!< IAR Compiler */
	#define _NO_DEFINITIONS_IN_HEADER_FILES		1
#endif

#include "stm32h7xx_hal.h"

TIM_HandleTypeDef tim2_handle;

static const uint32_t ulReloadCount = 10000000ul;
static const uint32_t ulPrescale = 200; // 64;

//84 / 1.3102697366785951497994115469534 =

static uint32_t ulInterruptCount = 0;

uint32_t ulTimer2Flags;
void TIM2_IRQHandler(void)
{
	ulTimer2Flags = tim2_handle.Instance->SR;
	if( ( ulTimer2Flags & TIM_FLAG_UPDATE ) != 0 )
	{
		__HAL_TIM_CLEAR_FLAG( &tim2_handle, TIM_FLAG_UPDATE );
		ulInterruptCount++;
	}
}


/* Timer2 initialization function */
void vStartHighResolutionTimer( void )
{
	/* TIM2 clock enable */
	__HAL_RCC_TIM2_CLK_ENABLE();

	tim2_handle.Instance = TIM2;     /* Register base address             */

	tim2_handle.Init.Prescaler = ( ulPrescale - 1ul );			/* Specifies the prescaler value used to divide the TIM clock. */
	tim2_handle.Init.CounterMode = TIM_COUNTERMODE_UP;			/* Specifies the counter mode. */
	tim2_handle.Init.Period = ( ulReloadCount - 1ul );			/* Specifies the period value to be loaded into the active. */
	tim2_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;	/* Specifies the clock division. */
	tim2_handle.Init.RepetitionCounter = 0ul;					/* Specifies the repetition counter value. */
	tim2_handle.Channel = HAL_TIM_ACTIVE_CHANNEL_1;

	/* NVIC configuration for DMA transfer complete interrupt */
	HAL_NVIC_SetPriority( TIM2_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0 );
	HAL_NVIC_EnableIRQ( TIM2_IRQn );


	HAL_TIM_Base_Init( &tim2_handle );
	HAL_TIM_Base_Start_IT( &tim2_handle );
	ulTimer2Flags = tim2_handle.Instance->SR;
	/* Ignore the initial interrupt which sets ulInterruptCount = 1.*/
	ulInterruptCount = 0ul;
}

uint64_t ullGetHighResolutionTime()
{
uint64_t ullReturn;
	if( tim2_handle.Instance == NULL )
	{
		ullReturn = 1000ull * xTaskGetTickCount();
	}
	else
	{
	uint32_t ulCounts[2];
	uint32_t ulSlowCount;

		for( ;; )
		{
			ulCounts[ 0 ] = tim2_handle.Instance->CNT;
			ulSlowCount = ulInterruptCount;
			ulCounts[ 1 ] = tim2_handle.Instance->CNT;
			if( ulCounts[ 1 ] >= ulCounts[ 0 ] )
			{
				/* TIM2_IRQHandler() has not occurred in between. */
				break;
			}
		}
		ullReturn = ( uint64_t )ulSlowCount * ulReloadCount + ulCounts[ 1 ];
	}

	return ullReturn;
}

