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

/* not NaN exception -> fcmps , fcmpd   */
/*     NaN exception -> fcmpes , fcmped */	
	.file	"_cmp_fl.s"

#include <SYS.h>
				/* VFP option not use helper function */
				/* VFP interface is differd OABI interface */			
#if defined(__VFP_FP__)
	ENTRY(__eqsf2)
	fmsr	s0, r0
	fmsr	s1, r1
	fcmps	s0, s1
	fmstat
	movne	r0, #1		/* as same as OABI interface */
	moveq	r0, #0		/* as same as OABI interface */
	bx	lr
	SET_SIZE(__eqsf2)
	
	ENTRY(__eqdf2)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	fcmpd	d0, d1
	fmstat
	movne	r0, #1		/* as same as OABI interface */
	moveq	r0, #0		/* as same as OABI interface */
	bx	lr
	SET_SIZE(__eqdf2)
	
	ENTRY(__nesf2)
	fmsr	s0, r0
	fmsr	s1, r1
	fcmps	s0, s1
	fmstat
	movne	r0, #1
	moveq	r0, #0
	bx	lr
	SET_SIZE(__nesf2)
	
	ENTRY(__nedf2)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	fcmpd	d0, d1
	fmstat
	movne	r0, #1
	moveq	r0, #0
	bx	lr
	SET_SIZE(__nedf2)
	
	ENTRY(__gesf2)
	fmsr	s0, r0
	fmsr	s1, r1
	fcmps	s0, s1
	fmstat
	movlt	r0, #-1		/* as same as OABI interface */
	movge	r0, #1
	bx	lr
	SET_SIZE(__gesf2)
	
	ENTRY(__gedf2)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	fcmpd	d0, d1
	fmstat
	movlt	r0, #-1		/* as same as OABI interface */
	movge	r0, #1
	bx	lr
	SET_SIZE(__gedf2)
	
	ENTRY(__gtsf2)
	fmsr	s0, r0
	fmsr	s1, r1
	fcmps	s0, s1
	fmstat
	movle	r0, #-1		/* as same as OABI interface */
	movgt	r0, #1
	bx	lr
	SET_SIZE(__gtsf2)
	
	ENTRY(__gtdf2)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	fcmpd	d0, d1
	fmstat
	movle	r0, #-1		/* as same as OABI interface */
	movgt	r0, #1
	bx	lr
	SET_SIZE(__gtdf2)
	
	ENTRY(__lesf2)
	fmsr	s0, r0
	fmsr	s1, r1
	fcmps	s0, s1
	fmstat
	movgt	r0, #1		
	movle	r0, #-1		/* as same as OABI interface */
	bx	lr
	SET_SIZE(__lesf2)
	
	ENTRY(__ledf2)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	fcmpd	d0, d1
	fmstat
	movgt	r0, #1		
	movle	r0, #-1		/* as same as OABI interface */
	bx	lr
	SET_SIZE(__ledf2)
	
	ENTRY(__ltsf2)
	fmsr	s0, r0
	fmsr	s1, r1
	fcmps	s0, s1
	fmstat
	movge	r0, #1		
	movlt	r0, #-1		/* as same as OABI interface */
	bx	lr
	SET_SIZE(__ltsf2)
	
	ENTRY(__ltdf2)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	fcmpd	d0, d1
	fmstat
	movge	r0, #1		
	movlt	r0, #-1		/* as same as OABI interface */
	bx	lr
	SET_SIZE(__ltdf2)

