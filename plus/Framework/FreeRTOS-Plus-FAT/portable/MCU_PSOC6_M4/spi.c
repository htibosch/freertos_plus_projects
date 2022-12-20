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

#include <string.h>

#include "project.h"
#include "spi.h"

#include "FreeRTOSFATConfig.h" /* for DBG_PRINTF */

void spi_ISR( spi_t * this )
{
    /* Mask the spi done interrupt bit */
    this->base->INTR_M_MASK &= ~SCB_INTR_M_SPI_DONE_Msk;

    /* Check the status of master */
    uint32_t masterStatus = Cy_SCB_SPI_GetSlaveMasterStatus( this->base );
    Cy_SCB_SPI_ClearSlaveMasterStatus( this->base, masterStatus );
    configASSERT( masterStatus == CY_SCB_SPI_MASTER_DONE );

    /* The xHigherPriorityTaskWoken parameter must be initialized to pdFALSE as
     * it will get set to pdTRUE inside the interrupt safe API function if a
     * context switch is required. */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* Send a notification directly to the task to which interrupt processing is
     * being deferred. */
    configASSERT( this->owner );
    vTaskNotifyGiveFromISR( this->owner, /* The handle of the task to which the notification is being sent. */
                            &xHigherPriorityTaskWoken );


    /* Pass the xHigherPriorityTaskWoken value into portYIELD_FROM_ISR(). If
     * xHigherPriorityTaskWoken was set to pdTRUE inside vTaskNotifyGiveFromISR()
     * then calling portYIELD_FROM_ISR() will request a context switch. If
     * xHigherPriorityTaskWoken is still pdFALSE then calling
     * portYIELD_FROM_ISR() will have no effect. */
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

/*******************************************************************************
* Function Name: RxDmaComplete
********************************************************************************
*
* Summary:
*  Handles Rx Dma descriptor completion interrupt source. Clears Interrupt and sets
*  the rxDmaCmplt flag.
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void spi_RxDmaComplete( spi_t * this )
{
    /* Check interrupt cause to capture errors. */
    if( ( CY_DMA_INTR_CAUSE_COMPLETION != Cy_DMA_Channel_GetStatus( this->rxDma_base, this->rxDma_channel ) ) &&
        ( CY_DMA_INTR_CAUSE_CURR_PTR_NULL != Cy_DMA_Channel_GetStatus( this->rxDma_base, this->rxDma_channel ) ) )
    {
        /* DMA error occured while RX operations */
        configASSERT( false );
    }

    /* Clear rx DMA interrupt. */
    Cy_DMA_Channel_ClearInterrupt( this->rxDma_base, this->rxDma_channel );

    /* The xHigherPriorityTaskWoken parameter must be initialized to pdFALSE as
     * it will get set to pdTRUE inside the interrupt safe API function if a
     * context switch is required. */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /* Send a notification directly to the task to which interrupt processing is
     * being deferred. */
    configASSERT( this->owner );
    vTaskNotifyGiveFromISR( this->owner, /* The handle of the task to which the notification is being sent. */
                            &xHigherPriorityTaskWoken );

    /* Pass the xHigherPriorityTaskWoken value into portYIELD_FROM_ISR(). If
     * xHigherPriorityTaskWoken was set to pdTRUE inside vTaskNotifyGiveFromISR()
     * then calling portYIELD_FROM_ISR() will request a context switch. If
     * xHigherPriorityTaskWoken is still pdFALSE then calling
     * portYIELD_FROM_ISR() will have no effect. */
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
}

/*******************************************************************************
* Function Name: TxDmaComplete
********************************************************************************
*
* Summary:
*  Handles Tx Dma descriptor completion interrupt source: only used for
*  indication.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
void spi_TxDmaComplete( spi_t * this )
{
    /* Check tx DMA status */
    if( ( CY_DMA_INTR_CAUSE_COMPLETION != Cy_DMA_Channel_GetStatus( this->txDma_base, this->txDma_channel ) ) &&
        ( CY_DMA_INTR_CAUSE_CURR_PTR_NULL != Cy_DMA_Channel_GetStatus( this->txDma_base, this->txDma_channel ) ) )
    {
        /* DMA error occured while TX operations */
        configASSERT( false );
    }

    /* Clear tx DMA interrupt */
    Cy_DMA_Channel_ClearInterrupt( this->txDma_base, this->txDma_channel );

    /* No notification here, since if RX is complete, we can assume TX must be. */
}

