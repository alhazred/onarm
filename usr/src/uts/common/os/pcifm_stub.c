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

#pragma ident	"@(#)pcifm_stub.c"

#include <sys/types.h>
#include <sys/sunndi.h>
#include <sys/sysmacros.h>
#include <sys/ddifm_impl.h>
#include <sys/fm/util.h>
#include <sys/fm/protocol.h>
#include <sys/fm/io/pci.h>
#include <sys/fm/io/ddi.h>
#include <sys/pci.h>
#include <sys/pcie.h>
#include <sys/pci_impl.h>
#include <sys/epm.h>
#include <sys/pcifm.h>

uint32_t pcie_expected_ue_mask = 0x0;

void
pci_ereport_setup(dev_info_t *dip)
{
}

void
pci_ereport_teardown(dev_info_t *dip)
{
}

void
pci_ereport_post(dev_info_t *dip, ddi_fm_error_t *derr, uint16_t *xx_status)
{
}
