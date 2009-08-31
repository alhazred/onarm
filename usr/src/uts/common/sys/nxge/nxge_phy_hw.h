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

#ifndef	_SYS_NXGE_NXGE_PHY_HW_H
#define	_SYS_NXGE_NXGE_PHY_HW_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"


#ifdef	__cplusplus
extern "C" {
#endif

#include <nxge_defs.h>

#define	NXGE_MAX_PHY_PORTS		32
#define	NXGE_EXT_PHY_PORT_ST		8

#define	NXGE_PMA_PMD_DEV_ADDR		1
#define	NXGE_PCS_DEV_ADDR		3
#define	NXGE_DEV_ID_REG_1		2
#define	NXGE_DEV_ID_REG_2		3
#define	NXGE_PHY_ID_REG_1		2
#define	NXGE_PHY_ID_REG_2		3

#define	BCM8704_CHIP_ID			0x8704
#define	BCM8706_CHIP_ID			0x8706

/*
 * The BCM_PHY_ID_MASK is explained below:
 * The first nibble (bits 0 through 3) is changed with every revision
 * of the silicon. So these bits are masked out to support future revisions
 * of the same chip. The third nibble (bits 8 through 11) is changed for
 * different chips of the same family. So these bits are masked out to
 * support chips of the same family.
 */
#define	BCM_PHY_ID_MASK			0xfffff0f0
#define	BCM8704_DEV_ID			0x206033
#define	BCM5464R_PHY_ID			0x2060b1
#define	BCM8706_DEV_ID			0x206035
#define	PHY_BCM8704_FAMILY		(BCM8704_DEV_ID & BCM_PHY_ID_MASK)
#define	PHY_BCM5464R_FAMILY		(BCM5464R_PHY_ID & BCM_PHY_ID_MASK)

#define	CLAUSE_45_TYPE	1
#define	CLAUSE_22_TYPE	2

#define	BCM5464_NEPTUNE_PORT_ADDR_BASE		10
#define	BCM8704_NEPTUNE_PORT_ADDR_BASE		8
#define	BCM8704_N2_PORT_ADDR_BASE		16

/*
 * Phy address for the second NIU port on Goa NEM card can be either
 * 20 or 17
 */
#define	BCM8706_GOA_PORT_ADDR_BASE		16
#define	BCM8706_ALT_GOA_PORT1_ADDR		20
/*
 * Phy addresses for Maramba support. Support for P0 will eventually
 * be removed.
 */
#define	BCM5464_MARAMBA_P0_PORT_ADDR_BASE	10
#define	BCM5464_MARAMBA_P1_PORT_ADDR_BASE	26
#define	BCM8704_MARAMBA_PORT_ADDR_BASE		16

#define	BCM8704_PMA_PMD_DEV_ADDR		1
#define	BCM8704_PCS_DEV_ADDR			3
#define	BCM8704_USER_DEV3_ADDR			3
#define	BCM8704_PHYXS_ADDR			4
#define	BCM8704_USER_DEV4_ADDR			4

/* Definitions for BCM 5464R PHY chip */

#define	BCM5464R_PHY_ECR	16
#define	BCM5464R_PHY_ESR	17
#define	BCM5464R_RXERR_CNT	18
#define	BCM5464R_FALSECS_CNT	19
#define	BCM5464R_RX_NOTOK_CNT	20
#define	BCM5464R_ER_DATA	21
#define	BCM5464R_RES		22
#define	BCM5464R_ER_ACC		23
#define	BCM5464R_AUX_CTL	24
#define	BCM5464R_AUX_S		25
#define	BCM5464R_INTR_S		26
#define	BCM5464R_INTR_M		27
#define	BCM5464R_MISC		28
#define	BCM5464R_MISC1		29
#define	BCM5464R_TESTR1		30

#define	PHY_BCM_5464R_OUI	0x001018
#define	PHY_BCM_5464R_MODEL	0x0B

/*
 * MII Register 16:  PHY Extended Control Register
 */

typedef	union _mii_phy_ecr_t {
	uint16_t value;
	struct {
#ifdef _BIT_FIELDS_HTOL
		uint16_t mac_phy_if_mode	: 1;
		uint16_t dis_automdicross	: 1;
		uint16_t tx_dis			: 1;
		uint16_t intr_dis		: 1;
		uint16_t force_intr		: 1;
		uint16_t bypass_encdec		: 1;
		uint16_t bypass_scrdes		: 1;
		uint16_t bypass_mlt3		: 1;
		uint16_t bypass_rx_sym		: 1;
		uint16_t reset_scr		: 1;
		uint16_t en_led_traffic		: 1;
		uint16_t force_leds_on		: 1;
		uint16_t force_leds_off		: 1;
		uint16_t res			: 2;
		uint16_t gmii_fifo_elas		: 1;
#else
		uint16_t gmii_fifo_elas		: 1;
		uint16_t res			: 2;
		uint16_t force_leds_off		: 1;
		uint16_t force_leds_on		: 1;
		uint16_t en_led_traffic		: 1;
		uint16_t reset_scr		: 1;
		uint16_t bypass_rx_sym		: 1;
		uint16_t bypass_mlt3		: 1;
		uint16_t bypass_scrdes		: 1;
		uint16_t bypass_encdec		: 1;
		uint16_t force_intr		: 1;
		uint16_t intr_dis		: 1;
		uint16_t tx_dis			: 1;
		uint16_t dis_automdicross	: 1;
		uint16_t mac_phy_if_mode	: 1;
#endif
	} bits;
} mii_phy_ecr_t, *p_mii_phy_ecr_t;

/*
 * MII Register 17:  PHY Extended Status Register
 */
typedef	union _mii_phy_esr_t {
	uint16_t value;
	struct {
#ifdef _BIT_FIELDS_HTOL
		uint16_t anbpsfm		: 1;
		uint16_t wsdwngr		: 1;
		uint16_t mdi_crst		: 1;
		uint16_t intr_s			: 1;
		uint16_t rmt_rx_s		: 1;
		uint16_t loc_rx_s		: 1;
		uint16_t locked			: 1;
		uint16_t link_s			: 1;
		uint16_t crc_err		: 1;
		uint16_t cext_err		: 1;
		uint16_t bad_ssd		: 1;
		uint16_t bad_esd		: 1;
		uint16_t rx_err			: 1;
		uint16_t tx_err			: 1;
		uint16_t lock_err		: 1;
		uint16_t mlt3_cerr		: 1;
#else
		uint16_t mlt3_cerr		: 1;
		uint16_t lock_err		: 1;
		uint16_t tx_err			: 1;
		uint16_t rx_err			: 1;
		uint16_t bad_esd		: 1;
		uint16_t bad_ssd		: 1;
		uint16_t cext_err		: 1;
		uint16_t crc_err		: 1;
		uint16_t link_s			: 1;
		uint16_t locked			: 1;
		uint16_t loc_rx_s		: 1;
		uint16_t rmt_rx_s		: 1;
		uint16_t intr_s			: 1;
		uint16_t mdi_crst		: 1;
		uint16_t wsdwngr		: 1;
		uint16_t anbpsfm		: 1;
#endif
	} bits;
} mii_phy_esr_t, *p_mii_phy_esr_t;

/*
 * MII Register 18:  Receive Error Counter Register
 */
typedef	union _mii_rxerr_cnt_t {
	uint16_t value;
	struct {
		uint16_t rx_err_cnt		: 16;
	} bits;
} mii_rxerr_cnt_t, *p_mii_rxerr_cnt_t;

/*
 * MII Register 19:  False Carrier Sense Counter Register
 */
typedef	union _mii_falsecs_cnt_t {
	uint16_t value;
	struct {
#ifdef _BIT_FIELDS_HTOL
		uint16_t res			: 8;
		uint16_t false_cs_cnt		: 8;
#else
		uint16_t false_cs_cnt		: 8;
		uint16_t res			: 8;
#endif
	} bits;
} mii_falsecs_cnt_t, *p_mii_falsecs_cnt_t;

/*
 * MII Register 20:  Receiver NOT_OK Counter Register
 */
typedef	union _mii_rx_notok_cnt_t {
	uint16_t value;
	struct {
#ifdef _BIT_FIELDS_HTOL
		uint16_t l_rx_notok_cnt		: 8;
		uint16_t r_rx_notok_cnt		: 8;
#else
		uint16_t r_rx_notok_cnt		: 8;
		uint16_t l_rx_notok_cnt		: 8;
#endif
	} bits;
} mii_rx_notok_cnt_t, *p_mii_rx_notok_t;

/*
 * MII Register 21:  Expansion Register Data Register
 */
typedef	union _mii_er_data_t {
	uint16_t value;
	struct {
		uint16_t reg_data;
	} bits;
} mii_er_data_t, *p_mii_er_data_t;

/*
 * MII Register 23:  Expansion Register Access Register
 */
typedef	union _mii_er_acc_t {
	struct {
#ifdef _BIT_FIELDS_HTOL
		uint16_t res			: 4;
		uint16_t er_sel			: 4;
		uint16_t er_acc			: 8;
#else
		uint16_t er_acc			: 8;
		uint16_t er_sel			: 4;
		uint16_t res			: 4;
#endif
	} bits;
} mii_er_acc_t, *p_mii_er_acc_t;

#define	EXP_RXTX_PKT_CNT		0x0
#define	EXP_INTR_STAT			0x1
#define	MULTICOL_LED_SEL		0x4
#define	MULTICOL_LED_FLASH_RATE_CTL	0x5
#define	MULTICOL_LED_BLINK_CTL		0x6
#define	CABLE_DIAG_CTL			0x10
#define	CABLE_DIAG_RES			0x11
#define	CABLE_DIAG_LEN_CH_2_1		0x12
#define	CABLE_DIAG_LEN_CH_4_3		0x13

/*
 * MII Register 24:  Auxiliary Control Register
 */
typedef	union _mii_aux_ctl_t {
	uint16_t value;
	struct {
#ifdef _BIT_FIELDS_HTOL
		uint16_t ext_lb			: 1;
		uint16_t ext_pkt_len		: 1;
		uint16_t edge_rate_ctl_1000	: 2;
		uint16_t res			: 1;
		uint16_t write_1		: 1;
		uint16_t res1			: 2;
		uint16_t dis_partial_resp	: 1;
		uint16_t res2			: 1;
		uint16_t edge_rate_ctl_100	: 2;
		uint16_t diag_mode		: 1;
		uint16_t shadow_reg_sel		: 3;
#else
		uint16_t shadow_reg_sel		: 3;
		uint16_t diag_mode		: 1;
		uint16_t edge_rate_ctl_100	: 2;
		uint16_t res2			: 1;
		uint16_t dis_partial_resp	: 1;
		uint16_t res1			: 2;
		uint16_t write_1		: 1;
		uint16_t res			: 1;
		uint16_t edge_rate_ctl_1000	: 2;
		uint16_t ext_pkt_len		: 1;
		uint16_t ext_lb			: 1;
#endif
	} bits;
} mii_aux_ctl_t, *p_mii_aux_ctl_t;

#define	AUX_REG				0x0
#define	AUX_10BASET			0x1
#define	AUX_PWR_CTL			0x2
#define	AUX_MISC_TEST			0x4
#define	AUX_MISC_CTL			0x7

/*
 * MII Register 25:  Auxiliary Status Summary Register
 */
typedef	union _mii_aux_s_t {
	uint16_t value;
	struct {
#ifdef _BIT_FIELDS_HTOL
		uint16_t an_complete		: 1;
		uint16_t an_complete_ack	: 1;
		uint16_t an_ack_detect		: 1;
		uint16_t an_ability_detect	: 1;
		uint16_t an_np_wait		: 1;
		uint16_t an_hcd			: 3;
		uint16_t pd_fault		: 1;
		uint16_t rmt_fault		: 1;
		uint16_t an_page_rx		: 1;
		uint16_t lp_an_ability		: 1;
		uint16_t lp_np_ability		: 1;
		uint16_t link_s			: 1;
		uint16_t pause_res_rx_dir	: 1;
		uint16_t pause_res_tx_dir	: 1;
#else
		uint16_t pause_res_tx_dir	: 1;
		uint16_t pause_res_rx_dir	: 1;
		uint16_t link_s			: 1;
		uint16_t lp_np_ability		: 1;
		uint16_t lp_an_ability		: 1;
		uint16_t an_page_rx		: 1;
		uint16_t rmt_fault		: 1;
		uint16_t pd_fault		: 1;
		uint16_t an_hcd			: 3;
		uint16_t an_np_wait		: 1;
		uint16_t an_ability_detect	: 1;
		uint16_t an_ack_detect		: 1;
		uint16_t an_complete_ack	: 1;
		uint16_t an_complete		: 1;
#endif
	} bits;
} mii_aux_s_t, *p_mii_aux_s_t;

/*
 * MII Register 26, 27:  Interrupt Status and Mask Registers
 */
typedef	union _mii_intr_t {
	uint16_t value;
	struct {
#ifdef _BIT_FIELDS_HTOL
		uint16_t res			: 1;
		uint16_t illegal_pair_swap	: 1;
		uint16_t mdix_status_change	: 1;
		uint16_t exceed_hicnt_thres	: 1;
		uint16_t exceed_locnt_thres	: 1;
		uint16_t an_page_rx		: 1;
		uint16_t hcd_nolink		: 1;
		uint16_t no_hcd			: 1;
		uint16_t neg_unsupported_hcd	: 1;
		uint16_t scr_sync_err		: 1;
		uint16_t rmt_rx_status_change	: 1;
		uint16_t loc_rx_status_change	: 1;
		uint16_t duplex_mode_change	: 1;
		uint16_t link_speed_change	: 1;
		uint16_t link_status_change	: 1;
		uint16_t crc_err		: 1;
#else
		uint16_t crc_err		: 1;
		uint16_t link_status_change	: 1;
		uint16_t link_speed_change	: 1;
		uint16_t duplex_mode_change	: 1;
		uint16_t loc_rx_status_change	: 1;
		uint16_t rmt_rx_status_change	: 1;
		uint16_t scr_sync_err		: 1;
		uint16_t neg_unsupported_hcd	: 1;
		uint16_t no_hcd			: 1;
		uint16_t hcd_nolink		: 1;
		uint16_t an_page_rx		: 1;
		uint16_t exceed_locnt_thres	: 1;
		uint16_t exceed_hicnt_thres	: 1;
		uint16_t mdix_status_change	: 1;
		uint16_t illegal_pair_swap	: 1;
		uint16_t res			: 1;
#endif
	} bits;
} mii_intr_t, *p_mii_intr_t;

/*
 * MII Register 28:  Register 1C Access Register
 */
typedef	union _mii_misc_t {
	uint16_t value;
	struct {
#ifdef _BIT_FIELDS_HTOL
		uint16_t w_en			: 1;
		uint16_t shadow_reg_sel		: 5;
		uint16_t data			: 10;
#else
		uint16_t data			: 10;
		uint16_t shadow_reg_sel		: 5;
		uint16_t w_en			: 1;
#endif
	} bits;
} mii_misc_t, *p_mii_misc_t;

#define	LINK_LED_MODE			0x2
#define	CLK_ALIGN_CTL			0x3
#define	WIRE_SP_RETRY			0x4
#define	CLK125				0x5
#define	LED_STATUS			0x8
#define	LED_CONTROL			0x9
#define	AUTO_PWR_DOWN			0xA
#define	LED_SEL1			0xD
#define	LED_SEL2			0xE

/*
 * MII Register 29:  Master/Slave Seed / HCD Status Register
 */

typedef	union _mii_misc1_t {
	uint16_t value;
	struct {
#ifdef _BIT_FIELDS_HTOL
		uint16_t en_shadow_reg		: 1;
		uint16_t data			: 15;
#else
		uint16_t data			: 15;
		uint16_t en_shadow_reg		: 1;
#endif
	} bits;
} mii_misc1_t, *p_mii_misc1_t;

/*
 * MII Register 30:  Test Register 1
 */

typedef	union _mii_test1_t {
	uint16_t value;
	struct {
#ifdef _BIT_FIELDS_HTOL
		uint16_t crc_err_cnt_sel	: 1;
		uint16_t res			: 7;
		uint16_t manual_swap_mdi_st	: 1;
		uint16_t res1			: 7;
#else
		uint16_t res1			: 7;
		uint16_t manual_swap_mdi_st	: 1;
		uint16_t res			: 7;
		uint16_t crc_err_cnt_sel	: 1;
#endif
	} bits;
} mii_test1_t, *p_mii_test1_t;


/* Definitions of BCM8704 */

#define	BCM8704_PMD_CONTROL_REG			0
#define	BCM8704_PMD_STATUS_REG			0x1
#define	BCM8704_PMD_ID_0_REG			0x2
#define	BCM8704_PMD_ID_1_REG			0x3
#define	BCM8704_PMD_SPEED_ABIL_REG		0x4
#define	BCM8704_PMD_DEV_IN_PKG1_REG		0x5
#define	BCM8704_PMD_DEV_IN_PKG2_REG		0x6
#define	BCM8704_PMD_CONTROL2_REG		0x7
#define	BCM8704_PMD_STATUS2_REG			0x8
#define	BCM8704_PMD_TRANSMIT_DIS_REG		0x9
#define	BCM8704_PMD_RECEIVE_SIG_DETECT		0xa
#define	BCM8704_PMD_ORG_UNIQUE_ID_0_REG		0xe
#define	BCM8704_PMD_ORG_UNIQUE_ID_1_REG		0xf
#define	BCM8704_PCS_CONTROL_REG			0
#define	BCM8704_PCS_STATUS1_REG			0x1
#define	BCM8704_PCS_ID_0_REG			0x2
#define	BCM8704_PCS_ID_1_REG			0x3
#define	BCM8704_PCS_SPEED_ABILITY_REG		0x4
#define	BCM8704_PCS_DEV_IN_PKG1_REG		0x5
#define	BCM8704_PCS_DEV_IN_PKG2_REG		0x6
#define	BCM8704_PCS_CONTROL2_REG		0x7
#define	BCM8704_PCS_STATUS2_REG			0x8
#define	BCM8704_PCS_ORG_UNIQUE_ID_0_REG		0xe
#define	BCM8704_PCS_ORG_UNIQUE_ID_1_REG		0xf
#define	BCM8704_PCS_STATUS_REG			0x18
#define	BCM8704_10GBASE_R_PCS_STATUS_REG	0x20
#define	BCM8704_10GBASE_R_PCS_STATUS2_REG	0x21
#define	BCM8704_PHYXS_CONTROL_REG		0
#define	BCM8704_PHYXS_STATUS_REG		0x1
#define	BCM8704_PHY_ID_0_REG			0x2
#define	BCM8704_PHY_ID_1_REG			0x3
#define	BCM8704_PHYXS_SPEED_ABILITY_REG		0x4
#define	BCM8704_PHYXS_DEV_IN_PKG2_REG		0x5
#define	BCM8704_PHYXS_DEV_IN_PKG1_REG		0x6
#define	BCM8704_PHYXS_STATUS2_REG		0x8
#define	BCM8704_PHYXS_ORG_UNIQUE_ID_0_REG	0xe
#define	BCM8704_PHYXS_ORG_UNIQUE_ID_1_REG	0xf
#define	BCM8704_PHYXS_XGXS_LANE_STATUS_REG	0x18
#define	BCM8704_PHYXS_XGXS_TEST_CONTROL_REG	0x19
#define	BCM8704_USER_CONTROL_REG		0xC800
#define	BCM8704_USER_ANALOG_CLK_REG		0xC801
#define	BCM8704_USER_PMD_RX_CONTROL_REG		0xC802
#define	BCM8704_USER_PMD_TX_CONTROL_REG		0xC803
#define	BCM8704_USER_ANALOG_STATUS0_REG		0xC804
#define	BCM8704_CHIP_ID_REG			0xC807
#define	BCM8704_USER_OPTICS_DIGITAL_CTRL_REG	0xC808
#define	BCM8704_USER_RX2_CONTROL1_REG		0x80C6
#define	BCM8704_USER_RX1_CONTROL1_REG		0x80D6
#define	BCM8704_USER_RX0_CONTROL1_REG		0x80E6
#define	BCM8704_USER_TX_ALARM_STATUS_REG	0x9004

/* Rx Channel Control1 Register bits */
#define	BCM8704_RXPOL_FLIP			0x20

typedef	union _phyxs_control {
	uint16_t value;
	struct {
#ifdef _BIT_FIELDS_HTOL
		uint16_t reset			: 1;
		uint16_t loopback		: 1;
		uint16_t speed_sel2		: 1;
		uint16_t res2			: 1;
		uint16_t low_power		: 1;
		uint16_t res1			: 4;
		uint16_t speed_sel1		: 1;
		uint16_t speed_sel0		: 4;
		uint16_t res0			: 2;
#else
		uint16_t res0			: 2;
		uint16_t speed_sel0		: 4;
		uint16_t speed_sel1		: 1;
		uint16_t res1			: 4;
		uint16_t low_power		: 1;
		uint16_t res2			: 1;
		uint16_t speed_sel2		: 1;
		uint16_t loopback		: 1;
		uint16_t reset			: 1;
#endif
	} bits;
} phyxs_control_t, *p_phyxs_control_t, pcs_control_t, *p_pcs_control_t;


/* PMD/Optics Digital Control Register (Dev=3 Addr=0xc800) */

typedef	union _control {
	uint16_t value;
	struct {
#ifdef _BIT_FIELDS_HTOL
		uint16_t optxenb_lvl		: 1;
		uint16_t optxrst_lvl		: 1;
		uint16_t opbiasflt_lvl		: 1;
		uint16_t obtmpflt_lvl		: 1;
		uint16_t opprflt_lvl		: 1;
		uint16_t optxflt_lvl		: 1;
		uint16_t optrxlos_lvl		: 1;
		uint16_t oprxflt_lvl		: 1;
		uint16_t optxon_lvl		: 1;
		uint16_t res1			: 7;
#else
		uint16_t res1			: 7;
		uint16_t optxon_lvl		: 1;
		uint16_t oprxflt_lvl		: 1;
		uint16_t optrxlos_lvl		: 1;
		uint16_t optxflt_lvl		: 1;
		uint16_t opprflt_lvl		: 1;
		uint16_t obtmpflt_lvl		: 1;
		uint16_t opbiasflt_lvl		: 1;
		uint16_t optxrst_lvl		: 1;
		uint16_t optxenb_lvl		: 1;
#endif
	} bits;
} control_t, *p_control_t;

typedef	union _pmd_tx_control {
	uint16_t value;
	struct {
#ifdef _BIT_FIELDS_HTOL
		uint16_t res1			: 7;
		uint16_t xfp_clken		: 1;
		uint16_t tx_dac_txd		: 2;
		uint16_t tx_dac_txck		: 2;
		uint16_t tsd_lpwren		: 1;
		uint16_t tsck_lpwren		: 1;
		uint16_t cmu_lpwren		: 1;
		uint16_t sfiforst		: 1;
#else
		uint16_t sfiforst		: 1;
		uint16_t cmu_lpwren		: 1;
		uint16_t tsck_lpwren		: 1;
		uint16_t tsd_lpwren		: 1;
		uint16_t tx_dac_txck		: 2;
		uint16_t tx_dac_txd		: 2;
		uint16_t xfp_clken		: 1;
		uint16_t res1			: 7;
#endif
	} bits;
} pmd_tx_control_t, *p_pmd_tx_control_t;


/* PMD/Optics Digital Control Register (Dev=3 Addr=0xc808) */


/* PMD/Optics Digital Control Register (Dev=3 Addr=0xc808) */

typedef	union _optics_dcntr {
	uint16_t value;
	struct {
#ifdef _BIT_FIELDS_HTOL
		uint16_t fault_mode		: 1;
		uint16_t tx_pwrdown		: 1;
		uint16_t rx_pwrdown		: 1;
		uint16_t ext_flt_en		: 1;
		uint16_t opt_rst		: 1;
		uint16_t pcs_tx_inv_b		: 1;
		uint16_t pcs_rx_inv		: 1;
		uint16_t res3			: 2;
		uint16_t gpio_sel		: 2;
		uint16_t res2			: 1;
		uint16_t lpbk_err_dis		: 1;
		uint16_t res1			: 2;
		uint16_t txonoff_pwdwn_dis	: 1;
#else
		uint16_t txonoff_pwdwn_dis	: 1;
		uint16_t res1			: 2;
		uint16_t lpbk_err_dis		: 1;
		uint16_t res2			: 1;
		uint16_t gpio_sel		: 2;
		uint16_t res3			: 2;
		uint16_t pcs_rx_inv		: 1;
		uint16_t pcs_tx_inv_b		: 1;
		uint16_t opt_rst		: 1;
		uint16_t ext_flt_en		: 1;
		uint16_t rx_pwrdown		: 1;
		uint16_t tx_pwrdown		: 1;
		uint16_t fault_mode		: 1;
#endif
	} bits;
} optics_dcntr_t, *p_optics_dcntr_t;

/* PMD Receive Signal Detect Register (Dev = 1 Register Address = 0x000A) */

#define	PMD_RX_SIG_DET3			0x10
#define	PMD_RX_SIG_DET2			0x08
#define	PMD_RX_SIG_DET1			0x04
#define	PMD_RX_SIG_DET0			0x02
#define	GLOB_PMD_RX_SIG_OK		0x01

/* 10GBase-R PCS Status Register (Dev = 3, Register Address = 0x0020) */

#define	PCS_10GBASE_RX_LINK_STATUS	0x1000
#define	PCS_PRBS31_ABLE			0x0004
#define	PCS_10GBASE_R_HI_BER		0x0002
#define	PCS_10GBASE_R_PCS_BLK_LOCK	0x0001

/* XGXS Lane Status Register (Dev = 4, Register Address = 0x0018) */

#define	XGXS_LANE_ALIGN_STATUS		0x1000
#define	XGXS_PATTERN_TEST_ABILITY	0x0800
#define	XGXS_LANE3_SYNC			0x0008
#define	XGXS_LANE2_SYNC			0x0004
#define	XGXS_LANE1_SYNC			0x0002
#define	XGXS_LANE0_SYNC			0x0001

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_NXGE_NXGE_PHY_HW_H */
