/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "main.h"

#include "FreeRTOS.h"
#include "task.h"

#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "NetworkBufferManagement.h"

#include "UDPLoggingPrintf.h"
#include "iperf_task.h"
#include "eventLogging.h"
#include "hr_gettime.h"
#include "tcp_mem_stats.h"


#define HSEM_ID_0 (0U) /* HW semaphore 0*/

UART_HandleTypeDef huart1;

static TaskHandle_t xServerWorkTaskHandle = NULL;

static void vHeapInit( void );
static void vStartRandomGenerator( void );

#define mainTCP_SERVER_STACK_SIZE	640

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

const uint8_t ucMACAddress[ 6 ] = { configMAC_ADDR0, configMAC_ADDR1, configMAC_ADDR2, configMAC_ADDR3, configMAC_ADDR4, configMAC_ADDR5 };

static BaseType_t xTasksAlreadyCreated = pdFALSE;

RNG_HandleTypeDef hrng;
static uint32_t ulSeed;

int verboseLevel = 0;

static BaseType_t xDoCreateSockets;

static void prvServerWorkTask( void *pvParameters );

static BaseType_t run_command_line( void );

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
int32_t timeout = 0xFFFF;
	
	while( ( __HAL_RCC_GET_FLAG( RCC_FLAG_D2CKRDY ) != RESET ) && ( timeout > 0 ) )
	{
		timeout--;
	}
	if ( timeout < 0 )
	{
		Error_Handler();
	}
	/* Enable I-Cache */
	SCB_EnableICache();

	/* Enable D-Cache */
	/* _HT_ This project has not been tested with data cache enabled.
	Changes will be necessary in the network driver to make it work. */
//	SCB_EnableDCache();

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* Configure the system clock */
	SystemClock_Config();

	/*HW semaphore Clock enable*/
	__HAL_RCC_HSEM_CLK_ENABLE();

	/*Take HSEM */
	HAL_HSEM_FastTake(HSEM_ID_0);
	/*Release HSEM in order to notify the CPU2(CM4)*/

	HAL_HSEM_Release(HSEM_ID_0,0);
	/* wait until CPU2 wakes up from stop mode */

	timeout = 0xFFFF;
	while( ( __HAL_RCC_GET_FLAG( RCC_FLAG_D2CKRDY ) == RESET ) && ( timeout > 0 ) )
	{
		timeout--;
	}
	if ( timeout < 0 )
	{
		Error_Handler();
	}

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_USART1_UART_Init();

	vHeapInit();

	vStartRandomGenerator();

	vStartHighResolutionTimer();

	const uint8_t ucIPAddress[ 4 ] = { configIP_ADDR0, configIP_ADDR1, configIP_ADDR2, configIP_ADDR3 };
	const uint8_t ucNetMask[ 4 ] = { configNET_MASK0, configNET_MASK1, configNET_MASK2, configNET_MASK3 };
	const uint8_t ucGatewayAddress[ 4 ] = { configGATEWAY_ADDR0, configGATEWAY_ADDR1, configGATEWAY_ADDR2, configGATEWAY_ADDR3 };
	const uint8_t ucDNSServerAddress[ 4 ] = { configDNS_SERVER_ADDR0, configDNS_SERVER_ADDR1, configDNS_SERVER_ADDR2, configDNS_SERVER_ADDR3 };

	FreeRTOS_printf( ( "Calling FreeRTOS_IPInit\n" ) );

	FreeRTOS_IPInit( ucIPAddress, ucNetMask, ucGatewayAddress, ucDNSServerAddress, ucMACAddress );

	xTaskCreate( prvServerWorkTask, "SvrWork", mainTCP_SERVER_STACK_SIZE, NULL, 1, &xServerWorkTaskHandle );

	/* Start the RTOS scheduler. */
	vTaskStartScheduler();

	/* Infinite loop */
	while (1)
	{
	}
}

/**
 * RAM area	H747	H743	H742	Location
 * ------------------------------------------------
 * DTCM		128k	128k	128k	0x20000000
 * AXI-SRAM	511k	511k	384k	0x24000000
 *
 * SRAM1	128k	128k	32k		0x30000000
 * SRAM2	128k	128k	16k		0x30020000
 * SRAM3	32k		32k	 	-		0x30040000
 * SRAM4	64k		64k		64k		0x38000000
 * Backup   SRAM	4k		4k	4k	0x38800000
 */

