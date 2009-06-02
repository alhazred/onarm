/*
 * This file is provided under a CDDLv1 license.  When using or
 * redistributing this file, you may do so under this license.
 * In redistributing this file this license must be included
 * and no other modification of this header file is permitted.
 *
 * CDDL LICENSE SUMMARY
 *
 * Copyright(c) 1999 - 2008 Intel Corporation. All rights reserved.
 *
 * The contents of this file are subject to the terms of Version
 * 1.0 of the Common Development and Distribution License (the "License").
 *
 * You should have received a copy of the License with this software.
 * You can obtain a copy of the License at
 *	http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms of the CDDLv1.
 */

/*
 * Copyright (c) 2008 NEC Corporation
 */

#pragma ident	"@(#)e1000g_stat.c	1.20	08/01/21 SMI"

/*
 * **********************************************************************
 *									*
 * Module Name:  e1000g_stat.c						*
 *									*
 * Abstract: Functions for processing statistics			*
 *									*
 * **********************************************************************
 */
#include "e1000g_sw.h"
#include "e1000g_debug.h"

static int e1000g_update_stats(kstat_t *ksp, int rw);

/*
 * e1000_tbi_adjust_stats
 *
 * Adjusts statistic counters when a frame is accepted
 * under the TBI workaround. This function has been
 * adapted for Solaris from shared code.
 */
void
e1000_tbi_adjust_stats(struct e1000g *Adapter,
    uint32_t frame_len, uint8_t *mac_addr)
{
	struct e1000_hw *hw = &Adapter->shared;
	uint32_t carry_bit;
	p_e1000g_stat_t e1000g_ksp;

	if ((Adapter->e1000g_ksp == NULL) ||
		(Adapter->e1000g_ksp->ks_data == NULL)) {
		return;
	}

	e1000g_ksp = (p_e1000g_stat_t)Adapter->e1000g_ksp->ks_data;

	/* First adjust the frame length */
	frame_len--;

	/*
	 * We need to adjust the statistics counters, since the hardware
	 * counters overcount this packet as a CRC error and undercount
	 * the packet as a good packet
	 */
	/* This packet should not be counted as a CRC error */
	e1000g_ksp->Crcerrs.value.ul--;
	/* This packet does count as a Good Packet Received */
	e1000g_ksp->Gprc.value.ul++;

	/*
	 * Adjust the Good Octets received counters
	 */
	carry_bit = 0x80000000 & e1000g_ksp->Gorl.value.ul;
	e1000g_ksp->Gorl.value.ul += frame_len;
	/*
	 * If the high bit of Gorcl (the low 32 bits of the Good Octets
	 * Received Count) was one before the addition,
	 * AND it is zero after, then we lost the carry out,
	 * need to add one to Gorch (Good Octets Received Count High).
	 * This could be simplified if all environments supported
	 * 64-bit integers.
	 */
	if (carry_bit && ((e1000g_ksp->Gorl.value.ul & 0x80000000) == 0)) {
		e1000g_ksp->Gorh.value.ul++;
	}
	/*
	 * Is this a broadcast or multicast?  Check broadcast first,
	 * since the test for a multicast frame will test positive on
	 * a broadcast frame.
	 */
	if ((mac_addr[0] == (uint8_t)0xff) &&
	    (mac_addr[1] == (uint8_t)0xff)) {
		/*
		 * Broadcast packet
		 */
		e1000g_ksp->Bprc.value.ul++;
	} else if (*mac_addr & 0x01) {
		/*
		 * Multicast packet
		 */
		e1000g_ksp->Mprc.value.ul++;
	}

	if (frame_len == hw->mac.max_frame_size) {
		/*
		 * In this case, the hardware has overcounted the number of
		 * oversize frames.
		 */
		if (e1000g_ksp->Roc.value.ul > 0)
			e1000g_ksp->Roc.value.ul--;
	}

#ifdef E1000G_DEBUG
	/*
	 * Adjust the bin counters when the extra byte put the frame in the
	 * wrong bin. Remember that the frame_len was adjusted above.
	 */
	if (frame_len == 64) {
		e1000g_ksp->Prc64.value.ul++;
		e1000g_ksp->Prc127.value.ul--;
	} else if (frame_len == 127) {
		e1000g_ksp->Prc127.value.ul++;
		e1000g_ksp->Prc255.value.ul--;
	} else if (frame_len == 255) {
		e1000g_ksp->Prc255.value.ul++;
		e1000g_ksp->Prc511.value.ul--;
	} else if (frame_len == 511) {
		e1000g_ksp->Prc511.value.ul++;
		e1000g_ksp->Prc1023.value.ul--;
	} else if (frame_len == 1023) {
		e1000g_ksp->Prc1023.value.ul++;
		e1000g_ksp->Prc1522.value.ul--;
	} else if (frame_len == 1522) {
		e1000g_ksp->Prc1522.value.ul++;
	}
#endif
}


