/*
    FreeRTOS V8.2.1 - Copyright (C) 2015 Real Time Engineers Ltd.
    All rights reserved

    VISIT http://www.FreeRTOS.org TO ENSURE YOU ARE USING THE LATEST VERSION.

    This file is part of the FreeRTOS distribution.

    FreeRTOS is free software; you can redistribute it and/or modify it under
    the terms of the GNU General Public License (version 2) as published by the
    Free Software Foundation >>!AND MODIFIED BY!<< the FreeRTOS exception.

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

/*-----------------------------------------------------------
 * Simple IO routines to control the LEDs.
 * This file is called ParTest.c for historic reasons.  Originally it stood for
 * PARallel port TEST.
 *-----------------------------------------------------------*/

/* Scheduler includes. */
#include "FreeRTOS.h"
#include "task.h"

/* Demo includes. */
#include "partest.h"

/* Xilinx includes. */
#include "xgpiops.h"
#include "xscugic.h"
#include "xil_exception.h"

#define partstNUM_LEDS			( 1 )
#define partstDIRECTION_INPUT	( 0 )
#define partstDIRECTION_OUTPUT	( 1 )
#define partstOUTPUT_DISABLED	( 0 )
#define partstOUTPUT_ENABLED	( 1 )

/*-----------------------------------------------------------*/

static XGpioPs xGpio;

/*-----------------------------------------------------------*/

static void vGPIOGeneralHandler( XGpioPs *InstancePtr );
static void vGPIOInterruptHandler( void *CallBackRef, u32 Bank, u32 Status );

extern XScuGic xInterruptController; 	/* declared in RTOSDemo/src/FreeRTOS_tick_config.c */

static int iButtonBank;
static uint32_t ulButtonMask;

void vParTestInitialise( void )
{
XGpioPs_Config *pxConfigPtr;
BaseType_t xStatus;
uint8_t ucButtonPinNr, ucButtonBank;

	/* Initialise the GPIO driver. */
	pxConfigPtr = XGpioPs_LookupConfig( XPAR_XGPIOPS_0_DEVICE_ID );
	xStatus = XGpioPs_CfgInitialize( &xGpio, pxConfigPtr, pxConfigPtr->BaseAddr );
	configASSERT( xStatus == XST_SUCCESS );
	( void ) xStatus; /* Remove compiler warning if configASSERT() is not defined. */

	/* Enable outputs and set low. */
	XGpioPs_SetDirectionPin( &xGpio, mainTASK_LED, partstDIRECTION_OUTPUT );
	XGpioPs_SetOutputEnablePin( &xGpio, mainTASK_LED, partstOUTPUT_ENABLED );
	XGpioPs_WritePin( &xGpio, mainTASK_LED, 0x0 );

	XGpioPs_SetDirectionPin( &xGpio, mainTASK_BUTTON, partstDIRECTION_INPUT );
	XGpioPs_SetOutputEnablePin( &xGpio, mainTASK_BUTTON, partstOUTPUT_DISABLED );

	XGpioPs_GetBankPin( mainTASK_BUTTON, &ucButtonBank, &ucButtonPinNr );
	iButtonBank = ucButtonBank;
	ulButtonMask = 1ul << ucButtonPinNr;

	/* Configure 'mainTASK_BUTTON' as a GPIO interrupt. */
	XGpioPs_SetCallbackHandler( &xGpio, (void *)&xGpio, vGPIOInterruptHandler );

	XGpioPs_IntrEnablePin( &xGpio, mainTASK_BUTTON );
	XGpioPs_SetIntrTypePin( &xGpio, mainTASK_BUTTON, XGPIOPS_IRQ_TYPE_EDGE_RISING );

	XScuGic_Connect( &xInterruptController, XPS_GPIO_INT_ID, (Xil_ExceptionHandler)vGPIOGeneralHandler, ( void * ) &xGpio );
	XScuGic_Enable( &xInterruptController, 52);
}
/*-----------------------------------------------------------*/

static BaseType_t xHigherPriorityTaskWoken = pdFALSE;

volatile u8 Bank;
volatile u32 IntrStatus;
volatile u32 IntrEnabled;
static void vGPIOGeneralHandler(XGpioPs *InstancePtr)
{

	xHigherPriorityTaskWoken = pdFALSE;

	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

	for( Bank = 0; Bank < XGPIOPS_MAX_BANKS; Bank++ )
	{
		IntrStatus = XGpioPs_IntrGetStatus(InstancePtr, Bank);
		if( IntrStatus != 0 )
		{
			IntrEnabled = XGpioPs_IntrGetEnabled(InstancePtr,
							      Bank);
			XGpioPs_IntrClear(InstancePtr, Bank,
					   IntrStatus & IntrEnabled);
			InstancePtr->Handler( ( void * ) InstancePtr->CallBackRef, Bank, ( IntrStatus & IntrEnabled ) );
		}
	}
	if( xHigherPriorityTaskWoken != pdFALSE )
	{
		portYIELD_FROM_ISR( pdTRUE );
	}
}
/*-----------------------------------------------------------*/

void vHandleButtonClick( BaseType_t xButton, BaseType_t *pxHigherPriorityTaskWoken );

static void vGPIOInterruptHandler(void *CallBackRef, u32 Bank, u32 Status)
{
	/* Interrupt has been clear already. */
/*
	XGpioPs *Gpioint = (XGpioPs *)CallBackRef;
	XGpioPs_IntrClearPin( Gpioint, mainTASK_BUTTON );
*/
	if( ( Bank == iButtonBank ) && ( Status & ulButtonMask ) != 0 )
	{
		vHandleButtonClick( mainTASK_BUTTON, &xHigherPriorityTaskWoken );
	}
}
/*-----------------------------------------------------------*/

void vParTestSetLED( UBaseType_t uxLED, BaseType_t xValue )
{
	( void ) uxLED;
	XGpioPs_WritePin( &xGpio, mainTASK_LED, xValue );
}
/*-----------------------------------------------------------*/

static BaseType_t xLEDState = 1;
void vParTestToggleLED( unsigned portBASE_TYPE uxLED )
{

	( void ) uxLED;
	XGpioPs_WritePin( &xGpio, mainTASK_LED, xLEDState );
	xLEDState = !xLEDState;
}

BaseType_t ParTestReadButton( void )
{
	return XGpioPs_ReadPin( &xGpio, mainTASK_BUTTON ) != 0;
}


//XGpioPs_IntrClearPin( &xGpio, mainTASK_BUTTON );
