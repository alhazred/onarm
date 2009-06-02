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

#ifndef _SATA_HBA_LOCAL_H
#define	_SATA_HBA_LOCAL_H


#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/sata/sata_defs.h>
#ifdef __arm
#include <sys/archsystm.h>
#endif /* __arm */

/*
 * Controller's features support flags (sata_tran_hba_features_support).
 * Note: SATA_CTLF_NCQ indicates that SATA controller supports NCQ in addition
 * to legacy queuing commands, indicated by SATA_CTLF_QCMD flag.
 */

#define	SATA_CTLF_DMA			0x0   /* DMA support */
#define	SATA_CTLF_PIO			0x200 /* PIO support */
#define	SATA_CTLF_DEFAULT		SATA_CTLF_NCQ /* normal support type */

#ifdef __arm
/*
 * SATA_BCOPY macro wrapping FAST_BCOPY.
 * Note: Only available in arm architecture.
 */
#define SATA_BCOPY(from, to, size)	FAST_BCOPY(from, to, size)
#endif /* __arm */

#ifdef	__cplusplus
}
#endif

#endif /* _SATA_HBA__LOCAL_H */
