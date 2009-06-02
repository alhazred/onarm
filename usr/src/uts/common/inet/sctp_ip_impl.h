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
/*
 * Copyright (c) 2008 NEC Corporation
 */

#ifndef _INET_SCTP_IP_IMPL_H
#define	_INET_SCTP_IP_IMPL_H

#pragma ident	"@(#)sctp_ip_impl.h"

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef SCTP_SHRINK

#undef SCTP_EXTRACT_IPINFO

#define SCTP_EXTRACT_IPINFO(mp, ire)

#define sctp_update_ill(x, y)
#define sctp_update_ipif(x, y)
#define sctp_move_ipif(x, y, z)
#define sctp_move_ipif(x, y, z)
#define sctp_update_ipif_addr(x, y)
#define sctp_ill_reindex(x, y)
#define sctp_ire_cache_flush(x)

#define sctp_free(x)

#define sctp_ddi_g_init()
#define sctp_ddi_g_destroy()

#endif /* SCTP_SHRINK */

#ifdef  __cplusplus
}
#endif

#endif  /* _INET_SCTP_IP_IMPL_H */
