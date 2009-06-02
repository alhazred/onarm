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
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)armpf/os/armpf_dep.c"

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/debug.h>
#include <sys/hold_page.h>
#include <sys/sdt.h>
#include <vm/page.h>
#include <vm/vm_dep.h>

#ifdef	DEBUG
#define	DBG_PROBE1(name, type, arg)	DTRACE_PROBE1(name, type, arg)
#else	/* !DEBUG */
#define	DBG_PROBE1(name, type, arg)
#endif	/* DEBUG */

/*
 * ARMPF dependant misc functions.
 */

/*
 * int
 * plat_hold_page(pfn_t pfn, int lock, page_t **pp_ret)
 *	Hold struct page for the use of swrand entropy generation.
 */
int
plat_hold_page(pfn_t pfn, int lock, page_t **pp_ret)
{
	if (lock == PLAT_HOLD_LOCK) {
		page_t	*pp = page_numtopp_nolock(pfn);

		if (pp == NULL || page_trylock(pp, SE_EXCL) == 0) {
			return (PLAT_HOLD_FAIL);
		}

		/* Do not touch DMA buffer. */
		if (PP_ARM_ISDMA(pp)) {
			DBG_PROBE1(hold__page__fail, page_t *, pp);
			page_unlock(pp);
			return (PLAT_HOLD_FAIL);
		}

		ASSERT(pp_ret != NULL);
		*pp_ret = pp;
	}

	return PLAT_HOLD_OK;
}

/*
 * void
 * plat_release_page(page_t *pp)
 *	Release page held by plat_hold_page().
 */
void
plat_release_page(page_t *pp)
{
	ASSERT(pp != NULL);
	ASSERT(PAGE_LOCKED(pp));

	page_unlock(pp);
}

/*
 * No special method for mem driver needs to be implemented.
 */

/*ARGSUSED*/
int
plat_mem_do_mmio(struct uio *uio, enum uio_rw rw)
{
	return ENOTSUP;
}

/*
 * Platform-dependant system dump routines
 */
int
dump_plat_addr()
{
	return 0;
}

void
dump_plat_pfn()
{
}

/* ARGSUSED */
int
dump_plat_data(void *dump_cdata)
{
	return 0;
}
