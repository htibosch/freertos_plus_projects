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
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "hr_gettime.h"

#if defined ( __ICCARM__ ) /*!< IAR Compiler */
	#define _NO_DEFINITIONS_IN_HEADER_FILES		1
#endif

#include "stm32f7xx_hal.h"

TIM_HandleTypeDef timer_handle;

static uint32_t ulInterruptCount = 0;

#if defined( HR_TIMER_INSTANCE )
	/* Settings are probably set in 'stm32f7xx_hal_conf.h' */
#else
	#define HR_TIMER_INSTANCE      TIM2
	#define HR_TIMER_IRQ           TIM2_IRQn
	#define HR_TIMER_IRQ_HANDLER   TIM2_IRQHandler
	#define HR_TIMER_CLK_ENABLE    __HAL_RCC_TIM2_CLK_ENABLE
	#define HR_TIMER_DIVISION      TIM_CLOCKDIVISION_DIV1
	#define HR_TIMER_CHANNEL       HAL_TIM_ACTIVE_CHANNEL_1
	#define HR_TIMER_RELOAD_COUNT  10000000ul
	#define HR_TIMER_PRESCALE      100ul  /* When the MCU runs at 200 MHz. */
#endif

static const uint32_t ulReloadCount = HR_TIMER_RELOAD_COUNT;
static const uint32_t ulPrescale = HR_TIMER_PRESCALE;

uint32_t ulTimerFlags;
static int ignoredFirst = pdFALSE;

void HR_TIMER_IRQ_HANDLER(void)
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
	/* TIMx clock enable */
	HR_TIMER_CLK_ENABLE();

	timer_handle.Instance = HR_TIMER_INSTANCE;            /* Register base address, e.g. TIM2 */

	timer_handle.Init.Prescaler = ( ulPrescale - 1ul );   /* Prescaler value used to divide the TIM clock. */
	timer_handle.Init.CounterMode = TIM_COUNTERMODE_UP;   /* Select counter mode. */
	timer_handle.Init.Period = ( ulReloadCount - 1ul );   /* Period value to be loaded into the active. */
	timer_handle.Init.ClockDivision = HR_TIMER_DIVISION;  /* Clock division. */
	timer_handle.Init.RepetitionCounter = 0ul;            /* Repetition counter value. */
	timer_handle.Channel = HR_TIMER_CHANNEL;

	/* NVIC configuration for DMA transfer complete interrupt */
	HAL_NVIC_SetPriority( HR_TIMER_IRQ, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY + 1, 0 );
	HAL_NVIC_EnableIRQ( HR_TIMER_IRQ );


	HAL_TIM_Base_Init( &timer_handle );
	HAL_TIM_Base_Start_IT( &timer_handle );
	ulTimerFlags = timer_handle.Instance->SR;
}

uint64_t ullGetHighResolutionTime()
{
uint64_t ullReturn;
	if( timer_handle.Instance == NULL )
	{
		ullReturn = 1000ull * xTaskGetTickCount();
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
				/* HR_TIMER_IRQ_HANDLER() has not occurred in between. */
				break;
			}
		}
		ullReturn = ( uint64_t )ulSlowCount * ulReloadCount + ulCounts[ 1 ];
	}

	return ullReturn;
}

void timer_start( void )
{
	HAL_NVIC_EnableIRQ( HR_TIMER_IRQ );
}

void timer_stop( void )
{
	HAL_NVIC_DisableIRQ( HR_TIMER_IRQ );
}
