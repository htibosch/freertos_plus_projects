/*
 * Copyright (c) 2007-2008, Advanced Micro Devices, Inc.
 *               All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *    * Neither the name of Advanced Micro Devices, Inc. nor the names
 *      of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written
 *      permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Some portions copyright (c) 2010-2013 Xilinx, Inc.  All rights reserved.
 *
 * Xilinx, Inc.
 * XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A
 * COURTESY TO YOU.  BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
 * ONE POSSIBLE   IMPLEMENTATION OF THIS FEATURE, APPLICATION OR
 * STANDARD, XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION
 * IS FREE FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE
 * FOR OBTAINING ANY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.
 * XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
 * THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO
 * ANY WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE
 * FROM CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 */

/* Standard includes. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "x_emacpsif.h"
//#include "lwipopts.h"
#include "xparameters_ps.h"
#include "xparameters.h"

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

///* FreeRTOS+TCP includes. */
/* FreeRTOS+TCP includes. */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_IP_Private.h"
#include "NetworkBufferManagement.h"

int phy_detected = 0;

/*** IMPORTANT: Define PEEP in xemacpsif.h and sys_arch_raw.c
 *** to run it on a PEEP board
 ***/

/* Advertisement control register. */
#define ADVERTISE_10HALF		0x0020  /* Try for 10mbps half-duplex  */
#define ADVERTISE_10FULL		0x0040  /* Try for 10mbps full-duplex  */
#define ADVERTISE_100HALF		0x0080  /* Try for 100mbps half-duplex */
#define ADVERTISE_100FULL		0x0100  /* Try for 100mbps full-duplex */

#define ADVERTISE_100_AND_10	(ADVERTISE_10FULL | ADVERTISE_100FULL | \
								ADVERTISE_10HALF | ADVERTISE_100HALF)
#define ADVERTISE_100			(ADVERTISE_100FULL | ADVERTISE_100HALF)
#define ADVERTISE_10			(ADVERTISE_10FULL | ADVERTISE_10HALF)

#define ADVERTISE_1000			0x0300


//#define PHY_REG_00_BMCR            0x00 // Basic mode control register
//#define PHY_REG_01_BMSR            0x01 // Basic mode status register
//#define PHY_REG_02_PHYSID1         0x02 // PHYS ID 1
//#define PHY_REG_03_PHYSID2         0x03 // PHYS ID 2
//#define PHY_REG_04_ADVERTISE       0x04 // Advertisement control reg

#define IEEE_CONTROL_REG_OFFSET                    0
#define IEEE_STATUS_REG_OFFSET                     1
#define IEEE_AUTONEGO_ADVERTISE_REG                4
#define IEEE_PARTNER_ABILITIES_1_REG_OFFSET        5
#define IEEE_PARTNER_ABILITIES_2_REG_OFFSET        8
#define IEEE_PARTNER_ABILITIES_3_REG_OFFSET        10
#define IEEE_1000_ADVERTISE_REG_OFFSET             9
#define IEEE_MMD_ACCESS_CONTROL_REG                13
#define IEEE_MMD_ACCESS_ADDRESS_DATA_REG           14
#define IEEE_COPPER_SPECIFIC_CONTROL_REG           16
#define IEEE_SPECIFIC_STATUS_REG                   17
#define IEEE_COPPER_SPECIFIC_STATUS_REG_2          19
#define IEEE_EXT_PHY_SPECIFIC_CONTROL_REG          20
#define IEEE_CONTROL_REG_MAC                       21
#define IEEE_PAGE_ADDRESS_REGISTER                 22

