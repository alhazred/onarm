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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2008 NEC Corporation
 */

#pragma ident	"@(#)lconstants.c	1.11	05/06/08 SMI"

/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/


#pragma weak lzero = _lzero
#pragma weak lone = _lone
#pragma weak lten = _lten

#include	"synonyms.h"
#include	<sys/types.h>
#include	<sys/dl.h>

#ifdef _LONG_LONG_LTOH
dl_t	lzero	= {0,  0};
dl_t	lone	= {1,  0};
dl_t	lten	= {10, 0};
#else
dl_t	lzero	= {0,  0};
dl_t	lone	= {0,  1};
dl_t	lten	= {0, 10};
#endif /* _LONG_LONG_LTOH */
