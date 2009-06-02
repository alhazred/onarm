/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (C) 2003-2005 Chelsio Communications.  All rights reserved.
 */

#pragma ident	"@(#)pm3393.c	1.4	07/03/15 SMI"	/* pm3393.c */

#include "common.h"
#include "regs.h"
#include "gmac.h"
#include "elmer0.h"
#include "suni1x10gexp_regs.h"

/* 802.3ae 10Gb/s MDIO Manageable Device(MMD)
 */
#define MMD_RESERVED        0
#define MMD_PMAPMD          1
#define MMD_WIS             2
#define MMD_PCS             3
#define MMD_PHY_XGXS        4	/* XGMII Extender Sublayer */
#define MMD_DTE_XGXS        5

#define PHY_XGXS_CTRL_1     0
#define PHY_XGXS_STATUS_1   1

#define OFFSET(REG_ADDR)    (REG_ADDR << 2)

/* Max frame size PM3393 can handle. Includes Ethernet header and CRC. */
#define MAX_FRAME_SIZE  9600

#define IPG 12
#define TXXG_CONF1_VAL ((IPG << SUNI1x10GEXP_BITOFF_TXXG_IPGT) | \
    SUNI1x10GEXP_BITMSK_TXXG_32BIT_ALIGN | SUNI1x10GEXP_BITMSK_TXXG_CRCEN | \
    SUNI1x10GEXP_BITMSK_TXXG_PADEN)
#define RXXG_CONF1_VAL (SUNI1x10GEXP_BITMSK_RXXG_PUREP | 0x14 | \
    SUNI1x10GEXP_BITMSK_RXXG_FLCHK | SUNI1x10GEXP_BITMSK_RXXG_CRC_STRIP)

/* Update statistics every 15 minutes */
#define STATS_TICK_SECS (15 * 60)

enum {                     /* RMON registers */
	RxOctetsReceivedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_1_LOW,
	RxUnicastFramesReceivedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_4_LOW,
	RxMulticastFramesReceivedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_5_LOW,
	RxBroadcastFramesReceivedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_6_LOW,
	RxPAUSEMACCtrlFramesReceived = SUNI1x10GEXP_REG_MSTAT_COUNTER_8_LOW,
	RxFrameCheckSequenceErrors = SUNI1x10GEXP_REG_MSTAT_COUNTER_10_LOW,
	RxFramesLostDueToInternalMACErrors = SUNI1x10GEXP_REG_MSTAT_COUNTER_11_LOW,
	RxSymbolErrors = SUNI1x10GEXP_REG_MSTAT_COUNTER_12_LOW,
	RxInRangeLengthErrors = SUNI1x10GEXP_REG_MSTAT_COUNTER_13_LOW,
	RxFramesTooLongErrors = SUNI1x10GEXP_REG_MSTAT_COUNTER_15_LOW,
	RxJabbers = SUNI1x10GEXP_REG_MSTAT_COUNTER_16_LOW,
	RxFragments = SUNI1x10GEXP_REG_MSTAT_COUNTER_17_LOW,
	RxUndersizedFrames =  SUNI1x10GEXP_REG_MSTAT_COUNTER_18_LOW,
	RxJumboFramesReceivedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_25_LOW,
	RxJumboOctetsReceivedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_26_LOW,

	TxOctetsTransmittedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_33_LOW,
	TxFramesLostDueToInternalMACTransmissionError = SUNI1x10GEXP_REG_MSTAT_COUNTER_35_LOW,
	TxTransmitSystemError = SUNI1x10GEXP_REG_MSTAT_COUNTER_36_LOW,
	TxUnicastFramesTransmittedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_38_LOW,
	TxMulticastFramesTransmittedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_40_LOW,
	TxBroadcastFramesTransmittedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_42_LOW,
	TxPAUSEMACCtrlFramesTransmitted = SUNI1x10GEXP_REG_MSTAT_COUNTER_43_LOW,
	TxJumboFramesReceivedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_51_LOW,
	TxJumboOctetsReceivedOK = SUNI1x10GEXP_REG_MSTAT_COUNTER_52_LOW
};

struct _cmac_instance {
	u8 enabled;
	u8 fc;
	u8 mac_addr[6];
};

static int pmread(struct cmac *cmac, u32 reg, u32 * data32)
{
	(void) t1_tpi_read(cmac->adapter, OFFSET(reg), data32);
	return 0;
}

static int pmwrite(struct cmac *cmac, u32 reg, u32 data32)
{
	(void) t1_tpi_write(cmac->adapter, OFFSET(reg), data32);
	return 0;
}

/* Port reset. */
/* ARGSUSED */
static int pm3393_reset(struct cmac *cmac)
{
	return 0;
}

