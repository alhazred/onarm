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

	.ident	"@(#)atomic.s"
	.file	"atomic.s"

#include <sys/asm_linkage.h>
#include <asm/cpufunc.h>

#ifdef	_KERNEL
	/*
	 * Legacy kernel interfaces; they will go away (eventually).
	 */
	ANSI_PRAGMA_WEAK2(cas8, atomic_cas_8, function)
	ANSI_PRAGMA_WEAK2(cas32, atomic_cas_32, function)
	ANSI_PRAGMA_WEAK2(cas64, atomic_cas_64, function)
	ANSI_PRAGMA_WEAK2(caslong, atomic_cas_ulong, function)
	ANSI_PRAGMA_WEAK2(casptr, atomic_cas_ptr, function)
	ANSI_PRAGMA_WEAK2(atomic_and_long, atomic_and_ulong, function)
	ANSI_PRAGMA_WEAK2(atomic_or_long, atomic_or_ulong, function)
#else	/* !_KERNEL */
	/*
	 * Include the definitions for the libc weak aliases.
	 */
#include "../atomic_asm_weak.h"
#endif	/* _KERNEL */

/*
 * Templates for atomic operations.
 *
 *   - They use ARMv6T2 variant instructions.
 *   - Long long can't use them.
 */

/* Increment or decrement */
#define	ATOMIC_INCR(ex, op)						\
1:	ldr##ex	r1, [r0];	/* Load exclusive */			\
	op	r1, r1, #1;	/* Change value */			\
	str##ex	r2, r1, [r0];	/* Store exclusive */			\
	teq	r2, #0;		/* Check result */			\
	bne	1b;		/* Retry when fails */			\
	mov	pc, lr

/* Increment or decrement, and then return new value. */
#define	ATOMIC_INCR_NV(ex, op)						\
1:	ldr##ex	r1, [r0];	/* Load exclusive */			\
	op	r1, r1, #1;	/* Change value */			 \
	str##ex	r2, r1, [r0];	/* Store exclusive */			\
	teq	r2, #0;		/* Check result */			\
	bne	1b;		/* Retry when fails */			\
	mov	r0, r1;		/* Return new value */			\
	mov	pc, lr

/* Calculate. */
#define	ATOMIC_CALC(ex, op)						\
1:	ldr##ex	r2, [r0];	/* Load exclusive */			\
	op	r2, r2, r1;	/* Calculate with the specified value */\
	str##ex	r3, r2, [r0];	/* Store exclusive */			\
	teq	r3, #0;		/* Check result */			\
	bne	1b;		/* Retry when fails */			\
	mov pc, lr

/* Calculate, and tehen return new value. */
#define	ATOMIC_CALC_NV(ex, op)						\
1:	ldr##ex	r2, [r0];	/* Load exclusive */			\
	op	r2, r2, r1;	/* Calculate with the specified value */\
	str##ex	r3, r2, [r0];	/* Store exclusive */			\
	teq	r3, #0;		/* Check result */			\
	bne	1b;		/* Retry when fails */			\
	mov	r0, r2;		/* Return new value */			\
	mov pc, lr

/* Compare values, and then store if matched. */
#define	ATOMIC_CAS(ex)							\
	mov	ip, r0;		/* Keep target address */		\
1:									\
	ldr##ex	r0, [ip];	/* Load target */			\
	teq	r0, r1;		/* Compare values */			\
	movne	pc, lr;		/* Do nothing if not matched */		\
	str##ex	r3, r2, [ip];	/* Store new value */			\
	teq	r3, #0;		/* Check result */			\
	bne	1b;		/* Retry when fails */			\
	mov	pc, lr

/* Swap value */
#define	ATOMIC_SWAP(ex)							\
1:	ldr##ex	r2, [r0];	/* Load target */			\
	str##ex	r3, r1, [r0];	/* Install new value */			\
	teq	r3, #0;		/* Check result */			\
	bne	1b;		/* Return when fails */			\
	mov	r0, r2;		/* Return old value */			\
	mov	pc, lr

ENTRY(atomic_inc_8)
ALTENTRY(atomic_inc_uchar)
	ATOMIC_INCR(exb, add)
	SET_SIZE(atomic_inc_uchar)
	SET_SIZE(atomic_inc_8)

ENTRY(atomic_inc_16)
ALTENTRY(atomic_inc_ushort)
	ATOMIC_INCR(exh, add)
	SET_SIZE(atomic_inc_ushort)
	SET_SIZE(atomic_inc_16)

