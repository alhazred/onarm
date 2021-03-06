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
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved	*/


/*	Copyright (c) 1989 by Sun Microsystems, Inc.		*/

.ident	"%Z%%M%	%I%	%E% SMI"
		/* SVr4.0 1.4.1.9	*/

/*
 * fp_except fpsetmask(mask)
 * 	fp_except mask;
 * set exception masks as defined by user and return
 * previous setting
 * any sticky bit set whose corresponding mask is dis-abled
 * is cleared
 */

	.file	"fpsetmask.s"

#include <sys/asm_linkage.h>

	ANSI_PRAGMA_WEAK(fpsetmask,function)

#include "synonyms.h"

	ENTRY(fpsetmask)
	add	%sp, -SA(MINFRAME), %sp	! get an additional word of storage
	set	0x0f800000, %o4		! mask of trap enable bits
	sll	%o0, 23, %o1		! move input bits into position
	st	%fsr, [%sp+STACK_BIAS+ARGPUSH]	! get fsr value
	ld	[%sp+STACK_BIAS+ARGPUSH], %o0	! load into register
	and	%o1, %o4, %o1		! generate new fsr value
	andn	%o0, %o4, %o2
	or	%o1, %o2, %o1
	st	%o1, [%sp+STACK_BIAS+ARGPUSH]	! move new fsr value to memory
	ld	[%sp+STACK_BIAS+ARGPUSH], %fsr	! load fsr with new value
	and	%o0, %o4, %o0		! mask off bits of interest in old fsr
	srl	%o0, 23, %o0		! return old trap enable value
	retl
	add	%sp, SA(MINFRAME), %sp	! reclaim stack space

	SET_SIZE(fpsetmask)
