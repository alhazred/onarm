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
 * Copyright (c) 2007-2008 NEC Corporation
 * All rights reserved.
 */

	.ident  "@(#)_ext_help.s"

#if defined(__ARM_EABI__)

#include "SYS.h"
#include <sys/asm_linkage.h>

	ENTRY(__aeabi_drsub)
#if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	fsubd	d0, d1, d0
	fmrs	r0, s0
	fmrs	r1, s1
#else
	.word	0xee001a10	/* fmsr		s0, r1 */
	.word	0xee000a90	/* fmsr		s1, r0 */
	.word	0xee013a10	/* fmsr		s2, r3 */
	.word	0xee012a90	/* fmsr		s3, r2 */
	.word	0xee310b40	/* fsubd	d0, d1, d0 */
	.word	0xee101a10	/* fmrs		r1, s0 */
	.word	0xee100a90	/* fmrs		r0, s1 */
#endif
	bx	lr
	SET_SIZE(__aeabi_drsub)

	ENTRY(__aeabi_dneg)
#if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fnegd	d0, d0
	fmrs	r0, s0
	fmrs	r1, s1
#else
	.word	0xee001a10	/* fmsr		s0, r1 */
	.word	0xee000a90	/* fmsr		s1, r0 */
	.word	0xeeb10b40	/* fnegd	d0, d0 */
	.word	0xee101a10	/* fmrs		r1, s0 */
	.word	0xee100a90	/* fmrs		r0, s1 */
#endif
	bx	lr
	SET_SIZE(__aeabi_dneg)

	ENTRY(__aeabi_frsub)
#if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fsubs	s0, s1, s0
	fmrs	r0, s0
#else
	.word	0xee000a10	/* fmsr		s0, r0 */
	.word	0xee001a90	/* fmsr		s1, r1 */
	.word	0xee300ac0	/* fsubs	s0, s1, s0 */
	.word	0xee100a10	/* fmrs		r0, s0 */
#endif
	bx	lr
	SET_SIZE(__aeabi_frsub)

	ENTRY(__aeabi_fneg)
#if defined(__VFP_FP__)
	fmsr	s0, r0
	fnegs	s0, s0
	fmrs	r0, s0
#else
	.word	0xee000a10	/* fmsr		s0, r0 */
	.word	0xeeb10a40	/* fnegs	s0, s0 */
	.word	0xee100a10	/* fmrs		r0, s0 */
#endif
	bx	lr
	SET_SIZE(__aeabi_fneg)

	ENTRY(__aeabi_cdcmpeq)
# if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	fcmpd	d0, d1
	fmstat
# else
	.word	0xee001a10	/* fmsr		s0, r1	*/
	.word	0xee000a90	/* fmsr		s1, r0	*/
	.word	0xee013a10	/* fmsr		s2, r3	*/
	.word	0xee012a90	/* fmsr		s3, r2	*/
	.word	0xeeb40b41	/* fcmpd	d0, d1	*/
	.word	0xeef1fa10	/* fmstat		*/
# endif
	bx	lr
	SET_SIZE(__aeabi_cdcmpeq)

	ENTRY(__aeabi_cdcmple)
# if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	fcmpd	d0, d1
	fmstat
# else
	.word	0xee001a10	/* fmsr		s0, r1	*/
	.word	0xee000a90	/* fmsr		s1, r0	*/
	.word	0xee013a10	/* fmsr		s2, r3	*/
	.word	0xee012a90	/* fmsr		s3, r2	*/
	.word	0xeeb40b41	/* fcmpd	d0, d1	*/
	.word	0xeef1fa10	/* fmstat		*/
# endif
	bx	lr
	SET_SIZE(__aeabi_cdcmple)

	ENTRY(__aeabi_cdrcmple)
# if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	fcmpd	d1, d0
	fmstat
# else
	.word	0xee001a10	/* fmsr		s0, r1	*/
	.word	0xee000a90	/* fmsr		s1, r0	*/
	.word	0xee013a10	/* fmsr		s2, r3	*/
	.word	0xee012a90	/* fmsr		s3, r2	*/
	.word	0xeeb41b40	/* fcmpd	d1, d0	*/
	.word	0xeef1fa10	/* fmstat		*/