/*
 * Enable interrupts for the PM3393
 
    1. Enable PM3393 BLOCK interrupts.
    2. Enable PM3393 Master Interrupt bit(INTE)
    3. Enable ELMER's PM3393 bit.
    4. Enable Terminator external interrupt.
*/
static int pm3393_interrupt_enable(struct cmac *cmac)
{
#if 0
	u32 elmer;
#endif
	u32 pl_intr;

	/* PM3393 - Enabling all hardware block interrupts.
	 */
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_SERDES_3125_INTERRUPT_ENABLE, 0xffff);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_XRF_INTERRUPT_ENABLE, 0xffff);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_XRF_DIAG_INTERRUPT_ENABLE, 0xffff);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXOAM_INTERRUPT_ENABLE, 0xffff);

	/* Don't interrupt on statistics overflow, we are polling */
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_MSTAT_INTERRUPT_MASK_0, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_MSTAT_INTERRUPT_MASK_1, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_MSTAT_INTERRUPT_MASK_2, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_MSTAT_INTERRUPT_MASK_3, 0);

	(void) pmwrite(cmac, SUNI1x10GEXP_REG_IFLX_FIFO_OVERFLOW_ENABLE, 0xffff);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_PL4ODP_INTERRUPT_MASK, 0xffff);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_XTEF_INTERRUPT_ENABLE, 0xffff);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_TXOAM_INTERRUPT_ENABLE, 0xffff);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_CONFIG_3, 0xffff);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_PL4IO_LOCK_DETECT_MASK, 0xffff);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_TXXG_CONFIG_3, 0xffff);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_PL4IDU_INTERRUPT_MASK, 0xffff);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_EFLX_FIFO_OVERFLOW_ERROR_ENABLE, 0xffff);

	/* PM3393 - Global interrupt enable
	 */
	/* TBD XXX Disable for now until we figure out why error interrupts keep asserting. */
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_GLOBAL_INTERRUPT_ENABLE,
		0 /*SUNI1x10GEXP_BITMSK_TOP_INTE */ );

#if 0
	/* ELMER - External chip interrupts.
	 */
	(void) t1_tpi_read(cmac->adapter, A_ELMER0_INT_ENABLE, &elmer);
	elmer |= ELMER0_GP_BIT1;
	(void) t1_tpi_write(cmac->adapter, A_ELMER0_INT_ENABLE, elmer);
#endif

	/* TERMINATOR - PL_INTERUPTS_EXT */
	pl_intr = t1_read_reg_4(cmac->adapter, A_PL_ENABLE);
	pl_intr |= F_PL_INTR_EXT;
	t1_write_reg_4(cmac->adapter, A_PL_ENABLE, pl_intr);
	return 0;
}

static int pm3393_interrupt_disable(struct cmac *cmac)
{
	u32 elmer;

	/* PM3393 - Enabling HW interrupt blocks. */
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_SERDES_3125_INTERRUPT_ENABLE, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_XRF_INTERRUPT_ENABLE, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_XRF_DIAG_INTERRUPT_ENABLE, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXOAM_INTERRUPT_ENABLE, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_MSTAT_INTERRUPT_MASK_0, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_MSTAT_INTERRUPT_MASK_1, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_MSTAT_INTERRUPT_MASK_2, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_MSTAT_INTERRUPT_MASK_3, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_IFLX_FIFO_OVERFLOW_ENABLE, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_PL4ODP_INTERRUPT_MASK, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_XTEF_INTERRUPT_ENABLE, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_TXOAM_INTERRUPT_ENABLE, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_CONFIG_3, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_PL4IO_LOCK_DETECT_MASK, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_TXXG_CONFIG_3, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_PL4IDU_INTERRUPT_MASK, 0);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_EFLX_FIFO_OVERFLOW_ERROR_ENABLE, 0);

	/* PM3393 - Global interrupt enable */
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_GLOBAL_INTERRUPT_ENABLE, 0);

	/* ELMER - External chip interrupts. */
	(void) t1_tpi_read(cmac->adapter, A_ELMER0_INT_ENABLE, &elmer);
	elmer &= ~ELMER0_GP_BIT1;
	(void) t1_tpi_write(cmac->adapter, A_ELMER0_INT_ENABLE, elmer);

	/* TERMINATOR - PL_INTERUPTS_EXT */
	/* DO NOT DISABLE TERMINATOR's EXTERNAL INTERRUPTS. ANOTHER CHIP
	 * COULD WANT THEM ENABLED. We disable PM3393 at the ELMER level.
	 */

	return 0;
}

static int pm3393_interrupt_clear(struct cmac *cmac)
{
	u32 elmer;
	u32 pl_intr;
	u32 val32;

	/* PM3393 - Clearing HW interrupt blocks. Note, this assumes
	 *          bit WCIMODE=0 for a clear-on-read.
	 */
	(void) pmread(cmac, SUNI1x10GEXP_REG_SERDES_3125_INTERRUPT_STATUS, &val32);
	(void) pmread(cmac, SUNI1x10GEXP_REG_XRF_INTERRUPT_STATUS, &val32);
	(void) pmread(cmac, SUNI1x10GEXP_REG_XRF_DIAG_INTERRUPT_STATUS, &val32);
	(void) pmread(cmac, SUNI1x10GEXP_REG_RXOAM_INTERRUPT_STATUS, &val32);
	(void) pmread(cmac, SUNI1x10GEXP_REG_PL4ODP_INTERRUPT, &val32);
	(void) pmread(cmac, SUNI1x10GEXP_REG_XTEF_INTERRUPT_STATUS, &val32);
	(void) pmread(cmac, SUNI1x10GEXP_REG_IFLX_FIFO_OVERFLOW_INTERRUPT, &val32);
	(void) pmread(cmac, SUNI1x10GEXP_REG_TXOAM_INTERRUPT_STATUS, &val32);
	(void) pmread(cmac, SUNI1x10GEXP_REG_RXXG_INTERRUPT, &val32);
	(void) pmread(cmac, SUNI1x10GEXP_REG_TXXG_INTERRUPT, &val32);
	(void) pmread(cmac, SUNI1x10GEXP_REG_PL4IDU_INTERRUPT, &val32);
	(void) pmread(cmac, SUNI1x10GEXP_REG_EFLX_FIFO_OVERFLOW_ERROR_INDICATION,
	       &val32);
	(void) pmread(cmac, SUNI1x10GEXP_REG_PL4IO_LOCK_DETECT_STATUS, &val32);
	(void) pmread(cmac, SUNI1x10GEXP_REG_PL4IO_LOCK_DETECT_CHANGE, &val32);

	/* PM3393 - Global interrupt status 
	 */
	(void) pmread(cmac, SUNI1x10GEXP_REG_MASTER_INTERRUPT_STATUS, &val32);

	/* ELMER - External chip interrupts.
	 */
	(void) t1_tpi_read(cmac->adapter, A_ELMER0_INT_CAUSE, &elmer);
	elmer |= ELMER0_GP_BIT1;
	(void) t1_tpi_write(cmac->adapter, A_ELMER0_INT_CAUSE, elmer);

	/* TERMINATOR - PL_INTERUPTS_EXT 
	 */
	pl_intr = t1_read_reg_4(cmac->adapter, A_PL_CAUSE);
	pl_intr |= F_PL_INTR_EXT;
	t1_write_reg_4(cmac->adapter, A_PL_CAUSE, pl_intr);

	return 0;
}

