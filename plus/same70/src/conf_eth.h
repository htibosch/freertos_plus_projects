 /**
 * \file
 *
 * \brief KSZ8851SNL driver configuration.
 *
 * Copyright (c) 2013-2015 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */
/*
 * Support and FAQ: visit <a href="http://www.atmel.com/design-support/">Atmel Support</a>
 */

#ifndef CONF_ETH_H_INCLUDED
#define CONF_ETH_H_INCLUDED

#error conf_eth.h

/* Standard includes. */
#include <stdint.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"

/** Disable lwIP checksum (performed by hardware). */
#define CHECKSUM_GEN_IP                               0
#define CHECKSUM_GEN_UDP                              0
#define CHECKSUM_GEN_TCP                              0
#define CHECKSUM_GEN_ICMP                             0
#define CHECKSUM_CHECK_IP                             0
#define CHECKSUM_CHECK_UDP                            0
#define CHECKSUM_CHECK_TCP                            0

/** MAC address definition.  The MAC address must be unique on the network. */
#define ETHERNET_CONF_ETHADDR0                        configMAC_ADDR0
#define ETHERNET_CONF_ETHADDR1                        configMAC_ADDR1
#define ETHERNET_CONF_ETHADDR2                        configMAC_ADDR2
#define ETHERNET_CONF_ETHADDR3                        configMAC_ADDR3
#define ETHERNET_CONF_ETHADDR4                        configMAC_ADDR4
#define ETHERNET_CONF_ETHADDR5                        configMAC_ADDR5

/** The IP address being used. */
#define ETHERNET_CONF_IPADDR0                         configIP_ADDR0
#define ETHERNET_CONF_IPADDR1                         configIP_ADDR1
#define ETHERNET_CONF_IPADDR2                         configIP_ADDR2
#define ETHERNET_CONF_IPADDR3                         configIP_ADDR3

/** The gateway address being used. */
#define ETHERNET_CONF_GATEWAY_ADDR0                   configGATEWAY_ADDR0
#define ETHERNET_CONF_GATEWAY_ADDR1                   configGATEWAY_ADDR1
#define ETHERNET_CONF_GATEWAY_ADDR2                   configGATEWAY_ADDR2
#define ETHERNET_CONF_GATEWAY_ADDR3                   configGATEWAY_ADDR3

/** The network mask being used. */
#define ETHERNET_CONF_NET_MASK0                       configNET_MASK0
#define ETHERNET_CONF_NET_MASK1                       configNET_MASK1
#define ETHERNET_CONF_NET_MASK2                       configNET_MASK2
#define ETHERNET_CONF_NET_MASK3                       configNET_MASK3

enum {
	EXT_1_PIN01_ID_1		= PIO_PA28_IDX,	// 
	EXT_1_PIN02_GND			= 0,           	// 
	EXT_1_PIN03_ADC			= PIO_PB2_IDX, 	//
	EXT_1_PIN04_ADC			= PIO_PB3_IDX, 	// 
	EXT_1_PIN05_GPIO		= PIO_PA24_IDX,	//  5 = Debug PIN purple
	EXT_1_PIN06_GPIO		= PIO_PA25_IDX,	//  6 = brownRST ( Reset line for Ethernet, active low )
	EXT_1_PIN07_PWM_PLUS	= PIO_PA15_IDX,	//  7 = DBG2
	EXT_1_PIN08_PWM_MIN		= PIO_PA16_IDX,	// 
	EXT_1_PIN9_IRQ_GPIO		= PIO_PA11_IDX,	//  9 = rood IRQ
	EXT_1_PIN10_SPI_SS_B	= PIO_PD25_IDX,	// 
	EXT_1_PIN11_TWI_SDA		= PIO_PA3_IDX, 	// 
	EXT_1_PIN12_TWI_SCL		= PIO_PA4_IDX, 	// 
	EXT_1_PIN13_UART_RX		= PIO_PA21_IDX,	// 
	EXT_1_PIN14_UART_TX		= PIO_PA22_IDX,	// 
	EXT_1_PIN15_SPI_SS_A	= PIO_PB14_IDX,	// 15 orange SS
	EXT_1_PIN16_SPI_MOSI	= PIO_PA13_IDX,	// 16 yellow MOSI
	EXT_1_PIN17_SPI_MISO	= PIO_PA12_IDX,	// 17 green MISO
	EXT_1_PIN18_SPI_SCK		= PIO_PA14_IDX,	// 18 blue SCK
	EXT_1_PIN19_GROUND		= 0,			// 19 GND
	EXT_1_PIN20_VCC_3V3		= 0,
};

