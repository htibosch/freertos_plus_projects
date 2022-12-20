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

#ifndef _SD_CARD_H_
#define _SD_CARD_H_

#include <stdint.h>

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "ff_headers.h"
#include <semphr.h>

#include "project.h"
#include "spi.h"

/* "Class" representing SD Cards */
typedef struct
{
    const char * pcName;
    spi_t * const spi;
    /* Slave select is here in sd_card_t because multiple SDs can share an SPI */
    const cy_en_scb_spi_slave_select_t ss;       /* Slave select for this SD card */
    GPIO_PRT_Type * const card_detect_gpio_port; /* Card detect */
    const uint32_t card_detect_gpio_num;         /* Card detect */
    const uint32_t card_detected_true;           /* 0 for full-size SD, 1 for micro */
    const cy_stc_sysint_t * card_detect_int_cfg; /* Port Interrupt configuration structure */
    const cy_israddress card_detect_ISR;         /*Address of the ISR for the port interrupt; */
    /* can be NULL if ISR is shared over multiple cards */
    TaskHandle_t card_detect_task;               /* handles card detect ISRs */
    int m_Status;                                /* Card status */
    uint64_t sectors;                            /* Assigned dynamically */
    int card_type;                               /* Assigned dynamically */
    SemaphoreHandle_t mutex;                     /* Guard semaphore, assigned dynamically */
    size_t ff_disk_count;
    FF_Disk_t ** ff_disks;                       /* FreeRTOS+FAT "disks" using this device */
} sd_card_t;

#define SD_BLOCK_DEVICE_ERROR_NONE               0
#define SD_BLOCK_DEVICE_ERROR_WOULD_BLOCK        -5001  /*!< operation would block */
#define SD_BLOCK_DEVICE_ERROR_UNSUPPORTED        -5002  /*!< unsupported operation */
#define SD_BLOCK_DEVICE_ERROR_PARAMETER          -5003  /*!< invalid parameter */
#define SD_BLOCK_DEVICE_ERROR_NO_INIT            -5004  /*!< uninitialized */
#define SD_BLOCK_DEVICE_ERROR_NO_DEVICE          -5005  /*!< device is missing or not connected */
#define SD_BLOCK_DEVICE_ERROR_WRITE_PROTECTED    -5006  /*!< write protected */
#define SD_BLOCK_DEVICE_ERROR_UNUSABLE           -5007  /*!< unusable card */
#define SD_BLOCK_DEVICE_ERROR_NO_RESPONSE        -5008  /*!< No response from device */
#define SD_BLOCK_DEVICE_ERROR_CRC                -5009  /*!< CRC error */
#define SD_BLOCK_DEVICE_ERROR_ERASE              -5010  /*!< Erase error: reset/sequence */
#define SD_BLOCK_DEVICE_ERROR_WRITE              -5011  /*!< SPI Write error: !SPI_DATA_ACCEPTED */

/* Disk Status Bits (DSTATUS) */
enum
{
    STA_NOINIT = 0x01, /* Drive not initialized */
    STA_NODISK = 0x02, /* No medium in the drive */
    STA_PROTECT = 0x04 /* Write protected */
};

int sd_init( sd_card_t * this );
int sd_deinit( sd_card_t * this );
int sd_write_blocks( sd_card_t * this, const uint8_t * buffer, uint64_t ulSectorNumber, uint32_t blockCnt );
int sd_read_blocks( sd_card_t * this, uint8_t * buffer, uint64_t ulSectorNumber, uint32_t ulSectorCount );
bool sd_card_detect( sd_card_t * this );
uint64_t sd_sectors( sd_card_t * this );

void card_detect_ISR( sd_card_t * this, size_t card_num );

#endif /* ifndef _SD_CARD_H_ */
/* [] END OF FILE */