/* Interrupt handler */
static int pm3393_interrupt_handler(struct cmac *cmac)
{
	u32 master_intr_status;
/*
    1. Read master interrupt register.
    2. Read BLOCK's interrupt status registers.
    3. Handle BLOCK interrupts.
*/
	/* Read the master interrupt status register. */
	(void) pmread(cmac, SUNI1x10GEXP_REG_MASTER_INTERRUPT_STATUS,
	       &master_intr_status);
	CH_DBG(cmac->adapter, INTR, "PM3393 intr cause 0x%x\n",
	       master_intr_status);

	/* Handle BLOCK's interrupts. */

	if (SUNI1x10GEXP_BITMSK_TOP_PL4IO_INT & master_intr_status) {
		/* EMPTY */
	}

	if (SUNI1x10GEXP_BITMSK_TOP_IRAM_INT & master_intr_status) {
		/* EMPTY */
	}

	if (SUNI1x10GEXP_BITMSK_TOP_ERAM_INT & master_intr_status) {
		/* EMPTY */
	}

	/* SERDES */
	if (SUNI1x10GEXP_BITMSK_TOP_XAUI_INT & master_intr_status) {
		/* EMPTY */
	}

	/* MSTAT */
	if (SUNI1x10GEXP_BITMSK_TOP_MSTAT_INT & master_intr_status) {
		/* EMPTY */
	}

	/* RXXG */
	if (SUNI1x10GEXP_BITMSK_TOP_RXXG_INT & master_intr_status) {
		/* EMPTY */
	}

	/* TXXG */
	if (SUNI1x10GEXP_BITMSK_TOP_TXXG_INT & master_intr_status) {
		/* EMPTY */
	}

	/* XRF */
	if (SUNI1x10GEXP_BITMSK_TOP_XRF_INT & master_intr_status) {
		/* EMPTY */
	}

	/* XTEF */
	if (SUNI1x10GEXP_BITMSK_TOP_XTEF_INT & master_intr_status) {
		/* EMPTY */
	}

	/* MDIO */
	if (SUNI1x10GEXP_BITMSK_TOP_MDIO_BUSY_INT & master_intr_status) {
		/* Not used. 8000 uses MDIO through Elmer. */
		/* EMPTY */
	}

	/* RXOAM */
	if (SUNI1x10GEXP_BITMSK_TOP_RXOAM_INT & master_intr_status) {
		/* EMPTY */
	}

	/* TXOAM */
	if (SUNI1x10GEXP_BITMSK_TOP_TXOAM_INT & master_intr_status) {
		/* EMPTY */
	}

	/* IFLX */
	if (SUNI1x10GEXP_BITMSK_TOP_IFLX_INT & master_intr_status) {
		/* EMPTY */
	}

	/* EFLX */
	if (SUNI1x10GEXP_BITMSK_TOP_EFLX_INT & master_intr_status) {
		/* EMPTY */
	}

	/* PL4ODP */
	if (SUNI1x10GEXP_BITMSK_TOP_PL4ODP_INT & master_intr_status) {
		/* EMPTY */
	}

	/* PL4IDU */
	if (SUNI1x10GEXP_BITMSK_TOP_PL4IDU_INT & master_intr_status) {
		/* EMPTY */
	}

	/* TBD XXX Lets just clear everything for now */
	(void) pm3393_interrupt_clear(cmac);

	return 0;
}

static int pm3393_enable(struct cmac *cmac, int which)
{
	if (which & MAC_DIRECTION_RX)
		(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_CONFIG_1,
			(RXXG_CONF1_VAL | SUNI1x10GEXP_BITMSK_RXXG_RXEN));

	if (which & MAC_DIRECTION_TX) {
		u32 val = TXXG_CONF1_VAL | SUNI1x10GEXP_BITMSK_TXXG_TXEN0;

		if (cmac->instance->fc & PAUSE_RX)
			val |= SUNI1x10GEXP_BITMSK_TXXG_FCRX;
		if (cmac->instance->fc & PAUSE_TX)
			val |= SUNI1x10GEXP_BITMSK_TXXG_FCTX;
		(void) pmwrite(cmac, SUNI1x10GEXP_REG_TXXG_CONFIG_1, val);
	}

	cmac->instance->enabled |= which;
	return 0;
}

/* ARGSUSED */
static int pm3393_enable_port(struct cmac *cmac, int which)
{
	/* Clear port statistics */
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_MSTAT_CONTROL,
		SUNI1x10GEXP_BITMSK_MSTAT_CLEAR);
	DELAY_US(2);
	(void) memset(&cmac->stats, 0, sizeof(struct cmac_statistics));

	(void) pm3393_enable(cmac, which);

	/*
	 * XXX This should be done by the PHY and preferrably not at all.
	 * The PHY doesn't give us link status indication on its own so have
	 * the link management code query it instead.
	 */
	{
		extern void link_changed(adapter_t *adapter, int port_id);
		link_changed(cmac->adapter, 0);
	}
	return 0;
}

