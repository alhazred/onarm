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

#ifndef _E1000G_SW_H
#define	_E1000G_SW_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * **********************************************************************
 * Module Name:								*
 *   e1000g_sw.h							*
 *									*
 * Abstract:								*
 *   This header file contains Software-related data structures		*
 *   definitions.							*
 *									*
 * **********************************************************************
 */

#include <sys/types.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/stropts.h>
#include <sys/stream.h>
#include <sys/strsun.h>
#include <sys/strlog.h>
#include <sys/kmem.h>
#include <sys/stat.h>
#include <sys/kstat.h>
#include <sys/modctl.h>
#include <sys/errno.h>
#include <sys/mac.h>
#include <sys/mac_ether.h>
#include <sys/vlan.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/disp.h>
#include <sys/pci.h>
#include <sys/sdt.h>
#include <sys/ethernet.h>
#include <sys/pattr.h>
#include <sys/strsubr.h>
#include <sys/netlb.h>
#include <inet/common.h>
#include <inet/ip.h>
#include <inet/mi.h>
#include <inet/nd.h>
#include <sys/ddifm.h>
#include <sys/fm/protocol.h>
#include <sys/fm/util.h>
#include <sys/fm/io/ddi.h>
#ifdef	__arm
#include <sys/archsystm.h>
#endif	/* __arm */
#include "e1000_api.h"


#define	JUMBO_FRAG_LENGTH		4096

#define	LAST_RAR_ENTRY			(E1000_RAR_ENTRIES - 1)
#define	MAX_NUM_UNICAST_ADDRESSES	E1000_RAR_ENTRIES
#define	MAX_NUM_MULTICAST_ADDRESSES	256

#define	MAX_TX_DESC_PER_PACKET		16

/*
 * constants used in setting flow control thresholds
 */
#define	E1000_PBA_MASK		0xffff
#define	E1000_PBA_SHIFT		10
#define	E1000_FC_HIGH_DIFF	0x1638 /* High: 5688 bytes below Rx FIFO size */
#define	E1000_FC_LOW_DIFF	0x1640 /* Low: 5696 bytes below Rx FIFO size */
#define	E1000_FC_PAUSE_TIME	0x0680 /* 858 usec */

#ifdef	__arm
#define	MAX_NUM_TX_DESCRIPTOR		1024
#define	MAX_NUM_RX_DESCRIPTOR		1024
#define	MAX_NUM_RX_FREELIST		1024
#define	MAX_NUM_TX_FREELIST		1024
#define	MAX_RX_LIMIT_ON_INTR		1024
#else	/* !__arm */
#define	MAX_NUM_TX_DESCRIPTOR		4096
#define	MAX_NUM_RX_DESCRIPTOR		4096
#define	MAX_NUM_RX_FREELIST		4096
#define	MAX_NUM_TX_FREELIST		4096
#define	MAX_RX_LIMIT_ON_INTR		4096
#endif	/* __arm */
#define	MAX_RX_INTR_DELAY		65535
#define	MAX_RX_INTR_ABS_DELAY		65535
#define	MAX_TX_INTR_DELAY		65535
#define	MAX_TX_INTR_ABS_DELAY		65535
#define	MAX_INTR_THROTTLING		65535
#define	MAX_RX_BCOPY_THRESHOLD		E1000_RX_BUFFER_SIZE_2K
#define	MAX_TX_BCOPY_THRESHOLD		E1000_TX_BUFFER_SIZE_2K
#define	MAX_TX_RECYCLE_THRESHOLD	MAX_NUM_TX_DESCRIPTOR
#define	MAX_TX_RECYCLE_NUM		MAX_NUM_TX_DESCRIPTOR

#ifdef	__arm
#define	MIN_NUM_TX_DESCRIPTOR		20
#define	MIN_NUM_RX_DESCRIPTOR		20
#define	MIN_NUM_RX_FREELIST		18
#define	MIN_NUM_TX_FREELIST		8
#define	MIN_RX_LIMIT_ON_INTR		4
#else	/* !__arm */
#define	MIN_NUM_TX_DESCRIPTOR		80
#define	MIN_NUM_RX_DESCRIPTOR		80
#define	MIN_NUM_RX_FREELIST		64
#define	MIN_NUM_TX_FREELIST		80
#define	MIN_RX_LIMIT_ON_INTR		16
#endif	/* __arm */
#define	MIN_RX_INTR_DELAY		0
#define	MIN_RX_INTR_ABS_DELAY		0
#define	MIN_TX_INTR_DELAY		0
#define	MIN_TX_INTR_ABS_DELAY		0
#define	MIN_INTR_THROTTLING		0
#define	MIN_RX_BCOPY_THRESHOLD		0
#define	MIN_TX_BCOPY_THRESHOLD		MINIMUM_ETHERNET_PACKET_SIZE
#define	MIN_TX_RECYCLE_THRESHOLD	0
#define	MIN_TX_RECYCLE_NUM		MAX_TX_DESC_PER_PACKET

#ifdef	__arm
#define	DEFAULT_NUM_RX_DESCRIPTOR	512
#define	DEFAULT_NUM_TX_DESCRIPTOR	512
#define	DEFAULT_NUM_RX_FREELIST		512
#define	DEFAULT_NUM_TX_FREELIST		16
#define	DEFAULT_RX_LIMIT_ON_INTR	64
#else	/* !__arm */
#define	DEFAULT_NUM_RX_DESCRIPTOR	2048
#define	DEFAULT_NUM_TX_DESCRIPTOR	2048
#define	DEFAULT_NUM_RX_FREELIST		4096
#define	DEFAULT_NUM_TX_FREELIST		2304
#define	DEFAULT_RX_LIMIT_ON_INTR	128
#endif	/* __arm */