ENTRY(atomic_inc_32)
ALTENTRY(atomic_inc_uint)
ALTENTRY(atomic_inc_ulong)
	ATOMIC_INCR(ex, add)
	SET_SIZE(atomic_inc_ulong)
	SET_SIZE(atomic_inc_uint)
	SET_SIZE(atomic_inc_32)

ENTRY(atomic_inc_8_nv)
ALTENTRY(atomic_inc_uchar_nv)
	ATOMIC_INCR_NV(exb, add)
	SET_SIZE(atomic_inc_uchar_nv)
	SET_SIZE(atomic_inc_8_nv)

ENTRY(atomic_inc_16_nv)
ALTENTRY(atomic_inc_ushort_nv)
	ATOMIC_INCR_NV(exh, add)
	SET_SIZE(atomic_inc_ushort_nv)
	SET_SIZE(atomic_inc_16_nv)

ENTRY(atomic_inc_32_nv)
ALTENTRY(atomic_inc_uint_nv)
ALTENTRY(atomic_inc_ulong_nv)
	ATOMIC_INCR_NV(ex, add)
	SET_SIZE(atomic_inc_ulong_nv)
	SET_SIZE(atomic_inc_uint_nv)
	SET_SIZE(atomic_inc_32_nv)

ENTRY(atomic_dec_8)
ALTENTRY(atomic_dec_uchar)
	ATOMIC_INCR(exb, sub)
	SET_SIZE(atomic_dec_uchar)
	SET_SIZE(atomic_dec_8)

ENTRY(atomic_dec_16)
ALTENTRY(atomic_dec_ushort)
	ATOMIC_INCR(exh, sub)
	SET_SIZE(atomic_dec_ushort)
	SET_SIZE(atomic_dec_16)

ENTRY(atomic_dec_32)
ALTENTRY(atomic_dec_uint)
ALTENTRY(atomic_dec_ulong)
	ATOMIC_INCR(ex, sub)
	SET_SIZE(atomic_dec_ulong)
	SET_SIZE(atomic_dec_uint)
	SET_SIZE(atomic_dec_32)

ENTRY(atomic_dec_8_nv)
ALTENTRY(atomic_dec_uchar_nv)
	ATOMIC_INCR_NV(exb, sub)
	SET_SIZE(atomic_dec_uchar_nv)
	SET_SIZE(atomic_dec_8_nv)

ENTRY(atomic_dec_16_nv)
ALTENTRY(atomic_dec_ushort_nv)
	ATOMIC_INCR_NV(exh, sub)
	SET_SIZE(atomic_dec_ushort_nv)
	SET_SIZE(atomic_dec_16_nv)

ENTRY(atomic_dec_32_nv)
ALTENTRY(atomic_dec_uint_nv)
ALTENTRY(atomic_dec_ulong_nv)
	ATOMIC_INCR_NV(ex, sub)
	SET_SIZE(atomic_dec_ulong_nv)
	SET_SIZE(atomic_dec_uint_nv)
	SET_SIZE(atomic_dec_32_nv)

ENTRY(atomic_add_8)
ALTENTRY(atomic_add_char)
	ATOMIC_CALC(exb, add)
	SET_SIZE(atomic_add_char)
	SET_SIZE(atomic_add_8)

ENTRY(atomic_add_16)
ALTENTRY(atomic_add_short)
	ATOMIC_CALC(exh, add)
	SET_SIZE(atomic_add_short)
	SET_SIZE(atomic_add_16)

ENTRY(atomic_add_32)
ALTENTRY(atomic_add_int)
ALTENTRY(atomic_add_ptr)
ALTENTRY(atomic_add_long)
	ATOMIC_CALC(ex, add)
	SET_SIZE(atomic_add_long)
	SET_SIZE(atomic_add_ptr)
	SET_SIZE(atomic_add_int)
	SET_SIZE(atomic_add_32)

ENTRY(atomic_or_8)
ALTENTRY(atomic_or_uchar)
	ATOMIC_CALC(exb, orr)
	SET_SIZE(atomic_or_uchar)
	SET_SIZE(atomic_or_8)

ENTRY(atomic_or_16)
ALTENTRY(atomic_or_ushort)
	ATOMIC_CALC(exh, orr)
	SET_SIZE(atomic_or_ushort)
	SET_SIZE(atomic_or_16)