enum {
	EXT_2_PIN01_ID_2		= PIO_PB1_IDX,
	EXT_2_PIN02_GND			= 0,
	EXT_2_PIN03_ADC			= 0,
	EXT_2_PIN04_ADC			= 0,
	EXT_2_PIN05_GPIO		= PIO_PE2_IDX,
	EXT_2_PIN06_GPIO		= PIO_PB5_IDX,
	EXT_2_PIN07_PWM_PLUS	= PIO_PD21_IDX,
	EXT_2_PIN08_PWM_MIN		= 0,
	EXT_2_PIN09_IRQ_GPIO	= PIO_PD29_IDX,
	EXT_2_PIN10_SPI_SS_B	= PIO_PB4_IDX,
	EXT_2_PIN11_TWI_SDA		= PIO_PA3_IDX,
	EXT_2_PIN12_TWI_SCL		= PIO_PA4_IDX,
	EXT_2_PIN13_UART_RX		= PIO_PA5_IDX,
	EXT_2_PIN14_UART_TX		= PIO_PA6_IDX,
	EXT_2_PIN15_SPI_SS_A	= PIO_PD23_IDX,
	EXT_2_PIN16_SPI_MOSI	= PIO_PA13_IDX,
	EXT_2_PIN17_SPI_MISO	= PIO_PA12_IDX,
	EXT_2_PIN18_SPI_SCK		= PIO_PA14_IDX,
	EXT_2_PIN19_GROUND		= 0,
	EXT_2_PIN20_VCC_3V3		= 0,
 };

enum {
	EXT_3_PIN01_ID_2		= PIO_PB2_IDX,
	EXT_3_PIN02_GND			= 0,
	EXT_3_PIN03_ADC			= PIO_PA17_IDX,
	EXT_3_PIN04_ADC			= PIO_PC13_IDX,
	EXT_3_PIN05_GPIO		= PIO_PD28_IDX,
	EXT_3_PIN06_GPIO		= PIO_PD17_IDX,
	EXT_3_PIN07_PWM_PLUS	= PIO_PD20_IDX,
	EXT_3_PIN08_PWM_MIN		= PIO_PD24_IDX,
	EXT_3_PIN09_IRQ_GPIO	= PIO_PE1_IDX,
	EXT_3_PIN10_SPI_SS_B	= PIO_PD26_IDX,
	EXT_3_PIN11_TWI_SDA		= PIO_PA3_IDX,
	EXT_3_PIN12_TWI_SCL		= PIO_PA4_IDX,
	EXT_3_PIN13_UART_RX		= PIO_PA5_IDX,
	EXT_3_PIN14_UART_TX		= PIO_PA6_IDX,
	EXT_3_PIN15_SPI_SS_A	= PIO_PD30_IDX,
	EXT_3_PIN16_SPI_MOSI	= PIO_PA13_IDX,
	EXT_3_PIN17_SPI_MISO	= PIO_PA12_IDX,
	EXT_3_PIN18_SPI_SCK		= PIO_PA14_IDX,
	EXT_3_PIN19_GROUND		= 0,
	EXT_3_PIN20_VCC_3V3		= 0,
};

/** SPI settings. */
#define KSZ8851SNL_SPI                                SPI
#define KSZ8851SNL_CLOCK_SPEED                        30000000
//#define KSZ8851SNL_CLOCK_SPEED                        20000000

#define KSZ8851SNL_CS_PIN                             0

/** Pins configuration. GPIO values need to be set properly. */
#define KSZ8851SNL_RSTN_GPIO                          EXT_1_PIN06_GPIO			// PIO_PA25_IDX
#define KSZ8851SNL_RSTN_FLAGS                         PIO_OUTPUT_1

#define KSZ8851SNL_CSN_GPIO                           EXT_1_PIN15_SPI_SS_A	// PIO_PB14_IDX
#define KSZ8851SNL_CSN_FLAGS                          PIO_OUTPUT_1

/** Push button pin definition. */
/* Using EXT_1_PIN9_IRQ_GPIO	= PIO_PA11_IDX, */
#define KSZ8851SNL_INTN_GPIO                          EXT_1_PIN9_IRQ_GPIO	// PIO_PA11_IDX
#define INTN_PIO                                      PIOA
#define INTN_ID                                       ID_PIOA
#define INTN_PIN_MSK                                  (1 << 11)					// PIO_PA11_IDX
#define INTN_ATTR                                     (PIO_DEBOUNCE | PIO_IT_FALL_EDGE)

/* Interrupt priorities. (lowest value = highest priority) */
/* ISRs using FreeRTOS *FromISR APIs must have priorities below or equal to */
/* configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY. */
#define INT_PRIORITY_SPI                              12
#define INT_PRIORITY_PIO                              12

#define INTN_IRQn                                     PIOA_IRQn

#define TX_USES_RECV				1

#if( TX_USES_RECV == 1 )
	#define SPI_END_OF_TX			SPI_SR_RXBUFF
#else
	#define SPI_END_OF_TX			SPI_SR_TXBUFE
#endif

#define SPI_END_OF_RX			SPI_SR_RXBUFF

#endif /* CONF_ETH_H_INCLUDED */