static int pm3393_disable(struct cmac *cmac, int which)
{
	if (which & MAC_DIRECTION_RX)
		(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_CONFIG_1, RXXG_CONF1_VAL);
	if (which & MAC_DIRECTION_TX)
		(void) pmwrite(cmac, SUNI1x10GEXP_REG_TXXG_CONFIG_1, TXXG_CONF1_VAL);

	/*
	 * The disable is graceful. Give the PM3393 time.  Can't wait very
	 * long here, we may be holding locks.
	 */
	DELAY_US(20);

	cmac->instance->enabled &= ~which;
	return 0;
}

/* ARGSUSED */
static int pm3393_loopback_enable(struct cmac *cmac)
{
	return 0;
}

/* ARGSUSED */
static int pm3393_loopback_disable(struct cmac *cmac)
{
	return 0;
}

static int pm3393_set_mtu(struct cmac *cmac, int mtu)
{
	int enabled = cmac->instance->enabled;

	/* MAX_FRAME_SIZE includes header + FCS, mtu doesn't */
	mtu += 14 + 4;
	if (mtu > MAX_FRAME_SIZE)
		return -EINVAL;

	/* Disable Rx/Tx MAC before configuring it. */
	if (enabled)
		(void) pm3393_disable(cmac, MAC_DIRECTION_RX | MAC_DIRECTION_TX);

	(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_MAX_FRAME_LENGTH, mtu);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_TXXG_MAX_FRAME_SIZE, mtu);

	if (enabled)
		(void) pm3393_enable(cmac, enabled);
	return 0;
}

static u32 calc_crc(u8 *b, int len)
{
        int i;
	u32 crc = (u32)~0;

	/* calculate crc one bit at a time */
        while (len--) {
                crc ^= *b++;
                for (i = 0; i < 8; i++) {
			if (crc & 0x1)
				crc = (crc >> 1) ^ 0xedb88320;
			else
				crc = (crc >> 1);
		}
        }

	/* reverse bits */
        crc = ((crc >> 4) & 0x0f0f0f0f) | ((crc << 4) & 0xf0f0f0f0);
        crc = ((crc >> 2) & 0x33333333) | ((crc << 2) & 0xcccccccc);
        crc = ((crc >> 1) & 0x55555555) | ((crc << 1) & 0xaaaaaaaa);
	/* swap bytes */
        crc = (crc >> 16) | (crc << 16);
        crc = (crc >> 8 & 0x00ff00ff) | (crc << 8 & 0xff00ff00);

        return crc;
}

static int pm3393_set_rx_mode(struct cmac *cmac, struct t1_rx_mode *rm)
{
	int enabled = cmac->instance->enabled & MAC_DIRECTION_RX;
	u32 rx_mode;

	/* Disable MAC RX before reconfiguring it */
	if (enabled)
		(void) pm3393_disable(cmac, MAC_DIRECTION_RX);

	(void) pmread(cmac, SUNI1x10GEXP_REG_RXXG_ADDRESS_FILTER_CONTROL_2, &rx_mode);
	rx_mode &= ~(SUNI1x10GEXP_BITMSK_RXXG_PMODE | SUNI1x10GEXP_BITMSK_RXXG_MHASH_EN);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_ADDRESS_FILTER_CONTROL_2, (u16)rx_mode);

        if (t1_rx_mode_promisc(rm)) {
                /* Promiscuous mode. */
		rx_mode |= SUNI1x10GEXP_BITMSK_RXXG_PMODE;
        }
	if (t1_rx_mode_allmulti(rm)) {
                /* Accept all multicast. */
		(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_MULTICAST_HASH_LOW, 0xffff);
		(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_MULTICAST_HASH_MIDLOW, 0xffff);
		(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_MULTICAST_HASH_MIDHIGH, 0xffff);
		(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_MULTICAST_HASH_HIGH, 0xffff);
		rx_mode |= SUNI1x10GEXP_BITMSK_RXXG_MHASH_EN;
        } else if (t1_rx_mode_mc_cnt(rm)) {
                /* Accept one or more multicast(s). */
		u8 *addr;
		int bit;
		u16 mc_filter[4] = { 0, };

		while ((addr = t1_get_next_mcaddr(rm))) {
			bit = (calc_crc(addr, ETH_ALEN) >> 23) & 0x3f;	/* bit[23:28] */
			mc_filter[bit >> 4] |= 1 << (bit & 0xf);
		}
		(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_MULTICAST_HASH_LOW, mc_filter[0]);
		(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_MULTICAST_HASH_MIDLOW, mc_filter[1]);
		(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_MULTICAST_HASH_MIDHIGH, mc_filter[2]);
		(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_MULTICAST_HASH_HIGH, mc_filter[3]);
		rx_mode |= SUNI1x10GEXP_BITMSK_RXXG_MHASH_EN;
        }

	(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_ADDRESS_FILTER_CONTROL_2, (u16)rx_mode);

	if (enabled)
		(void) pm3393_enable(cmac, MAC_DIRECTION_RX);

	return 0;
}

static int pm3393_get_speed_duplex_fc(struct cmac *cmac, int *speed,
				      int *duplex, int *fc)
{
	if (speed)
		*speed = SPEED_10000;
	if (duplex)
		*duplex = DUPLEX_FULL;
	if (fc)
		*fc = cmac->instance->fc;
	return 0;
}

