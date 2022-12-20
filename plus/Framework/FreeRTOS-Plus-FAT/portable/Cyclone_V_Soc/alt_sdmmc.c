/******************************************************************************
*
* Copyright 2013 Altera Corporation. All Rights Reserved.
* 
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
* 
* 1. Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* 
* 2. Redistributions in binary form must reproduce the above copyright notice,
* this list of conditions and the following disclaimer in the documentation
* and/or other materials provided with the distribution.
* 
* 3. Neither the name of the copyright holder nor the names of its contributors
* may be used to endorse or promote products derived from this software without
* specific prior written permission.
* 
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
* 
******************************************************************************/

/*
 * $Id: //acds/rel/20.1/embedded/ip/hps/altera_hps/hwlib/src/hwmgr/alt_sdmmc.c#1 $
 */

#include "alt_sdmmc.h"
#include "alt_clock_manager.h"
#include "alt_reset_manager.h"
#include "alt_cache.h"
#include "hwlib.h"
#include "socal/alt_sdmmc.h"
#include "socal/alt_rstmgr.h"
#include "socal/alt_sysmgr.h"
#include "socal/hps.h"
#include "socal/socal.h"
#include <stdio.h>
#warning Need to include?
//#include "alt_printf.h"

#if defined (soc_a10)
#include "socal/alt_ecc_sdmmc.h"
#endif

#ifdef DEBUG_ALT_SDMMC
  #define dprintf printf
#else
	extern int lUDPLoggingPrintf( const char *pcFormatString, ... );
	extern void serial_printf(const char *pcFormat, ...);
	#define dprintf  serial_printf
	#define LOGGER   1

	static int ALT_MIN( uint32_t ulLeft, uint32_t ulRight )
	{
		uint32_t ulReturn;
		if( ulLeft >= ulRight )
		{
			ulReturn = ulLeft;
		}
		else
		{
			ulReturn = ulRight;
		}
		return ulReturn;
	}
#endif

#define ARRAY_COUNT(array) (sizeof(array) / sizeof(array[0]))


/*  Timeout for reset manager*/
#define  ALT_SDMMC_RESET_TMO_INIT      8192
/*  Timeout for disable device*/
#define  ALT_SDMMC_MAX_T_POLL_COUNT    8192
/*  Timeout for waiting event*/
#define  ALT_SDMMC_TMO_WAITER          1000000

#define ALT_SDMMC_DMA_SEGMENT_SIZE      512
#define ALT_SDMMC_DMA_DESC_COUNT        128

#define ALT_SDMMC_FSM_IDLE              0
#define ALT_SDMMC_DMA_FSM_IDLE          0

#define ALT_SDMMC_CSD_CSD_STRUCTURE_MASK        0xC0000000
#define ALT_SDMMC_CSD_CSD_STRUCTURE_GET(val)    ((val & ALT_SDMMC_CSD_CSD_STRUCTURE_MASK) >> 30)
#define ALT_SDMMC_CSD_CSD_STUCTURE_VERSION_1    0
#define ALT_SDMMC_CSD_CSD_STUCTURE_VERSION_2    1

#define ALT_SDMMC_CSD_V1_C_SIZE_MULT_MSK           0x00038000
#define ALT_SDMMC_CSD_V1_C_SIZE_LOW_MSK            0xC0000000
#define ALT_SDMMC_CSD_V1_C_SIZE_HIGH_MSK           0x000003FF
#define ALT_SDMMC_CSD_V1_C_SIZE_MULT_GET(val)      ((val & ALT_SDMMC_CSD_V1_C_SIZE_MULT_MSK) >> 15)
#define ALT_SDMMC_CSD_V1_C_SIZE_LOW_GET(val)       ((val & ALT_SDMMC_CSD_V1_C_SIZE_LOW_MSK) >> 30)
#define ALT_SDMMC_CSD_V1_C_SIZE_HIGH_GET(val)      ((val & ALT_SDMMC_CSD_V1_C_SIZE_HIGH_MSK) >> 0)

#define ALT_SDMMC_CSD_V2_C_SIZE_LOW_MSK            0xFFFF0000
#define ALT_SDMMC_CSD_V2_C_SIZE_HIGH_MSK           0x0000003F
#define ALT_SDMMC_CSD_V2_C_SIZE_LOW_GET(val)       ((val & ALT_SDMMC_CSD_V2_C_SIZE_LOW_MSK) >> 16)
#define ALT_SDMMC_CSD_V2_C_SIZE_HIGH_GET(val)      ((val & ALT_SDMMC_CSD_V2_C_SIZE_HIGH_MSK) >> 0)

#define CCC_CLASS_10 (1<<10)

#define ALT_SDMMC_CSD_MAX_R_BLK_MSK             0x000F0000
#define ALT_SDMMC_CSD_MAX_W_BLK_MSK             0x03C00000
#define ALT_SDMMC_CSD_PART_R_ALLOW_MSK          0x00008000
#define ALT_SDMMC_CSD_PART_W_ALLOW_MSK          0x00200000
#define ALT_SDMMC_CSD_SPEED_RATE_MSK            0x00000007
#define ALT_SDMMC_CSD_SPEED_TIME_MSK            0x00000078
#define ALT_SDMMC_CSD_CCC_MSK                   0xFFF00000
#define ALT_SDMMC_CSD_MAX_R_BLK_GET(val)        ((val & ALT_SDMMC_CSD_MAX_R_BLK_MSK) >> 16)
#define ALT_SDMMC_CSD_MAX_W_BLK_GET(val)        ((val & ALT_SDMMC_CSD_MAX_W_BLK_MSK) >> 22)
#define ALT_SDMMC_CSD_PART_R_ALLOW_GET(val)     ((val & ALT_SDMMC_CSD_PART_R_ALLOW_MSK) >> 15)
#define ALT_SDMMC_CSD_PART_W_ALLOW_GET(val)     ((val & ALT_SDMMC_CSD_PART_W_ALLOW_MSK) >> 21)
#define ALT_SDMMC_CSD_SPEED_RATE_GET(val)       ((val & ALT_SDMMC_CSD_SPEED_RATE_MSK) >> 0)
#define ALT_SDMMC_CSD_SPEED_TIME_GET(val)       ((val & ALT_SDMMC_CSD_SPEED_TIME_MSK) >> 3)
#define ALT_SDMMC_CSD_CCC_GET(val)              ((val & ALT_SDMMC_CSD_CCC_MSK) >> 20)

#define ALT_SDMMC_SWITCH_FUNC_MODE_CHECK  (0UL << 31)
#define ALT_SDMMC_SWITCH_FUNC_MODE_SWITCH (1UL << 31)
#define ALT_SDMMC_SWITCH_FUNC_HIGH_SPEED  (1UL << 0)
#define ALT_SDMMC_SWITCH_FUNC_DEFAULT_SPEED  (0UL << 0)
#define ALT_SDMMC_SWITCH_FUNC_G1_KEEP     (0xfUL << 0)
#define ALT_SDMMC_SWITCH_FUNC_G2_KEEP     (0xfUL << 4)
#define ALT_SDMMC_SWITCH_FUNC_G3_KEEP     (0xfUL << 8)
#define ALT_SDMMC_SWITCH_FUNC_G4_KEEP     (0xfUL << 12)
#define ALT_SDMMC_SWITCH_FUNC_G5_KEEP     (0xfUL << 16)
#define ALT_SDMMC_SWITCH_FUNC_G6_KEEP     (0xfUL << 20)

#define ALT_SDMMC_HIGH_SPEED_BUS_SPEED    1
#define ALT_SDMMC_UHS_SDR25_BUS_SPEED     1
#define ALT_SDMMC_UHS_SDR50_BUS_SPEED     2
#define ALT_SDMMC_UHS_SDR104_BUS_SPEED    3
#define ALT_SDMMC_UHS_DDR50_BUS_SPEED     4

#define ALT_SDMMC_SD_MODE_HIGH_SPEED      (1 << ALT_SDMMC_HIGH_SPEED_BUS_SPEED)
#define ALT_SDMMC_SD_MODE_UHS_SDR12       (1 << ALT_SDMMC_UHS_SDR12_BUS_SPEED)
#define ALT_SDMMC_SD_MODE_UHS_SDR25       (1 << ALT_SDMMC_UHS_SDR25_BUS_SPEED)
#define ALT_SDMMC_SD_MODE_UHS_SDR50       (1 << ALT_SDMMC_UHS_SDR50_BUS_SPEED)
#define ALT_SDMMC_SD_MODE_UHS_SDR104      (1 << ALT_SDMMC_UHS_SDR104_BUS_SPEED)
#define ALT_SDMMC_SD_MODE_UHS_DDR50       (1 << ALT_SDMMC_UHS_DDR50_BUS_SPEED)

typedef enum ALT_SDMMC_TMOD_e
{
    ALT_SDMMC_TMOD_READ          = 0,
    ALT_SDMMC_TMOD_WRITE         = 1
} ALT_SDMMC_TMOD_t;

#ifdef LOGGER
uint32_t log_buffer[1000] = { 0 };
uint32_t log_index = 0;

#define D_ADD_VALUE(value)({if (log_index < 1000) log_buffer[log_index++] = value;})
#endif

static alt_freq_t clock_freq;
static ALT_STATUS_CODE alt_sdmmc_card_scr_get(uint64_t* scr_reg);
static ALT_STATUS_CODE alt_sdmmc_card_switch_function(uint32_t function_arg, uint8_t * switch_status);
static __inline ALT_STATUS_CODE alt_sdmmc_read_short_response(uint32_t *response);
static ALT_STATUS_CODE alt_sdmmc_card_read_switch(ALT_SDMMC_CARD_INFO_t * card_info);

#define    ALT_DEFAULT_CMD_INDEX                         0
#define    ALT_DEFAULT_RESPONSE_EXPECT                   false
#define    ALT_DEFAULT_RESPONSE_LENGTH_LONG              false
#define    ALT_DEFAULT_CHECK_RESPONSE_CRC                false
#define    ALT_DEFAULT_DATA_EXPECTED                     0
#define    ALT_DEFAULT_WRITE_ACTIVE                      false
#define    ALT_DEFAULT_STREAM_MODE_ACTIVE                false
#define    ALT_DEFAULT_SEND_AUTO_STOP                    false
#define    ALT_DEFAULT_WAIT_PRVDATA_COMPLETE             false
#define    ALT_DEFAULT_STOP_ABORT_CMD                    false
#define    ALT_DEFAULT_SEND_INITIALIZATION               false
#define    ALT_DEFAULT_CARD_NUMBER                       0
#define    ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY       false
#define    ALT_DEFAULT_READ_CEATA_DEVICE                 false
#define    ALT_DEFAULT_CSS_EXPECTED                      false
#define    ALT_DEFAULT_ENABLE_BOOT                       false
#define    ALT_DEFAULT_EXPECT_BOOT_ACK                   false
#define    ALT_DEFAULT_DISABLE_BOOT                      false
#define    ALT_DEFAULT_BOOT_MODE                         false
#define    ALT_DEFAULT_VOLT_SWITCH                       false
#define    ALT_DEFAULT_USE_HOLD_REG                      false
#define    ALT_DEFAULT_RESERVED                          0
#define    ALT_DEFAULT_START_BIT                         0

/*  Default configurations of used commands*/
static ALT_SDMMC_CMD_CONFIG_t cmd_default_cfg[] =
{
    {
        ALT_SDMMC_WRITE_MULTIPLE_BLOCK, /* .cmd_index */
        true, /* .response_expect */
        ALT_DEFAULT_RESPONSE_LENGTH_LONG,
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        1, /* .data_expected  */
        true, /* .write_active */
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        true, /* .send_auto_stop */
        true, /* .wait_prvdata_complete */
        ALT_DEFAULT_STOP_ABORT_CMD,
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    },

    {
        ALT_SDMMC_READ_MULTIPLE_BLOCK, /* .cmd_index */
        true, /* .response_expect */
        ALT_DEFAULT_RESPONSE_LENGTH_LONG,
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        1, /* .data_expected */
        ALT_DEFAULT_WRITE_ACTIVE,
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        true, /* .send_auto_stop */
        true, /* .wait_prvdata_complete */
        ALT_DEFAULT_STOP_ABORT_CMD,
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    },
    {
        ALT_SDMMC_WRITE_BLOCK, /* .cmd_index */
        true, /* .response_expect */
        ALT_DEFAULT_RESPONSE_LENGTH_LONG,
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        true, /* .data_expected */
        true, /* .write_active */
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        ALT_DEFAULT_SEND_AUTO_STOP,
        true, /* .wait_prvdata_complete */
        ALT_DEFAULT_STOP_ABORT_CMD,
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    },
    {
        ALT_SDMMC_READ_SINGLE_BLOCK, /* .cmd_index */
        true, /* .response_expect */
        ALT_DEFAULT_RESPONSE_LENGTH_LONG,
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        true, /* .data_expected */
        ALT_DEFAULT_WRITE_ACTIVE,
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        ALT_DEFAULT_SEND_AUTO_STOP,
        true, /* .wait_prvdata_complete */
        ALT_DEFAULT_STOP_ABORT_CMD,
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    },
    {
        ALT_SDMMC_SET_BLOCKLEN, /* .cmd_index */
        true, /* .response_expect */
        ALT_DEFAULT_RESPONSE_LENGTH_LONG,
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        ALT_DEFAULT_DATA_EXPECTED,
        ALT_DEFAULT_WRITE_ACTIVE,
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        ALT_DEFAULT_SEND_AUTO_STOP,
        true, /* .wait_prvdata_complete */
        ALT_DEFAULT_STOP_ABORT_CMD,
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    },
    {
        ALT_SDMMC_SEL_DES_CARD, /* .cmd_index */
        true, /* .response_expect */
        ALT_DEFAULT_RESPONSE_LENGTH_LONG,
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        ALT_DEFAULT_DATA_EXPECTED,
        ALT_DEFAULT_WRITE_ACTIVE,
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        ALT_DEFAULT_SEND_AUTO_STOP,
        true, /* .wait_prvdata_complete */
        ALT_DEFAULT_STOP_ABORT_CMD,
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    },
    {
        ALT_SDMMC_APP_CMD, /* .cmd_index */
        true, /* .response_expect */
        ALT_DEFAULT_RESPONSE_LENGTH_LONG,
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        ALT_DEFAULT_DATA_EXPECTED,
        ALT_DEFAULT_WRITE_ACTIVE,
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        ALT_DEFAULT_SEND_AUTO_STOP,
        true, /* .wait_prvdata_complete */
        ALT_DEFAULT_STOP_ABORT_CMD,
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    },
    {
        ALT_SDMMC_IF_COND, /* .cmd_index */
        true, /* .response_expect */
        ALT_DEFAULT_RESPONSE_LENGTH_LONG,
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        ALT_DEFAULT_DATA_EXPECTED,
        ALT_DEFAULT_WRITE_ACTIVE,
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        ALT_DEFAULT_SEND_AUTO_STOP,
        true, /* .wait_prvdata_complete */
        ALT_DEFAULT_STOP_ABORT_CMD,
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    },
    {
        ALT_SDMMC_SET_RELATIVE_ADDR, /* .cmd_index */
        true, /* .response_expect */
        ALT_DEFAULT_RESPONSE_LENGTH_LONG,
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        ALT_DEFAULT_DATA_EXPECTED,
        ALT_DEFAULT_WRITE_ACTIVE,
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        ALT_DEFAULT_SEND_AUTO_STOP,
        true, /* .wait_prvdata_complete */
        ALT_DEFAULT_STOP_ABORT_CMD,
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    },
    {
        ALT_SDMMC_SEND_STATUS, /* .cmd_index */
        true, /* .response_expect */
        ALT_DEFAULT_RESPONSE_LENGTH_LONG,
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        ALT_DEFAULT_DATA_EXPECTED,
        ALT_DEFAULT_WRITE_ACTIVE,
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        ALT_DEFAULT_SEND_AUTO_STOP,
        ALT_DEFAULT_WAIT_PRVDATA_COMPLETE,
        ALT_DEFAULT_STOP_ABORT_CMD,
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    },
    {
        ALT_SDMMC_ALL_SEND_CID, /* .cmd_index */
        true, /* .response_expect */
        true, /* .response_length_long */
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        ALT_DEFAULT_DATA_EXPECTED,
        ALT_DEFAULT_WRITE_ACTIVE,
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        ALT_DEFAULT_SEND_AUTO_STOP,
        true, /* .wait_prvdata_complete */
        ALT_DEFAULT_STOP_ABORT_CMD,
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    },
    {
        ALT_SDMMC_SEND_CID, /* .cmd_index */
        true, /* .response_expect */
        true, /* .response_length_long */
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        ALT_DEFAULT_DATA_EXPECTED,
        ALT_DEFAULT_WRITE_ACTIVE,
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        ALT_DEFAULT_SEND_AUTO_STOP,
        true, /* .wait_prvdata_complete */
        ALT_DEFAULT_STOP_ABORT_CMD,
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    },
    {
        ALT_SDMMC_SEND_CSD, /* .cmd_index */
        true, /* .response_expect */
        true, /* .response_length_long */
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        ALT_DEFAULT_DATA_EXPECTED,
        ALT_DEFAULT_WRITE_ACTIVE,
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        ALT_DEFAULT_SEND_AUTO_STOP,
        true, /* .wait_prvdata_complete */
        ALT_DEFAULT_STOP_ABORT_CMD,
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    },
    {
        ALT_SDMMC_STOP_TRANSMISSION, /* .cmd_index */
        ALT_DEFAULT_RESPONSE_EXPECT,
        ALT_DEFAULT_RESPONSE_LENGTH_LONG,
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        ALT_DEFAULT_DATA_EXPECTED,
        ALT_DEFAULT_WRITE_ACTIVE,
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        ALT_DEFAULT_SEND_AUTO_STOP,
        true, /* .wait_prvdata_complete */
        true, /* .stop_abort_cmd */
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    },
    {
        ALT_SDMMC_GO_IDLE_STATE, /* .cmd_index */
        ALT_DEFAULT_RESPONSE_EXPECT,
        ALT_DEFAULT_RESPONSE_LENGTH_LONG,
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        ALT_DEFAULT_DATA_EXPECTED,
        ALT_DEFAULT_WRITE_ACTIVE,
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        ALT_DEFAULT_SEND_AUTO_STOP,
        true, /* .wait_prvdata_complete */
        ALT_DEFAULT_STOP_ABORT_CMD,
        true, /* .send_initialization */
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    },
    {
        ALT_SDMMC_SWITCH, /* .cmd_index */
        true, /* .response_expect */
        ALT_DEFAULT_RESPONSE_LENGTH_LONG,
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        true, /* .data_expected */
        ALT_DEFAULT_WRITE_ACTIVE,
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        ALT_DEFAULT_SEND_AUTO_STOP,
        true, /* .wait_prvdata_complete */
        ALT_DEFAULT_STOP_ABORT_CMD,
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    }
};

