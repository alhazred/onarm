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
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved 	*/

/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2008 NEC Corporation
 */

#ifndef	_SYS_NE_UART_H
#define	_SYS_NE_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/tty.h>
#include <sys/ksynch.h>
#include <sys/dditypes.h>

/*
 * Ring buffer and async line management definitions.
 */
#define	RINGBITS	10		/* # of bits in ring ptrs */
#define	RINGSIZE	(1<<RINGBITS)   /* size of ring */
#define	RINGMASK	(RINGSIZE-1)
#define	RINGFRAC	8		/* fraction of ring to force flush */

#define	RING_INIT(ap)  ((ap)->async_rput = (ap)->async_rget = 0)
#define	RING_CNT(ap)   (((ap)->async_rput >= (ap)->async_rget) ? \
	((ap)->async_rput - (ap)->async_rget):\
	((0x10000 - (ap)->async_rget) + (ap)->async_rput))
#define	RING_FRAC(ap)  ((int)RING_CNT(ap) >= (int)(RINGSIZE/RINGFRAC))
#define	RING_POK(ap, n) ((int)RING_CNT(ap) < (int)(RINGSIZE-(n)))
#define	RING_PUT(ap, c) \
	((ap)->async_ring[(ap)->async_rput++ & RINGMASK] =  (uchar_t)(c))
#define	RING_UNPUT(ap) ((ap)->async_rput--)
#define	RING_GOK(ap, n) ((int)RING_CNT(ap) >= (int)(n))
#define	RING_GET(ap)   ((ap)->async_ring[(ap)->async_rget++ & RINGMASK])
#define	RING_EAT(ap, n) ((ap)->async_rget += (n))
#define	RING_MARK(ap, c, s) \
	((ap)->async_ring[(ap)->async_rput++ & RINGMASK] = \
	((uchar_t)(c)|(s)))
#define	RING_UNMARK(ap) \
	((ap)->async_ring[((ap)->async_rget) & RINGMASK] &= ~S_ERRORS)
#define	RING_ERR(ap, c) \
	((ap)->async_ring[((ap)->async_rget) & RINGMASK] & (c))

/*
 * ne_uart tracing macros. 
 * These are a bit similar to some macros in sys/vtrace.h .
 *
 * XXX - Needs review:  would it be better to use the macros in sys/vtrace.h ?
 */
#ifdef DEBUG
#define	DEBUGWARN0(fac, format) \
	if (debug & (fac)) \
		cmn_err(CE_WARN, format)
#define	DEBUGNOTE0(fac, format) \
	if (debug & (fac)) \
		cmn_err(CE_NOTE, format)
#define	DEBUGNOTE1(fac, format, arg1) \
	if (debug & (fac)) \
		cmn_err(CE_NOTE, format, arg1)
#define	DEBUGNOTE2(fac, format, arg1, arg2) \
	if (debug & (fac)) \
		cmn_err(CE_NOTE, format, arg1, arg2)
#define	DEBUGNOTE3(fac, format, arg1, arg2, arg3) \
	if (debug & (fac)) \
		cmn_err(CE_NOTE, format, arg1, arg2, arg3)
#define	DEBUGCONT0(fac, format) \
	if (debug & (fac)) \
		cmn_err(CE_CONT, format)
#define	DEBUGCONT1(fac, format, arg1) \
	if (debug & (fac)) \
		cmn_err(CE_CONT, format, arg1)
#define	DEBUGCONT2(fac, format, arg1, arg2) \
	if (debug & (fac)) \
		cmn_err(CE_CONT, format, arg1, arg2)
#define	DEBUGCONT3(fac, format, arg1, arg2, arg3) \
	if (debug & (fac)) \
		cmn_err(CE_CONT, format, arg1, arg2, arg3)
#define	DEBUGCONT4(fac, format, arg1, arg2, arg3, arg4) \
	if (debug & (fac)) \
		cmn_err(CE_CONT, format, arg1, arg2, arg3, arg4)
#define	DEBUGCONT10(fac, format, \
	arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10) \
	if (debug & (fac)) \
		cmn_err(CE_CONT, format, \
		arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10)
#else
#define	DEBUGWARN0(fac, format)
#define	DEBUGNOTE0(fac, format)
#define	DEBUGNOTE1(fac, format, arg1)
#define	DEBUGNOTE2(fac, format, arg1, arg2)
#define	DEBUGNOTE3(fac, format, arg1, arg2, arg3)
#define	DEBUGCONT0(fac, format)
#define	DEBUGCONT1(fac, format, arg1)
#define	DEBUGCONT2(fac, format, arg1, arg2)
#define	DEBUGCONT3(fac, format, arg1, arg2, arg3)
#define	DEBUGCONT4(fac, format, arg1, arg2, arg3, arg4)
#define	DEBUGCONT10(fac, format, \
	arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10)
#endif

