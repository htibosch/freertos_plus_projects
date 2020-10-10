/**
 * \file
 *
 * \brief Board configuration.
 *
 * Copyright (c) 2015-2016 Atmel Corporation. All rights reserved.
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

#ifndef CONF_BOARD_H_INCLUDED
#define CONF_BOARD_H_INCLUDED

/* Enable ICache and DCache */
/*
#define CONF_BOARD_ENABLE_CACHE
*/
#warning Disabling CONF_BOARD_ENABLE_CACHE

/** Enable Com Port. */
#define CONF_BOARD_UART_CONSOLE

//#define CONF_BOARD_KSZ8051MNL 1
//#define CONF_BOARD_SRAM       1

//#define CONF_BOARD_SD_MMC_HSMCI		1

///* Needs SPI if the ksz8851snl Ethernet controller is being used. */
//#define CONF_BOARD_SPI			1

///* Enable USB interface (USB) */
//#define CONF_BOARD_USB_PORT
///* ID detect enabled,  uncomment it if jumper PB05/USB set */
//#define CONF_BOARD_USB_ID_DETECT
///* Host VBUS control enabled,  uncomment it if jumper PC08/USB set */
//#define CONF_BOARD_USB_VBUS_CONTROL
///* Host VBUS control enabled,  uncomment it if jumper PC08/USB set */
//#define CONF_BOARD_USB_VBUS_ERR_DETECT

#define _KBYTE_					( 1024ul )
#define _MBYTE_					( _KBYTE_ * _KBYTE_ )

#define FLASH_BASE_ADDRESS		0x00400000
#define ROM_BASE_ADDRESS		0x00800000
#define SRAM_BASE_ADDRESS		0x20400000
//#define SDRAM_BASE_ADDRESS		0x60000000
#define SDRAM_BASE_ADDRESS		0x70000000

#define FLASH_SIZE				( 2ul * _MBYTE_ )
#define ROM_SIZE   				( 4ul * _MBYTE_ )
#define SRAM_SIZE				( 384 * _KBYTE_ )
#define SDRAM_SIZE 				( 16ul * _MBYTE_ )

#endif /* CONF_BOARD_H_INCLUDED */
