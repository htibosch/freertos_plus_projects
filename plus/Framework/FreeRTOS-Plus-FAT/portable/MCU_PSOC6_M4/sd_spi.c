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

/* Standard includes. */
#include <stdint.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "FreeRTOSFATConfig.h" /* for DBG_PRINTF */

#include "project.h"

#include "spi.h"
#include "sd_card.h"
#include "sd_spi.h"

/* Lock the SPI and set SS appropriately for this SD Card */
/* Note: The Cy_SCB_SPI_SetActiveSlaveSelect really only needs to be done here if multiple SDs are on the same SPI. */
void sd_spi_acquire( sd_card_t * this )
{
    xSemaphoreTakeRecursive( this->spi->mutex, portMAX_DELAY );
    this->spi->owner = xTaskGetCurrentTaskHandle();

    /* Function Name: Cy_SCB_SPI_SetActiveSlaveSelect */
    /* Set active slave select line */
    /* Note: */
    /* Calling this function when the SPI is busy (master preforms data transfer */
    /*  or slave communicates with the master) may cause transfer corruption */
    /*  because the hardware stops driving the outputs and ignores the inputs. */
    /*  Ensure that the SPI is not busy before calling this function. */
    configASSERT( !Cy_SCB_SPI_IsBusBusy( this->spi->base ) );
    Cy_SCB_SPI_SetActiveSlaveSelect( this->spi->base, this->ss );
/*	Cy_SCB_SPI_Enable(this->spi->base); */
}
void sd_spi_release( sd_card_t * this )
{
    configASSERT( !Cy_SCB_SPI_IsBusBusy( this->spi->base ) );
/*	Cy_SCB_SPI_Disable(this->spi->base, this->spi->context); */
    this->spi->owner = 0;
    xSemaphoreGiveRecursive( this->spi->mutex );
}

void sd_spi_go_high_frequency( sd_card_t * this )
{
    /* 100 kHz: */
    /* In TopDesign.cysch, initial clock setting is */
    /*   Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_8_BIT, 1u, 124u); */
    /* as seen in cyfitter_cfg.c */
    /* 400 kHz: */
    /*   Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_8_BIT, 0u, 30u); */
    /* For 20 MHz: */
    /*   Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_8_BIT, 0u, 0u); */
    /* HOWEVER: "Actual data rate (kbps): 12500 */
    /*   (with 4X oversample (the minimum). */
    /* For 1 Mhz: */
    /*  UserSDCrd_SPI_SCBCLK_SetDivider(12u); */
    /* For 6 Mhz: */
    /*  UserSDCrd_SPI_SCBCLK_SetDivider(1u); */

    /* For 12.5 MHz:  0 */
    const static uint32_t div = 1; /* Gives 6250 kHz */

    if( div == Cy_SysClk_PeriphGetDivider( this->spi->dividerType,
                                           this->spi->dividerNum ) )
    {
        /* Nothing to do */
        sd_spi_release( this );
        return;
    }

    Cy_SCB_SPI_Disable( this->spi->base, this->spi->context );
    cy_en_sysclk_status_t rc;
    rc = Cy_SysClk_PeriphDisableDivider( this->spi->dividerType,
                                         this->spi->dividerNum );
    configASSERT( CY_SYSCLK_SUCCESS == rc );
    rc = Cy_SysClk_PeriphSetDivider( this->spi->dividerType,
                                     this->spi->dividerNum, div );
    configASSERT( CY_SYSCLK_SUCCESS == rc );
    rc = Cy_SysClk_PeriphEnableDivider( this->spi->dividerType,
                                        this->spi->dividerNum );
    configASSERT( CY_SYSCLK_SUCCESS == rc );
    Cy_SCB_SPI_Enable( this->spi->base );
}
void sd_spi_go_low_frequency( sd_card_t * this )
{
    /* In TopDesign.cysch, initial clock setting (for 100 kHz) is */
    /*   Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_8_BIT, 1u, 124u); */
    /* as seen in cyfitter_cfg.c. */
    /* For 100 kHz: */
    /*   UserSDCrd_SPI_SCBCLK_SetDivider(124u); */
    /* 400 kHz: 30u */
    const static uint32_t div = 124u;

    if( div == Cy_SysClk_PeriphGetDivider( this->spi->dividerType,
                                           this->spi->dividerNum ) )
    {
        return;
    }

    Cy_SCB_SPI_Disable( this->spi->base, this->spi->context );
    cy_en_sysclk_status_t rc;
    rc = Cy_SysClk_PeriphDisableDivider( this->spi->dividerType,
                                         this->spi->dividerNum );
    configASSERT( CY_SYSCLK_SUCCESS == rc );
    rc = Cy_SysClk_PeriphSetDivider( this->spi->dividerType,
                                     this->spi->dividerNum, div );
    configASSERT( CY_SYSCLK_SUCCESS == rc );
    rc = Cy_SysClk_PeriphEnableDivider( this->spi->dividerType,
                                        this->spi->dividerNum );
    configASSERT( CY_SYSCLK_SUCCESS == rc );
    Cy_SCB_SPI_Enable( this->spi->base );
}

