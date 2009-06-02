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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007-2008 NEC Corporation
 */

#pragma ident	"@(#)ndifm_stub.c"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/debug.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ddi.h>
#include <sys/ndi_impldefs.h>
#include <sys/devctl.h>
#include <sys/nvpair.h>
#include <sys/ddifm.h>
#include <sys/ndifm.h>
#include <sys/spl.h>
#include <sys/sysmacros.h>
#include <sys/devops.h>
#include <sys/atomic.h>
#include <sys/fm/io/ddi.h>

void
i_ndi_fmc_create(ndi_fmc_t **fcpp, int qlen, ddi_iblock_cookie_t ibc)
{
}

void
i_ndi_fmc_destroy(ndi_fmc_t *fcp)
{
}

void
ndi_fmc_insert(dev_info_t *dip, int flag, void *resource, void *bus_specific)
{
}

void
ndi_fmc_remove(dev_info_t *dip, int flag, const void *resource)
{
}

int
ndi_fmc_entry_error(dev_info_t *dip, int flag, ddi_fm_error_t *derr,
    const void *bus_err_state)
{
	return DDI_FM_UNKNOWN;
}

int
ndi_fmc_error(dev_info_t *dip, dev_info_t *tdip, int flag, uint64_t ena,
    const void *bus_err_state)
{
	return DDI_FM_UNKNOWN;
}

int
ndi_fmc_entry_error_all(dev_info_t *dip, int flag, ddi_fm_error_t *derr)
{
	return DDI_FM_UNKNOWN;
}

int
ndi_fm_handler_dispatch(dev_info_t *dip, dev_info_t *tdip,
    const ddi_fm_error_t *nerr)
{
	return DDI_FM_UNKNOWN;
}

void
ndi_fm_acc_err_set(ddi_acc_handle_t handle, ddi_fm_error_t *dfe)
{
}

void
ndi_fm_dma_err_set(ddi_dma_handle_t handle, ddi_fm_error_t *dfe)
{
}

int
i_ndi_busop_fm_init(dev_info_t *dip, int tcap, ddi_iblock_cookie_t *ibc)
{
	return DDI_FM_NOT_CAPABLE;
}

void
i_ndi_busop_fm_fini(dev_info_t *dip)
{
}

void
i_ndi_busop_access_enter(dev_info_t *dip, ddi_acc_handle_t handle)
{
}

void
i_ndi_busop_access_exit(dev_info_t *dip, ddi_acc_handle_t handle)
{
}
