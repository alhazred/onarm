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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SYS_NXGE_NXGE_TXDMA_H
#define	_SYS_NXGE_NXGE_TXDMA_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/nxge/nxge_txdma_hw.h>
#include <npi_txdma.h>
#include <sys/nxge/nxge_serialize.h>

#define	TXDMA_PORT_BITMAP(nxgep)		(nxgep->pt_config.tx_dma_map)

#define	TXDMA_RECLAIM_PENDING_DEFAULT		64
#define	TX_FULL_MARK				3

/*
 * Transmit load balancing definitions.
 */
#define	NXGE_TX_LB_TCPUDP			0	/* default policy */
#define	NXGE_TX_LB_HASH				1	/* from the hint data */
#define	NXGE_TX_LB_DEST_MAC			2	/* Dest. MAC */

/*
 * Descriptor ring empty:
 *		(1) head index is equal to tail index.
 *		(2) wrapped around bits are the same.
 * Descriptor ring full:
 *		(1) head index is equal to tail index.
 *		(2) wrapped around bits are different.
 *
 */
#define	TXDMA_RING_EMPTY(head, head_wrap, tail, tail_wrap)	\
	((head == tail && head_wrap == tail_wrap) ? B_TRUE : B_FALSE)

#define	TXDMA_RING_FULL(head, head_wrap, tail, tail_wrap)	\
	((head == tail && head_wrap != tail_wrap) ? B_TRUE : B_FALSE)

#define	TXDMA_DESC_NEXT_INDEX(index, entries, wrap_mask) \
			((index + entries) & wrap_mask)

#define	TXDMA_DRR_WEIGHT_DEFAULT	0x001f

typedef enum {
	NXGE_USE_SERIAL	= 0,
	NXGE_USE_START,
} nxge_tx_mode_t;

typedef struct _tx_msg_t {
	nxge_os_block_mv_t 	flags;		/* DMA, BCOPY, DVMA (?) */
	nxge_os_dma_common_t	buf_dma;	/* premapped buffer blocks */
	nxge_os_dma_handle_t	buf_dma_handle; /* premapped buffer handle */
	nxge_os_dma_handle_t 	dma_handle;	/* DMA handle for normal send */
	nxge_os_dma_handle_t 	dvma_handle;	/* Fast DVMA  handle */

	p_mblk_t 		tx_message;
	uint32_t 		tx_msg_size;
	size_t			bytes_used;
	int			head;
	int			tail;
} tx_msg_t, *p_tx_msg_t;

/*
 * TX  Statistics.
 */
typedef struct _nxge_tx_ring_stats_t {
	uint64_t	opackets;
	uint64_t	obytes;
	uint64_t	oerrors;

	uint32_t	tx_inits;
	uint32_t	tx_no_buf;

	uint32_t		mbox_err;
	uint32_t		pkt_size_err;
	uint32_t 		tx_ring_oflow;
	uint32_t 		pre_buf_par_err;
	uint32_t 		nack_pref;
	uint32_t 		nack_pkt_rd;
	uint32_t 		conf_part_err;
	uint32_t 		pkt_part_err;
	uint32_t		tx_starts;
	uint32_t		tx_nocanput;
	uint32_t		tx_msgdup_fail;
	uint32_t		tx_allocb_fail;
	uint32_t		tx_no_desc;
	uint32_t		tx_dma_bind_fail;
	uint32_t		tx_uflo;

	uint32_t		tx_hdr_pkts;
	uint32_t		tx_ddi_pkts;
	uint32_t		tx_dvma_pkts;

	uint32_t		tx_max_pend;
	uint32_t		tx_jumbo_pkts;

	txdma_ring_errlog_t	errlog;
} nxge_tx_ring_stats_t, *p_nxge_tx_ring_stats_t;

typedef struct _tx_ring_t {
	nxge_os_dma_common_t	tdc_desc;
	struct _nxge_t		*nxgep;
	p_tx_msg_t 		tx_msg_ring;
	uint32_t		tnblocks;
	tx_rng_cfig_t		tx_ring_cfig;
	tx_ring_hdl_t		tx_ring_hdl;
	tx_ring_kick_t		tx_ring_kick;
	tx_cs_t			tx_cs;
	tx_dma_ent_msk_t	tx_evmask;
	txdma_mbh_t		tx_mbox_mbh;
	txdma_mbl_t		tx_mbox_mbl;
	log_page_vld_t		page_valid;
	log_page_mask_t		page_mask_1;
	log_page_mask_t		page_mask_2;
	log_page_value_t	page_value_1;
	log_page_value_t	page_value_2;
	log_page_relo_t		page_reloc_1;
	log_page_relo_t		page_reloc_2;
	log_page_hdl_t		page_hdl;
	txc_dma_max_burst_t	max_burst;
	boolean_t		cfg_set;
	uint32_t		tx_ring_state;

	nxge_os_mutex_t		lock;
	uint16_t 		index;
	uint16_t		tdc;
	struct nxge_tdc_cfg	*tdc_p;
	uint_t 			tx_ring_size;
	uint32_t 		num_chunks;

	uint_t 			tx_wrap_mask;
	uint_t 			rd_index;
	uint_t 			wr_index;
	boolean_t		wr_index_wrap;
	uint_t 			head_index;
	boolean_t		head_wrap;
	tx_ring_hdl_t		ring_head;
	tx_ring_kick_t		ring_kick_tail;
	txdma_mailbox_t		tx_mbox;

	uint_t 			descs_pending;
	boolean_t 		queueing;

	nxge_os_mutex_t		sq_lock;
	nxge_serialize_t 	*serial;
	p_mblk_t 		head;
	p_mblk_t 		tail;

	uint16_t		ldg_group_id;
	p_nxge_tx_ring_stats_t tdc_stats;

	nxge_os_mutex_t 	dvma_lock;
	uint_t 			dvma_wr_index;
	uint_t 			dvma_rd_index;
	uint_t 			dvma_pending;
	uint_t 			dvma_available;
	uint_t 			dvma_wrap_mask;

	nxge_os_dma_handle_t 	*dvma_ring;

#if	defined(sun4v) && defined(NIU_LP_WORKAROUND)
	uint64_t		hv_tx_buf_base_ioaddr_pp;
	uint64_t		hv_tx_buf_ioaddr_size;
	uint64_t		hv_tx_cntl_base_ioaddr_pp;
	uint64_t		hv_tx_cntl_ioaddr_size;
	boolean_t		hv_set;
#endif
} tx_ring_t, *p_tx_ring_t;