#if	defined(__arm)
#define	MAX_INTR_PER_SEC		5000
#define	MIN_INTR_PER_SEC		1500
#define	DEFAULT_INTR_PACKET_LOW		5
#define	DEFAULT_INTR_PACKET_HIGH	64
#define	DEFAULT_TX_RECYCLE_THRESHOLD	512
#elif	defined(__sparc)
#define	MAX_INTR_PER_SEC		7100
#define	MIN_INTR_PER_SEC		3000
#define	DEFAULT_INTR_PACKET_LOW		5
#define	DEFAULT_INTR_PACKET_HIGH	128
#define	DEFAULT_TX_RECYCLE_THRESHOLD	512
#else
#define	MAX_INTR_PER_SEC		15000
#define	MIN_INTR_PER_SEC		4000
#define	DEFAULT_INTR_PACKET_LOW		10
#define	DEFAULT_INTR_PACKET_HIGH	48
#define	DEFAULT_TX_RECYCLE_THRESHOLD	DEFAULT_TX_NO_RESOURCE
#endif

#define	DEFAULT_RX_INTR_DELAY		0
#define	DEFAULT_RX_INTR_ABS_DELAY	64
#define	DEFAULT_TX_INTR_DELAY		64
#define	DEFAULT_TX_INTR_ABS_DELAY	64
#define	DEFAULT_INTR_THROTTLING_HIGH    1000000000/(MIN_INTR_PER_SEC*256)
#define	DEFAULT_INTR_THROTTLING_LOW	1000000000/(MAX_INTR_PER_SEC*256)
#define	DEFAULT_INTR_THROTTLING		DEFAULT_INTR_THROTTLING_LOW

#define	DEFAULT_RX_BCOPY_THRESHOLD	128
#define	DEFAULT_TX_BCOPY_THRESHOLD	512
#define	DEFAULT_TX_RECYCLE_NUM		64
#define	DEFAULT_TX_UPDATE_THRESHOLD	256
#define	DEFAULT_TX_NO_RESOURCE		6

#define	DEFAULT_TX_INTR_ENABLE		1
#define	DEFAULT_FLOW_CONTROL		3
#define	DEFAULT_MASTER_LATENCY_TIMER	0	/* BIOS should decide */
						/* which is normally 0x040 */
#define	DEFAULT_TBI_COMPAT_ENABLE	1	/* Enable SBP workaround */
#define	DEFAULT_MSI_ENABLE		1	/* MSI Enable */
#define	DEFAULT_TX_HCKSUM_ENABLE	1	/* Hardware checksum enable */

#define	TX_DRAIN_TIME		(200)	/* # milliseconds xmit drain */

/*
 * The size of the receive/transmite buffers
 */
#define	E1000_RX_BUFFER_SIZE_2K		(2048)
#define	E1000_RX_BUFFER_SIZE_4K		(4096)
#define	E1000_RX_BUFFER_SIZE_8K		(8192)
#define	E1000_RX_BUFFER_SIZE_16K	(16384)

#define	E1000_TX_BUFFER_SIZE_2K		(2048)
#define	E1000_TX_BUFFER_SIZE_4K		(4096)
#define	E1000_TX_BUFFER_SIZE_8K		(8192)
#define	E1000_TX_BUFFER_SIZE_16K	(16384)

#define	FORCE_BCOPY_EXCEED_FRAGS	0x1
#define	FORCE_BCOPY_UNDER_SIZE		0x2

#ifdef	__arm

/*
 * We should always use bcopy() to send data because passing cached memory
 * to ddi_dma_addr_bind_handle() makes system slower.
 */
#define	E1000G_TX_FORCE_BCOPY		1

#define	E1000G_BCOPY(from, to, size)	FAST_BCOPY(from, to, size)

#else	/* !__arm */

#define	E1000G_BCOPY(from, to, size)	bcopy(from, to, size)

#endif	/* __arm */

#define	E1000G_RX_SW_FREE		0x0
#define	E1000G_RX_SW_SENDUP		0x1
#define	E1000G_RX_SW_STOP		0x2
#define	E1000G_RX_SW_DETACH		0x3

/*
 * definitions for smartspeed workaround
 */
#define	  E1000_SMARTSPEED_MAX		30	/* 30 watchdog iterations */
						/* or 30 seconds */
#define	  E1000_SMARTSPEED_DOWNSHIFT	6	/* 6 watchdog iterations */
						/* or 6 seconds */

/*
 * Definitions for module_info.
 */
#define	 WSNAME			"e1000g"	/* module name */

/*
 * Defined for IP header alignment. We also need to preserve space for
 * VLAN tag (4 bytes)
 */
#define	E1000G_IPALIGNROOM		6
#define	E1000G_IPALIGNPRESERVEROOM	64

/*
 * bit flags for 'attach_progress' which is a member variable in struct e1000g
 */
#define	ATTACH_PROGRESS_PCI_CONFIG	0x0001	/* PCI config setup */
#define	ATTACH_PROGRESS_REGS_MAP	0x0002	/* Registers mapped */
#define	ATTACH_PROGRESS_SETUP		0x0004	/* Setup driver parameters */
#define	ATTACH_PROGRESS_ADD_INTR	0x0008	/* Interrupt added */
#define	ATTACH_PROGRESS_LOCKS		0x0010	/* Locks initialized */
#define	ATTACH_PROGRESS_SOFT_INTR	0x0020	/* Soft interrupt added */
#define	ATTACH_PROGRESS_KSTATS		0x0040	/* Kstats created */
#define	ATTACH_PROGRESS_ALLOC		0x0080	/* DMA resources allocated */
#define	ATTACH_PROGRESS_INIT		0x0100	/* Driver initialization */
#define	ATTACH_PROGRESS_NDD		0x0200	/* NDD initialized */
#define	ATTACH_PROGRESS_MAC		0x0400	/* MAC registered */
#define	ATTACH_PROGRESS_ENABLE_INTR	0x0800	/* DDI interrupts enabled */
#define	ATTACH_PROGRESS_FMINIT		0x1000	/* FMA initiated */