/* SPI Transfer: Read & Write (simultaneously) on SPI bus */
/*   If the data that will be received is not important, pass NULL as rx. */
/*   If the data that will be transmitted is not important, */
/*     pass NULL as tx and then the SPI_FILL_CHAR is sent out as each data element. */
bool sd_spi_transfer( sd_card_t * this,
                      const uint8_t * tx,
                      uint8_t * rx,
                      size_t length )
{
    BaseType_t rc;

    configASSERT( 512 == length );
    configASSERT( tx || rx );

    sd_spi_acquire( this );

    configASSERT( !Cy_SCB_SPI_IsBusBusy( this->spi->base ) );
    configASSERT( !Cy_SCB_SPI_GetNumInTxFifo( this->spi->base ) );
/*	Cy_SCB_SPI_ClearTxFifoStatus(this->spi->base, CY_SCB_SPI_TX_TRIGGER); */

    configASSERT( !Cy_SCB_SPI_GetNumInRxFifo( this->spi->base ) );
/*	Cy_SCB_SPI_ClearRxFifoStatus(this->spi->base, CY_SCB_SPI_RX_TRIGGER); */

/*	Cy_SCB_SPI_ClearRxFifo(this->spi->base); */
/*	Cy_SCB_SPI_ClearTxFifo(this->spi->base); */

    /* Ensure this task does not already have a notification pending by calling
     * ulTaskNotifyTake() with the xClearCountOnExit parameter set to pdTRUE, and a block time of 0
     * (don't block). */
    rc = ulTaskNotifyTake( pdTRUE, 0 );
    configASSERT( !rc );

    /* Unmasking only the spi done interrupt bit */
    this->spi->base->INTR_M_MASK = SCB_INTR_M_SPI_DONE_Msk;

    spi_dma_transfer( this->spi, length, tx, rx );

    /* Timeout 1 sec */
    uint32_t timeOut = 1000;

    /* Wait until master completes transfer or time out has occured. */
    /* Wait 2x, for RX and SPI to complete. */
    /* uint32_t ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait ); */
    rc = ulTaskNotifyTake( pdFALSE, pdMS_TO_TICKS( timeOut ) ); /* Wait for notification from ISR */

    if( !rc )
    {
        /* This indicates that xTaskNotifyWait() returned without the calling task receiving a task notification. */
        /* The calling task will have been held in the Blocked state to wait for its notification state to become pending, */
        /* but the specified block time expired before that happened. */
        DBG_PRINTF( "Task %s timed out in %s\n",
                    pcTaskGetName( xTaskGetCurrentTaskHandle() ), __FUNCTION__ );
        sd_spi_release( this );
        return false;
    }

    rc = ulTaskNotifyTake( pdFALSE, pdMS_TO_TICKS( timeOut ) ); /* Wait for notification from ISR */

    if( !rc )
    {
        /* This indicates that xTaskNotifyWait() returned without the calling task receiving a task notification. */
        /* The calling task will have been held in the Blocked state to wait for its notification state to become pending, */
        /* but the specified block time expired before that happened. */
        DBG_PRINTF( "Task %s timed out in %s\n",
                    pcTaskGetName( xTaskGetCurrentTaskHandle() ), __FUNCTION__ );
        sd_spi_release( this );
        return false;
    }

    /* There should be no more notifications pending: */
    configASSERT( !ulTaskNotifyTake( pdTRUE, 0 ) );
    configASSERT( !Cy_SCB_SPI_IsBusBusy( this->spi->base ) );
    configASSERT( CY_DMA_CHANNEL_DISABLED == Cy_DMA_Descriptor_GetChannelState( this->spi->txDma_Descriptor_1 ) );
    configASSERT( Cy_SCB_SPI_IsTxComplete( this->spi->base ) );
    configASSERT( 0 == Cy_SCB_SPI_GetNumInRxFifo( this->spi->base ) );
    configASSERT( 0 == Cy_SCB_SPI_GetNumInTxFifo( this->spi->base ) );

    sd_spi_release( this );
    return true;
}

