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
 * Copyright (c) 2008 NEC Corporation
 */

#pragma ident   "@(#)timer_prdc.c"

#include <sys/avintr.h>
#include <sys/cmn_err.h>
#include <sys/sunddi.h>
#include <sys/ddi_timer.h>

/*
 * Register these software interrupts for ddi timer.
 * Software interrupts up to the level 10 are supported.
 */
void
prdc_timer_init(void)
{
	int i;

	for (i = DDI_IPL_1; i <= DDI_IPL_10; i++) { 
		char name[sizeof("timer_softintr") + 2]; 
		(void)snprintf(name, sizeof(name), "timer_softintr%02d", i); 
		(void)add_avsoftintr((void *)&softlevel_hdl[i-1], i, 
				(avfunc)timer_softintr, name, 
				(caddr_t)(uintptr_t)i, NULL); 
	} 
}
