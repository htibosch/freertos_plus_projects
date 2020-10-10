/**
 * \file
 *
 * \brief Analog-Front-End Controller driver for SAM.
 *
 * Copyright (c) 2011-2012 Atmel Corporation. All rights reserved.
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

#ifndef AFEC_H_INCLUDED
#define AFEC_H_INCLUDED

#include "compiler.h"

/// @cond 0
/**INDENT-OFF**/
#ifdef __cplusplus
extern "C" {
#endif
/**INDENT-ON**/
/// @endcond

/* The max afec sample freq definition*/
#define AFEC_FREQ_MAX   20000000
/* The min afec sample freq definition*/
#define AFEC_FREQ_MIN    1000000
/* The normal afec startup time*/
#define AFEC_STARTUP_NORM     40
/* The fast afec startup time*/
#define AFEC_STARTUP_FAST     12

/* Definitions for AFEC resolution */
enum afec_resolution_t {
	AFEC_10_BITS = AFEC_EMR_RES_LOW_RES,       /* AFEC 10-bit resolution */
	AFEC_12_BITS = AFEC_EMR_RES_NO_AVERAGE     /* AFEC 12-bit resolution */
};

/* Definitions for AFEC trigger */
enum afec_trigger_t {
	AFEC_TRIG_SW = AFEC_MR_TRGEN_DIS,  /* Starting a conversion is only possible by software. */
	AFEC_TRIG_EXT = ((AFEC_MR_TRGSEL_AFEC_TRIG0 << AFEC_MR_TRGSEL_Pos) &
									AFEC_MR_TRGSEL_Msk) | AFEC_MR_TRGEN,  /* External trigger */
	AFEC_TRIG_TIO_CH_0 = (AFEC_MR_TRGSEL_AFEC_TRIG1  & AFEC_MR_TRGSEL_Msk) |
											AFEC_MR_TRGEN,  /* TIO Output of the Timer Counter Channel 0 */
	AFEC_TRIG_TIO_CH_1 = (AFEC_MR_TRGSEL_AFEC_TRIG2  & AFEC_MR_TRGSEL_Msk) |
											AFEC_MR_TRGEN,  /* TIO Output of the Timer Counter Channel 1 */
	AFEC_TRIG_TIO_CH_2 = (AFEC_MR_TRGSEL_AFEC_TRIG3  & AFEC_MR_TRGSEL_Msk) |
											AFEC_MR_TRGEN,  /* TIO Output of the Timer Counter Channel 2 */
	AFEC_TRIG_PWM_EVENT_LINE_0 = (AFEC_MR_TRGSEL_AFEC_TRIG4  & AFEC_MR_TRGSEL_Msk) |
															AFEC_MR_TRGEN, /* PWM Event Line 0 */
	AFEC_TRIG_PWM_EVENT_LINE_1 = (AFEC_MR_TRGSEL_AFEC_TRIG5  & AFEC_MR_TRGSEL_Msk) |
															AFEC_MR_TRGEN  /* PWM Event Line 1 */
} ;

/* Definitions for AFEC channel number */
enum afec_channel_num_t {
	AFEC_CHANNEL_0  = 0,
	AFEC_CHANNEL_1  = 1,
	AFEC_CHANNEL_2  = 2,
	AFEC_CHANNEL_3  = 3,
	AFEC_CHANNEL_4  = 4,
	AFEC_CHANNEL_5  = 5,
	AFEC_CHANNEL_6  = 6,
	AFEC_CHANNEL_7  = 7,
	AFEC_CHANNEL_8  = 8,
	AFEC_CHANNEL_9  = 9,
	AFEC_CHANNEL_10 = 10,
	AFEC_CHANNEL_11 = 11,
	AFEC_CHANNEL_12 = 12,
	AFEC_CHANNEL_13 = 13,
	AFEC_CHANNEL_14 = 14,
	AFEC_TEMPERATURE_SENSOR = 15,
} ;

/* Definitions for AFEC DAC offset */
enum afec_dacc_offset_t {
	AFEC_DAC_OFFSET_512  = 512,
	AFEC_DAC_OFFSET_1024  = 1024,
	AFEC_DAC_OFFSET_2048  = 2048,
} ;

/* Definitions for AFEC gain value */
enum afec_gainvalue_t{
	AFEC_GAINVALUE_0 = 0,
	AFEC_GAINVALUE_1 = 1,
	AFEC_GAINVALUE_2 = 2,
	AFEC_GAINVALUE_3 = 3
};

/* Definitions for AFEC analog settling time */
enum afec_settling_time_t{
	AFEC_SETTLING_TIME_0 = AFEC_MR_SETTLING_AST3,
	AFEC_SETTLING_TIME_1 = AFEC_MR_SETTLING_AST5,
	AFEC_SETTLING_TIME_2 = AFEC_MR_SETTLING_AST9,
	AFEC_SETTLING_TIME_3 = AFEC_MR_SETTLING_AST17
};

uint32_t afec_init(Afec *p_afec, const uint32_t ul_mck,
                            const uint32_t ul_afec_clock, const uint8_t uc_startup);
void afec_configure_trigger(Afec *p_afec, const enum afec_trigger_t trigger,
				const uint8_t uc_freerun);
void afec_configure_power_save(Afec *p_afec, const uint8_t uc_sleep, const uint8_t uc_fwup);
void afec_configure_sequence(Afec *p_afec, const enum afec_channel_num_t ch_list[],
                                                               const uint8_t uc_num);
void afec_enable_tag(Afec *p_afec);
void afec_disable_tag(Afec *p_afec);
enum afec_channel_num_t afec_get_tag(const Afec *p_afec);
void afec_start_sequencer(Afec *p_afec);
void afec_stop_sequencer(Afec *p_afec);
void afec_set_comparison_mode(Afec *p_afec, const uint8_t uc_mode);
uint32_t afec_get_comparison_mode(const Afec *p_afec);
void afec_set_comparison_window(Afec *p_afec, const uint16_t us_low_threshold,
                                                                        const uint16_t us_high_threshold);
void afec_set_comparison_channel(Afec *p_afec, const enum afec_channel_num_t channel);
void afec_set_writeprotect(Afec *p_afec, const uint32_t ul_enable);
uint32_t afec_get_writeprotect_status(const Afec *p_afec);
void afec_check(Afec* p_afec, const uint32_t ul_mck);
uint32_t afec_get_overrun_status(const Afec *p_afec);

void afec_set_resolution(Afec *p_afec, const enum afec_resolution_t resolution);
void afec_start(Afec *p_afec);
void afec_stop(Afec *p_afec);
void afec_enable_channel(Afec *p_afec, const enum afec_channel_num_t afec_ch);
void afec_disable_channel(Afec *p_afec, const enum afec_channel_num_t afec_ch);
void afec_enable_all_channel(Afec *p_afec);
void afec_disable_all_channel(Afec *p_afec);
uint32_t afec_get_channel_status(const Afec *p_afec, const enum afec_channel_num_t afec_ch);
uint32_t afec_get_channel_value(Afec *p_afec,const  enum afec_channel_num_t afec_ch);
uint32_t afec_get_latest_value(const Afec *p_afec);
uint32_t afec_get_actual_afec_clock(const Afec *p_afec, const uint32_t ul_mck);
void afec_enable_interrupt(Afec *p_afec, const uint32_t ul_source);
void afec_disable_interrupt(Afec *p_afec, const uint32_t ul_source);
uint32_t afec_get_status(const Afec *p_afec);
uint32_t afec_get_interrupt_mask(const Afec *p_afec);
Pdc *afec_get_pdc_base(const Afec *p_afec);

void afec_configure_timing(Afec *p_afec, const uint8_t uc_tracking,
                                         const enum afec_settling_time_t settling, const uint8_t uc_transfer);
void afec_enable_anch( Afec *p_afec );
void afec_disable_anch( Afec *p_afec );
void afec_enable_channel_differential_input(Afec *p_afec, const enum afec_channel_num_t channel);
void afec_disable_channel_differential_input(Afec *p_afec, const enum afec_channel_num_t channel);
void afec_enable_channel_input_offset(Afec *p_afec, const enum afec_channel_num_t channel);
void afec_disable_channel_input_offset(Afec *p_afec, const enum afec_channel_num_t channel);
void afec_set_channel_dac_offset(Afec *p_afec, const enum afec_dacc_offset_t offset);
void afec_set_channel_input_gain(Afec *p_afec, const enum afec_channel_num_t channel,
                                                       const enum afec_gainvalue_t uc_gain);
void afec_set_bias_current(Afec *p_afec, const uint8_t uc_ibctl);
void afec_enable_ts(void);
void afec_disable_ts(void);

void afec_set_calibmode(Afec *p_afec);

/// @cond 0
/**INDENT-OFF**/
#ifdef __cplusplus
}
#endif
/**INDENT-ON**/
/// @endcond