#define IEEE_CTRL_1GBPS_LINKSPEED_MASK             0x2040
#define IEEE_CTRL_LINKSPEED_MASK                   0x0040
#define IEEE_CTRL_LINKSPEED_1000M                  0x0040
#define IEEE_CTRL_LINKSPEED_100M                   0x2000
#define IEEE_CTRL_LINKSPEED_10M                    0x0000
#define IEEE_CTRL_FULL_DUPLEX                      0x100
#define IEEE_CTRL_RESET_MASK                       0x8000
#define IEEE_CTRL_AUTONEGOTIATE_ENABLE             0x1000
#define IEEE_STAT_AUTONEGOTIATE_CAPABLE            0x0008
#define IEEE_STAT_AUTONEGOTIATE_COMPLETE           0x0020
#define IEEE_STAT_AUTONEGOTIATE_RESTART            0x0200
#define IEEE_STAT_LINK_STATUS                      0x0004
#define IEEE_STAT_1GBPS_EXTENSIONS                 0x0100
#define IEEE_AN1_ABILITY_MASK                      0x1FE0
#define IEEE_AN3_ABILITY_MASK_1GBPS                0x0C00
#define IEEE_AN1_ABILITY_MASK_100MBPS              0x0380
#define IEEE_AN1_ABILITY_MASK_10MBPS               0x0060
#define IEEE_RGMII_TXRX_CLOCK_DELAYED_MASK         0x0030

#define IEEE_SPEED_MASK                            0xC000
#define IEEE_SPEED_1000                            0x8000
#define IEEE_SPEED_100                             0x4000

#define IEEE_ASYMMETRIC_PAUSE_MASK                 0x0800
#define IEEE_PAUSE_MASK                            0x0400
#define IEEE_AUTONEG_ERROR_MASK                    0x8000

#define IEEE_MMD_ACCESS_CTRL_DEVAD_MASK            0x1F
#define IEEE_MMD_ACCESS_CTRL_PIDEVAD_MASK          0x801F
#define IEEE_MMD_ACCESS_CTRL_NOPIDEVAD_MASK        0x401F

#define PHY_DETECT_REG  						1
#define PHY_IDENTIFIER_1_REG					2
#define PHY_IDENTIFIER_2_REG					3
#define PHY_DETECT_MASK 					0x1808
#define PHY_MARVELL_IDENTIFIER				0x0141
#define PHY_TI_IDENTIFIER					0x2000
#define PHY_REALTEK_IDENTIFIER				0x001c
#define PHY_XILINX_PCS_PMA_ID1				0x0174
#define PHY_XILINX_PCS_PMA_ID2				0x0C00

#define XEMACPS_GMII2RGMII_SPEED1000_FD		0x140
#define XEMACPS_GMII2RGMII_SPEED100_FD		0x2100
#define XEMACPS_GMII2RGMII_SPEED10_FD		0x100
#define XEMACPS_GMII2RGMII_REG_NUM			0x10

#define PHY_REGCR		0x0D
#define PHY_ADDAR		0x0E
#define PHY_RGMIIDCTL	0x86
#define PHY_RGMIICTL	0x32
#define PHY_STS			0x11
#define PHY_TI_CR		0x10
#define PHY_TI_CFG4		0x31

#define PHY_REGCR_ADDR	0x001F
#define PHY_REGCR_DATA	0x401F
#define PHY_TI_CRVAL	0x5048
#define PHY_TI_CFG4RESVDBIT7	0x80

/* Frequency setting */
#define SLCR_LOCK_ADDR			(XPS_SYS_CTRL_BASEADDR + 0x4)
#define SLCR_UNLOCK_ADDR		(XPS_SYS_CTRL_BASEADDR + 0x8)
#define SLCR_GEM0_CLK_CTRL_ADDR	(XPS_SYS_CTRL_BASEADDR + 0x140)
#define SLCR_GEM1_CLK_CTRL_ADDR	(XPS_SYS_CTRL_BASEADDR + 0x144)
#ifdef PEEP
#define SLCR_GEM_10M_CLK_CTRL_VALUE		0x00103031
#define SLCR_GEM_100M_CLK_CTRL_VALUE	0x00103001
#define SLCR_GEM_1G_CLK_CTRL_VALUE		0x00103011
#endif
#define SLCR_GEM_SRCSEL_EMIO	0x40
#define SLCR_LOCK_KEY_VALUE 	0x767B
#define SLCR_UNLOCK_KEY_VALUE	0xDF0D
#define SLCR_ADDR_GEM_RST_CTRL	(XPS_SYS_CTRL_BASEADDR + 0x214)
#define EMACPS_SLCR_DIV_MASK	0xFC0FC0FF