/*
 * e1000g_update_stats - update driver private kstat counters
 *
 * This routine will dump and reset the e1000's internal
 * statistics counters. The current stats dump values will
 * be sent to the kernel status area.
 */
static int
e1000g_update_stats(kstat_t *ksp, int rw)
{
	struct e1000g *Adapter;
	struct e1000_hw *hw;
	p_e1000g_stat_t e1000g_ksp;
	e1000g_tx_ring_t *tx_ring;
	e1000g_rx_ring_t *rx_ring;
	uint64_t val;
	uint32_t low_val, high_val;

	if (rw == KSTAT_WRITE)
		return (EACCES);

	Adapter = (struct e1000g *)ksp->ks_private;
	ASSERT(Adapter != NULL);
	e1000g_ksp = (p_e1000g_stat_t)ksp->ks_data;
	ASSERT(e1000g_ksp != NULL);
	hw = &Adapter->shared;

	tx_ring = Adapter->tx_ring;
	rx_ring = Adapter->rx_ring;

	rw_enter(&Adapter->chip_lock, RW_WRITER);

	e1000g_ksp->link_speed.value.ul = Adapter->link_speed;
	e1000g_ksp->reset_count.value.ul = Adapter->reset_count;

	e1000g_ksp->rx_error.value.ul = rx_ring->stat_error;
	e1000g_ksp->rx_esballoc_fail.value.ul = rx_ring->stat_esballoc_fail;
	e1000g_ksp->rx_allocb_fail.value.ul = rx_ring->stat_allocb_fail;

	e1000g_ksp->tx_no_swpkt.value.ul = tx_ring->stat_no_swpkt;
	e1000g_ksp->tx_no_desc.value.ul = tx_ring->stat_no_desc;
	e1000g_ksp->tx_send_fail.value.ul = tx_ring->stat_send_fail;
	e1000g_ksp->tx_reschedule.value.ul = tx_ring->stat_reschedule;
	e1000g_ksp->tx_over_size.value.ul = tx_ring->stat_over_size;

#ifdef E1000G_DEBUG
	e1000g_ksp->rx_none.value.ul = rx_ring->stat_none;
	e1000g_ksp->rx_multi_desc.value.ul = rx_ring->stat_multi_desc;
	e1000g_ksp->rx_no_freepkt.value.ul = rx_ring->stat_no_freepkt;
	e1000g_ksp->rx_avail_freepkt.value.ul = rx_ring->avail_freepkt;

	e1000g_ksp->tx_under_size.value.ul = tx_ring->stat_under_size;
	e1000g_ksp->tx_exceed_frags.value.ul = tx_ring->stat_exceed_frags;
	e1000g_ksp->tx_empty_frags.value.ul = tx_ring->stat_empty_frags;
	e1000g_ksp->tx_recycle.value.ul = tx_ring->stat_recycle;
	e1000g_ksp->tx_recycle_intr.value.ul = tx_ring->stat_recycle_intr;
	e1000g_ksp->tx_recycle_retry.value.ul = tx_ring->stat_recycle_retry;
	e1000g_ksp->tx_recycle_none.value.ul = tx_ring->stat_recycle_none;
	e1000g_ksp->tx_copy.value.ul = tx_ring->stat_copy;
	e1000g_ksp->tx_bind.value.ul = tx_ring->stat_bind;
	e1000g_ksp->tx_multi_copy.value.ul = tx_ring->stat_multi_copy;
	e1000g_ksp->tx_multi_cookie.value.ul = tx_ring->stat_multi_cookie;
	e1000g_ksp->tx_lack_desc.value.ul = tx_ring->stat_lack_desc;
#endif

	/*
	 * Standard Stats
	 */
	e1000g_ksp->Mpc.value.ul += E1000_READ_REG(hw, E1000_MPC);
	e1000g_ksp->Rlec.value.ul += E1000_READ_REG(hw, E1000_RLEC);
	e1000g_ksp->Xonrxc.value.ul += E1000_READ_REG(hw, E1000_XONRXC);
	e1000g_ksp->Xontxc.value.ul += E1000_READ_REG(hw, E1000_XONTXC);
	e1000g_ksp->Xoffrxc.value.ul += E1000_READ_REG(hw, E1000_XOFFRXC);
	e1000g_ksp->Xofftxc.value.ul += E1000_READ_REG(hw, E1000_XOFFTXC);
	e1000g_ksp->Fcruc.value.ul += E1000_READ_REG(hw, E1000_FCRUC);

	if ((hw->mac.type != e1000_ich8lan) &&
	    (hw->mac.type != e1000_ich9lan)) {
		e1000g_ksp->Symerrs.value.ul +=
		    E1000_READ_REG(hw, E1000_SYMERRS);
#ifdef E1000G_DEBUG
		e1000g_ksp->Prc64.value.ul +=
		    E1000_READ_REG(hw, E1000_PRC64);
		e1000g_ksp->Prc127.value.ul +=
		    E1000_READ_REG(hw, E1000_PRC127);
		e1000g_ksp->Prc255.value.ul +=
		    E1000_READ_REG(hw, E1000_PRC255);
		e1000g_ksp->Prc511.value.ul +=
		    E1000_READ_REG(hw, E1000_PRC511);
		e1000g_ksp->Prc1023.value.ul +=
		    E1000_READ_REG(hw, E1000_PRC1023);
		e1000g_ksp->Prc1522.value.ul +=
		    E1000_READ_REG(hw, E1000_PRC1522);

		e1000g_ksp->Ptc64.value.ul +=
		    E1000_READ_REG(hw, E1000_PTC64);
		e1000g_ksp->Ptc127.value.ul +=
		    E1000_READ_REG(hw, E1000_PTC127);
		e1000g_ksp->Ptc255.value.ul +=
		    E1000_READ_REG(hw, E1000_PTC255);
		e1000g_ksp->Ptc511.value.ul +=
		    E1000_READ_REG(hw, E1000_PTC511);
		e1000g_ksp->Ptc1023.value.ul +=
		    E1000_READ_REG(hw, E1000_PTC1023);
		e1000g_ksp->Ptc1522.value.ul +=
		    E1000_READ_REG(hw, E1000_PTC1522);
#endif
	}

	e1000g_ksp->Gprc.value.ul += E1000_READ_REG(hw, E1000_GPRC);
	e1000g_ksp->Gptc.value.ul += E1000_READ_REG(hw, E1000_GPTC);
	e1000g_ksp->Ruc.value.ul += E1000_READ_REG(hw, E1000_RUC);
	e1000g_ksp->Rfc.value.ul += E1000_READ_REG(hw, E1000_RFC);
	e1000g_ksp->Roc.value.ul += E1000_READ_REG(hw, E1000_ROC);
	e1000g_ksp->Rjc.value.ul += E1000_READ_REG(hw, E1000_RJC);
	e1000g_ksp->Tpr.value.ul += E1000_READ_REG(hw, E1000_TPR);
	e1000g_ksp->Tncrs.value.ul += E1000_READ_REG(hw, E1000_TNCRS);
	e1000g_ksp->Tsctc.value.ul += E1000_READ_REG(hw, E1000_TSCTC);
	e1000g_ksp->Tsctfc.value.ul += E1000_READ_REG(hw, E1000_TSCTFC);

	/*
	 * Adaptive Calculations
	 */
	hw->mac.tx_packet_delta = E1000_READ_REG(hw, E1000_TPT);
	e1000g_ksp->Tpt.value.ul += hw->mac.tx_packet_delta;

	/*
	 * The 64-bit register will reset whenever the upper
	 * 32 bits are read. So we need to read the lower
	 * 32 bits first, then read the upper 32 bits.
	 */
	low_val = E1000_READ_REG(hw, E1000_GORCL);
	high_val = E1000_READ_REG(hw, E1000_GORCH);
	val = (uint64_t)e1000g_ksp->Gorh.value.ul << 32 |
	    (uint64_t)e1000g_ksp->Gorl.value.ul;
	val += (uint64_t)high_val << 32 | (uint64_t)low_val;
	e1000g_ksp->Gorl.value.ul = (uint32_t)val;
	e1000g_ksp->Gorh.value.ul = (uint32_t)(val >> 32);

	low_val = E1000_READ_REG(hw, E1000_GOTCL);
	high_val = E1000_READ_REG(hw, E1000_GOTCH);
	val = (uint64_t)e1000g_ksp->Goth.value.ul << 32 |
	    (uint64_t)e1000g_ksp->Gotl.value.ul;
	val += (uint64_t)high_val << 32 | (uint64_t)low_val;
	e1000g_ksp->Gotl.value.ul = (uint32_t)val;
	e1000g_ksp->Goth.value.ul = (uint32_t)(val >> 32);

	low_val = E1000_READ_REG(hw, E1000_TORL);
	high_val = E1000_READ_REG(hw, E1000_TORH);
	val = (uint64_t)e1000g_ksp->Torh.value.ul << 32 |
	    (uint64_t)e1000g_ksp->Torl.value.ul;
	val += (uint64_t)high_val << 32 | (uint64_t)low_val;
	e1000g_ksp->Torl.value.ul = (uint32_t)val;
	e1000g_ksp->Torh.value.ul = (uint32_t)(val >> 32);

	low_val = E1000_READ_REG(hw, E1000_TOTL);
	high_val = E1000_READ_REG(hw, E1000_TOTH);
	val = (uint64_t)e1000g_ksp->Toth.value.ul << 32 |
	    (uint64_t)e1000g_ksp->Totl.value.ul;
	val += (uint64_t)high_val << 32 | (uint64_t)low_val;
	e1000g_ksp->Totl.value.ul = (uint32_t)val;
	e1000g_ksp->Toth.value.ul = (uint32_t)(val >> 32);

	rw_exit(&Adapter->chip_lock);

	if (e1000g_check_acc_handle(Adapter->osdep.reg_handle) != DDI_FM_OK)
		ddi_fm_service_impact(Adapter->dip, DDI_SERVICE_UNAFFECTED);

	return (0);
}