/**
 * \page sam_afec_quickstart Quickstart guide for SAM AFEC driver
 *
 * This is the quickstart guide for the \ref afec_group "SAM AFEC driver",
 * with step-by-step instructions on how to configure and use the driver in a
 * selection of use cases.
 *
 * The use cases contain several code fragments. The code fragments in the
 * steps for setup can be copied into a custom initialization function, while
 * the steps for usage can be copied into, e.g., the main application function.
 *
 * \section afec_basic_use_case Basic use case
 * In this basic use case, the AFEC module and single channel are configured for:
 * - 12-bit, unsigned conversions
 * - Internal bandgap as 3.3 V reference
 * - AFEC clock rate of at most 6.4 MHz and maximum sample rate is 1 MHz
 * - Software triggering of conversions
 * - Interrupt-based conversion handling
 * - Single channel measurement
 * - AFEC_CHANNEL_5 as positive input
 *
 * \subsection sam_afec_quickstart_prereq Prerequisites
 * -# \ref sysclk_group "System Clock Management (Sysclock)"
 * -# \ref pmc_group "Power Management Controller (PMC)"
 *
 * \section afec_basic_use_case_setup Setup steps
 * \subsection afec_basic_use_case_setup_code Example code
 * Add to application C-file:
 * \code
 *   void AFEC_IrqHandler(void)
 *   {
 *       // Check the AFEC conversion status
 *       if ((afec_get_status(AFEC).isr_status & AFEC_ISR_DRDY) ==	AFEC_ISR_DRDY)
 *       {
 *       // Get latest digital data value from AFEC and can be used by application
 *           uint32_t result = afec_get_latest_value(AFEC);
 *       }
 *   }
 *   void afec_setup(void)
 *   {
 *       afec_init(AFEC, sysclk_get_main_hz(), AFEC_CLOCK, 8);
 *
 *       afec_configure_timing(AFEC, 0, AFEC_SETTLING_TIME_3, 1);
 *
 *       afec_set_resolution(AFEC, AFEC_MR_LOWRES_BITS_12);
 *
 *       afec_enable_channel(AFEC, AFEC_CHANNEL_5);
 *
 *       afec_enable_interrupt(AFEC, AFEC_IER_DRDY);
 *
 *       afec_configure_trigger(AFEC, AFEC_TRIG_SW, 0);
 *   }
 * \endcode
 *
 * \subsection afec_basic_use_case_setup_flow Workflow
 * -# Define the interrupt service handler in the application:
 *   - \code
 *   void AFEC_IrqHandler(void)
 *   {
 *       //Check the AFEC conversion status
 *       if ((afec_get_status(AFEC).isr_status & AFEC_ISR_DRDY) ==	AFEC_ISR_DRDY)
 *       {
 *       //Get latest digital data value from AFEC and can be used by application
 *           uint32_t result = afec_get_latest_value(AFEC);
 *       }
 *   }
 * \endcode
 *   - \note Get AFEC status and check if the conversion is finished. If done, read the last AFEC result data.
 * -# Initialize the given AFEC with the specified AFEC clock and startup time:
 *   - \code afec_init(AFEC, sysclk_get_main_hz(), AFEC_CLOCK, 8); \endcode
 *   - \note The AFEC clock range is between master clock / 2 and master clock / 512.
 * The function sysclk_get_main_hz() is used to get the master clock frequency while AFEC_CLOCK gives the AFEC clock frequency.
 * -# Configure AFEC timing:
 *   - \code afec_configure_timing(AFEC, 0, AFEC_SETTLING_TIME_3, 1); \endcode
 *   - \note Tracking Time = (0 + 1) * AFEC Clock period
 * Settling Time =  AFEC_SETTLING_TIME_3 * AFEC Clock period
 * Transfer Time = (1 * 2 + 3) * AFEC Clock period
 * -# Set the AFEC resolution.
 *   - \code afec_set_resolution(AFEC, AFEC_MR_LOWRES_BITS_12); \endcode
 *   - \note The resolution value can be set to 10 bits or 12 bits.
 * -# Enable the specified AFEC channel:
 *   - \code afec_enable_channel(AFEC, AFEC_CHANNEL_5); \endcode
 * -# Enable AFEC interrupts:
 *   - \code afec_enable_interrupt(AFEC, AFEC_IER_DRDY); \endcode
 * -# Configure software conversion trigger:
 *   - \code afec_configure_trigger(AFEC, AFEC_TRIG_SW, 0); \endcode
 *
 * \section afec_basic_use_case_usage Usage steps
 * \subsection afec_basic_use_case_usage_code Example code
 * Add to, e.g., main loop in application C-file:
 * \code
 *    afec_start(AFEC);
 * \endcode
 *
 * \subsection afec_basic_use_case_usage_flow Workflow
 * -# Start AFEC conversion on channel:
 *   - \code afec_start(AFEC); \endcode
 *
 * \section afec_use_cases Advanced use cases
 * For more advanced use of the AFEC driver, see the following use cases:
 * - \subpage afec_use_case_1 : 12-bits unsigned, comparison event happen and interrupt
 * driven
 */
