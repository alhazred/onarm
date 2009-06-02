/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2008 NEC Corporation
 */

#ifndef	_SYS_NE_UART_IMPL_H
#define	_SYS_NE_UART_IMPL_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef	NEUART_EXUART
#define	UART_REGSPAC	0x10
#else	/* !NEUART_EXUART */
#define	UART_REGSPAC	0x400
#endif	/* NEUART_EXUART */

/*
 * Definitions of baud rate and divisor.
 */
#ifdef	NEUART_EXUART
/* These values for SCLK = 18.432MHz. */
#define	NEUART_B0	0x0
#define	NEUART_B50	0x0
#define	NEUART_B75	0x0
#define	NEUART_B110	0x0
#define	NEUART_B134	0x0
#define	NEUART_B150	0x0
#define	NEUART_B200	0x0
#define	NEUART_B300	0x0
#define	NEUART_B600	0x0
#define	NEUART_B1200	0x0
#define	NEUART_B1800	0x0
#define	NEUART_B2400	0x00
#define	NEUART_B4800	0x00
#define	NEUART_B9600	0x0078
#define	NEUART_B19200	0x003c
#define	NEUART_B38400	0x001e
#define	NEUART_B57600	0x0014
#define	NEUART_B76800	0x0
#define	NEUART_B115200	0x000a
#define	NEUART_B153600	0x0
#define	NEUART_B230400	0x0005
#define	NEUART_B307200	0x0
#define	NEUART_B460800	0x0

#else	/* !NEUART_EXUART */

/* These values for SCLK = 133MHz. */
#define	NEUART_B0	0x0
#define	NEUART_B50	0x0
#define	NEUART_B75	0x0
#define	NEUART_B110	0x0
#define	NEUART_B134	0x0
#define	NEUART_B150	0xd879
#define	NEUART_B200	0x0
#define	NEUART_B300	0x6c3c
#define	NEUART_B600	0x361e
#define	NEUART_B1200	0x1b0f
#define	NEUART_B1800	0x120a
#define	NEUART_B2400	0x0d88
#define	NEUART_B4800	0x06c4
#define	NEUART_B9600	0x0362
#define	NEUART_B19200	0x01b1
#define	NEUART_B38400	0x00d8
#define	NEUART_B57600	0x0090
#define	NEUART_B76800	0x0
#define	NEUART_B115200	0x0048
#define	NEUART_B153600	0x0
#define	NEUART_B230400	0x0
#define	NEUART_B307200	0x0
#define	NEUART_B460800	0x0

#endif	/* NEUART_EXUART */

#ifdef	NEUART_EXUART
#define	NEUART_REGSIZE	0x2
#else	/* !NEUART_EXUART */
#define	NEUART_REGSIZE	0x4
#endif	/* NEUART_EXUART */

/*
 * Definitions for INS8250 / 16550  chips
 */

/* defined as offsets from the data register */
#define	DAT	0x00 			/* receive/transmit data */
#define	ICR	NEUART_REGSIZE		/* interrupt control register */
#define	ISR	(ICR + NEUART_REGSIZE)	/* interrupt status register */
#define	LCR	(ISR + NEUART_REGSIZE)	/* line control register */
#define	MCR	(LCR + NEUART_REGSIZE)	/* modem control register */
#define	LSR	(MCR + NEUART_REGSIZE)	/* line status register */
#define	MSR	(LSR + NEUART_REGSIZE)	/* modem status register */
#define	SCR	(MSR + NEUART_REGSIZE)	/* scratch register */
#define	FDR	(SCR + NEUART_REGSIZE)	/* FIFO DMA request control register */
#define	TFIR	(TFIR + NEUART_REGSIZE)	/* transfer finish interrupt register */
#define	DLL	DAT			/* divisor latch (lsb) */
#define	DLH	ICR			/* divisor latch (msb) */
#define	FIFOR	ISR			/* FIFO register for 16550 */

/* Enhanced feature register for 16650 */
#define	EFR	ISR

/* Device Identification Register for 16C2552 */
#define	DREV	DAT
#define	DVID	ICR

/* Alternate Function Register for 16C2552 */
#define	AFR	ISR

/*
 * INTEL 8210-A/B & 16450/16550 Registers Structure.
 */