static int pm3393_set_speed_duplex_fc(struct cmac *cmac, int speed, int duplex,
				      int fc)
{
	if (speed >= 0 && speed != SPEED_10000)
		return -1;
	if (duplex >= 0 && duplex != DUPLEX_FULL)
		return -1;
	if (fc & ~(PAUSE_TX | PAUSE_RX))
		return -1;

	if (fc != cmac->instance->fc) {
		cmac->instance->fc = (u8) fc;
		if (cmac->instance->enabled & MAC_DIRECTION_TX)
			(void) pm3393_enable(cmac, MAC_DIRECTION_TX);
	}
	return 0;
}

#define RMON_UPDATE(mac, name, stat_name) \
	{ \
		(void) t1_tpi_read((mac)->adapter, OFFSET(name), &val0);	\
		(void) t1_tpi_read((mac)->adapter, OFFSET(((name)+1)), &val1); \
		(void) t1_tpi_read((mac)->adapter, OFFSET(((name)+2)), &val2); \
		(mac)->stats.stat_name = (u16)val0  | (((u16)val1) << 16) \
						   | ((u64)((u8)val2) << 32) \
						   | ((mac)->stats.stat_name & (~(u64)0 << 40)); \
		if (ro & ((name -  SUNI1x10GEXP_REG_MSTAT_COUNTER_0_LOW) >> 2)) \
			(mac)->stats.stat_name += ((u64)1 << 40); \
	}

/* ARGSUSED */
static const struct cmac_statistics *pm3393_update_statistics(struct cmac *mac,
							      int flag)
{
	u64	ro;
	u32	val0, val1, val2, val3;

	/* Snap the counters */
	(void) pmwrite(mac, SUNI1x10GEXP_REG_MSTAT_CONTROL,
		SUNI1x10GEXP_BITMSK_MSTAT_SNAP);

	/* Counter rollover, clear on read */ 
	(void) pmread(mac, SUNI1x10GEXP_REG_MSTAT_COUNTER_ROLLOVER_0, &val0);
	(void) pmread(mac, SUNI1x10GEXP_REG_MSTAT_COUNTER_ROLLOVER_1, &val1);
	(void) pmread(mac, SUNI1x10GEXP_REG_MSTAT_COUNTER_ROLLOVER_2, &val2);
	(void) pmread(mac, SUNI1x10GEXP_REG_MSTAT_COUNTER_ROLLOVER_3, &val3);
	ro = (u16)val0 | (((u16)val1) << 16) | ((u64)((u16)val2) << 32) 
	     | ((u64)((u16)val3) << 48);

	/* Rx stats */
	RMON_UPDATE(mac, RxOctetsReceivedOK, RxOctetsOK);
	RMON_UPDATE(mac, RxUnicastFramesReceivedOK, RxUnicastFramesOK);
	RMON_UPDATE(mac, RxMulticastFramesReceivedOK, RxMulticastFramesOK);
	RMON_UPDATE(mac, RxBroadcastFramesReceivedOK, RxBroadcastFramesOK);
	RMON_UPDATE(mac, RxPAUSEMACCtrlFramesReceived, RxPauseFrames);
	RMON_UPDATE(mac, RxFrameCheckSequenceErrors, RxFCSErrors);
	RMON_UPDATE(mac, RxFramesLostDueToInternalMACErrors, RxInternalMACRcvError);
	RMON_UPDATE(mac, RxSymbolErrors, RxSymbolErrors);
	RMON_UPDATE(mac, RxInRangeLengthErrors, RxInRangeLengthErrors);
	RMON_UPDATE(mac, RxFramesTooLongErrors , RxFrameTooLongErrors);
	RMON_UPDATE(mac, RxJabbers, RxJabberErrors);
	RMON_UPDATE(mac, RxFragments, RxRuntErrors);
	RMON_UPDATE(mac, RxUndersizedFrames, RxRuntErrors);

	/* Tx stats */
	RMON_UPDATE(mac, TxOctetsTransmittedOK, TxOctetsOK);
	RMON_UPDATE(mac, TxFramesLostDueToInternalMACTransmissionError, TxInternalMACXmitError);
	RMON_UPDATE(mac, TxTransmitSystemError, TxFCSErrors);
	RMON_UPDATE(mac, TxUnicastFramesTransmittedOK, TxUnicastFramesOK);
	RMON_UPDATE(mac, TxMulticastFramesTransmittedOK, TxMulticastFramesOK);
	RMON_UPDATE(mac, TxBroadcastFramesTransmittedOK, TxBroadcastFramesOK);
	RMON_UPDATE(mac, TxPAUSEMACCtrlFramesTransmitted, TxPauseFrames);

	return &mac->stats;
}

static int pm3393_macaddress_get(struct cmac *cmac, u8 mac_addr[6])
{
	memcpy(mac_addr, cmac->instance->mac_addr, 6);
	return 0;
}