uint8_t sd_spi_write( sd_card_t * this,
                      const uint8_t value )
{
    uint8_t received = 0xFF;

    sd_spi_acquire( this );

    configASSERT( !Cy_SCB_SPI_IsBusBusy( this->spi->base ) );
    configASSERT( !Cy_SCB_SPI_GetNumInTxFifo( this->spi->base ) );
    configASSERT( !Cy_SCB_SPI_GetNumInRxFifo( this->spi->base ) );

/*	Cy_SCB_SPI_ClearRxFifo(this->spi->base); */
/*	Cy_SCB_SPI_ClearTxFifo(this->spi->base); */

    /* Ensure this task does not already have a notification pending by calling
     * ulTaskNotifyTake() with the xClearCountOnExit parameter set to pdTRUE, and a block time of 0
     * (don't block). */
    BaseType_t rc = ulTaskNotifyTake( pdTRUE, 0 );
    configASSERT( !rc );

    /* Unmasking only the spi done interrupt bit */
    this->spi->base->INTR_M_MASK = SCB_INTR_M_SPI_DONE_Msk;

    uint32_t n = Cy_SCB_SPI_Write( this->spi->base, value );
    configASSERT( 1 == n );

    /* Timeout 1 sec */
    uint32_t timeOut = 1000UL;

    /* Wait until master completes transfer or time out has occured. */
    /* uint32_t ulTaskNotifyTake( BaseType_t xClearCountOnExit, TickType_t xTicksToWait ); */
    rc = ulTaskNotifyTake( pdFALSE, pdMS_TO_TICKS( timeOut ) ); /* Wait for notification from ISR */

    if( !rc )
    {
        /* This indicates that xTaskNotifyWait() returned without the calling task receiving a task notification. */
        /* The calling task will have been held in the Blocked state to wait for its notification state to become pending, */
        /* but the specified block time expired before that happened. */
        DBG_PRINTF( "Task %s timed out in %s\n",
                    pcTaskGetName( xTaskGetCurrentTaskHandle() ), __FUNCTION__ );

        sd_spi_release( this );
        return false;
    }

    /* There should be no more notifications pending: */
    configASSERT( !ulTaskNotifyTake( pdTRUE, 0 ) );
    configASSERT( !Cy_SCB_SPI_IsBusBusy( this->spi->base ) );
    configASSERT( Cy_SCB_SPI_IsTxComplete( this->spi->base ) );
    configASSERT( 1 == Cy_SCB_SPI_GetNumInRxFifo( this->spi->base ) );

    n = Cy_SCB_SPI_Read( this->spi->base );
    configASSERT( CY_SCB_SPI_RX_NO_DATA != n );
    received = n;

    configASSERT( 0 == Cy_SCB_SPI_GetNumInTxFifo( this->spi->base ) );
    configASSERT( 0 == Cy_SCB_SPI_GetNumInRxFifo( this->spi->base ) );

    sd_spi_release( this );
    return received;
}

/* [] END OF FILE */
