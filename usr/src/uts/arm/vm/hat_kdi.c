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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006 NEC Corporation
 */

#ident	"@(#)arm/vm/hat_kdi.c"

/*
 * HAT interfaces used by the kernel debugger to interact with the VM system.
 * These interfaces are invoked when the world is stopped.  As such, no blocking
 * operations may be performed.
 */

#include <sys/cpuvar.h>
#include <sys/kdi_impl.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/mman.h>
#include <sys/bootconf.h>
#include <sys/cmn_err.h>
#include <vm/seg_kmem.h>
#include <sys/machsystm.h>

/* Just a stub */

void
hat_boot_kdi_init(void)
{
}

void
hat_kdi_init(void)
{
}

/*ARGSUSED*/
int
kdi_vtop(uintptr_t va, uint64_t *pap)
{
	return 0;
}

/*ARGSUSED*/
int
kdi_pread(caddr_t buf, size_t nbytes, uint64_t addr, size_t *ncopiedp)
{
	return 0;
}

/*ARGSUSED*/
int
kdi_pwrite(caddr_t buf, size_t nbytes, uint64_t addr, size_t *ncopiedp)
{
	return 0;
}


/*
 * Return the number of bytes, relative to the beginning of a given range, that
 * are non-toxic (can be read from and written to with relative impunity).
 */
/*ARGSUSED*/
size_t
kdi_range_is_nontoxic(uintptr_t va, size_t sz, int write)
{
	return 0;
}

void
hat_kdi_fini(void)
{
}