/*
 * Speed and Duplex Settings
 */
#define	GDIAG_10_HALF		1
#define	GDIAG_10_FULL		2
#define	GDIAG_100_HALF		3
#define	GDIAG_100_FULL		4
#define	GDIAG_1000_FULL		6
#define	GDIAG_ANY		7

/*
 * Coexist Workaround RP: 07/04/03
 * 82544 Workaround : Co-existence
 */
#define	MAX_TX_BUF_SIZE		(8 * 1024)

#define	ROUNDOFF		0x1000

/*
 * Defines for Jumbo Frame
 */
#define	FRAME_SIZE_UPTO_2K	2048
#define	FRAME_SIZE_UPTO_4K	4096
#define	FRAME_SIZE_UPTO_8K	8192
#define	FRAME_SIZE_UPTO_16K	16384
#define	FRAME_SIZE_UPTO_9K	9234

/* The sizes (in bytes) of a ethernet packet */
#define	MAXIMUM_ETHERNET_FRAME_SIZE	1518 /* With FCS */
#define	MINIMUM_ETHERNET_FRAME_SIZE	64   /* With FCS */
#define	ETHERNET_FCS_SIZE		4
#define	MAXIMUM_ETHERNET_PACKET_SIZE	\
	(MAXIMUM_ETHERNET_FRAME_SIZE - ETHERNET_FCS_SIZE)
#define	MINIMUM_ETHERNET_PACKET_SIZE	\
	(MINIMUM_ETHERNET_FRAME_SIZE - ETHERNET_FCS_SIZE)
#define	CRC_LENGTH			ETHERNET_FCS_SIZE

/* Defines for Tx stall check */
#define	E1000G_STALL_WATCHDOG_COUNT	8

#define	MAX_TX_LINK_DOWN_TIMEOUT	8

/* Defines for DVMA */
#ifdef __sparc
#define	E1000G_DEFAULT_DVMA_PAGE_NUM	2
#endif

/*
 * Loopback definitions
 */
#define	E1000G_LB_NONE			0
#define	E1000G_LB_EXTERNAL_1000		1
#define	E1000G_LB_EXTERNAL_100		2
#define	E1000G_LB_EXTERNAL_10		3
#define	E1000G_LB_INTERNAL_PHY		4

/*
 * Private dip list definitions
 */
#define	E1000G_PRIV_DEVI_ATTACH	0x0
#define	E1000G_PRIV_DEVI_DETACH	0x1

/*
 * QUEUE_INIT_LIST -- Macro which will init ialize a queue to NULL.
 */
#define	QUEUE_INIT_LIST(_LH)	\
	(_LH)->Flink = (_LH)->Blink = (PSINGLE_LIST_LINK)0

/*
 * IS_QUEUE_EMPTY -- Macro which checks to see if a queue is empty.
 */
#define	IS_QUEUE_EMPTY(_LH)	\
	((_LH)->Flink == (PSINGLE_LIST_LINK)0)

/*
 * QUEUE_GET_HEAD -- Macro which returns the head of the queue, but does
 * not remove the head from the queue.
 */
#define	QUEUE_GET_HEAD(_LH)	((PSINGLE_LIST_LINK)((_LH)->Flink))

/*
 * QUEUE_REMOVE_HEAD -- Macro which removes the head of the head of a queue.
 */
#define	QUEUE_REMOVE_HEAD(_LH)	\
{ \
	PSINGLE_LIST_LINK ListElem; \
	if (ListElem = (_LH)->Flink) \
	{ \
		if (!((_LH)->Flink = ListElem->Flink)) \
			(_LH)->Blink = (PSINGLE_LIST_LINK) 0; \
	} \
}

/*
 * QUEUE_POP_HEAD -- Macro which  will pop the head off of a queue (list),
 *	and return it (this differs from QUEUE_REMOVE_HEAD only in
 *	the 1st line).
 */
#define	QUEUE_POP_HEAD(_LH)	\
	(PSINGLE_LIST_LINK)(_LH)->Flink; \
	{ \
		PSINGLE_LIST_LINK ListElem; \
		ListElem = (_LH)->Flink; \
		if (ListElem) \
		{ \
			(_LH)->Flink = ListElem->Flink; \
			if (!(_LH)->Flink) \
				(_LH)->Blink = (PSINGLE_LIST_LINK)0; \
		} \
	}

/*
 * QUEUE_GET_TAIL -- Macro which returns the tail of the queue, but does not
 *	remove the tail from the queue.
 */
#define	QUEUE_GET_TAIL(_LH)	((PSINGLE_LIST_LINK)((_LH)->Blink))

/*
 * QUEUE_PUSH_TAIL -- Macro which puts an element at the tail (end) of the queue
 */
#define	QUEUE_PUSH_TAIL(_LH, _E)	\
	if ((_LH)->Blink) \
	{ \
		((PSINGLE_LIST_LINK)(_LH)->Blink)->Flink = \
			(PSINGLE_LIST_LINK)(_E); \
		(_LH)->Blink = (PSINGLE_LIST_LINK)(_E); \
	} else { \
		(_LH)->Flink = \
			(_LH)->Blink = (PSINGLE_LIST_LINK)(_E); \
	} \
	(_E)->Flink = (PSINGLE_LIST_LINK)0;

/*
 * QUEUE_PUSH_HEAD -- Macro which puts an element at the head of the queue.
 */