/*
 * Hardware channel common data. One structure per port.
 * Each of the fields in this structure is required to be protected by a
 * mutex lock at the highest priority at which it can be altered.
 * The neuart_flags, and neuart_next fields can be altered by interrupt
 * handling code that must be protected by the mutex whose handle is
 * stored in neuart_excl_hi.  All others can be protected by the neuart_excl
 * mutex, which is lower priority and adaptive.
 */

struct neuart_com {
	int		neuart_flags;	/* random flags  */
					/* protected by neuart_excl_hi lock */
	uint_t		neuart_hwtype;	/* HW type: NEUART16550A, etc. */
	uint_t		neuart_use_fifo;	/* HW FIFO use it or not ?? */
	uint_t		neuart_fifo_buf; /* With FIFO = 16, otherwise = 1 */
	uint_t		neuart_flags2;	/* flags which don't change, no lock */
	uint8_t		*neuart_ioaddr;	/* i/o address of NEUART port */
	/* protocol private data -- neuart_asyncline */
	struct neuart_asyncline *neuart_priv;
	dev_info_t	*neuart_dip;	/* dev_info */
	int		neuart_unit;	/* which port */
	ddi_iblock_cookie_t neuart_iblock;
	kmutex_t	neuart_excl;	/* neuart adaptive mutex */
	kmutex_t	neuart_excl_hi;	/* neuart spinlock mutex */

	/*
	 * The neuart_soft_sr mutex should only be taken by the soft interrupt
	 * handler and the driver DDI_SUSPEND/DDI_RESUME code.  It
	 * shouldn't be taken by any code that may get called indirectly
	 * by the soft interrupt handler (e.g. as a result of a put or
	 * putnext call).
	 */
	kmutex_t	neuart_soft_sr;	/* soft int suspend/resume mutex */
	uchar_t		neuart_msr;	/* saved modem status */
	uchar_t		neuart_mcr;	/* soft carrier bits */
	uchar_t		neuart_lcr;	/* console lcr bits */
	uchar_t		neuart_bidx;	/* console baud rate index */
	tcflag_t	neuart_cflag;	/* console mode bits */
	struct cons_polledio	polledio;	/* polled I/O functions */
	ddi_acc_handle_t	neuart_iohandle;	/* Data access handle */
	tcflag_t	neuart_ocflag;	/* old console mode bits */
	uchar_t		neuart_com_port;	/* COM port number, or zero */
	uchar_t		neuart_fifor;	/* FIFOR register setting */
#ifdef DEBUG
	int		neuart_msint_cnt;/* number of times in async_msint */
#endif
};

/*
 * Asychronous protocol private data structure for NE_UART.
 * Each of the fields in the structure is required to be protected by
 * the lower priority lock except the fields that are set only at
 * base level but cleared (with out lock) at interrupt level.
 */

struct neuart_asyncline {
	int		async_flags;	/* random flags */
	kcondvar_t	async_flags_cv; /* condition variable for flags */
	/* condition variable for async_ops */
	kcondvar_t	async_ops_cv;
	dev_t		async_dev;	/* device major/minor numbers */
	mblk_t		*async_xmitblk; /* transmit: active msg block */
	struct neuart_com	*async_common; /* device common data */
	tty_common_t 	async_ttycommon; /* tty driver common data */
	/* id for pending write-side bufcall */
	bufcall_id_t	async_wbufcid;
	size_t		async_wbufcds; /* Buffer size requested in bufcall */
	timeout_id_t	async_polltid;	/* softint poll timeout id */
	timeout_id_t    async_dtrtid;   /* delaying DTR turn on */
	/* hold minimum untimed break time id */
	timeout_id_t    async_utbrktid;

	/*
	 * The following fields are protected by the neuart_excl_hi lock.
	 * Some, such as async_flowc, are set only at the base level and
	 * cleared (without the lock) only by the interrupt level.
	 */
	uchar_t		*async_optr;	/* output pointer */
	int		async_ocnt;	/* output count */
	ushort_t	async_rput;	/* producing pointer for input */
	ushort_t	async_rget;	/* consuming pointer for input */

	/*
	 * Each character stuffed into the ring has two bytes associated
	 * with it.  The first byte is used to indicate special conditions
	 * and the second byte is the actual data.  The ring buffer
	 * needs to be defined as ushort_t to accomodate this.
	 */
	ushort_t	async_ring[RINGSIZE];

	short		async_break;	/* break count */
	int		async_inflow_source; /* input flow control type */

	union {
		struct {
			uchar_t _hw;	/* overrun (hw) */
			uchar_t _sw;	/* overrun (sw) */
		} _a;
		ushort_t uover_overrun;
	} async_uover;
#define	async_overrun		async_uover._a.uover_overrun
#define	async_hw_overrun	async_uover._a._hw
#define	async_sw_overrun	async_uover._a._sw
	short		async_ext;	/* modem status change count */
	short		async_work;	/* work to do flag */
	timeout_id_t	async_timer;	/* close drain progress timer */

	mblk_t		*async_suspqf;	/* front of suspend queue */
	mblk_t		*async_suspqb;	/* back of suspend queue */
	int		async_ops;	/* active operations counter */
};

