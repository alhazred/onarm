/*
 * This file is provided under a CDDLv1 license.  When using or
 * redistributing this file, you may do so under this license.
 * In redistributing this file this license must be included
 * and no other modification of this header file is permitted.
 *
 * CDDL LICENSE SUMMARY
 *
 * Copyright(c) 1999 - 2007 Intel Corporation. All rights reserved.
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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms of the CDDLv1.
 */

/*
 * IntelVersion: HSD_2343720b_DragonLake3 v2007-06-14_HSD_2343720b_DragonLake3
 */
#ifndef _E1000_80003ES2LAN_H_
#define	_E1000_80003ES2LAN_H_

#pragma ident	"@(#)e1000_80003es2lan.h	1.1	07/08/14 SMI"

#ifdef __cplusplus
extern "C" {
#endif

#define	E1000_KMRNCTRLSTA_OFFSET_FIFO_CTRL	0x00
#define	E1000_KMRNCTRLSTA_OFFSET_INB_CTRL	0x02
#define	E1000_KMRNCTRLSTA_OFFSET_HD_CTRL	0x10

#define	E1000_KMRNCTRLSTA_FIFO_CTRL_RX_BYPASS	0x0008
#define	E1000_KMRNCTRLSTA_FIFO_CTRL_TX_BYPASS	0x0800
#define	E1000_KMRNCTRLSTA_INB_CTRL_DIS_PADDING	0x0010

#define	E1000_KMRNCTRLSTA_HD_CTRL_10_100_DEFAULT 0x0004
#define	E1000_KMRNCTRLSTA_HD_CTRL_1000_DEFAULT	0x0000

/* Gigabit Carry Extend Padding */
#define	E1000_TCTL_EXT_GCEX_MASK		0x000FFC00

#define	DEFAULT_TCTL_EXT_GCEX_80003ES2LAN	0x00010000

#define	DEFAULT_TIPG_IPGT_1000_80003ES2LAN	0x8
#define	DEFAULT_TIPG_IPGT_10_100_80003ES2LAN	0x9

/* GG82563 PHY Specific Status Register (Page 0, Register 16 */
#define	GG82563_PSCR_POLARITY_REVERSAL_DISABLE	0x0002 /* 1=Reversal Disabled */
#define	GG82563_PSCR_CROSSOVER_MODE_MASK	0x0060
#define	GG82563_PSCR_CROSSOVER_MODE_MDI		0x0000 /* 00=Manual MDI */
#define	GG82563_PSCR_CROSSOVER_MODE_MDIX	0x0020 /* 01=Manual MDIX */
#define	GG82563_PSCR_CROSSOVER_MODE_AUTO	0x0060 /* 11=Auto crossover */

/* PHY Specific Control Register 2 (Page 0, Register 26) */
#define	GG82563_PSCR2_REVERSE_AUTO_NEG		0x2000
						/* 1=Reverse Auto-Negotiation */

/* MAC Specific Control Register (Page 2, Register 21) */
/* Tx clock speed for Link Down and 1000BASE-T for the following speeds */
#define	GG82563_MSCR_TX_CLK_MASK		0x0007
#define	GG82563_MSCR_TX_CLK_10MBPS_2_5		0x0004
#define	GG82563_MSCR_TX_CLK_100MBPS_25		0x0005
#define	GG82563_MSCR_TX_CLK_1000MBPS_2_5	0x0006
#define	GG82563_MSCR_TX_CLK_1000MBPS_25		0x0007

#define	GG82563_MSCR_ASSERT_CRS_ON_TX		0x0010	/* 1=Assert */

/* DSP Distance Register (Page 5, Register 26) */
/*
 * 0 = <50M
 * 1 = 50-80M
 * 2 = 80-100M
 * 3 = 110-140M
 * 4 = >140M
 */
#define	GG82563_DSPD_CABLE_LENGTH		0x0007

/* Kumeran Mode Control Register (Page 193, Register 16) */
#define	GG82563_KMCR_PASS_FALSE_CARRIER		0x0800

/* Max number of times Kumeran read/write should be validated */
#define	GG82563_MAX_KMRN_RETRY			0x5

/* Power Management Control Register (Page 193, Register 20) */
#define	GG82563_PMCR_ENABLE_ELECTRICAL_IDLE	0x0001
					/* 1=Enable SERDES Electrical Idle */

/* In-Band Control Register (Page 194, Register 18) */
#define	GG82563_ICR_DIS_PADDING			0x0010	/* Disable Padding */

#ifdef __cplusplus
}
#endif

#endif	/* _E1000_80003ES2LAN_H_ */