ENTRY(atomic_or_32)
ALTENTRY(atomic_or_uint)
ALTENTRY(atomic_or_ulong)
	ATOMIC_CALC(ex, orr)
	SET_SIZE(atomic_or_ulong)
	SET_SIZE(atomic_or_uint)
	SET_SIZE(atomic_or_32)

ENTRY(atomic_and_8)
ALTENTRY(atomic_and_uchar)
	ATOMIC_CALC(exb, and)
	SET_SIZE(atomic_and_uchar)
	SET_SIZE(atomic_and_8)

ENTRY(atomic_and_16)
ALTENTRY(atomic_and_ushort)
	ATOMIC_CALC(exh, and)
	SET_SIZE(atomic_and_ushort)
	SET_SIZE(atomic_and_16)

ENTRY(atomic_and_32)
ALTENTRY(atomic_and_uint)
ALTENTRY(atomic_and_ulong)
	ATOMIC_CALC(ex, and)
	SET_SIZE(atomic_and_ulong)
	SET_SIZE(atomic_and_uint)
	SET_SIZE(atomic_and_32)

ENTRY(atomic_add_8_nv)
ALTENTRY(atomic_add_char_nv)
	ATOMIC_CALC_NV(exb, add)
	SET_SIZE(atomic_add_char_nv)
	SET_SIZE(atomic_add_8_nv)

ENTRY(atomic_add_16_nv)
ALTENTRY(atomic_add_short_nv)
	ATOMIC_CALC_NV(exh, add)
	SET_SIZE(atomic_add_short_nv)
	SET_SIZE(atomic_add_16_nv)

ENTRY(atomic_add_32_nv)
ALTENTRY(atomic_add_int_nv)
ALTENTRY(atomic_add_ptr_nv)
ALTENTRY(atomic_add_long_nv)
	ATOMIC_CALC_NV(ex, add)
	SET_SIZE(atomic_add_long_nv)
	SET_SIZE(atomic_add_ptr_nv)
	SET_SIZE(atomic_add_int_nv)
	SET_SIZE(atomic_add_32_nv)

ENTRY(atomic_or_8_nv)
ALTENTRY(atomic_or_uchar_nv)
	ATOMIC_CALC_NV(exb, orr)
	SET_SIZE(atomic_or_uchar_nv)
	SET_SIZE(atomic_or_8_nv)

ENTRY(atomic_or_16_nv)
ALTENTRY(atomic_or_ushort_nv)
	ATOMIC_CALC_NV(exh, orr)
	SET_SIZE(atomic_or_ushort_nv)
	SET_SIZE(atomic_or_16_nv)

ENTRY(atomic_or_32_nv)
ALTENTRY(atomic_or_uint_nv)
ALTENTRY(atomic_or_ulong_nv)
	ATOMIC_CALC_NV(ex, orr)
	SET_SIZE(atomic_or_ulong_nv)
	SET_SIZE(atomic_or_uint_nv)
	SET_SIZE(atomic_or_32_nv)

ENTRY(atomic_and_8_nv)
ALTENTRY(atomic_and_uchar_nv)
	ATOMIC_CALC_NV(exb, and)
	SET_SIZE(atomic_and_uchar_nv)
	SET_SIZE(atomic_and_8_nv)

ENTRY(atomic_and_16_nv)
ALTENTRY(atomic_and_ushort_nv)
	ATOMIC_CALC_NV(exh, and)
	SET_SIZE(atomic_and_ushort_nv)
	SET_SIZE(atomic_and_16_nv)

ENTRY(atomic_and_32_nv)
ALTENTRY(atomic_and_uint_nv)
ALTENTRY(atomic_and_ulong_nv)
	ATOMIC_CALC_NV(ex, and)
	SET_SIZE(atomic_and_ulong_nv)
	SET_SIZE(atomic_and_uint_nv)
	SET_SIZE(atomic_and_32_nv)

ENTRY(atomic_cas_8)
ALTENTRY(atomic_cas_uchar)
	ATOMIC_CAS(exb)
	SET_SIZE(atomic_cas_uchar)
	SET_SIZE(atomic_cas_8)

ENTRY(atomic_cas_16)
ALTENTRY(atomic_cas_ushort)
	ATOMIC_CAS(exh)
	SET_SIZE(atomic_cas_ushort)
	SET_SIZE(atomic_cas_16)