#define	QUEUE_PUSH_HEAD(_LH, _E)	\
	if (!((_E)->Flink = (_LH)->Flink)) \
	{ \
		(_LH)->Blink = (PSINGLE_LIST_LINK)(_E); \
	} \
	(_LH)->Flink = (PSINGLE_LIST_LINK)(_E);

/*
 * QUEUE_GET_NEXT -- Macro which returns the next element linked to the
 *	current element.
 */
#define	QUEUE_GET_NEXT(_LH, _E)		\
	(PSINGLE_LIST_LINK)((((_LH)->Blink) == (_E)) ? \
	(0) : ((_E)->Flink))

/*
 * QUEUE_APPEND -- Macro which appends a queue to the tail of another queue
 */
#define	QUEUE_APPEND(_LH1, _LH2)	\
	if ((_LH2)->Flink) { \
		if ((_LH1)->Flink) { \
			((PSINGLE_LIST_LINK)(_LH1)->Blink)->Flink = \
				((PSINGLE_LIST_LINK)(_LH2)->Flink); \
		} else { \
			(_LH1)->Flink = \
				((PSINGLE_LIST_LINK)(_LH2)->Flink); \
		} \
		(_LH1)->Blink = ((PSINGLE_LIST_LINK)(_LH2)->Blink); \
	}

/*
 * Property lookups
 */
#define	E1000G_PROP_EXISTS(d, n)	ddi_prop_exists(DDI_DEV_T_ANY, (d), \
						DDI_PROP_DONTPASS, (n))
#define	E1000G_PROP_GET_INT(d, n)	ddi_prop_get_int(DDI_DEV_T_ANY, (d), \
						DDI_PROP_DONTPASS, (n), -1)

/*
 * Shorthand for the NDD parameters
 */
#define	param_adv_autoneg	nd_params[PARAM_ADV_AUTONEG_CAP].ndp_val
#define	param_adv_pause		nd_params[PARAM_ADV_PAUSE_CAP].ndp_val
#define	param_adv_asym_pause	nd_params[PARAM_ADV_ASYM_PAUSE_CAP].ndp_val
#define	param_adv_1000fdx	nd_params[PARAM_ADV_1000FDX_CAP].ndp_val
#define	param_adv_1000hdx	nd_params[PARAM_ADV_1000HDX_CAP].ndp_val
#define	param_adv_100fdx	nd_params[PARAM_ADV_100FDX_CAP].ndp_val
#define	param_adv_100hdx	nd_params[PARAM_ADV_100HDX_CAP].ndp_val
#define	param_adv_10fdx		nd_params[PARAM_ADV_10FDX_CAP].ndp_val
#define	param_adv_10hdx		nd_params[PARAM_ADV_10HDX_CAP].ndp_val
#define	param_force_speed_duplex nd_params[PARAM_FORCE_SPEED_DUPLEX].ndp_val

#ifdef E1000G_DEBUG
/*
 * E1000G-specific ioctls ...
 */
#define	E1000G_IOC		((((((('E' << 4) + '1') << 4) \
				+ 'K') << 4) + 'G') << 4)

/*
 * These diagnostic IOCTLS are enabled only in DEBUG drivers
 */
#define	E1000G_IOC_REG_PEEK	(E1000G_IOC | 1)
#define	E1000G_IOC_REG_POKE	(E1000G_IOC | 2)
#define	E1000G_IOC_CHIP_RESET	(E1000G_IOC | 3)

#define	E1000G_PP_SPACE_REG	0	/* PCI memory space	*/
#define	E1000G_PP_SPACE_E1000G	1	/* driver's soft state	*/

typedef struct {
	uint64_t pp_acc_size;	/* It's 1, 2, 4 or 8	*/
	uint64_t pp_acc_space;	/* See #defines below	*/
	uint64_t pp_acc_offset;	/* See regs definition	*/
	uint64_t pp_acc_data;	/* output for peek	*/
				/* input for poke	*/
} e1000g_peekpoke_t;
#endif	/* E1000G_DEBUG */

/*
 * (Internal) return values from ioctl subroutines
 */
enum ioc_reply {
	IOC_INVAL = -1,		/* bad, NAK with EINVAL	*/
	IOC_DONE,		/* OK, reply sent	*/
	IOC_ACK,		/* OK, just send ACK	*/
	IOC_REPLY		/* OK, just send reply	*/
};

/*
 * Named Data (ND) Parameter Management Structure
 */
typedef struct {
	uint32_t ndp_info;
	uint32_t ndp_min;
	uint32_t ndp_max;
	uint32_t ndp_val;
	struct e1000g *ndp_instance;
	char *ndp_name;
} nd_param_t;

/*
 * NDD parameter indexes, divided into:
 *
 *	read-only parameters describing the hardware's capabilities
 *	read-write parameters controlling the advertised capabilities
 *	read-only parameters describing the partner's capabilities
 *	read-write parameters controlling the force speed and duplex
 *	read-only parameters describing the link state
 *	read-only parameters describing the driver properties
 *	read-write parameters controlling the driver properties
 */
enum {
	PARAM_AUTONEG_CAP,
	PARAM_PAUSE_CAP,
	PARAM_ASYM_PAUSE_CAP,
	PARAM_1000FDX_CAP,
	PARAM_1000HDX_CAP,
	PARAM_100T4_CAP,
	PARAM_100FDX_CAP,
	PARAM_100HDX_CAP,
	PARAM_10FDX_CAP,
	PARAM_10HDX_CAP,