static int pm3393_macaddress_set(struct cmac *cmac, u8 ma[6])
{
	u32 val, lo, mid, hi, enabled = cmac->instance->enabled;

	/*
	 * MAC addr: 00:07:43:00:13:09
	 *
	 * ma[5] = 0x09
	 * ma[4] = 0x13
	 * ma[3] = 0x00
	 * ma[2] = 0x43
	 * ma[1] = 0x07
	 * ma[0] = 0x00
	 *
	 * The PM3393 requires byte swapping and reverse order entry 
	 * when programming MAC addresses:
	 *
	 * low_bits[15:0]    = ma[1]:ma[0]
	 * mid_bits[31:16]   = ma[3]:ma[2]
	 * high_bits[47:32]  = ma[5]:ma[4]
	 */

	/* Store local copy */
	memcpy(cmac->instance->mac_addr, ma, 6);

	lo = ((u32) ma[1] << 8) | (u32) ma[0];
	mid = ((u32) ma[3] << 8) | (u32) ma[2];
	hi = ((u32) ma[5] << 8) | (u32) ma[4];

	/* Disable Rx/Tx MAC before configuring it. */
	if (enabled)
		(void) pm3393_disable(cmac, MAC_DIRECTION_RX | MAC_DIRECTION_TX);

	/* Set RXXG Station Address */
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_SA_15_0, lo);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_SA_31_16, mid);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_SA_47_32, hi);

	/* Set TXXG Station Address */
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_TXXG_SA_15_0, lo);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_TXXG_SA_31_16, mid);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_TXXG_SA_47_32, hi);

	/* Setup Exact Match Filter 1 with our MAC address
	 *
	 * Must disable exact match filter before configuring it.
	 */
	(void) pmread(cmac, SUNI1x10GEXP_REG_RXXG_ADDRESS_FILTER_CONTROL_0, &val);
	val &= 0xff0f;
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_ADDRESS_FILTER_CONTROL_0, val);

	(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_EXACT_MATCH_ADDR_1_LOW, lo);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_EXACT_MATCH_ADDR_1_MID, mid);
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_EXACT_MATCH_ADDR_1_HIGH, hi);

	val |= 0x0090;
	(void) pmwrite(cmac, SUNI1x10GEXP_REG_RXXG_ADDRESS_FILTER_CONTROL_0, val);

	if (enabled)
		(void) pm3393_enable(cmac, enabled);
	return 0;
}

static void pm3393_destroy(struct cmac *cmac)
{
	t1_os_free((void *)cmac, sizeof(*cmac) + sizeof(cmac_instance));
}

#ifdef C99_NOT_SUPPORTED
static struct cmac_ops pm3393_ops = {
	pm3393_destroy,
	pm3393_reset,
	pm3393_interrupt_enable,
	pm3393_interrupt_disable,
	pm3393_interrupt_clear,
	pm3393_interrupt_handler,
	pm3393_enable,
	pm3393_disable,
	pm3393_loopback_enable,
	pm3393_loopback_disable,
	pm3393_set_mtu,
	pm3393_set_rx_mode,
	pm3393_set_speed_duplex_fc,
	pm3393_get_speed_duplex_fc,
	pm3393_update_statistics,
	pm3393_macaddress_get,
	pm3393_macaddress_set
};
#else
static struct cmac_ops pm3393_ops = {
	.destroy                 = pm3393_destroy,
	.reset                   = pm3393_reset,
	.interrupt_enable        = pm3393_interrupt_enable,
	.interrupt_disable       = pm3393_interrupt_disable,
	.interrupt_clear         = pm3393_interrupt_clear,
	.interrupt_handler       = pm3393_interrupt_handler,
	.enable                  = pm3393_enable_port,
	.disable                 = pm3393_disable,
	.loopback_enable         = pm3393_loopback_enable,
	.loopback_disable        = pm3393_loopback_disable,
	.set_mtu                 = pm3393_set_mtu,
	.set_rx_mode             = pm3393_set_rx_mode,
	.get_speed_duplex_fc     = pm3393_get_speed_duplex_fc,
	.set_speed_duplex_fc     = pm3393_set_speed_duplex_fc,
	.statistics_update       = pm3393_update_statistics,
	.macaddress_get          = pm3393_macaddress_get,
	.macaddress_set          = pm3393_macaddress_set
};
#endif

/* ARGSUSED */
static struct cmac *pm3393_mac_create(adapter_t *adapter, int index)
{
	struct cmac *cmac;

	cmac = t1_os_malloc_wait_zero(sizeof(*cmac) + sizeof(cmac_instance));
	if (!cmac)
		return NULL;

	cmac->ops = &pm3393_ops;
	cmac->instance = (cmac_instance *) (cmac + 1);
	cmac->adapter = adapter;
	cmac->instance->fc = PAUSE_TX | PAUSE_RX;

	(void) t1_tpi_write(adapter, OFFSET(0x0001), 0x00008000);
	(void) t1_tpi_write(adapter, OFFSET(0x0001), 0x00000000);
	(void) t1_tpi_write(adapter, OFFSET(0x2308), 0x00009800);
	(void) t1_tpi_write(adapter, OFFSET(0x2305), 0x00001001);   /* PL4IO Enable */
	(void) t1_tpi_write(adapter, OFFSET(0x2320), 0x00008800);
	(void) t1_tpi_write(adapter, OFFSET(0x2321), 0x00008800);
	(void) t1_tpi_write(adapter, OFFSET(0x2322), 0x00008800);
	(void) t1_tpi_write(adapter, OFFSET(0x2323), 0x00008800);
	(void) t1_tpi_write(adapter, OFFSET(0x2324), 0x00008800);
	(void) t1_tpi_write(adapter, OFFSET(0x2325), 0x00008800);
	(void) t1_tpi_write(adapter, OFFSET(0x2326), 0x00008800);
	(void) t1_tpi_write(adapter, OFFSET(0x2327), 0x00008800);
	(void) t1_tpi_write(adapter, OFFSET(0x2328), 0x00008800);
	(void) t1_tpi_write(adapter, OFFSET(0x2329), 0x00008800);
	(void) t1_tpi_write(adapter, OFFSET(0x232a), 0x00008800);
	(void) t1_tpi_write(adapter, OFFSET(0x232b), 0x00008800);
	(void) t1_tpi_write(adapter, OFFSET(0x232c), 0x00008800);
	(void) t1_tpi_write(adapter, OFFSET(0x232d), 0x00008800);
	(void) t1_tpi_write(adapter, OFFSET(0x232e), 0x00008800);
	(void) t1_tpi_write(adapter, OFFSET(0x232f), 0x00008800);
	(void) t1_tpi_write(adapter, OFFSET(0x230d), 0x00009c00);
	(void) t1_tpi_write(adapter, OFFSET(0x2304), 0x00000202);	/* PL4IO Calendar Repetitions */