ENTRY(atomic_cas_32)
ALTENTRY(atomic_cas_uint)
ALTENTRY(atomic_cas_ulong)
ALTENTRY(atomic_cas_ptr)
	ATOMIC_CAS(ex)
	SET_SIZE(atomic_cas_ptr)
	SET_SIZE(atomic_cas_ulong)
	SET_SIZE(atomic_cas_uint)
	SET_SIZE(atomic_cas_32)

ENTRY(atomic_swap_8)
ALTENTRY(atomic_swap_uchar)
	ATOMIC_SWAP(exb)
	SET_SIZE(atomic_swap_uchar)
	SET_SIZE(atomic_swap_8)

ENTRY(atomic_swap_16)
ALTENTRY(atomic_swap_ushort)
	ATOMIC_SWAP(exh)
	SET_SIZE(atomic_swap_ushort)
	SET_SIZE(atomic_swap_16)

ENTRY(atomic_swap_32)
ALTENTRY(atomic_swap_uint)
ALTENTRY(atomic_swap_ptr)
ALTENTRY(atomic_swap_ulong)
	ATOMIC_SWAP(ex)
	SET_SIZE(atomic_swap_ulong)
	SET_SIZE(atomic_swap_ptr)
	SET_SIZE(atomic_swap_uint)
	SET_SIZE(atomic_swap_32)

#ifdef	__ARM_EABI__

/*
 * On EABI mode, address of 64-bit value must be double word aligned,
 * and 64-bit argument is passed via even number register.
 * So we can use ldrexd/strexd safely.
 */
#define	ATOMIC_INCR64(func, ophi, oplo)					\
	mov	ip, r0;		/* Keep target address */		\
1:									\
	ldrexd	r0, [ip];	/* r0, r1 are used as destination */	\
	oplo	r0, r0, #1;	/* Change value (lower 32 bit) */	\
	ophi	r1, r1, #0;	/* Change value (higher 32 bit) */	\
	strexd	r2, r0, [ip];	/* Store result */			\
	teq	r2, #0;							\
	bne	1b;		/* Retry when fails */			\
	mov	pc, lr

/* Calculate 64 bit value, and return new value */
#define	ATOMIC_CALC64(func, ophi, oplo)					\
	stmfd	sp!, {r4};						\
	mov	ip, r0;		/* Keep target address */		\
1:									\
	ldrexd	r0, [ip];	/* r0, r1 are used as destination */	\
	oplo	r0, r0, r2;	/* Calculate (lower 32 bit) */		\
	ophi	r1, r1, r3;	/* Calculate (higher 32 bit) */		\
	strexd	r4, r0, [ip];	/* Store result */			\
	teq	r4, #0;							\
	bne	1b;		/* Retry when fails */			\
	ldmfd	sp!, {r4};						\
	mov	pc, lr

#else	/* !__ARM_EABI__ */

/*
 * Although we can use ldrexd/strexd for atomic operation, they requires
 * double word aligned address. On ARM architecture, 64 bit is represented
 * by a pair of register, so 64 bit value can located at word alignd address.
 *
 * The following atomic operations for 64bit value checks whether
 * the specified address is double word aligned. If double word aligned,
 * they uses ldrexd/strexd instructions. If not, they acquires global
 * lock.
 */

/* Symbol name of lock bit for the specified function */
#define	LOCKBIT_ATOMIC64(func)		lock_##func
#define	LOCKBIT_ATOMIC64_ADDR(func)	.Laddr_lock_##func

#ifdef	_KERNEL

/*
 * We must disable interrupt while atomic operation.
 * Note that this macro uses ip.
 */
#define	LOCK_INTR()							\
	mrs	ip, cpsr;						\
	cpsid	i

#define	UNLOCK_INTR()							\
	msr	cpsr_c, ip

#else	/* _KERNEL */

#define	LOCK_INTR()
#define	UNLOCK_INTR()

#endif	/* _KERNEL */

/* Acquire global lock for the specified function. */
#define	LOCK_ATOMIC64(func, lkaddr, reg1, reg2)				\
	LOCK_INTR();							\
	ldr	lkaddr, LOCKBIT_ATOMIC64_ADDR(func);			\
