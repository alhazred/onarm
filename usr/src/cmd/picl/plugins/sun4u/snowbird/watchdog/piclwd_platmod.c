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

#pragma ident	"@(#)piclwd_platmod.c	1.2	05/06/08 SMI"

#include <picl.h>
#include <picltree.h>
#include <picldefs.h>
#include <string.h>
#include <syslog.h>
#include "piclwatchdog.h"

#define	NETRA_CP2300		"SUNW,Netra-CP2300"

int
wd_get_chassis_type()
{
	picl_nodehdl_t	chassis_h;
	char chassis_type[PICL_PROPNAMELEN_MAX];

	if (ptree_get_node_by_path(PICL_FRUTREE_CHASSIS,
		&chassis_h) != PICL_SUCCESS) {
		return (-1);
	}

	if (ptree_get_propval_by_name(chassis_h, PICL_PROP_CHASSIS_TYPE,
		chassis_type, sizeof (chassis_type)) != PICL_SUCCESS) {
		return (-1);
	}

	if (strcmp(chassis_type, NETRA_CP2300) == 0) {
		return (WD_STANDALONE);
	} else {
		return (-1);
	}
}
