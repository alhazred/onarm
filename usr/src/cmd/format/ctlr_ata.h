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
 * Copyright 1998-2002 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007 NEC Corporation
 */

#ifndef	_CTLR_ATA_H
#define	_CTLR_ATA_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/buf.h>


/*
 * Rounded parameter, as returned in Extended Sense information
 */
#define	ROUNDED_PARAMETER	0x37


/*
 * Convert a three-byte triplet into an int
 */
#define	TRIPLET(u, m, l)	((int)((((u))&0xff<<16) + \
				(((m)&0xff)<<8) + (l&0xff)))
#if	defined(i386) || defined(__arm)
daddr_t	altsec_offset;		/* Alternate sector offset */
#endif	/* defined(i386) || defined(__arm) */

#ifdef	__STDC__
/*
 *	Local prototypes for ANSI C compilers
 */

#if	defined(i386) || defined(__arm)
int	ata_rdwr(int, int, diskaddr_t, int, caddr_t, int, int *);
#else	/* defined(i386) || defined(__arm) */
static int	ata_rdwr(int, int, diskaddr_t, int, caddr_t, int, int *);
#endif	/* defined(i386) || defined(__arm) */

int	ata_ex_man(struct defect_list *);
int	ata_ex_grown(struct defect_list *);
int	ata_read_defect_data(struct defect_list *, int);
int	apply_chg_list(int, int, uchar_t *, uchar_t *, struct chg_list *);

#else /* ! _STDC_ */

#if	defined(i386) || defined(__arm)
int	ata_rdwr();
int	ata_ex_cur();
#else	/* defined(i386) || defined(__arm) */
static int	ata_rdwr();
static int	ata_ex_cur();
#endif	/* defined(i386) || defined(__arm) */

int	ata_ck_format();
int	ata_ex_man();
int	ata_ex_grown();
int	ata_read_defect_data();
int	apply_chg_list();

#endif	/* __STDC__ */

#ifdef	__cplusplus
}
#endif

#endif	/* _CTLR_ATA_H */
