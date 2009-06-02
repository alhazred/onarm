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

#ifndef	_SYS_MACH_GIC_H
#define	_SYS_MACH_GIC_H

#ident	"@(#)armpf/sys/mach_gic.h"

/*
 * ARM platform specific GIC definitions.
 * Kernel build tree private.
 */

#ifndef	_SYS_GIC_H
#error	Do NOT include mach_gic.h directly.
#endif	/* !_SYS_GIC_H */

#include <sys/mach_gic_impl.h>

#ifdef	_ASM

#include <sys/mpcore.h>
#include "assym.h"

/*
 * GIC_RAISE_PIL(reg1, reg2, reg3, reg4)
 *	Raise processor priority level.
 *	This macro never downgrade PIL below current PIL and CPU base PIL.
 *
 * Calling/Exit State:
 *	New PIL must be set in r0 by the caller.
 *	Old PIL is stored into r0.
 */
#define	GIC_RAISE_PIL(reg1, reg2, reg3, reg4)				\
	DISABLE_IRQ_SAVE(reg4);		/* Disable IRQ */		\
	LOADCPU(reg2);			/* reg2 = CPU */		\
	ldr	reg1, [reg2, #CPU_PRI];	/* reg1 = current PIL */	\
	cmp	reg1, r0;		/* is PIL high enough? */	\
	bhs	1f;			/* yes, return */		\
	ldr	reg3, [reg2, #CPU_BASE_SPL];/* reg3 = CPU->cpu_base_spl */ \
	cmp	r0, reg3;		/* Compare new to base */	\
	movlo	r0, reg3;		/* Use new if base lower */	\
	str	r0, [reg2, #CPU_PRI];	/* CPU->cpu_pri = new PIL */	\
	stmfd	sp!, {reg1, lr};					\
	bl	setlvlx;		/* mask lower irqs */		\
	ldmfd	sp!, {reg1, lr};					\
1:									\
	RESTORE_INTR(reg4);		/* Restore CPSR */		\
	mov	r0, reg1		/* Return old PIL */

/*
 * GIC_RAISE_PIL_CONST(newpil, reg1, reg2, reg3, reg4)
 *	Same as GIC_RAISE_PIL(), but new PIL must be specified by
 *	numeric constant.
 */
#define	GIC_RAISE_PIL_CONST(newpil, reg1, reg2, reg3, reg4)		\
	mov	r0, #(newpil);						\
	GIC_RAISE_PIL(reg1, reg2, reg3, reg4)

/*
 * GIC_SET_PIL(reg1, reg2, reg3, reg4)
 *	Set processor priority level.
 *	This macro never downgrade PIL below CPU base PIL.
 *
 * Calling/Exit State:
 *	New PIL must be set in r0 by the caller.
 *	Old PIL is stored into r0.
 */
#define	GIC_SET_PIL(reg1, reg2, reg3, reg4)				\
	DISABLE_IRQ_SAVE(reg4);		/* Disable IRQ */		\
	LOADCPU(reg2);			/* reg2 = CPU */		\
	ldr	reg1, [reg2, #CPU_PRI];	/* reg1 = current PIL */	\
	ldr	reg3, [reg2, #CPU_BASE_SPL];/* reg3 = CPU->cpu_base_spl */ \
	cmp	r0, reg3;		/* Compare new to base */	\
	movlo	r0, reg3;		/* Use new if base lower */	\
	str	r0, [reg2, #CPU_PRI];	/* CPU->cpu_pri = new PIL */	\
	stmfd	sp!, {reg1, lr};					\
	bl	setlvlx;		/* mask lower IRQs */		\
	ldmfd	sp!, {reg1, lr};					\
	RESTORE_INTR(reg4);		/* Restore CPSR */		\
	mov	r0, reg1		/* Return old PIL */

/*
 * GIC_SET_PIL_CONST(newpil, reg1, reg2, reg3, reg4)
 *	Same as GIC_SET_PIL(), but new PIL must be specified by
 *	numeric constant.
 */
#define	GIC_SET_PIL_CONST(newpil, reg1, reg2, reg3, reg4)		\
	mov	r0, #(newpil);						\
	GIC_SET_PIL(reg1, reg2, reg3, reg4)

#endif	/* _ASM */

#endif	/* !_SYS_MACH_GIC_H */
