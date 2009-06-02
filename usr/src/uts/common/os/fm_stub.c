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
 * Copyright (c) 2007 NEC Corporation
 */

#pragma ident	"@(#)fm_stub.c"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/sysevent.h>
#include <sys/sysevent_impl.h>
#include <sys/nvpair.h>
#include <sys/cmn_err.h>
#include <sys/cpuvar.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/ddifm.h>
#include <sys/ddifm_impl.h>
#include <sys/spl.h>
#include <sys/dumphdr.h>
#include <sys/compress.h>
#include <sys/cpuvar.h>
#include <sys/console.h>
#include <sys/panic.h>
#include <sys/kobj.h>
#include <sys/sunddi.h>
#include <sys/systeminfo.h>
#include <sys/sysevent/eventdefs.h>
#include <sys/fm/util.h>
#include <sys/fm/protocol.h>

void
fm_init(void)
{
}

void
fm_nvprint(nvlist_t *nvl)
{
}

void
fm_panic(const char *format, ...)
{
	panic(format);
}

void
fm_banner(void)
{
}

void
fm_ereport_dump(void)
{
}

void
fm_ereport_post(nvlist_t *ereport, int evc_flag)
{
}

nv_alloc_t *
fm_nva_xcreate(char *buf, size_t bufsz)
{
	return NULL;
}

void
fm_nva_xdestroy(nv_alloc_t *nva)
{
}

nvlist_t *
fm_nvlist_create(nv_alloc_t *nva)
{
	return NULL;
}

void
fm_nvlist_destroy(nvlist_t *nvl, int flag)
{
}

int
i_fm_payload_set(nvlist_t *payload, const char *name, va_list ap)
{
	return ENOTSUP;
}

void
fm_payload_set(nvlist_t *payload, ...)
{
}

void
fm_ereport_set(nvlist_t *ereport, int version, const char *erpt_class,
    uint64_t ena, const nvlist_t *detector, ...)
{
}

void
fm_fmri_hc_set(nvlist_t *fmri, int version, const nvlist_t *auth,
    nvlist_t *snvl, int npairs, ...)
{
}

void
fm_fmri_dev_set(nvlist_t *fmri_dev, int version, const nvlist_t *auth,
    const char *devpath, const char *devid)
{
}

void
fm_fmri_cpu_set(nvlist_t *fmri_cpu, int version, const nvlist_t *auth,
    uint32_t cpu_id, uint8_t *cpu_maskp, const char *serial_idp)
{
}

void
fm_fmri_mem_set(nvlist_t *fmri, int version, const nvlist_t *auth,
    const char *unum, const char *serial, uint64_t offset)
{
}

void
fm_fmri_zfs_set(nvlist_t *fmri, int version, uint64_t pool_guid,
    uint64_t vdev_guid)
{
}

uint64_t
fm_ena_increment(uint64_t ena)
{
	return 0;
}

uint64_t
fm_ena_generate_cpu(uint64_t timestamp, processorid_t cpuid, uchar_t format)
{
	return 0;
}

uint64_t
fm_ena_generate(uint64_t timestamp, uchar_t format)
{
	return 0;
}

uint64_t
fm_ena_generation_get(uint64_t ena)
{
	return 0;
}

uchar_t
fm_ena_format_get(uint64_t ena)
{

	return (ENA_FORMAT(ena));
}

uint64_t
fm_ena_id_get(uint64_t ena)
{
	return 0;
}

uint64_t
fm_ena_time_get(uint64_t ena)
{
	return 0;
}

void
fm_payload_stack_add(nvlist_t *payload, const pc_t *stack, int depth)
{
}