	PARAM_ADV_AUTONEG_CAP,
	PARAM_ADV_PAUSE_CAP,
	PARAM_ADV_ASYM_PAUSE_CAP,
	PARAM_ADV_1000FDX_CAP,
	PARAM_ADV_1000HDX_CAP,
	PARAM_ADV_100T4_CAP,
	PARAM_ADV_100FDX_CAP,
	PARAM_ADV_100HDX_CAP,
	PARAM_ADV_10FDX_CAP,
	PARAM_ADV_10HDX_CAP,

	PARAM_LP_AUTONEG_CAP,
	PARAM_LP_PAUSE_CAP,
	PARAM_LP_ASYM_PAUSE_CAP,
	PARAM_LP_1000FDX_CAP,
	PARAM_LP_1000HDX_CAP,
	PARAM_LP_100T4_CAP,
	PARAM_LP_100FDX_CAP,
	PARAM_LP_100HDX_CAP,
	PARAM_LP_10FDX_CAP,
	PARAM_LP_10HDX_CAP,

	PARAM_FORCE_SPEED_DUPLEX,

	PARAM_LINK_STATUS,
	PARAM_LINK_SPEED,
	PARAM_LINK_DUPLEX,
	PARAM_LINK_AUTONEG,

	PARAM_MAX_FRAME_SIZE,
	PARAM_LOOP_MODE,
	PARAM_INTR_TYPE,

	PARAM_TX_BCOPY_THRESHOLD,
	PARAM_TX_INTR_ENABLE,
	PARAM_TX_TIDV,
	PARAM_TX_TADV,
	PARAM_RX_BCOPY_THRESHOLD,
	PARAM_RX_PKT_ON_INTR,
	PARAM_RX_RDTR,
	PARAM_RX_RADV,

	PARAM_COUNT
};

/*
 * The entry of the private dip list
 */
typedef struct _private_devi_list {
	dev_info_t *priv_dip;
	uint16_t flag;
	struct _private_devi_list *next;
} private_devi_list_t;

/*
 * A structure that points to the next entry in the queue.
 */
typedef struct _SINGLE_LIST_LINK {
	struct _SINGLE_LIST_LINK *Flink;
} SINGLE_LIST_LINK, *PSINGLE_LIST_LINK;

/*
 * A "ListHead" structure that points to the head and tail of a queue
 */
typedef struct _LIST_DESCRIBER {
	struct _SINGLE_LIST_LINK *volatile Flink;
	struct _SINGLE_LIST_LINK *volatile Blink;
} LIST_DESCRIBER, *PLIST_DESCRIBER;

/*
 * Address-Length pair structure that stores descriptor info
 */
typedef struct _sw_desc {
	uint64_t address;
	uint32_t length;
} sw_desc_t, *p_sw_desc_t;

typedef struct _desc_array {
	sw_desc_t descriptor[4];
	uint32_t elements;
} desc_array_t, *p_desc_array_t;

typedef enum {
	USE_NONE,
	USE_BCOPY,
	USE_DVMA,
	USE_DMA
} dma_type_t;

typedef enum {
	E1000G_STOP,
	E1000G_START,
	E1000G_ERROR
} chip_state_t;

typedef struct _dma_buffer {
	caddr_t address;
	uint64_t dma_address;
	ddi_acc_handle_t acc_handle;
	ddi_dma_handle_t dma_handle;
	size_t size;
	size_t len;
} dma_buffer_t, *p_dma_buffer_t;

/*
 * Transmit Control Block (TCB), Ndis equiv of SWPacket This
 * structure stores the additional information that is
 * associated with every packet to be transmitted. It stores the
 * message block pointer and the TBD addresses associated with
 * the m_blk and also the link to the next tcb in the chain
 */
typedef struct _tx_sw_packet {
	/* Link to the next tx_sw_packet in the list */
	SINGLE_LIST_LINK Link;
	mblk_t *mp;
	uint32_t num_desc;
	uint32_t num_mblk_frag;
	dma_type_t dma_type;
	dma_type_t data_transfer_type;
	ddi_dma_handle_t tx_dma_handle;
	dma_buffer_t tx_buf[1];
	sw_desc_t desc[MAX_TX_DESC_PER_PACKET];
} tx_sw_packet_t, *p_tx_sw_packet_t;

/*
 * This structure is similar to the rx_sw_packet structure used
 * for Ndis. This structure stores information about the 2k
 * aligned receive buffer into which the FX1000 DMA's frames.
 * This structure is maintained as a linked list of many
 * receiver buffer pointers.
 */
typedef struct _rx_sw_packet {
	/* Link to the next rx_sw_packet_t in the list */
	SINGLE_LIST_LINK Link;
	struct _rx_sw_packet *next;
	uint16_t flag;
	mblk_t *mp;
	caddr_t rx_ring;
	dma_type_t dma_type;
	frtn_t free_rtn;
	dma_buffer_t rx_buf[1];
} rx_sw_packet_t, *p_rx_sw_packet_t;

typedef struct _mblk_list {
	mblk_t *head;
	mblk_t *tail;
} mblk_list_t, *p_mblk_list_t;

typedef struct _cksum_data {
	uint32_t ether_header_size;
	uint32_t cksum_flags;
	uint32_t cksum_start;
	uint32_t cksum_stuff;
} cksum_data_t;

typedef union _e1000g_ether_addr {
	struct {
		uint32_t high;
		uint32_t low;
	} reg;
	struct {
		uint8_t set;
		uint8_t redundant;
		uint8_t addr[ETHERADDRL];
	} mac;
} e1000g_ether_addr_t;

