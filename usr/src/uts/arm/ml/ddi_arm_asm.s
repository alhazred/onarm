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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2007 NEC Corporation
 */

#ident	"@(#)arm/ml/ddi_arm_asm.s"

#if defined(lint) || defined(__lint)
#include <sys/types.h>
#include <sys/sunddi.h>
#else
#include <sys/asm_linkage.h>
#include <sys/cpuvar_impl.h>
#include "assym.h"
#endif

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
uint8_t
ddi_get8(ddi_acc_handle_t handle, uint8_t *addr)
{
	return (0);
}

#ifndef NO_USEDDI
/*ARGSUSED*/
uint8_t
ddi_mem_get8(ddi_acc_handle_t handle, uint8_t *addr)
{
	return (0);
}
#endif /* NO_USEDDI */

#ifndef NO_USEDDI
/*ARGSUSED*/
uint8_t
ddi_io_get8(ddi_acc_handle_t handle, uint8_t *dev_addr)
{
	return (0);
}
#endif /* NO_USEDDI */

/*ARGSUSED*/
uint16_t
ddi_get16(ddi_acc_handle_t handle, uint16_t *addr)
{
	return (0);
}

#ifndef NO_USEDDI
/*ARGSUSED*/
uint16_t
ddi_mem_get16(ddi_acc_handle_t handle, uint16_t *addr)
{
	return (0);
}
#endif /* NO_USEDDI */

#ifndef NO_USEDDI
/*ARGSUSED*/
uint16_t
ddi_io_get16(ddi_acc_handle_t handle, uint16_t *dev_addr)
{
	return (0);
}
#endif /* NO_USEDDI */

/*ARGSUSED*/
uint32_t
ddi_get32(ddi_acc_handle_t handle, uint32_t *addr)
{
	return (0);
}

#ifndef NO_USEDDI
/*ARGSUSED*/
uint32_t
ddi_mem_get32(ddi_acc_handle_t handle, uint32_t *addr)
{
	return (0);
}
#endif /* NO_USEDDI */

#ifndef NO_USEDDI
/*ARGSUSED*/
uint32_t
ddi_io_get32(ddi_acc_handle_t handle, uint32_t *dev_addr)
{
	return (0);
}
#endif /* NO_USEDDI */

/*ARGSUSED*/
uint64_t
ddi_get64(ddi_acc_handle_t handle, uint64_t *addr)
{
	return (0);
}

#ifndef NO_USEDDI
/*ARGSUSED*/
uint64_t
ddi_mem_get64(ddi_acc_handle_t handle, uint64_t *addr)
{
	return (0);
}
#endif /* NO_USEDDI */

/*ARGSUSED*/
void
ddi_put8(ddi_acc_handle_t handle, uint8_t *addr, uint8_t value)
{}

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_mem_put8(ddi_acc_handle_t handle, uint8_t *dev_addr, uint8_t value)
{}
#endif /* NO_USEDDI */

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_io_put8(ddi_acc_handle_t handle, uint8_t *dev_addr, uint8_t value)
{}
#endif /* NO_USEDDI */

/*ARGSUSED*/
void
ddi_put16(ddi_acc_handle_t handle, uint16_t *addr, uint16_t value)
{}

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_mem_put16(ddi_acc_handle_t handle, uint16_t *dev_addr, uint16_t value)
{}
#endif /* NO_USEDDI */

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_io_put16(ddi_acc_handle_t handle, uint16_t *dev_addr, uint16_t value)
{}
#endif /* NO_USEDDI */

/*ARGSUSED*/
void
ddi_put32(ddi_acc_handle_t handle, uint32_t *addr, uint32_t value)
{}

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_mem_put32(ddi_acc_handle_t handle, uint32_t *dev_addr, uint32_t value)
{}
#endif /* NO_USEDDI */

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_io_put32(ddi_acc_handle_t handle, uint32_t *dev_addr, uint32_t value)
{}
#endif /* NO_USEDDI */