static uint8_t ucRAM_1 [384 * 1024] __attribute__( ( section( ".ethernet_data" ) ) );
static uint8_t ucRAM_2 [128 * 1024] __attribute__( ( section( ".ram2_data" ) ) );
static uint8_t ucRAM_3 [ 32 * 1024] __attribute__( ( section( ".ram3_data" ) ) );

#define mainMEM_REGION( REGION )   REGION, sizeof( REGION )

static void vHeapInit( )
{
	/* Note: the memories must be sorted on their physical address. */
	HeapRegion_t xHeapRegions[] = {
		{ mainMEM_REGION( ucRAM_1 ) },
		{ mainMEM_REGION( ucRAM_2 ) },
		{ mainMEM_REGION( ucRAM_3 ) },
		{ NULL, 0 }
		};

	vPortDefineHeapRegions( xHeapRegions );
}
/*-----------------------------------------------------------*/

static void vStartRandomGenerator( void )
{
	/* Enable the clock for the RNG. */
	__HAL_RCC_RNG_CLK_ENABLE();
	RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;
	RNG->CR |= RNG_CR_RNGEN;

	hrng.Instance = RNG;
	hrng.Init.ClockErrorDetection = RNG_CED_ENABLE;
	if (HAL_RNG_Init(&hrng) != HAL_OK)
	{
		Error_Handler();
	}
	/* Get random numbers. */
	HAL_RNG_GenerateRandomNumber( &hrng, &ulSeed );
}
/*-----------------------------------------------------------*/

void HAL_ETH_MspInit(ETH_HandleTypeDef* ethHandle)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	if(ethHandle->Instance==ETH)
	{
		/* USER CODE BEGIN ETH_MspInit 0 */

		/* USER CODE END ETH_MspInit 0 */

		__HAL_RCC_GPIOG_CLK_ENABLE();
		__HAL_RCC_GPIOC_CLK_ENABLE();
		__HAL_RCC_GPIOA_CLK_ENABLE();
		/**ETH GPIO Configuration
		PG11     ------> ETH_TX_EN
		PG12     ------> ETH_TXD1
		PG13     ------> ETH_TXD0
		PC1     ------> ETH_MDC
		PA2     ------> ETH_MDIO
		PA1     ------> ETH_REF_CLK
		PA7     ------> ETH_CRS_DV
		PC4     ------> ETH_RXD0
		PC5     ------> ETH_RXD1
		*/
		GPIO_InitStruct.Pin = ETH_TX_EN_Pin|ETH_TXD1_Pin|ETH_TXD0_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
		HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = ETH_MDC_SAI4_D1_Pin|ETH_RXD0_Pin|ETH_RXD1_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

		GPIO_InitStruct.Pin = ETH_MDIO_Pin|ETH_REF_CLK_Pin|ETH_CRS_DV_Pin;
		GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF11_ETH;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		/* Enable Peripheral clock */
		__HAL_RCC_ETH1MAC_CLK_ENABLE();
		__HAL_RCC_ETH1TX_CLK_ENABLE();
		__HAL_RCC_ETH1RX_CLK_ENABLE();

		/* Peripheral interrupt init */
		HAL_NVIC_SetPriority(ETH_IRQn, 5, 0);
		HAL_NVIC_EnableIRQ(ETH_IRQn);
		HAL_NVIC_SetPriority(ETH_WKUP_IRQn, 5, 0);
		HAL_NVIC_EnableIRQ(ETH_WKUP_IRQn);
		/* USER CODE BEGIN ETH_MspInit 1 */

		/* USER CODE END ETH_MspInit 1 */
	}
}