/*  Default configurations of used ACMDs*/
static ALT_SDMMC_CMD_CONFIG_t acmd_default_cfg[] =
{
    {
        ALT_SD_SEND_SCR, /* .cmd_index */
        true, /* .response_expect */
        ALT_DEFAULT_RESPONSE_LENGTH_LONG,
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        true, /* .data_expected */
        ALT_DEFAULT_WRITE_ACTIVE,
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        ALT_DEFAULT_SEND_AUTO_STOP,
        ALT_DEFAULT_WAIT_PRVDATA_COMPLETE,
        ALT_DEFAULT_STOP_ABORT_CMD,
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    },
    {
        ALT_SD_SEND_OP_COND, /* .cmd_index */
        true, /* .response_expect */
        ALT_DEFAULT_RESPONSE_LENGTH_LONG,
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        ALT_DEFAULT_DATA_EXPECTED,
        ALT_DEFAULT_WRITE_ACTIVE,
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        ALT_DEFAULT_SEND_AUTO_STOP,
        true, /* .wait_prvdata_complete */
        ALT_DEFAULT_STOP_ABORT_CMD,
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    },
    {
        ALT_SD_SET_BUS_WIDTH, /* .cmd_index */
        true, /* .response_expect */
        ALT_DEFAULT_RESPONSE_LENGTH_LONG,
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        ALT_DEFAULT_DATA_EXPECTED,
        ALT_DEFAULT_WRITE_ACTIVE,
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        ALT_DEFAULT_SEND_AUTO_STOP,
        true, /* .wait_prvdata_complete */
        ALT_DEFAULT_STOP_ABORT_CMD,
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    },
    {
        ALT_SD_SD_STATUS, /* .cmd_index */
        true, /* .response_expect */
        ALT_DEFAULT_RESPONSE_LENGTH_LONG,
        ALT_DEFAULT_CHECK_RESPONSE_CRC,
        true, /* .data_expected */
        ALT_DEFAULT_WRITE_ACTIVE,
        ALT_DEFAULT_STREAM_MODE_ACTIVE,
        ALT_DEFAULT_SEND_AUTO_STOP,
        ALT_DEFAULT_WAIT_PRVDATA_COMPLETE,
        ALT_DEFAULT_STOP_ABORT_CMD,
        ALT_DEFAULT_SEND_INITIALIZATION,
        0, /* .card_number */
        ALT_DEFAULT_UPDATE_CLOCK_REGISTERS_ONLY,
        ALT_DEFAULT_READ_CEATA_DEVICE,
        ALT_DEFAULT_CSS_EXPECTED,
        ALT_DEFAULT_ENABLE_BOOT,
        ALT_DEFAULT_EXPECT_BOOT_ACK,
        ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
        ALT_DEFAULT_VOLT_SWITCH,
        true, /* .use_hold_reg */
        ALT_DEFAULT_RESERVED,
        ALT_DEFAULT_START_BIT
    }
};
static ALT_SDMMC_CMD_CONFIG_t cmd_clock_cfg = 
{
    ALT_DEFAULT_CMD_INDEX,
    ALT_DEFAULT_RESPONSE_EXPECT,
    ALT_DEFAULT_RESPONSE_LENGTH_LONG,
    ALT_DEFAULT_CHECK_RESPONSE_CRC,
    ALT_DEFAULT_DATA_EXPECTED,
    ALT_DEFAULT_WRITE_ACTIVE,
    ALT_DEFAULT_STREAM_MODE_ACTIVE,
    ALT_DEFAULT_SEND_AUTO_STOP,
    true, /* .wait_prvdata_complete */
    ALT_DEFAULT_STOP_ABORT_CMD,
    ALT_DEFAULT_SEND_INITIALIZATION,
    ALT_DEFAULT_CARD_NUMBER,
    true, /* .update_clock_registers_only */
    ALT_DEFAULT_READ_CEATA_DEVICE,
    ALT_DEFAULT_CSS_EXPECTED,
    ALT_DEFAULT_ENABLE_BOOT,
    ALT_DEFAULT_EXPECT_BOOT_ACK,
    ALT_DEFAULT_DISABLE_BOOT,
        ALT_DEFAULT_BOOT_MODE,
    ALT_DEFAULT_VOLT_SWITCH,
    ALT_DEFAULT_USE_HOLD_REG,
    ALT_DEFAULT_RESERVED,
    ALT_DEFAULT_START_BIT
};

static uint32_t                    rca_number; /*!< Relative card address.  */
static ALT_SDMMC_DMA_BUF_DESC_t    dma_descriptors[ALT_SDMMC_DMA_DESC_COUNT] __attribute__ ((aligned (ALT_CACHE_LINE_SIZE)));
                                        /*!< Array of DMA descriptors.  */
static ALT_SDMMC_DMA_BUF_DESC_t    *dma_cur_descr __attribute__ ((aligned (ALT_CACHE_LINE_SIZE)));
                                        /*!< Current descriptor.  */
#define ALT_SDMMC_DMA_BUF_DESC_CACHE_SIZE (((ALT_SDMMC_DMA_DESC_COUNT*sizeof(ALT_SDMMC_DMA_BUF_DESC_t)) + ALT_CACHE_LINE_SIZE - 1) & ~(ALT_CACHE_LINE_SIZE-1))

/*
// Reset SD/MMC module by reset manager without deassert
*/
static ALT_STATUS_CODE alt_sdmmc_rstmgr_set(void)
{
#ifdef soc_cv_av
    alt_setbits_word(ALT_RSTMGR_PERMODRST_ADDR, ALT_RSTMGR_PERMODRST_SDMMC_SET_MSK);
#else /* a10 */
    alt_setbits_word(ALT_RSTMGR_PER0MODRST_ADDR, ALT_RSTMGR_PER0MODRST_SDMMC_SET_MSK);
#endif
    return ALT_E_SUCCESS;
}

/*
// Assert reset SD/MMC module by reset manager, wait, deassert
*/
static ALT_STATUS_CODE alt_sdmmc_rstmgr_strobe(void)
{
    volatile uint32_t timeout = ALT_SDMMC_RESET_TMO_INIT;
#ifdef soc_cv_av
    alt_setbits_word(ALT_RSTMGR_PERMODRST_ADDR, ALT_RSTMGR_PERMODRST_SDMMC_SET_MSK);
#else
    alt_setbits_word(ALT_RSTMGR_PER0MODRST_ADDR, ALT_RSTMGR_PER0MODRST_SDMMC_SET_MSK);
#endif
    /*  Wait while SD/MMC module is reseting*/
    while (timeout--)
        ;

#ifdef soc_cv_av
    /*  Deassert the appropriate SD/MMC module reset signal via the Reset Manager Peripheral Reset register.*/
    alt_clrbits_word(ALT_RSTMGR_PERMODRST_ADDR, ALT_RSTMGR_PERMODRST_SDMMC_SET_MSK);
#else
    alt_clrbits_word(ALT_RSTMGR_PER0MODRST_ADDR, ALT_RSTMGR_PER0MODRST_SDMMC_SET_MSK);
#endif

    return ALT_E_SUCCESS;
}

/*
// Initialize descriptor chain for dma
*/
static ALT_STATUS_CODE alt_sdmmc_desc_chain_init()
{
    uint32_t count;
    ALT_SDMMC_DMA_BUF_DESC_t * dma_desc = dma_descriptors;

    /*  Initialising descriptor chain*/
    for (count = 0; count < ALT_SDMMC_DMA_DESC_COUNT; count++)
    {
        dma_desc[count].des0.fld.own  = 0;
        dma_desc[count].des0.fld.ch   = 1;
        dma_desc[count].des0.fld.er   = 0;
        dma_desc[count].des1.fld.bs1  = 0;
        dma_desc[count].des2.fld.bap1 = 0;

        /*  Create chain description list*/
        if (count == (ALT_SDMMC_DMA_DESC_COUNT - 1))
        {
            /*  If it is latest element set pointer to the ring head.*/
            dma_desc[count].des3.fld.bap2_or_next = (uint32_t) dma_desc;
        }
        else
        {
            /*  Set pointer to the next element in the ring*/
            dma_desc[count].des3.fld.bap2_or_next = (uint32_t) (&dma_desc[count + 1]);
        }
    }
    
    dma_cur_descr = dma_desc;
    
    return alt_cache_system_purge(dma_descriptors, ALT_SDMMC_DMA_BUF_DESC_CACHE_SIZE);
}

/*
// Clear descriptors of chain for DMA operations
*/
static ALT_STATUS_CODE alt_sdmmc_desc_chain_clear()
{
    uint32_t count;
    ALT_SDMMC_DMA_BUF_DESC_t * dma_desc = dma_descriptors;

    /*  Clean descriptions*/
    for (count = 0; count < ALT_SDMMC_DMA_DESC_COUNT; count++)
    {
        dma_desc[count].des0.fld.own  = 0;
        dma_desc[count].des0.fld.dic  = 0;
        dma_desc[count].des0.fld.ld   = 0;
        dma_desc[count].des0.fld.fs   = 0;
        dma_desc[count].des1.fld.bs1  = 0;
        dma_desc[count].des2.fld.bap1 = 0;
    }

    dma_cur_descr = dma_desc;
        
    return alt_cache_system_purge(dma_descriptors, ALT_SDMMC_DMA_BUF_DESC_CACHE_SIZE);
}

/*
// Initialize the specified SD/MMC controller instance for use and return a device
// handle referencing it.
*/
ALT_STATUS_CODE alt_sdmmc_init()
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;

    if (alt_clk_is_enabled(ALT_CLK_SDMMC) != ALT_E_TRUE)
    {
        return ALT_E_BAD_CLK;
    }

    /*  Query the SD/MMC clock.*/
    if (status == ALT_E_SUCCESS)
    {
#ifdef soc_cv_av
        status = alt_clk_freq_get(ALT_CLK_SDMMC, &clock_freq);
#else
        status = alt_clk_freq_get_sdmmc(&clock_freq);
#endif
    }

    /*  Reset SD/MMC module*/
    if (status == ALT_E_SUCCESS)
    {
        status = alt_sdmmc_reset();
    }

    return status;
}

/*
//Reset the SD/MMC controller by stopping any data transfers in progress and
//putting the controller into reset and reinit it after reset complete.
*/
ALT_STATUS_CODE alt_sdmmc_reset()
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;

    bool already_enabled = (alt_sdmmc_card_pwr_is_on() == ALT_E_TRUE);

    if (already_enabled)
    {
        /*  Temporarily power off the card*/
        status = alt_sdmmc_card_pwr_off();
        if (status != ALT_E_SUCCESS)
        {
            return status;
        }
    }
    
    /*  Reset SD/MMC module by reset manager*/
    alt_sdmmc_rstmgr_strobe();
    
    if (already_enabled)
    {
        /*  Re-enable card power*/
        status = alt_sdmmc_card_pwr_on();
    }

    /*  Relative card address has not been read yet*/
    rca_number = 0;
    /*  Init description chain*/
    alt_sdmmc_desc_chain_init();

    if (status == ALT_E_SUCCESS)
    {
        /* Enable default clock for alt_sdmmc_command_send to work*/
        alt_write_word(ALT_SDMMC_CLKENA_ADDR, ALT_SDMMC_CLKENA_CCLK_EN_SET(true));
        status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_CLK_INDEX, 0x0, NULL);
    }
#ifdef soc_a10
    alt_write_word(ALT_SDMMC_CTL_ADDR, ALT_SDMMC_CTL_CTLLER_RST_SET_MSK |
                                       ALT_SDMMC_CTL_FIFO_RST_SET_MSK |
                                       ALT_SDMMC_CTL_DMA_RST_SET_MSK);
#endif
    return status;
}

/*
// Uninitialize the SD/MMC controller referenced by the sdmmc_dev handle.
*/
ALT_STATUS_CODE alt_sdmmc_uninit(void)
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;

    /*  Clean descriptor chain*/
    alt_sdmmc_desc_chain_clear();

    /*  Card power off*/
    if (status == ALT_E_SUCCESS)
    {
        status = alt_sdmmc_card_pwr_off();
    }

    /*  Reset SD/MMC module by reset manager*/
    if (status == ALT_E_SUCCESS)
    {
        status = alt_sdmmc_rstmgr_set();
    }

    return status;
}

/*
// Power on of the card.
*/
ALT_STATUS_CODE alt_sdmmc_card_pwr_on(void)
{
    alt_setbits_word(ALT_SDMMC_PWREN_ADDR, 
                     ALT_SDMMC_PWREN_POWER_EN_SET_MSK);

    return ALT_E_SUCCESS;
}

/*
//Power off of the card.
*/
ALT_STATUS_CODE alt_sdmmc_card_pwr_off(void)
{
    /* If SD/MMC controller is enabled, return with sucess*/
    if (alt_sdmmc_card_pwr_is_on() == ALT_E_FALSE)
    {
        return ALT_E_SUCCESS;
    }

    /* Else clear enable bit of SD/MMC_enable register*/
    alt_clrbits_word(ALT_SDMMC_PWREN_ADDR,
                     ALT_SDMMC_PWREN_POWER_EN_SET_MSK);
    
    /*  Clear interrupt status*/
    alt_sdmmc_int_clear(ALT_SDMMC_INT_STATUS_ALL);

    /*  Relative card address has not been read yet*/
    rca_number = 0;
    /*  Reset state of card stack*/
    return ALT_E_SUCCESS;
}