/*******************************************************************************
* Function Name: spi_rxDma_configure
********************************************************************************
*
* Summary:
* Configures DMA Rx channel for operation.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
static void spi_rxDma_configure( spi_t * this )
{
    cy_en_dma_status_t dma_init_status;

    /* Initialize descriptor */
    dma_init_status = Cy_DMA_Descriptor_Init( this->rxDma_Descriptor_1, this->rxDma_Descriptor_1_config );

    if( dma_init_status != CY_DMA_SUCCESS )
    {
        configASSERT( false );
    }

    /* Initialize the DMA channel */
    dma_init_status = Cy_DMA_Channel_Init( this->rxDma_base, this->rxDma_channel, &this->rxDma_channelConfig );

    if( dma_init_status != CY_DMA_SUCCESS )
    {
        configASSERT( false );
    }

    /* Set source for descriptor 1 */
    /* warning: passing argument 2 of 'Cy_DMA_Descriptor_SetSrcAddress' discards 'volatile' qualifier from pointer target type */
    Cy_DMA_Descriptor_SetSrcAddress( this->rxDma_Descriptor_1, ( void * ) &this->base->RX_FIFO_RD );

    /* Initialize and enable interrupt from RxDma */
    Cy_SysInt_Init( this->intRxDma_cfg, this->intRxDma_ISR );
    NVIC_EnableIRQ( this->intRxDma_cfg->intrSrc );
    __NVIC_ClearPendingIRQ( this->intRxDma_cfg->intrSrc );

    /* Enable DMA interrupt source. */
    Cy_DMA_Channel_SetInterruptMask( this->rxDma_base, this->rxDma_channel, CY_DMA_INTR_MASK );

    Cy_DMA_Enable( this->rxDma_base ); /* Does not enable channel */
}

/*******************************************************************************
* Function Name: spi_txDma_configure
********************************************************************************
*
* Summary:
* Configures DMA Tx channel for operation.
*
* Parameters:
*  None
*
* Return:
*  None
*
*******************************************************************************/
static void spi_txDma_configure( spi_t * this )
{
    cy_en_dma_status_t dma_init_status;

    /* Initialize descriptor */
    dma_init_status = Cy_DMA_Descriptor_Init( this->txDma_Descriptor_1, this->txDma_Descriptor_1_config );

    if( dma_init_status != CY_DMA_SUCCESS )
    {
        configASSERT( false );
    }

    /* Initialize the DMA channel */
    dma_init_status = Cy_DMA_Channel_Init( this->txDma_base, this->txDma_channel, &this->txDma_channelConfig );

    if( dma_init_status != CY_DMA_SUCCESS )
    {
        configASSERT( false );
    }

    /* Set destination for descriptor 1 */
    /* warning: passing argument 2 of 'Cy_DMA_Descriptor_SetDstAddress' discards 'volatile' qualifier from pointer target type */
    Cy_DMA_Descriptor_SetDstAddress( this->txDma_Descriptor_1, ( void * ) &this->base->TX_FIFO_WR );

    /* Initialize and enable the interrupt from TxDma */
    Cy_SysInt_Init( this->intTxDma_cfg, this->intTxDma_ISR );
    NVIC_EnableIRQ( this->intTxDma_cfg->intrSrc );
    __NVIC_ClearPendingIRQ( this->intTxDma_cfg->intrSrc );

    /* Enable DMA interrupt source. */
    Cy_DMA_Channel_SetInterruptMask( this->txDma_base, this->txDma_channel, CY_DMA_INTR_MASK );

    Cy_DMA_Enable( this->txDma_base ); /* Does not enable channel */
}