# endif
	bx	lr
	SET_SIZE(__aeabi_cdrcmple)

	ENTRY(__aeabi_cfcmpeq)
# if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fcmps	s0, s1
	fmstat
#else
	.word	0xee000a10	/* fmsr		s0, r0	*/
	.word	0xee001a90	/* fmsr		s1, r1	*/
	.word	0xeeb40a60	/* fcmps	s0, s1	*/
	.word	0xeef1fa10	/* fmstat		*/
#endif
	bx	lr
	SET_SIZE(__aeabi_cfcmpeq)

	ENTRY(__aeabi_cfcmple)
# if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fcmps	s0, s1
	fmstat
#else
	.word	0xee000a10	/* fmsr		s0, r0	*/
	.word	0xee001a90	/* fmsr		s1, r1	*/
	.word	0xeeb40a60	/* fcmps	s0, s1	*/
	.word	0xeef1fa10	/* fmstat		*/
#endif
	bx	lr
	SET_SIZE(__aeabi_cfcmple)

	ENTRY(__aeabi_cfrcmple)
# if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fcmps	s1, s0
	fmstat
#else
	.word	0xee000a10	/* fmsr		s0, r0	*/
	.word	0xee001a90	/* fmsr		s1, r1	*/
	.word	0xeef40a40	/* fcmps	s1, s0	*/
	.word	0xeef1fa10	/* fmstat		*/
