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
 * Copyright (c) 2006-2008 NEC Corporation
 * All rights reserved.
 */

	.ident	"@(#)copy.s"
	.file	"copy.s"

#include <sys/errno.h>
#include <sys/feature_tests.h>
#include <sys/asm_linkage.h>
#include <sys/machparam.h>
#include <sys/stack.h>
#include <asm/thread.h>
#include <asm/cpufunc.h>
#include "assym.h"

/*
 * Remarks:
 * On ARM architecture, XXX_nta() functions are implemented as alias.
 */

/*
 * void
 * bzero(void *addr, size_t count)
 *	Block zero routine.
 *
 * Remarks:
 *	This routine should not use stack. If uses, you must change kzero().
 *
 * REVISIT:
 *	This routine should be more optimized.
 */
ENTRY(bzero)
	teq	r1, #0			/* if (count == 0) */
	moveq	pc, lr			/*   return */
	add	r2, r0, r1		/* r2 = addr + count */
	mov	r3, #0			/* r3 = 0 */

	/* Check alignment. */
	tst	r0, #CLONGMASK
	bne	10f
	tst	r1, #CLONGMASK
	bne	10f

	/* This is a common case. Zero words. */
1:
	str	r3, [r0], #CLONGSIZE	/* *(uint_t *)addr = 0, addr += 4 */
	cmp	r0, r2			/* if (addr < r2) */
	blo	1b			/*   goto 1b */
	mov	pc, lr

10:
	/* Do simple byte zero. */
	strb	r3, [r0], #1		/* *addr = 0, addr++ */
	cmp	r0, r2			/* if (addr < r2) */
	blo	10b			/*   goto 1b */
	mov	pc, lr
	SET_SIZE(bzero)

/*
 * void
 * bcopy(const void *from, void *to, size_t count)
 *	Copy a block of storage.
 *
 * Remarks:
 *	This routine should not use stack. If uses, you must change kcopy().
 *
 * REVISIT:
 *	This routine should be more optimized.
 */
ENTRY(bcopy)
	teq	r2, #0			/* if (count == 0) */
	moveq	pc, lr			/*   return */
	add	r2, r0, r2		/* r2 = from + count */

	/* Check alignment. */
	tst	r0, #CLONGMASK
	bne	10f
	tst	r1, #CLONGMASK
	bne	10f
	tst	r2, #CLONGMASK
	bne	10f

	/* This is a common case. Do word copy. */
1:	
	ldr	r3, [r0], #CLONGSIZE	/* r3 = *(uint_t *)from, from += 4 */
	str	r3, [r1], #CLONGSIZE	/* *(uint_t *)to = r3, to += 4 */
	cmp	r0, r2			/* if (from < r2) */
	blo	1b			/*   goto 1b */
	mov	pc, lr

10:
	/* Do simple byte copy. */
	ldrb	r3, [r0], #1		/* r3 = *from, from++ */
	strb	r3, [r1], #1		/* *to = r3, to++ */
	cmp	r0, r2			/* if (from < r2) */
	blo	10b			/*   goto 10b */
	mov	pc, lr
	SET_SIZE(bcopy)

/*
 * void
 * ovbcopy(const void *src, void *dst, size_t count)
 *	Overlapping bcopy (source and target may overlap arbitrarily).
 */
ENTRY(ovbcopy)
	subs	r3, r0, r1		/* if ((src-dst) < 0) */
	rsblt	r3, r3, #0		/*   diff = -diff */
	cmp	r0, r1			/* if ((src <= dst) */
	cmpcc	r3, r2			/*   || (diff >= count)) */
	bcs	10f			/*     go to 10f */
1:
	subs	r2, r2, #1		/* count-- */
	ldrb	r3, [r2, r0]		/* r3 = *(src+count) */
	strb	r3, [r2, r1]		/* *(dst+count) = r3 */
	bxeq	lr			/* if (count == 0) return */
	b	1b			/* go to 1b */
10:
	cmp	r0, r1			/* if (src == dst) */
	bxeq	lr			/*   return */
	cmp	r2, #0			/* if (count == 0) */
	bxeq	lr			/*   return */
	add	ip, r1, r2		/* ip = dst + count */
100:
	ldrb	r3, [r0], #1		/* r3 = *(src) */
	strb	r3, [r1], #1		/* *(dst) = r3 */
	cmp	r1, ip			/* if (!(dst == ip)) { */
	bne	100b			/*   go to 100b */
	bx	lr			/* } else { return } */
	SET_SIZE(ovbcopy)

/*
 * int
 * kcopy(const void *from, void *to, size_t count)
 *	Copy a block of storage, returning an error code if `from' or
 *	`to' takes a kernel pagefault which cannot be resolved.
 *
 * Calling/Exit State:
 *	Returns errno value on pagefault error, 0 if all ok
 *
 * Remarks:
 *	We don't adjust stack alignment even if STACK_ENTRY_ALIGN is 8 bytes
 *	because we know bcopy() is a leaf function, and it never touches stack.
 */