void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
	RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

	/** Supply configuration update enable
	*/
	HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);
	/** Configure the main internal regulator output voltage
	*/
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY))
	{
	}
	/** Macro to configure the PLL clock source
	*/
	__HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSE);
	/** Initializes the RCC Oscillators according to the specified parameters
	* in the RCC_OscInitTypeDef structure.
	*/
	/* Enable HSE Oscillator and activate PLL with HSE as source */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;

	RCC_OscInitStruct.CSIState = RCC_CSI_OFF;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;

	RCC_OscInitStruct.PLL.PLLM = 5;
	RCC_OscInitStruct.PLL.PLLN = 160;
	RCC_OscInitStruct.PLL.PLLFRACN = 0;
	RCC_OscInitStruct.PLL.PLLP = 2;
	RCC_OscInitStruct.PLL.PLLR = 2;
	RCC_OscInitStruct.PLL.PLLQ = 4;

	RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
	RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;

	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}


	/* Select PLL as system clock source and configure  bus clocks dividers */
	RCC_ClkInitStruct.ClockType = ( RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 | \
	RCC_CLOCKTYPE_PCLK2  | RCC_CLOCKTYPE_D3PCLK1 );

	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
	RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
	if( HAL_RCC_ClockConfig( &RCC_ClkInitStruct, FLASH_LATENCY_4 ) != HAL_OK)
	{
		Error_Handler();
	}

	PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC|RCC_PERIPHCLK_USART1
	|RCC_PERIPHCLK_UART8|RCC_PERIPHCLK_SPDIFRX
	|RCC_PERIPHCLK_SPI5|RCC_PERIPHCLK_SPI2
	|RCC_PERIPHCLK_SAI1|RCC_PERIPHCLK_SDMMC
	|RCC_PERIPHCLK_ADC|RCC_PERIPHCLK_CEC
	|RCC_PERIPHCLK_QSPI|RCC_PERIPHCLK_FMC
	|RCC_PERIPHCLK_RNG;
	PeriphClkInitStruct.PLL2.PLL2M = 13;
	PeriphClkInitStruct.PLL2.PLL2N = 129;
	PeriphClkInitStruct.PLL2.PLL2P = 2;
	PeriphClkInitStruct.PLL2.PLL2Q = 2;
	PeriphClkInitStruct.PLL2.PLL2R = 2;
	PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_0;
	PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
	PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
	PeriphClkInitStruct.FmcClockSelection = RCC_FMCCLKSOURCE_D1HCLK;
	PeriphClkInitStruct.QspiClockSelection = RCC_QSPICLKSOURCE_D1HCLK;
	PeriphClkInitStruct.SdmmcClockSelection = RCC_SDMMCCLKSOURCE_PLL;
	PeriphClkInitStruct.Sai1ClockSelection = RCC_SAI1CLKSOURCE_PLL;
	PeriphClkInitStruct.Spi123ClockSelection = RCC_SPI123CLKSOURCE_PLL;
	PeriphClkInitStruct.Spi45ClockSelection = RCC_SPI45CLKSOURCE_D2PCLK1;
	PeriphClkInitStruct.SpdifrxClockSelection = RCC_SPDIFRXCLKSOURCE_PLL;
	PeriphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
	PeriphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
	PeriphClkInitStruct.CecClockSelection = RCC_CECCLKSOURCE_LSI;
	PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
	PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
	PeriphClkInitStruct.RngClockSelection = RCC_RNGCLKSOURCE_HSI48;

	if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
	{
		Error_Handler();
	}
	HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_HSI, RCC_MCODIV_1);
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{
	huart1.Instance = USART1;
	huart1.Init.BaudRate = 115200;
	huart1.Init.WordLength = UART_WORDLENGTH_8B;
	huart1.Init.StopBits = UART_STOPBITS_1;
	huart1.Init.Parity = UART_PARITY_NONE;
	huart1.Init.Mode = UART_MODE_TX_RX;
	huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart1.Init.OverSampling = UART_OVERSAMPLING_16;
	huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
	huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart1) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
	{
		Error_Handler();
	}
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOG_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();

	/*Configure GPIO pin : CEC_CK_MCO1_Pin */
	GPIO_InitStruct.Pin = CEC_CK_MCO1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
	HAL_GPIO_Init(CEC_CK_MCO1_GPIO_Port, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
	configASSERT( 1 == 0 );
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

uint32_t ulGetRunTimeCounterValue()
{
	return 0U;
}

void vAssertCalled( const char *pcFile, uint32_t ulLine )
{
volatile unsigned long ul = 0;

	( void ) pcFile;
	( void ) ulLine;

	taskENTER_CRITICAL();
	{
		/* Set ul to a non-zero value using the debugger to step out of this
		function. */
		while( ul == 0 )
		{
			__NOP();
		}
	}
	taskEXIT_CRITICAL();
}

void vApplicationMallocFailedHook( void )
{
	/* Called if a call to pvPortMalloc() fails because there is insufficient
	free memory available in the FreeRTOS heap.  pvPortMalloc() is called
	internally by FreeRTOS API functions that create tasks, queues, software
	timers, and semaphores.  The size of the FreeRTOS heap is set by the
	configTOTAL_HEAP_SIZE configuration constant in FreeRTOSConfig.h. */

	/* Force an assert. */
	configASSERT( ( volatile void * ) NULL );
}
/*-----------------------------------------------------------*/

//int lUDPLoggingPrintf( const char *pcFormatString, ... )
//{
//	( void ) pcFormatString;
//	return 0;
//

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

void vApplicationIPNetworkEventHook( eIPCallbackEvent_t eNetworkEvent )
{
	/* If the network has just come up...*/
	if( eNetworkEvent == eNetworkUp )
	{
	uint32_t ulIPAddress, ulNetMask, ulGatewayAddress, ulDNSServerAddress;
	char cBuffer[ 16 ];

		/* Create the tasks that use the IP stack if they have not already been
		created. */
		if( xTasksAlreadyCreated == pdFALSE )
		{
			xTasksAlreadyCreated = pdTRUE;
			/* Sockets, and tasks that use the TCP/IP stack can be created here. */
			xDoCreateSockets = pdTRUE;
		}
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
/*-----------------------------------------------------------*/

const char *pcApplicationHostnameHook( void )
{
	/* Assign the name "STM32H7" to this network node.  This function will be
	called during the DHCP: the machine will be registered with an IP address
	plus this name. */
	return "STM32H7";
}
/*-----------------------------------------------------------*/

BaseType_t xApplicationGetRandomNumber( uint32_t *pulValue )
{
BaseType_t xReturn;

	if( HAL_RNG_GenerateRandomNumber( &hrng, pulValue ) == HAL_OK )
	{
		xReturn = pdPASS;
	}
	else
	{
		xReturn = pdFAIL;
	}

	return xReturn;
}
/*-----------------------------------------------------------*/

struct xREGISTER_STACK {
	uint32_t spare0[ 8 ];
	uint32_t r0;
	uint32_t r1;
	uint32_t r2;
	uint32_t r3;
	uint32_t r12;
	uint32_t lr; /* Link register. */
	uint32_t pc; /* Program counter. */
	uint32_t psr;/* Program status register. */
	uint32_t spare1[ 8 ];
};

volatile struct xREGISTER_STACK *pxRegisterStack = NULL;

void prvGetRegistersFromStack( uint32_t * pulFaultStackAddress )
{
	/* When the debuggger stops here, you can inspect the registeers of the
	application by looking at *pxRegisterStack. */
	pxRegisterStack = ( volatile struct xREGISTER_STACK * )
		( pulFaultStackAddress - ARRAY_SIZE( pxRegisterStack->spare0 ) );

	/* When the following line is hit, the variables contain the register values. */
	for( ;; );
}
/*-----------------------------------------------------------*/

void HardFault_Handler(void)
{
	__asm volatile
	(
		" tst lr, #4                                                \n"
		" ite eq                                                    \n"
		" mrseq r0, msp                                             \n"
		" mrsne r0, psp                                             \n"
		" ldr r1, [r0, #24]                                         \n"
		" bl prvGetRegistersFromStack                               \n"
	);
}
/*-----------------------------------------------------------*/

#define CONTINUOUS_PING	0

static void prvServerWorkTask( void *pvParameters )
{
#if( CONTINUOUS_PING != 0 )
	/* CONTINUOUS_PING can be used while testing the network driver. */
	uint32_t ulIPAddress = FreeRTOS_inet_addr_quick( 192, 168, 2, 5 );
	size_t uxNumberOfBytesToSend = 16;
	TickType_t uxBlockTimeTicks = ipMS_TO_MIN_TICKS( 100U );
#endif	/* ( CONTINUOUS_PING != 0 ) */

	for( ;; )
	{
		vTaskDelay( 10U );
		if( xDoCreateSockets != pdFALSE )
		{
			xDoCreateSockets = pdFALSE;
			/* Start a new task to fetch logging lines and send them out.
			See FreeRTOSConfig.h for the configuration of UDP logging. */
			vUDPLoggingTaskCreate();
			vIPerfInstall();
			#if( USE_LOG_EVENT != 0 )
			{
				iEventLogInit();
			}
			#endif
		}

		#if( CONTINUOUS_PING != 0 )
		{
			if( xTasksAlreadyCreated != pdFALSE )
			{
				FreeRTOS_SendPingRequest( ulIPAddress, uxNumberOfBytesToSend, uxBlockTimeTicks );
			}
		}
		#endif

		run_command_line();
	}
}

#define USE_ZERO_COPY  1

#include "hr_gettime.h"

static BaseType_t run_command_line()
{
char  pcBuffer[ 92 ];
BaseType_t xCount;
struct freertos_sockaddr xSourceAddress;
socklen_t xSourceAddressLength = sizeof( xSourceAddress );
xSocket_t xSocket = xLoggingGetSocket();
static NetworkBufferDescriptor_t *pxDescriptor = NULL;

	if( xSocket == NULL )
	{
		return 0;
	}

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
			&xSourceAddress, &xSourceAddressLength );
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
		lUDPLoggingPrintf( "CPU sped %lu\n", configCPU_CLOCK_HZ );
	}
	else if( strncmp( pcBuffer, "hrtime", 6 ) == 0 )
	{
		static uint64_t lastTime = 0ULL;
		uint64_t curTime = ullGetHighResolutionTime();
		uint32_t difTime = ( uint32_t ) ( ( curTime - lastTime ) / 1000U );

		static TickType_t ulLastTime = 0U;
		TickType_t ulCurTime = xTaskGetTickCount();
		TickType_t ulDifTime = ulCurTime - ulLastTime;

		lUDPLoggingPrintf( "hr_time %lu  FreeRTOS %u\n",
			( uint32_t ) difTime,
			( uint32_t ) ulDifTime );
		lastTime = curTime;
		ulLastTime = ulCurTime;
	}
	else if( memcmp( pcBuffer, "random", 6 ) == 0 )
	{
	uint32_t ulFrequencies[ 32 ];
	uint32_t ulCount;
	uint32_t ulIndex;
	uint32_t ulValue;

		memset( ulFrequencies, 0, sizeof ulFrequencies );
		for( ulCount = 0U; ulCount < 20000; ulCount++ )
		{
			HAL_RNG_GenerateRandomNumber( &hrng, &ulValue );
			for( ulIndex = 0; ulIndex < ARRAY_SIZE( ulFrequencies ); ulIndex++ )
			{
				if( ( ulValue & 0x80000000U ) != 0 )
				{
					ulFrequencies[ ulIndex ]++;
				}
				ulValue <<= 1;
			}
		}
		for( ulIndex = 0; ulIndex < ARRAY_SIZE( ulFrequencies ); ulIndex += 8 )
		{
				/* Expected: +/- 10000  10000  10000  10000  10000  10000  */
			FreeRTOS_printf( ( "%2d - %2d : %6u %6u %6u %6u %6u %6u %6u %6u\n",
				ulIndex, ulIndex+7,
				ulFrequencies[ ulIndex + 0 ],
				ulFrequencies[ ulIndex + 1 ],
				ulFrequencies[ ulIndex + 2 ],
				ulFrequencies[ ulIndex + 3 ],
				ulFrequencies[ ulIndex + 4 ],
				ulFrequencies[ ulIndex + 5 ],
				ulFrequencies[ ulIndex + 6 ],
				ulFrequencies[ ulIndex + 7 ] ) );
		}
		FreeRTOS_printf( ( "\n" ) );
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
		FreeRTOS_printf( ( "Don't know: %s\n", pcBuffer ) );
	}
	return xCount;
}

#if ( ipconfigSUPPORT_OUTGOING_PINGS == 1 )
	void vApplicationPingReplyHook( ePingReplyStatus_t eStatus, uint16_t usIdentifier )
	{
		FreeRTOS_printf( ( "Received ping ID %04X\n", usIdentifier ) );
	}
#endif
/*-----------------------------------------------------------*/
