/* ========================================
 *
 * Copyright YOUR COMPANY, THE YEAR
 * All Rights Reserved
 * UNPUBLISHED, LICENSED SOFTWARE.
 *
 * CONFIDENTIAL AND PROPRIETARY INFORMATION
 * WHICH IS THE PROPERTY OF your company.
 *
 * ========================================
 */
#ifndef _SPI_H_
#define _SPI_H_

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include <task.h>
#include <semphr.h>

#include "project.h"

#define SPI_FILL_CHAR    ( 0xFF )

/* "Class" representing SPIs */
typedef struct
{
    /* SPI HW */
    CySCB_Type * const base;
    cy_stc_scb_spi_context_t * const context;
    cy_stc_scb_spi_config_t const * config;
    const cy_en_divider_types_t dividerType; /* Clock divider for setting SPI frequency */
    const uint32_t dividerNum;               /* Clock divider for setting SPI frequency */
    GPIO_PRT_Type * const spi_miso_port;     /* GPIO port for SPI pins */
    const uint32_t spi_miso_num;             /* SPI MISO pin number for GPIO */
    const cy_stc_sysint_t * const intcfg;    /* SPI ISR */
    const cy_israddress userIsr;             /* SPI ISR */
    /* RX DMA: */
    DW_Type * const rxDma_base;
    const uint32_t rxDma_channel;
    cy_stc_dma_descriptor_t * const rxDma_Descriptor_1;
    const cy_stc_dma_descriptor_config_t * rxDma_Descriptor_1_config;
    const cy_stc_dma_channel_config_t rxDma_channelConfig;
    const cy_israddress intRxDma_ISR;
    const cy_stc_sysint_t * intRxDma_cfg;
    /* TX DMA: */
    DW_Type * const txDma_base;
    const uint32_t txDma_channel;
    cy_stc_dma_descriptor_t * const txDma_Descriptor_1;
    const cy_stc_dma_descriptor_config_t * txDma_Descriptor_1_config;
    const cy_stc_dma_channel_config_t txDma_channelConfig;
    const cy_israddress intTxDma_ISR;
    const cy_stc_sysint_t * intTxDma_cfg;
    /* State variables: */
    bool initialized;        /* Assigned dynamically */
    TaskHandle_t owner;      /* Assigned dynamically */
    SemaphoreHandle_t mutex; /* Assigned dynamically */
} spi_t;

bool spi_init( spi_t * pSD );

/* SPI Interrupt callback */
void spi_ISR( spi_t * this );

/* SPI DMA interrupts */
void spi_RxDmaComplete( spi_t * this );
void spi_TxDmaComplete( spi_t * this );

void spi_dma_transfer( spi_t * this, uint16 len, const uint8_t * tx, uint8_t * rx );

#endif /* ifndef _SPI_H_ */
/* [] END OF FILE */