ENTRY(kcopy)
ALTENTRY(kcopy_nta)
	stmfd	sp!, {r4-r5, lr}
	mov	r3, #0
	SYNC_BARRIER(r3)		/* Memory synchronization barrier */
	THREADP(r4)			/* r4 = curthread() */
	ldr	r5, [r4, #T_LOFAULT]	/* Save existing handler */
	adr	r3, .Lkcopy_err
	str	r3, [r4, #T_LOFAULT]	/* Put .Lkcopy_err in t_lofault */

	bl	bcopy			/* Do copy. */

	str	r5, [r4, #T_LOFAULT]	/* Restore lofault handler */
	mov	r0, #0			/* Return 0 */
	ldmfd	sp!, {r4-r5, pc}

.Lkcopy_err:
	/*
	 * Remarks:
	 * If you change bcopy() code to use stack in it, you must restore
	 * registers here.
	 */
	str	r5, [r4, #T_LOFAULT]	/* Restore lofault handler */
	mov	r0, #EFAULT
	ldmfd	sp!, {r4-r5, pc}
	SET_SIZE(kcopy_nta)
	SET_SIZE(kcopy)


/*
 * int
 * kzero(void *addr, size_t count)
 *	Zero a block of storage, returning an error code if we take a kernel
 *	pagefault which cannot be resolved.
 *
 * Calling/Exit State:
 *	Returns errno value on pagefault error, 0 if all ok
 *
 * Remarks:
 *	We don't adjust stack alignment even if STACK_ENTRY_ALIGN is 8 bytes
 *	because we know bzero() is a leaf function, and it never touches stack.
 */
ENTRY(kzero)
	stmfd	sp!, {r4-r5, lr}
	mov	r3, #0
	SYNC_BARRIER(r3)		/* Memory synchronization barrier */
	THREADP(r4)			/* r4 = curthread() */
	ldr	r5, [r4, #T_LOFAULT]	/* Save existing handler */
	adr	r3, .Lkzero_err
	str	r3, [r4, #T_LOFAULT]	/* Put .Lkzero_err in t_lofault */

	bl	bzero			/* Zero out bytes. */

	str	r5, [r4, #T_LOFAULT]	/* Restore lofault handler */
	mov	r0, #0			/* Return 0 */
	ldmfd	sp!, {r4-r5, pc}

.Lkzero_err:
	/*
	 * Remarks:
	 * If you change bzero() code to use stack in it, you must restore
	 * registers here.
	 */
	str	r5, [r4, #T_LOFAULT]	/* Restore lofault handler */
	mov	r0, #EFAULT
	ldmfd	sp!, {r4-r5, pc}
	SET_SIZE(kzero)

/*
 * Transfer data to and from user space -
 * Note that these routines can cause faults
 * It is assumed that the kernel has nothing at
 * less than KERNELBASE in the virtual address space.
 *
 * Note that copyin(9F) and copyout(9F) are part of the
 * DDI/DKI which specifies that they return '-1' on "errors."
 *
 * Sigh.
 *
 * So there's two extremely similar routines - xcopyin_nta() and
 * xcopyout_nta() which return the errno that we've faithfully computed.
 * This allows other callers (e.g. uiomove(9F)) to work correctly.
 * Given that these are used pretty heavily, we expand the calling
 * sequences inline for all flavours (rather than making wrappers).
 */

/*
 * int
 * copyin(const void *uaddr, void *kaddr, size_t count)
 *	Copy user data to kernel space.
 */
ENTRY(copyin)
	stmfd	sp!, {r0-r2,r4-r5}
	adr	r3, .Lcopyin_err	/* r3 = .Lcopyin_err */

.Ldo_copyin:
	/*
	 * Common copyin routine. The caller must:
	 *  - do "stmfd  sp!, {r0-r2,r4-r5}".
	 *    We must save r0-r2 to call copyops handler.
	 *  - set error handler to r3.
	 *
	 * REVISIT:
	 *	This routine should be more optimized.
	 */
	mov	r4, #0
	SYNC_BARRIER(r4)		/* Memory synchronization barrier */
	THREADP(r4)			/* r4 = curthread() */
	ldr	r5, [r4, #T_LOFAULT]	/* Save existing handler */
	str	r3, [r4, #T_LOFAULT]	/* Put error handler in t_lofault */

	teq	r2, #0			/* if (count == 0) */
	beq	.Lcopyin_done		/*   goto .Lcopyin_done */
	add	r2, r0, r2		/* r2 = uaddr + count */

	/* Check alignment. */
	tst	r0, #CLONGMASK
	bne	10f
	tst	r1, #CLONGMASK
	bne	10f
	tst	r2, #CLONGMASK
	bne	10f

	/* This is a common case. Do word copy. */
1:
	ldrt	r3, [r0], #CLONGSIZE	/* r3 = *(uint_t *)uaddr,uaddr += 4 */
	str	r3, [r1], #CLONGSIZE	/* *(uint_t *)kaddr = r3, kaddr += 4 */
	cmp	r0, r2			/* if (uaddr < r2) */
	blo	1b			/*   goto 1b */
	b	.Lcopyin_done

10:
	/* Do simple byte copy. */
	ldrbt	r3, [r0], #1		/* r3 = *uaddr, uaddr++ */
	strb	r3, [r1], #1		/* *kaddr = r3, kaddr++ */
	cmp	r0, r2			/* if (uaddr < r2) */
	blo	10b			/*   goto 10b */

.Lcopyin_done:
	str	r5, [r4, #T_LOFAULT]	/* Restore lofault handler */
	ldmfd	sp!, {r0-r2,r4-r5}
	mov	r0, #0
	mov	pc, lr			/* return 0 */

.Lcopyin_err:
	str	r5, [r4, #T_LOFAULT]	/* Restore lofault handler */
	mov	r3, r4
	ldmfd	sp!, {r0-r2,r4-r5}
	ldr	r3, [r3, #T_COPYOPS]
	teq	r3, #0			/* if (curthread->t_copyops == NULL) */
	beq	20f			/*   goto 20f */
	ldr	pc, [r3, #CP_COPYIN]	/* Jump to copyin handler. */
20:
	mvn	r0, #0
	mov	pc, lr			/* return -1 */
	SET_SIZE(copyin)

/*
 * int
 * xcopyin(const void *uaddr, void *kaddr, size_t count)
 *	Same as copyin(), but it returns errno on error.
 */
ENTRY(xcopyin)
ALTENTRY(xcopyin_nta)
	stmfd	sp!, {r0-r2,r4-r5}
	adr	r3, .Lxcopyin_err	/* r3 = .Lxcopyin_err */

	/* Let .Ldo_copyin do the rest. */
	b	.Ldo_copyin

.Lxcopyin_err:
	str	r5, [r4, #T_LOFAULT]	/* Restore lofault handler */
	mov	r3, r4
	ldmfd	sp!, {r0-r2,r4-r5}
	ldr	r3, [r3, #T_COPYOPS]
	teq	r3, #0			/* if (curthread->t_copyops == NULL) */
	beq	20f			/*   goto 20f */
	ldr	pc, [r3, #CP_XCOPYIN]	/* Jump to xcopyin handler */
20:
	mov	r0, #EFAULT
	mov	pc, lr			/* return EFAULT */
	SET_SIZE(xcopyin_nta)
	SET_SIZE(xcopyin)

/*
 * int
 * copyout(const void *kaddr, void *uaddr, size_t count)
 *	Copy kernel data to user space.
 */
ENTRY(copyout)
	stmfd	sp!, {r0-r2,r4-r5}
	adr	r3, .Lcopyout_err	/* r3 = .Lcopyout_err */

.Ldo_copyout:
	/*
	 * Common copyout routine. The caller must:
	 *  - do "stmfd  sp!, {r0-r2,r4-r5}".
	 *    We must save r0-r2 to call copyops handler.
	 *  - set error handler to r3.
	 *
	 * REVISIT:
	 *	This routine should be more optimized.
	 */
	mov	r4, #0
	SYNC_BARRIER(r4)		/* Memory synchronization barrier */
	THREADP(r4)			/* r4 = curthread() */
	ldr	r5, [r4, #T_LOFAULT]	/* Save existing handler */
	str	r3, [r4, #T_LOFAULT]	/* Put error handler in t_lofault */

	teq	r2, #0			/* if (count == 0) */
	beq	.Lcopyout_done		/*   goto .Lcopyout_done */
	add	r2, r0, r2		/* r2 = kaddr + count */

	/* Check alignment. */
	tst	r0, #CLONGMASK
	bne	10f
	tst	r1, #CLONGMASK
	bne	10f
	tst	r2, #CLONGMASK
	bne	10f

	/* This is a common case. Do word copy. */
1:
	ldr	r3, [r0], #CLONGSIZE	/* r3 = *(uint_t *)kaddr,kaddr += 4 */
	strt	r3, [r1], #CLONGSIZE	/* *(uint_t *)uaddr = r3, uaddr += 4 */
	cmp	r0, r2			/* if (kaddr < r2) */
	blo	1b			/*   goto 1b */
	b	.Lcopyout_done

10:
	/* Do simple byte copy. */
	ldrb	r3, [r0], #1		/* r3 = *kaddr, kaddr++ */
	strbt	r3, [r1], #1		/* *uaddr = r3, uaddr++ */
	cmp	r0, r2			/* if (kaddr < r2) */
	blo	10b			/*   goto 10b */

.Lcopyout_done:
	str	r5, [r4, #T_LOFAULT]	/* Restore lofault handler */
	ldmfd	sp!, {r0-r2,r4-r5}
	mov	r0, #0
	mov	pc, lr			/* return 0 */

.Lcopyout_err:
	str	r5, [r4, #T_LOFAULT]	/* Restore lofault handler */
	mov	r3, r4
	ldmfd	sp!, {r0-r2,r4-r5}
	ldr	r3, [r3, #T_COPYOPS]
	teq	r3, #0			/* if (curthread->t_copyops == NULL) */
	beq	20f			/*   goto 20f */
	ldr	pc, [r3, #CP_COPYOUT]	/* Jump to copyout handler. */
20:
	mvn	r0, #0
	mov	pc, lr			/* return -1 */
	SET_SIZE(copyout)

/*
 * int
 * xcopyout(const void *kaddr, void *uaddr, size_t count)
 *	Same as copyout(), but it returns errno on error.
 */
ENTRY(xcopyout)
ALTENTRY(xcopyout_nta)
	stmfd	sp!, {r0-r2,r4-r5}
	adr	r3, .Lxcopyout_err	/* r3 = .Lcopyout_err */

	/* Let .Ldo_copyout do the rest. */
	b	.Ldo_copyout

.Lxcopyout_err:
	str	r5, [r4, #T_LOFAULT]	/* Restore lofault handler */
	mov	r3, r4
	ldmfd	sp!, {r0-r2,r4-r5}
	ldr	r3, [r3, #T_COPYOPS]
	teq	r3, #0			/* if (curthread->t_copyops == NULL) */
	beq	20f			/*   goto 20f */
	ldr	pc, [r3, #CP_XCOPYOUT]	/* Jump to xcopyout handler. */
20:
	mov	r0, #EFAULT
	mov	pc, lr			/* return errno */
	SET_SIZE(xcopyout_nta)
	SET_SIZE(xcopyout)

#define	COPYINOUT_NOERR(LDWORD, STWORD, LDBYTE, STBYTE)			\
	add	r2, r0, r2;		/* r2 = from + to */		\
	/* Check alignment. */						\
	tst	r0, #CLONGMASK;						\
	bne	10f;							\
	tst	r1, #CLONGMASK;						\
	bne	10f;							\
	tst	r2, #CLONGMASK;						\
	bne	10f;							\
									\
	/* This is a common case. Do word copy. */			\
1:									\
	LDWORD	r3, [r0], #CLONGSIZE;	/* r3 = *(uint_t *)from,from += 4 */ \
	STWORD	r3, [r1], #CLONGSIZE;	/* *(uint_t *)to = r3, to += 4 */ \
	cmp	r0, r2;			/* if (from < r2) */		\
	blo	1b;			/*   goto 1b */			\
	mov	pc, lr;							\
									\
10:									\
	/* Do simple byte copy. */					\
	LDBYTE	r3, [r0], #1;		/* r3 = *from, from++ */	\
	STBYTE	r3, [r1], #1;		/* *to = r3, to++ */		\
	cmp	r0, r2;			/* if (from < r2) */		\
	blo	10b;			/*   goto 10b */		\
	mov	pc, lr

/*
 * void
 * copyin_noerr(const void *uaddr, void *kaddr, size_t count)
 *	Copy a block of storage - must not overlap
 *	No fault handler will be installed, so this function should be
 *	called under on_fault().
 */
ENTRY(copyin_noerr)
	COPYINOUT_NOERR(ldrt, str, ldrbt, strb)
	SET_SIZE(copyin_noerr)

/*
 * void
 * copyout_noerr(const void *kaddr, void *uaddr, size_t count)
 *	Copy a block of storage - must not overlap
 *	No fault handler will be installed, so this function should be
 *	called under on_fault().
 */
ENTRY(copyout_noerr)
	COPYINOUT_NOERR(ldr, strt, ldrb, strbt)
	SET_SIZE(copyout_noerr)

/*
 * int
 * copystr(const char *from, char *to, size_t maxlength, size_t *lencopied)
 *	Copy a null terminated string from one point to another in
 *	the kernel address space.
 */
ENTRY(copystr)
	stmfd	sp!, {r4-r7}
	mov	r4, #0
	mov	r7, r0			/* r7 = orgfrom = from */
	SYNC_BARRIER(r4)		/* Memory synchronization barrier */
	THREADP(r4)			/* r4 = curthread() */
	ldr	r5, [r4, #T_LOFAULT]	/* Save existing handler */
	adr	r6, .Lcopystr_err
	str	r6, [r4, #T_LOFAULT]	/* Put .Lcopystr_err in t_lofault */

	/* Return value should be set to r1. */
	cmp	r2, #0			/* if (maxlength < 0) */
	bgt	0f
	movlt	r1, #ENAMETOOLONG	/*   ret = ENAMETOOLONG */
	blt	100f
	moveq	r1, #0			/* if (maxlength == 0) */
	beq	100f			/*   ret = 0 */
0:
	add	r4, r0, r2		/* r4 = end = from + maxlength */
1:
	ldrb	r6, [r0], #1		/* r6 = *from, from++ */
	strb	r6, [r1], #1		/* *to = r6, to++ */
	teq	r6, #0			/* if (r6 == 0) */
	beq	2f			/*   break */
	cmp	r0, r4			/* if (from < end) */
	blo	1b			/*   loop */
	mov	r1, #ENAMETOOLONG	/* ret = ENAMETOOLONG */
	b	100f
2:
	mov	r1, #0			/* ret = 0 */
100:
	THREADP(r4)			/* r4 = curthread() */
	str	r5, [r4, #T_LOFAULT]	/* Restore lofault handler */
	teq	r3, #0			/* if (lencopied != NULL) */
	subne	r7, r0, r7
	strne	r7, [r3]		/*   *lencopied = orgfrom - from */
	mov	r0, r1			/* Set return value */
	ldmfd	sp!, {r4-r7}
	mov	pc, lr
.Lcopystr_err:
	mov	r1, #EFAULT
	b	100b
	SET_SIZE(copystr)

#define	COPYINOUTSTR(LOAD, STORE, COPYOP)				\
	/* r0-r3 must be saved to call copyops handler. */		\
	stmfd	sp!, {r0-r3};						\
	stmfd	sp!, {r4-r7};						\
	/* Return value should be set to r1. */				\
	mov	r4, #0;							\
	mov	r7, r0;			/* r7 = orgfrom = from */	\
	SYNC_BARRIER(r4);		/* Memory synchronization barrier */ \
	THREADP(r4);			/* r4 = curthread() */		\
	ldr	r5, [r4, #T_LOFAULT];	/* Save existing handler */	\
	adr	r6, 50f;						\
	str	r6, [r4, #T_LOFAULT];	/* Put 50f in t_lofault */	\
									\
	cmp	r2, #0;			/* if (maxlength < 0) */	\
	bgt	0f;							\
	movlt	r1, #ENAMETOOLONG;	/*   ret = ENAMETOOLONG */	\
	blt	100f;							\
	moveq	r1, #0;			/* if (maxlength == 0) */	\
	beq	100f;			/*   ret = 0 */			\
0:									\
	add	r4, r0, r2;		/* r4 = end = from + maxlength */ \
1:									\
	LOAD	r6, [r0], #1;		/* r6 = *from, from++ */	\
	STORE	r6, [r1], #1;		/* *to = r6, to++ */		\
	teq	r6, #0;			/* if (r6 == 0) */		\
	beq	2f;			/*   break */			\
	cmp	r0, r4;			/* if (uaddr < end) */		\
	blo	1b;			/*   loop */			\
	mov	r1, #ENAMETOOLONG;	/* ret = ENAMETOOLONG */	\
	b	100f;							\
2:									\
	mov	r1, #0;			/* ret = 0 */			\
100:									\
	THREADP(r4);			/* r4 = curthread() */		\
	str	r5, [r4, #T_LOFAULT];	/* Restore lofault handler */	\
	teq	r3, #0;			/* if (lencopied != NULL) */	\
	subne	r7, r0, r7;						\
	strne	r7, [r3];		/*   *lencopied = orgfrom - from */ \
	ldmfd	sp!, {r4-r7};						\
	add	sp, sp, #(4 * 4);					\
	mov	r0, r1;			/* Set return value */		\
	mov	pc, lr;							\
50:									\
	THREADP(r4);			/* r4 = curthread() */		\
	str	r5, [r4, #T_LOFAULT];	/* Restore lofault handler */	\
	ldr	r5, [r4, #T_COPYOPS];					\
	teq	r5, #0;		/* if (curthread->t_copyops == NULL) */	\
	beq	20f;		/*   goto 20f */			\
	mov	ip, r5;							\
	ldmfd	sp!, {r4-r7};		/* Restore r4-r7 */		\
	ldmfd	sp!, {r0-r3};		/* Restore arguments */		\
	ldr	pc, [ip, #(COPYOP)];	/* Jump to copyops handler */	\
20:									\
	mov	r0, #EFAULT;		/* return EFAULT */		\
	ldmfd	sp!, {r4-r7};						\
	add	sp, sp, #(4 * 4);					\
	mov	pc, lr

/*
 * int
 * copyinstr(const char *uaddr, char *kaddr, size_t maxlength,
 *	     size_t *lencopied)
 *	Copy a null terminated string from the user address space into
 *	the kernel address space.
 */
ENTRY(copyinstr)
	COPYINOUTSTR(ldrbt, strb, CP_COPYINSTR)
	SET_SIZE(copyinstr)

/*
 * int
 * copyoutstr(const char *kaddr, char *uaddr, size_t maxlength,
 *	      size_t *lencopied)
 *	Copy a null terminated string from the kernel
 *	address space to the user address space.
 */
ENTRY(copyoutstr)
	COPYINOUTSTR(ldrb, strbt, CP_COPYOUTSTR)
	SET_SIZE(copyoutstr)

#define	COPYINOUTSTR_NOERR(LOAD, STORE)					\
	stmfd	sp!, {r4-r6};						\
	mov	r6, r0;			/* r6 = orgfrom = from */	\
	cmp	r2, #0;			/* if (maxlength < 0) */	\
	bgt	0f;							\
	movlt	r1, #ENAMETOOLONG;	/*   ret = ENAMETOOLONG */	\
	blt	100f;							\
	moveq	r1, #0;			/* if (maxlength == 0) */	\
	beq	100f;			/*   ret = 0 */			\
0:									\
	add	r4, r0, r2;		/* r4 = end = from + maxlength */ \
1:									\
	LOAD	r5, [r0], #1;		/* r5 = *from, from++ */	\
	STORE	r5, [r1], #1;		/* *to = r5, to++ */		\
	teq	r5, #0;			/* if (r5 == 0) */		\
	beq	2f;			/*   break */			\
	cmp	r0, r4;			/* if (uaddr < end) */		\
	blo	1b;			/*   loop */			\
	mov	r1, #ENAMETOOLONG;	/* ret = ENAMETOOLONG */	\
	b	100f;							\
2:									\
	mov	r1, #0;			/* ret = 0 */			\
100:									\
	teq	r3, #0;			/* if (lencopied != NULL) */	\
	subne	r6, r0, r6;						\
	strne	r6, [r3];		/*   *lencopied = orgfrom - from */ \
	mov	r0, r1;			/* Set return value */		\
	ldmfd	sp!, {r4-r6};						\
	mov	pc, lr
	
/*
 * void
 * copyinstr_noerr(const char *uaddr, char *kaddr, size_t maxlength,
 *		   size_t *lencopied)
 *	Copy a null terminated string from the user address space into
 *	the kernel address space.
 *	No fault handler will be installed, so this function should be
 *	called under on_fault().
 */
ENTRY(copyinstr_noerr)
	COPYINOUTSTR_NOERR(ldrbt, strb)
	SET_SIZE(copyinstr_noerr)

/*
 * void
 * copyoutstr_noerr(const char *kaddr, char *uaddr, size_t maxlength,
 *		    size_t *lencopied)
 *	Copy a null terminated string from the kernel address space to
 *	the user address space.
 *	No fault handler will be installed, so this function should be
 *	called under on_fault().
 */
ENTRY(copyoutstr_noerr)
	COPYINOUTSTR_NOERR(ldrb, strbt)
	SET_SIZE(copyoutstr_noerr)

/*
 * void
 * ucopystr(const char *ufrom, char *uto, size_t umaxlength, size_t *lencopied)
 *	Copy a null terminated string in the user address space.
 *	No fault handler will be installed, so this function should be
 *	called under on_fault().
 */
ENTRY(ucopystr)
	COPYINOUTSTR_NOERR(ldrbt, strbt)
	SET_SIZE(ucopystr)

/*
 * Since all of the fuword() variants are so similar,
 * we have a macro to spit them out.
 */
#define	FUWORD(NAME, LOAD, STORE, COPYOP)				\
ENTRY(NAME);								\
	stmfd	sp!, {r4};						\
	mov	r3, #0;							\
	SYNC_BARRIER(r3);		/* Memory synchronization barrier */ \
	THREADP(r3);			/* r3 = curthread() */		\
	ldr	r4, [r3, #T_LOFAULT];	/* Save existing handler */	\
	adr	r2, 100f;						\
	str	r2, [r3, #T_LOFAULT];	/* Put error handler in t_lofault */ \
	LOAD	r2, [r0];		/* Load value */		\
	str	r4, [r3, #T_LOFAULT];	/* Restore lofault handler */	\
	STORE	r2, [r1];		/* Store value */		\
	mov	r0, #0;							\
	ldmfd	sp!, {r4};						\
	mov	pc, lr;			/* return 0 */			\
100:									\
	str	r4, [r3, #T_LOFAULT];	/* Restore lofault handler */	\
	ldmfd	sp!, {r4};						\
	ldr	r2, [r3, #T_COPYOPS];					\
	teq	r2, #0;		/* if (curthread->t_copyops == NULL) */	\
	beq	1f;		/*   goto 1f */				\
	ldr	pc, [r2, #(COPYOP)];	/* Jump to copyops handler */	\
1:									\
	mvn	r0, #0;							\
	mov	pc, lr;			/* return -1 */			\
	SET_SIZE(NAME)

FUWORD(fuword32, ldrt, str, CP_FUWORD32)
FUWORD(fuword8, ldrbt, strb, CP_FUWORD8)

#undef FUWORD

/*
 * FUWORD() macro can't be used to define fuword16() because
 * MPCore doesn't support ldrht. ldrht is supported on V6T2 variant.
 */
ENTRY(fuword16)
	stmfd	sp!, {r4}
	mov	r3, #0
	SYNC_BARRIER(r3)		/* Memory synchronization barrier */
	THREADP(r3)			/* r3 = curthread() */
	ldr	r4, [r3, #T_LOFAULT]	/* Save existing handler */
	adr	r2, 100f
	str	r2, [r3, #T_LOFAULT]	/* Put error handler in t_lofault */

	/* Load & store halfword using byte access */
	ldrbt	r2, [r0], #1
	strb	r2, [r1], #1
	ldrbt	r2, [r0]
	strb	r2, [r1]
	str	r4, [r3, #T_LOFAULT]	/* Restore lofault handler */
	mov	r0, #0
	ldmfd	sp!, {r4}
	mov	pc, lr			/* return 0 */
100:
	str	r4, [r3, #T_LOFAULT]	/* Restore lofault handler */
	ldmfd	sp!, {r4}
	ldr	r2, [r3, #T_COPYOPS]
	teq	r2, #0			/* if (curthread->t_copyops == NULL) */
	beq	1f			/*   goto 1f */
	ldr	pc, [r2, #CP_FUWORD16]	/* Jump to copyops handler */
1:
	mvn	r0, #0
	mov	pc, lr			/* return -1 */
	SET_SIZE(fuword16)

/*
 * Since all of the suword() variants are so similar,
 * we have a macro to spit them out.
 */
#define	SUWORD(NAME, STORE, COPYOP)					\
ENTRY(NAME);								\
	stmfd	sp!, {r4};						\
	mov	r3, #0;							\
	SYNC_BARRIER(r3);		/* Memory synchronization barrier */ \
	THREADP(r3);			/* r3 = curthread() */		\
	ldr	r4, [r3, #T_LOFAULT];	/* Save existing handler */	\
	adr	r2, 100f;						\
	str	r2, [r3, #T_LOFAULT];	/* Put error handler in t_lofault */ \
	STORE	r1, [r0];		/* Store value */		\
	str	r4, [r3, #T_LOFAULT];	/* Restore lofault handler */	\
	mov	r0, #0;							\
	ldmfd	sp!, {r4};						\
	mov	pc, lr;			/* return 0 */			\
100:									\
	str	r4, [r3, #T_LOFAULT];	/* Restore lofault handler */	\
	ldmfd	sp!, {r4};						\
	ldr	r2, [r3, #T_COPYOPS];					\
	teq	r2, #0;		/* if (curthread->t_copyops == NULL) */	\
	beq	1f;		/*   goto 1f */				\
	ldr	pc, [r2, #(COPYOP)];	/* Jump to copyops handler */	\
1:									\
	mvn	r0, #0;							\
	mov	pc, lr;			/* return -1 */			\
	SET_SIZE(NAME)

SUWORD(suword32, strt, CP_SUWORD32)
SUWORD(suword8, strbt, CP_SUWORD8)

#undef SUWORD

/*
 * SUWORD() macro can't be used to define suword16() because
 * MPCore doesn't support sdrht. sdrht is supported on V6T2 variant.
 */
ENTRY(suword16)
	stmfd	sp!, {r4}
	mov	r3, #0
	SYNC_BARRIER(r3)		/* Memory synchronization barrier */
	THREADP(r3)			/* r3 = curthread() */
	ldr	r4, [r3, #T_LOFAULT]	/* Save existing handler */
	adr	r2, 100f
	str	r2, [r3, #T_LOFAULT]	/* Put error handler in t_lofault */

	/* Store halfword using byteaccess */
#if	defined(_LITTLE_ENDIAN)
	strbt	r1, [r0], #1		/* *addr = (value & 0xff), addr++ */
	mov	r1, r1, lsr #8
	strbt	r1, [r0]		/* *addr = (value >> 8) & 0xff */
#elif defined(_BIG_ENDIAN)
	mov	r2, r1, lsr #8
	strbt	r2, [r0], #1	/* *addr = (value >> 8) & 0xff, addr++ */
	strbt	r1, [r0]	/* *addr = (value & 0xff) */
#else	/* !_LITTLE_ENDIAN && !_BIG_ENDIAN */
#error	"Endianess is not defined."
#endif	/* defined(_LITTLE_ENDIAN) */
	str	r4, [r3, #T_LOFAULT]	/* Restore lofault handler */
	mov	r0, #0
	ldmfd	sp!, {r4}
	mov	pc, lr			/* return 0 */
100:
	str	r4, [r3, #T_LOFAULT]	/* Restore lofault handler */
	ldmfd	sp!, {r4}
	ldr	r2, [r3, #T_COPYOPS]
	teq	r2, #0			/* if (curthread->t_copyops == NULL) */
	beq	1f			/*   goto 1f */
	ldr	pc, [r2, #CP_SUWORD16]	/* Jump to copyops handler */
1:
	mvn	r0, #0
	mov	pc, lr			/* return -1 */
	SET_SIZE(suword16)

#define	FUWORD_NOERR(NAME, LOAD, STORE)					\
ENTRY(NAME);								\
	LOAD	r2, [r0];		/* Load value */		\
	STORE	r2, [r1];		/* Store value */		\
	mov	pc, lr;			/* return */			\
	SET_SIZE(NAME)

FUWORD_NOERR(fuword32_noerr, ldrt, str)
FUWORD_NOERR(fuword8_noerr, ldrbt, strb)

#undef	FUWORD_NOERR

ENTRY(fuword16_noerr)
	ldrbt	r2, [r0], #1
	strb	r2, [r1], #1
	ldrbt	r2, [r0]
	strb	r2, [r1]
	mov	pc, lr			/* return */
	SET_SIZE(fuword16_noerr)

#define	SUWORD_NOERR(NAME, STORE)					\
ENTRY(NAME);								\
	STORE	r1, [r0];		/* Store value */		\
	mov	pc, lr;			/* return */			\
	SET_SIZE(NAME)

SUWORD_NOERR(suword32_noerr, strt)
SUWORD_NOERR(suword8_noerr, strbt)

#undef	SUWORD_NOERR

ENTRY(suword16_noerr)
#ifdef	_LITTLE_ENDIAN
	strbt	r1, [r0], #1		/* *addr = (value & 0xff), addr++ */
	mov	r1, r1, lsr #8
	strbt	r1, [r0]		/* *addr = (value >> 8) & 0xff */
#else	/* !_LITTLE_ENDIAN */
	mov	r2, r1, lsr #8
	strbt	r2, [r0], #1	/* *addr = (value >> 8) & 0xff, addr++ */
	strbt	r1, [r0]	/* *addr = (value & 0xff) */
#endif	/* _LITTLE_ENDIAN */
	mov	pc, lr			/* return */
	SET_SIZE(suword16_noerr)

ANSI_PRAGMA_WEAK2(subyte, suword8, function)
ANSI_PRAGMA_WEAK2(subyte_noerr, suword8_noerr, function)
ANSI_PRAGMA_WEAK2(fulword, fuword32, function)
ANSI_PRAGMA_WEAK2(fulword_noerr, fuword32_noerr, function)
ANSI_PRAGMA_WEAK2(sulword, suword32, function)
ANSI_PRAGMA_WEAK2(sulword_noerr, suword32_noerr, function)

/*
 * void
 * uzero(void *addr, size_t count)
 *	Zero a block of storage in user space.
 *	No fault handler will be installed, so this function should be
 *	called under on_fault().
 *
 * REVISIT:
 *	This routine should be more optimized.
 */
ENTRY(uzero)
	teq	r1, #0			/* if (count == 0) */
	moveq	pc, lr			/*   return */
	add	r2, r0, r1		/* r2 = addr + count */
	mov	r3, #0			/* r3 = 0 */

	/* Check alignment. */
	tst	r0, #CLONGMASK
	bne	10f
	tst	r1, #CLONGMASK
	bne	10f

	/* This is a common case. Zero words. */
1:
	strt	r3, [r0], #CLONGSIZE	/* *(uint_t *)addr = 0, addr += 4 */
	cmp	r0, r2			/* if (addr < r2) */
	blo	1b			/*   goto 1b */
	mov	pc, lr

10:
	/* Do simple byte zero. */
	strbt	r3, [r0], #1		/* *addr = 0, addr++ */
	cmp	r0, r2			/* if (addr < r2) */
	blo	10b			/*   goto 1b */
	mov	pc, lr
	SET_SIZE(uzero)

/*
 * void
 * ucopy(const void *ufrom, void *uto, size_t ulength)
 *	Copy a block of storage in user space.
 *	No fault handler will be installed, so this function should be
 *	called under on_fault().
 *
 * REVISIT:
 *	This routine should be more optimized.
 */
ENTRY(ucopy)
	teq	r2, #0			/* if (count == 0) */
	moveq	pc, lr			/*   return */
	add	r2, r0, r2		/* r2 = from + count */

	/* Check alignment. */
	tst	r0, #CLONGMASK
	bne	10f
	tst	r1, #CLONGMASK
	bne	10f
	tst	r2, #CLONGMASK
	bne	10f

	/* This is a common case. Do word copy. */
1:	
	ldrt	r3, [r0], #CLONGSIZE	/* r3 = *(uint_t *)from, from += 4 */
	strt	r3, [r1], #CLONGSIZE	/* *(uint_t *)to = r3, to += 4 */
	cmp	r0, r2			/* if (from < r2) */
	blo	1b			/*   goto 1b */
	mov	pc, lr

10:
	/* Do simple byte copy. */
	ldrbt	r3, [r0], #1		/* r3 = *from, from++ */
	strbt	r3, [r1], #1		/* *to = r3, to++ */
	cmp	r0, r2			/* if (from < r2) */
	blo	10b			/*   goto 10b */
	mov	pc, lr
	SET_SIZE(ucopy)

/*
 * void
 * fast_bcopy(const void *from, void *to, size_t size)
 *	Fast version of bcopy().
 *
 * Calling/Exit State:
 *	- Source and destination addresses must be aligned in 4 bytes boundary.
 *	- Size must not be zero, and be a multiple of 512 bytes.
 */

#define	COPY_BLOCK32()							\
	ldmia	r0!, {r3-r9, lr};					\
	stmia	r1!, {r3-r9, lr}

#define	COPY_BLOCK128()							\
	COPY_BLOCK32();		/* 0  - 31  */				\
	COPY_BLOCK32();		/* 32 - 63  */				\
	COPY_BLOCK32();		/* 64 - 95  */				\
	COPY_BLOCK32()		/* 96 - 127 */

#define	COPY_BLOCK512()							\
	COPY_BLOCK128();	/* 0   - 127 */				\
	COPY_BLOCK128();	/* 128 - 255 */				\
	COPY_BLOCK128();	/* 256 - 383 */				\
	COPY_BLOCK128();	/* 384 - 511 */
	.globl	panic
ENTRY(fast_bcopy)
	stmfd	sp!, {r4-r9, lr}
#ifdef	DEBUG
	/* Check address alignment. */
	tst	r0, #CLONGMASK
	bne	.Lfbc_bad_from
	tst	r1, #CLONGMASK
	bne	.Lfbc_bad_to

	/* Check size. */
	teq	r2, #0
	beq	.Lfbc_bad_size
	mov	r3, #FB_BLOCKSIZE
	sub	r3, r3, #1
	tst	r2, r3
	bne	.Lfbc_bad_size
#endif	/* DEBUG */

	/* Copy 512 bytes block. */
	add	r2, r0, r2
1:
	COPY_BLOCK512()
	cmp	r0, r2
	blo	1b

	ldmfd	sp!, {r4-r9, pc}

#ifdef	DEBUG
.Lfbc_bad_from:
	mov	r1, r0
	adr	r0, .Lfbc_bad_from_msg
#if	STACK_ENTRY_ALIGN == 8
	sub	sp, sp, #4
#endif	/* STACK_ENTRY_ALIGN == 8 */
	bl	panic
	/* NOTREACHED */
.Lfbc_bad_to:
	adr	r0, .Lfbc_bad_to_msg
#if	STACK_ENTRY_ALIGN == 8
	sub	sp, sp, #4
#endif	/* STACK_ENTRY_ALIGN == 8 */
	bl	panic
	/* NOTREACHED */
.Lfbc_bad_size:
	mov	r1, r2
	adr	r0, .Lfbc_bad_size_msg
#if	STACK_ENTRY_ALIGN == 8
	sub	sp, sp, #4
#endif	/* STACK_ENTRY_ALIGN == 8 */
	bl	panic
	/* NOTREACHED */
.Lfbc_bad_from_msg:
	.asciz	"fast_bcopy: Bad address alignment: from = 0x%p"
.Lfbc_bad_to_msg:
	.asciz	"fast_bcopy: Bad address alignment: to = 0x%p"
.Lfbc_bad_size_msg:
	.asciz	"fast_bcopy: Bad size: size = 0x%x"
#endif	/* DEBUG */
	SET_SIZE(fast_bcopy)

/*
 * void
 * fast_bzero(void *addr, size_t size)
 *	Fast version of bzero().
 *
 * Calling/Exit State:
 *	- Source and destination addresses must be aligned in 4 bytes boundary.
 *	- Size must not be zero, and be a multiple of 512 bytes.
 */

#define	ZERO_BLOCK32()							\
	stmia	r0!, {r2-r8, lr}

#define	ZERO_BLOCK128()							\
	ZERO_BLOCK32();		/* 0  - 31  */				\
	ZERO_BLOCK32();		/* 32 - 63  */				\
	ZERO_BLOCK32();		/* 64 - 95  */				\
	ZERO_BLOCK32()		/* 96 - 127 */

#define	ZERO_BLOCK512()							\
	ZERO_BLOCK128();	/* 0   - 127 */				\
	ZERO_BLOCK128();	/* 128 - 255 */				\
	ZERO_BLOCK128();	/* 256 - 383 */				\
	ZERO_BLOCK128();	/* 384 - 511 */
	.globl	panic
ENTRY(fast_bzero)
	stmfd	sp!, {r4-r8, lr}
#ifdef	DEBUG
	/* Check address alignment. */
	tst	r0, #CLONGMASK
	bne	.Lfbz_bad_addr

	/* Check size. */
	teq	r1, #0
	beq	.Lfbz_bad_size
	mov	r2, #FB_BLOCKSIZE
	sub	r2, r2, #1
	tst	r1, r2
	bne	.Lfbz_bad_size
#endif	/* DEBUG */

	/* Zero 512 bytes block. */
	mov	r2, #0
	mov	r3, #0
	mov	r4, #0
	mov	r5, #0
	mov	r6, #0
	mov	r7, #0
	mov	r8, #0
	mov	lr, #0
	add	r1, r0, r1
1:
	ZERO_BLOCK512()
	cmp	r0, r1
	blo	1b

	ldmfd	sp!, {r4-r8, pc}

#ifdef	DEBUG
.Lfbz_bad_addr:
	mov	r1, r0
	adr	r0, .Lfbz_bad_addr_msg
	bl	panic
	/* NOTREACHED */
.Lfbz_bad_size:
	adr	r0, .Lfbz_bad_size_msg
	bl	panic
	/* NOTREACHED */
.Lfbz_bad_addr_msg:
	.asciz	"fast_bzero: Bad address alignment: addr = 0x%p"
.Lfbz_bad_size_msg:
	.asciz	"fast_bzero: Bad size: size = 0x%x"
#endif	/* DEBUG */
	SET_SIZE(fast_bzero)