#define ZYNQ_EMACPS_0_BASEADDR 0xE000B000
#define ZYNQ_EMACPS_1_BASEADDR 0xE000C000

#define ZYNQMP_EMACPS_0_BASEADDR 0xFF0B0000
#define ZYNQMP_EMACPS_1_BASEADDR 0xFF0C0000
#define ZYNQMP_EMACPS_2_BASEADDR 0xFF0D0000
#define ZYNQMP_EMACPS_3_BASEADDR 0xFF0E0000

#define CRL_APB_GEM0_REF_CTRL	0xFF5E0050
#define CRL_APB_GEM1_REF_CTRL	0xFF5E0054
#define CRL_APB_GEM2_REF_CTRL	0xFF5E0058
#define CRL_APB_GEM3_REF_CTRL	0xFF5E005C

#define CRL_APB_GEM_DIV0_MASK	0x00003F00
#define CRL_APB_GEM_DIV0_SHIFT	8
#define CRL_APB_GEM_DIV1_MASK	0x003F0000
#define CRL_APB_GEM_DIV1_SHIFT	16

u32 phymapemac0[32];
u32 phymapemac1[32];

static void my_sleep( uint32_t uxTicks )
{
	for( uint32_t i = 0U; i < 1000000U; i++ ) {
		if( ( ( volatile int ) phy_detected ) == 6 ) {
			break;
		}
	}
}

unsigned detect_phy(XEmacPs *xemacpsp)
{
	u16 phy_reg;
	u32 phy_addr;
	u32 emacnum;

	if (xemacpsp->Config.BaseAddress == XPAR_XEMACPS_0_BASEADDR)
		emacnum = 0;
	else
		emacnum = 1;
	for (phy_addr = 31; phy_addr > 0; phy_addr--) {
		XEmacPs_PhyRead(xemacpsp, phy_addr, PHY_DETECT_REG,
							&phy_reg);

		if ((phy_reg != 0xFFFF) &&
			((phy_reg & PHY_DETECT_MASK) == PHY_DETECT_MASK)) {
			/* Found a valid PHY address */
			xil_printf("XEmacPs detect_phy: PHY detected at address %d.\r\n",
																	phy_addr);
			if (emacnum == 0)
				phymapemac0[phy_addr] = TRUE;
			else
				phymapemac1[phy_addr] = TRUE;

			XEmacPs_PhyRead(xemacpsp, phy_addr, PHY_IDENTIFIER_1_REG,
							&phy_reg);
			if ((phy_reg != PHY_MARVELL_IDENTIFIER) &&
				(phy_reg != PHY_TI_IDENTIFIER) &&
				(phy_reg != PHY_REALTEK_IDENTIFIER)) {
				xil_printf("WARNING: Not a Marvell or TI or Realtek Ethernet PHY. Please verify the initialization sequence\r\n");
			}
		}
	}
	return phy_addr;
}