/* Line Control Register */
#define	WLS0		0x01	/* word length select bit 0 */
#define	WLS1		0x02	/* word length select bit 2 */
#define	STB		0x04	/* number of stop bits */
#define	PEN		0x08	/* parity enable */
#define	EPS		0x10	/* even parity select */
#define	SETBREAK 	0x40	/* break key */
#define	DLAB		0x80	/* divisor latch access bit */
#define	RXLEN   	0x03   	/* # of data bits per received/xmitted char */
#define	STOP1   	0x00
#define	STOP2   	0x04
#define	PAREN   	0x08
#define	PAREVN  	0x10
#define	PARMARK 	0x20
#define	SNDBRK  	0x40
#define	EFRACCESS	0xBF	/* magic value for 16650 EFR access */

#define	BITS5		0x00	/* 5 bits per char */
#define	BITS6		0x01	/* 6 bits per char */
#define	BITS7		0x02	/* 7 bits per char */
#define	BITS8		0x03	/* 8 bits per char */

/* Line Status Register */
#define	RCA		0x01	/* data ready */
#define	OVRRUN		0x02	/* overrun error */
#define	PARERR		0x04	/* parity error */
#define	FRMERR		0x08	/* framing error */
#define	BRKDET  	0x10	/* a break has arrived */
#define	XHRE		0x20	/* tx hold reg is now empty */
#define	XSRE		0x40	/* tx shift reg is now empty */
#define	RFBE		0x80	/* rx FIFO Buffer error */

/* Interrupt Id Regisger */
#define	MSTATUS		0x00	/* modem status changed */
#define	NOINTERRUPT	0x01	/* no interrupt pending */
#define	TxRDY		0x02	/* Transmitter Holding Register Empty */
#define	RxRDY		0x04	/* Receiver Data Available */
#define	FFTMOUT 	0x0c	/* FIFO timeout - 16550AF */
#define	RSTATUS 	0x06	/* Receiver Line Status */

/* Interrupt Enable Register */
#define	RIEN		0x01	/* Received Data Ready */
#define	TIEN		0x02	/* Tx Hold Register Empty */
#define	SIEN		0x04	/* Receiver Line Status */
#define	MIEN		0x08	/* Modem Status */

/* Modem Control Register */
#define	DTR		0x01	/* Data Terminal Ready */
#define	RTS		0x02	/* Request To Send */
#define	OUT1		0x04	/* Aux output - not used */
#define	OUT2		0x08	/* turns intr to 386 on/off */
#define	NEUART_LOOP	0x10	/* loopback for diagnostics */

/* Modem Status Register */
#define	DCTS		0x01	/* Delta Clear To Send */
#define	DDSR		0x02	/* Delta Data Set Ready */
#define	DRI		0x04	/* Trail Edge Ring Indicator */
#define	DDCD		0x08	/* Delta Data Carrier Detect */
#define	CTS		0x10	/* Clear To Send */
#define	DSR		0x20	/* Data Set Ready */
#define	RI		0x40	/* Ring Indicator */
#define	DCD		0x80	/* Data Carrier Detect */

#define	DELTAS(x)	((x)&(DCTS|DDSR|DRI|DDCD))
#define	STATES(x)	((x)&(CTS|DSR|RI|DCD))

/* flags for FCR (FIFO Control register) */
#define	FIFO_OFF	0x00	/* fifo disabled */
#define	FIFO_ON		0x01	/* fifo enabled */
#define	FIFORXFLSH	0x02	/* flush receiver FIFO */
#define	FIFOTXFLSH	0x04	/* flush transmitter FIFO */
#define	FIFODMA		0x08	/* DMA mode 1 */
#define	FIFOEXTRA1	0x10	/* Longer fifos on some 16650's */
#define	FIFOEXTRA2	0x20	/* Longer fifos on some 16650's and 16750 */
#define	FIFO_TRIG_1	0x00	/* 1 byte trigger level */
#define	FIFO_TRIG_4	0x40	/* 4 byte trigger level */
#define	FIFO_TRIG_8	0x80	/* 8 byte trigger level */
#define	FIFO_TRIG_14	0xC0	/* 14 byte trigger level */

/* Serial in/out requests */

#define	OVERRUN		040000
#define	FRERROR		020000
#define	PERROR		010000
#define	S_ERRORS	(PERROR|OVERRUN|FRERROR)

/* EFR - Enhanced feature register for 16650 */
#define	ENHENABLE	0x10

/* SCR - scratch register */
#define	SCRTEST		0x5a	/* arbritrary value for testing SCR register */

#define	NEUART_FIFORESET_DLABON		0x0
#define	NEUART_FIFORESET_DLABOFF	0x1

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_NE_UART_IMPL_H */