	(void) t1_tpi_write(adapter, OFFSET(0x3200), 0x00008080);	/* EFLX Enable */
	(void) t1_tpi_write(adapter, OFFSET(0x3210), 0x00000000);	/* EFLX Channel Deprovision */
	(void) t1_tpi_write(adapter, OFFSET(0x3203), 0x00000000);	/* EFLX Low Limit */
	(void) t1_tpi_write(adapter, OFFSET(0x3204), 0x00000040);	/* EFLX High Limit */
	(void) t1_tpi_write(adapter, OFFSET(0x3205), 0x000002cc);	/* EFLX Almost Full */
	(void) t1_tpi_write(adapter, OFFSET(0x3206), 0x00000199);	/* EFLX Almost Empty */
	(void) t1_tpi_write(adapter, OFFSET(0x3207), 0x00000240);	/* EFLX Cut Through Threshold */
	(void) t1_tpi_write(adapter, OFFSET(0x3202), 0x00000000);	/* EFLX Indirect Register Update */
	(void) t1_tpi_write(adapter, OFFSET(0x3210), 0x00000001);	/* EFLX Channel Provision */
	(void) t1_tpi_write(adapter, OFFSET(0x3208), 0x0000ffff);	/* EFLX Undocumented */
	(void) t1_tpi_write(adapter, OFFSET(0x320a), 0x0000ffff);	/* EFLX Undocumented */
	(void) t1_tpi_write(adapter, OFFSET(0x320c), 0x0000ffff);	/* EFLX enable overflow interrupt The other bit are undocumented */
	(void) t1_tpi_write(adapter, OFFSET(0x320e), 0x0000ffff);	/* EFLX Undocumented */

	(void) t1_tpi_write(adapter, OFFSET(0x2200), 0x0000c000);	/* IFLX Configuration - enable */
	(void) t1_tpi_write(adapter, OFFSET(0x2201), 0x00000000);	/* IFLX Channel Deprovision */
	(void) t1_tpi_write(adapter, OFFSET(0x220e), 0x00000000);	/* IFLX Low Limit */
	(void) t1_tpi_write(adapter, OFFSET(0x220f), 0x00000100);	/* IFLX High Limit */
	(void) t1_tpi_write(adapter, OFFSET(0x2210), 0x00000c00);	/* IFLX Almost Full Limit */
	(void) t1_tpi_write(adapter, OFFSET(0x2211), 0x00000599);	/* IFLX Almost Empty Limit */
	(void) t1_tpi_write(adapter, OFFSET(0x220d), 0x00000000);	/* IFLX Indirect Register Update */
	(void) t1_tpi_write(adapter, OFFSET(0x2201), 0x00000001);	/* IFLX Channel Provision */
	(void) t1_tpi_write(adapter, OFFSET(0x2203), 0x0000ffff);	/* IFLX Undocumented */
	(void) t1_tpi_write(adapter, OFFSET(0x2205), 0x0000ffff);	/* IFLX Undocumented */
	(void) t1_tpi_write(adapter, OFFSET(0x2209), 0x0000ffff);	/* IFLX Enable overflow interrupt.  The other bit are undocumented */

	(void) t1_tpi_write(adapter, OFFSET(0x2241), 0xfffffffe);	/* PL4MOS Undocumented */
	(void) t1_tpi_write(adapter, OFFSET(0x2242), 0x0000ffff);	/* PL4MOS Undocumented */
	(void) t1_tpi_write(adapter, OFFSET(0x2243), 0x00000008);	/* PL4MOS Starving Burst Size */
	(void) t1_tpi_write(adapter, OFFSET(0x2244), 0x00000008);	/* PL4MOS Hungry Burst Size */
	(void) t1_tpi_write(adapter, OFFSET(0x2245), 0x00000008);	/* PL4MOS Transfer Size */
	(void) t1_tpi_write(adapter, OFFSET(0x2240), 0x00000005);	/* PL4MOS Disable */

	(void) t1_tpi_write(adapter, OFFSET(0x2280), 0x00002103);	/* PL4ODP Training Repeat and SOP rule */
	(void) t1_tpi_write(adapter, OFFSET(0x2284), 0x00000000);	/* PL4ODP MAX_T setting */

	(void) t1_tpi_write(adapter, OFFSET(0x3280), 0x00000087);	/* PL4IDU Enable data forward, port state machine. Set ALLOW_NON_ZERO_OLB */
	(void) t1_tpi_write(adapter, OFFSET(0x3282), 0x0000001f);	/* PL4IDU Enable Dip4 check error interrupts */

	(void) t1_tpi_write(adapter, OFFSET(0x3040), 0x0c32);	/* # TXXG Config */
	/* For T1 use timer based Mac flow control. */
	(void) t1_tpi_write(adapter, OFFSET(0x304d), 0x8000);
	(void) t1_tpi_write(adapter, OFFSET(0x2040), 0x059c);	/* # RXXG Config */
	(void) t1_tpi_write(adapter, OFFSET(0x2049), 0x0001);	/* # RXXG Cut Through */
	(void) t1_tpi_write(adapter, OFFSET(0x2070), 0x0000);	/* # Disable promiscuous mode */