/*ARGSUSED*/
void
ddi_put64(ddi_acc_handle_t handle, uint64_t *addr, uint64_t value)
{}

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_mem_put64(ddi_acc_handle_t handle, uint64_t *dev_addr, uint64_t value)
{}
#endif /* NO_USEDDI */

/*ARGSUSED*/
void
ddi_rep_get8(ddi_acc_handle_t handle, uint8_t *host_addr, uint8_t *dev_addr,
    size_t repcount, uint_t flags)
{}

/*ARGSUSED*/
void
ddi_rep_get16(ddi_acc_handle_t handle, uint16_t *host_addr, uint16_t *dev_addr,
    size_t repcount, uint_t flags)
{}

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_rep_get32(ddi_acc_handle_t handle, uint32_t *host_addr, uint32_t *dev_addr,
    size_t repcount, uint_t flags)
{}
#endif /* NO_USEDDI */

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_rep_get64(ddi_acc_handle_t handle, uint64_t *host_addr, uint64_t *dev_addr,
    size_t repcount, uint_t flags)
{}
#endif /* NO_USEDDI */

/*ARGSUSED*/
void
ddi_rep_put8(ddi_acc_handle_t handle, uint8_t *host_addr, uint8_t *dev_addr,
    size_t repcount, uint_t flags)
{}

/*ARGSUSED*/
void
ddi_rep_put16(ddi_acc_handle_t handle, uint16_t *host_addr, uint16_t *dev_addr,
    size_t repcount, uint_t flags)
{}

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_rep_put32(ddi_acc_handle_t handle, uint32_t *host_addr, uint32_t *dev_addr,
    size_t repcount, uint_t flags)
{}
#endif /* NO_USEDDI */

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_rep_put64(ddi_acc_handle_t handle, uint64_t *host_addr, uint64_t *dev_addr,
    size_t repcount, uint_t flags)
{}
#endif /* NO_USEDDI */

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_mem_rep_get8(ddi_acc_handle_t handle, uint8_t *host_addr,
    uint8_t *dev_addr, size_t repcount, uint_t flags)
{}
#endif /* NO_USEDDI */

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_mem_rep_get16(ddi_acc_handle_t handle, uint16_t *host_addr,
    uint16_t *dev_addr, size_t repcount, uint_t flags)
{}
#endif /* NO_USEDDI */

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_mem_rep_get32(ddi_acc_handle_t handle, uint32_t *host_addr,
    uint32_t *dev_addr, size_t repcount, uint_t flags)
{}
#endif /* NO_USEDDI */

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_mem_rep_get64(ddi_acc_handle_t handle, uint64_t *host_addr,
    uint64_t *dev_addr, size_t repcount, uint_t flags)
{}
#endif /* NO_USEDDI */

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_mem_rep_put8(ddi_acc_handle_t handle, uint8_t *host_addr,
    uint8_t *dev_addr, size_t repcount, uint_t flags)
{}
#endif /* NO_USEDDI */

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_mem_rep_put16(ddi_acc_handle_t handle, uint16_t *host_addr,
    uint16_t *dev_addr, size_t repcount, uint_t flags)
{}
#endif /* NO_USEDDI */

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_mem_rep_put32(ddi_acc_handle_t handle, uint32_t *host_addr,
    uint32_t *dev_addr, size_t repcount, uint_t flags)
{}
#endif /* NO_USEDDI */

#ifndef NO_USEDDI
/*ARGSUSED*/
void
ddi_mem_rep_put64(ddi_acc_handle_t handle, uint64_t *host_addr,
    uint64_t *dev_addr, size_t repcount, uint_t flags)
{}
#endif /* NO_USEDDI */