/* Zynq */
unsigned get_IEEE_phy_speed_US(XEmacPs *xemacpsp, u32 phy_addr)
{
		u16 temp;
		u16 control;
		u16 status;
		u16 status_speed;
		u32 timeout_counter = 0;
		u32 temp_speed;
		xil_printf("Start PHY autonegotiation \r\n");

		XEmacPs_PhyWrite(xemacpsp,phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 2);
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_MAC, &control);
		control |= IEEE_RGMII_TXRX_CLOCK_DELAYED_MASK;
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_MAC, control);

		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 0);

		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, &control);
		control |= IEEE_ASYMMETRIC_PAUSE_MASK;
		control |= IEEE_PAUSE_MASK;
		control |= ADVERTISE_100;
		control |= ADVERTISE_10;
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, control);

		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET,
						&control);
		control |= ADVERTISE_1000;
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET,
						control);

		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 0);
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_COPPER_SPECIFIC_CONTROL_REG,
																	&control);
		control |= (7 << 12);	/* max number of gigabit attempts */
		control |= (1 << 11);	/* enable downshift */
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_COPPER_SPECIFIC_CONTROL_REG,
																	control);
		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
		control |= IEEE_CTRL_AUTONEGOTIATE_ENABLE;
		control |= IEEE_STAT_AUTONEGOTIATE_RESTART;
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, control);

		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
		control |= IEEE_CTRL_RESET_MASK;
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, control);

		while (1) {
			XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
			if (control & IEEE_CTRL_RESET_MASK)
				continue;
			else
				break;
		}

		XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_STATUS_REG_OFFSET, &status);

		xil_printf("Waiting for PHY to complete autonegotiation.\r\n");

		while ( !(status & IEEE_STAT_AUTONEGOTIATE_COMPLETE) ) {
			my_sleep(1);
			XEmacPs_PhyRead(xemacpsp, phy_addr,
							IEEE_COPPER_SPECIFIC_STATUS_REG_2,  &temp);
			timeout_counter++;

			if (timeout_counter == 1000) {
				xil_printf("Auto negotiation error \r\n");
				return XST_FAILURE;
			}
			XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_STATUS_REG_OFFSET, &status);
		}
		xil_printf("autonegotiation complete \r\n");

		XEmacPs_PhyRead(xemacpsp, phy_addr,IEEE_SPECIFIC_STATUS_REG,
						&status_speed);
		if (status_speed & 0x400) {
			temp_speed = status_speed & IEEE_SPEED_MASK;

			if (temp_speed == IEEE_SPEED_1000)
				return 1000;
			else if(temp_speed == IEEE_SPEED_100)
				return 100;
			else
				return 10;
		}

		return XST_SUCCESS;
}

unsigned configure_IEEE_phy_speed_US(XEmacPs *xemacpsp, unsigned speed)
{
	u16 control;
	u32 phy_addr = detect_phy(xemacpsp);

	XEmacPs_PhyWrite(xemacpsp,phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 2);
	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_MAC, &control);
	control |= IEEE_RGMII_TXRX_CLOCK_DELAYED_MASK;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_MAC, control);

	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_PAGE_ADDRESS_REGISTER, 0);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, &control);
	control |= IEEE_ASYMMETRIC_PAUSE_MASK;
	control |= IEEE_PAUSE_MASK;
	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG, control);

	XEmacPs_PhyRead(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET, &control);
	control &= ~IEEE_CTRL_LINKSPEED_1000M;
	control &= ~IEEE_CTRL_LINKSPEED_100M;
	control &= ~IEEE_CTRL_LINKSPEED_10M;

	if (speed == 1000) {
		control |= IEEE_CTRL_LINKSPEED_1000M;
	}

	else if (speed == 100) {
		control |= IEEE_CTRL_LINKSPEED_100M;
		/* Dont advertise PHY speed of 1000 Mbps */
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET, 0);
		/* Dont advertise PHY speed of 10 Mbps */
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG,
																ADVERTISE_100);
	}

	else if (speed == 10) {
		control |= IEEE_CTRL_LINKSPEED_10M;
		/* Dont advertise PHY speed of 1000 Mbps */
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_1000_ADVERTISE_REG_OFFSET,
																			0);
		/* Dont advertise PHY speed of 100 Mbps */
		XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_AUTONEGO_ADVERTISE_REG,
																ADVERTISE_10);
	}

	XEmacPs_PhyWrite(xemacpsp, phy_addr, IEEE_CONTROL_REG_OFFSET,
											control | IEEE_CTRL_RESET_MASK);
	{
		volatile int wait;
		for (wait=0; wait < 100000; wait++);
	}
	return 0;
}