11:	ldrexb	reg1, [lkaddr];		/* Load lock bit */		\
	teq	reg1, #0;						\
	bne	11b;			/* Spin if already locked. */	\
	mov	reg1, #1;						\
	strexb	reg2, reg1, [lkaddr];					\
	teq	reg2, #0;						\
	bne	11b;			/* Lost the race. Try again. */	\
	MEMORY_BARRIER(reg2)		/* reg2 must be zero. */

/*
 * Release global lock for the specified function.
 *
 * Remarks:
 *	The caller must set register that keeps address of lock bit to lkaddr.
 */
#define	UNLOCK_ATOMIC64(lkaddr, reg1, reg2)				\
	mov	reg2, #0;						\
	MEMORY_BARRIER(reg2);						\
12:	ldrexb	reg1, [lkaddr];						\
	strexb	reg1, reg2, [lkaddr];					\
	teq	reg1, #0;						\
	bne	12b;			/* Lost the race. Try again. */	\
	UNLOCK_INTR()

/*
 * Declare lock bits for 64 bit atomic operations.
 * We assume that they are located at memory shared with all contexts
 * that use atomic operations.
 */
#define	LOCKBIT_ATOMIC64_DECL(func)					\
DGDEF2(LOCKBIT_ATOMIC64(func), 1)					\
	.byte	0;							\
	.text;								\
LOCKBIT_ATOMIC64_ADDR(func):						\
	.word	LOCKBIT_ATOMIC64(func)

LOCKBIT_ATOMIC64_DECL(atomic_inc_64)
LOCKBIT_ATOMIC64_DECL(atomic_dec_64)
LOCKBIT_ATOMIC64_DECL(atomic_add_64)
LOCKBIT_ATOMIC64_DECL(atomic_or_64)
LOCKBIT_ATOMIC64_DECL(atomic_and_64)
LOCKBIT_ATOMIC64_DECL(atomic_cas_64)
LOCKBIT_ATOMIC64_DECL(atomic_swap_64)

/* Increment or decrement 64 bit value */
#define	ATOMIC_INCR64(func, ophi, oplo)					\
	/* Check whether target address is double word aligned */	\
	tst	r0, #CLONGLONGMASK;					\
	bne	10f;							\
1:									\
	/* Address is double word aligned. We can use ldrexd/strexd. */	\
	ldrexd	r2, [r0];	/* r2, r3 are used as destination */	\
	oplo	r2, r2, #1;	/* Change value (lower 32 bit) */	\
	ophi	r3, r3, #0;	/* Change value (higher 32 bit) */	\
	strexd	r1, r2, [r0];	/* Store result */			\
	teq	r1, #0;							\
	bne	1b;		/* Retry when fails */			\
	mov	r0, r2;		/* Return new value */			\
	mov	r1, r3;							\
	b	100f;							\
10:									\
	stmfd	sp!, {r4};						\
	/*								\
	 * We can't use ldrexd/strexd because of bad address alignment.	\
	 * So we need to acquire global lock.				\
	 */								\
									\
	/* Lock address is preserved in r4. */				\
	LOCK_ATOMIC64(func, r4, r2, r3);				\
									\
	ldmia	r0, {r2-r3};	/* Load value into r2, r3 */		\
	oplo	r2, r2, #1;	/* Change value (lower 32 bit) */	\
	ophi	r3, r3, #0;	/* Change value (higher 32 bit) */	\
	stmia	r0, {r2-r3};	/* Store result */			\
	mov	r0, r2;		/* Return new value */			\
	mov	r1, r3;							\
									\
	UNLOCK_ATOMIC64(r4, r2, r3);					\
	ldmfd	sp!, {r4};						\
100:									\
	mov	pc, lr

/* Calculate 64 bit value, and return new value */
#define	ATOMIC_CALC64(func, ophi, oplo)					\
	stmfd	sp!, {r4-r5};						\
	/* Check whether target address is double word aligned */	\
	tst	r0, #CLONGLONGMASK;					\
	bne	10f;							\
1:									\
	/* Address is double word aligned. We can use ldrexd/strexd. */	\
	ldrexd	r4, [r0];	/* r4, r5 are used as destination */	\
	oplo	r4, r4, r1;	/* Calculate (lower 32 bit) */		\
	ophi	r5, r5, r2;	/* Calculate (higher 32 bit) */		\
	strexd	r3, r4, [r0];	/* Store result */			\
	teq	r3, #0;							\
	bne	1b;		/* Retry when fails */			\
	mov	r0, r4;		/* Return new value */			\
	mov	r1, r5;							\
	b	100f;							\
