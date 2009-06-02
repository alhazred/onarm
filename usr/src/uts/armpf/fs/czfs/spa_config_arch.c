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

#pragma ident	"@(#)spa_config_arch.c"

#include <sys/types.h>
#include <sys/byteorder.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <sys/nvpair.h>
#include <sys/bootconf.h>
#include <sys/sunddi.h>
#include <sys/fs/zfs.h>
#include <sys/czfs_poolcfg.h>
#include <zfs_types.h>

#include "zfs_subr.h"

int
spa_config_load_arch(nvlist_t **nvlist)
{
	int		err;
	nvlist_t	*cnvl;
	nvlist_t	*ccnvl;
	nvlist_t	*cccnvl[1];
	char		*bufp;
	size_t		buflen;
	char		*czfs_path;
	char		*pool_name;
	char		*czfs_guid;
	uint64_t	guid1, guid2;
	char		*slashp;

	if (strcmp(rootfs.bo_fstype, "czfs") == 0) {
		czfs_path = rootfs.bo_name;
	} else if (ddi_prop_lookup_string(DDI_DEV_T_ANY, ddi_root_node(),
	    DDI_PROP_DONTPASS, "czfs-path", &czfs_path) != DDI_SUCCESS) {
		czfs_path = CZFS_DEFAULT_PATH;
	}

	if (ddi_prop_lookup_string(DDI_DEV_T_ANY, ddi_root_node(),
	    DDI_PROP_DONTPASS, ZFS_BOOTFS, &pool_name) != DDI_SUCCESS) {
		pool_name = CZFS_DEFAULT_POOL;
	} else {
		slashp = strchr(pool_name, '/');
		if (slashp != NULL) {
			*slashp = '\0';
		}
	}
	
	if (ddi_prop_lookup_string(DDI_DEV_T_ANY, ddi_root_node(),
	    DDI_PROP_DONTPASS, "czfs-guid", &czfs_guid) != DDI_SUCCESS ||
	    zfs_guid_str_to_uint64(czfs_guid, &guid1, &guid2) != 0) {
		guid1 = CZFS_DEFAULT_GUID1;
		guid2 = CZFS_DEFAULT_GUID2;
	}

	nvlist_alloc(nvlist, NV_UNIQUE_NAME, KM_SLEEP);
	nvlist_alloc(&cnvl, NV_UNIQUE_NAME, KM_SLEEP);
	nvlist_alloc(&ccnvl, NV_UNIQUE_NAME, KM_SLEEP);
	nvlist_alloc(&cccnvl[0], NV_UNIQUE_NAME, KM_SLEEP);

	nvlist_add_uint64(cnvl, "version", SPA_VERSION);
	nvlist_add_string(cnvl, "name", pool_name);
	nvlist_add_uint64(cnvl, "state", 0ULL);
	nvlist_add_txg(cnvl, "txg", 4);
	nvlist_add_uint64(cnvl, "pool_guid", guid1);

	nvlist_add_string(ccnvl, "type", "root");
	nvlist_add_uint64(ccnvl, "id", 0ULL);
	nvlist_add_uint64(ccnvl, "guid", guid1);

	nvlist_add_string(cccnvl[0], "type", "disk");
	nvlist_add_uint64(cccnvl[0], "id", 0ULL);
	nvlist_add_uint64(cccnvl[0], "guid", guid2);
	nvlist_add_string(cccnvl[0], "path", czfs_path);
	nvlist_add_string(cccnvl[0], "phys_path", czfs_path);
	nvlist_add_uint64(cccnvl[0], "whole_disk", 0ULL);

	nvlist_add_nvlist_array(ccnvl, "children", cccnvl, 1);

	nvlist_add_nvlist(cnvl, "vdev_tree", ccnvl);

	nvlist_add_nvlist(*nvlist, pool_name, cnvl);

	return (0);
}