#else	/* lint */

	
ENTRY(ddi_get8)
#ifndef NO_USEDDI
ALTENTRY(ddi_getb)
ALTENTRY(ddi_mem_getb)
ALTENTRY(ddi_mem_get8)
ALTENTRY(ddi_io_getb)
ALTENTRY(ddi_io_get8)
#endif /* NO_USEDDI */
	/*
	 * read value (8 bit)
	 *    r0: ddi_acc_handle_t handle
	 *    r1: uint8_t *addr
	 *    r2: use temporary work space
	 */

	/* Call access procedure unless DDI_ACCATTR_DIRECT is set. */
	ldr	r2, [r0, #ACC_ATTR]
	tst	r2, #DDI_ACCATTR_DIRECT
	beq	1f

	ldrb	r0, [r1]		/* r0 = *addr */
	mov	pc, lr			/* return r0 */
1:
	ldr	pc, [r0, #ACC_GETB]	/* call handle->ahi_get8(handle, *addr)*/
	SET_SIZE(ddi_get8)
#ifndef NO_USEDDI
	SET_SIZE(ddi_getb)
	SET_SIZE(ddi_mem_getb)
	SET_SIZE(ddi_mem_get8)
	SET_SIZE(ddi_io_getb)
	SET_SIZE(ddi_io_get8)
#endif /* NO_USEDDI */

ENTRY(ddi_get16)
#ifndef NO_USEDDI
ALTENTRY(ddi_getw)
ALTENTRY(ddi_mem_getw)
ALTENTRY(ddi_mem_get16)
ALTENTRY(ddi_io_getw)
ALTENTRY(ddi_io_get16)
#endif /* NO_USEDDI */
	/*
	 * read value (16 bit)
	 *    r0: ddi_acc_handle_t handle
	 *    r1: uint16_t *addr
	 *    r2: use temporary work space
	 */

	/* Call access procedure unless DDI_ACCATTR_DIRECT is set. */
	ldr	r2, [r0, #ACC_ATTR]
	tst	r2, #DDI_ACCATTR_DIRECT
	beq	2f

	ldrh	r0, [r1]		/* r0 = *addr */
	mov	pc, lr			/* return r0 */
2:
	ldr	pc, [r0, #ACC_GETW]	/* call handle->ahi_get16(handle, *addr)*/
	SET_SIZE(ddi_get16)
#ifndef NO_USEDDI
	SET_SIZE(ddi_getw)
	SET_SIZE(ddi_mem_getw)
	SET_SIZE(ddi_mem_get16)
	SET_SIZE(ddi_io_getw)
	SET_SIZE(ddi_io_get16)
#endif /* NO_USEDDI */

ENTRY(ddi_get32)
#ifndef NO_USEDDI
ALTENTRY(ddi_getl)
ALTENTRY(ddi_mem_getl)
ALTENTRY(ddi_mem_get32)
ALTENTRY(ddi_io_getl)
ALTENTRY(ddi_io_get32)
#endif /* NO_USEDDI */
	/*
	 * read value (32 bit)
	 *    r0: ddi_acc_handle_t handle
	 *    r1: uint32_t *addr
	 *    r2: use temporary work space
	 */

	/* Call access procedure unless DDI_ACCATTR_DIRECT is set. */
	ldr	r2, [r0, #ACC_ATTR]
	tst	r2, #DDI_ACCATTR_DIRECT
	beq	3f

	ldr	r0, [r1]		/* r0 = *addr */
	mov	pc, lr			/* return r0 */
3:
	ldr	pc, [r0, #ACC_GETL]	/* call handle->ahi_get32(handle, *addr)*/
	SET_SIZE(ddi_get32)
#ifndef NO_USEDDI
	SET_SIZE(ddi_getl)
	SET_SIZE(ddi_mem_getl)
	SET_SIZE(ddi_mem_get32)
	SET_SIZE(ddi_io_getl)
	SET_SIZE(ddi_io_get32)
#endif /* NO_USEDDI */

ENTRY(ddi_get64)
#ifndef NO_USEDDI
ALTENTRY(ddi_getll)
ALTENTRY(ddi_mem_getll)
ALTENTRY(ddi_mem_get64)
#endif /* NO_USEDDI */
	/*
	 * read value (64 bit)
	 *    r0: ddi_acc_handle_t handle
	 *    r1: uint64_t *addr
	 */
	ldr	pc, [r0, #ACC_GETLL]	/*call handle->ahi_get64(handle, *addr)*/
	SET_SIZE(ddi_get64)
#ifndef NO_USEDDI
	SET_SIZE(ddi_getll)
	SET_SIZE(ddi_mem_getll)
	SET_SIZE(ddi_mem_get64)
#endif /* NO_USEDDI */

ENTRY(ddi_put8)
#ifndef NO_USEDDI
ALTENTRY(ddi_putb)
ALTENTRY(ddi_mem_putb)
ALTENTRY(ddi_mem_put8)
ALTENTRY(ddi_io_putb)
ALTENTRY(ddi_io_put8)
#endif /* NO_USEDDI */
	/*
	 * Write value (8 bit)
	 *    r0: ddi_acc_handle_t handle
	 *    r1: uint8_t *addr
	 *    r2: uint8_t value	
	 *    r3: use temporary work space
	 */

	/* Call access procedure unless DDI_ACCATTR_DIRECT is set. */
	ldr	r3, [r0, #ACC_ATTR]
	tst	r3, #DDI_ACCATTR_DIRECT
	beq	4f

	strb	r2, [r1]		/* *addr = value */
	mov	pc, lr			/* return */
4:
	ldr	pc, [r0, #ACC_PUTB]	/* call handle->ahi_put8(handle, *addr, value) */
	SET_SIZE(ddi_put8)
#ifndef NO_USEDDI
	SET_SIZE(ddi_putb)
	SET_SIZE(ddi_mem_putb)
	SET_SIZE(ddi_mem_put8)
	SET_SIZE(ddi_io_putb)
	SET_SIZE(ddi_io_put8)
#endif /* NO_USEDDI */

ENTRY(ddi_put16)
#ifndef NO_USEDDI
ALTENTRY(ddi_putw)
ALTENTRY(ddi_mem_putw)
ALTENTRY(ddi_mem_put16)
ALTENTRY(ddi_io_putw)
ALTENTRY(ddi_io_put16)
#endif /* NO_USEDDI */
	/*
	 * Write value (16 bit)
	 *    r0: ddi_acc_handle_t handle
	 *    r1: uint16_t *addr
	 *    r2: uint16_t value	
	 *    r3: use temporary work space
	 */

	/* Call access procedure unless DDI_ACCATTR_DIRECT is set. */
	ldr	r3, [r0, #ACC_ATTR]
	tst	r3, #DDI_ACCATTR_DIRECT
	beq	5f

	strh	r2, [r1]		/* *addr = value */
	mov	pc, lr			/* return */
5:
	ldr	pc, [r0, #ACC_PUTW]	/* call handle->ahi_put16(handle, *addr, value) */
	SET_SIZE(ddi_put16)
#ifndef NO_USEDDI
	SET_SIZE(ddi_putw)
	SET_SIZE(ddi_mem_putw)
	SET_SIZE(ddi_mem_put16)
	SET_SIZE(ddi_io_putw)
	SET_SIZE(ddi_io_put16)
#endif /* NO_USEDDI */

ENTRY(ddi_put32)
#ifndef NO_USEDDI
ALTENTRY(ddi_putl)
ALTENTRY(ddi_mem_putl)
ALTENTRY(ddi_mem_put32)
ALTENTRY(ddi_io_putl)
ALTENTRY(ddi_io_put32)
#endif /* NO_USEDDI */
	/*
	 * Write value (32 bit)
	 *    r0: ddi_acc_handle_t handle
	 *    r1: uint32_t *addr
	 *    r2: uint32_t value	
	 *    r3: use temporary work space
	 */

	/* Call access procedure unless DDI_ACCATTR_DIRECT is set. */
	ldr	r3, [r0, #ACC_ATTR]
	tst	r3, #DDI_ACCATTR_DIRECT
	beq	6f

	str	r2, [r1]		/* *addr = value */
	mov	pc, lr			/* return */
6:
	ldr	pc, [r0, #ACC_PUTL]	/* call handle->ahi_put32(handle, *addr, value) */
	SET_SIZE(ddi_put32)
#ifndef NO_USEDDI
	SET_SIZE(ddi_putl)
	SET_SIZE(ddi_mem_putl)
	SET_SIZE(ddi_mem_put32)
	SET_SIZE(ddi_io_putl)
	SET_SIZE(ddi_io_put32)
#endif /* NO_USEDDI */

ENTRY(ddi_put64)
#ifndef NO_USEDDI
ALTENTRY(ddi_putll)
ALTENTRY(ddi_mem_putll)
ALTENTRY(ddi_mem_put64)
#endif /* NO_USEDDI */
	/*
	 * Write value (64 bit)
	 *    r0: ddi_acc_handle_t handle
	 *    r1: uint32_t *addr
	 *    r2: uint32_t value	
	 */
	ldr	pc, [r0, #ACC_PUTLL]	/* call handle->ahi_put64(handle, *addr, value) */
	SET_SIZE(ddi_put64)
#ifndef NO_USEDDI
	SET_SIZE(ddi_putll)
	SET_SIZE(ddi_mem_putll)
	SET_SIZE(ddi_mem_put64)
#endif /* NO_USEDDI */

ENTRY(ddi_rep_get8)
#ifndef NO_USEDDI
ALTENTRY(ddi_rep_getb)
ALTENTRY(ddi_mem_rep_getb)
ALTENTRY(ddi_mem_rep_get8)
#endif /* NO_USEDDI */
	/*
	 * Read value (8 bit) repeatedly.
	 *    r0: ddi_acc_handle_t handle
	 *    r1: uint8_t *host_addr
	 *    r2: uint8_t *dev_addr
	 *    r3: size_t repcount
	 *    stack: uint_t flags
	 */
	ldr	pc, [r0, #ACC_REP_GETB]	/* call handle->ahi_rep_get8(handle, *host_add,
								     *dev_addr, repcount, flags) */
	SET_SIZE(ddi_rep_get8)
#ifndef NO_USEDDI
	SET_SIZE(ddi_rep_getb)
	SET_SIZE(ddi_mem_rep_getb)
	SET_SIZE(ddi_mem_rep_get8)
#endif /* NO_USEDDI */

ENTRY(ddi_rep_get16)
#ifndef NO_USEDDI
ALTENTRY(ddi_rep_getw)
ALTENTRY(ddi_mem_rep_getw)
ALTENTRY(ddi_mem_rep_get16)
#endif /* NO_USEDDI */
	/*
	 * Read value (16 bit) repeatedly.
	 *    r0: ddi_acc_handle_t handle
	 *    r1: uint16_t *host_addr
	 *    r2: uint16_t *dev_addr
	 *    r3: size_t repcount
	 *    stack: uint_t flags
	 */
	ldr	pc, [r0, #ACC_REP_GETW]	/* call handle->ahi_rep_get16(handle, *host_add,
								     *dev_addr, repcount, flags) */
	SET_SIZE(ddi_rep_get16)
#ifndef NO_USEDDI
	SET_SIZE(ddi_rep_getw)
	SET_SIZE(ddi_mem_rep_getw)
	SET_SIZE(ddi_mem_rep_get16)
#endif /* NO_USEDDI */

ENTRY(ddi_rep_get32)
#ifndef NO_USEDDI
ALTENTRY(ddi_rep_getl)
ALTENTRY(ddi_mem_rep_getl)
ALTENTRY(ddi_mem_rep_get32)
#endif /* NO_USEDDI */
	/*
	 * Read value (32 bit) repeatedly.
	 *    r0: ddi_acc_handle_t handle
	 *    r1: uint32_t *host_addr
	 *    r2: uint32_t *dev_addr
	 *    r3: size_t repcount
	 *    stack: uint_t flags
	 */
	ldr	pc, [r0, #ACC_REP_GETL]	/* call handle->ahi_rep_get32(handle, *host_add,
								     *dev_addr, repcount, flags) */
	SET_SIZE(ddi_rep_get32)
#ifndef NO_USEDDI
	SET_SIZE(ddi_rep_getl)
	SET_SIZE(ddi_mem_rep_getl)
	SET_SIZE(ddi_mem_rep_get32)
#endif /* NO_USEDDI */

ENTRY(ddi_rep_get64)
#ifndef NO_USEDDI
ALTENTRY(ddi_rep_getll)
ALTENTRY(ddi_mem_rep_getll)
ALTENTRY(ddi_mem_rep_get64)
#endif /* NO_USEDDI */
	/*
	 * Read value (64 bit) repeatedly.
	 *    r0: ddi_acc_handle_t handle
	 *    r1: uint64_t *host_addr
	 *    r2: uint64_t *dev_addr
	 *    r3: size_t repcount
	 *    stack: uint_t flags
	 */
	ldr	pc, [r0, #ACC_REP_GETLL] /* call handle->ahi_rep_get64(handle, *host_add,
								       *dev_addr, repcount, flags) */
	SET_SIZE(ddi_rep_get64)
#ifndef NO_USEDDI
	SET_SIZE(ddi_rep_getll)
	SET_SIZE(ddi_mem_rep_getll)
	SET_SIZE(ddi_mem_rep_get64)
#endif /* NO_USEDDI */

ENTRY(ddi_rep_put8)
#ifndef NO_USEDDI
ALTENTRY(ddi_rep_putb)
ALTENTRY(ddi_mem_rep_putb)
ALTENTRY(ddi_mem_rep_put8)
#endif /* NO_USEDDI */
	/*
	 * Write value (8 bit) repeatedly.
	 *    r0: ddi_acc_handle_t handle
	 *    r1: uint8_t *host_addr
	 *    r2: uint8_t *dev_addr
	 *    r3: size_t repcount
	 *    stack: uint_t flags
	 */
	ldr	pc, [r0, #ACC_REP_PUTB] /* call handle->ahi_rep_put8(handle, *host_add,
								     *dev_addr, repcount, flags) */
	SET_SIZE(ddi_rep_put8)
#ifndef NO_USEDDI
	SET_SIZE(ddi_rep_putb)
	SET_SIZE(ddi_mem_rep_putb)
	SET_SIZE(ddi_mem_rep_put8)
#endif /* NO_USEDDI */

ENTRY(ddi_rep_put16)
#ifndef NO_USEDDI
ALTENTRY(ddi_rep_putw)
ALTENTRY(ddi_mem_rep_putw)
ALTENTRY(ddi_mem_rep_put16)
#endif /* NO_USEDDI */
	/*
	 * Write value (16 bit) repeatedly.
	 *    r0: ddi_acc_handle_t handle
	 *    r1: uint16_t *host_addr
	 *    r2: uint16_t *dev_addr
	 *    r3: size_t repcount
	 *    stack: uint_t flags
	 */
	ldr	pc, [r0, #ACC_REP_PUTW] /* call handle->ahi_rep_put16(handle, *host_add,
								      *dev_addr, repcount, flags) */
	SET_SIZE(ddi_rep_put16)
#ifndef NO_USEDDI
	SET_SIZE(ddi_rep_putw)
	SET_SIZE(ddi_mem_rep_putw)
	SET_SIZE(ddi_mem_rep_put16)
#endif /* NO_USEDDI */

ENTRY(ddi_rep_put32)
#ifndef NO_USEDDI
ALTENTRY(ddi_rep_putl)
ALTENTRY(ddi_mem_rep_putl)
ALTENTRY(ddi_mem_rep_put32)
#endif /* NO_USEDDI */
	/*
	 * Write value (32 bit) repeatedly.
	 *    r0: ddi_acc_handle_t handle
	 *    r1: uint32_t *host_addr
	 *    r2: uint32_t *dev_addr
	 *    r3: size_t repcount
	 *    stack: uint_t flags
	 */
	ldr	pc, [r0, #ACC_REP_PUTL] /* call handle->ahi_rep_put32(handle, *host_add,
								      *dev_addr, repcount, flags) */
	SET_SIZE(ddi_rep_put32)
#ifndef NO_USEDDI
	SET_SIZE(ddi_rep_putl)
	SET_SIZE(ddi_mem_rep_putl)
	SET_SIZE(ddi_mem_rep_put32)
#endif /* NO_USEDDI */

ENTRY(ddi_rep_put64)
#ifndef NO_USEDDI
ALTENTRY(ddi_rep_putll)
ALTENTRY(ddi_mem_rep_putll)
ALTENTRY(ddi_mem_rep_put64)
#endif /* NO_USEDDI */
	/*
	 * Write value (64 bit) repeatedly.
	 *    r0: ddi_acc_handle_t handle
	 *    r1: uint64_t *host_addr
	 *    r2: uint64_t *dev_addr
	 *    r3: size_t repcount
	 *    stack: uint_t flags
	 */
	ldr	pc, [r0, #ACC_REP_PUTLL] /* call handle->ahi_rep_put64(handle, *host_add,
								       *dev_addr, repcount, flags) */
	SET_SIZE(ddi_rep_put64)
#ifndef NO_USEDDI
	SET_SIZE(ddi_rep_putll)
	SET_SIZE(ddi_mem_rep_putll)
	SET_SIZE(ddi_mem_rep_put64)
#endif /* NO_USEDDI */

#endif /* lint */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
uint8_t
i_ddi_vaddr_get8(ddi_acc_impl_t *hdlp, uint8_t *addr)
{
	return (*addr);
}

/*ARGSUSED*/
uint16_t
i_ddi_vaddr_get16(ddi_acc_impl_t *hdlp, uint16_t *addr)
{
	return (*addr);
}

/*ARGSUSED*/
uint32_t
i_ddi_vaddr_get32(ddi_acc_impl_t *hdlp, uint32_t *addr)
{
	return (*addr);
}

/*ARGSUSED*/
uint64_t
i_ddi_vaddr_get64(ddi_acc_impl_t *hdlp, uint64_t *addr)
{
	return (*addr);
}

#else	/* lint */

ENTRY(i_ddi_vaddr_get8)
	/*
	 * Read value (8 bit) from memory space.
	 *    r0: ddi_acc_impl_t *hdlp
	 *    r1: uint8_t *addr
	 */
	ldrb	r0, [r1]	/* return (*addr) */
	mov	pc, lr
	SET_SIZE(i_ddi_vaddr_get8)

ENTRY(i_ddi_vaddr_get16)
	/*
	 * Read value (16 bit) from memory space.
	 *    r0: ddi_acc_impl_t *hdlp
	 *    r1: uint16_t *addr
	 */
	ldrh	r0, [r1]	/* return (*addr) */
	mov	pc, lr
	SET_SIZE(i_ddi_vaddr_get16)

ENTRY(i_ddi_vaddr_get32)
	/*
	 * Read value (32 bit) from memory space.
	 *    r0: ddi_acc_impl_t *hdlp
	 *    r1: uint32_t *addr
	 */
	ldr	r0, [r1]	/* return (*addr) */
	mov	pc, lr
	SET_SIZE(i_ddi_vaddr_get32)

ENTRY(i_ddi_vaddr_get64)
	/*
	 * Read value (64 bit) from memory space.
	 *    r0: ddi_acc_impl_t *hdlp
	 *    r1: uint64_t *addr
	 */
	ldrd	r0, [r1]	/* return (*addr) */
	mov	pc, lr
	SET_SIZE(i_ddi_vaddr_get64)

#endif /* lint */

#if defined(lint) || defined(__lint)

/*ARGSUSED*/
void
i_ddi_vaddr_put8(ddi_acc_impl_t *hdlp, uint8_t *addr, uint8_t value)
{
	*addr = value;
}

/*ARGSUSED*/
void
i_ddi_vaddr_put16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value)
{
	*addr = value;
}

/*ARGSUSED*/
void
i_ddi_vaddr_put32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value)
{
	*(uint32_t *)addr = value;
}

/*ARGSUSED*/
void
i_ddi_vaddr_put64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value)
{
	*addr = value;
}

#else	/* lint */

ENTRY(i_ddi_vaddr_put8)
	/*
	 * Write value (8 bit) to memory space.
	 *    r0: ddi_acc_impl_t *hdlp
	 *    r1: uint8_t *addr
	 *    r2: uint8_t value
	 */
	strb	r2, [r1]	/* *addr = value */
	mov	pc, lr
	SET_SIZE(i_ddi_vaddr_put8)

ENTRY(i_ddi_vaddr_put16)
	/*
	 * Write value (16 bit) to memory space.
	 *    r0: ddi_acc_impl_t *hdlp
	 *    r1: uint16_t *addr
	 *    r2: uint16_t value
	 */
	strh	r2, [r1]	/* *addr = value */
	mov	pc, lr
	SET_SIZE(i_ddi_vaddr_put16)

ENTRY(i_ddi_vaddr_put32)
	/*
	 * Write value (32 bit) to memory space.
	 *    r0: ddi_acc_impl_t *hdlp
	 *    r1: uint32_t *addr
	 *    r2: uint32_t value
	 */
	str	r2, [r1]	/* *addr = value */
	mov	pc, lr
	SET_SIZE(i_ddi_vaddr_put32)

ENTRY(i_ddi_vaddr_put64)
	/*
	 * Write value (64 bit) to memory space.
	 *    r0: ddi_acc_impl_t *hdlp
	 *    r1: uint64_t *addr
	 *    r2, r3: uint64_t value
	 */
	strd	r2, [r1]	/* *addr = value */
	mov	pc, lr
	SET_SIZE(i_ddi_vaddr_put64)

#endif /* lint */

/*
 * uint64_t
 * i_ddi_vaddr_swap_get64(ddi_acc_impl_t *hdlp, uint64_t *addr)
 *	Read 64 bit data with byte swapping.
 *	Optimized for ARM V6 architecture.
 */

#if	defined(lint) || defined(__lint)

uint64_t
i_ddi_vaddr_swap_get64(ddi_acc_impl_t *hdlp, uint64_t *addr)
{
	return arm_bswap64(*addr);
}

#else	/* !(defined(lint) || defined(__lint)) */

ENTRY(i_ddi_vaddr_swap_get64)
	/*
	 * Swap bytes in higher and lower 32 bit.
	 */
	ldmia	r1, {r2, r3}
	rev	r2, r2
	rev	r3, r3

	/* Swap high and low 32 bit. */
	mov	r0, r3
	mov	r1, r2
	mov	pc, lr
	SET_SIZE(i_ddi_vaddr_swap_get64)

#endif	/* defined(lint) || defined(__lint) */

/*
 * void
 * i_ddi_vaddr_swap_put64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value)
 *	Write 64 bit data with byte swapping.
 *	Optimized for ARM V6 architecture.
 */

#if	defined(lint) || defined(__lint)

void
i_ddi_vaddr_swap_put64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value)
{
	*addr = arm_bswap64(value);
}

#else	/* !(defined(lint) || defined(__lint)) */

ENTRY(i_ddi_vaddr_swap_put64)
	/* Swap bytes in higher and lower 32 bit. */
	rev	r2, r2
	rev	r3, r3

	/*
	 * Store 64 bit data with swapping higher and lower 32 bit.
	 * We can use r0 as temporary register because hdlp is not used.
	 */
	mov	r0, r3
	stmia	r1, {r0, r2}
	mov	pc, lr
	SET_SIZE(i_ddi_vaddr_swap_put64)

#endif	/* defined(lint) || defined(__lint) */
