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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * This file includes structures, macros used to communicate with linkmgmt
 * daemon.
 */

#ifndef _LIBDLMGMT_H
#define	_LIBDLMGMT_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>
#include <libdladm.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * datalink management related macros, structures.
 */

/*
 * Door call commands.
 */
#define	DLMGMT_CMD_CREATE_LINKID	(DLMGMT_CMD_BASE + 0)
#define	DLMGMT_CMD_DESTROY_LINKID	(DLMGMT_CMD_BASE + 1)
#define	DLMGMT_CMD_REMAP_LINKID		(DLMGMT_CMD_BASE + 2)
#define	DLMGMT_CMD_CREATECONF		(DLMGMT_CMD_BASE + 3)
#define	DLMGMT_CMD_READCONF		(DLMGMT_CMD_BASE + 4)
#define	DLMGMT_CMD_WRITECONF		(DLMGMT_CMD_BASE + 5)
#define	DLMGMT_CMD_UP_LINKID		(DLMGMT_CMD_BASE + 6)
#define	DLMGMT_CMD_SETATTR		(DLMGMT_CMD_BASE + 7)
#define	DLMGMT_CMD_UNSETATTR		(DLMGMT_CMD_BASE + 8)
#define	DLMGMT_CMD_REMOVECONF		(DLMGMT_CMD_BASE + 9)
#define	DLMGMT_CMD_DESTROYCONF		(DLMGMT_CMD_BASE + 10)
#define	DLMGMT_CMD_GETATTR		(DLMGMT_CMD_BASE + 11)

typedef struct dlmgmt_door_createid_s {
	int			ld_cmd;
	char			ld_link[MAXLINKNAMELEN];
	datalink_class_t	ld_class;
	uint32_t		ld_media;
	boolean_t		ld_prefix;
	uint32_t		ld_flags;
} dlmgmt_door_createid_t;

typedef struct dlmgmt_door_destroyid_s {
	int		ld_cmd;
	datalink_id_t	ld_linkid;
	uint32_t	ld_flags;
} dlmgmt_door_destroyid_t;

typedef struct dlmgmt_door_remapid_s {
	int		ld_cmd;
	datalink_id_t	ld_linkid;
	char		ld_link[MAXLINKNAMELEN];
} dlmgmt_door_remapid_t;

typedef struct dlmgmt_door_upid_s {
	int		ld_cmd;
	datalink_id_t	ld_linkid;
} dlmgmt_door_upid_t;

typedef struct dlmgmt_door_createconf_s {
	int			ld_cmd;
	char			ld_link[MAXLINKNAMELEN];
	datalink_id_t		ld_linkid;
	datalink_class_t	ld_class;
	uint32_t		ld_media;
} dlmgmt_door_createconf_t;

typedef struct dlmgmt_door_setattr_s {
	int			ld_cmd;
	dladm_conf_t		ld_conf;
	char			ld_attr[MAXLINKATTRLEN];
	size_t			ld_attrsz;
	dladm_datatype_t	ld_type;
	char			ld_attrval[1];
} dlmgmt_door_setattr_t;

typedef struct dlmgmt_door_unsetattr_s {
	int		ld_cmd;
	dladm_conf_t	ld_conf;
	char		ld_attr[MAXLINKATTRLEN];
} dlmgmt_door_unsetattr_t;

typedef struct dlmgmt_door_writeconf_s {
	int		ld_cmd;
	dladm_conf_t	ld_conf;
} dlmgmt_door_writeconf_t;

typedef struct dlmgmt_door_removeconf_s {
	int		ld_cmd;
	datalink_id_t	ld_linkid;
} dlmgmt_door_removeconf_t;

typedef struct dlmgmt_door_destroyconf_s {
	int		ld_cmd;
	dladm_conf_t	ld_conf;
} dlmgmt_door_destroyconf_t;

typedef struct dlmgmt_door_readconf_s {
	int		ld_cmd;
	datalink_id_t	ld_linkid;
} dlmgmt_door_readconf_t;

typedef struct dlmgmt_door_getattr_s {
	int		ld_cmd;
	dladm_conf_t	ld_conf;
	char		ld_attr[MAXLINKATTRLEN];
} dlmgmt_door_getattr_t;

typedef union dlmgmt_door_arg_s {
	int				ld_cmd;
	dlmgmt_upcall_arg_create_t	kcreate;
	dlmgmt_upcall_arg_destroy_t	kdestroy;
	dlmgmt_upcall_arg_getattr_t	kgetattr;
	dlmgmt_door_getlinkid_t		getlinkid;
	dlmgmt_door_getnext_t		getnext;
	dlmgmt_door_createid_t		createid;
	dlmgmt_door_destroyid_t		destroyid;
	dlmgmt_door_remapid_t		remapid;
	dlmgmt_door_upid_t		upid;
	dlmgmt_door_createconf_t	createconf;
	dlmgmt_door_getname_t		getname;
	dlmgmt_door_getattr_t		getattr;
	dlmgmt_door_setattr_t		setattr;
	dlmgmt_door_writeconf_t		writeconf;
	dlmgmt_door_removeconf_t	removeconf;
	dlmgmt_door_destroyconf_t	destroyconf;
	dlmgmt_door_readconf_t		readconf;
} dlmgmt_door_arg_t;

typedef struct dlmgmt_handle_retval_s {
	uint_t			lr_err;
	dladm_conf_t		lr_conf;
} dlmgmt_createconf_retval_t, dlmgmt_readconf_retval_t;

typedef struct dlmgmt_null_retval_s	dlmgmt_remapid_retval_t,
	dlmgmt_upid_retval_t,
	dlmgmt_destroyid_retval_t,
	dlmgmt_setattr_retval_t,
	dlmgmt_unsetattr_retval_t,
	dlmgmt_writeconf_retval_t,
	dlmgmt_removeconf_retval_t,
	dlmgmt_destroyconf_retval_t;

typedef struct dlmgmt_linkid_retval_s	dlmgmt_createid_retval_t;

typedef union dlmgmt_retval {
	uint_t				lr_err; /* return error code */
	dlmgmt_create_retval_t		kcreate;
	dlmgmt_destroy_retval_t		kdestroy;
	dlmgmt_getattr_retval_t		getattr;
	dlmgmt_getname_retval_t		getname;
	dlmgmt_getlinkid_retval_t	getlinkid;
	dlmgmt_getnext_retval_t		getnext;
	dlmgmt_createid_retval_t	createid;
	dlmgmt_destroyid_retval_t	destroyid;
	dlmgmt_remapid_retval_t		remapid;
	dlmgmt_upid_retval_t		upid;
	dlmgmt_createconf_retval_t	createconf;
	dlmgmt_readconf_retval_t	readconf;
	dlmgmt_setattr_retval_t		setattr;
	dlmgmt_writeconf_retval_t	writeconf;
	dlmgmt_removeconf_retval_t	removeconf;
	dlmgmt_destroyconf_retval_t	destroyconf;
} dlmgmt_retval_t;

#ifdef __cplusplus
}
#endif

#endif /* _LIBDLMGMT_H */
