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

#pragma ident	"@(#)ddifm_stub.c"

#include <sys/types.h>
#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/kmem.h>
#include <sys/nvpair.h>
#include <sys/fm/protocol.h>
#include <sys/ndifm.h>
#include <sys/ddifm.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi_isa.h>
#include <sys/spl.h>
#include <sys/varargs.h>
#include <sys/systm.h>
#include <sys/disp.h>
#include <sys/atomic.h>
#include <sys/errorq_impl.h>
#include <sys/kobj.h>
#include <sys/fm/util.h>
#include <sys/fm/io/ddi.h>

void
ddi_fm_service_impact(dev_info_t *dip, int svc_impact)
{
}

void
i_ddi_drv_ereport_post(dev_info_t *dip, const char *error_class,
    nvlist_t *errp, int sflag)
{
}

void
ddi_fm_ereport_post(dev_info_t *dip, const char *error_class, uint64_t ena,
    int sflag, ...)
{
}

void
i_ddi_fm_handler_enter(dev_info_t *dip)
{
}

void
i_ddi_fm_handler_exit(dev_info_t *dip)
{
}

void
ddi_fm_handler_register(dev_info_t *dip, ddi_err_func_t handler,
    void *impl_data)
{
}

void
ddi_fm_handler_unregister(dev_info_t *dip)
{
}

void
ddi_fm_init(dev_info_t *dip, int *fmcap, ddi_iblock_cookie_t *ibcp)
{
	*fmcap = DDI_FM_NOT_CAPABLE;
}

void
ddi_fm_fini(dev_info_t *dip)
{
}

int
ddi_fm_capable(dev_info_t *dip)
{
	return DDI_FM_NOT_CAPABLE;
}

#define	DDI_FM_SETOK(de)				\
	do {						\
		(de)->fme_status = DDI_FM_OK;		\
		(de)->fme_ena = 0;			\
		(de)->fme_flag = DDI_FM_ERR_UNEXPECTED;	\
	} while (0)

void
ddi_fm_acc_err_get(ddi_acc_handle_t handle, ddi_fm_error_t *de, int version)
{
	DDI_FM_SETOK(de);
	de->fme_acc_handle = handle;
}

void
ddi_fm_dma_err_get(ddi_dma_handle_t handle, ddi_fm_error_t *de, int version)
{
	DDI_FM_SETOK(de);
	de->fme_dma_handle = handle;
}

void
ddi_fm_acc_err_clear(ddi_acc_handle_t handle, int version)
{
}

void
ddi_fm_dma_err_clear(ddi_dma_handle_t handle, int version)
{
}

void
i_ddi_fm_acc_err_set(ddi_acc_handle_t handle, uint64_t ena, int status,
    int flag)
{
}

void
i_ddi_fm_dma_err_set(ddi_dma_handle_t handle, uint64_t ena, int status,
    int flag)
{
}

ddi_fmcompare_t
i_ddi_fm_acc_err_cf_get(ddi_acc_handle_t handle)
{
	ddi_acc_impl_t *i_hdlp = (ddi_acc_impl_t *)handle;

	return (i_hdlp->ahi_err->err_cf);
}

ddi_fmcompare_t
i_ddi_fm_dma_err_cf_get(ddi_dma_handle_t handle)
{
	ddi_dma_impl_t *hdlp = (ddi_dma_impl_t *)handle;

	return (hdlp->dmai_error.err_cf);
}