/*
// Check whether SD/MMC controller is enable
*/
bool alt_sdmmc_card_pwr_is_on(void)
{
    if (ALT_SDMMC_PWREN_POWER_EN_GET(alt_read_word(ALT_SDMMC_PWREN_ADDR)) == 
        ALT_SDMMC_PWREN_POWER_EN_E_ON)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/*
// Returns ALT_E_TRUE if the SD/MMC controller is busy
*/
static ALT_STATUS_CODE alt_sdmmc_is_busy(void)
{
    if (ALT_SDMMC_STAT_DATA_BUSY_GET(alt_read_word(ALT_SDMMC_STAT_ADDR)) ==
        ALT_SDMMC_STAT_DATA_BUSY_E_CARDBUSY)
    {
        return ALT_E_TRUE;
    }
    else
    {
        return ALT_E_FALSE;
    }
}

/*
// Returns ALT_E_TRUE if the SD/MMC and iddmac controller is in idle state
*/
static ALT_STATUS_CODE alt_sdmmc_is_idle(void)
{
    uint32_t mmc_state = ALT_SDMMC_STAT_CMD_FSM_STATES_GET(alt_read_word(ALT_SDMMC_STAT_ADDR));
    
    uint32_t dma_state = ALT_SDMMC_IDSTS_FSM_GET(alt_read_word(ALT_SDMMC_IDSTS_ADDR));
    
    if ((mmc_state != ALT_SDMMC_FSM_IDLE) || (dma_state != ALT_SDMMC_DMA_FSM_IDLE))
    {
#ifdef LOGGER
        dprintf("\nstate %x dma_state %x\n", (int)mmc_state, (int)dma_state);
#endif
        return ALT_E_FALSE;
    }
    else
    {
        return ALT_E_TRUE;
    }
}

/*
// Get config clock parameters
*/
uint32_t alt_sdmmc_card_clk_div_get(void)
{
    return ALT_SDMMC_CLKDIV_CLK_DIVR0_GET(alt_read_word(ALT_SDMMC_CLKDIV_ADDR));
}

/*
// Set config clock parameters (7.2.3 Clock Programming)
*/
ALT_STATUS_CODE alt_sdmmc_card_clk_div_set(const uint32_t clk_div)
{
    ALT_STATUS_CODE status;
    bool low_power_mode;

    if (alt_sdmmc_is_busy() == ALT_E_TRUE)
    {
        return ALT_E_ERROR;
    }

    low_power_mode = alt_sdmmc_card_clk_low_power_is_enabled();

    /*  Disable cclk_enable bit of clkena*/
    status = alt_sdmmc_card_clk_disable();
    if (status != ALT_E_SUCCESS)
    {
        dprintf("alt_sdmmc_card_clk_disable failed\n");
        return status;
    }

    /*  6. reset sdmmc_clk_enable to 0 in the enable register of perpllgrp*/
    alt_clk_clock_disable(ALT_CLK_SDMMC);

    /*  7. program drvsel and samplsel in ctrl of sdmmcgrp*/
    if (clk_div == 0)
    {
#ifdef soc_cv_av
        /*  50 MHz*/
        alt_write_word(ALT_SYSMGR_SDMMC_CTL_ADDR, ALT_SYSMGR_SDMMC_CTL_SMPLSEL_SET(ALT_SYSMGR_SDMMC_CTL_SMPLSEL_E_DEGREES90) |
                                                  ALT_SYSMGR_SDMMC_CTL_DRVSEL_SET(ALT_SYSMGR_SDMMC_CTL_DRVSEL_E_DEGREES90));
#else
        alt_write_word(ALT_SYSMGR_SDMMC_ADDR, ALT_SYSMGR_SDMMC_SMPLSEL_SET(ALT_SYSMGR_SDMMC_SMPLSEL_E_DEGREES90)|
                                              ALT_SYSMGR_SDMMC_DRVSEL_SET(ALT_SYSMGR_SDMMC_DRVSEL_E_DEGREES90));
#endif
    }
    else
    {
#ifdef soc_cv_av
        /*  25MHz and lower*/
        alt_write_word(ALT_SYSMGR_SDMMC_CTL_ADDR, ALT_SYSMGR_SDMMC_CTL_SMPLSEL_SET(ALT_SYSMGR_SDMMC_CTL_SMPLSEL_E_DEGREES180) |
                                                  ALT_SYSMGR_SDMMC_CTL_DRVSEL_SET(ALT_SYSMGR_SDMMC_CTL_DRVSEL_E_DEGREES180));
#else
        alt_write_word(ALT_SYSMGR_SDMMC_ADDR, 3);
            /*ALT_SYSMGR_SDMMC_SMPLSEL_SET(ALT_SYSMGR_SDMMC_SMPLSEL_E_DEGREES180)|
                                              ALT_SYSMGR_SDMMC_DRVSEL_SET(ALT_SYSMGR_SDMMC_DRVSEL_E_DEGREES180));*/
#endif
    }

    /*  8. set sdmmc_clk_enable to 1*/
    alt_clk_clock_enable(ALT_CLK_SDMMC);

    alt_write_word(ALT_SDMMC_CLKDIV_ADDR, ALT_SDMMC_CLKDIV_CLK_DIVR0_SET(clk_div));
    
    /*  Re-enable required clocks via Clock Enable*/
    status = alt_sdmmc_card_clk_enable(low_power_mode);
    return status;
}

uint32_t alt_sdmmc_card_speed_get(void)
{
    uint32_t clk_div = alt_sdmmc_card_clk_div_get();

    /*  The sdmmc_clk(clock_freq) is divided by 4, then further divided by 2*clk_div inside the controller.*/
    uint32_t speed_bps = clock_freq / (4 * 2 * clk_div);

    return speed_bps;
}

#ifdef soc_cv_av
ALT_STATUS_CODE alt_sdmmc_card_speed_set(ALT_SDMMC_CARD_INFO_t * card_info, uint32_t xfer_speed)
#else
ALT_STATUS_CODE alt_sdmmc_card_speed_set(__attribute__((unused))ALT_SDMMC_CARD_INFO_t * card_info, uint32_t xfer_speed)
#endif
{
    uint32_t        clk_div = clock_freq / (4 * 2 * xfer_speed);
    ALT_STATUS_CODE status = ALT_E_SUCCESS;
    uint32_t        current_clk_div;
    bool            clock_disabled = false;
    bool            low_power_enable =  false;

#ifdef soc_cv_av
       uint8_t switch_function[64] = {0}; /* switch function status 64 bytes long */
#endif

    current_clk_div = alt_sdmmc_card_clk_div_get();
    if (current_clk_div != clk_div)
    {
        /*  For backwards compatibility, the caller may have disabled the clk before calling this function.*/
        /*  Need to enable it for switch function to work.*/
        if (alt_sdmmc_card_clk_is_enabled() == false)
        {
            clock_disabled = true;
            low_power_enable = alt_sdmmc_card_clk_low_power_is_enabled();
            alt_sdmmc_card_clk_enable(low_power_enable);
        }
#ifdef soc_cv_av
       /* switch_function[64] = {0};  switch function status 64 bytes long */

        if (clk_div == 0) /*  need to switch from 25MHz to 50MHz*/
        {
            if (card_info->high_speed)
            {
                #ifdef LOGGER
                dprintf("High speed supported. Switching...\n");
                #endif
                status = alt_sdmmc_card_switch_function(ALT_SDMMC_SWITCH_FUNC_MODE_SWITCH |
                                                            ALT_SDMMC_SWITCH_FUNC_G6_KEEP |
                                                            ALT_SDMMC_SWITCH_FUNC_G5_KEEP |
                                                            ALT_SDMMC_SWITCH_FUNC_G4_KEEP |
                                                            ALT_SDMMC_SWITCH_FUNC_G3_KEEP |
                                                            ALT_SDMMC_SWITCH_FUNC_G2_KEEP |
                                                            ALT_SDMMC_SWITCH_FUNC_HIGH_SPEED,
                                                            switch_function);
                if ((status == ALT_E_SUCCESS) && (switch_function[16] & 0xF) == 1)
                {
                    #ifdef LOGGER
                    dprintf("Switching to high speed succeeded\n");
                    #endif
                    /*  Delay 8 cycle*/
                    volatile uint32_t count = 0x8;
                    while (count > 0)
                    {
                        --count;
                    }
                }
                else
                {
                    dprintf("Switching to high speed failed, switch_function[16] = 0x%x\n", (int)switch_function[16]);
                }
            }
            else
            {
                dprintf("High speed not supported.\n");
            }
        }
        else if (current_clk_div == 0) /*  need to switch from 50MHz to 25MHz*/
        {
            /*  CMD 6 and switch function must be supported, otherwise, it couldn't have gone to high speed mode.*/
            status = alt_sdmmc_card_switch_function(ALT_SDMMC_SWITCH_FUNC_MODE_SWITCH     |
                                                            ALT_SDMMC_SWITCH_FUNC_G6_KEEP |
                                                            ALT_SDMMC_SWITCH_FUNC_G5_KEEP |
                                                            ALT_SDMMC_SWITCH_FUNC_G4_KEEP |
                                                            ALT_SDMMC_SWITCH_FUNC_G3_KEEP |
                                                            ALT_SDMMC_SWITCH_FUNC_G2_KEEP |
                                                            ALT_SDMMC_SWITCH_FUNC_DEFAULT_SPEED,
                                                            switch_function);
            if ((status == ALT_E_SUCCESS) && ((switch_function[16] & 0xF) == 0))
            {
                #ifdef LOGGER
                dprintf("Switching to default speed succeeded\n");
                #endif
                /*  Delay 8 cycle*/
                volatile uint32_t count = 0x8;
                while (count > 0)
                {
                    --count;
                }
            }
            else
            {
                dprintf("Switching to default speed failed\n");
            }
        }
#endif
        if (status == ALT_E_SUCCESS)
        {
            status = alt_sdmmc_card_clk_div_set(clk_div);
            if (status !=  ALT_E_SUCCESS)
            {
                dprintf("alt_sdmmc_card_clk_div_set failed\n");
                return status;
            }
        }
        if (clock_disabled)
        {
            status = alt_sdmmc_card_clk_disable();
        }
    }
    return status;
}

ALT_STATUS_CODE alt_sdmmc_card_clk_disable(void)
{
    if (alt_sdmmc_is_busy() == ALT_E_TRUE)
    {
        return ALT_E_ERROR;
    }

    alt_write_word(ALT_SDMMC_CLKENA_ADDR, ALT_SDMMC_CLKENA_CCLK_EN_SET(false));

    return alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_CLK_INDEX, 0x0, NULL);
}

/*
// Enables the card clock (sdmmc_cclk_out).
*/
ALT_STATUS_CODE alt_sdmmc_card_clk_enable(const bool use_low_pwr_mode)
{
    if (alt_sdmmc_is_busy() == ALT_E_TRUE)
    {
        return ALT_E_ERROR;
    }

    /*  Enable clock*/
    alt_write_word(ALT_SDMMC_CLKENA_ADDR, ALT_SDMMC_CLKENA_CCLK_EN_SET(true)
                                        | ALT_SDMMC_CLKENA_CCLK_LOW_POWER_SET(use_low_pwr_mode));

    return alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_CLK_INDEX, 0x0, NULL);
}

/*
// Returns true if the card clock (sdmmc_cclk_out) is enabled otherwise returns false
*/
bool alt_sdmmc_card_clk_is_enabled(void)
{
    return ALT_SDMMC_CLKENA_CCLK_EN_GET(alt_read_word(ALT_SDMMC_CLKENA_ADDR));
}

/*
// Returns true if low-power mode (cclk_low_power) is enabled otherwise returns false
*/
bool alt_sdmmc_card_clk_low_power_is_enabled(void)
{
    return ALT_SDMMC_CLKENA_CCLK_LOW_POWER_GET(alt_read_word(ALT_SDMMC_CLKENA_ADDR));
}

/*
// Get SD/MMC bus width
*/
static ALT_SDMMC_BUS_WIDTH_t alt_sdmmc_bus_width_get(void)
{
    uint32_t ctype_register = alt_read_word(ALT_SDMMC_CTYPE_ADDR);

    uint16_t card_width1 = ALT_SDMMC_CTYPE_CARD_WIDTH1_GET(ctype_register);
    uint16_t card_width2 = ALT_SDMMC_CTYPE_CARD_WIDTH2_GET(ctype_register);

    if (card_width1 == ALT_SDMMC_CTYPE_CARD_WIDTH1_E_MOD8BIT)
    {
        return ALT_SDMMC_BUS_WIDTH_8;
    }
    else if (card_width2 == ALT_SDMMC_CTYPE_CARD_WIDTH2_E_MOD4BIT)
    {
        return ALT_SDMMC_BUS_WIDTH_4;
    }
    else
    {
        return ALT_SDMMC_BUS_WIDTH_1;
    }
}

/*
// Set SD/MMC bus width
*/
static ALT_STATUS_CODE alt_sdmmc_bus_width_set(const ALT_SDMMC_BUS_WIDTH_t width)
{
    /*  Set config parameters to appropriate registers*/
    switch (width)
    {
    case ALT_SDMMC_BUS_WIDTH_8:
        alt_replbits_word(ALT_SDMMC_CTYPE_ADDR,
                          ALT_SDMMC_CTYPE_CARD_WIDTH1_SET_MSK,
                          ALT_SDMMC_CTYPE_CARD_WIDTH1_SET(ALT_SDMMC_CTYPE_CARD_WIDTH1_E_MOD8BIT));
        break;

    case ALT_SDMMC_BUS_WIDTH_4:
        alt_replbits_word(ALT_SDMMC_CTYPE_ADDR,
                          ALT_SDMMC_CTYPE_CARD_WIDTH1_SET_MSK,
                          ALT_SDMMC_CTYPE_CARD_WIDTH1_SET(ALT_SDMMC_CTYPE_CARD_WIDTH1_E_NON8BIT));
        alt_replbits_word(ALT_SDMMC_CTYPE_ADDR,
                          ALT_SDMMC_CTYPE_CARD_WIDTH2_SET_MSK, 
                          ALT_SDMMC_CTYPE_CARD_WIDTH2_SET(ALT_SDMMC_CTYPE_CARD_WIDTH2_E_MOD4BIT));
        break;

    case ALT_SDMMC_BUS_WIDTH_1:
        alt_replbits_word(ALT_SDMMC_CTYPE_ADDR,
                          ALT_SDMMC_CTYPE_CARD_WIDTH1_SET_MSK, 
                          ALT_SDMMC_CTYPE_CARD_WIDTH1_SET(ALT_SDMMC_CTYPE_CARD_WIDTH1_E_NON8BIT));
        alt_replbits_word(ALT_SDMMC_CTYPE_ADDR,
                          ALT_SDMMC_CTYPE_CARD_WIDTH2_SET_MSK, 
                          ALT_SDMMC_CTYPE_CARD_WIDTH2_SET(ALT_SDMMC_CTYPE_CARD_WIDTH2_E_MOD1BIT));
        break;

    default:
        return ALT_E_BAD_ARG;
    }

    return ALT_E_SUCCESS;
}

/*
// Get block size
*/
static __inline uint16_t alt_sdmmc_block_size_get(void)
{
    uint32_t blksiz_register = alt_read_word(ALT_SDMMC_BLKSIZ_ADDR);
    return ALT_SDMMC_BLKSIZ_BLOCK_SIZE_GET(blksiz_register);
}

/*
// Set block size
*/
static __inline ALT_STATUS_CODE alt_sdmmc_block_size_set(uint16_t block_size)
{
    alt_replbits_word(ALT_SDMMC_BLKSIZ_ADDR,
                      ALT_SDMMC_BLKSIZ_BLOCK_SIZE_SET_MSK,
                      ALT_SDMMC_BLKSIZ_BLOCK_SIZE_SET(block_size));

    return ALT_E_SUCCESS;
}

