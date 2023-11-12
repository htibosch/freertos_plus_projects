/*
 * FreeRTOS+TCP V2.3.4
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* FreeRTOS includes. */
#if( USE_FREERTOS != 0 )
	#include "FreeRTOS.h"
	#include "task.h"
	#include "queue.h"
#else
	#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
	#define pdTRUE  1
	#define pdFALSE 0
#endif

#include "hr_gettime.h"

#if defined ( __ICCARM__ ) /*!< IAR Compiler */
	#define _NO_DEFINITIONS_IN_HEADER_FILES		1
#endif

#include "stm32h7xx_hal.h"

TIM_HandleTypeDef timer_handle;

static const uint32_t ulReloadCount = 10000000ul;
static const uint32_t ulPrescale = 84ul;

/*static*/ uint32_t ulInterruptCount = 0;
char hr_started = 0;

//#define TIMER_INSTANCE      TIM3
//#define TIMER_IRQ           TIM3_IRQn
//#define TIMER_IRQ_HANDLER   TIM3_IRQHandler
//#define TIMER_CLK_ENABLE    __HAL_RCC_TIM3_CLK_ENABLE

#define TIMER_INSTANCE      TIM2
#define TIMER_IRQ           TIM2_IRQn
#define TIMER_IRQ_HANDLER   TIM2_IRQHandler
#define TIMER_CLK_ENABLE    __HAL_RCC_TIM2_CLK_ENABLE

uint32_t ulTimerFlags;
static int ignoredFirst = pdFALSE;
void TIMER_IRQ_HANDLER(void)
{
	ulTimerFlags = timer_handle.Instance->SR;
	if( ( ulTimerFlags & TIM_FLAG_UPDATE ) != 0 )
	{
		__HAL_TIM_CLEAR_FLAG( &timer_handle, TIM_FLAG_UPDATE );
		/* Ignore the initial interrupt which sets ulInterruptCount = 1.*/
		if (ignoredFirst) {
			ulInterruptCount++;
		} else {
			ignoredFirst = pdTRUE;
		}
	}
}


/* Timer2 initialization function */
void vStartHighResolutionTimer( void )
{
	/* TIM2 clock enable */
	TIMER_CLK_ENABLE();

	timer_handle.Instance = TIMER_INSTANCE;     /* Register base address             */

	timer_handle.Init.Prescaler = ( ulPrescale - 1ul );			/* Specifies the prescaler value used to divide the TIM clock. */
	timer_handle.Init.CounterMode = TIM_COUNTERMODE_UP;			/* Specifies the counter mode. */
	timer_handle.Init.Period = ( ulReloadCount - 1ul );			/* Specifies the period value to be loaded into the active. */
	timer_handle.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;	/* Specifies the clock division. */
	timer_handle.Init.RepetitionCounter = 0ul;					/* Specifies the repetition counter value. */
	timer_handle.Channel = HAL_TIM_ACTIVE_CHANNEL_1;

	/* NVIC configuration for DMA transfer complete interrupt */
	HAL_NVIC_SetPriority( TIMER_IRQ, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0 );
	HAL_NVIC_EnableIRQ( TIMER_IRQ );


	HAL_TIM_Base_Init( &timer_handle );
	HAL_TIM_Base_Start_IT( &timer_handle );
	ulTimerFlags = timer_handle.Instance->SR;

	hr_started = 1;
}

void vStopHighResolutionTimer( void )
{
	if (hr_started) {
		HAL_TIM_Base_Stop_IT (&timer_handle);
		HAL_NVIC_DisableIRQ (TIMER_IRQ);
		hr_started = 0;
	}
}

uint64_t ullGetHighResolutionTime()
{
uint64_t ullReturn;
	if (!hr_started)
		return 0ull;
	if( timer_handle.Instance == NULL )
	{
		#if( USE_FREERTOS != 0 )
			ullReturn = 1000ull * xTaskGetTickCount();
		#else
			ullReturn = 1000ull * HAL_GetTick();
		#endif
	}
	else
	{
	uint32_t ulCounts[2];
	uint32_t ulSlowCount;

		for( ;; )
		{
			ulCounts[ 0 ] = timer_handle.Instance->CNT;
			ulSlowCount = ulInterruptCount;
			ulCounts[ 1 ] = timer_handle.Instance->CNT;
			if( ulCounts[ 1 ] >= ulCounts[ 0 ] )
			{
				/* TIMER_IRQ_HANDLER() has not occurred in between. */
				break;
			}
		}
		ullReturn = ( uint64_t )ulSlowCount * ulReloadCount + ulCounts[ 1 ];
	}

	return ullReturn;
}

void vSetHighResolutionTime(unsigned long long aMsec)
{

//	// HR_RELOAD_COUNT = 2000
//	ullReturn = ( uint64_t )ulSlowCount * HR_RELOAD_COUNT + ulCounts[ 1 ];

	ulInterruptCount = aMsec / ulReloadCount;
	timer_handle.Instance->CNT = aMsec % ulReloadCount;
}
