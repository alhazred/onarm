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

#ifndef _SYS_GIC_H
#define _SYS_GIC_H

#ident	"@(#)arm/sys/gic.h"

#ifndef	_ASM

#include <sys/autoconf.h>  /* DDI_INTR_IMPLDBG() */
#include <sys/sunddi.h>    /* DDI_SUCCESS/DDI_FAILURE */

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * GIC_OPS definitions
 */
typedef enum gic_intr_op_e {
	GIC_INTR_OP_GET_PENDING = 1,	/* 1.  Get pending information */
	GIC_INTR_OP_CLEAR_MASK,		/* 2.  Clear interrupt mask */
	GIC_INTR_OP_SET_MASK,		/* 3.  Set interrupt mask */
	GIC_INTR_OP_GET_CAP,		/* 4.  Get devices's capabilities */
	GIC_INTR_OP_SET_CAP,		/* 5.  Set devices's capabilities */
	GIC_INTR_OP_SET_PRI,		/* 6.  Set the interrupt priority */
	GIC_INTR_OP_GET_SHARED,		/* 7.  Get the shared intr info */
} gic_intr_op_t;
#endif	/* !_ASM */

#define GIC_SUCCESS    DDI_SUCCESS
#define GIC_FAILURE    DDI_FAILURE

#ifdef	_KERNEL

#ifndef	_ASM
extern void gic_send_ipi(cpuset_t, uint32_t);
extern void gic_send_ipi_without_mask(cpuset_t, uint32_t);
extern void ipi_send_by_cpuid(processorid_t, uint32_t);
extern void gic_preshutdown(int, int);
extern void gic_shutdown(int, int);
extern void gic_bind_intr(int, processorid_t);
extern void setlvlx(int);
#endif	/* !_ASM */

#if	defined(_KERNEL_BUILD_TREE) && defined(_MACHDEP)
#include <sys/mach_gic.h>
#endif	/* defined(_KERNEL_BUILD_TREE) && defined(_MACHDEP) */

#endif	/* _KERNEL */

#ifndef	_ASM
#ifdef	__cplusplus
}
#endif	/* __cplusplus */
#endif	/* !_ASM */
#endif /* _SYS_GIC_H */