static void SetUpSLCRDivisors(u32 mac_baseaddr, s32 speed)
{
	volatile u32 slcrBaseAddress;
	u32 SlcrDiv0 = 0;
	u32 SlcrDiv1 = 0;
	u32 SlcrTxClkCntrl;
	u32 gigeversion;
	volatile u32 CrlApbBaseAddr;
	u32 CrlApbDiv0 = 0;
	u32 CrlApbDiv1 = 0;
	u32 CrlApbGemCtrl;

	gigeversion = ((Xil_In32(mac_baseaddr + 0xFC)) >> 16) & 0xFFF;
	if (gigeversion == 2) {

		*(volatile u32 *)(SLCR_UNLOCK_ADDR) = SLCR_UNLOCK_KEY_VALUE;

		if (mac_baseaddr == ZYNQ_EMACPS_0_BASEADDR) {
			slcrBaseAddress = SLCR_GEM0_CLK_CTRL_ADDR;
		} else {
			slcrBaseAddress = SLCR_GEM1_CLK_CTRL_ADDR;
		}

		if((*(volatile u32 *)(UINTPTR)(slcrBaseAddress)) &
			SLCR_GEM_SRCSEL_EMIO) {
				return;
		}

		if (speed == 1000) {
			if (mac_baseaddr == XPAR_XEMACPS_0_BASEADDR) {
#ifdef XPAR_PS7_ETHERNET_0_ENET_SLCR_1000MBPS_DIV0
				SlcrDiv0 = XPAR_PS7_ETHERNET_0_ENET_SLCR_1000MBPS_DIV0;
				SlcrDiv1 = XPAR_PS7_ETHERNET_0_ENET_SLCR_1000MBPS_DIV1;
#endif
			} else {
#ifdef XPAR_PS7_ETHERNET_1_ENET_SLCR_1000MBPS_DIV0
				SlcrDiv0 = XPAR_PS7_ETHERNET_1_ENET_SLCR_1000MBPS_DIV0;
				SlcrDiv1 = XPAR_PS7_ETHERNET_1_ENET_SLCR_1000MBPS_DIV1;
#endif
			}
		} else if (speed == 100) {
			if (mac_baseaddr == XPAR_XEMACPS_0_BASEADDR) {
#ifdef XPAR_PS7_ETHERNET_0_ENET_SLCR_100MBPS_DIV0
				SlcrDiv0 = XPAR_PS7_ETHERNET_0_ENET_SLCR_100MBPS_DIV0;
				SlcrDiv1 = XPAR_PS7_ETHERNET_0_ENET_SLCR_100MBPS_DIV1;
#endif
			} else {
#ifdef XPAR_PS7_ETHERNET_1_ENET_SLCR_100MBPS_DIV0
				SlcrDiv0 = XPAR_PS7_ETHERNET_1_ENET_SLCR_100MBPS_DIV0;
				SlcrDiv1 = XPAR_PS7_ETHERNET_1_ENET_SLCR_100MBPS_DIV1;
#endif
			}
		} else {
			if (mac_baseaddr == XPAR_XEMACPS_0_BASEADDR) {
#ifdef XPAR_PS7_ETHERNET_0_ENET_SLCR_10MBPS_DIV0
				SlcrDiv0 = XPAR_PS7_ETHERNET_0_ENET_SLCR_10MBPS_DIV0;
				SlcrDiv1 = XPAR_PS7_ETHERNET_0_ENET_SLCR_10MBPS_DIV1;
#endif
			} else {
#ifdef XPAR_PS7_ETHERNET_1_ENET_SLCR_10MBPS_DIV0
				SlcrDiv0 = XPAR_PS7_ETHERNET_1_ENET_SLCR_10MBPS_DIV0;
				SlcrDiv1 = XPAR_PS7_ETHERNET_1_ENET_SLCR_10MBPS_DIV1;
#endif
			}
		}

		if (SlcrDiv0 != 0 && SlcrDiv1 != 0) {
			SlcrTxClkCntrl = *(volatile u32 *)(UINTPTR)(slcrBaseAddress);
			SlcrTxClkCntrl &= EMACPS_SLCR_DIV_MASK;
			SlcrTxClkCntrl |= (SlcrDiv1 << 20);
			SlcrTxClkCntrl |= (SlcrDiv0 << 8);
			*(volatile u32 *)(UINTPTR)(slcrBaseAddress) = SlcrTxClkCntrl;
			*(volatile u32 *)(SLCR_LOCK_ADDR) = SLCR_LOCK_KEY_VALUE;
		} else {
			xil_printf("Clock Divisors incorrect - Please check\r\n");
		}
	} else if (gigeversion > 2) {
		/* Setup divisors in CRL_APB for Zynq Ultrascale+ MPSoC */
		if (mac_baseaddr == ZYNQMP_EMACPS_0_BASEADDR) {
			CrlApbBaseAddr = CRL_APB_GEM0_REF_CTRL;
		} else if (mac_baseaddr == ZYNQMP_EMACPS_1_BASEADDR) {
			CrlApbBaseAddr = CRL_APB_GEM1_REF_CTRL;
		} else if (mac_baseaddr == ZYNQMP_EMACPS_2_BASEADDR) {
			CrlApbBaseAddr = CRL_APB_GEM2_REF_CTRL;
		} else if (mac_baseaddr == ZYNQMP_EMACPS_3_BASEADDR) {
			CrlApbBaseAddr = CRL_APB_GEM3_REF_CTRL;
		}

		if (speed == 1000) {
			if (mac_baseaddr == ZYNQMP_EMACPS_0_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_0_ENET_SLCR_1000MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_0_ENET_SLCR_1000MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_0_ENET_SLCR_1000MBPS_DIV1;
#endif
			} else if (mac_baseaddr == ZYNQMP_EMACPS_1_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_1_ENET_SLCR_1000MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_1_ENET_SLCR_1000MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_1_ENET_SLCR_1000MBPS_DIV1;
#endif
			} else if (mac_baseaddr == ZYNQMP_EMACPS_2_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_2_ENET_SLCR_1000MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_2_ENET_SLCR_1000MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_2_ENET_SLCR_1000MBPS_DIV1;
#endif
			} else if (mac_baseaddr == ZYNQMP_EMACPS_3_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_3_ENET_SLCR_1000MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_3_ENET_SLCR_1000MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_3_ENET_SLCR_1000MBPS_DIV1;
#endif
			}
		} else if (speed == 100) {
			if (mac_baseaddr == ZYNQMP_EMACPS_0_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_0_ENET_SLCR_100MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_0_ENET_SLCR_100MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_0_ENET_SLCR_100MBPS_DIV1;
#endif
			} else if (mac_baseaddr == ZYNQMP_EMACPS_1_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_1_ENET_SLCR_100MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_1_ENET_SLCR_100MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_1_ENET_SLCR_100MBPS_DIV1;
#endif
			} else if (mac_baseaddr == ZYNQMP_EMACPS_2_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_2_ENET_SLCR_100MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_2_ENET_SLCR_100MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_2_ENET_SLCR_100MBPS_DIV1;
#endif
			} else if (mac_baseaddr == ZYNQMP_EMACPS_3_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_3_ENET_SLCR_100MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_3_ENET_SLCR_100MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_3_ENET_SLCR_100MBPS_DIV1;
#endif
			}
		} else {
			if (mac_baseaddr == ZYNQMP_EMACPS_0_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_0_ENET_SLCR_10MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_0_ENET_SLCR_10MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_0_ENET_SLCR_10MBPS_DIV1;
#endif
			} else if (mac_baseaddr == ZYNQMP_EMACPS_1_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_1_ENET_SLCR_10MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_1_ENET_SLCR_10MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_1_ENET_SLCR_10MBPS_DIV1;
#endif
			} else if (mac_baseaddr == ZYNQMP_EMACPS_2_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_2_ENET_SLCR_10MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_2_ENET_SLCR_10MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_2_ENET_SLCR_10MBPS_DIV1;
#endif
			} else if (mac_baseaddr == ZYNQMP_EMACPS_3_BASEADDR) {
#ifdef XPAR_PSU_ETHERNET_3_ENET_SLCR_10MBPS_DIV0
				CrlApbDiv0 = XPAR_PSU_ETHERNET_3_ENET_SLCR_10MBPS_DIV0;
				CrlApbDiv1 = XPAR_PSU_ETHERNET_3_ENET_SLCR_10MBPS_DIV1;
#endif
			}
		}

		if (CrlApbDiv0 != 0 && CrlApbDiv1 != 0) {
		#if EL1_NONSECURE
			XSmc_OutVar RegRead;
			RegRead = Xil_Smc(MMIO_READ_SMC_FID, (u64)(CrlApbBaseAddr),
								0, 0, 0, 0, 0, 0);
			CrlApbGemCtrl = RegRead.Arg0 >> 32;
		#else
			CrlApbGemCtrl = *(volatile u32 *)(UINTPTR)(CrlApbBaseAddr);
        #endif
			CrlApbGemCtrl &= ~CRL_APB_GEM_DIV0_MASK;
			CrlApbGemCtrl |= CrlApbDiv0 << CRL_APB_GEM_DIV0_SHIFT;
			CrlApbGemCtrl &= ~CRL_APB_GEM_DIV1_MASK;
			CrlApbGemCtrl |= CrlApbDiv1 << CRL_APB_GEM_DIV1_SHIFT;
		#if EL1_NONSECURE
			Xil_Smc(MMIO_WRITE_SMC_FID, (u64)(CrlApbBaseAddr) | ((u64)(0xFFFFFFFF) << 32),
				(u64)CrlApbGemCtrl, 0, 0, 0, 0, 0);
			do {
			RegRead = Xil_Smc(MMIO_READ_SMC_FID, (u64)(CrlApbBaseAddr),
				0, 0, 0, 0, 0, 0);
			} while((RegRead.Arg0 >> 32) != CrlApbGemCtrl);
		#else
			*(volatile u32 *)(UINTPTR)(CrlApbBaseAddr) = CrlApbGemCtrl;
        #endif
		} else {
			xil_printf("Clock Divisors incorrect - Please check\r\n");
		}
	}

	return;
}


