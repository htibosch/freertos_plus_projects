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

/*
 *
 * This file should be tailored to match the hardware design that is specified in
 * TopDesign.cysch and Design Wide Resources/Pins (the .cydwr file).
 *
 * There should be one element of the spi[] array for each hardware SPI, and the
 * name prefix (e.g.: "SPI_1") will depend on the name given in TopDesign.
 * Each SPI needs its own interrupt callback function that specifies the correct
 * element in the spi[] array. E.g.: spi[0].
 *
 * There should be one element of the sd_cards[] array for each SD card slot.
 * (There is no corresponding component in TopDesign.)
 * The name is arbitrary. The rest of the constants will depend on the type of socket,
 * which SPI it is driven by, and how it is wired.
 *
 */

#include <string.h>

/*/ * FreeRTOS includes. * / */
/*#include "FreeRTOS.h" */
/*#include "FreeRTOSFATConfig.h" */

/* Make it easier to spot errors: */
#include "project.h"
#include "hw_config.h"

/* SPI Interrupt callbacks */
void SPI_1_ISR();

/* SPI DMA Interrupts */
void SPI_1_RxDmaComplete();
void SPI_1_TxDmaComplete();

/* Card Detect Port Interrupt Service Routines */
void SDCard_detect_ISR();

/* Hardware Configuration of SPI "objects" */
/* Note: multiple SD cards can be driven by one SPI if they use different slave selects. */
static spi_t spi[] =                            /* One for each SPI. */
{     {
          .base = SPI_1_HW,                     /* SPI component */
          .context = &SPI_1_context,            /* SPI component */
          .config = &SPI_1_config,              /* SPI component */
          .dividerType = SPI_1_SCBCLK_DIV_TYPE, /* Clock divider for setting frequency */
          .dividerNum = SPI_1_SCBCLK_DIV_NUM,   /* Clock divider for setting frequency */
          .spi_miso_port = SPI_1_miso_m_0_PORT, /* Refer to Design Wide Resouces, Pins, in the .cydwr file */
          .spi_miso_num = SPI_1_miso_m_0_NUM,   /* MISO GPIO pin number for pull-up (see cyfitter_gpio.h) */
          .intcfg = &SPI_1_SCB_IRQ_cfg,         /* Interrupt */
          .userIsr = &SPI_1_ISR,                /* Interrupt */
          .rxDma_base = rxDma_1_HW,
          .rxDma_channel = rxDma_1_DW_CHANNEL,
          .rxDma_Descriptor_1 = &rxDma_1_Descriptor_1,
          .rxDma_Descriptor_1_config = &rxDma_1_Descriptor_1_config,
          .rxDma_channelConfig =
          {
              &rxDma_1_Descriptor_1,
              rxDma_1_PREEMPTABLE,
              rxDma_1_PRIORITY,
              .enable = false,
              rxDma_1_BUFFERABLE
          },
          .intRxDma_ISR = SPI_1_RxDmaComplete,
          .intRxDma_cfg = &intRxDma_1_cfg,
          .txDma_base = txDma_1_HW,
          .txDma_channel = txDma_1_DW_CHANNEL,
          .txDma_Descriptor_1 = &txDma_1_Descriptor_1,
          .txDma_Descriptor_1_config = &txDma_1_Descriptor_1_config,
          .txDma_channelConfig =
          {
              &txDma_1_Descriptor_1,
              txDma_1_PREEMPTABLE,
              txDma_1_PRIORITY,
              .enable = false,
              txDma_1_BUFFERABLE
          },
          .intTxDma_ISR = SPI_1_TxDmaComplete,
          .intTxDma_cfg = &intTxDma_1_cfg,
          /* Following attributes are dynamically assigned */
          .initialized = false,                                                  /*initialized flag */
          .owner = 0,                                                            /* Owning task, assigned dynamically */
          .mutex = 0                                                             /* Guard semaphore, assigned dynamically */
      } };

/* Hardware Configuration of the SD Card "objects" */
static sd_card_t sd_cards[] =                    /* One for each SD card */
{ {
      .pcName = "SDCard",                        /* Name used to mount device */
      .spi = &spi[ 0 ],                          /* Pointer to the SPI driving this card */
      .ss = SPI_1_SPI_SLAVE_SELECT0,             /* The SPI slave select line for this SD card */
      .card_detect_gpio_port = Card_Detect_PORT, /* Card detect */
      .card_detect_gpio_num = Card_Detect_NUM,   /* Card detect */
      .card_detected_true = 0,                   /* truth (card is present) is 0 */
      .card_detect_int_cfg = &Card_Detect_Interrupt_cfg,
      .card_detect_ISR = SDCard_detect_ISR,
      /* Following attributes are dynamically assigned */
      .card_detect_task = 0,
      .m_Status = STA_NOINIT,
      .sectors = 0,
      .card_type = 0,
      .mutex = 0,
      .ff_disk_count = 0,
      .ff_disks = NULL
  } };

/* Interrupt to notify the user about occurrences of SPI Events. */
/* Each SPI has its own interrupt. */
/* Notifies task when a transfer is complete. */
void SPI_1_ISR()
{
    spi_ISR( &spi[ 0 ] );
}

void SPI_1_RxDmaComplete()
{
    spi_RxDmaComplete( &spi[ 0 ] );
}

void SPI_1_TxDmaComplete()
{
    spi_TxDmaComplete( &spi[ 0 ] );
}

void SDCard_detect_ISR()
{
    card_detect_ISR( &sd_cards[ 0 ], 0 );
}

/* ********************************************************************** */
sd_card_t * sd_get_by_name( const char * const name )
{
    size_t i;

    for( i = 0; i < sizeof( sd_cards ) / sizeof( sd_cards[ 0 ] ); ++i )
    {
        if( 0 == strcmp( sd_cards[ i ].pcName, name ) )
        {
            break;
        }
    }

    if( sizeof( sd_cards ) / sizeof( sd_cards[ 0 ] ) == i )
    {
        DBG_PRINTF( "FF_SDDiskInit: unknown name %s\n", name );
        return NULL;
    }

    return &sd_cards[ i ];
}

sd_card_t * sd_get_by_num( size_t num )
{
    if( num <= sizeof( sd_cards ) / sizeof( sd_cards[ 0 ] ) )
    {
        return &sd_cards[ num ];
    }
    else
    {
        return NULL;
    }
}


/* [] END OF FILE */