#endif
	bx	lr
	SET_SIZE(__aeabi_cfrcmple)

	ENTRY(__aeabi_llsl)
	cmp	r2, #0
	bxle	lr
	str	lr, [sp, #-8]
	sub	sp, sp, #16
	mov	r3, r2
	mov	r2, r1
	mov	r1, r0
	mov	r0, sp
	bl	_fref_(lshiftl)
	ldmia	sp, {r0, r1}	
	add	sp, sp, #16
	ldr	pc, [sp, #-8]
	SET_SIZE(__aeabi_llsl)

	ENTRY(__aeabi_dcmpeq)
#if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	fcmpd	d0, d1
	fmstat
#else
	.word	0xee001a10	/* fmsr		s0, r1	*/
	.word	0xee000a90	/* fmsr		s1, r0	*/
	.word	0xee013a10	/* fmsr		s2, r3	*/
	.word	0xee012a90	/* fmsr		s3, r2	*/
	.word	0xeeb40b41	/* fcmpd	d0, d1	*/
	.word	0xeef1fa10	/* fmstat		*/
#endif
	movne	r0, #0
	moveq	r0, #1
	bx	lr
	SET_SIZE(__aeabi_dcmpeq)

	ENTRY(__aeabi_dcmplt)
#if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	fcmpd	d0, d1
	fmstat
#else
	.word	0xee001a10	/* fmsr		s0, r1	*/
	.word	0xee000a90	/* fmsr		s1, r0	*/
	.word	0xee013a10	/* fmsr		s2, r3	*/
	.word	0xee012a90	/* fmsr		s3, r2	*/
	.word	0xeeb40b41	/* fcmpd	d0, d1	*/
	.word	0xeef1fa10	/* fmstat		*/
#endif
	movge	r0, #0
	movlt	r0, #1
	bx	lr
	SET_SIZE(__aeabi_dcmplt)

	ENTRY(__aeabi_dcmple)
#if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	fcmpd	d0, d1
	fmstat
#else
	.word	0xee001a10	/* fmsr		s0, r1	*/
	.word	0xee000a90	/* fmsr		s1, r0	*/
	.word	0xee013a10	/* fmsr		s2, r3	*/
	.word	0xee012a90	/* fmsr		s3, r2	*/
	.word	0xeeb40b41	/* fcmpd	d0, d1	*/
	.word	0xeef1fa10	/* fmstat		*/
#endif
	movgt	r0, #0
	movle	r0, #1
	bx	lr
	SET_SIZE(__aeabi_dcmple)

	ENTRY(__aeabi_dcmpge)
#if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	fcmpd	d0, d1
	fmstat
#else
	.word	0xee001a10	/* fmsr		s0, r1	*/
	.word	0xee000a90	/* fmsr		s1, r0	*/
	.word	0xee013a10	/* fmsr		s2, r3	*/
	.word	0xee012a90	/* fmsr		s3, r2	*/
	.word	0xeeb40b41	/* fcmpd	d0, d1	*/
	.word	0xeef1fa10	/* fmstat		*/
#endif
	movlt	r0, #0
	movge	r0, #1
	bx	lr
	SET_SIZE(__aeabi_dcmpge)

	ENTRY(__aeabi_dcmpgt)
#if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fmsr	s2, r2
	fmsr	s3, r3
	fcmpd	d0, d1
	fmstat
#else
	.word	0xee001a10	/* fmsr		s0, r1	*/
	.word	0xee000a90	/* fmsr		s1, r0	*/
	.word	0xee013a10	/* fmsr		s2, r3	*/
	.word	0xee012a90	/* fmsr		s3, r2	*/
	.word	0xeeb40b41	/* fcmpd	d0, d1	*/
	.word	0xeef1fa10	/* fmstat		*/
#endif
	movle	r0, #0
	movgt	r0, #1
	bx	lr
	SET_SIZE(__aeabi_dcmpgt)

	ENTRY(__aeabi_fcmpeq)
#if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fcmps	s0, s1
	fmstat
#else
	.word	0xee000a10	/* fmsr		s0, r0	*/
	.word	0xee001a90	/* fmsr		s1, r1	*/
	.word	0xeeb40a60	/* fcmps	s0, s1	*/
	.word	0xeef1fa10	/* fmstat		*/
#endif
	movne	r0, #0
	moveq	r0, #1
	bx	lr
	SET_SIZE(__aeabi_fcmpeq)

	ENTRY(__aeabi_fcmplt)
#if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fcmps	s0, s1
	fmstat
#else
	.word	0xee000a10	/* fmsr		s0, r0	*/
	.word	0xee001a90	/* fmsr		s1, r1	*/
	.word	0xeeb40a60	/* fcmps	s0, s1	*/
	.word	0xeef1fa10	/* fmstat		*/
#endif
	movge	r0, #0
	movlt	r0, #1
	bx	lr
	SET_SIZE(__aeabi_fcmplt)

	ENTRY(__aeabi_fcmple)
#if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fcmps	s0, s1
	fmstat
#else
	.word	0xee000a10	/* fmsr		s0, r0	*/
	.word	0xee001a90	/* fmsr		s1, r1	*/
	.word	0xeeb40a60	/* fcmps	s0, s1	*/
	.word	0xeef1fa10	/* fmstat		*/
#endif
	movgt	r0, #0
	movle	r0, #1
	bx	lr
	SET_SIZE(__aeabi_fcmple)

	ENTRY(__aeabi_fcmpge)
#if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fcmps	s0, s1
	fmstat
#else
	.word	0xee000a10	/* fmsr		s0, r0	*/
	.word	0xee001a90	/* fmsr		s1, r1	*/
	.word	0xeeb40a60	/* fcmps	s0, s1	*/
	.word	0xeef1fa10	/* fmstat		*/
#endif
	movlt	r0, #0
	movge	r0, #1
	bx	lr
	SET_SIZE(__aeabi_fcmpge)

	ENTRY(__aeabi_fcmpgt)
#if defined(__VFP_FP__)
	fmsr	s0, r0
	fmsr	s1, r1
	fcmps	s0, s1
	fmstat
#else
	.word	0xee000a10	/* fmsr		s0, r0	*/
	.word	0xee001a90	/* fmsr		s1, r1	*/
	.word	0xeeb40a60	/* fcmps	s0, s1	*/
	.word	0xeef1fa10	/* fmstat		*/
#endif
	movle	r0, #0
	movgt	r0, #1
	bx	lr
	SET_SIZE(__aeabi_fcmpgt)
	
	ENTRY(__aeabi_ldivmod)
	str	lr, [sp, #-8]
	sub	sp, sp, #24
	add	ip, sp, #8
	str	ip, [sp]
	bl	_fref_(__divrem64)
	add	sp, sp, #8
	ldmia	sp, {r2, r3}
	add	sp, sp, #16
	ldr	pc, [sp, #-8]
	SET_SIZE(__aeabi_ldivmod)

	ENTRY(__aeabi_uldivmod)
	str	lr, [sp, #-8]
	sub	sp, sp, #24
	add	ip, sp, #8
	str	ip, [sp]
	bl	_fref_(__udivrem64)
	add	sp, sp, #8
	ldmia	sp, {r2, r3}
	add	sp, sp, #16
	ldr	pc, [sp, #-8]
	SET_SIZE(__aeabi_uldivmod)

	ENTRY(__aeabi_lcmp)
	subs	ip, r1, r3
	cmp	ip, #0
	sbceqs	ip, r0, r2
	mov	r0, ip
	bx	lr
	SET_SIZE(__aeabi_lcmp)

	ENTRY(__aeabi_ulcmp)
	subs	ip, r1, r3
	cmp	ip, #0
	sbceqs	ip, r0, r2
	mov	r0, ip
	bx	lr
	SET_SIZE(__aeabi_ulcmp)

	ENTRY(__aeabi_uread4)
	tst	r0, #3
	ldreq	r2, [r0]
	beq	.Lend_uread4
	ldrb	r2, [r0, #3]	/* read 4th byte */
	ldrb	r3, [r0, #2]	/* read 3rd byte */
	orr	r2, r3, r2, lsl #8
	ldrb	r3, [r0, #1]	/* read 2nd byte */
	orr	r2, r3, r2, lsl #8
	ldrb	r3, [r0]	/* read 1st byte */
	orr	r2, r3, r2, lsl #8
.Lend_uread4:
	mov	r0, r2
	bx	lr
	SET_SIZE(__aeabi_uread4)

	ENTRY(__aeabi_uwrite4)
	tst	r1, #3
	streq	r0, [r1]
	beq	.Lend_uwrite_4
	mov	r2, r0, lsr #8
	mov	r3, r0, lsr #16
	strb	r0, [r1]	/* write 1st byte */
	strb	r2, [r1, #1]	/* write 2nd byte */
	mov	r2, r0, lsr #24
	strb	r3, [r1, #2]	/* write 3rd byte */
	strb	r2, [r1, #3]	/* write 4th byte */
.Lend_uwrite_4:
	bx	lr
	SET_SIZE(__aeabi_uwrite4)

	ENTRY(__aeabi_uread8)
	tst	r0, #3
	ldreq	r1, [r0], #4
	ldreq	r2, [r0]
	beq	.Lend_uread8
	ldrb	r1, [r0, #3]	/* read 4th byte */
	ldrb	r3, [r0, #2]	/* read 3rd byte */
	orr	r1, r3, r1, lsl #8
	ldrb	r3, [r0, #1]	/* read 2nd byte */
	orr	r1, r3, r1, lsl #8
	ldrb	r3, [r0]	/* read 1st byte */
	orr	r1, r3, r1, lsl #8
	ldrb	r2, [r0, #7]	/* read 8th byte */
	ldrb	r3, [r0, #6]	/* read 7th byte */
	orr	r2, r3, r2, lsl #8
	ldrb	r3, [r0, #5]	/* read 6th byte */
	orr	r2, r3, r2, lsl #8
	ldrb	r3, [r0, #4]	/* read 5th byte */
	orr	r2, r3, r2, lsl #8
.Lend_uread8:
	mov	r0, r1
	mov	r1, r2
	bx	lr
	SET_SIZE(__aeabi_uread8)

	ENTRY(__aeabi_uwrite8)
	tst	r2, #3
	streq	r0, [r2]
	streq	r1, [r2, #4]
	beq	.Lend_uwrite8
	mov	r3, r0, lsr #8
	mov	ip, r0, lsr #16
	strb	r0, [r2]	/* write 1st byte */
	strb	r3, [r2, #1]	/* write 2nd byte */
	mov	r3, r0, lsr #24
	strb	ip, [r2, #2]	/* write 3rd byte */
	strb	r3, [r2, #3]	/* write 4th byte */
	mov	r3, r1, lsr #8
	mov	ip, r1, lsr #16
	strb	r1, [r2, #4]	/* write 5th byte */
	strb	r3, [r2, #5]	/* write 6th byte */
	mov	r3, r1, lsr #24
	strb	ip, [r2, #6]	/* write 7th byte */
	strb	r3, [r2, #7]	/* write 8th byte */
.Lend_uwrite8:
	bx	lr
	SET_SIZE(__aeabi_uwrite8)
		
#endif