	/* Setup Exact Match Filter 0 to allow broadcast packets.
	 */
	(void) t1_tpi_write(adapter, OFFSET(0x206e), 0x0000);	/* # Disable Match Enable bit */
	(void) t1_tpi_write(adapter, OFFSET(0x204a), 0xffff);	/* # low addr */
	(void) t1_tpi_write(adapter, OFFSET(0x204b), 0xffff);	/* # mid addr */
	(void) t1_tpi_write(adapter, OFFSET(0x204c), 0xffff);	/* # high addr */
	(void) t1_tpi_write(adapter, OFFSET(0x206e), 0x0009);	/* # Enable Match Enable bit */

	(void) t1_tpi_write(adapter, OFFSET(0x0003), 0x0000);	/* # NO SOP/ PAD_EN setup */
	(void) t1_tpi_write(adapter, OFFSET(0x0100), 0x0ff0);	/* # RXEQB disabled */
	(void) t1_tpi_write(adapter, OFFSET(0x0101), 0x0f0f);	/* # No Preemphasis */

	return cmac;
}

static int pm3393_mac_reset(adapter_t * adapter)
{
	u32 val;
	u32 x;
	u32 is_pl4_reset_finished;
	u32 is_pl4_outof_lock;
	u32 is_xaui_mabc_pll_locked;
	u32 successful_reset;
	int i;

	/* The following steps are required to properly reset
	 * the PM3393. This information is provided in the
	 * PM3393 datasheet (Issue 2: November 2002)
	 * section 13.1 -- Device Reset.
	 *
	 * The PM3393 has three types of components that are
	 * individually reset:
	 *
	 * DRESETB      - Digital circuitry
	 * PL4_ARESETB  - PL4 analog circuitry
	 * XAUI_ARESETB - XAUI bus analog circuitry
	 *
	 * Steps to reset PM3393 using RSTB pin:
	 *
	 * 1. Assert RSTB pin low ( write 0 )
	 * 2. Wait at least 1ms to initiate a complete initialization of device.
	 * 3. Wait until all external clocks and REFSEL are stable.
	 * 4. Wait minimum of 1ms. (after external clocks and REFEL are stable)
	 * 5. De-assert RSTB ( write 1 )
	 * 6. Wait until internal timers to expires after ~14ms.
	 *    - Allows analog clock synthesizer(PL4CSU) to stabilize to 
	 *      selected reference frequency before allowing the digital
	 *      portion of the device to operate.
	 * 7. Wait at least 200us for XAUI interface to stabilize.
	 * 8. Verify the PM3393 came out of reset successfully.
	 *    Set successful reset flag if everything worked else try again
	 *    a few more times.
	 */

	successful_reset = 0;
	for (i = 0; i < 3 && !successful_reset; i++) {
		/* 1 */
		(void) t1_tpi_read(adapter, A_ELMER0_GPO, &val);
		val &= ~1;
		(void) t1_tpi_write(adapter, A_ELMER0_GPO, val);

		/* 2 */
		DELAY_MS(1);

		/* 3 */
		DELAY_MS(1);

		/* 4 */
		DELAY_MS(2 /*1 extra ms for safety */ );

		/* 5 */
		val |= 1;
		(void) t1_tpi_write(adapter, A_ELMER0_GPO, val);

		/* 6 */
		DELAY_MS(15 /*1 extra ms for safety */ );

		/* 7 */
		DELAY_MS(1);

		/* 8 */

		/* Has PL4 analog block come out of reset correctly? */
		(void) t1_tpi_read(adapter, OFFSET(SUNI1x10GEXP_REG_DEVICE_STATUS), &val);
		is_pl4_reset_finished = (val & SUNI1x10GEXP_BITMSK_TOP_EXPIRED);

		/* TBD XXX SUNI1x10GEXP_BITMSK_TOP_PL4_IS_DOOL gets locked later in the init sequence
		 *         figure out why? */

		/* Have all PL4 block clocks locked? */
		x = (SUNI1x10GEXP_BITMSK_TOP_PL4_ID_DOOL
		     /*| SUNI1x10GEXP_BITMSK_TOP_PL4_IS_DOOL */  |
		     SUNI1x10GEXP_BITMSK_TOP_PL4_ID_ROOL |
		     SUNI1x10GEXP_BITMSK_TOP_PL4_IS_ROOL |
		     SUNI1x10GEXP_BITMSK_TOP_PL4_OUT_ROOL);
		is_pl4_outof_lock = (val & x);

		/* ??? If this fails, might be able to software reset the XAUI part
		 *     and try to recover... thus saving us from doing another HW reset */
		/* Has the XAUI MABC PLL circuitry stablized? */
		is_xaui_mabc_pll_locked =
		    (val & SUNI1x10GEXP_BITMSK_TOP_SXRA_EXPIRED);

		successful_reset = (is_pl4_reset_finished && !is_pl4_outof_lock
				    && is_xaui_mabc_pll_locked);

		CH_DBG(adapter, HW,
		       "PM3393 HW reset %d: pl4_reset 0x%x, val 0x%x, "
		       "is_pl4_outof_lock 0x%x, xaui_locked 0x%x\n",
		       i, is_pl4_reset_finished, val, is_pl4_outof_lock,
		       is_xaui_mabc_pll_locked);
	}
	return successful_reset ? 0 : 1;
}

struct gmac t1_pm3393_ops = {
	STATS_TICK_SECS,
	pm3393_mac_create,
	pm3393_mac_reset
};