int
e1000g_m_stat(void *arg, uint_t stat, uint64_t *val)
{
	struct e1000g *Adapter = (struct e1000g *)arg;
	struct e1000_hw *hw = &Adapter->shared;
	p_e1000g_stat_t e1000g_ksp;
	uint32_t low_val, high_val;

	if ((Adapter->e1000g_ksp == NULL) ||
		(Adapter->e1000g_ksp->ks_data == NULL)) {
		return (0);
	}

	e1000g_ksp = (p_e1000g_stat_t)Adapter->e1000g_ksp->ks_data;

	rw_enter(&Adapter->chip_lock, RW_READER);

	switch (stat) {
	case MAC_STAT_IFSPEED:
		*val = Adapter->link_speed * 1000000ull;
		break;

	case MAC_STAT_MULTIRCV:
		e1000g_ksp->Mprc.value.ul +=
		    E1000_READ_REG(hw, E1000_MPRC);
		*val = e1000g_ksp->Mprc.value.ul;
		break;

	case MAC_STAT_BRDCSTRCV:
		e1000g_ksp->Bprc.value.ul +=
		    E1000_READ_REG(hw, E1000_BPRC);
		*val = e1000g_ksp->Bprc.value.ul;
		break;

	case MAC_STAT_MULTIXMT:
		e1000g_ksp->Mptc.value.ul +=
		    E1000_READ_REG(hw, E1000_MPTC);
		*val = e1000g_ksp->Mptc.value.ul;
		break;

	case MAC_STAT_BRDCSTXMT:
		e1000g_ksp->Bptc.value.ul +=
		    E1000_READ_REG(hw, E1000_BPTC);
		*val = e1000g_ksp->Bptc.value.ul;
		break;

	case MAC_STAT_NORCVBUF:
		e1000g_ksp->Rnbc.value.ul +=
		    E1000_READ_REG(hw, E1000_RNBC);
		*val = e1000g_ksp->Rnbc.value.ul;
		break;

	case MAC_STAT_IERRORS:
		e1000g_ksp->Rxerrc.value.ul +=
		    E1000_READ_REG(hw, E1000_RXERRC);
		e1000g_ksp->Algnerrc.value.ul +=
		    E1000_READ_REG(hw, E1000_ALGNERRC);
		e1000g_ksp->Rlec.value.ul +=
		    E1000_READ_REG(hw, E1000_RLEC);
		e1000g_ksp->Crcerrs.value.ul +=
		    E1000_READ_REG(hw, E1000_CRCERRS);
		e1000g_ksp->Cexterr.value.ul +=
		    E1000_READ_REG(hw, E1000_CEXTERR);
		*val = e1000g_ksp->Rxerrc.value.ul +
		    e1000g_ksp->Algnerrc.value.ul +
		    e1000g_ksp->Rlec.value.ul +
		    e1000g_ksp->Crcerrs.value.ul +
		    e1000g_ksp->Cexterr.value.ul;
		break;

	case MAC_STAT_NOXMTBUF:
		*val = Adapter->tx_ring->stat_no_desc;
		break;

	case MAC_STAT_OERRORS:
		e1000g_ksp->Ecol.value.ul +=
		    E1000_READ_REG(hw, E1000_ECOL);
		*val = e1000g_ksp->Ecol.value.ul;
		break;

	case MAC_STAT_COLLISIONS:
		e1000g_ksp->Colc.value.ul +=
		    E1000_READ_REG(hw, E1000_COLC);
		*val = e1000g_ksp->Colc.value.ul;
		break;

	case MAC_STAT_RBYTES:
		/*
		 * The 64-bit register will reset whenever the upper
		 * 32 bits are read. So we need to read the lower
		 * 32 bits first, then read the upper 32 bits.
		 */
		low_val = E1000_READ_REG(hw, E1000_TORL);
		high_val = E1000_READ_REG(hw, E1000_TORH);
		*val = (uint64_t)e1000g_ksp->Torh.value.ul << 32 |
		    (uint64_t)e1000g_ksp->Torl.value.ul;
		*val += (uint64_t)high_val << 32 | (uint64_t)low_val;

		e1000g_ksp->Torl.value.ul = (uint32_t)*val;
		e1000g_ksp->Torh.value.ul = (uint32_t)(*val >> 32);
		break;

	case MAC_STAT_IPACKETS:
		e1000g_ksp->Tpr.value.ul +=
		    E1000_READ_REG(hw, E1000_TPR);
		*val = e1000g_ksp->Tpr.value.ul;
		break;

	case MAC_STAT_OBYTES:
		/*
		 * The 64-bit register will reset whenever the upper
		 * 32 bits are read. So we need to read the lower
		 * 32 bits first, then read the upper 32 bits.
		 */
		low_val = E1000_READ_REG(hw, E1000_TOTL);
		high_val = E1000_READ_REG(hw, E1000_TOTH);
		*val = (uint64_t)e1000g_ksp->Toth.value.ul << 32 |
		    (uint64_t)e1000g_ksp->Totl.value.ul;
		*val += (uint64_t)high_val << 32 | (uint64_t)low_val;

		e1000g_ksp->Totl.value.ul = (uint32_t)*val;
		e1000g_ksp->Toth.value.ul = (uint32_t)(*val >> 32);
		break;

	case MAC_STAT_OPACKETS:
		e1000g_ksp->Tpt.value.ul +=
		    E1000_READ_REG(hw, E1000_TPT);
		*val = e1000g_ksp->Tpt.value.ul;
		break;

	case ETHER_STAT_ALIGN_ERRORS:
		e1000g_ksp->Algnerrc.value.ul +=
		    E1000_READ_REG(hw, E1000_ALGNERRC);
		*val = e1000g_ksp->Algnerrc.value.ul;
		break;

	case ETHER_STAT_FCS_ERRORS:
		e1000g_ksp->Crcerrs.value.ul +=
		    E1000_READ_REG(hw, E1000_CRCERRS);
		*val = e1000g_ksp->Crcerrs.value.ul;
		break;

	case ETHER_STAT_SQE_ERRORS:
		e1000g_ksp->Sec.value.ul +=
		    E1000_READ_REG(hw, E1000_SEC);
		*val = e1000g_ksp->Sec.value.ul;
		break;

	case ETHER_STAT_CARRIER_ERRORS:
		e1000g_ksp->Cexterr.value.ul +=
		    E1000_READ_REG(hw, E1000_CEXTERR);
		*val = e1000g_ksp->Cexterr.value.ul;
		break;

	case ETHER_STAT_EX_COLLISIONS:
		e1000g_ksp->Ecol.value.ul +=
		    E1000_READ_REG(hw, E1000_ECOL);
		*val = e1000g_ksp->Ecol.value.ul;
		break;

	case ETHER_STAT_TX_LATE_COLLISIONS:
		e1000g_ksp->Latecol.value.ul +=
		    E1000_READ_REG(hw, E1000_LATECOL);
		*val = e1000g_ksp->Latecol.value.ul;
		break;

	case ETHER_STAT_DEFER_XMTS:
		e1000g_ksp->Dc.value.ul +=
		    E1000_READ_REG(hw, E1000_DC);
		*val = e1000g_ksp->Dc.value.ul;
		break;

	case ETHER_STAT_FIRST_COLLISIONS:
		e1000g_ksp->Scc.value.ul +=
		    E1000_READ_REG(hw, E1000_SCC);
		*val = e1000g_ksp->Scc.value.ul;
		break;

	case ETHER_STAT_MULTI_COLLISIONS:
		e1000g_ksp->Mcc.value.ul +=
		    E1000_READ_REG(hw, E1000_MCC);
		*val = e1000g_ksp->Mcc.value.ul;
		break;

	case ETHER_STAT_MACRCV_ERRORS:
		e1000g_ksp->Rxerrc.value.ul +=
		    E1000_READ_REG(hw, E1000_RXERRC);
		*val = e1000g_ksp->Rxerrc.value.ul;
		break;

	case ETHER_STAT_MACXMT_ERRORS:
		e1000g_ksp->Ecol.value.ul +=
		    E1000_READ_REG(hw, E1000_ECOL);
		*val = e1000g_ksp->Ecol.value.ul;
		break;

	case ETHER_STAT_TOOLONG_ERRORS:
		e1000g_ksp->Roc.value.ul +=
		    E1000_READ_REG(hw, E1000_ROC);
		*val = e1000g_ksp->Roc.value.ul;
		break;

	case ETHER_STAT_XCVR_ADDR:
		/* The Internal PHY's MDI address for each MAC is 1 */
		*val = 1;
		break;

	case ETHER_STAT_XCVR_ID:
		*val = hw->phy.id | hw->phy.revision;
		break;

	case ETHER_STAT_XCVR_INUSE:
		switch (Adapter->link_speed) {
		case SPEED_1000:
			*val =
			    (hw->media_type == e1000_media_type_copper) ?
			    XCVR_1000T : XCVR_1000X;
			break;
		case SPEED_100:
			*val =
			    (hw->media_type == e1000_media_type_copper) ?
			    (Adapter->phy_status & MII_SR_100T4_CAPS) ?
			    XCVR_100T4 : XCVR_100T2 : XCVR_100X;
			break;
		case SPEED_10:
			*val = XCVR_10;
			break;
		default:
			*val = XCVR_NONE;
			break;
		}
		break;

	case ETHER_STAT_CAP_1000FDX:
		*val = ((Adapter->phy_ext_status & IEEE_ESR_1000T_FD_CAPS) ||
		    (Adapter->phy_ext_status & IEEE_ESR_1000X_FD_CAPS)) ? 1 : 0;
		break;

	case ETHER_STAT_CAP_1000HDX:
		*val = ((Adapter->phy_ext_status & IEEE_ESR_1000T_HD_CAPS) ||
		    (Adapter->phy_ext_status & IEEE_ESR_1000X_HD_CAPS)) ? 1 : 0;
		break;

	case ETHER_STAT_CAP_100FDX:
		*val = ((Adapter->phy_status & MII_SR_100X_FD_CAPS) ||
		    (Adapter->phy_status & MII_SR_100T2_FD_CAPS)) ? 1 : 0;
		break;

	case ETHER_STAT_CAP_100HDX:
		*val = ((Adapter->phy_status & MII_SR_100X_HD_CAPS) ||
		    (Adapter->phy_status & MII_SR_100T2_HD_CAPS)) ? 1 : 0;
		break;

	case ETHER_STAT_CAP_10FDX:
		*val = (Adapter->phy_status & MII_SR_10T_FD_CAPS) ? 1 : 0;
		break;

	case ETHER_STAT_CAP_10HDX:
		*val = (Adapter->phy_status & MII_SR_10T_HD_CAPS) ? 1 : 0;
		break;

	case ETHER_STAT_CAP_ASMPAUSE:
		*val = (Adapter->phy_an_adv & NWAY_AR_ASM_DIR) ? 1 : 0;
		break;

	case ETHER_STAT_CAP_PAUSE:
		*val = (Adapter->phy_an_adv & NWAY_AR_PAUSE) ? 1 : 0;
		break;

	case ETHER_STAT_CAP_AUTONEG:
		*val = (Adapter->phy_status & MII_SR_AUTONEG_CAPS) ? 1 : 0;
		break;

	case ETHER_STAT_ADV_CAP_1000FDX:
		*val = (Adapter->phy_1000t_ctrl & CR_1000T_FD_CAPS) ? 1 : 0;
		break;

	case ETHER_STAT_ADV_CAP_1000HDX:
		*val = (Adapter->phy_1000t_ctrl & CR_1000T_HD_CAPS) ? 1 : 0;
		break;

	case ETHER_STAT_ADV_CAP_100FDX:
		*val = (Adapter->phy_an_adv & NWAY_AR_100TX_FD_CAPS) ? 1 : 0;
		break;

	case ETHER_STAT_ADV_CAP_100HDX:
		*val = (Adapter->phy_an_adv & NWAY_AR_100TX_HD_CAPS) ? 1 : 0;
		break;

	case ETHER_STAT_ADV_CAP_10FDX:
		*val = (Adapter->phy_an_adv & NWAY_AR_10T_FD_CAPS) ? 1 : 0;
		break;

	case ETHER_STAT_ADV_CAP_10HDX:
		*val = (Adapter->phy_an_adv & NWAY_AR_10T_HD_CAPS) ? 1 : 0;
		break;

	case ETHER_STAT_ADV_CAP_ASMPAUSE:
		*val = (Adapter->phy_an_adv & NWAY_AR_ASM_DIR) ? 1 : 0;
		break;

	case ETHER_STAT_ADV_CAP_PAUSE:
		*val = (Adapter->phy_an_adv & NWAY_AR_PAUSE) ? 1 : 0;
		break;

	case ETHER_STAT_ADV_CAP_AUTONEG:
		*val = hw->mac.autoneg;
		break;

	case ETHER_STAT_LP_CAP_1000FDX:
		*val =
		    (Adapter->phy_1000t_status & SR_1000T_LP_FD_CAPS) ? 1 : 0;
		break;

	case ETHER_STAT_LP_CAP_1000HDX:
		*val =
		    (Adapter->phy_1000t_status & SR_1000T_LP_HD_CAPS) ? 1 : 0;
		break;

	case ETHER_STAT_LP_CAP_100FDX:
		*val = (Adapter->phy_lp_able & NWAY_LPAR_100TX_FD_CAPS) ? 1 : 0;
		break;

	case ETHER_STAT_LP_CAP_100HDX:
		*val = (Adapter->phy_lp_able & NWAY_LPAR_100TX_HD_CAPS) ? 1 : 0;
		break;

	case ETHER_STAT_LP_CAP_10FDX:
		*val = (Adapter->phy_lp_able & NWAY_LPAR_10T_FD_CAPS) ? 1 : 0;
		break;

	case ETHER_STAT_LP_CAP_10HDX:
		*val = (Adapter->phy_lp_able & NWAY_LPAR_10T_HD_CAPS) ? 1 : 0;
		break;

	case ETHER_STAT_LP_CAP_ASMPAUSE:
		*val = (Adapter->phy_lp_able & NWAY_LPAR_ASM_DIR) ? 1 : 0;
		break;

	case ETHER_STAT_LP_CAP_PAUSE:
		*val = (Adapter->phy_lp_able & NWAY_LPAR_PAUSE) ? 1 : 0;
		break;

	case ETHER_STAT_LP_CAP_AUTONEG:
		*val = (Adapter->phy_an_exp & NWAY_ER_LP_NWAY_CAPS) ? 1 : 0;
		break;

	case ETHER_STAT_LINK_ASMPAUSE:
		*val = (Adapter->phy_an_adv & NWAY_AR_ASM_DIR) ? 1 : 0;
		break;

	case ETHER_STAT_LINK_PAUSE:
		*val = (Adapter->phy_an_adv & NWAY_AR_PAUSE) ? 1 : 0;
		break;

	case ETHER_STAT_LINK_AUTONEG:
		*val = (Adapter->phy_ctrl & MII_CR_AUTO_NEG_EN) ? 1 : 0;
		break;

	case ETHER_STAT_LINK_DUPLEX:
		*val = (Adapter->link_duplex == FULL_DUPLEX) ?
		    LINK_DUPLEX_FULL : LINK_DUPLEX_HALF;
		break;

	default:
		rw_exit(&Adapter->chip_lock);
		return (ENOTSUP);
	}

	rw_exit(&Adapter->chip_lock);

	if (e1000g_check_acc_handle(Adapter->osdep.reg_handle) != DDI_FM_OK)
		ddi_fm_service_impact(Adapter->dip, DDI_SERVICE_UNAFFECTED);

	return (0);
}