/*
// Set byte count
*/
static __inline ALT_STATUS_CODE alt_sdmmc_byte_count_set(uint32_t count)
{
    alt_replbits_word(ALT_SDMMC_BYTCNT_ADDR,
                      ALT_SDMMC_BYTCNT_BYTE_COUNT_SET_MSK,
                      ALT_SDMMC_BYTCNT_BYTE_COUNT_SET(count));

    return ALT_E_SUCCESS;
}

/*
// Get SD/MMC timeouts for command response and data sending
*/
ALT_STATUS_CODE alt_sdmmc_card_misc_get(ALT_SDMMC_CARD_MISC_t *card_misc_cfg)
{
    uint32_t tmout_register = alt_read_word(ALT_SDMMC_TMOUT_ADDR);

    card_misc_cfg->response_timeout = ALT_SDMMC_TMOUT_RESPONSE_TMO_GET(tmout_register);
    card_misc_cfg->data_timeout     = ALT_SDMMC_TMOUT_DATA_TMO_GET(tmout_register);

    card_misc_cfg->card_width = alt_sdmmc_bus_width_get();
    card_misc_cfg->block_size = alt_sdmmc_block_size_get();

    card_misc_cfg->debounce_count = ALT_SDMMC_DEBNCE_DEBOUNCE_COUNT_GET(alt_read_word(ALT_SDMMC_DEBNCE_ADDR));

    return ALT_E_SUCCESS;
}

/*
//Set SD/MMC timeouts for command response and data sending
*/
ALT_STATUS_CODE alt_sdmmc_card_misc_set(const ALT_SDMMC_CARD_MISC_t *card_misc_cfg)
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;

    uint32_t tmout_value = ALT_SDMMC_TMOUT_RESPONSE_TMO_SET(card_misc_cfg->response_timeout)
                         | ALT_SDMMC_TMOUT_DATA_TMO_SET(card_misc_cfg->data_timeout);

    alt_write_word(ALT_SDMMC_TMOUT_ADDR, tmout_value);

    alt_replbits_word(ALT_SDMMC_DEBNCE_ADDR,
                      ALT_SDMMC_DEBNCE_DEBOUNCE_COUNT_SET_MSK,
                      ALT_SDMMC_DEBNCE_DEBOUNCE_COUNT_SET(card_misc_cfg->debounce_count));

    if (status == ALT_E_SUCCESS)
    {
        status = alt_sdmmc_bus_width_set(card_misc_cfg->card_width);
    }

    if (status == ALT_E_SUCCESS)
    {
        status = alt_sdmmc_block_size_set(card_misc_cfg->block_size);
    }

    return status;
}

/*
// Starts the SD/MMC internal DMA transfer with the specified
// descriptor an bus mode transfer configuration.
*/
ALT_STATUS_CODE alt_sdmmc_dma_start(ALT_SDMMC_DMA_BUF_DESC_t *buf_desc_list, 
                                    const uint32_t desc_skip_len,
                                    const ALT_SDMMC_DMA_PBL_t burst_len,
                                    const bool use_fixed_burst)
{
    uint32_t bmod_set_value = ALT_SDMMC_BMOD_PBL_SET(burst_len)
                            | ALT_SDMMC_BMOD_FB_SET(use_fixed_burst)
                            | ALT_SDMMC_BMOD_DSL_SET(desc_skip_len);

    uint32_t bmod_set_mask = ALT_SDMMC_BMOD_PBL_SET_MSK
                           | ALT_SDMMC_BMOD_FB_SET_MSK
                           | ALT_SDMMC_BMOD_DSL_SET_MSK;

    /*  Set start address of descriptor chain*/
    alt_write_word(ALT_SDMMC_DBADDR_ADDR, (uint32_t)buf_desc_list);

    alt_replbits_word(ALT_SDMMC_BMOD_ADDR, bmod_set_mask, bmod_set_value);

    return ALT_E_SUCCESS;
}

/*
// Enables the sdmmc write protect.
*/
bool alt_sdmmc_card_is_write_protected(void)
{
    alt_setbits_word(ALT_SDMMC_WRTPRT_ADDR, 
                     ALT_SDMMC_WRTPRT_WR_PROTECT_SET_MSK);

    return ALT_E_SUCCESS;
}

/*
// FIFO reset
*/
ALT_STATUS_CODE alt_sdmmc_fifo_reset(void)
{
    uint32_t timeout = ALT_SDMMC_MAX_T_POLL_COUNT;

    /*  Activate fifo reset*/
    alt_setbits_word(ALT_SDMMC_CTL_ADDR, ALT_SDMMC_CTL_FIFO_RST_SET_MSK);
    
    /*  Wait to complete reset or timeout*/
    while (ALT_SDMMC_CTL_FIFO_RST_GET(alt_read_word(ALT_SDMMC_CTL_ADDR))
                                                && --timeout)
        ;

    /*  If fifo reset still are active, return timeout error*/
    if (timeout == 0)
    {
        return ALT_E_TMO;
    }

    return ALT_E_SUCCESS;
}
    
/*
// DMA reset
*/
ALT_STATUS_CODE alt_sdmmc_dma_reset(void)
{
    uint32_t timeout = ALT_SDMMC_MAX_T_POLL_COUNT;

    /* Activate dma reset*/
    alt_setbits_word(ALT_SDMMC_CTL_ADDR, ALT_SDMMC_CTL_DMA_RST_SET_MSK);
    
    /*  Wait to complete reset or timeout*/
    while (ALT_SDMMC_CTL_DMA_RST_GET(alt_read_word(ALT_SDMMC_CTL_ADDR))
                                                && --timeout)
        ;

    /*  If dma reset still are active, return timeout error*/
    if (timeout == 0)
    {
        return ALT_E_TMO;
    }

    return ALT_E_SUCCESS;
}
        

/*
// Returns ALT_E_TRUE if the SD/MMC controller is present depend on cdata_in.
*/
bool alt_sdmmc_card_is_detected(void)
{
    if (ALT_SDMMC_STAT_DATA_3_STAT_GET(alt_read_word(ALT_SDMMC_STAT_ADDR)) ==
        ALT_SDMMC_STAT_DATA_3_STAT_E_CARDPRESENT)
/*    if (ALT_SDMMC_CDETECT_CARD_DETECT_N_GET(alt_read_word(ALT_SDMMC_CDETECT_ADDR))
                                        == ALT_SDMMC_CDETECT_CARD_DETECT_N_E_DETECTED) */
    {
        return true;
    }
    else
    {
        return false;
    }
}

/*
//Set command configuration
*/
static ALT_STATUS_CODE alt_sdmmc_cmd_set(const ALT_SDMMC_CMD_INDEX_t cmd_index,
                                         const ALT_SDMMC_CMD_CONFIG_t *cmd_cfg,
                                         bool start_cmd)
{
     uint32_t cmd_register = ALT_SDMMC_CMD_CMD_INDEX_SET(cmd_index)
                           | ALT_SDMMC_CMD_RESPONSE_EXPECT_SET(cmd_cfg->response_expect) 
                           | ALT_SDMMC_CMD_RESPONSE_LEN_SET(cmd_cfg->response_length_long)
                           | ALT_SDMMC_CMD_CHECK_RESPONSE_CRC_SET(cmd_cfg->check_response_crc)
                           | ALT_SDMMC_CMD_DATA_EXPECTED_SET(cmd_cfg->data_expected)
                           | ALT_SDMMC_CMD_RD_WR_SET(cmd_cfg->write_active)
                           | ALT_SDMMC_CMD_TFR_MOD_SET(cmd_cfg->stream_mode_active)
                           | ALT_SDMMC_CMD_SEND_AUTO_STOP_SET(cmd_cfg->send_auto_stop)
                           | ALT_SDMMC_CMD_WAIT_PRVDATA_COMPLETE_SET(cmd_cfg->wait_prvdata_complete)
                           | ALT_SDMMC_CMD_STOP_ABT_CMD_SET(cmd_cfg->stop_abort_cmd)
                           | ALT_SDMMC_CMD_SEND_INITIALIZATION_SET(cmd_cfg->send_initialization)
                           | ALT_SDMMC_CMD_UPDATE_CLK_REGS_ONLY_SET(cmd_cfg->update_clock_registers_only)
                           | ALT_SDMMC_CMD_RD_CEATA_DEVICE_SET(cmd_cfg->read_ceata_device)
                           | ALT_SDMMC_CMD_CCS_EXPECTED_SET(cmd_cfg->ccs_expected)
                           | ALT_SDMMC_CMD_EN_BOOT_SET(cmd_cfg->enable_boot)
                           | ALT_SDMMC_CMD_EXPECT_BOOT_ACK_SET(cmd_cfg->expect_boot_ack)
                           | ALT_SDMMC_CMD_DIS_BOOT_SET(cmd_cfg->disable_boot)
                           | ALT_SDMMC_CMD_BOOT_MOD_SET(cmd_cfg->boot_mode)
                           | ALT_SDMMC_CMD_VOLT_SWITCH_SET(cmd_cfg->volt_switch)
                           | ALT_SDMMC_CMD_USE_HOLD_REG_SET(cmd_cfg->use_hold_reg)
                           | ALT_SDMMC_CMD_START_CMD_SET(start_cmd);

    alt_write_word(ALT_SDMMC_CMD_ADDR, cmd_register);

#ifdef DMA_LOGGER
    dprintf("\ncommand = %X\n", (int)cmd_register);
#endif

    return ALT_E_SUCCESS;
}

/*
// Set command argument
*/
static __inline ALT_STATUS_CODE alt_sdmmc_cmd_arg_set(uint32_t cmdarg)
{
    alt_write_word(ALT_SDMMC_CMDARG_ADDR, cmdarg);

    return ALT_E_SUCCESS;
}

/*
// Get response of previous command.
*/
static __inline ALT_STATUS_CODE alt_sdmmc_read_short_response(uint32_t *response)
{
    uint32_t resp0 = alt_read_word(ALT_SDMMC_RESP0_ADDR);
    *response = (uint32_t)(ALT_SDMMC_RESP0_RESPONSE0_GET(resp0));

    return ALT_E_SUCCESS;
}

/*
// Get long response of previous command.
*/
ALT_STATUS_CODE alt_sdmmc_read_long_response(ALT_SDMMC_RESPONSE_t *response)
{
    uint32_t resp0 = alt_read_word(ALT_SDMMC_RESP0_ADDR);
    uint32_t resp1 = alt_read_word(ALT_SDMMC_RESP1_ADDR);
    uint32_t resp2 = alt_read_word(ALT_SDMMC_RESP2_ADDR);
    uint32_t resp3 = alt_read_word(ALT_SDMMC_RESP3_ADDR);

    response->resp0 = (uint32_t)(ALT_SDMMC_RESP0_RESPONSE0_GET(resp0));
    response->resp1 = (uint32_t)(ALT_SDMMC_RESP1_RESPONSE1_GET(resp1));
    response->resp2 = (uint32_t)(ALT_SDMMC_RESP2_RESPONSE2_GET(resp2));
    response->resp3 = (uint32_t)(ALT_SDMMC_RESP3_RESPONSE3_GET(resp3));

    return ALT_E_SUCCESS;
}

/*
//This function reads a single data byte from the receive FIFO.
*/
ALT_STATUS_CODE alt_sdmmc_fifo_read(void *dest, const size_t size)
{
    uint32_t counter;
    uint32_t * dest_ptr = dest;
    for (counter = 0; counter < size / 4; counter++)
    {
        dest_ptr[counter] = (uint32_t)(ALT_SDMMC_DATA_VALUE_GET(alt_read_word(ALT_SDMMC_DATA_ADDR)));
    }
    
    if (size & 0x3)
    {
        uint8_t * add_dest_ptr = (uint8_t*)dest + (size / 4);
        uint32_t word_notfull = (uint32_t)(ALT_SDMMC_DATA_VALUE_GET(alt_read_word(ALT_SDMMC_DATA_ADDR)));
    
        for (counter = 0; counter < (size & 0x3); counter++)
        {
            add_dest_ptr[counter] = (uint8_t)word_notfull;
            word_notfull = word_notfull >> 8;
        }
    }

    return ALT_E_SUCCESS;
}

/*
// This function writes a single data byte to the transmit FIFO.
*/
ALT_STATUS_CODE alt_sdmmc_fifo_write(const void *src, const size_t size)
{
    uint32_t counter;
    const uint32_t * src_ptr = src;
    for (counter = 0; counter < size / 4; counter++)
    {
        alt_write_word(ALT_SDMMC_DATA_ADDR, ALT_SDMMC_DATA_VALUE_SET(src_ptr[counter]));
    }

    if (size & 0x3)
    {
        const uint8_t *add_src_ptr = (uint8_t*)src + (size / 4);
        uint32_t word_notfull = 0;

        for (counter = 0; counter < (size & 0x3); counter++)
        {
            word_notfull |= (uint32_t)add_src_ptr[counter] << (8 * counter);
        }

        alt_write_word(ALT_SDMMC_DATA_ADDR, ALT_SDMMC_DATA_VALUE_SET(word_notfull));
    }
    
    return ALT_E_SUCCESS;
}

/*
// Returns the current SD/MMC controller interrupt status conditions.
*/
uint32_t alt_sdmmc_int_status_get(void)
{
    return alt_read_word(ALT_SDMMC_MINTSTS_ADDR);
}

/*
// Returns the SD/MMC controller raw interrupt status conditions irrespective of
// the interrupt status condition enablement state.
*/
uint32_t alt_sdmmc_int_mask_get(void)
{
    return alt_read_word(ALT_SDMMC_INTMSK_ADDR);
}

/*
// Clears the specified SD/MMC controller interrupt status conditions identified
// in the mask.
*/
ALT_STATUS_CODE alt_sdmmc_int_clear(const uint32_t mask)
{
    alt_write_word(ALT_SDMMC_RINTSTS_ADDR, mask);

    return ALT_E_SUCCESS;
}

/*
// Disable the specified SD/MMC controller interrupt status conditions identified in
// the mask.
*/
ALT_STATUS_CODE alt_sdmmc_int_disable(const uint32_t mask)
{
    alt_clrbits_word(ALT_SDMMC_INTMSK_ADDR, mask);

    return ALT_E_SUCCESS;
}

/*
// Enable the specified SD/MMC controller interrupt status conditions identified in
// the mask.
*/
ALT_STATUS_CODE alt_sdmmc_int_enable(const uint32_t mask)
{
    if (mask & 0x0001ffff)
    {
        alt_setbits_word(ALT_SDMMC_CTL_ADDR, 
                     ALT_SDMMC_CTL_INT_EN_SET_MSK);

        alt_setbits_word(ALT_SDMMC_INTMSK_ADDR, mask);
    }
    return ALT_E_SUCCESS;
}