typedef struct _e1000g_stat {

	kstat_named_t link_speed;	/* Link Speed */
	kstat_named_t reset_count;	/* Reset Count */

	kstat_named_t rx_error;		/* Rx Error in Packet */
	kstat_named_t rx_esballoc_fail;	/* Rx Desballoc Failure */
	kstat_named_t rx_allocb_fail;	/* Rx Allocb Failure */

	kstat_named_t tx_no_desc;	/* Tx No Desc */
	kstat_named_t tx_no_swpkt;	/* Tx No Pkt Buffer */
	kstat_named_t tx_send_fail;	/* Tx SendPkt Failure */
	kstat_named_t tx_over_size;	/* Tx Pkt Too Long */
	kstat_named_t tx_reschedule;	/* Tx Reschedule */

#ifdef E1000G_DEBUG
	kstat_named_t rx_none;		/* Rx No Incoming Data */
	kstat_named_t rx_multi_desc;	/* Rx Multi Spanned Pkt */
	kstat_named_t rx_no_freepkt;	/* Rx No Free Pkt */
	kstat_named_t rx_avail_freepkt;	/* Rx Freelist Avail Buffers */

	kstat_named_t tx_under_size;	/* Tx Packet Under Size */
	kstat_named_t tx_empty_frags;	/* Tx Empty Frags */
	kstat_named_t tx_exceed_frags;	/* Tx Exceed Max Frags */
	kstat_named_t tx_recycle;	/* Tx Recycle */
	kstat_named_t tx_recycle_intr;	/* Tx Recycle in Intr */
	kstat_named_t tx_recycle_retry;	/* Tx Recycle Retry */
	kstat_named_t tx_recycle_none;	/* Tx No Desc Recycled */
	kstat_named_t tx_copy;		/* Tx Send Copy */
	kstat_named_t tx_bind;		/* Tx Send Bind */
	kstat_named_t tx_multi_copy;	/* Tx Copy Multi Fragments */
	kstat_named_t tx_multi_cookie;	/* Tx Pkt Span Multi Cookies */
	kstat_named_t tx_lack_desc;	/* Tx Lack of Desc */
#endif

	kstat_named_t Crcerrs;	/* CRC Error Count */
	kstat_named_t Symerrs;	/* Symbol Error Count */
	kstat_named_t Mpc;	/* Missed Packet Count */
	kstat_named_t Scc;	/* Single Collision Count */
	kstat_named_t Ecol;	/* Excessive Collision Count */
	kstat_named_t Mcc;	/* Multiple Collision Count */
	kstat_named_t Latecol;	/* Late Collision Count */
	kstat_named_t Colc;	/* Collision Count */
	kstat_named_t Dc;	/* Defer Count */
	kstat_named_t Sec;	/* Sequence Error Count */
	kstat_named_t Rlec;	/* Receive Length Error Count */
	kstat_named_t Xonrxc;	/* XON Received Count */
	kstat_named_t Xontxc;	/* XON Xmitted Count */
	kstat_named_t Xoffrxc;	/* XOFF Received Count */
	kstat_named_t Xofftxc;	/* Xoff Xmitted Count */
	kstat_named_t Fcruc;	/* Unknown Flow Conrol Packet Rcvd Count */
#ifdef E1000G_DEBUG
	kstat_named_t Prc64;	/* Packets Received - 64b */
	kstat_named_t Prc127;	/* Packets Received - 65-127b */
	kstat_named_t Prc255;	/* Packets Received - 127-255b */
	kstat_named_t Prc511;	/* Packets Received - 256-511b */
	kstat_named_t Prc1023;	/* Packets Received - 511-1023b */
	kstat_named_t Prc1522;	/* Packets Received - 1024-1522b */
#endif
	kstat_named_t Gprc;	/* Good Packets Received Count */
	kstat_named_t Bprc;	/* Broadcasts Pkts Received Count */
	kstat_named_t Mprc;	/* Multicast Pkts Received Count */
	kstat_named_t Gptc;	/* Good Packets Xmitted Count */
	kstat_named_t Gorl;	/* Good Octets Recvd Lo Count */
	kstat_named_t Gorh;	/* Good Octets Recvd Hi Count */
	kstat_named_t Gotl;	/* Good Octets Xmitd Lo Count */
	kstat_named_t Goth;	/* Good Octets Xmitd Hi Count */
	kstat_named_t Rnbc;	/* Receive No Buffers Count */
	kstat_named_t Ruc;	/* Receive Undersize Count */
	kstat_named_t Rfc;	/* Receive Frag Count */
	kstat_named_t Roc;	/* Receive Oversize Count */
	kstat_named_t Rjc;	/* Receive Jabber Count */
	kstat_named_t Torl;	/* Total Octets Recvd Lo Count */
	kstat_named_t Torh;	/* Total Octets Recvd Hi Count */
	kstat_named_t Totl;	/* Total Octets Xmted Lo Count */
	kstat_named_t Toth;	/* Total Octets Xmted Hi Count */
	kstat_named_t Tpr;	/* Total Packets Received */
	kstat_named_t Tpt;	/* Total Packets Xmitted */
#ifdef E1000G_DEBUG
	kstat_named_t Ptc64;	/* Packets Xmitted (64b) */
	kstat_named_t Ptc127;	/* Packets Xmitted (64-127b) */
	kstat_named_t Ptc255;	/* Packets Xmitted (128-255b) */
	kstat_named_t Ptc511;	/* Packets Xmitted (255-511b) */
	kstat_named_t Ptc1023;	/* Packets Xmitted (512-1023b) */
	kstat_named_t Ptc1522;	/* Packets Xmitted (1024-1522b */
#endif
	kstat_named_t Mptc;	/* Multicast Packets Xmited Count */
	kstat_named_t Bptc;	/* Broadcast Packets Xmited Count */
	kstat_named_t Algnerrc;	/* Alignment Error count */
	kstat_named_t Tuc;	/* Transmit Underrun count */
	kstat_named_t Rxerrc;	/* Rx Error Count */
	kstat_named_t Tncrs;	/* Transmit with no CRS */
	kstat_named_t Cexterr;	/* Carrier Extension Error count */
	kstat_named_t Rutec;	/* Receive DMA too Early count */
	kstat_named_t Tsctc;	/* TCP seg contexts xmit count */
	kstat_named_t Tsctfc;	/* TCP seg contexts xmit fail count */
} e1000g_stat_t, *p_e1000g_stat_t;