/*
 * e1000g_init_stats - initialize kstat data structures
 *
 * This routine will create and initialize the driver private
 * statistics counters.
 */
int
e1000g_init_stats(struct e1000g *Adapter)
{
	kstat_t *ksp;
	p_e1000g_stat_t e1000g_ksp;

	/*
	 * Create and init kstat
	 */
	ksp = kstat_create(WSNAME, ddi_get_instance(Adapter->dip),
	    "statistics", "net", KSTAT_TYPE_NAMED,
	    sizeof (e1000g_stat_t) / sizeof (kstat_named_t), 0);

	if (ksp == NULL) {
		Adapter->e1000g_ksp = NULL;
		return (DDI_SUCCESS);
	}

	Adapter->e1000g_ksp = ksp;	/* Fill in the Adapters ksp */

	e1000g_ksp = (p_e1000g_stat_t)ksp->ks_data;

	/*
	 * Initialize all the statistics
	 */
	kstat_named_init(&e1000g_ksp->link_speed, "link_speed",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->reset_count, "Reset Count",
	    KSTAT_DATA_ULONG);

	kstat_named_init(&e1000g_ksp->rx_error, "Rx Error",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->rx_esballoc_fail, "Rx Desballoc Failure",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->rx_allocb_fail, "Rx Allocb Failure",
	    KSTAT_DATA_ULONG);

	kstat_named_init(&e1000g_ksp->tx_no_desc, "Tx No Desc",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->tx_no_swpkt, "Tx No Buffer",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->tx_send_fail, "Tx Send Failure",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->tx_over_size, "Tx Pkt Over Size",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->tx_reschedule, "Tx Reschedule",
	    KSTAT_DATA_ULONG);

	kstat_named_init(&e1000g_ksp->Mpc, "Recv_Missed_Packets",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Symerrs, "Recv_Symbol_Errors",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Rlec, "Recv_Length_Errors",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Xonrxc, "XONs_Recvd",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Xontxc, "XONs_Xmitd",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Xoffrxc, "XOFFs_Recvd",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Xofftxc, "XOFFs_Xmitd",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Fcruc, "Recv_Unsupport_FC_Pkts",
	    KSTAT_DATA_ULONG);
#ifdef E1000G_DEBUG
	kstat_named_init(&e1000g_ksp->Prc64, "Pkts_Recvd_(  64b)",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Prc127, "Pkts_Recvd_(  65- 127b)",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Prc255, "Pkts_Recvd_( 127- 255b)",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Prc511, "Pkts_Recvd_( 256- 511b)",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Prc1023, "Pkts_Recvd_( 511-1023b)",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Prc1522, "Pkts_Recvd_(1024-1522b)",
	    KSTAT_DATA_ULONG);
#endif
	kstat_named_init(&e1000g_ksp->Gprc, "Good_Pkts_Recvd",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Gptc, "Good_Pkts_Xmitd",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Gorl, "Good_Octets_Recvd_Lo",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Gorh, "Good_Octets_Recvd_Hi",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Gotl, "Good_Octets_Xmitd_Lo",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Goth, "Good_Octets_Xmitd_Hi",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Ruc, "Recv_Undersize",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Rfc, "Recv_Frag",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Roc, "Recv_Oversize",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Rjc, "Recv_Jabber",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Torl, "Total_Octets_Recvd_Lo",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Torh, "Total_Octets_Recvd_Hi",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Totl, "Total_Octets_Xmitd_Lo",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Toth, "Total_Octets_Xmitd_Hi",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Tpr, "Total_Packets_Recvd",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Tpt, "Total_Packets_Xmitd",
	    KSTAT_DATA_ULONG);
#ifdef E1000G_DEBUG
	kstat_named_init(&e1000g_ksp->Ptc64, "Pkts_Xmitd_(  64b)",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Ptc127, "Pkts_Xmitd_(  65- 127b)",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Ptc255, "Pkts_Xmitd_( 128- 255b)",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Ptc511, "Pkts_Xmitd_( 255- 511b)",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Ptc1023, "Pkts_Xmitd_( 512-1023b)",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Ptc1522, "Pkts_Xmitd_(1024-1522b)",
	    KSTAT_DATA_ULONG);
#endif
	kstat_named_init(&e1000g_ksp->Tncrs, "Xmit_with_No_CRS",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Tsctc, "Xmit_TCP_Seg_Contexts",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->Tsctfc, "Xmit_TCP_Seg_Contexts_Fail",
	    KSTAT_DATA_ULONG);

#ifdef E1000G_DEBUG
	kstat_named_init(&e1000g_ksp->rx_none, "Rx No Data",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->rx_multi_desc, "Rx Span Multi Desc",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->rx_no_freepkt, "Rx Freelist Empty",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->rx_avail_freepkt, "Rx Freelist Avail",
	    KSTAT_DATA_ULONG);

	kstat_named_init(&e1000g_ksp->tx_under_size, "Tx Pkt Under Size",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->tx_exceed_frags, "Tx Exceed Max Frags",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->tx_empty_frags, "Tx Empty Frags",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->tx_recycle, "Tx Recycle",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->tx_recycle_intr, "Tx Recycle Intr",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->tx_recycle_retry, "Tx Recycle Retry",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->tx_recycle_none, "Tx Recycled None",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->tx_copy, "Tx Send Copy",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->tx_bind, "Tx Send Bind",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->tx_multi_copy, "Tx Copy Multi Frags",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->tx_multi_cookie, "Tx Bind Multi Cookies",
	    KSTAT_DATA_ULONG);
	kstat_named_init(&e1000g_ksp->tx_lack_desc, "Tx Desc Insufficient",
	    KSTAT_DATA_ULONG);
#endif

	/*
	 * Function to provide kernel stat update on demand
	 */
	ksp->ks_update = e1000g_update_stats;

	/*
	 * Pointer into provider's raw statistics
	 */
	ksp->ks_private = (void *)Adapter;

	/*
	 * Add kstat to systems kstat chain
	 */
	kstat_install(ksp);

	return (DDI_SUCCESS);
}