#else
	ENTRY(__eqsf2)
	.word	0xee000a10		/* fmsr		s0, r0 */
	.word	0xee001a90		/* fmsr		s1, r1 */
	.word	0xeeb40a60		/* fcmps	s0, s1 */
	.word	0xeef1fa10		/* fmstat	       */
	movne	r0, #1
	moveq	r0, #0	
	bx	lr
	SET_SIZE(__eqsf2)
	
	ENTRY(__eqdf2)
	.word	0xee001a10		/* fmsr		s0, r1 */
	.word	0xee000a90		/* fmsr		s1, r0 */
	.word	0xee013a10		/* fmsr		s2, r3 */
	.word	0xee012a90		/* fmsr		s3, r2 */
	.word	0xeeb40b41		/* fcmpd	d0, d1 */
	.word	0xeef1fa10		/* fmstat	       */
	movne	r0, #1
	moveq	r0, #0	
	bx	lr
	SET_SIZE(__eqdf2)
	
	ENTRY(__nesf2)
	.word	0xee000a10		/* fmsr		s0, r0 */
	.word	0xee001a90		/* fmsr		s1, r1 */
	.word	0xeeb40a60		/* fcmps	s0, s1 */
	.word	0xeef1fa10		/* fmstat	       */
	movne	r0, #1
	moveq	r0, #0
	bx	lr
	SET_SIZE(__nesf2)
	
	ENTRY(__nedf2)
	.word	0xee001a10		/* fmsr		s0, r1 */
	.word	0xee000a90		/* fmsr		s1, r0 */
	.word	0xee013a10		/* fmsr		s2, r3 */
	.word	0xee012a90		/* fmsr		s3, r2 */
	.word	0xeeb40b41		/* fcmpd	d0, d1 */
	.word	0xeef1fa10		/* fmstat	       */
	movne	r0, #1
	moveq	r0, #0
	bx	lr
	SET_SIZE(__nedf2)
	
	ENTRY(__gesf2)
	.word	0xee000a10		/* fmsr		s0, r0 */
	.word	0xee001a90		/* fmsr		s1, r1 */
	.word	0xeeb40a60		/* fcmps	s0, s1 */
	.word	0xeef1fa10		/* fmstat	       */
	movlt	r0, #-1
	movge	r0, #1
	bx	lr
	SET_SIZE(__gesf2)
	
	ENTRY(__gedf2)
	.word	0xee001a10		/* fmsr		s0, r1 */
	.word	0xee000a90		/* fmsr		s1, r0 */
	.word	0xee013a10		/* fmsr		s2, r3 */
	.word	0xee012a90		/* fmsr		s3, r2 */
	.word	0xeeb40b41		/* fcmpd	d0, d1 */
	.word	0xeef1fa10		/* fmstat	       */
	movlt	r0, #-1
	movge	r0, #1
	bx	lr
	SET_SIZE(__gedf2)
	
	ENTRY(__gtsf2)
	.word	0xee000a10		/* fmsr		s0, r0 */
	.word	0xee001a90		/* fmsr		s1, r1 */
	.word	0xeeb40a60		/* fcmps	s0, s1 */
	.word	0xeef1fa10		/* fmstat	       */
	movle	r0, #-1
	movgt	r0, #1
	bx	lr
	SET_SIZE(__gtsf2)
	
	ENTRY(__gtdf2)
	.word	0xee001a10		/* fmsr		s0, r1 */
	.word	0xee000a90		/* fmsr		s1, r0 */
	.word	0xee013a10		/* fmsr		s2, r3 */
	.word	0xee012a90		/* fmsr		s3, r2 */
	.word	0xeeb40b41		/* fcmpd	d0, d1 */
	.word	0xeef1fa10		/* fmstat	       */
	movle	r0, #-1
	movgt	r0, #1
	bx	lr
	SET_SIZE(__gtdf2)
	
	ENTRY(__lesf2)
	.word	0xee000a10		/* fmsr		s0, r0 */
	.word	0xee001a90		/* fmsr		s1, r1 */
	.word	0xeeb40a60		/* fcmps	s0, s1 */
	.word	0xeef1fa10		/* fmstat	       */
	movgt	r0, #1
	movle	r0, #-1
	bx	lr
	SET_SIZE(__lesf2)
	
	ENTRY(__ledf2)
	.word	0xee001a10		/* fmsr		s0, r1 */
	.word	0xee000a90		/* fmsr		s1, r0 */
	.word	0xee013a10		/* fmsr		s2, r3 */
	.word	0xee012a90		/* fmsr		s3, r2 */
	.word	0xeeb40b41		/* fcmpd	d0, d1 */
	.word	0xeef1fa10		/* fmstat	       */
	movgt	r0, #1
	movle	r0, #-1
	bx	lr
	SET_SIZE(__ledf2)
	
	ENTRY(__ltsf2)
	.word	0xee000a10		/* fmsr		s0, r0 */
	.word	0xee001a90		/* fmsr		s1, r1 */
	.word	0xeeb40a60		/* fcmps	s0, s1 */
	.word	0xeef1fa10		/* fmstat	       */
	movge	r0, #1
	movlt	r0, #-1
	bx	lr
	SET_SIZE(__ltsf2)
	
	ENTRY(__ltdf2)
	.word	0xee001a10		/* fmsr		s0, r1 */
	.word	0xee000a90		/* fmsr		s1, r0 */
	.word	0xee013a10		/* fmsr		s2, r3 */
	.word	0xee012a90		/* fmsr		s3, r2 */
	.word	0xeeb40b41		/* fcmpd	d0, d1 */
	.word	0xeef1fa10		/* fmstat	       */
	movge	r0, #1
	movlt	r0, #-1
	bx	lr
	SET_SIZE(__ltdf2)
#endif

