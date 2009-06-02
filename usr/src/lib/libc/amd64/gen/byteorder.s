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

	.ident	"@(#)byteorder.s	1.3	07/05/21 SMI"

	.file	"byteorder.s"

#include "SYS.h"

	/*
	 * NOTE: htonl/ntohl are identical routines, as are htons/ntohs.
	 * As such, they could be implemented as a single routine, using
	 * multiple ALTENTRY/SET_SIZE definitions. We don't do this so
	 * that they will have unique addresses, allowing DTrace and
	 * other debuggers to tell them apart. 
	 */


/*
 *	unsigned long htonl( hl )
 *	unsigned long ntohl( hl )
 *	long hl;
 *	reverses the byte order of 'uint32_t hl'
 */
	ENTRY(htonl)
	movl	%edi, %eax	/* %eax = hl */
	bswap	%eax		/* reverses the byte order of %eax */
	ret			/* return (%eax) */
	SET_SIZE(htonl)

	ENTRY(ntohl)
	movl	%edi, %eax	/* %eax = hl */
	bswap	%eax		/* reverses the byte order of %eax */
	ret			/* return (%eax) */
	SET_SIZE(ntohl)

/*
 *	unsigned short htons( hs )
 *	short hs;
 *
 *	reverses the byte order in hs.
 */
	ENTRY(htons)
	movl	%edi, %eax	/* %eax = hs */
	bswap	%eax		/* reverses the byte order of %eax */
	shrl	$16, %eax	/* moves high 16-bit to low 16-bit */
	ret			/* return (%eax) */
	SET_SIZE(htons)

	ENTRY(ntohs)
	movl	%edi, %eax	/* %eax = hs */
	bswap	%eax		/* reverses the byte order of %eax */
	shrl	$16, %eax	/* moves high 16-bit to low 16-bit */
	ret			/* return (%eax) */
	SET_SIZE(ntohs)