/*
//Returns true if SD/MMC controller FIFO has reached the receive watermark level
//otherwise returns false.
*/
bool alt_sdmmc_fifo_is_rx_wtrmk_reached(void)
{
    if (ALT_SDMMC_STAT_FIFO_RX_WATERMARK_GET(alt_read_word(ALT_SDMMC_STAT_ADDR)) == 
                    ALT_SDMMC_STAT_FIFO_RX_WATERMARK_E_RXWATERMARK)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/*
//Returns true if SD/MMC controller FIFO has reached the transmit watermark level
//otherwise returns false.
*/
bool alt_sdmmc_fifo_is_tx_wtrmk_reached(void)
{
    if (ALT_SDMMC_STAT_FIFO_TX_WATERMARK_GET(alt_read_word(ALT_SDMMC_STAT_ADDR)) == 
                     ALT_SDMMC_STAT_FIFO_TX_WATERMARK_E_TXWATERMARK)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/*
// Returns ALT_E_TRUE when the receive FIFO is empty.
*/
bool alt_sdmmc_fifo_is_empty(void)
{
    if (ALT_SDMMC_STAT_FIFO_EMPTY_GET(alt_read_word(ALT_SDMMC_STAT_ADDR)) ==
        ALT_SDMMC_STAT_FIFO_EMPTY_E_FIFOEMPTY)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/*
// Returns ALT_E_TRUE when the receive FIFO is completely full.
*/
bool alt_sdmmc_fifo_is_full(void)
{
    if (ALT_SDMMC_STAT_FIFO_FULL_GET(alt_read_word(ALT_SDMMC_STAT_ADDR)) ==
        /*ALT_SDMMC_STAT_FIFO_FULL_E_FIFOFULL*/1) /*  TODO fix this macro properly*/
    {
        return true;
    }
    else
    {
        return false;
    }
}

/*
// Returns the number of valid entries in the receive FIFO.
*/
int32_t alt_sdmmc_fifo_count(void)
{
    return (int32_t)ALT_SDMMC_STAT_FIFO_COUNT_GET(alt_read_word(ALT_SDMMC_STAT_ADDR));
}

/*
// Gets the configured FIFO operational parameter values.
*/
ALT_STATUS_CODE alt_sdmmc_fifo_param_get(uint32_t *rx_wtrmk, uint32_t *tx_wtrmk, ALT_SDMMC_MULT_TRANS_t *mult_trans_size)
{
    uint32_t fifoth = alt_read_word(ALT_SDMMC_FIFOTH_ADDR);

    *rx_wtrmk        = ALT_SDMMC_FIFOTH_RX_WMARK_GET(fifoth);
    *tx_wtrmk        = ALT_SDMMC_FIFOTH_TX_WMARK_GET(fifoth);
    *mult_trans_size = (ALT_SDMMC_MULT_TRANS_t)ALT_SDMMC_FIFOTH_DW_DMA_MULT_TRANSACTION_SIZE_GET(fifoth);

    return ALT_E_SUCCESS;
}

/*
// Sets the configured FIFO operational parameter values.
*/
ALT_STATUS_CODE alt_sdmmc_fifo_param_set(uint32_t rx_wtrmk, uint32_t tx_wtrmk, ALT_SDMMC_MULT_TRANS_t mult_trans_size)
{
    uint32_t fifoth_set_mask = ALT_SDMMC_FIFOTH_RX_WMARK_SET_MSK
                             | ALT_SDMMC_FIFOTH_TX_WMARK_SET_MSK
                             | ALT_SDMMC_FIFOTH_DW_DMA_MULT_TRANSACTION_SIZE_SET_MSK;
    
    uint32_t fifoth_set_value = ALT_SDMMC_FIFOTH_RX_WMARK_SET(rx_wtrmk)
                              | ALT_SDMMC_FIFOTH_TX_WMARK_SET(tx_wtrmk)
                              | ALT_SDMMC_FIFOTH_DW_DMA_MULT_TRANSACTION_SIZE_SET(mult_trans_size);
                            
    alt_replbits_word(ALT_SDMMC_FIFOTH_ADDR,
                      fifoth_set_mask,
                      fifoth_set_value);

    return ALT_E_SUCCESS;
}

/*
// Card reset
*/
ALT_STATUS_CODE alt_sdmmc_card_reset(void)
{
    volatile uint32_t timeout = ALT_SDMMC_RESET_TMO_INIT;

    /*  Assert card reset */
    alt_setbits_word(ALT_SDMMC_RST_N_ADDR, 
                     ALT_SDMMC_RST_N_CARD_RST_SET_MSK);

    /*  Wait while card reset*/
    while (timeout--)
        ;

    /*  Deassert the appropriate card reset.*/
    alt_clrbits_word(ALT_SDMMC_RST_N_ADDR, 
                     ALT_SDMMC_RST_N_CARD_RST_SET_MSK);

    return ALT_E_SUCCESS;
}

/*
// Enables the SD/MMC Internal DMA Controller.
*/
ALT_STATUS_CODE alt_sdmmc_dma_enable(void)
{
    alt_setbits_word(ALT_SDMMC_CTL_ADDR, 
                     ALT_SDMMC_CTL_USE_INTERNAL_DMAC_SET_MSK);
    alt_setbits_word(ALT_SDMMC_BMOD_ADDR, 
                     ALT_SDMMC_BMOD_DE_SET_MSK);

    return ALT_E_SUCCESS;
}

/*
// Disables the SD/MMC Internal DMA Controller
*/
ALT_STATUS_CODE alt_sdmmc_dma_disable(void)
{
    alt_clrbits_word(ALT_SDMMC_CTL_ADDR, 
                     ALT_SDMMC_CTL_USE_INTERNAL_DMAC_SET_MSK);
    alt_clrbits_word(ALT_SDMMC_BMOD_ADDR, 
                     ALT_SDMMC_BMOD_DE_SET_MSK);

    return ALT_E_SUCCESS;
}

/*
// Enables the SD/MMC Internal DMA Controller.
*/
ALT_STATUS_CODE alt_sdmmc_is_dma_enabled(void)
{
    if (   ALT_SDMMC_CTL_USE_INTERNAL_DMAC_GET(alt_read_word(ALT_SDMMC_CTL_ADDR))
        && ALT_SDMMC_BMOD_DE_GET(alt_read_word(ALT_SDMMC_BMOD_ADDR)))
    {
        return ALT_E_TRUE;
    }
    else
    {
        return ALT_E_FALSE;
    }
}

/*
// Returns the current SD/MMC controller interrupt IDMAC status conditions.
*/
uint32_t alt_sdmmc_dma_int_status_get(void)
{
    return alt_read_word(ALT_SDMMC_IDSTS_ADDR);
}

/*
// Returns the SD/MMC internal DMA controller interrupt mask value which
// reflects the enabled internal DMA controller interrupt status conditions.
*/
uint32_t alt_sdmmc_dma_int_mask_get(void)
{
    return alt_read_word(ALT_SDMMC_IDINTEN_ADDR);
}

/*
// Clears the specified SD/MMC controller interrupt status IDMAC conditions identified
// in the mask.
*/
ALT_STATUS_CODE alt_sdmmc_dma_int_clear(const uint32_t mask)
{
    alt_write_word(ALT_SDMMC_IDSTS_ADDR, mask);

    return ALT_E_SUCCESS;
}

/*
// Disable the specified SD/MMC controller interrupt IDMAC status conditions identified in
// the mask.
*/
ALT_STATUS_CODE alt_sdmmc_dma_int_disable(const uint32_t mask)
{
    alt_clrbits_word(ALT_SDMMC_IDINTEN_ADDR, mask);

    return ALT_E_SUCCESS;
}

/*
// Enable the specified SD/MMC controller interrupt status conditions identified in
// the mask.
*/
ALT_STATUS_CODE alt_sdmmc_dma_int_enable(const uint32_t mask)
{
    alt_setbits_word(ALT_SDMMC_IDINTEN_ADDR, mask);

    return ALT_E_SUCCESS;
}

/*
// Sets value into this register for the IDMAC FSM to resume normal descriptor fetch operation.
*/
ALT_STATUS_CODE alt_sdmmc_poll_demand_set(const uint32_t value)
{
    alt_write_word(ALT_SDMMC_PLDMND_ADDR, ALT_SDMMC_PLDMND_PD_SET(value));

    return ALT_E_SUCCESS;
}

/*
// Disable Card Read Threshold .
*/
ALT_STATUS_CODE alt_sdmmc_card_rd_threshold_disable(void)
{
    alt_clrbits_word(ALT_SDMMC_CARDTHRCTL_ADDR,
                     ALT_SDMMC_CARDTHRCTL_CARDRDTHREN_SET_MSK);

    return ALT_E_SUCCESS;
}


/*
// Enable Card Read Threshold .
*/
ALT_STATUS_CODE alt_sdmmc_card_rd_threshold_enable(const uint32_t threshold)
{
    alt_replbits_word(ALT_SDMMC_CARDTHRCTL_ADDR,
                        ALT_SDMMC_CARDTHRCTL_CARDRDTHRESHOLD_SET_MSK 
                      | ALT_SDMMC_CARDTHRCTL_CARDRDTHREN_SET_MSK,
                        ALT_SDMMC_CARDTHRCTL_CARDRDTHRESHOLD_SET(threshold) 
                      | ALT_SDMMC_CARDTHRCTL_CARDRDTHREN_SET(ALT_SDMMC_CARDTHRCTL_CARDRDTHREN_E_END));

    return ALT_E_SUCCESS;
}

/*
// This function return ALT_E_ERROR if interrupt error was detected
*/
static ALT_STATUS_CODE alt_sdmmc_error_status_detect(void)
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;
    uint32_t int_status = 0;

    /*  All SD/MMC interrupt status caused by an error*/
    uint32_t err = (  ALT_SDMMC_INT_STATUS_RE
                    | ALT_SDMMC_INT_STATUS_RCRC
                    | ALT_SDMMC_INT_STATUS_DCRC
                    | ALT_SDMMC_INT_STATUS_RTO
                    | ALT_SDMMC_INT_STATUS_DRTO
                    | ALT_SDMMC_INT_STATUS_FRUN
                    | ALT_SDMMC_INT_STATUS_HLE
                    | ALT_SDMMC_INT_STATUS_SBE
                    | ALT_SDMMC_INT_STATUS_EBE);

    int_status = alt_sdmmc_int_status_get();
    
    /*  Checking on errors*/
    if (int_status & err)
    {
        dprintf("alt_sdmmc_int_status_get err=0x%x\n", (unsigned int)int_status);
        status = ALT_E_ERROR;
    }

    return status;
}

/*
// Read/write all data from/to buffer
*/
static ALT_STATUS_CODE alt_sdmmc_transfer_helper(uint32_t * buffer,
                                                 const size_t size,
                                                 ALT_SDMMC_TMOD_t transfer_mode)
{
#ifdef LOGGER
    dprintf("\nalt_sdmmc_transfer_helper\n");
#endif

    ALT_STATUS_CODE status = ALT_E_SUCCESS;

    uint32_t data_size = size;
    bool read_freeze  = false;
    bool write_freeze = false;
    
    while (data_size > 0)
    {
        uint32_t timeout = ALT_SDMMC_TMO_WAITER;
        uint32_t level;


#ifdef LOGGER
        dprintf("\ndata_size = %x\n", (int)data_size);
        /*  Error handling*/
#endif
        /*  Error checking*/
        status = alt_sdmmc_error_status_detect();

        if (status != ALT_E_SUCCESS)
        {
            dprintf("Interrupt error detected\n");
            break;
        }

        do
        {
            read_freeze =  ((transfer_mode == ALT_SDMMC_TMOD_READ)
                                            && (alt_sdmmc_fifo_is_empty() == true));
            write_freeze = ((transfer_mode == ALT_SDMMC_TMOD_WRITE)
                                            && (alt_sdmmc_fifo_is_full() == true));
#ifdef LOGGER
            dprintf("\nread_freeze = %x write_freeze = %x\n", (int)read_freeze, (int)write_freeze);
#endif
            if (--timeout == 0)
            {
                status = ALT_E_TMO;
                dprintf("Timed out due to FIFO not available\n");
                return status;
            }
        }
        while (read_freeze || write_freeze);
        
        level = alt_sdmmc_fifo_count();

#ifdef LOGGER
        dprintf("\nfifo level = %x\n", (int)level);
#endif

        /*  Top up the TX FIFO with read issues*/

        if (transfer_mode == ALT_SDMMC_TMOD_WRITE)
        {
            uint32_t i;
            uint32_t free_space = ALT_SDMMC_FIFO_NUM_ENTRIES - level;
            free_space = ALT_MIN(data_size / 4, free_space);

            for (i = 0; i < free_space; i++)
            {
                alt_write_word(ALT_SDMMC_DATA_ADDR, *buffer);
                ++buffer;
            }
            data_size -= free_space * 4;
        }

        /*  Read out the resulting received data as they come in.*/

        if (transfer_mode == ALT_SDMMC_TMOD_READ)
        {
            uint32_t i;
            level = ALT_MIN(data_size / 4, level);

            for (i = 0; i < level; i++)
            {
                *buffer = ALT_SDMMC_DATA_VALUE_GET(alt_read_word(ALT_SDMMC_DATA_ADDR));
                ++buffer;
            }

            data_size -= level * 4;
        }
    }

    return status;
}

/*
// Fill descriptors
*/
static ALT_STATUS_CODE alt_sdmmc_dma_trans_helper(uint32_t * buffer,
                                                  size_t buf_len)
{
#ifdef DMA_LOGGER
    dprintf("\nalt_sdmmc_dma_trans_helper: buf_len = %d\n",
                                                (int)buf_len);
#endif
    ALT_STATUS_CODE status = ALT_E_SUCCESS;
    /* Pointer to current descriptor*/
    ALT_SDMMC_DMA_BUF_DESC_t *cur_dma_desc = dma_cur_descr;

    uint32_t cur_buffer = (uint32_t)buffer;
    uint32_t len_left = buf_len;

    while (len_left > 0)
    {
        uint32_t idmac_status;
        /* Error checking*/
        status = alt_sdmmc_error_status_detect();
        if (status != ALT_E_SUCCESS)
        {
            status = ALT_E_ERROR;
            break;
        }
        /* If current descriptor is free then fill it*/
        if (cur_dma_desc->des0.fld.own == 0)
        {
            int set_len = len_left > ALT_SDMMC_DMA_SEGMENT_SIZE ? ALT_SDMMC_DMA_SEGMENT_SIZE : len_left;
            /* Disable interrupt after it will be free*/
            cur_dma_desc->des0.fld.dic = 1;/* socfpga->dma_cur_pos % 4;*/
            /* Set If it is first part of buffer for transfer*/
            cur_dma_desc->des0.fld.fs = (buf_len == len_left) ? 1 : 0;
            /* Set size of des2*/
            cur_dma_desc->des1.fld.bs1 = set_len;
            /* Set address of buffer in memory*/
            cur_dma_desc->des2.fld.bap1 = cur_buffer;

#ifdef LOGGER
//            dprintf("socfpga_setup_dma_add: des_adrdr %08X des2_paddr %08X des1_len %08X len_left %08X\n", 
//                        (int)cur_dma_desc, (int)cur_buffer, (int)set_len, (int)len_left);
#endif

            /* Update address buffer and buffer len*/
            cur_buffer += set_len;
            len_left -= set_len;
            /* Set if it is last part of buffer*/
            cur_dma_desc->des0.fld.ld = (len_left == 0) ? 1 : 0;
            /* Descriptor could be used*/
            cur_dma_desc->des0.fld.own = 1;
            /* Currernt descriptor set to the next element */
            cur_dma_desc = (ALT_SDMMC_DMA_BUF_DESC_t *)cur_dma_desc->des3.fld.bap2_or_next;
        }
        
        alt_cache_system_purge(dma_descriptors, ALT_SDMMC_DMA_BUF_DESC_CACHE_SIZE);
        
        idmac_status = alt_sdmmc_dma_int_status_get();

        /*  If DMA status is as descriptor unavailable then resume transfer and clean interrupt status*/
        if (idmac_status & ALT_SDMMC_DMA_INT_STATUS_DU)
        {
            alt_sdmmc_dma_int_clear(ALT_SDMMC_DMA_INT_STATUS_ALL);
            alt_sdmmc_poll_demand_set(0x1);
        }
        /*  If DMA status is another abnormal then break with error*/
        else if (idmac_status & ALT_SDMMC_DMA_INT_STATUS_AI)
        {
            status = ALT_E_ERROR;
            break;
        }

    }

    return status;

}

/*
// Waiter of data transfer complete
*/
static ALT_STATUS_CODE alt_sdmmc_data_done_waiter(void)
{
    ALT_STATUS_CODE status = ALT_E_TMO;
    uint32_t timeout = ALT_SDMMC_TMO_WAITER;

    while (--timeout)
    {
        uint32_t int_status;
        int_status = alt_sdmmc_int_status_get();

        /*  Error checking*/
        if (alt_sdmmc_error_status_detect() != ALT_E_SUCCESS)
        {
            status = ALT_E_ERROR;
            break;
        }

        if (alt_sdmmc_is_dma_enabled() == ALT_E_TRUE)
        {
            uint32_t idmac_status = alt_sdmmc_dma_int_status_get();

            /*  If DMA status is abnormal then transfer complete with error*/
            if (idmac_status & ALT_SDMMC_DMA_INT_STATUS_AI)
            {
                status = ALT_E_ERROR;
                dprintf("Abnormal DMA reported.\n");
                break;
            }
        }
        /*  Data transfer over caused by complete transfer operation*/
        if (int_status & ALT_SDMMC_INT_STATUS_DTO)
        {
            alt_sdmmc_int_clear(ALT_SDMMC_INT_STATUS_DTO);
            status = ALT_E_SUCCESS;
            break;
        }
    }

    timeout = ALT_SDMMC_TMO_WAITER;
    while (!alt_sdmmc_is_idle() && --timeout)
        ;
    if (timeout == 0)
    {
        dprintf("Timed out waiting for SDMMC to become idle\n");
        status = ALT_E_TMO;
    }

    return status;
}


/*
// Waiter of clock command complete
*/
static ALT_STATUS_CODE alt_sdmmc_clock_waiter(void)
{
    ALT_STATUS_CODE status = ALT_E_TMO;
    uint32_t timeout = ALT_SDMMC_TMO_WAITER;
    
    while (--timeout)
    {
        uint32_t cmd_register = alt_read_word(ALT_SDMMC_CMD_ADDR);
        
        /*  Error checking*/
        if (alt_sdmmc_error_status_detect() != ALT_E_SUCCESS)
        {
            status = ALT_E_ERROR;
            break;
        }

        /*  Only for clock command detect complete operation by 0 in start_cmd bit of cmd register*/
        if (ALT_SDMMC_CMD_START_CMD_GET(cmd_register) == ALT_SDMMC_CMD_START_CMD_E_NOSTART)
        {
            status = ALT_E_SUCCESS;
            break;
        }
    }
    return status;
}

/*
// Waiter of command complete
*/
static ALT_STATUS_CODE alt_sdmmc_cmd_waiter(void)
{
    ALT_STATUS_CODE status = ALT_E_TMO;
    uint32_t timeout = ALT_SDMMC_TMO_WAITER;
    
    while (--timeout)
    {
        uint32_t int_status;
        int_status = alt_sdmmc_int_status_get();

        /*  Error checking*/
        if (alt_sdmmc_error_status_detect() != ALT_E_SUCCESS)
        {
            status = ALT_E_ERROR;
            break;
        }
        
        /* Check command done*/
        if (int_status & ALT_SDMMC_INT_STATUS_CMD)
        {
            alt_sdmmc_int_clear(ALT_SDMMC_INT_STATUS_CMD);
            status = ALT_E_SUCCESS;
            break;
        }
    }
    return status;
}

/*
// Read SRC register from card and read supported bus width
*/
static ALT_STATUS_CODE alt_sdmmc_card_scr_get(uint64_t* scr_reg)
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;
#ifdef LOGGER
    uint32_t response = 0;
#endif
    
    uint16_t prev_blk_size = 0;
    /*  Save current block size and change it*/
    prev_blk_size = alt_sdmmc_block_size_get();
    alt_sdmmc_block_size_set(8);
    alt_sdmmc_byte_count_set(8);

     /*  reset FIFO*/
    status = alt_sdmmc_fifo_reset();
    if (status != ALT_E_SUCCESS)
    {
        return status;
    }

    /*  reset DMA*/
    if (alt_sdmmc_is_dma_enabled())
    {
        status = alt_sdmmc_dma_reset();
        if (status != ALT_E_SUCCESS)
        {
            return status;
        }
    }

    /* alt_sdmmc_card_rd_threshold_enable(0x80);*/

    if (alt_sdmmc_is_dma_enabled())
    {
        /*  Clean descriptor chain*/
        alt_sdmmc_desc_chain_clear();

        alt_sdmmc_dma_start(dma_cur_descr, 0x0,
                            ALT_SDMMC_DMA_PBL_1, false);
        /* Enable all dma interrupt status*/
        alt_sdmmc_dma_int_enable(ALT_SDMMC_DMA_INT_STATUS_ALL);
    }

    /*  Activate ACMD commands*/
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_APP_CMD, rca_number, NULL);
    if (status != ALT_E_SUCCESS)
    {
        return status;
    }

#ifdef LOGGER
    alt_sdmmc_read_short_response(&response);
    dprintf("\nALT_SDMMC_APP_CMD response = %x\n", (int)response);
#endif
    
    /*  Send request for read SCR register*/
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_ACMD, ALT_SD_SEND_SCR, 0x0, NULL);
    if (status != ALT_E_SUCCESS)
    {
        return status;
    }

