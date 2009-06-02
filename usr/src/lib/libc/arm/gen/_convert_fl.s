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
 * Copyright (c) 2006-2007 NEC Corporation
 * All rights reserved.
 */

	.file	"_convert_fl.s"

#include <sys/asm_linkage.h>
#if defined(__ARM_EABI__)
#include "synonyms.h"

	ANSI_PRAGMA_WEAK2(__aeabi_d2iz,__fixdfsi,function)
	ANSI_PRAGMA_WEAK2(__aeabi_d2uiz,__fixunsdfsi,function)
	ANSI_PRAGMA_WEAK2(__aeabi_f2iz,__fixsfsi,function)
	ANSI_PRAGMA_WEAK2(__aeabi_f2uiz,__fixunssfsi,function)
	ANSI_PRAGMA_WEAK2(__aeabi_d2f,__truncdfsf2,function)
	ANSI_PRAGMA_WEAK2(__aeabi_f2d,__extendsfdf2,function)
	ANSI_PRAGMA_WEAK2(__aeabi_i2d,__floatsidf,function)
	ANSI_PRAGMA_WEAK2(__aeabi_ui2d,__float_ui2d,function)
	ANSI_PRAGMA_WEAK2(__aeabi_i2f,__floatsisf,function)
	ANSI_PRAGMA_WEAK2(__aeabi_ui2f,__float_ui2f,function)	
#endif

#if defined(__VFP_FP__)
	ENTRY(__extendsfdf2)		/* float -> double */
	fmsr		s2, r0
	fcvtds		d0, s2
	fmrs		r0, s0
	fmrs		r1, s1
	bx	lr
	SET_SIZE(__extendsfdf2)
	
	ENTRY(__truncdfsf2)		/* double -> float */
	fmsr		s2, r0
	fmsr		s3, r1
	fcvtsd		s0, d1
	fmrs		r0, s0
	bx	lr
	SET_SIZE(__truncdfsf2)
	
	ENTRY(__fixsfsi)		/* float -> int */
	fmsr		s0, r0
	ftosizs		s1, s0
	fmrs		r0, s1
	bx	lr
	SET_SIZE(__fixsfsi)
	
	ENTRY(__fixunssfsi)		/* float -> unsigned int */
	fmsr		s0, r0
	ftouizs		s1, s0
	fmrs		r0, s1
	bx	lr
	SET_SIZE(__fixunssfsi)	
		
	ENTRY(__fixdfsi)		/* double -> int */
	fmsr		s0, r0
	fmsr		s1, r1
	ftosizd		s2, d0
	fmrs		r0, s2
	bx	lr
	SET_SIZE(__fixdfsi)
	
	ENTRY(__fixunsdfsi)		/* double -> unsigned int */
	fmsr		s0, r0
	fmsr		s1, r1
	ftouizd		s2, d0
	fmrs		r0, s2
	bx	lr
	SET_SIZE(__fixunsdfsi)	
		

	ENTRY(__floatsisf)		/* int -> float */
	fmsr		s0, r0
	fsitos		s1, s0
	fmrs		r0, s1
	bx	lr
	SET_SIZE(__floatsisf)
	
	ENTRY(__floatsidf)		/* int -> double */
	fmsr		s0, r0
	fsitod		d1, s0
	fmrs		r0, s2
	fmrs		r1, s3
	bx	lr
	SET_SIZE(__floatsidf)

	ENTRY(__float_ui2f)		/* unsigned int -> float */
	fmsr		s0, r0
	fuitos		s1, s0
	fmrs		r0, s1
	bx	lr
	SET_SIZE(__float_ui2f)
	
	ENTRY(__float_ui2d)		/* unsigned int -> double */
	fmsr		s0, r0
	fuitod		d1, s0
	fmrs		r0, s2
	fmrs		r1, s3
	bx	lr
	SET_SIZE(__float_ui2d)

	
#else
	ENTRY(__extendsfdf2)		/* float -> double */
	.word	0xee010a10		/* fmsr		s2, r0 */
	.word	0xeeb70ac1		/* fcvtds	d0, s2 */
	.word	0xee101a10		/* fmrs		r1, s0 */
	.word	0xee100a90		/* fmrs		r0, s1 */
	bx	lr
	SET_SIZE(__extendsfdf2)
	
	ENTRY(__truncdfsf2)		/* double -> float */
	.word	0xee011a10		/* fmsr		s2, r1 */
	.word	0xee010a90		/* fmsr		s3, r0 */
	.word	0xeeb70bc1		/* fcvtsd	s0, d1 */
	.word	0xee100a10		/* fmrs		r0, s0 */
	bx	lr
	SET_SIZE(__truncdfsf2)
	
	ENTRY(__fixsfsi)		/* float -> int */
	.word	0xee000a10		/* fmsr		s0, r0 */
	.word	0xeefd0ac0		/* ftosizs	s1, s0 */
	.word	0xee100a90		/* fmrs		r0, s1 */
	bx	lr
	SET_SIZE(__fixsfsi)
	
	ENTRY(__fixunssfsi)		/* float -> unsigned int */
	.word	0xee000a10		/* fmsr		s0, r0 */
	.word	0xeefc0ac0		/* ftouizs	s1, s0 */	
	.word	0xee100a90		/* fmrs		r0, s1 */
	bx	lr
	SET_SIZE(__fixunssfsi)	
		
	ENTRY(__fixdfsi)		/* double -> int */
	.word	0xee001a10		/* fmsr		s0, r1 */
	.word	0xee000a90		/* fmsr		s1, r0 */
	.word	0xeebd1bc0		/* ftosizd	s2, d0 */
	.word	0xee110a10		/* fmrs		r0, s2 */
	bx	lr
	SET_SIZE(__fixdfsi)
	
	ENTRY(__fixunsdfsi)		/* double -> unsigned int */
	.word	0xee001a10		/* fmsr		s0, r1 */
	.word	0xee000a90		/* fmsr		s1, r0 */
	.word	0xeebc1bc0		/* ftouizd	s2, d0 */	
	.word	0xee110a10		/* fmrs		r0, s2 */
	bx	lr
	SET_SIZE(__fixunsdfsi)	
		

	ENTRY(__floatsisf)		/* int -> float */
	.word	0xee000a10		/* fmsr		s0, r0 */
	.word	0xeef80ac0		/* fsitos	s1, s0 */
	.word	0xee100a90		/* fmrs		r0, s1 */
	bx	lr
	SET_SIZE(__floatsisf)
	
	ENTRY(__floatsidf)		/* int -> double */
	.word	0xee000a10		/* fmsr		s0, r0 */
	.word	0xeeb81bc0		/* fsitod	d1, s0 */
	.word	0xee111a10		/* fmrs		r1, s2 */
	.word	0xee110a90		/* fmrs		r0, s3 */
	bx	lr
	SET_SIZE(__floatsidf)
#endif