u32 Phy_Setup_US (XEmacPs *xemacpsp, u32 phy_addr)
{
	u32 link_speed = 0;
	u32 conv_present = 0;
	u32 convspeeddupsetting = 0;
	u32 convphyaddr = 0;

#ifdef XPAR_GMII2RGMIICON_0N_ETH0_ADDR
	convphyaddr = XPAR_GMII2RGMIICON_0N_ETH0_ADDR;
	conv_present = 1;
#else
#ifdef XPAR_GMII2RGMIICON_0N_ETH1_ADDR
	convphyaddr = XPAR_GMII2RGMIICON_0N_ETH1_ADDR;
	conv_present = 1;
#endif
#endif

#ifdef  ipconfigNIC_LINKSPEED_AUTODETECT
	link_speed = get_IEEE_phy_speed_US(xemacpsp, phy_addr);
	if (link_speed == 1000) {
		SetUpSLCRDivisors(xemacpsp->Config.BaseAddress,1000);
		convspeeddupsetting = XEMACPS_GMII2RGMII_SPEED1000_FD;
	} else if (link_speed == 100) {
		SetUpSLCRDivisors(xemacpsp->Config.BaseAddress,100);
		convspeeddupsetting = XEMACPS_GMII2RGMII_SPEED100_FD;
	} else if (link_speed != XST_FAILURE){
		SetUpSLCRDivisors(xemacpsp->Config.BaseAddress,10);
		convspeeddupsetting = XEMACPS_GMII2RGMII_SPEED10_FD;
	} else {
		xil_printf("Phy setup error \r\n");
		return XST_FAILURE;
	}
#elif	defined(ipconfigNIC_LINKSPEED1000)
	SetUpSLCRDivisors(xemacpsp->Config.BaseAddress,1000);
	link_speed = 1000;
	configure_IEEE_phy_speed_US(xemacpsp, link_speed);
	convspeeddupsetting = XEMACPS_GMII2RGMII_SPEED1000_FD;
	my_sleep(1);
#elif	defined(ipconfigNIC_LINKSPEED100)
	SetUpSLCRDivisors(xemacpsp->Config.BaseAddress,100);
	link_speed = 100;
	configure_IEEE_phy_speed_US(xemacpsp, link_speed);
	convspeeddupsetting = XEMACPS_GMII2RGMII_SPEED100_FD;
	my_sleep(1);
#elif	defined(ipconfigNIC_LINKSPEED10)
	SetUpSLCRDivisors(xemacpsp->Config.BaseAddress,10);
	link_speed = 10;
	configure_IEEE_phy_speed_US(xemacpsp, link_speed);
	convspeeddupsetting = XEMACPS_GMII2RGMII_SPEED10_FD;
	my_sleep(1);
#endif
	if (conv_present) {
		XEmacPs_PhyWrite(xemacpsp, convphyaddr,
		XEMACPS_GMII2RGMII_REG_NUM, convspeeddupsetting);
	}

	xil_printf("link speed: %d\r\n", link_speed);
	return link_speed;
}