/* definitions for async_flags field */
#define	NEUARTAS_EXCL_OPEN 0x10000000 /* exclusive open */
#define	NEUARTAS_WOPEN	 0x00000001 /* waiting for open to complete */
#define	NEUARTAS_ISOPEN	 0x00000002 /* open is complete */
#define	NEUARTAS_OUT	 0x00000004 /* line being used for dialout */
#define	NEUARTAS_CARR_ON 0x00000008 /* carrier on last time we looked */
#define	NEUARTAS_STOPPED 0x00000010 /* output is stopped */
#define	NEUARTAS_DELAY	 0x00000020 /* waiting for delay to finish */
#define	NEUARTAS_BREAK	 0x00000040 /* waiting for break to finish */
#define	NEUARTAS_BUSY	 0x00000080 /* waiting for transmission to finish */
#define	NEUARTAS_DRAINING 0x00000100	/* waiting for output to drain */
#define	NEUARTAS_SERVICEIMM 0x00000200	/* queue soft interrupt as soon as */
#define	NEUARTAS_HW_IN_FLOW 0x00000400	/* input flow control in effect */
#define	NEUARTAS_HW_OUT_FLW 0x00000800	/* output flow control in effect */
#define	NEUARTAS_PROGRESS   0x00001000	/* made progress on output effort */
#define	NEUARTAS_CLOSING    0x00002000	/* processing close on stream */
#define	NEUARTAS_OUT_SUSPEND 0x00004000 /* waiting for TIOCSBRK to finish */
#define	NEUARTAS_HOLD_UTBRK 0x00008000	/* waiting for untimed break hold */
					/* the minimum time */
#define	NEUARTAS_DTR_DELAY  0x00010000	/* delaying DTR turn on */
#define	NEUARTAS_SW_IN_FLOW 0x00020000	/* sw input flow control in effect */
#define	NEUARTAS_SW_OUT_FLW 0x00040000	/* sw output flow control in effect */
#define	NEUARTAS_SW_IN_NEEDED 0x00080000 /* sw input flow control char is */
					 /* needed to be sent */
#define	NEUARTAS_OUT_FLW_RESUME 0x00100000 /* output need to be resumed */
					   /* because of transition of flow */
					   /* control from stop to start */
#define	NEUARTAS_DDI_SUSPENDED  0x00200000 /* suspended by DDI */
/* call bufcall when resumed by DDI */
#define	NEUARTAS_RESUME_BUFCALL 0x00400000

/* neuart_hwtype definitions */
#define	NEUART8250A	0x2		/* 8250A or 16450 */
#define	NEUART16550	0x3		/* broken FIFO which must not be used */
#define	NEUART16550A	0x4		/* usable FIFO */
#define	NEUART16650	0x5
#define	NEUART16C2552	0x6
#define	NEUART16750	0x10

/* definitions for neuart_flags field */
#define	NEUART_NEEDSOFT		0x00000001
#define	NEUART_DOINGSOFT	0x00000002
#define	NEUART_PPS		0x00000004
#define	NEUART_PPS_EDGE		0x00000008
#define	NEUART_DOINGSOFT_RETRY	0x00000010
#define	NEUART_RTS_DTR_OFF	0x00000020
#define	NEUART_IGNORE_CD	0x00000040
#define	NEUART_CONSOLE		0x00000080
#define	NEUART_DDI_SUSPENDED	0x00000100 /* suspended by DDI */

/* definitions for neuart_flags2 field */
#define	NEUART2_NO_LOOPBACK 0x00000001	/* Device doesn't support loopback */

/* definitions for async_inflow_source field in struct neuart_asyncline */
#define	IN_FLOW_NULL		0x00000000
#define	IN_FLOW_RINGBUFF	0x00000001
#define	IN_FLOW_STREAMS		0x00000002
#define	IN_FLOW_USER		0x00000004

/*
 * OUTLINE defines the high-order flag bit in the minor device number that
 * controls use of a tty line for dialin and dialout simultaneously.
 */
#ifdef _LP64
#define	OUTLINE		(1 << (NBITSMINOR32 - 1))
#else
#define	OUTLINE		(1 << (NBITSMINOR - 1))
#endif
#define	UNIT(x)		(getminor(x) & ~OUTLINE)

/*
 * NEUARTSETSOFT macro to pend a soft interrupt if one isn't already pending.
 */

extern kmutex_t	neuart_soft_lock;	/* ptr to lock for neuart_softpend */
extern int neuart_softpend;		/* secondary interrupt pending */

#define	NEUARTSETSOFT(neuart)	{				\
	mutex_enter(&neuart_soft_lock);				\
	neuart->neuart_flags |= NEUART_NEEDSOFT;		\
	if (!neuart_softpend) {					\
		neuart_softpend = 1;				\
		mutex_exit(&neuart_soft_lock);			\
		ddi_trigger_softintr(neuart_softintr_id);	\
	}							\
	else							\
		mutex_exit(&neuart_soft_lock);			\
}

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_NE_UART_H */
