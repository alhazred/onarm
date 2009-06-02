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
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007-2008 NEC Corporation
 */

#ident	"@(#)dtrace_asm.s"

#include <sys/asm_linkage.h>
#include <asm/cpufunc.h>
#include <sys/regset.h>

#if defined(lint)
#include <sys/dtrace_impl.h>
#else
#include "assym.h"
#endif

#if defined(lint) || defined(__lint)

greg_t
dtrace_getsp(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(dtrace_getsp)
	mov	r0, sp
	mov	pc, lr
	SET_SIZE(dtrace_getsp)

#endif	/* lint */

#if defined(lint) || defined(__lint)

greg_t
dtrace_getpc(void)
{ return (0); }

#else	/* lint */

	ENTRY_NP(dtrace_getpc)
	mov	r0, lr
	mov	pc, lr
	SET_SIZE(dtrace_getpc)

#endif	/* lint */


#if defined(lint) || defined(__lint)

uint32_t
dtrace_cas32(uint32_t *target, uint32_t cmp, uint32_t new)
{
	uint32_t old;

	if ((old = *target) == cmp)
		*target = new;
	return (old);
}

void *
dtrace_casptr(void *target, void *cmp, void *new)
{
	void *old;

	if ((old = *(void **)target) == cmp)
		*(void **)target = new;
	return (old);
}

#else	/* lint */

	ENTRY(dtrace_cas32)
	ALTENTRY(dtrace_casptr)
1:
	ldrex	r3, [r0]
	teq	r3, r1
	movne	r0, r3
	movne	pc, lr
	strex	ip, r2, [r0]
	teq	ip, #0
	bne	1b
	mov	r0, r3
	mov	pc, lr
	SET_SIZE(dtrace_casptr)
	SET_SIZE(dtrace_cas32)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
uintptr_t
dtrace_caller(int aframes)
{
	return (0);
}

#else	/* lint */

	ENTRY(dtrace_caller)
	mov	r0, #-1
	mov	pc, lr
	SET_SIZE(dtrace_caller)

#endif	/* lint */

#if defined(lint)

/*ARGSUSED*/
void
dtrace_copyin(uintptr_t uaddr, uintptr_t kaddr, size_t size,
	      volatile uint16_t *flags)
{}

#else

	ENTRY(dtrace_copyin)
	teq	r2, #0		/* if (size == 0) */
	moveq	pc, lr		/*   return */
	ldrh	ip, [r3]
	tst	ip, #CPU_DTRACE_BADADDR
	movne	pc, lr		/* if (*flags & CPU_DTRACE_BADADDR) return */
	mov	r3, #0
	SYNC_BARRIER(r3)	/* Memory synchronization barrier */
	add	r2, r0, r2	/* r2 = uaddr + size */
1:
	ldrbt	r3, [r0], #1	/* r3 = *uaddr, uaddr++ */
	strb    r3, [r1], #1	/* *kaddr = r3, kaddr++ */
	cmp	r0, r2		/* if (uaddr < r2) */
	blo	1b		/*   goto 1b */
	mov	pc, lr		/* return */
	SET_SIZE(dtrace_copyin)

#endif

#if defined(lint)

/*ARGSUSED*/
void
dtrace_copyinstr(uintptr_t uaddr, uintptr_t kaddr, size_t size,
		 volatile uint16_t *flags)
{}

#else

	ENTRY(dtrace_copyinstr)
	teq	r2, #0		/* if (size == 0) */
	moveq	pc, lr		/*   return */
	ldrh	ip, [r3]
	tst	ip, #CPU_DTRACE_BADADDR
	movne	pc, lr		/* if (*flags & CPU_DTRACE_BADADDR) return */
	mov	r3, #0
	SYNC_BARRIER(r3)	/* Memory synchronization barrier */
	add	r2, r0, r2	/* r2 = uaddr + size */
1:
	ldrbt	r3, [r0], #1	/* r3 = *uaddr, uaddr++ */
	strb    r3, [r1], #1	/* *kaddr = r3, kaddr++ */
	teq	r3, #0		/* if (r3 == 0) */
	moveq	pc, lr		/*   return */
	cmp	r0, r2		/* if (uaddr < r2) */
	blo	1b		/*   goto 1b */
	mov	pc, lr		/* return */
	SET_SIZE(dtrace_copyinstr)

#endif

#if defined(lint)

/*ARGSUSED*/
void
dtrace_copyout(uintptr_t kaddr, uintptr_t uaddr, size_t size,
	       volatile uint16_t *flags)
{}

#else

	ENTRY(dtrace_copyout)
	teq	r2, #0		/* if (size == 0) */
	moveq	pc, lr		/*   return */
	ldrh	ip, [r3]
	tst	ip, #CPU_DTRACE_BADADDR
	movne	pc, lr		/* if (*flags & CPU_DTRACE_BADADDR) return */
	mov	r3, #0
	SYNC_BARRIER(r3)	/* Memory synchronization barrier */
	add	r2, r0, r2	/* r2 = kaddr + size */
1:
	ldrb	r3, [r0], #1	/* r3 = *kaddr, kaddr++ */
	strbt	r3, [r1], #1	/* *uaddr = r3, uaddr++ */
	cmp	r0, r2		/* if (kaadr < r2) */
	blo	1b		/*   goto 1b */
	mov	pc, lr		/* return */
	SET_SIZE(dtrace_copyout)

#endif

#if defined(lint)

/*ARGSUSED*/
void
dtrace_copyoutstr(uintptr_t kaddr, uintptr_t uaddr, size_t size,
		  volatile uint16_t *flags)
{}

#else

	ENTRY(dtrace_copyoutstr)
	teq	r2, #0		/* if (size == 0) */
	moveq	pc, lr		/*   return */
	ldrh	ip, [r3]
	tst	ip, #CPU_DTRACE_BADADDR
	movne	pc, lr		/* if (*flags & CPU_DTRACE_BADADDR) return */
	 mov	r3, #0
	SYNC_BARRIER(r3)	/* Memory synchronization barrier */
	add	r2, r0, r2	/* r2 = kaddr + size */
1:
	ldrb	r3, [r0], #1	/* r3 = *kaddr, kaddr++ */
	strbt	r3, [r1], #1	/* *uaddr = r3, uaddr++ */
	teq	r3, #0		/* if (r3 == 0) */
	moveq	pc, lr		/*   return */
	cmp	r0, r2		/* if (kaadr < r2) */
	blo	1b		/*   goto 1b */
	mov	pc, lr		/* return */
	SET_SIZE(dtrace_copyoutstr)

#endif

#if defined(lint)

/*ARGSUSED*/
uint8_t
dtrace_fuword8(void *addr)
{ return (0); }

#else

	ENTRY(dtrace_fuword8)
	mov	r3, #0
	SYNC_BARRIER(r3)	/* Memory synchronization barrier */
	ldrbt	r1, [r0]
	mov	r0, r1
	mov	pc, lr
	SET_SIZE(dtrace_fuword8)

#endif

#if defined(lint)

/*ARGSUSED*/
uint16_t
dtrace_fuword16(void *addr)
{ return (0); }

#else

	ENTRY(dtrace_fuword16)
	mov	r3, #0
	SYNC_BARRIER(r3)	/* Memory synchronization barrier */
	ldrbt	r1, [r0], #1
	ldrbt	r2, [r0]
	orr	r0, r1, r2, lsl #8
	mov	pc, lr
	SET_SIZE(dtrace_fuword16)

#endif

#if defined(lint)

/*ARGSUSED*/
uint32_t
dtrace_fuword32(void *addr)
{ return (0); }

#else

	ENTRY(dtrace_fuword32)
	mov	r3, #0
	SYNC_BARRIER(r3)	/* Memory synchronization barrier */
	ldrt	r1, [r0]
	mov	r0, r1
	mov	pc, lr
	SET_SIZE(dtrace_fuword32)

#endif

#if defined(lint)

/*ARGSUSED*/
uint64_t
dtrace_fuword64(void *addr)
{ return (0); }

#else

	ENTRY(dtrace_fuword64)
	mov	r3, #0
	SYNC_BARRIER(r3)	/* Memory synchronization barrier */
	ldrt	r2, [r0], #4
	ldrt	r1, [r0]
	mov	r0, r2
	mov	pc, lr
	SET_SIZE(dtrace_fuword64)

#endif