/* Transmit Mailbox */
typedef struct _tx_mbox_t {
	nxge_os_mutex_t 	lock;
	uint16_t		index;
	struct _nxge_t		*nxgep;
	uint16_t		tdc;
	nxge_os_dma_common_t	tx_mbox;
	txdma_mbl_t		tx_mbox_l;
	txdma_mbh_t		tx_mbox_h;
} tx_mbox_t, *p_tx_mbox_t;

typedef struct _tx_rings_t {
	p_tx_ring_t 		*rings;
	boolean_t		txdesc_allocated;
	uint32_t		ndmas;
	nxge_os_dma_common_t	tdc_dma;
	nxge_os_dma_common_t	tdc_mbox;
} tx_rings_t, *p_tx_rings_t;


#if defined(_KERNEL) || (defined(COSIM) && !defined(IODIAG))

typedef struct _tx_buf_rings_t {
	struct _tx_buf_ring_t 	*txbuf_rings;
	boolean_t		txbuf_allocated;
} tx_buf_rings_t, *p_tx_buf_rings_t;

#endif

typedef struct _tx_mbox_areas_t {
	p_tx_mbox_t 		*txmbox_areas_p;
	boolean_t		txmbox_allocated;
} tx_mbox_areas_t, *p_tx_mbox_areas_t;

typedef struct _tx_param_t {
	nxge_logical_page_t tx_logical_pages[NXGE_MAX_LOGICAL_PAGES];
} tx_param_t, *p_tx_param_t;

typedef struct _tx_params {
	struct _tx_param_t 	*tx_param_p;
} tx_params_t, *p_tx_params_t;

/*
 * Global register definitions per chip and they are initialized
 * using the function zero control registers.
 * .
 */
typedef struct _txdma_globals {
	boolean_t		mode32;
} txdma_globals_t, *p_txdma_globals;


#if	defined(SOLARIS) && (defined(_KERNEL) || \
	(defined(COSIM) && !defined(IODIAG)))

/*
 * Transmit prototypes.
 */
nxge_status_t nxge_init_txdma_channels(p_nxge_t);
void nxge_uninit_txdma_channels(p_nxge_t);
void nxge_setup_dma_common(p_nxge_dma_common_t, p_nxge_dma_common_t,
		uint32_t, uint32_t);
nxge_status_t nxge_reset_txdma_channel(p_nxge_t, uint16_t,
	uint64_t);
nxge_status_t nxge_init_txdma_channel_event_mask(p_nxge_t,
	uint16_t, p_tx_dma_ent_msk_t);
nxge_status_t nxge_init_txdma_channel_cntl_stat(p_nxge_t,
	uint16_t, uint64_t);
nxge_status_t nxge_enable_txdma_channel(p_nxge_t, uint16_t,
	p_tx_ring_t, p_tx_mbox_t);

p_mblk_t nxge_tx_pkt_header_reserve(p_mblk_t, uint8_t *);
int nxge_tx_pkt_nmblocks(p_mblk_t, int *);
boolean_t nxge_txdma_reclaim(p_nxge_t, p_tx_ring_t, int);

void nxge_fill_tx_hdr(p_mblk_t, boolean_t, boolean_t,
	int, uint8_t, p_tx_pkt_hdr_all_t);

nxge_status_t nxge_txdma_hw_mode(p_nxge_t, boolean_t);
void nxge_hw_start_tx(p_nxge_t);
void nxge_txdma_stop(p_nxge_t);
void nxge_txdma_stop_start(p_nxge_t);
void nxge_fixup_txdma_rings(p_nxge_t);
void nxge_txdma_hw_kick(p_nxge_t);
void nxge_txdma_fix_channel(p_nxge_t, uint16_t);
void nxge_txdma_fixup_channel(p_nxge_t, p_tx_ring_t,
	uint16_t);
void nxge_txdma_hw_kick_channel(p_nxge_t, p_tx_ring_t,
	uint16_t);

void nxge_txdma_regs_dump(p_nxge_t, int);
void nxge_txdma_regs_dump_channels(p_nxge_t);

void nxge_check_tx_hang(p_nxge_t);
void nxge_fixup_hung_txdma_rings(p_nxge_t);
void nxge_txdma_fix_hung_channel(p_nxge_t, uint16_t);
void nxge_txdma_fixup_hung_channel(p_nxge_t, p_tx_ring_t,
	uint16_t);

void nxge_reclaim_rings(p_nxge_t);
int nxge_txdma_channel_hung(p_nxge_t,
	p_tx_ring_t tx_ring_p, uint16_t);
int nxge_txdma_hung(p_nxge_t);
int nxge_txdma_stop_inj_err(p_nxge_t, int);
void nxge_txdma_inject_err(p_nxge_t, uint32_t, uint8_t);

#endif

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_NXGE_NXGE_TXDMA_H */
