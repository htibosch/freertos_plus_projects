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


#include "sam4e.h"
#include "afec.h"

/// @cond 0
/**INDENT-OFF**/
#ifdef __cplusplus
extern "C" {
#endif
/**INDENT-ON**/
/// @endcond

/**
 * \defgroup sam_drivers_afec_group Analog-Front-End Controller (AFEC)
 *
 * See \ref sam_afec_quickstart.
 *
 * Driver for the Analog-Front-End Controller. This driver provides access to the main
 * features of the AFEC controller.
 *
 * @{
 */

#define AFEC_WPMR_WPKEY(value) ((AFEC_WPMR_WPKEY_Msk & ((value) << AFEC_WPMR_WPKEY_Pos)))

/**
 * \brief Initialize the given AFEC with the specified AFEC clock and startup time.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param ul_mck Main clock of the device (value in Hz).
 * \param ul_afec_clock Analog-to-Digital conversion clock (value in Hz).
 * \param uc_startup AFEC start up time. Please refer to the product datasheet
 * for details.
 *
 * \return 0 on success.
 */
uint32_t afec_init(Afec *p_afec, const uint32_t ul_mck,
		const uint32_t ul_afec_clock, const uint8_t uc_startup)
{
	uint32_t ul_prescal;

	/*  Reset the controller. */
	p_afec->AFEC_CR = AFEC_CR_SWRST;

	/* Reset Mode Register. */
	p_afec->AFEC_MR = 0;

	/* Reset PDC transfer. */
	p_afec->AFEC_PTCR = (AFEC_PTCR_RXTDIS | AFEC_PTCR_TXTDIS);
	p_afec->AFEC_RCR = 0;
	p_afec->AFEC_RNCR = 0;

	ul_prescal = ul_mck / (2 * ul_afec_clock) - 1;
	p_afec->AFEC_MR |= AFEC_MR_PRESCAL(ul_prescal) |
			((uc_startup << AFEC_MR_STARTUP_Pos) &
			AFEC_MR_STARTUP_Msk);
	return 0;
}

/**
 * \brief Configure the conversion resolution.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param resolution AFEC resolution.
 *
 */
void afec_set_resolution(Afec *p_afec, const enum afec_resolution_t resolution)
{
	p_afec->AFEC_EMR |= resolution & AFEC_EMR_RES_Msk;
}

/**
 * \brief Configure conversion trigger and free run mode.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param trigger Conversion trigger.
 * \param uc_freerun AFEC_MR_FREERUN_ON enables freerun mode,
 * AFEC_MR_FREERUN_OFF disables freerun mode.
 *
 */
void afec_configure_trigger(Afec *p_afec, const enum afec_trigger_t trigger,
		uint8_t uc_freerun)
{
	p_afec->AFEC_MR |= trigger | ((uc_freerun << 7) & AFEC_MR_FREERUN);
}

/**
 * \brief Configures AFEC power saving mode.
 *
 * \param p_afec Pointer to an AFEC instanSce.
 * \param uc_sleep AFEC_MR_SLEEP_NORMAL keeps the AFEC Core and reference voltage
 * circuitry ON between conversions.
 * AFEC_MR_SLEEP_SLEEP keeps the AFEC Core and reference voltage circuitry OFF
 * between conversions.
 * \param uc_fwup AFEC_MR_FWUP_OFF configures sleep mode as uc_sleep setting,
 * AFEC_MR_FWUP_ON keeps voltage reference ON and AFEC Core OFF between conversions.
 */
void afec_configure_power_save(Afec *p_afec, const uint8_t uc_sleep, const uint8_t uc_fwup)
{
	p_afec->AFEC_MR |= (((uc_sleep << 5) & AFEC_MR_SLEEP) |
			((uc_fwup << 6) & AFEC_MR_FWUP));
}

/**
 * \brief Configure conversion sequence.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param ch_list Channel sequence list.
 * \param number Number of channels in the list.
 */
void afec_configure_sequence(Afec *p_afec, const enum afec_channel_num_t ch_list[],
		uint8_t uc_num)
{
	uint8_t uc_counter;
	if (uc_num < 8) {
		for (uc_counter = 0; uc_counter < uc_num; uc_counter++) {
			p_afec->AFEC_SEQ1R |=
					ch_list[uc_counter] << (4 * uc_counter);
		}
	} else {
		for (uc_counter = 0; uc_counter < 8; uc_counter++) {
			p_afec->AFEC_SEQ1R |=
					ch_list[uc_counter] << (4 * uc_counter);
		}
		for (uc_counter = 0; uc_counter < uc_num - 8; uc_counter++) {
			p_afec->AFEC_SEQ2R |=
					ch_list[uc_counter] << (4 * uc_counter);
		}
	}
}

/**
 * \brief Configure AFEC timing.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param uc_tracking AFEC tracking time = uc_tracking / AFEC clock.
 * \param uc_settling Analog settling time = (uc_settling + 1) / AFEC clock.
 * \param uc_transfer Data transfer time = (uc_transfer * 2 + 3) / AFEC clock.
 */
void afec_configure_timing(Afec *p_afec, const uint8_t uc_tracking,
		const enum afec_settling_time_t settling,const uint8_t uc_transfer)
{
	p_afec->AFEC_MR |= AFEC_MR_TRANSFER(uc_transfer)
			| settling | AFEC_MR_TRACKTIM(uc_tracking);
}

/**
 * \brief Enable analog change.
 *
 * \note It allows different analog settings for each channel.
 *
 * \param p_Afec Pointer to an AFEC instance.
 */
void afec_enable_anch(Afec *p_afec)
{
	p_afec->AFEC_MR |= AFEC_MR_ANACH;
}

/**
 * \brief Disable analog change.
 *
 * \note DIFF0, GAIN0 and OFF0 are used for all channels.
 *
 * \param p_Afec Pointer to an AFEC instance.
 */
void afec_disable_anch(Afec *p_afec)
{
	p_afec->AFEC_MR &= ~AFEC_MR_ANACH;
}

/**
 * \brief Start analog-to-digital conversion.
 *
 * \note If one of the hardware event is selected as AFEC trigger,
 * this function can NOT start analog to digital conversion.
 *
 * \param p_afec Pointer to an AFEC instance.
 */

void afec_start(Afec *p_afec)
{
	p_afec->AFEC_CR = AFEC_CR_START;
}

/**
 * \brief Stop analog-to-digital conversion.
 *
 * \param p_afec Pointer to an AFEC instance.
 */
void afec_stop(Afec *p_afec)
{
	p_afec->AFEC_CR = AFEC_CR_SWRST;
}

/**
 * \brief Enable the specified AFEC channel.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param afec_ch AFEC channel number.
 */
void afec_enable_channel(Afec *p_afec, const enum afec_channel_num_t afec_ch)
{
	p_afec->AFEC_CHER = 1 << afec_ch;
}

/**
 * \brief Enable all AFEC channels.
 *
 * \param p_afec Pointer to an AFEC instance.
 */
void afec_enable_all_channel(Afec *p_afec)
{
	p_afec->AFEC_CHER = 0xFFFF;
}

/**
 * \brief Disable the specified AFEC channel.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param afec_ch AFEC channel number.
 */
void afec_disable_channel(Afec *p_afec, const enum afec_channel_num_t afec_ch)
{
	p_afec->AFEC_CHDR = 1 << afec_ch;
}

/**
 * \brief Disable all AFEC channel.
 *
 * \param p_afec Pointer to an AFEC instance.
 */
void afec_disable_all_channel(Afec *p_afec)
{
	p_afec->AFEC_CHDR = 0xFFFF;
}

/**
 * \brief Read the AFEC channel status.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param afec_ch AFEC channel number.
 *
 * \retval 1 if channel is enabled.
 * \retval 0 if channel is disabled.
 */
uint32_t afec_get_channel_status(const Afec *p_afec, const enum afec_channel_num_t afec_ch)
{
	return p_afec->AFEC_CHSR & (1 << afec_ch);
}

/**
 * \brief Read the AFEC result data of the specified channel.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param afec_ch AFEC channel number.
 *
 * \return AFEC value of the specified channel.
 */
uint32_t afec_get_channel_value(Afec *p_afec, const enum afec_channel_num_t afec_ch)
{
	uint32_t ul_data = 0;

	/* EOCx: End of Conversion x :
	This flag is cleared when reading the 
	AFEC_CDR register with the AFEC_CSELR register programmed with CSEL=x. */
	if (15 >= afec_ch) {
	  	p_afec->AFEC_CSELR = afec_ch;
		ul_data = p_afec->AFEC_CDR;
	}

	return ul_data;
}

/**
 * \brief Read the last AFEC result data.
 *
 * \param p_afec Pointer to an AFEC instance.
 *
 * \return AFEC latest value.
 */
uint32_t afec_get_latest_value(const Afec *p_afec)
{
	return p_afec->AFEC_LCDR;
}

/**
 * \brief Enable TAG option so that the number of the last converted channel
 * can be indicated.
 *
 * \param p_afec Pointer to an AFEC instance.
 */
void afec_enable_tag(Afec *p_afec)
{
	p_afec->AFEC_EMR |= AFEC_EMR_TAG;
}

/**
 * \brief Disable TAG option.
 *
 * \param p_afec Pointer to an AFEC instance.
 */
void afec_disable_tag(Afec *p_afec)
{
	p_afec->AFEC_EMR &= ~AFEC_EMR_TAG;
}

/**
 * \brief Indicate the last converted channel.
 *
 * \note If TAG option is NOT enabled before, an incorrect channel
 * number is returned.
 *
 * \param p_afec Pointer to an AFEC instance.
 *
 * \return The last converted channel number.
 */
enum afec_channel_num_t afec_get_tag(const Afec *p_afec)
{
	return (enum afec_channel_num_t)
			((p_afec->AFEC_LCDR & AFEC_LCDR_CHNB_Msk) >> AFEC_LCDR_CHNB_Pos);
}

/**
 * \brief Enable conversion sequencer.
 *
 * \param p_afec Pointer to an AFEC instance.
 */
void afec_start_sequencer(Afec *p_afec)
{
	p_afec->AFEC_MR |= AFEC_MR_USEQ;
}

/**
 * \brief Disable conversion sequencer.
 *
 * \param p_afec Pointer to an AFEC instance.
 */
void afec_stop_sequencer(Afec *p_afec)
{
	p_afec->AFEC_MR &= ~AFEC_MR_USEQ;
}

/**
 * \brief Configure comparison mode.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param uc_mode AFEC comparison mode.
 */
void afec_set_comparison_mode(Afec *p_afec, const uint8_t uc_mode)
{
	p_afec->AFEC_EMR &= (uint32_t) ~ (AFEC_EMR_CMPMODE_Msk);
	p_afec->AFEC_EMR |= (uc_mode & AFEC_EMR_CMPMODE_Msk);
}

/**
 * \brief Get comparison mode.
 *
 * \param p_afec Pointer to an AFEC instance.
 *
 * \retval Compare mode value.
 */
uint32_t afec_get_comparison_mode(const Afec *p_afec)
{
	return p_afec->AFEC_EMR & AFEC_EMR_CMPMODE_Msk;
}

/**
 * \brief Configure AFEC compare window.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param w_low_threshold Low threshold of compare window.
 * \param w_high_threshold High threshold of compare window.
 */
void afec_set_comparison_window(Afec *p_afec, const uint16_t us_low_threshold,
		const uint16_t us_high_threshold)
{
	p_afec->AFEC_CWR = AFEC_CWR_LOWTHRES(us_low_threshold) |
			AFEC_CWR_HIGHTHRES(us_high_threshold);
}

/**
 * \brief Configure comparison selected channel.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param channel AFEC channel number.
 */
void afec_set_comparison_channel(Afec *p_afec, const enum afec_channel_num_t channel)
{
	if (channel < 16) {
		p_afec->AFEC_EMR &= (uint32_t) ~ (AFEC_EMR_CMPALL);
		p_afec->AFEC_EMR &= (uint32_t) ~ (AFEC_EMR_CMPSEL_Msk);
		p_afec->AFEC_EMR |= (channel << AFEC_EMR_CMPSEL_Pos);
	} else {
		p_afec->AFEC_EMR |= AFEC_EMR_CMPALL;
	}
}

/**
 * \brief Enable differential input for the specified channel.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param channel AFEC channel number.
 */
void afec_enable_channel_differential_input(Afec *p_afec, const enum afec_channel_num_t channel)
{
	p_afec->AFEC_DIFFR |= 0x01u << channel;
}

/**
 * \brief Disable differential input for the specified channel.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param channel AFEC channel number.
 */
void afec_disable_channel_differential_input(Afec *p_afec, const enum afec_channel_num_t channel)
{
	p_afec->AFEC_DIFFR &= ~(1 << channel);
}

/**
 * \brief Enable analog signal offset for the specified channel.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param channel AFEC channel number.
 */
void afec_enable_channel_input_offset(Afec *p_afec, const enum afec_channel_num_t channel)
{
	p_afec->AFEC_CDOR |= 0x01u << channel;
}

/**
 * \brief Disable analog signal offset for the specified channel.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param channel AFEC channel number.
 */
void afec_disable_channel_input_offset(Afec *p_afec, const enum afec_channel_num_t channel)
{
	p_afec->AFEC_CDOR &= ~(1 << channel);
}

/**
 * \brief Set the DAC DC offset for the specified channel.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param offset AFEC DAC offset.
 */
void afec_set_channel_dac_offset(Afec *p_afec, const enum afec_dacc_offset_t offset)
{
	p_afec->AFEC_COCR = offset;
}

/**
 * \brief Configure input gain for the specified channel.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param channel AFEC channel number.
 * \param gain Gain value for the input.
 */
void afec_set_channel_input_gain(Afec *p_afec, const enum afec_channel_num_t channel,
		const enum afec_gainvalue_t gain)
{
	p_afec->AFEC_CGR |= (0x03u << (2 * channel)) & (gain << (2 * channel));
}

/**
 * \brief Set AFEC auto calibration mode.
 *
 * \param p_afec Pointer to an AFEC instance.
 */
void afec_set_calibmode(Afec * p_afec)
{
	p_afec->AFEC_CR |= AFEC_CR_AUTOCAL;
}

/**
 * \brief Return the actual AFEC clock.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param ul_mck Main clock of the device (in Hz).
 *
 * \return The actual AFEC clock (in Hz).
 */
uint32_t afec_get_actual_afec_clock(const Afec *p_afec, const uint32_t ul_mck)
{
	uint32_t ul_afecfreq;
	uint32_t ul_prescal;

	/* AFECClock = MCK / ( (PRESCAL+1) * 2 ) */
	ul_prescal = ((p_afec->AFEC_MR & AFEC_MR_PRESCAL_Msk) >> AFEC_MR_PRESCAL_Pos);
	ul_afecfreq = ul_mck / ((ul_prescal + 1) * 2);
	return ul_afecfreq;
}

/**
 * \brief Enable AFEC interrupts.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param ul_source Interrupts to be enabled.
 */
void afec_enable_interrupt(Afec *p_afec, const uint32_t ul_source)
{
	p_afec->AFEC_IER = ul_source;
}

/**
 * \brief Disable AFEC interrupts.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param ul_source Interrupts to be disabled.
 */
void afec_disable_interrupt(Afec *p_afec, const uint32_t ul_source)
{
	p_afec->AFEC_IDR = ul_source;
}

/**
 * \brief Get AFEC interrupt and overrun error status.
 *
 * \param p_afec Pointer to an AFEC instance.
 *
 * \return AFEC status structure.
 */
uint32_t afec_get_status(const Afec *p_afec)
{
	return p_afec->AFEC_ISR;
}

/**
 * \brief Get AFEC interrupt and overrun error status.
 *
 * \param p_afec Pointer to an AFEC instance.
 *
 * \return AFEC status structure.
 */
uint32_t afec_get_overrun_status(const Afec *p_afec)
{
	return p_afec->AFEC_OVER;
}

/**
 * \brief Read AFEC interrupt mask.
 *
 * \param p_afec Pointer to an AFEC instance.
 *
 * \return The interrupt mask value.
 */
uint32_t afec_get_interrupt_mask(const Afec *p_afec)
{
	return p_afec->AFEC_IMR;
}

/**
 * \brief Adapt performance versus power consumption.
 *
 * \note Please refer to AFEC Characteristics in the product datasheet
 * for more details.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param ibctl AFEC Bias current control.
 */
void afec_set_bias_current(Afec *p_afec, const uint8_t uc_ibctl)
{
	p_afec->AFEC_ACR |= AFEC_ACR_IBCTL(uc_ibctl);
}

/**
 * \brief Turn on temperature sensor.
 *
 * \param p_afec Pointer to an AFEC instance.
 */
void afec_enable_ts(void)
{
  	afec_enable_channel(AFEC0, AFEC_TEMPERATURE_SENSOR);
}

/**
 * \brief Turn off temperature sensor.
 *
 * \param p_afec Pointer to an AFEC instance.
 */
void afec_disable_ts(void)
{
  	afec_disable_channel(AFEC0, AFEC_TEMPERATURE_SENSOR);
}

/**
 * \brief Enable or disable write protection of AFEC registers.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param ul_enable 1 to enable, 0 to disable.
 */
void afec_set_writeprotect(Afec *p_afec, const uint32_t ul_enable)
{
	p_afec->AFEC_WPMR |= AFEC_WPMR_WPKEY(ul_enable);
}

/**
 * \brief Indicate write protect status.
 *
 * \param p_afec Pointer to an AFEC instance.
 *
 * \return 0 if the peripheral is not protected, or 16-bit write protect
 * violation Status.
 */
uint32_t afec_get_writeprotect_status(const Afec *p_afec)
{
	return p_afec->AFEC_WPSR & AFEC_WPSR_WPVS;
}

/**
 * \brief calcul_startup
 */
static uint32_t calcul_startup(const uint32_t ul_startup)
{
	uint32_t ul_startup_value = 0;

	if (ul_startup == 0)
		ul_startup_value = 0;
	else if (ul_startup == 1)
		ul_startup_value = 8;
	else if (ul_startup == 2)
		ul_startup_value = 16;
	else if (ul_startup == 3)
		ul_startup_value = 24;
	else if (ul_startup == 4)
		ul_startup_value = 64;
	else if (ul_startup == 5)
		ul_startup_value = 80;
	else if (ul_startup == 6)
		ul_startup_value = 96;
	else if (ul_startup == 7)
		ul_startup_value = 112;
	else if (ul_startup == 8)
		ul_startup_value = 512;
	else if (ul_startup == 9)
		ul_startup_value = 576;
	else if (ul_startup == 10)
		ul_startup_value = 640;
	else if (ul_startup == 11)
		ul_startup_value = 704;
	else if (ul_startup == 12)
		ul_startup_value = 768;
	else if (ul_startup == 13)
		ul_startup_value = 832;
	else if (ul_startup == 14)
		ul_startup_value = 896;
	else if (ul_startup == 15)
		ul_startup_value = 960;

	return ul_startup_value;
}

/**
 * \brief Check AFEC configurations.
 *
 * \param p_afec Pointer to an AFEC instance.
 * \param ul_mck Main clock of the device (in Hz).
 */
void afec_check(Afec *p_afec, const uint32_t ul_mck)
{
	uint32_t ul_afecfreq;
	uint32_t ul_prescal;
	uint32_t ul_startup;

	/* AFECClock = MCK / ( (PRESCAL+1) * 2 ) */
	ul_prescal = ((p_afec->AFEC_MR & AFEC_MR_PRESCAL_Msk) >>
			AFEC_MR_PRESCAL_Pos);
	ul_afecfreq = ul_mck / ((ul_prescal + 1) * 2);
	printf("AFEC clock frequency = %d Hz\r\n", (int)ul_afecfreq);

	if (ul_afecfreq < AFEC_FREQ_MIN) {
		printf("afec frequency too low (out of specification: %d Hz)\r\n",
			(int)AFEC_FREQ_MIN);
	}
	if (ul_afecfreq > AFEC_FREQ_MAX) {
		printf("afec frequency too high (out of specification: %d Hz)\r\n",
			(int)AFEC_FREQ_MAX);
	}

	ul_startup = ((p_afec->AFEC_MR & AFEC_MR_STARTUP_Msk) >>
			AFEC_MR_STARTUP_Pos);
	if (!(p_afec->AFEC_MR & AFEC_MR_SLEEP_SLEEP)) {
		/* 40ms */
		if (AFEC_STARTUP_NORM * ul_afecfreq / 1000000 >
				calcul_startup(ul_startup)) {
			printf("Startup time too small: %d, programmed: %d\r\n",
					(int)(AFEC_STARTUP_NORM * ul_afecfreq /
							1000000),
					(int)calcul_startup(ul_startup));
		}
	} else {
		if (p_afec->AFEC_MR & AFEC_MR_FREERUN_ON) {
			puts("FreeRun forbidden in sleep mode\r");
		}
		if (!(p_afec->AFEC_MR & AFEC_MR_FWUP_ON)) {
			/* Sleep 40ms */
			if (AFEC_STARTUP_NORM * ul_afecfreq / 1000000 >
					calcul_startup(ul_startup)) {
				printf("Startup time too small: %d, programmed: %d\r\n",
					(int)(AFEC_STARTUP_NORM * ul_afecfreq / 1000000),
					(int)(calcul_startup(ul_startup)));
			}
		} else {
			if (p_afec->AFEC_MR & AFEC_MR_FWUP_ON) {
				/* Fast Wake Up Sleep Mode: 12ms */
				if (AFEC_STARTUP_FAST * ul_afecfreq / 1000000 >
						calcul_startup(ul_startup)) {
					printf("Startup time too small: %d, programmed: %d\r\n",
						(int)(AFEC_STARTUP_NORM * ul_afecfreq / 1000000),
						(int)(calcul_startup(ul_startup)));
				}
			}
		}
	}
}

/**
 * \brief Get PDC registers base address.
 *
 * \param p_afec Pointer to an AFEC instance.
 *
 * \return AFEC PDC register base address.
 */
Pdc *afec_get_pdc_base(const Afec *p_afec)
{
	if (p_afec == AFEC0) {
		return PDC_AFEC0;
	}
	else if (p_afec == AFEC1) {
		return PDC_AFEC1;
	}
	else {
	  return NULL;
	}
}

//@}

/// @cond 0
/**INDENT-OFF**/
#ifdef __cplusplus
}
#endif
/**INDENT-ON**/
/// @endcond
