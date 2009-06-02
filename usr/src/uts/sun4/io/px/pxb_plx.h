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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SYS_PXB_PLX_H
#define	_SYS_PXB_PLX_H

#pragma ident	"@(#)pxb_plx.h	1.1	07/05/15 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

/* PLX Vendor/Device IDs */
#define	PXB_VENDOR_PLX		0x10B5
#define	PXB_DEVICE_PLX_8516	0x8516
#define	PXB_DEVICE_PLX_8532	0x8532
#define	PXB_DEVICE_PLX_8533	0x8533
#define	PXB_DEVICE_PLX_8548	0x8548

#define	PXB_VENDOR_SUN		0x108E
#define	PXB_DEVICE_PLX_PCIX	0x9010
#define	PXB_DEVICE_PLX_PCIE	0x9020

/* Last known bad rev for MSI and other issues */
#define	PXB_DEVICE_PLX_AA_REV	0xAA

/* Register offsets and bits specific to the 8548 and 8533 */
#define	PLX_EGRESS_PERFCTR_OFFSET	0x1F0
#define	PLX_RO_MODE_BIT			0x100000

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_PXB_PLX_H */