typedef struct _e1000g_tx_ring {
	kmutex_t tx_lock;
	kmutex_t freelist_lock;
	kmutex_t usedlist_lock;
	/*
	 * Descriptor queue definitions
	 */
	ddi_dma_handle_t tbd_dma_handle;
	ddi_acc_handle_t tbd_acc_handle;
	struct e1000_tx_desc *tbd_area;
	uint64_t tbd_dma_addr;
	struct e1000_tx_desc *tbd_first;
	struct e1000_tx_desc *tbd_last;
	struct e1000_tx_desc *tbd_oldest;
	struct e1000_tx_desc *tbd_next;
	uint32_t tbd_avail;
	/*
	 * Software packet structures definitions
	 */
	p_tx_sw_packet_t packet_area;
	LIST_DESCRIBER used_list;
	LIST_DESCRIBER free_list;
	/*
	 * TCP/UDP checksum offload
	 */
	cksum_data_t cksum_data;
	/*
	 * Timer definitions for 82547
	 */
	timeout_id_t timer_id_82547;
	boolean_t timer_enable_82547;
	/*
	 * reschedule when tx resource is available
	 */
	boolean_t resched_needed;
	uint32_t frags_limit;
	uint32_t stall_watchdog;
	uint32_t recycle_fail;
	mblk_list_t mblks;
	/*
	 * Statistics
	 */
	uint32_t stat_no_swpkt;
	uint32_t stat_no_desc;
	uint32_t stat_send_fail;
	uint32_t stat_reschedule;
	uint32_t stat_over_size;
#ifdef E1000G_DEBUG
	uint32_t stat_under_size;
	uint32_t stat_exceed_frags;
	uint32_t stat_empty_frags;
	uint32_t stat_recycle;
	uint32_t stat_recycle_intr;
	uint32_t stat_recycle_retry;
	uint32_t stat_recycle_none;
	uint32_t stat_copy;
	uint32_t stat_bind;
	uint32_t stat_multi_copy;
	uint32_t stat_multi_cookie;
	uint32_t stat_lack_desc;
#endif
	/*
	 * Pointer to the adapter
	 */
	struct e1000g *adapter;
} e1000g_tx_ring_t, *pe1000g_tx_ring_t;

typedef struct _e1000g_rx_ring {
	kmutex_t rx_lock;
	kmutex_t freelist_lock;
	/*
	 * Descriptor queue definitions
	 */
	ddi_dma_handle_t rbd_dma_handle;
	ddi_acc_handle_t rbd_acc_handle;
	struct e1000_rx_desc *rbd_area;
	uint64_t rbd_dma_addr;
	struct e1000_rx_desc *rbd_first;
	struct e1000_rx_desc *rbd_last;
	struct e1000_rx_desc *rbd_next;
	/*
	 * Software packet structures definitions
	 */
	p_rx_sw_packet_t packet_area;
	LIST_DESCRIBER recv_list;
	LIST_DESCRIBER free_list;

	p_rx_sw_packet_t pending_list;
	uint32_t pending_count;
	uint32_t avail_freepkt;
	uint32_t rx_mblk_len;
	mblk_t *rx_mblk;
	mblk_t *rx_mblk_tail;
	/*
	 * Statistics
	 */
	uint32_t stat_error;
	uint32_t stat_esballoc_fail;
	uint32_t stat_allocb_fail;
	uint32_t stat_exceed_pkt;
#ifdef E1000G_DEBUG
	uint32_t stat_none;
	uint32_t stat_multi_desc;
	uint32_t stat_no_freepkt;
#endif
	/*
	 * Pointer to the adapter
	 */
	struct e1000g *adapter;
} e1000g_rx_ring_t, *pe1000g_rx_ring_t;