#ifdef LOGGER
    alt_sdmmc_read_short_response(&response);
    dprintf("\nALT_SD_SEND_SCR response R1 = %x\n", (int)response);
#endif

    /* Read SCR register*/
    /* Send or read data*/
    if (alt_sdmmc_is_dma_enabled())
    {
        /* Fill descriptors*/
        status = alt_sdmmc_dma_trans_helper((uint32_t*)scr_reg, 8);
    }
    else
    {
        status = alt_sdmmc_transfer_helper((uint32_t*)scr_reg, 8, ALT_SDMMC_TMOD_READ);
    }

    if (status != ALT_E_SUCCESS)
    {
        dprintf("alt_sdmmc_transfer_helper failed\n");
        return status;
    }

    /*  Transfer complete*/
    status = alt_sdmmc_data_done_waiter();
    if (status != ALT_E_SUCCESS)
    {
        dprintf("alt_sdmmc_data_done_waiter failed\n");
        return status;
    }

#ifdef LOGGER
    uint8_t* scr_buf_8 = (uint8_t*)scr_reg;
    dprintf("ALT_SD_SEND_SCR data = %02x %02x %02x %02x  %02x %02x %02x %02x\n",
        scr_buf_8[0], scr_buf_8[1], scr_buf_8[2], scr_buf_8[3],
        scr_buf_8[4], scr_buf_8[5], scr_buf_8[6], scr_buf_8[7]);
#endif

    /*  Re-change block size*/
    alt_sdmmc_block_size_set(prev_blk_size);

    return status;
}

/*
// Set SD/MMC card width
*/
ALT_STATUS_CODE alt_sdmmc_card_bus_width_set(ALT_SDMMC_CARD_INFO_t * card_info, const ALT_SDMMC_BUS_WIDTH_t width)
{
    uint32_t set_width_arg;
    ALT_STATUS_CODE status = ALT_E_SUCCESS;

    if ((card_info->scr_bus_widths & width) == 0)
    {
        return ALT_E_BAD_ARG;
    }


    uint32_t buswidth = 0;

    switch (width)
    {
    case ALT_SDMMC_BUS_WIDTH_8:
        set_width_arg = 0x3;
		buswidth = 8;
        break;
    case ALT_SDMMC_BUS_WIDTH_4:
        set_width_arg = 0x2;
		buswidth = 4;
        break;
    case ALT_SDMMC_BUS_WIDTH_1:
        set_width_arg = 0x0;
		buswidth = 1;
        break;
    default:
        return ALT_E_BAD_ARG;
    }
dprintf("Card bus width = %u\n", buswidth);
#ifdef LOGGER
    uint32_t response = 0;
#endif

    /*  Activate ACMD commands*/
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_APP_CMD, rca_number, NULL);
    if (status != ALT_E_SUCCESS)
    {
        return status;
    }

#ifdef LOGGER
    alt_sdmmc_read_short_response(&response);
    dprintf("\nALT_SDMMC_APP_CMD response = %x\n", (int)response);
#endif
    
    /*  Send new card bus width*/
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_ACMD, ALT_SD_SET_BUS_WIDTH, set_width_arg, NULL);
    if (status != ALT_E_SUCCESS)
    {
        return status;
    }

#ifdef LOGGER
    alt_sdmmc_read_short_response(&response);
    dprintf("\nALT_SD_SET_BUS_WIDTH response = %x\n", (int)response);
#endif
    
    /*  Set new bus width in controller register*/
    alt_sdmmc_bus_width_set(width);
    
    return status;
}

/*
// Set block size
*/
ALT_STATUS_CODE alt_sdmmc_card_block_size_set(const uint16_t block_size)
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;
    
    /*  Send new block size to card*/
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_SET_BLOCKLEN, block_size, NULL);
    if (status != ALT_E_SUCCESS)
    {
        dprintf("\nALT_SDMMC_SET_BLOCKLEN failed\n");
        return status;
    }

#ifdef LOGGER
    uint32_t response;

    alt_sdmmc_read_short_response(&response);
    dprintf("\nALT_SDMMC_SET_BLOCKLEN response = %x\n", (int)response);
#endif

    /*  Set new block size in controller register*/
    alt_sdmmc_block_size_set(block_size);

    return status;
}

/*
// Enumerated Card Stack ident sdio io only
*/
static ALT_STATUS_CODE alt_sdmmc_card_ident_io_only(ALT_SDMMC_CARD_INFO_t *card_info)
{
#ifdef LOGGER
    dprintf("\nalt_sdmmc_card_ident_io_only\n");
#endif
    ALT_STATUS_CODE status = ALT_E_SUCCESS;

    uint32_t int_status = 0;
    uint32_t response = 0;
    bool is_sdio_combo;
    /*  Enumerated Card Stack p.2a - 2b*/
    /*  Activates the card's initialization process.*/
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_SEND_OP_COND, 0x0, &response);
    if (status != ALT_E_SUCCESS)
    {
        int_status = alt_sdmmc_int_status_get();
        if (int_status & ALT_SDMMC_INT_STATUS_RTO)
        {
            return ALT_E_SUCCESS;
        }
        else
        {
            return status;
        }
    }

#ifdef LOGGER
    alt_sdmmc_read_short_response(&response);
    dprintf("\nALT_SDMMC_SEND_OP_COND_1 = %x\n", (int)response);
#endif

    /*  Enumerated Card Stack p.2c*/
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_SEND_OP_COND, 0x100000, &response);
    if (status != ALT_E_SUCCESS)
    {
        int_status = alt_sdmmc_int_status_get();
        if (int_status & ALT_SDMMC_INT_STATUS_RTO)
        {
            return ALT_E_SUCCESS;
        }
        else
        {
            return status;
        }
    }
    
#ifdef LOGGER
    dprintf("\nALT_SDMMC_SEND_OP_COND_2 response = %x\n", (int)response);
#endif
    /*  Enumerated Card Stack p.2d*/
    is_sdio_combo = response & (1 << 27);
    card_info->card_type = (is_sdio_combo) 
                           ? ALT_SDMMC_CARD_TYPE_NOTDETECT
                           : ALT_SDMMC_CARD_TYPE_SDIOIO;

    return ALT_E_SUCCESS;
}

/*
// Enumerated Card Stack ident sdhc type
*/
static ALT_STATUS_CODE alt_sdmmc_card_ident_sdhc(ALT_SDMMC_CARD_INFO_t *card_info)
{
#ifdef LOGGER
    dprintf("\nalt_sdmmc_card_ident_sdhc\n");
#endif
    ALT_STATUS_CODE status = ALT_E_SUCCESS;

    uint32_t int_status = 0;
    uint32_t response = 0;

    uint32_t clk_div = clock_freq / (4 * 2 * 400000);
    uint32_t timeout;
    
    status = alt_sdmmc_card_clk_div_set(clk_div);
    if (status != ALT_E_SUCCESS)
    {
        dprintf("alt_sdmmc_card_clk_div_set failed\n");
        return status;
    }

    /*  Resets all cards to Idle State*/
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_GO_IDLE_STATE, 0x0, NULL);
    if (status != ALT_E_SUCCESS)
    {
        int_status = alt_sdmmc_int_status_get();
        if (int_status & ALT_SDMMC_INT_STATUS_RTO)
        {
            dprintf("Response timeout\n");
            return status;
        }
        else
        {
            dprintf("Unexpected interrupt, not timeout\n");
            return status;
        }
    }
    
    /*  Enumerated Card Stack p.3a*/
    /*  For only SDC V2. Check voltage range.*/
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_IF_COND, 0x1AA, &response);

    if (status != ALT_E_SUCCESS)
    {
        dprintf("ALT_SDMMC_IF_COND failed\n");
        int_status = alt_sdmmc_int_status_get();
        if (int_status & ALT_SDMMC_INT_STATUS_RTO)
        {
            dprintf("ALT_SDMMC_IF_COND response timed out\n"); /*  do not support 2.7V to 3.6V*/
            return status;
        }
        else
        {
            dprintf("ALT_SDMMC_IF_COND interrupt unexpected\n");
            return status;
        }
    }
    else
    {
    #ifdef LOGGER
        alt_sdmmc_read_short_response(&response);
        dprintf("\nALT_SDMMC_IF_COND response = %x\n", (int)response);
    #endif

        if (response != 0x1AA) /*  R7 echo back expected*/
        {
            return ALT_E_ERROR;
        }
    }

    timeout = 1000000;
    do
    {
        /*  Indicates to the card that the next command is an*/
        /*  application specific command rather than a*/
        /*  standard command*/
        status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_APP_CMD, 0x0, &response);
        if (status != ALT_E_SUCCESS)
        {
            int_status = alt_sdmmc_int_status_get();
            if (int_status & ALT_SDMMC_INT_STATUS_RTO)
            {
                 dprintf("ALT_SDMMC_APP_CMD timed out\n");
                return ALT_E_SUCCESS;
            }
            else
            {
                dprintf("ALT_SDMMC_APP_CMD error not timed out\n");
                return status;
            }
        }

#ifdef LOGGER
     alt_sdmmc_read_short_response(&response);
     dprintf("\nALT_SDMMC_APP_CMD response R1 = %x\n", (int)response);
#endif

        /*  Enumerated Card Stack p.3c*/
        /*  Asks the accessed card to send its operating condition*/
        /*  register (OCR) content in the response on the CMD*/
        /*  line.*/
        /*  SDHC/SDXC supported, power saving, use current signal voltage.*/
        status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_ACMD, ALT_SD_SEND_OP_COND, 0x40FF8000, &response);
        if (status != ALT_E_SUCCESS)
        {
            int_status = alt_sdmmc_int_status_get();
            if (int_status & ALT_SDMMC_INT_STATUS_RTO)
            {
                dprintf("ALT_SDMMC_SEND_OP_COND timed out\n");
                return ALT_E_SUCCESS;
            }
            else
            {
                return status;
            }
        }

        /*  R3 response*/
#ifdef LOGGER
        alt_sdmmc_read_short_response(&response);
        dprintf("\nALT_SD_SEND_OP_COND response = %x\n", (int)response);
#endif

        /* Enumerated Card Stack p.3d*/
        if (response & 0x80000000) /*  Initialization Complete*/
        {
            if (response & 0x40000000)
                card_info->card_type = ALT_SDMMC_CARD_TYPE_SDHC; /*  and SDXC*/
            else
                card_info->card_type = ALT_SDMMC_CARD_TYPE_SD;
           break;
        }
    } while (--timeout);
    
    if (timeout == 0)
    {
        status = ALT_E_TMO;
    }
    return status;
}

/*
//Enumerated Card Stack ident sd type
*/
static ALT_STATUS_CODE alt_sdmmc_card_ident_sd(ALT_SDMMC_CARD_INFO_t *card_info)
{
#ifdef LOGGER
    dprintf("\nalt_sdmmc_card_ident_sd\n");
#endif
    ALT_STATUS_CODE status = ALT_E_SUCCESS;

    uint32_t int_status = 0;
#ifdef LOGGER
    uint32_t response = 0;
#endif
    /*  Enumerated Card Stack p.3e*/
    /*  Indicates to the card that the next command is an */
    /*  application specific command rather than a */
    /*  standard command */
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_APP_CMD, 0x0, NULL);
    if (status != ALT_E_SUCCESS)
    {
        int_status = alt_sdmmc_int_status_get();
        if (int_status & ALT_SDMMC_INT_STATUS_RTO)
        {
            return ALT_E_SUCCESS;
        }
        else
        {
            return status;
        }
    }

