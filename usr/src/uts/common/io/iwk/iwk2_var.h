/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007, Intel Corporation
 * All rights reserved.
 */

/*
 * Copyright (c) 2006
 * Copyright (c) 2007
 *	Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _IWK_VAR_H
#define	_IWK_VAR_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef __cplusplus
extern "C" {
#endif

#define	IWK_DMA_SYNC(area, flag) \
	(void) ddi_dma_sync((area).dma_hdl, (area).offset, \
	(area).alength, (flag))

typedef struct iwk_dma_area {
	ddi_acc_handle_t	acc_hdl; /* handle for memory */
	caddr_t			mem_va; /* CPU VA of memory */
	uint32_t		nslots; /* number of slots */
	uint32_t		size;   /* size per slot */
	size_t			alength; /* allocated size */
					/* >= product of above */
	ddi_dma_handle_t	dma_hdl; /* DMA handle */
	offset_t		offset;  /* relative to handle */
	ddi_dma_cookie_t	cookie; /* associated cookie */
	uint32_t		ncookies;
	uint32_t		token; /* arbitrary identifier */
} iwk_dma_t;

typedef struct iwk_tx_data {
	iwk_dma_t		dma_data;
	iwk_tx_desc_t		*desc;
	uint32_t		paddr_desc;
	iwk_cmd_t		*cmd;
	uint32_t		paddr_cmd;
} iwk_tx_data_t;

typedef struct iwk_tx_ring {
	iwk_dma_t		dma_desc;
	iwk_dma_t		dma_cmd;
	iwk_tx_data_t	*data;
	int			qid;
	int			count;
	int			window;
	int			queued;
	int			cur;
} iwk_tx_ring_t;

typedef struct iwk_rx_data {
	iwk_dma_t		dma_data;
} iwk_rx_data_t;

typedef struct iwk_rx_ring {
	iwk_dma_t		dma_desc;
	uint32_t 		*desc;
	iwk_rx_data_t	data[RX_QUEUE_SIZE];
	int			cur;
} iwk_rx_ring_t;

typedef struct iwk_softc {
	struct ieee80211com	sc_ic;
	dev_info_t		*sc_dip;
	int			(*sc_newstate)(struct ieee80211com *,
	    enum ieee80211_state, int);
	enum ieee80211_state	sc_ostate;
	kmutex_t		sc_glock;
	kmutex_t		sc_mt_lock;
	kmutex_t		sc_tx_lock;
	kcondvar_t		sc_mt_cv;
	kcondvar_t		sc_tx_cv;
	kcondvar_t		sc_cmd_cv;
	kcondvar_t		sc_fw_cv;

	kthread_t		*sc_mf_thread;
	uint32_t		sc_mf_thread_switch;

	uint32_t		sc_flags;
	uint32_t		sc_dmabuf_sz;
	uint16_t		sc_clsz;
	uint8_t			sc_rev;
	uint8_t			sc_resv;
	uint16_t		sc_assoc_id;
	uint16_t		sc_reserved0;

	/* shared area */
	iwk_dma_t		sc_dma_sh;
	iwk_shared_t		*sc_shared;
	/* keep warm area */
	iwk_dma_t		sc_dma_kw;
	/* tx scheduler base address */
	uint32_t		sc_scd_base_addr;

	iwk_tx_ring_t		sc_txq[IWK_NUM_QUEUES];
	iwk_rx_ring_t		sc_rxq;

	/* firmware dma */
	iwk_firmware_hdr_t	*sc_hdr;
	char			*sc_boot;
	iwk_dma_t		sc_dma_fw_text;
	iwk_dma_t		sc_dma_fw_init_text;
	iwk_dma_t		sc_dma_fw_data;
	iwk_dma_t		sc_dma_fw_data_bak;
	iwk_dma_t		sc_dma_fw_init_data;

	ddi_acc_handle_t	sc_cfg_handle;
	caddr_t			sc_cfg_base;
	ddi_acc_handle_t	sc_handle;
	caddr_t			sc_base;
	ddi_iblock_cookie_t	sc_iblk;

	iwk_rxon_cmd_t	sc_config;
	struct iwk_eep		sc_eep_map; /* eeprom map */
	uint32_t		sc_scd_base;

	struct iwk_alive_resp	sc_card_alive_run;
	struct iwk_init_alive_resp	sc_card_alive_init;

	uint32_t		sc_tx_timer;
	uint8_t			*sc_fw_bin;

	ddi_softintr_t		sc_rx_softint_id;
	uint32_t		sc_rx_softint_pending;
	uint32_t		sc_need_reschedule;

	/* kstats */
	uint32_t		sc_tx_nobuf;
	uint32_t		sc_rx_nobuf;
	uint32_t		sc_tx_err;
	uint32_t		sc_rx_err;
	uint32_t		sc_tx_retries;
} iwk_sc_t;

#define	IWK_F_ATTACHED		(1 << 0)
#define	IWK_F_CMD_DONE		(1 << 1)
#define	IWK_F_FW_INIT		(1 << 2)
#define	IWK_F_HW_ERR_RECOVER		(1 << 3)
#define	IWK_F_RATE_AUTO_CTL		(1 << 4)
#define	IWK_F_RUNNING		(1 << 5)
#define	IWK_F_SCANNING		(1 << 6)

#define	IWK_SUCCESS		0
#define	IWK_FAIL		1
#ifdef __cplusplus
}
#endif

#endif /* _IWK_VAR_H */