typedef struct e1000g {
	int instance;
	dev_info_t *dip;
	dev_info_t *priv_dip;
	mac_handle_t mh;
	mac_resource_handle_t mrh;
	struct e1000_hw shared;
	struct e1000g_osdep osdep;

	chip_state_t chip_state;
	boolean_t e1000g_promisc;
	boolean_t strip_crc;
	boolean_t rx_buffer_setup;
	link_state_t link_state;
	uint32_t link_speed;
	uint32_t link_duplex;
	uint32_t master_latency_timer;
	uint32_t smartspeed;	/* smartspeed w/a counter */
	uint32_t init_count;
	uint32_t reset_count;
	uint32_t attach_progress;	/* attach tracking */
	uint32_t loopback_mode;

	uint32_t tx_desc_num;
	uint32_t tx_freelist_num;
	uint32_t rx_desc_num;
	uint32_t rx_freelist_num;
	uint32_t tx_buffer_size;
	uint32_t rx_buffer_size;

	uint32_t tx_link_down_timeout;
	uint32_t tx_bcopy_thresh;
	uint32_t rx_limit_onintr;
	uint32_t rx_bcopy_thresh;
#ifndef NO_82542_SUPPORT
	uint32_t rx_buf_align;
#endif

	boolean_t intr_adaptive;
	boolean_t tx_intr_enable;
	uint32_t tx_recycle_thresh;
	uint32_t tx_recycle_num;
	uint32_t tx_intr_delay;
	uint32_t tx_intr_abs_delay;
	uint32_t rx_intr_delay;
	uint32_t rx_intr_abs_delay;
	uint32_t intr_throttling_rate;

	boolean_t watchdog_timer_enabled;
	boolean_t watchdog_timer_started;
	timeout_id_t watchdog_tid;
	boolean_t link_complete;
	timeout_id_t link_tid;

	e1000g_rx_ring_t rx_ring[1];
	e1000g_tx_ring_t tx_ring[1];

	/*
	 * Rx and Tx packet count for interrupt adaptive setting
	 */
	uint32_t rx_pkt_cnt;
	uint32_t tx_pkt_cnt;

	/*
	 * The watchdog_lock must be held when updateing the
	 * timeout fields in struct e1000g, that is,
	 * watchdog_tid, watchdog_timer_started.
	 */
	kmutex_t watchdog_lock;
	/*
	 * The link_lock protects the link fields in struct e1000g,
	 * such as link_state, link_speed, link_duplex, link_complete, and
	 * link_tid.
	 */
	kmutex_t link_lock;
	/*
	 * The chip_lock assures that the Rx/Tx process must be
	 * stopped while other functions change the hardware
	 * configuration of e1000g card, such as e1000g_reset(),
	 * e1000g_reset_hw() etc are executed.
	 */
	krwlock_t chip_lock;

	boolean_t unicst_init;
	uint32_t unicst_avail;
	uint32_t unicst_total;
	e1000g_ether_addr_t unicst_addr[MAX_NUM_UNICAST_ADDRESSES];

	uint32_t mcast_count;
	struct ether_addr mcast_table[MAX_NUM_MULTICAST_ADDRESSES];

#ifdef __sparc
	ulong_t sys_page_sz;
	uint_t dvma_page_num;
#endif

	boolean_t msi_enabled;
	boolean_t tx_hcksum_enabled;
	int intr_type;
	int intr_cnt;
	int intr_cap;
	size_t intr_size;
	uint_t intr_pri;
	ddi_intr_handle_t *htable;

	int tx_softint_pri;
	ddi_softint_handle_t tx_softint_handle;

	kstat_t *e1000g_ksp;

	/*
	 * NDD parameters
	 */
	caddr_t nd_data;
	nd_param_t nd_params[PARAM_COUNT];

	uint16_t phy_ctrl;		/* contents of PHY_CTRL */
	uint16_t phy_status;		/* contents of PHY_STATUS */
	uint16_t phy_an_adv;		/* contents of PHY_AUTONEG_ADV */
	uint16_t phy_an_exp;		/* contents of PHY_AUTONEG_EXP */
	uint16_t phy_ext_status;	/* contents of PHY_EXT_STATUS */
	uint16_t phy_1000t_ctrl;	/* contents of PHY_1000T_CTRL */
	uint16_t phy_1000t_status;	/* contents of PHY_1000T_STATUS */
	uint16_t phy_lp_able;		/* contents of PHY_LP_ABILITY */

	/*
	 * FMA capabilities
	 */
	int fm_capabilities;
} e1000g_t;


/*
 * Function prototypes
 */
int e1000g_alloc_dma_resources(struct e1000g *Adapter);
void e1000g_release_dma_resources(struct e1000g *Adapter);
void e1000g_free_rx_sw_packet(p_rx_sw_packet_t packet);
void e1000g_tx_setup(struct e1000g *Adapter);
void e1000g_rx_setup(struct e1000g *Adapter);
void e1000g_setup_multicast(struct e1000g *Adapter);
boolean_t e1000g_reset(struct e1000g *Adapter);

int e1000g_recycle(e1000g_tx_ring_t *tx_ring);
void e1000g_free_tx_swpkt(p_tx_sw_packet_t packet);
void e1000g_tx_freemsg(e1000g_tx_ring_t *tx_ring);
uint_t e1000g_tx_softint_worker(caddr_t arg1, caddr_t arg2);
mblk_t *e1000g_m_tx(void *arg, mblk_t *mp);
mblk_t *e1000g_receive(struct e1000g *Adapter);
void e1000g_rxfree_func(p_rx_sw_packet_t packet);

int e1000g_m_stat(void *arg, uint_t stat, uint64_t *val);
int e1000g_init_stats(struct e1000g *Adapter);
void e1000_tbi_adjust_stats(struct e1000g *Adapter,
    uint32_t frame_len, uint8_t *mac_addr);
enum ioc_reply e1000g_nd_ioctl(struct e1000g *Adapter,
    queue_t *wq, mblk_t *mp, struct iocblk *iocp);
void e1000g_nd_cleanup(struct e1000g *Adapter);
int e1000g_nd_init(struct e1000g *Adapter);

void e1000g_clear_interrupt(struct e1000g *Adapter);
void e1000g_mask_interrupt(struct e1000g *Adapter);
void e1000g_clear_all_interrupts(struct e1000g *Adapter);
void e1000g_clear_tx_interrupt(struct e1000g *Adapter);
void e1000g_mask_tx_interrupt(struct e1000g *Adapter);
void phy_spd_state(struct e1000_hw *hw, boolean_t enable);
void e1000_enable_pciex_master(struct e1000_hw *hw);
void e1000g_get_driver_control(struct e1000_hw *hw);
int e1000g_check_acc_handle(ddi_acc_handle_t handle);
int e1000g_check_dma_handle(ddi_dma_handle_t handle);
void e1000g_fm_ereport(struct e1000g *Adapter, char *detail);
void e1000g_set_fma_flags(struct e1000g *Adapter, int acc_flag, int dma_flag);

#pragma inline(e1000_rar_set)

/*
 * Global variables
 */
extern boolean_t e1000g_force_detach;
extern uint32_t e1000g_mblks_pending;
extern krwlock_t e1000g_rx_detach_lock;
extern private_devi_list_t *e1000g_private_devi_list;

#ifdef __cplusplus
}
#endif

#endif	/* _E1000G_SW_H */