10:									\
	/*								\
	 * We can't use ldrexd/strexd because of bad address alignment.	\
	 * So we need to acquire global lock.				\
	 */								\
									\
	/* Lock address is preserved in r3. */				\
	LOCK_ATOMIC64(func, r3, r4, r5);				\
									\
	ldmia	r0, {r4-r5};	/* Load value into r4, r5 */		\
	oplo	r4, r4, r1;	/* Calculate (lower 32 bit) */		\
	ophi	r5, r5, r2;	/* Calculate (higher 32 bit) */		\
	stmia	r0, {r4-r5};	/* Store result */			\
	mov	r0, r4;		/* Return new value */			\
	mov	r1, r5;							\
									\
	UNLOCK_ATOMIC64(r3, r4, r5);					\
100:									\
	ldmfd	sp!, {r4-r5};						\
	mov	pc, lr

#endif	/* __ARM_EABI__ */

ENTRY(atomic_inc_64)
ALTENTRY(atomic_inc_64_nv)
	ATOMIC_INCR64(atomic_inc_64, adc, adds)
	SET_SIZE(atomic_inc_64_nv)
	SET_SIZE(atomic_inc_64)

ENTRY(atomic_dec_64)
ALTENTRY(atomic_dec_64_nv)
	ATOMIC_INCR64(atomic_dec_64, sbc, subs)
	SET_SIZE(atomic_dec_64_nv)
	SET_SIZE(atomic_dec_64)

ENTRY(atomic_add_64)
ALTENTRY(atomic_add_64_nv)
	ATOMIC_CALC64(atomic_add_64, adc, adds)
	SET_SIZE(atomic_add_64_nv)
	SET_SIZE(atomic_add_64)

ENTRY(atomic_or_64)
ALTENTRY(atomic_or_64_nv)
	ATOMIC_CALC64(atomic_or_64, orr, orr)
	SET_SIZE(atomic_or_64_nv)
	SET_SIZE(atomic_or_64)

ENTRY(atomic_and_64)
ALTENTRY(atomic_and_64_nv)
	ATOMIC_CALC64(atomic_and_64, and, and)
	SET_SIZE(atomic_and_64_nv)
	SET_SIZE(atomic_and_64)

