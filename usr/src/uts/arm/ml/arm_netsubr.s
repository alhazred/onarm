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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 *  Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.
 *  Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T
 *    All Rights Reserved
 */

/*
 * Copyright (c) 2006 NEC Corporation
 */

	.ident	"@(#)arm_netsubr.s"
	.file	"arm_netsubr.s"

#include <sys/feature_tests.h>
#include <sys/asm_linkage.h>

/*
 * Network related subroutines
 */

#ifdef	_LITTLE_ENDIAN
/*
 * uint32_t
 * htonl(uint32_t x)
 * ntohl(uint32_t x)
 *	Convert 32 bit quantities between network and host byte order.
 */
ENTRY(htonl)
ALTENTRY(ntohl)
	rev	r0, r0
	mov	pc, lr
	SET_SIZE(ntohl)
	SET_SIZE(htonl)

/*
 * uint16_t
 * htons(uint16_t x)
 * ntohs(uint16_t x)
 *	Convert 16 bit quantities between network and host byte order.
 */
ENTRY(htons)
ALTENTRY(ntohs)
	rev16	r0, r0
	mov	pc, lr
	SET_SIZE(ntohs)
	SET_SIZE(htons)

#endif	/* _LITTLE_ENDIAN */

/*
 *   Checksum routine for Internet Protocol Headers
 */

#if defined(__lint)

/* ARGSUSED */
unsigned int
ip_ocsum(
	ushort_t *address,	/* ptr to 1st message buffer */
	int halfword_count,	/* length of data */
	unsigned int sum)	/* partial checksum */
{ 
	int		i;
	unsigned int	psum = 0;	/* partial sum */

	for (i = 0; i < halfword_count; i++, address++) {
		psum += *address;
	}

	while ((psum >> 16) != 0) {
		psum = (psum & 0xffff) + (psum >> 16);
	}

	psum += sum;

	while ((psum >> 16) != 0) {
		psum = (psum & 0xffff) + (psum >> 16);
	}

	return (psum);
}

#else	/* __lint */

ENTRY(ip_ocsum)
	stmfd	sp!, {r4}
	add	r3, r0, r1, lsl #1	/* r3 = address + halfword_count */
	mov	r1, #0			/* r1 = psum = 0 */
10:
	cmp	r0, r3			/* if (address >= r3) */
	bhs	11f			/*   goto 11f */
	ldrh	r4, [r0], #2
	add	r1, r1, r4		/* psum += *address, address++ */
	b	10b			/* loop */
11:
	mov	r3, #0x10000
	mov	r0, r1			/* r0 = psum */
	sub	r3, r3, #1		/* r3 = 0xffff */
20:
	movs	r4, r0, lsr #16		/* if ((psum >> 16) == 0) */
	beq	21f			/*   goto 21f */
	and	r1, r0, r3		/* r1 = psum & 0xffff */
	add	r0, r1, r4		/* psum = r1 + (psum >> 16) */
	b	20b			/* loop */
21:
	add	r0, r0, r2		/* psum += sum */
30:
	movs	r4, r0, lsr #16		/* if ((psum >> 16) == 0) */
	beq	31f			/*   goto 31f */
	and	r1, r0, r3		/* r1 = psum & 0xffff */
	add	r0, r1, r4		/* psum = r1 + (psum >> 16) */
	b	30b			/* loop */
31:
	ldmfd	sp!, {r4}
	mov	pc, lr
	SET_SIZE(ip_ocsum)
#endif
