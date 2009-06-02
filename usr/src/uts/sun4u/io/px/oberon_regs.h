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

#ifndef _SYS_OBERON_REGS_H
#define	_SYS_OBERON_REGS_H

#pragma ident	"@(#)oberon_regs.h	1.4	07/09/10 SMI"

#ifdef	__cplusplus
extern "C" {
#endif


#define	UBC_ERROR_LOG_ENABLE			0x471000
#define	UBC_ERROR_STATUS_CLEAR			0x471018
#define	UBC_INTERRUPT_ENABLE			0x471008
#define	UBC_INTERRUPT_STATUS			0x471010
#define	UBC_INTERRUPT_STATUS_DMARDUEA_P		0
#define	UBC_INTERRUPT_STATUS_DMAWTUEA_P		1
#define	UBC_INTERRUPT_STATUS_MEMRDAXA_P		2
#define	UBC_INTERRUPT_STATUS_MEMWTAXA_P		3
#define	UBC_INTERRUPT_STATUS_DMARDUEB_P		8
#define	UBC_INTERRUPT_STATUS_DMAWTUEB_P		9
#define	UBC_INTERRUPT_STATUS_MEMRDAXB_P		10
#define	UBC_INTERRUPT_STATUS_MEMWTAXB_P		11
#define	UBC_INTERRUPT_STATUS_PIOWTUE_P		16
#define	UBC_INTERRUPT_STATUS_PIOWBEUE_P		17
#define	UBC_INTERRUPT_STATUS_PIORBEUE_P		18
#define	UBC_INTERRUPT_STATUS_DMARDUEA_S		32
#define	UBC_INTERRUPT_STATUS_DMAWTUEA_S		33
#define	UBC_INTERRUPT_STATUS_MEMRDAXA_S		34
#define	UBC_INTERRUPT_STATUS_MEMWTAXA_S		35
#define	UBC_INTERRUPT_STATUS_DMARDUEB_S		40
#define	UBC_INTERRUPT_STATUS_DMAWTUEB_S		41
#define	UBC_INTERRUPT_STATUS_MEMRDAXB_S		42
#define	UBC_INTERRUPT_STATUS_MEMWTAXB_S		43
#define	UBC_INTERRUPT_STATUS_PIOWTUE_S		48
#define	UBC_INTERRUPT_STATUS_PIOWBEUE_S		49
#define	UBC_INTERRUPT_STATUS_PIORBEUE_S		50
#define	UBC_ERROR_STATUS_SET			0x471020
#define	UBC_PERFORMANCE_COUNTER_SELECT		0x472000
#define	UBC_PERFORMANCE_COUNTER_ZERO		0x472008
#define	UBC_PERFORMANCE_COUNTER_ONE		0x472010
#define	UBC_PERFORMANCE_COUNTER_SEL_MASKS	0x3f3f
#define	UBC_MEMORY_UE_LOG			0x471028
#define	UBC_MEMORY_UE_LOG_EID			60
#define	UBC_MEMORY_UE_LOG_EID_MASK		0x3
#define	UBC_MEMORY_UE_LOG_MARKED		48
#define	UBC_MEMORY_UE_LOG_MARKED_MASK		0x3fff
#define	UBC_MARKED_MAX_CPUID_MASK		0x1ff
/*
 * Class qualifiers on errors for which EID is valid.
 */
#define	UBC_EID_MEM	0
#define	UBC_EID_CHANNEL	1
#define	UBC_EID_CPU	2
#define	UBC_EID_PATH	3
/*
 * Mask within UBC_INTERRUPT_STATUS for Leaf-A errors
 */
#define	UBC_INTERRUPT_STATUS_LEAFA	\
	((1UL << UBC_INTERRUPT_STATUS_DMARDUEA_P) |\
	(1UL << UBC_INTERRUPT_STATUS_DMAWTUEA_P) |\
	(1UL << UBC_INTERRUPT_STATUS_MEMRDAXA_P) |\
	(1UL << UBC_INTERRUPT_STATUS_MEMWTAXA_P) |\
	(1UL << UBC_INTERRUPT_STATUS_DMARDUEA_S) |\
	(1UL << UBC_INTERRUPT_STATUS_DMAWTUEA_S) |\
	(1UL << UBC_INTERRUPT_STATUS_MEMRDAXA_S) |\
	(1UL << UBC_INTERRUPT_STATUS_MEMWTAXA_S))
/*
 * Mask within UBC_INTERRUPT_STATUS for Leaf-B errors
 */
#define	UBC_INTERRUPT_STATUS_LEAFB	\
	((1UL << UBC_INTERRUPT_STATUS_DMARDUEB_P) |\
	(1UL << UBC_INTERRUPT_STATUS_DMAWTUEB_P) |\
	(1UL << UBC_INTERRUPT_STATUS_MEMRDAXB_P) |\
	(1UL << UBC_INTERRUPT_STATUS_MEMWTAXB_P) |\
	(1UL << UBC_INTERRUPT_STATUS_DMARDUEB_S) |\
	(1UL << UBC_INTERRUPT_STATUS_DMAWTUEB_S) |\
	(1UL << UBC_INTERRUPT_STATUS_MEMRDAXB_S) |\
	(1UL << UBC_INTERRUPT_STATUS_MEMWTAXB_S))

#define	OBERON_UBC_ID_MAX		64
#define	OBERON_UBC_ID_IOC		0
#define	OBERON_UBC_ID_LSB		2

#define	OBERON_PORT_ID_LEAF		0
#define	OBERON_PORT_ID_LEAF_MASK	0x1
#define	OBERON_PORT_ID_IOC		1
#define	OBERON_PORT_ID_IOC_MASK		0x03
#define	OBERON_PORT_ID_LSB		4
#define	OBERON_PORT_ID_LSB_MASK		0x0F

/* values for OBERON_PORT_ID_LEAF field */
#define	OBERON_PORT_ID_LEAF_A		0
#define	OBERON_PORT_ID_LEAF_B		1

#define	INTERRUPT_MAPPING_ENTRIES_T_DESTID	21
#define	INTERRUPT_MAPPING_ENTRIES_T_DESTID_MASK	0x3ff

#define	OBERON_TLU_CONTROL_DRN_TR_DIS		35
#define	OBERON_TLU_CONTROL_CPLEP_DEN		34
#define	OBERON_TLU_CONTROL_ECRCCHK_DIS		33
#define	OBERON_TLU_CONTROL_ECRCGEN_DIS		32

#define	TLU_SLOT_CAPABILITIES_HP		6
#define	TLU_SLOT_CAPABILITIES_HPSUP		5
#define	TLU_SLOT_CAPABILITIES_PWINDP		4
#define	TLU_SLOT_CAPABILITIES_ATINDP		3
#define	TLU_SLOT_CAPABILITIES_MRLSP		2
#define	TLU_SLOT_CAPABILITIES_PWCNTLP		1
#define	TLU_SLOT_CAPABILITIES_ATBTNP		0

#define	DLU_INTERRUPT_MASK					0xe2048
#define	DLU_INTERRUPT_MASK_MSK_INTERRUPT_EN			31
#define	DLU_INTERRUPT_MASK_MSK_LINK_LAYER			5
#define	DLU_INTERRUPT_MASK_MSK_PHY_ERROR			4
#define	DLU_LINK_LAYER_CONFIG					0xe2200
#define	DLU_LINK_LAYER_CONFIG_VC0_EN				8
#define	DLU_LINK_LAYER_CONFIG_TLP_XMIT_FC_EN			3
#define	DLU_LINK_LAYER_CONFIG_FREQ_ACK_ENABLE			2
#define	DLU_LINK_LAYER_CONFIG_RETRY_DISABLE			1
#define	DLU_LINK_LAYER_STATUS					0xe2208
#define	DLU_LINK_LAYER_STATUS_LNK_STATE_MACH_STS_MASK		0x7
#define	DLU_LINK_LAYER_STATUS_LNK_STATE_MACH_STS_DL_INACTIVE	0x1
#define	DLU_LINK_LAYER_STATUS_LNK_STATE_MACH_STS_DL_INIT	0x2
#define	DLU_LINK_LAYER_STATUS_LNK_STATE_MACH_STS_DL_ACTIVE	0x4
#define	DLU_LINK_LAYER_STATUS_DLUP_STS				3
#define	DLU_LINK_LAYER_STATUS_INIT_FC_SM_STS			4
#define	DLU_LINK_LAYER_STATUS_INIT_FC_SM_STS_MASK		0x3
#define	DLU_LINK_LAYER_STATUS_INIT_FC_SM_STS_FC_IDLE		0x0
#define	DLU_LINK_LAYER_STATUS_INIT_FC_SM_STS_FC_INIT_1		0x1
#define	DLU_LINK_LAYER_STATUS_INIT_FC_SM_STS_FC_INIT_2		0x3
#define	DLU_LINK_LAYER_STATUS_INIT_FC_SM_STS_FC_INIT_DONE	0x2
#define	DLU_LINK_LAYER_INTERRUPT_AND_STATUS			0xe2210
#define	DLU_LINK_LAYER_INTERRUPT_AND_STATUS_INT_LINK_ERR_ACT	31
#define	DLU_LINK_LAYER_INTERRUPT_AND_STATUS_INT_PARABUS_PE	23
#define	DLU_LINK_LAYER_INTERRUPT_AND_STATUS_INT_UNSPRTD_DLLP	22
#define	DLU_LINK_LAYER_INTERRUPT_AND_STATUS_INT_SRC_ERR_TLP	17
#define	DLU_LINK_LAYER_INTERRUPT_MASK				0xe2220
#define	DLU_LINK_LAYER_INTERRUPT_MASK_MSK_LINK_ERR_ACT		31
#define	DLU_LINK_LAYER_INTERRUPT_MASK_MSK_PARABUS_PE		23
#define	DLU_LINK_LAYER_INTERRUPT_MASK_MSK_UNSPRTD_DLLP		22
#define	DLU_LINK_LAYER_INTERRUPT_MASK_MSK_SRC_ERR_TLP		17
#define	DLU_FLOW_CONTROL_UPDATE_CONTROL				0xe2240
#define	DLU_FLOW_CONTROL_UPDATE_CONTROL_FC0_U_C_EN		2
#define	DLU_FLOW_CONTROL_UPDATE_CONTROL_FC0_U_NP_EN		1
#define	DLU_FLOW_CONTROL_UPDATE_CONTROL_FC0_U_P_EN		0
#define	DLU_TXLINK_REPLAY_TIMER_THRESHOLD			0xe2410
#define	DLU_TXLINK_REPLAY_TIMER_THRESHOLD_RPLAY_TMR_THR		0
#define	DLU_TXLINK_REPLAY_TIMER_THRESHOLD_RPLAY_TMR_THR_MASK	0xfffff
#define	DLU_TXLINK_REPLAY_TIMER_THRESHOLD_DEFAULT		0xc9
#define	DLU_PORT_CONTROL					0xe2b00
#define	DLU_PORT_CONTROL_CK_EN					0
#define	DLU_PORT_STATUS						0xe2b08

#define	MMU_INTERRUPT_STATUS_TTC_DUE_P				8
#define	MMU_INTERRUPT_STATUS_TTC_DUE_S				40
#define	ILU_INTERRUPT_STATUS_IHB_UE_P				4
#define	ILU_INTERRUPT_STATUS_IHB_UE_S				36
#define	TLU_UNCORRECTABLE_ERROR_STATUS_CLEAR_ECRC_P		19
#define	TLU_UNCORRECTABLE_ERROR_STATUS_CLEAR_ECRC_S		51
#define	TLU_UNCORRECTABLE_ERROR_STATUS_CLEAR_POIS_P		12
#define	TLU_UNCORRECTABLE_ERROR_STATUS_CLEAR_POIS_S		44
#define	TLU_OTHER_EVENT_STATUS_CLEAR_EIUE_P			0
#define	TLU_OTHER_EVENT_STATUS_CLEAR_EIUE_S			32
#define	TLU_OTHER_EVENT_STATUS_CLEAR_ERBUE_P			1
#define	TLU_OTHER_EVENT_STATUS_CLEAR_ERBUE_S			33
#define	TLU_OTHER_EVENT_STATUS_CLEAR_TLUEITMO_P			7
#define	TLU_OTHER_EVENT_STATUS_CLEAR_TLUEITMO_S			39
#define	TLU_OTHER_EVENT_STATUS_CLEAR_EHBUE_P			12
#define	TLU_OTHER_EVENT_STATUS_CLEAR_EHBUE_S			44
#define	TLU_OTHER_EVENT_STATUS_CLEAR_EDBUE_P			12
#define	TLU_OTHER_EVENT_STATUS_CLEAR_EDBUE_S			44

#define	TLU_CONTROL_DRN_TR_DIS					35

#define	TLU_SLOT_CONTROL					0x90038
#define	TLU_SLOT_CONTROL_PWFDEN					1
#define	TLU_SLOT_STATUS						0x90040
#define	TLU_SLOT_STATUS_PSD					6
#define	TLU_SLOT_STATUS_MRLS					5
#define	TLU_SLOT_STATUS_CMDCPLT					4
#define	TLU_SLOT_STATUS_PSDC					3
#define	TLU_SLOT_STATUS_MRLC					2
#define	TLU_SLOT_STATUS_PWFD					1
#define	TLU_SLOT_STATUS_ABTN					0

#define	FLP_PORT_LINK_CONTROL					0xe5008
#define	FLP_PORT_LINK_CONTROL_RETRAIN				5

#define	FLP_PORT_CONTROL					0xe5200
#define	FLP_PORT_CONTROL_PORT_DIS				0

#define	FLP_PORT_ACTIVE_STATUS					0xe5240
#define	FLP_PORT_ACTIVE_STATUS_TRAIN_ERROR			1

#define	HOTPLUG_CONTROL						0x88000
#define	HOTPLUG_CONTROL_SLOTPON					3
#define	HOTPLUG_CONTROL_PWREN					2
#define	HOTPLUG_CONTROL_CLKEN					1
#define	HOTPLUG_CONTROL_N_PERST					0

#define	DRAIN_CONTROL_STATUS					0x51100
#define	DRAIN_CONTROL_STATUS_DRAIN				0

#define	PX_PCIEHP_PIL (LOCK_LEVEL - 1)
#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_OBERON_REGS_H */