ENTRY(atomic_cas_64)
#ifdef	__ARM_EABI__
	stmfd	sp!, {r4-r6}
	mov	ip, r0		/* Keep target address */
	ldrd	r4, [sp, #12]	/* Load new value into r4, r5 */
1:
	ldrexd	r0, [ip]	/* r0, r1 are used as destination */
	teq	r0, r2		/* Compare (lower 32 bit) */
	bne	10f		/* Return old value without change */
	teq	r1, r3		/* Compare (higher 32 bit) */
	bne	10f		/* Return old value without change */
	strexd	r6, r4, [ip]	/* Store new value */
	teq	r6, #0
	bne	1b		/* Retry when fails */
10:
	ldmfd	sp!, {r4-r6}
	mov	pc, lr
#else	/* !__ARM_EABI__ */
	stmfd	sp!, {r4-r8}

	mov	r6, r3
	ldr	r7, [sp, #20]	/* Load new value (higher 32 bit) */

	/* Check whether target address is double word aligned */
	tst	r0, #CLONGLONGMASK
	bne	10f
1:
	/* Address is double word aligned. We can use ldrexd/strexd. */
	ldrexd	r4, [r0]	/* r4, r5 are used as destination */
	teq	r4, r1		/* Compare (lower 32 bit) */
	movne	r0, r4
	movne	r1, r5
	bne	100f		/* Do nothing if not matched */
	teq	r5, r2		/* Compare (higher 32 bit) */
	movne	r0, r4
	movne	r1, r5
	bne	100f		/* Do nothing if not matched */
	strexd	r8, r6, [r0]	/* Store new value */
	teq	r8, #0
	bne	1b		/* Retry when fails */
	mov	r0, r4		/* Return old value */
	mov	r1, r5
	b	100f
10:
	/*
	 * We can't use ldrexd/strexd because of bad address alignment.
	 * So we need to acquire global lock.
	 */

	/* Lock address is preserved in r8. */
	LOCK_ATOMIC64(atomic_cas_64, r8, r4, r5)

	ldmia	r0, {r4-r5}	/* Load value into r4, r5 */
	teq	r4, r1		/* Compare (lower 32 bit) */
	movne	r0, r4
	movne	r1, r5
	bne	11f		/* Do nothing if not matched */
	teq	r5, r2		/* Compare (higher 32 bit) */
	movne	r0, r4
	movne	r1, r5
	bne	11f		/* Do nothing if not matched */
	stmia	r0, {r6-r7}	/* Store new value */
	mov	r0, r4		/* Return old value */
	mov	r1, r5
11:
	UNLOCK_ATOMIC64(r8, r4, r5)
100:
	ldmfd	sp!, {r4-r8}
	mov	pc, lr
#endif	/* __ARM_EABI__ */
	SET_SIZE(atomic_cas_64)

ENTRY(atomic_swap_64)
#ifdef	__ARM_EABI__
	stmfd	sp!, {r4}
	mov	ip, r0		/* Keep target address */
1:
	ldrexd	r0, [ip]	/* r0, r1 are used as destination */
	strexd	r4, r2, [ip]	/* Store new value */
	teq	r4, #0
	bne	1b		/* Retry when fails */
	ldmfd	sp!, {r4}
	mov	pc, lr
#else	/* !__ARM_EABI__ */
	stmfd	sp!, {r4-r6}

	mov	r3, r0

	/* Check whether target address is double word aligned */
	tst	r0, #CLONGLONGMASK
	bne	10f

	/* Address is double word aligned. We can use ldrexd/strexd. */

	/*
	 * Move new value to r0, r1 because strexd requires even
	 * register number.
	 */
	mov	r0, r1
	mov	r1, r2
1:
	ldrexd	r4, [r3]	/* r4, r5 are used as destination */
	strexd	r2, r0, [r3]	/* Store new value */
	teq	r2, #0
	bne	1b		/* Retry when fails */
	mov	r0, r4		/* Return old value */
	mov	r1, r5
	b	100f
10:
	/*
	 * We can't use ldrexd/strexd because of bad address alignment.
	 * So we need to acquire global lock.
	 */

	/* Lock address is preserved in r6. */
	LOCK_ATOMIC64(atomic_swap_64, r6, r4, r5)

	ldmia	r3, {r4-r5}	/* Load value into r4, r5 */
	stmia	r3, {r1-r2}	/* Store new value */
	mov	r0, r4		/* Return old value */
	mov	r1, r5

	UNLOCK_ATOMIC64(r6, r4, r5)
100:
	ldmfd	sp!, {r4-r6}
	mov	pc, lr
#endif	/* __ARM_EABI__ */
	SET_SIZE(atomic_swap_64)

ENTRY(atomic_set_long_excl)
	mov	r2, #1
	mov	r3, r2, asl r1	/* r3 = (1 << r1) */
1:
	ldrex	r1, [r0]	/* Load target */
	tst	r1, r3
	mvnne	r0, #0
	bne	10f		/* Return -1 if the specified bit is set */
	orr	r1, r1, r3
	strex	r2, r1, [r0]	/* Set the specified bit */
	teq	r2, #0
	bne	1b		/* Retry when fails */
	mov	r0, #0		/* Return 0 on successful completion */
10:
	mov	pc, lr
	SET_SIZE(atomic_set_long_excl)

ENTRY(atomic_clear_long_excl)
	mov	r2, #1
	mov	r3, r2, asl r1	/* r3 = (1 << r1) */
1:
	ldrex	r1, [r0]	/* Load target */
	tst	r1, r3
	mvneq	r0, #0
	beq	10f		/* Return -1 if the specified bit is not set */
	mvn	r2, r3
	and	r1, r1, r2	
	strex	r2, r1, [r0]	/* Clear the specified bit */
	teq	r2, #0
	bne	1b		/* Retry when fails */
	mov	r0, #0		/* Return 0 on successful completion */
10:
	mov	pc, lr
	SET_SIZE(atomic_clear_long_excl)

#ifndef	_KERNEL

ENTRY(membar_enter)
ALTENTRY(membar_exit)
ALTENTRY(membar_producer)
ALTENTRY(membar_consumer)
	mov	r0, #0
	mcr	p15, 0, r0, c7, c10, 5
	mov	pc, lr
	SET_SIZE(membar_consumer)
	SET_SIZE(membar_producer)
	SET_SIZE(membar_exit)
	SET_SIZE(membar_enter)

#endif	/* !_KERNEL */
