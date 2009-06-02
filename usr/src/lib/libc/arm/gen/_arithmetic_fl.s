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
 * Copyright (c) 2006 NEC Corporation
 * All rights reserved.
 */

	.file	"_arithmetic_fl.s"

#if defined(__ARM_EABI__)
#include <sys/asm_linkage.h>
#include "synonyms.h"

	ANSI_PRAGMA_WEAK2(__aeabi_dadd,__adddf3,function)
	ANSI_PRAGMA_WEAK2(__aeabi_ddiv,__divdf3,function)
	ANSI_PRAGMA_WEAK2(__aeabi_dmul,__muldf3,function)
	ANSI_PRAGMA_WEAK2(__aeabi_dsub,__subdf3,function)
	ANSI_PRAGMA_WEAK2(__aeabi_fadd,__addsf3,function)
	ANSI_PRAGMA_WEAK2(__aeabi_fdiv,__divsf3,function)
	ANSI_PRAGMA_WEAK2(__aeabi_fmul,__mulsf3,function)
	ANSI_PRAGMA_WEAK2(__aeabi_fsub,__subsf3,function)
#endif

#include <SYS.h>

#if defined(__VFP_FP__)
	ENTRY(__addsf3)
	fmsr	s0, r0
	fmsr	s1, r1
	fadds	s0, s0, s1
	fmrs	r0, s0
	bx	lr
	SET_SIZE(__addsf3)
		
	ENTRY(__adddf3)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	faddd	d0, d0, d1
	fmrs	r0, s0
	fmrs	r1, s1
	bx	lr
	SET_SIZE(__adddf3)
	
	ENTRY(__subsf3)
	fmsr	s0, r0
	fmsr	s1, r1
	fsubs	s0, s0, s1
	fmrs	r0, s0
	bx	lr
	SET_SIZE(__subsf3)	

	ENTRY(__subdf3)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	fsubd	d0, d0, d1
	fmrs	r0, s0
	fmrs	r1, s1
	bx	lr
	SET_SIZE(__subdf3)	
		
	ENTRY(__mulsf3)
	fmsr	s0, r0
	fmsr	s1, r1
	fmuls	s0, s0, s1
	fmrs	r0, s0
	bx	lr
	SET_SIZE(__mulsf3)	

	ENTRY(__muldf3)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	fmuld	d0, d0, d1
	fmrs	r0, s0
	fmrs	r1, s1
	bx	lr
	SET_SIZE(__muldf3)	
		
	ENTRY(__divsf3)
	fmsr	s0, r0
	fmsr	s1, r1
	fdivs	s0, s0, s1
	fmrs	r0, s0
	bx	lr
	SET_SIZE(__divsf3)	

	ENTRY(__divdf3)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	fdivd	d0, d0, d1
	fmrs	r0, s0
	fmrs	r1, s1
	bx	lr
	SET_SIZE(__divdf3)
#else
	ENTRY(__addsf3)
	.word	0xee000a10		/* fmsr		s0, r0 */
	.word	0xee001a90		/* fmsr		s1, r1 */
	.word	0xee300a20		/* fadds	s0, s0, s1 */
	.word	0xee100a10		/* fmrs		r0, s0 */
	bx	lr
	SET_SIZE(__addsf3)	
		
	ENTRY(__adddf3)
	.word	0xee001a10		/* fmsr		s0, r1 */
	.word	0xee000a90		/* fmsr		s1, r0 */
	.word	0xee013a10		/* fmsr		s2, r3 */
	.word	0xee012a90		/* fmsr		s3, r2 */
	.word	0xee300b01		/* faddd	d0, d0, d1 */
	.word	0xee101a10		/* fmrs		r1, s0 */
	.word	0xee100a90		/* fmrs		r0, s1 */
	bx	lr
	SET_SIZE(__adddf3)
	
	ENTRY(__subsf3)
	.word	0xee000a10		/* fmsr		s0, r0 */
	.word	0xee001a90		/* fmsr		s1, r1 */
	.word	0xee300a60		/* fsubs	s0, s0, s1 */
	.word	0xee100a10		/* fmrs		r0, s0 */
	bx	lr
	SET_SIZE(__subsf3)	

	ENTRY(__subdf3)
	.word	0xee001a10		/* fmsr		s0, r1 */
	.word	0xee000a90		/* fmsr		s1, r0 */
	.word	0xee013a10		/* fmsr		s2, r3 */
	.word	0xee012a90		/* fmsr		s3, r2 */
	.word	0xee300b41		/* fsubd	d0, d0, d1 */
	.word	0xee101a10		/* fmrs		r1, s0 */
	.word	0xee100a90		/* fmrs		r0, s1 */	
	bx	lr
	SET_SIZE(__subdf3)	
		
	ENTRY(__mulsf3)
	.word	0xee000a10		/* fmsr		s0, r0 */
	.word	0xee001a90		/* fmsr		s1, r1 */
	.word	0xee200a20		/* fmuls	s0, s0, s1 */
	.word	0xee100a10		/* fmrs		r0, s0 */
	bx	lr
	SET_SIZE(__mulsf3)	

	ENTRY(__muldf3)
	.word	0xee001a10		/* fmsr		s0, r1 */
	.word	0xee000a90		/* fmsr		s1, r0 */
	.word	0xee013a10		/* fmsr		s2, r3 */
	.word	0xee012a90		/* fmsr		s3, r2 */
	.word	0xee200b01		/* fmuld	d0, d0, d1 */
	.word	0xee101a10		/* fmrs		r1, s0 */
	.word	0xee100a90		/* fmrs		r0, s1 */
	bx	lr
	SET_SIZE(__muldf3)	
		
	ENTRY(__divsf3)
	.word	0xee000a10		/* fmsr		s0, r0 */
	.word	0xee001a90		/* fmsr		s1, r1 */	
	.word	0xee800a20		/* fdivs	s0, s0, s1 */
	.word	0xee100a10		/* fmrs		r0, s0 */
	bx	lr
	SET_SIZE(__divsf3)	

	ENTRY(__divdf3)
	.word	0xee001a10		/* fmsr		s0, r1 */
	.word	0xee000a90		/* fmsr		s1, r0 */
	.word	0xee013a10		/* fmsr		s2, r3 */
	.word	0xee012a90		/* fmsr		s3, r2 */
	.word	0xee800b01		/* fdivd	d0, d0, d1 */
	.word	0xee101a10		/* fmrs		r1, s0 */
	.word	0xee100a90		/* fmrs		r0, s1 */
	bx	lr
	SET_SIZE(__divdf3)
#endif