bool spi_init( spi_t * this )
{
    cy_en_scb_spi_status_t initStatus;
    cy_en_sysint_status_t sysSpistatus;

    /* bool __atomic_test_and_set (void *ptr, int memorder) */
    /* This built-in function performs an atomic test-and-set operation on the byte at *ptr. */
    /* The byte is set to some implementation defined nonzero “set” value */
    /* and the return value is true if and only if the previous contents were “set”. */
    if( __atomic_test_and_set( &( this->initialized ), __ATOMIC_SEQ_CST ) )
    {
        return true;
    }

    spi_rxDma_configure( this );
    spi_txDma_configure( this );

    /* Configure component */
    initStatus = Cy_SCB_SPI_Init( this->base, this->config, this->context );

    if( initStatus != CY_SCB_SPI_SUCCESS )
    {
        DBG_PRINTF( "FAILED_OPERATION: Cy_SCB_SPI_Init\n" );
        return false;
    }

    /* Hook interrupt service routine */
    sysSpistatus = Cy_SysInt_Init( this->intcfg, this->userIsr );

    if( sysSpistatus != CY_SYSINT_SUCCESS )
    {
        DBG_PRINTF( "FAILED_OPERATION: Cy_SysInt_Init\n" );
        return false;
    }

    /* Enable interrupt in NVIC */
    NVIC_EnableIRQ( this->intcfg->intrSrc );
    /* Clear any pending interrupts */
    __NVIC_ClearPendingIRQ( this->intcfg->intrSrc );

    /* SD cards' DO MUST be pulled up. */
    Cy_GPIO_SetDrivemode( this->spi_miso_port, this->spi_miso_num, CY_GPIO_DM_PULLUP );

    /* Do this here if not done in sd_spi_acquire: */
/*	Cy_SCB_SPI_SetActiveSlaveSelect(this->base, this->ss); */

/*	/ * Enable SPI master hardware. * / */
    Cy_SCB_SPI_Enable( this->base );

    /* The SPI may be shared (using multiple SSs); protect it */
    this->mutex = xSemaphoreCreateRecursiveMutex();

    return true;
}

void spi_dma_transfer( spi_t * this,
                       uint16 len,
                       const uint8_t * tx,
                       uint8_t * rx )
{
    /* Cannot be a stack variable, since the function can return before the DMA channel runs: */
    static uint8_t dummy = SPI_FILL_CHAR;

    configASSERT( 512 == len );
    configASSERT( tx || rx );
    configASSERT( CY_DMA_CHANNEL_DISABLED == Cy_DMA_Descriptor_GetChannelState( this->txDma_Descriptor_1 ) );
    configASSERT( CY_DMA_CHANNEL_DISABLED == Cy_DMA_Descriptor_GetChannelState( this->rxDma_Descriptor_1 ) );

    /* Transfer 512 bytes using 256 X loops and 2 Y loops.
     * If tx or rx is NULL, point it to a single byte and don't increment. */

    int32_t txSrcXincrement, txSrcYincrement;

    if( !tx )
    {
        tx = &dummy;
        txSrcXincrement = 0;
        txSrcYincrement = 0;
    }
    else
    {
        txSrcXincrement = 1;
        txSrcYincrement = 256;
    }

    Cy_DMA_Descriptor_SetXloopSrcIncrement( this->txDma_Descriptor_1, txSrcXincrement );
    Cy_DMA_Descriptor_SetYloopSrcIncrement( this->txDma_Descriptor_1, txSrcYincrement );

    int32_t rxDstXincrement, rxDstYincrement;

    if( !rx )
    {
        rx = &dummy;
        rxDstXincrement = 0;
        rxDstYincrement = 0;
    }
    else
    {
        rxDstXincrement = 1;
        rxDstYincrement = 256;
    }

    Cy_DMA_Descriptor_SetXloopDstIncrement( this->rxDma_Descriptor_1, rxDstXincrement );
    Cy_DMA_Descriptor_SetYloopDstIncrement( this->rxDma_Descriptor_1, rxDstYincrement );

    /*Set up SRC addr for TX DMA*/
    Cy_DMA_Descriptor_SetSrcAddress( this->txDma_Descriptor_1, tx );
    /*Set up DEST addr for RX DMA*/
    Cy_DMA_Descriptor_SetDstAddress( this->rxDma_Descriptor_1, rx );

    /*Enable DMA_channels*/
    Cy_DMA_Channel_Enable( this->rxDma_base, this->rxDma_channel );
    Cy_DMA_Channel_Enable( this->txDma_base, this->txDma_channel );
}

/* [] END OF FILE */