#ifdef LOGGER
    alt_sdmmc_read_short_response(&response);
    dprintf("\nALT_SDMMC_APP_CMD response = %x\n", (int)response);
#endif
    
    /*  Asks the accessed card to send its operating condition */
    /*  register (OCR) content in the response on the CMD*/
    /*  line. */
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_ACMD, ALT_SD_SEND_OP_COND, 0x00FF8000, NULL);
    if (status != ALT_E_SUCCESS)
    {
        int_status = alt_sdmmc_int_status_get();
        if (int_status & ALT_SDMMC_INT_STATUS_RTO)
        {
            return ALT_E_SUCCESS;
        }
        else
        {
            return status;
        }
    }
    
#ifdef LOGGER
    alt_sdmmc_read_short_response(&response);
    dprintf("\nALT_SD_SEND_OP_COND response = %x\n", (int)response);
#endif
    
    /*  Enumerated Card Stack p.3f*/
    card_info->card_type = ALT_SDMMC_CARD_TYPE_SD;

    return status;
}

/*
// Enumerated Card Stack enumerate SD card type, including SDSC, SDHC and SDXC
*/
static ALT_STATUS_CODE alt_sdmmc_card_enum_sd(ALT_SDMMC_CARD_INFO_t *card_info)
{
#ifdef LOGGER
    dprintf("\nalt_sdmmc_card_enum_sd\n");
#endif
    ALT_STATUS_CODE status = ALT_E_SUCCESS;
    ALT_SDMMC_RESPONSE_t response_long;

    uint32_t clk_div = clock_freq / (4 * 2 * 400000);
    uint32_t response = 0;
    uint32_t ocr_reg = 0xFF8000;
    uint32_t RCA_number;
    uint32_t rate_unit;
    uint32_t time_val;
    uint32_t csd_structure;
    uint32_t c_size_low;
    uint32_t c_size_high;
    static const uint32_t tran_speed_mul_x10[] = { 0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80 };
    static const uint32_t freq_unit[] = { 100000, 1000000, 10000000, 100000000 };

    alt_sdmmc_card_clk_disable();
    status = alt_sdmmc_card_clk_div_set(clk_div);
    if (status != ALT_E_SUCCESS)
    {
        return status;
    }
    alt_sdmmc_card_clk_enable(false);
    
    /*  Resets all cards to Idle State*/
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_GO_IDLE_STATE, 0x0, NULL);
    if (status != ALT_E_SUCCESS)
    {
        return status;
    }

    /*  Idle State*/

    /*  For only SDC V2. Check voltage range. 0xAA Check pattern, 2.7-3.6V*/
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_IF_COND, 0x1AA, &response);
    if (status != ALT_E_SUCCESS)
    {
        return status;
    }

    /*  R7 expected, echo back 0x1AA*/
#ifdef LOGGER
    alt_sdmmc_read_short_response(&response);
    dprintf("\nALT_SDMMC_IF_COND response = %x\n", (int)response);
#endif

/*    OCR Bit VDD Voltage Window 
           0-3          Reserved 
            4             1.6-1.7 
            5             1.7-1.8 
            6             1.8-1.9 
            7             1.9-2.0 
            8             2.0-2.1 
            9             2.1-2.2 
            10           2.2-2.3 
            11           2.3-2.4 
            12           2.4-2.5 
            13           2.5-2.6 
            14           2.6-2.7 
            15           2.7-2.8 
            16           2.8-2.9 
            17           2.9-3.0 
            18           3.0-3.1 
            19           3.1-3.2 
            20           3.2-3.3 
            21           3.3-3.4 
            22           3.4-3.5 
            23           3.5-3.6 
          24-29       Reserved 
            30          High capacity card
            31          Card power up status bit (busy)
*/

    if (card_info->card_type == ALT_SDMMC_CARD_TYPE_SDHC)
    {
        ocr_reg |= (1 << 30);
    }

    do
    {
        volatile uint32_t timeout = 1000000;

        status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_APP_CMD, 0x0, &response);
        if (status != ALT_E_SUCCESS)
        {
            return status;
        }

#ifdef LOGGER
        alt_sdmmc_read_short_response(&response);
        dprintf("\nALT_SDMMC_APP_CMD response = %x\n", (int)response);
#endif
        
        /*  Wait while SD/MMC module is reseting*/
        while (timeout--)
            ;
        status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_ACMD, ALT_SD_SEND_OP_COND, 0x40FF8000, &response);
        if (status != ALT_E_SUCCESS)
        {
            return status;
        }

#ifdef LOGGER
        alt_sdmmc_read_short_response(&response);
        dprintf("\nALT_SD_SEND_OP_COND response = %x\n", (int)response);
#endif
    }
    while ((response & (1UL << 31)) == 0);
    
    /* Asks any card to send their CID numbers on the CMD line.*/
    /* (Any card that is connected to the host will respond.)*/
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_ALL_SEND_CID, 0x0, /*&response*/NULL);
    if (status != ALT_E_SUCCESS)
    {
        return status;
    }

    /*  R2 response*/
#ifdef LOGGER
    alt_sdmmc_read_long_response(&response_long);
    dprintf("\nALT_SDMMC_ALL_SEND_CID response = %x %x %x %x\n",
            (int)response_long.resp3,
            (int)response_long.resp2,
            (int)response_long.resp1,
            (int)response_long.resp0);
#endif
    /*  Asks the card to publish a new relative address (RCA).*/
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_SET_RELATIVE_ADDR, 0x0, &response);
    if (status != ALT_E_SUCCESS)
    {
        return status;
    }

#ifdef LOGGER
    dprintf("\nALT_SDMMC_SET_RELATIVE_ADDR response = %x\n", (int)response);
#endif
    /*  Expect R6 response, new bit 31:16 RCA bit 15:0 card status*/
    RCA_number = response & 0xFFFF0000;
    rca_number = RCA_number;

    /*  BEGIN Stand-by state*/

    /*  Addressed card sends its card identification (CID) on the CMD line.*/
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_SEND_CID, rca_number, NULL);
    if (status != ALT_E_SUCCESS)
    {
        return status;
    }

    /*  R2 response*/
#ifdef LOGGER

    alt_sdmmc_read_long_response(&response_long);
    dprintf("\nALT_SDMMC_ALL_SEND_CID response = %x %x %x %x\n",
            (int)response_long.resp3,
            (int)response_long.resp2,
            (int)response_long.resp1,
            (int)response_long.resp0);
#endif

    /*  Addressed card sends its card-specific data (CSD) */
    /*  on the CMD line.*/
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_SEND_CSD, rca_number, &response);
    if (status != ALT_E_SUCCESS)
    {
        return status;
    }

    /* R2 response*/
    alt_sdmmc_read_long_response(&response_long);
    #ifdef LOGGER
    dprintf("\nALT_SDMMC_ALL_SEND_CSD response = %x %x %x %x\n",
            (int)response_long.resp3,
            (int)response_long.resp2,
            (int)response_long.resp1,
            (int)response_long.resp0);
    #endif
    card_info->max_r_blkln = 1 << ALT_SDMMC_CSD_MAX_R_BLK_GET(response_long.resp2);
    card_info->max_w_blkln = 1 << ALT_SDMMC_CSD_MAX_W_BLK_GET(response_long.resp0);
    card_info->csd_ccc = ALT_SDMMC_CSD_CCC_GET(response_long.resp2);
    card_info->partial_r_allowed = ALT_SDMMC_CSD_PART_R_ALLOW_GET(response_long.resp2);
    card_info->partial_w_allowed = ALT_SDMMC_CSD_PART_W_ALLOW_GET(response_long.resp0);
    rate_unit = ALT_SDMMC_CSD_SPEED_RATE_GET(response_long.resp3);
    time_val = ALT_SDMMC_CSD_SPEED_TIME_GET(response_long.resp3);
    csd_structure = ALT_SDMMC_CSD_CSD_STRUCTURE_GET(response_long.resp3);
    if ((time_val != 0) && (rate_unit <= 3))
    {
        /*  uint32_t speed_rate = (rate_unit == 0) ? 100 : pow(10, rate_unit - 1) * 1000;*/
        uint32_t speed_rate = freq_unit[rate_unit];
        card_info->xfer_speed = speed_rate * tran_speed_mul_x10[time_val] / 10;
    }
    else
    {
        return ALT_E_ERROR;
    }

    csd_structure = ALT_SDMMC_CSD_CSD_STRUCTURE_GET(response_long.resp3);
    if (csd_structure == ALT_SDMMC_CSD_CSD_STUCTURE_VERSION_1)
    {
        uint32_t c_size_mult = ALT_SDMMC_CSD_V1_C_SIZE_MULT_GET(response_long.resp1);
        c_size_low = ALT_SDMMC_CSD_V1_C_SIZE_LOW_GET(response_long.resp1);
        c_size_high = ALT_SDMMC_CSD_V1_C_SIZE_HIGH_GET(response_long.resp2);
        card_info->blk_number_low = ( (((c_size_high << 2) | c_size_low) + 1 ) * (1 << (c_size_mult + 2)) );
        card_info->blk_number_high = 0;
    }
    else
    {
        uint64_t c_size_plus_1;
        c_size_low = ALT_SDMMC_CSD_V2_C_SIZE_LOW_GET(response_long.resp1);
        c_size_high = ALT_SDMMC_CSD_V2_C_SIZE_HIGH_GET(response_long.resp2);
        c_size_plus_1 = ((c_size_high << 16) | c_size_low) + 1;
        card_info->blk_number_high = ( c_size_plus_1 >> 22); /*   ( c_size_plus_1 * 1024) >> 32)*/
        card_info->blk_number_low = (c_size_plus_1 << 10) & 0xFFFFFFFF;
    }
#ifdef LOGGER
	uint32_t ulMByte = card_info->blk_number_low / 2048;

	dprintf("Number of blocks = %u (%u MB) speed %u\n",
		(unsigned)card_info->blk_number_low,
		(unsigned)ulMByte,
		(unsigned)card_info->xfer_speed);
#endif
    /*  END Stand-by State*/
    /*  Command toggles a card between the Stand-by */
    /*  and Transfer states or between the Programming */
    /*  and Disconnect state*/
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_SEL_DES_CARD, rca_number, &response);
    if (status != ALT_E_SUCCESS)
    {
        return status;
    }

    /*  R1b response*/
#ifdef LOGGER
    alt_sdmmc_read_short_response(&response);
    dprintf("\nALT_SDMMC_SEL_DES_CARD response = %x\n", (int)response);
#endif

    /*  BEGIN Transfer State*/
    /*  Addressed card sends its status register*/
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_SEND_STATUS, rca_number, &response);
    if (status != ALT_E_SUCCESS)
    {
        return status;
    }

#ifdef LOGGER
    alt_sdmmc_read_short_response(&response);
    dprintf("\nALT_SDMMC_SEND_STATUS response = %x\n", (int)response);
#endif
    {
        uint64_t scr_reg;
        uint8_t * scr_buf_8;

        /*  Read SCR register*/
		scr_reg = 0U;
        status = alt_sdmmc_card_scr_get(&scr_reg);
        if (status != ALT_E_SUCCESS)
        {
            return status;
        }
#warning Take the second call away
		scr_reg = 0U;
        status = alt_sdmmc_card_scr_get(&scr_reg);
        if (status != ALT_E_SUCCESS)
        {
            return status;
        }
   
        scr_buf_8 = (uint8_t*)&scr_reg;
        card_info->scr_sd_spec = scr_buf_8[0] & 0xF;
        card_info->scr_bus_widths = scr_buf_8[1] & 0xF;
#ifdef LOGGER
    dprintf("scr_reg %02X %02X %02X %02X  %02X %02X %02X %02X\n",
		scr_buf_8[0], scr_buf_8[1], scr_buf_8[2], scr_buf_8[3],
		scr_buf_8[4], scr_buf_8[5], scr_buf_8[6], scr_buf_8[7]);
#endif
    }
#ifdef LOGGER
    dprintf("\nSD SPEC = %02x\n", (int)card_info->scr_sd_spec);
#endif

    /*  Read switch info*/
    status = alt_sdmmc_card_read_switch(card_info);

    /*  Enter data state*/
    return status;
}

static ALT_STATUS_CODE alt_sdmmc_card_read_switch(ALT_SDMMC_CARD_INFO_t * card_info)
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;
    uint8_t switch_function[64] = {0};

    card_info->high_speed = false;
    if ( ( card_info->scr_sd_spec == 0 ) ||               /* version 1.01 */
	     ( ( card_info->csd_ccc & CCC_CLASS_10 ) == 0U ) ) /* or not Class 10 */
    {
        dprintf("High speed not supported\n");
		dprintf("scr_sd_spec   = 0x%02X\n", card_info->scr_sd_spec);
		dprintf("card->csd_ccc = 0x%02X\n", card_info->csd_ccc);
        return ALT_E_BAD_ARG;
    }

    #ifdef LOGGER
    dprintf("\nSWITCH - Check Function\n");
    #endif
    /*  CMD6 should be supported. CMD6 is valid under the "Transfer State".*/
    /*  Mode bit31 = 0, everything else 0.*/
    status = alt_sdmmc_card_switch_function(ALT_SDMMC_SWITCH_FUNC_MODE_CHECK, switch_function);
    if (status == ALT_E_SUCCESS)
    {
        if (switch_function[13] & ALT_SDMMC_SD_MODE_HIGH_SPEED)
        {
            #ifdef LOGGER
                    dprintf("High speed supported. Switching...\n");
                    #endif
            card_info->high_speed = true;
        }
        else
        {
            dprintf("High speed not supported.\n");
        }
    }
    return status;
}

/*
// Enumerated Card Stack
*/
ALT_STATUS_CODE alt_sdmmc_card_identify(ALT_SDMMC_CARD_INFO_t * card_info)
{
#ifdef LOGGER
    dprintf("\nalt_sdmmc_card_identify\n");
#endif
    ALT_STATUS_CODE status = ALT_E_SUCCESS;

    card_info->card_type = ALT_SDMMC_CARD_TYPE_NOTDETECT;

    /* Enumerated Card Stack p.1*/
    alt_sdmmc_bus_width_set(ALT_SDMMC_BUS_WIDTH_1);
    if (status == ALT_E_SUCCESS)
    {
        status =  alt_sdmmc_card_ident_sdhc(card_info);
        if (card_info->card_type != ALT_SDMMC_CARD_TYPE_NOTDETECT && status == ALT_E_SUCCESS)
        {
            /*  If card is identified as SDSC, or SDHC(inc. SDXC) then prepare it*/
            status = alt_sdmmc_card_enum_sd(card_info);
            return status;
        }
    }
    if (status != ALT_E_SUCCESS)
    {
        status =  alt_sdmmc_card_ident_sd(card_info);
        if (card_info->card_type != ALT_SDMMC_CARD_TYPE_NOTDETECT && status == ALT_E_SUCCESS)
        {
            /*  If card is identified as SD card then prepare it*/
            status = alt_sdmmc_card_enum_sd(card_info);
            return status;
        }
    }

    if (status == ALT_E_SUCCESS)
    {
        status = alt_sdmmc_card_ident_io_only(card_info);
        /*  If card is identified as SDIO IO only or SDIO COMBO then prepare it*/
        if (card_info->card_type != ALT_SDMMC_CARD_TYPE_NOTDETECT && status == ALT_E_SUCCESS)
        {
            return status;
        }
    }

    return status;
}