/**
 * \page afec_use_case_1 Use case #1
 * In this use case the AFEC module and one channel are configured for:
 * - 12-bit, unsigned conversions
 * - Internal bandgap as 3.3 V reference
 * - AFEC clock rate of at most 6.4 MHz and maximum sample rate is 1 MHz
 * - Software triggering of conversions
 * - Comparison event happen and interrupt handling
 * - Single channel measurement
 * - AFEC_CHANNEL_5 as positive input
 *
 * \section afec_use_case_1_setup Setup steps
 * \subsection afec_use_case_1_setup_code Example code
 * Add to application C-file:
 * \code
 *   void AFEC_IrqHandler(void)
 *   {
 *       // Check the AFEC conversion status
 *       if ((afec_get_status(AFEC).isr_status & AFEC_ISR_COMPE) == AFEC_ISR_COMPE)
 *       {
 *           // Get comparison mode of AFEC
 *           uint32_t ul_mode = afec_get_comparison_mode(AFEC);
 *           // Get latest digital data value from AFEC and can be used by application
 *           uint16_t us_afec = afec_get_channel_value(AFEC, AFEC_CHANNEL_5);
 *       }
 *   }
 * \endcode
 *
 * \code
 *   void afec_setup(void)
 *   {
 *       afec_init(AFEC, sysclk_get_main_hz(), AFEC_CLOCK, 8);
 *
 *       afec_configure_timing(AFEC, 0, AFEC_SETTLING_TIME_3, 1);
 *
 *       afec_set_resolution(AFEC, AFEC_MR_LOWRES_BITS_12);
 *
 *       afec_enable_channel(AFEC, AFEC_CHANNEL_5);
 *
 *       afec_set_comparison_channel(AFEC, AFEC_CHANNEL_5);
 *       afec_set_comparison_mode(AFEC, AFEC_EMR_CMPMODE_IN);
 *       afec_set_comparison_window(AFEC, MAX_DIGITAL, 0);
 *
 *       afec_enable_interrupt(AFEC, AFEC_IER_COMPE);
 *
 *       afec_configure_trigger(AFEC, AFEC_TRIG_TIO_CH_0, 0);
 *   }
 * \endcode
 *
 * \subsection afec_basic_use_case_setup_flow Workflow
 * -# Define the interrupt service handler in the application:
 *   - \code
 *   void AFEC_IrqHandler(void)
 *   {
 *       // Check the AFEC conversion status
 *       if ((afec_get_status(AFEC).isr_status & AFEC_ISR_COMPE) == AFEC_ISR_COMPE)
 *       {
 *           // Get comparison mode of AFEC
 *           uint32_t ul_mode = afec_get_comparison_mode(AFEC);
 *           // Get latest digital data value from AFEC and can be used by application
 *           uint16_t us_afec = afec_get_channel_value(AFEC, AFEC_CHANNEL_5);
 *       }
 *   }
 * \endcode
 *   - \note Get AFEC status and check if comparison event occurred. If occurred, read the AFEC channel value and comparison mode.
 * -# Initialize the given AFEC with the specified AFEC clock and startup time:
 *   - \code afec_init(AFEC, sysclk_get_main_hz(), AFEC_CLOCK, 10); \endcode
 *   - \note The AFEC clock range is between master clock/2 and master clock/512.
 * The function sysclk_get_main_hz() is used to get the master clock frequency while AFEC_CLOCK gives the AFEC clock frequency.
 * -# Configure AFEC timing:
 *   - \code afec_configure_timing(AFEC, 0, AFEC_SETTLING_TIME_3, 1); \endcode
 *   - \note Tracking Time = (0 + 1) * AFEC Clock period
 * Settling Time =  AFEC_SETTLING_TIME_3 * AFEC Clock period
 * Transfer Time = (1 * 2 + 3) * AFEC Clock period
 * -# Set the AFEC resolution.
 *   - \code afec_set_resolution(AFEC, AFEC_MR_LOWRES_BITS_12); \endcode
 *   - \note The resolution value can be set to 10 bits or 12 bits.
 * -# Enable the specified AFEC channel:
 *   - \code afec_enable_channel(AFEC, AFEC_CHANNEL_5); \endcode
 * -# Set the comparison AFEC channel, mode and window:
 *   - \code afec_set_comparison_channel(AFEC, AFEC_CHANNEL_5);
 * afec_set_comparison_mode(AFEC, AFEC_EMR_CMPMODE_IN);
 * afec_set_comparison_window(AFEC, us_high_threshold, us_low_threshold); \endcode
 *   - \note The high and low threshold of comparison window can be set by the user.
 * An event will be generated whenever the converted data is in the comparison window.
 * -# Enable AFEC interrupts:
 *   - \code afec_enable_interrupt(AFEC, AFEC_IER_COMPE); \endcode
 * -# Configure software conversion trigger:
 *   - \code afec_configure_trigger(AFEC, AFEC_TRIG_SW, 0); \endcode
 *
 * \section afec_use_case_1_usage Usage steps
 * \subsection afec_use_case_1_usage_code Example code
 * Add to, e.g., main loop in application C-file:
 * \code
 *    afec_start(AFEC);
 * \endcode
 *
 * \subsection afec_use_case_1_usage_flow Workflow
 * -# Start AFEC conversion on the configured channels:
 *   - \code afec_start(AFEC); \endcode
 */
#endif /* AFEC_H_INCLUDED */