/*
// Send the a command and command argument to the card and optionally return the
// command response.
*/
ALT_STATUS_CODE alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_t command_type,
                                       ALT_SDMMC_CMD_INDEX_t command,
                                       uint32_t command_arg,
                                       uint32_t *response)
{
    const ALT_SDMMC_CMD_CONFIG_t * cmd_cfg = NULL;
    uint32_t counter;
    uint32_t array_count = 0;
    bool found = false;
    /*  Create interrupt mask by command configurations*/
    uint32_t int_mask = ALT_SDMMC_INT_STATUS_RE             /*  Response error*/
                      | ALT_SDMMC_INT_STATUS_RTO            /*  Response timeout*/
                      | ALT_SDMMC_INT_STATUS_CD             /*  Card detect (CD) interrupt*/
                      | ALT_SDMMC_INT_STATUS_HLE            /*  Hardware Locked Write Error*/
                      | ALT_SDMMC_INT_STATUS_CMD;           /*  Command done (CD) interrupt*/
    ALT_STATUS_CODE status = 0;

    ALT_SDMMC_CMD_CONFIG_t *default_cfg;
    if (command_type == ALT_SDMMC_CMD_TYPE_BASIC)
    {
        default_cfg = cmd_default_cfg;
        array_count = sizeof(cmd_default_cfg)/sizeof(cmd_default_cfg[0]);
    }
    else if (command_type == ALT_SDMMC_CMD_TYPE_ACMD)
    {
        default_cfg = acmd_default_cfg;
        array_count = sizeof(acmd_default_cfg)/sizeof(acmd_default_cfg[0]);
    }
    else
    {
        dprintf("\ncommand_type not supported\n");
        return ALT_E_BAD_ARG;
    }

    if (command == ALT_SDMMC_CLK_INDEX)
    {
        cmd_cfg = &cmd_clock_cfg;
        found = true;
    }

    for (counter = 0; counter < array_count; counter++)
    {
        if (found == true)
        {
            break;
        }
        if (default_cfg[counter].cmd_index == command)
        {
            cmd_cfg = &default_cfg[counter];
            found = true;
        }
    }

    if (found == false)
    {
        return ALT_E_BAD_ARG;
    }

    if (cmd_cfg->wait_prvdata_complete)
    {
        uint32_t timeout = ALT_SDMMC_TMO_WAITER;
        while (alt_sdmmc_is_busy() && timeout--)
            ;
    }

    if (cmd_cfg->data_expected == true)
    {
        int_mask |= ALT_SDMMC_INT_STATUS_DTO                /*  Data transfer over*/
                  | ALT_SDMMC_INT_STATUS_RCRC               /*  Response CRC error*/
                  | ALT_SDMMC_INT_STATUS_DCRC               /*  Data CRC error*/
                  | ALT_SDMMC_INT_STATUS_HTO                /*  Data starvation by host timeout*/
                  | ALT_SDMMC_INT_STATUS_FRUN               /*  FIFO underrun/overrun error*/
                  | ALT_SDMMC_INT_STATUS_EBE;               /*  End-bit error*/
        
        if (cmd_cfg->write_active == ALT_SDMMC_TMOD_WRITE)
        {
            int_mask |= ALT_SDMMC_INT_STATUS_TXDR           /*  Transmit FIFO data request (TXDR)*/
                      | ALT_SDMMC_INT_STATUS_HLE;           /*  Hardware locked write error (HLE)*/
        }
        else
        {
            int_mask |= ALT_SDMMC_INT_STATUS_RXDR           /*  Receive FIFO data request (RXDR)*/
                       |ALT_SDMMC_INT_STATUS_SBE;           /*  Start-bit error (SBE)*/
        }
        
    }
    
    alt_sdmmc_int_disable(ALT_SDMMC_INT_STATUS_ALL);
    /*  Reset all possible interrupts*/
    alt_sdmmc_int_clear(ALT_SDMMC_INT_STATUS_ALL);
    /*  Interrupts enable*/
    alt_sdmmc_int_enable(int_mask);
    /*  Setup the Argument Register and send CMD*/
    alt_sdmmc_cmd_arg_set(command_arg);

    /*  Set command configurations*/
    alt_sdmmc_cmd_set(command, cmd_cfg, false);
    /*  Send command*/
    alt_sdmmc_cmd_set(command, cmd_cfg, true);

#ifdef DMA_LOGGER
    {
        uint32_t state = (uint32_t)ALT_SDMMC_STAT_CMD_FSM_STATES_GET(alt_read_word(ALT_SDMMC_STAT_ADDR));
    
        uint32_t dma_state = (uint32_t)ALT_SDMMC_IDSTS_FSM_GET(alt_read_word(ALT_SDMMC_IDSTS_ADDR));

        dprintf("\nstate %x dma_state %x\n", (int)state, (int)dma_state);
        dprintf("\nCMD = %d ARG = %x\n", (int)command, (int)command_arg);
    }
#endif

    if (cmd_cfg->update_clock_registers_only == true)
    {
        /* Wait for complete clock update command*/
        status = alt_sdmmc_clock_waiter();
#ifdef LOGGER
        if (status == ALT_E_TMO)
        {
            dprintf("\nTIMEOUT\n");
        }
#endif
        return status;
    }

    /* Wait for complete*/
    if (   alt_sdmmc_is_dma_enabled() == ALT_E_FALSE
        || cmd_cfg->data_expected == false)
    {
        status = alt_sdmmc_cmd_waiter();
    }
#ifdef LOGGER
    if (status == ALT_E_TMO)
    {
        dprintf("\nTIMEOUT\n");
    }
    if (status == ALT_E_ERROR)
    {
        dprintf("alt_sdmmc_cmd_waiter ERROR\n");
    }
#endif
    if ((status == ALT_E_SUCCESS) && response)
    {
        alt_sdmmc_read_short_response(response);
    }
    
    return status;
}

static ALT_STATUS_CODE alt_sdmmc_transfer(ALT_SDMMC_CARD_INFO_t * card_info,
                                          uint32_t start_addr,
                                          uint32_t buffer[],
                                          const size_t buf_len,
                                          ALT_SDMMC_TMOD_t transfer_mode)
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;
    uint32_t block_count;
    uint32_t byte_count;
    uint16_t block_size;
    uint32_t cmd_index = 0;

    if (buf_len == 0)
    {
        return ALT_E_SUCCESS;
    }

    if (!alt_sdmmc_is_idle())
    {
        return ALT_E_ERROR;
    }

    block_size = alt_sdmmc_block_size_get();

    if (   (start_addr % block_size != 0) 
        || (buf_len    % block_size != 0))
    {
        return ALT_E_BAD_ARG;
    }

    /*  Number of block to transfer*/
    block_count = buf_len / block_size;
    /*  New count of reading byte*/
    byte_count = block_count * block_size;

    /*  reset FIFO*/
    status = alt_sdmmc_fifo_reset();
    if (status != ALT_E_SUCCESS)
    {
        return status;
    }
    
    /*  reset DMA*/
    if (alt_sdmmc_is_dma_enabled())
    {
        status = alt_sdmmc_dma_reset();
        if (status != ALT_E_SUCCESS)
        {
            return status;
        }
    }

    alt_sdmmc_byte_count_set(byte_count);
    alt_sdmmc_card_rd_threshold_enable(0x80);

    if (buf_len == block_size)
    {
        cmd_index = (transfer_mode == ALT_SDMMC_TMOD_READ) 
                                        ? ALT_SDMMC_READ_SINGLE_BLOCK 
                                        : ALT_SDMMC_WRITE_BLOCK;
    }
    else
    {
        cmd_index = (transfer_mode == ALT_SDMMC_TMOD_READ) 
                                        ? ALT_SDMMC_READ_MULTIPLE_BLOCK 
                                        : ALT_SDMMC_WRITE_MULTIPLE_BLOCK;
    }

    if (alt_sdmmc_is_dma_enabled())
    {
        /*  Clean descriptor chain*/
        alt_sdmmc_desc_chain_clear();
        
        alt_sdmmc_dma_start(dma_cur_descr, 0x0,
                            ALT_SDMMC_DMA_PBL_1, false);
        /* Enable all dma interrupt status*/
        alt_sdmmc_dma_int_enable(ALT_SDMMC_DMA_INT_STATUS_ALL);
    }

#ifdef DMA_LOGGER
    dprintf("\nstart_addr = %d\n", (int)start_addr);
#endif

    /* Send transfer command*/

    if (card_info->card_type == ALT_SDMMC_CARD_TYPE_SDHC)
    {
        status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, (ALT_SDMMC_CMD_INDEX_t)cmd_index, start_addr / block_size, NULL);
    }
    else
    {
        status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, (ALT_SDMMC_CMD_INDEX_t)cmd_index, start_addr, NULL);
    }

    if (status != ALT_E_SUCCESS)
    {
        return status;
    }

    /* Send or read data*/
    if (alt_sdmmc_is_dma_enabled())
    {
        /* Fill descriptors*/
        status = alt_sdmmc_dma_trans_helper(buffer, byte_count);
    }
    else
    {
        status = alt_sdmmc_transfer_helper(buffer, byte_count, transfer_mode);
    }

    if (status != ALT_E_SUCCESS)
    {
        return status;
    }

    /* Wait for data transfer complete*/
    status = alt_sdmmc_data_done_waiter();

    return status;
}

/*
// This function performs SDMMC write.
*/
ALT_STATUS_CODE alt_sdmmc_write(ALT_SDMMC_CARD_INFO_t * card_info, void *dest, void *src, const size_t size)
{
    return alt_sdmmc_transfer(card_info, (uint32_t)dest, src, size, ALT_SDMMC_TMOD_WRITE);
}

/*
// This function performs SDMMC read.
*/
ALT_STATUS_CODE alt_sdmmc_read(ALT_SDMMC_CARD_INFO_t * card_info, void *dest, void *src, const size_t size)
{
    return alt_sdmmc_transfer(card_info, (uint32_t)src, dest, size, ALT_SDMMC_TMOD_READ);
}

/*
// Send CMD6 switch to card and get the response and status
*/
static ALT_STATUS_CODE alt_sdmmc_card_switch_function(uint32_t function_arg, uint8_t * switch_status)
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;
#ifdef LOGGER
    uint32_t response = 0;
    uint32_t i;
#endif

    uint16_t prev_blk_size = 0;
    /*  Save current block size and change it*/
    prev_blk_size = alt_sdmmc_block_size_get();

    /*  block_size 64 bytes, internally predefined, no need to send CMD16 SET_BLOCKLEN*/
    alt_sdmmc_block_size_set(64);

    /*  bytecnt in bytes*/
    alt_sdmmc_byte_count_set(64);

    /*  reset FIFO*/
    status = alt_sdmmc_fifo_reset();
    if (status != ALT_E_SUCCESS)
    {
        return status;
    }

    /*  reset DMA*/
    if (alt_sdmmc_is_dma_enabled())
    {
        status = alt_sdmmc_dma_reset();
        if (status != ALT_E_SUCCESS)
        {
            return status;
        }
    }

    /* alt_sdmmc_card_rd_threshold_enable(0x80);*/

    if (alt_sdmmc_is_dma_enabled())
    {
        /*  Clean descriptor chain*/
        alt_sdmmc_desc_chain_clear();

        alt_sdmmc_dma_start(dma_cur_descr, 0x0,
                            ALT_SDMMC_DMA_PBL_1, false);
        /* Enable all dma interrupt status*/
        alt_sdmmc_dma_int_enable(ALT_SDMMC_DMA_INT_STATUS_ALL);
    }

    /* Send transfer command*/
    status = alt_sdmmc_command_send(ALT_SDMMC_CMD_TYPE_BASIC, ALT_SDMMC_SWITCH, function_arg, NULL);
    #ifdef LOGGER
    alt_sdmmc_read_short_response(&response);
    dprintf("\nALT_SDMMC_SWITCH response = %x\n", (int)response);
    #endif
    if (status != ALT_E_SUCCESS)
    {
        return status;
    }

    /* Send or read data*/
    if (alt_sdmmc_is_dma_enabled())
    {
        /* Fill descriptors*/
        status = alt_sdmmc_dma_trans_helper((uint32_t*)switch_status, 64);
    }
    else
    {
        status = alt_sdmmc_transfer_helper((uint32_t*)switch_status, 64, ALT_SDMMC_TMOD_READ);
    }

    if (status != ALT_E_SUCCESS)
    {
        return status;
    }
    #ifdef LOGGER
    dprintf("Switch function status\n");
    for (i = 0; i < 64; i ++)
    {
        dprintf("B[%d] = %x\t", i, switch_status[i]);
        if (i % 8 == 7)
            dprintf("\n");
    }
    #endif
    /* Wait for data transfer complete*/
    status = alt_sdmmc_data_done_waiter();
    if (status != ALT_E_SUCCESS)
    {
        dprintf("alt_sdmmc_data_done_waiter failed\n");
        return status;
    }

    /*  Restore block size*/
    alt_sdmmc_block_size_set(prev_blk_size);
    return status;
}

__attribute__((weak)) ALT_STATUS_CODE alt_cache_system_purge(__attribute__((unused))void * address,__attribute__((unused)) size_t length)
{
    return ALT_E_SUCCESS;
}

#if defined (soc_a10)
ALT_STATUS_CODE alt_sdmmc_ecc_start(void)
{
    int i = 0;

    /*Is the SDMMC Card Powered On? */
    if (alt_sdmmc_card_pwr_is_on() == false)
    {
        dprintf("ECC[start][SDMMC]: Card is not powered.\n");
        return ALT_E_ERROR;
    }

    /* Start ECC memory initialization and wait for it to complete.
     * NOTE: This needs to be done before enabling ECC.
     */
    alt_write_word(ALT_ECC_SDMMC_CTL_ADDR, ALT_ECC_SDMMC_CTL_INITA_SET_MSK);

    while (!(alt_read_word(ALT_ECC_SDMMC_INITSTAT_ADDR) &
            (ALT_ECC_SDMMC_INITSTAT_INITCOMPLETEA_SET_MSK)))
    {
        ++i;
        if (i == 100000)
        {
            dprintf("ECC[start][SDMMC][A]: Timeout waiting for HW init.\n");
            return ALT_E_TMO;
        }
    }
    dprintf("ECC[start][SDMMC][A]: i = %d.\n", i);

    i = 0;

    /* Start ECC memory initialization and wait for it to complete.
     * NOTE: This needs to be done before enabling ECC.
     */
    alt_write_word(ALT_ECC_SDMMC_CTL_ADDR, ALT_ECC_SDMMC_CTL_INITB_SET_MSK);

    while (!(alt_read_word(ALT_ECC_SDMMC_INITSTAT_ADDR) &
            (ALT_ECC_SDMMC_INITSTAT_INITCOMPLETEB_SET_MSK)))
    {
        ++i;
        if (i == 100000)
        {
            dprintf("ECC[start][SDMMC][B]: Timeout waiting for HW init.\n");
            return ALT_E_TMO;
        }
    }
    dprintf("ECC[start][SDMMC][B]: i = %d.\n", i);

    /* INITCOMPLETE needs to be reset to 0 after init to complete state machine */
    alt_write_word(ALT_ECC_SDMMC_INITSTAT_ADDR, ALT_ECC_SDMMC_INITSTAT_INITCOMPLETEA_RESET);
    alt_write_word(ALT_ECC_SDMMC_INITSTAT_ADDR, ALT_ECC_SDMMC_INITSTAT_INITCOMPLETEB_RESET);

    /* 
     * Enable ECC on SDMMC.
     */
    alt_write_word(ALT_ECC_SDMMC_CTL_ADDR, ALT_ECC_SDMMC_CTL_ECC_EN_SET_MSK);

    return ALT_E_SUCCESS;
}


ALT_STATUS_CODE alt_sdmmc_ecc_get_correct_data(uint32_t address, uint32_t * data)
{
    ALT_STATUS_CODE status = ALT_E_SUCCESS;
    
    /* Enable Data Byte control */
    alt_write_word(ALT_ECC_SDMMC_ECC_DBYTECTL_ADDR, ALT_ECC_SDMMC_DBYTECTL_DBEN_SET_MSK);

    /* Set the Data and ECC override access control for read and start access. */
    alt_write_word(ALT_ECC_SDMMC_ECC_ACCCTL_ADDR,
                     ALT_ECC_SDMMC_ACCCTL_DATAOVR_SET_MSK
                   | ALT_ECC_SDMMC_ACCCTL_ECCOVR_SET_MSK);

    /* Read the ECC corrected data */
    status = alt_write_word(ALT_ECC_SDMMC_ECC_ADDRBUS_ADDR, address);
    *data = alt_read_word(ALT_ECC_SDMMC_ECC_RDATA0BUS_ADDR);

    return status;
}
#endif
