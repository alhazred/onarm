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

/*
 * DSL permissions are stored in a two level zap attribute
 * mechanism.   The first level identifies the "class" of
 * entry.  The class is identified by the first 2 letters of
 * the attribute.  The second letter "l" or "d" identifies whether
 * it is a local or descendent permission.  The first letter
 * identifies the type of entry.
 *
 * ul$<id>    identifies permissions granted locally for this userid.
 * ud$<id>    identifies permissions granted on descendent datasets for
 *            this userid.
 * Ul$<id>    identifies permission sets granted locally for this userid.
 * Ud$<id>    identifies permission sets granted on descendent datasets for
 *            this userid.
 * gl$<id>    identifies permissions granted locally for this groupid.
 * gd$<id>    identifies permissions granted on descendent datasets for
 *            this groupid.
 * Gl$<id>    identifies permission sets granted locally for this groupid.
 * Gd$<id>    identifies permission sets granted on descendent datasets for
 *            this groupid.
 * el$        identifies permissions granted locally for everyone.
 * ed$        identifies permissions granted on descendent datasets
 *            for everyone.
 * El$        identifies permission sets granted locally for everyone.
 * Ed$        identifies permission sets granted to descendent datasets for
 *            everyone.
 * c-$        identifies permission to create at dataset creation time.
 * C-$        identifies permission sets to grant locally at dataset creation
 *            time.
 * s-$@<name> permissions defined in specified set @<name>
 * S-$@<name> Sets defined in named set @<name>
 *
 * Each of the above entities points to another zap attribute that contains one
 * attribute for each allowed permission, such as create, destroy,...
 * All of the "upper" case class types will specify permission set names
 * rather than permissions.
 *
 * Basically it looks something like this:
 * ul$12 -> ZAP OBJ -> permissions...
 *
 * The ZAP OBJ is referred to as the jump object.
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/dmu.h>
#include <sys/dmu_objset.h>
#include <sys/dmu_tx.h>
#include <sys/dsl_dataset.h>
#include <sys/dsl_dir.h>
#include <sys/dsl_prop.h>
#include <sys/dsl_synctask.h>
#include <sys/dsl_deleg.h>
#include <sys/spa.h>
#include <sys/spa_impl.h>
#include <sys/zio_checksum.h> /* for the default checksum value */
#include <sys/zap.h>
#include <sys/fs/zfs.h>
#include <sys/cred.h>
#include <sys/sunddi.h>
#include <zfs_types.h>

#include "zfs_deleg.h"

/*
 * Validate that user is allowed to delegate specified permissions.
 *
 * In order to delegate "create" you must have "create"
 * and "allow".
 */
int
dsl_deleg_can_allow(char *ddname, nvlist_t *nvp, cred_t *cr)
{
	nvpair_t *whopair = NULL;
	int error;

	if ((error = dsl_deleg_access(ddname, ZFS_DELEG_PERM_ALLOW, cr)) != 0)
		return (error);

	while (whopair = nvlist_next_nvpair(nvp, whopair)) {
		nvlist_t *perms;
		nvpair_t *permpair = NULL;

		VERIFY(nvpair_value_nvlist(whopair, &perms) == 0);

		while (permpair = nvlist_next_nvpair(perms, permpair)) {
			const char *perm = nvpair_name(permpair);

			if (strcmp(perm, ZFS_DELEG_PERM_ALLOW) == 0)
				return (EPERM);

			if ((error = dsl_deleg_access(ddname, perm, cr)) != 0)
				return (error);
		}
	}
	return (0);
}

/*
 * Validate that user is allowed to unallow specified permissions.  They
 * must have the 'allow' permission, and even then can only unallow
 * perms for their uid.
 */
int
dsl_deleg_can_unallow(char *ddname, nvlist_t *nvp, cred_t *cr)
{
	nvpair_t *whopair = NULL;
	int error;
	char idstr[32];

	if ((error = dsl_deleg_access(ddname, ZFS_DELEG_PERM_ALLOW, cr)) != 0)
		return (error);

	(void) snprintf(idstr, sizeof (idstr), "%lld",
	    (longlong_t)crgetuid(cr));

	while (whopair = nvlist_next_nvpair(nvp, whopair)) {
		zfs_deleg_who_type_t type = nvpair_name(whopair)[0];

		if (type != ZFS_DELEG_USER &&
		    type != ZFS_DELEG_USER_SETS)
			return (EPERM);

		if (strcmp(idstr, &nvpair_name(whopair)[3]) != 0)
			return (EPERM);
	}
	return (0);
}

static void
dsl_deleg_set_sync(void *arg1, void *arg2, cred_t *cr, dmu_tx_t *tx)
{
	dsl_dir_t *dd = arg1;
	nvlist_t *nvp = arg2;
	objset_t *mos = dd->dd_pool->dp_meta_objset;
	nvpair_t *whopair = NULL;
	objid_t zapobj = dd->dd_phys->dd_deleg_zapobj;

	if (zapobj == 0) {
		dmu_buf_will_dirty(dd->dd_dbuf, tx);
		zapobj = dd->dd_phys->dd_deleg_zapobj = zap_create(mos,
		    DMU_OT_DSL_PERMS, DMU_OT_NONE, 0, tx);
	}

	while (whopair = nvlist_next_nvpair(nvp, whopair)) {
		const char *whokey = nvpair_name(whopair);
		nvlist_t *perms;
		nvpair_t *permpair = NULL;
		objid_t jumpobj;

		VERIFY(nvpair_value_nvlist(whopair, &perms) == 0);

		if (zap_lookup(mos, zapobj, whokey, sizeof (jumpobj),
		    1, &jumpobj) != 0) {
			jumpobj = zap_create(mos, DMU_OT_DSL_PERMS,
			    DMU_OT_NONE, 0, tx);
			VERIFY(zap_update(mos, zapobj,
			    whokey, sizeof (jumpobj), 1, &jumpobj, tx) == 0);
		}

		while (permpair = nvlist_next_nvpair(perms, permpair)) {
			const char *perm = nvpair_name(permpair);
			uint64_t n = 0;

			VERIFY(zap_update(mos, jumpobj,
			    perm, 8, 1, &n, tx) == 0);
			spa_history_internal_log(LOG_DS_PERM_UPDATE,
			    dd->dd_pool->dp_spa, tx, cr,
			    "%s %s dataset = %" PRIuOBJID, whokey, perm,
			    dd->dd_phys->dd_head_dataset_obj);
		}
	}
}

static void
dsl_deleg_unset_sync(void *arg1, void *arg2, cred_t *cr, dmu_tx_t *tx)
{
	dsl_dir_t *dd = arg1;
	nvlist_t *nvp = arg2;
	objset_t *mos = dd->dd_pool->dp_meta_objset;
	nvpair_t *whopair = NULL;
	objid_t zapobj = dd->dd_phys->dd_deleg_zapobj;

	if (zapobj == 0)
		return;

	while (whopair = nvlist_next_nvpair(nvp, whopair)) {
		const char *whokey = nvpair_name(whopair);
		nvlist_t *perms;
		nvpair_t *permpair = NULL;
		objid_t jumpobj;

		if (nvpair_value_nvlist(whopair, &perms) != 0) {
			if (zap_lookup(mos, zapobj, whokey, sizeof (jumpobj),
			    1, &jumpobj) == 0) {
				(void) zap_remove(mos, zapobj, whokey, tx);
				VERIFY(0 == zap_destroy(mos, jumpobj, tx));
			}
			spa_history_internal_log(LOG_DS_PERM_WHO_REMOVE,
			    dd->dd_pool->dp_spa, tx, cr,
			    "%s dataset = %" PRIuOBJID, whokey,
			    dd->dd_phys->dd_head_dataset_obj);
			continue;
		}

		if (zap_lookup(mos, zapobj, whokey, sizeof (jumpobj),
		    1, &jumpobj) != 0)
			continue;

		while (permpair = nvlist_next_nvpair(perms, permpair)) {
			const char *perm = nvpair_name(permpair);
			uint64_t n = 0;

			(void) zap_remove(mos, jumpobj, perm, tx);
			if (zap_count(mos, jumpobj, &n) == 0 && n == 0) {
				(void) zap_remove(mos, zapobj,
				    whokey, tx);
				VERIFY(0 == zap_destroy(mos,
				    jumpobj, tx));
			}
			spa_history_internal_log(LOG_DS_PERM_REMOVE,
			    dd->dd_pool->dp_spa, tx, cr,
			    "%s %s dataset = %" PRIuOBJID, whokey, perm,
			    dd->dd_phys->dd_head_dataset_obj);
		}
	}
}

int
dsl_deleg_set(const char *ddname, nvlist_t *nvp, boolean_t unset)
{
	dsl_dir_t *dd;
	int error;
	nvpair_t *whopair = NULL;
	int blocks_modified = 0;

	error = dsl_dir_open(ddname, FTAG, &dd, NULL);
	if (error)
		return (error);

	if (spa_version(dmu_objset_spa(dd->dd_pool->dp_meta_objset)) <
	    SPA_VERSION_DELEGATED_PERMS) {
		dsl_dir_close(dd, FTAG);
		return (ENOTSUP);
	}

	while (whopair = nvlist_next_nvpair(nvp, whopair))
		blocks_modified++;

	error = dsl_sync_task_do(dd->dd_pool, NULL,
	    unset ? dsl_deleg_unset_sync : dsl_deleg_set_sync,
	    dd, nvp, blocks_modified);
	dsl_dir_close(dd, FTAG);

	return (error);
}

/*
 * Find all 'allow' permissions from a given point and then continue
 * traversing up to the root.
 *
 * This function constructs an nvlist of nvlists.
 * each setpoint is an nvlist composed of an nvlist of an nvlist
 * of the individual * users/groups/everyone/create
 * permissions.
 *
 * The nvlist will look like this.
 *
 * { source fsname -> { whokeys { permissions,...}, ...}}
 *
 * The fsname nvpairs will be arranged in a bottom up order.  For example,
 * if we have the following structure a/b/c then the nvpairs for the fsnames
 * will be ordered a/b/c, a/b, a.
 */
int
dsl_deleg_get(const char *ddname, nvlist_t **nvp)
{
	dsl_dir_t *dd, *startdd;
	dsl_pool_t *dp;
	int error;
	objset_t *mos;

	error = dsl_dir_open(ddname, FTAG, &startdd, NULL);
	if (error)
		return (error);

	dp = startdd->dd_pool;
	mos = dp->dp_meta_objset;

	VERIFY(nvlist_alloc(nvp, NV_UNIQUE_NAME, KM_SLEEP) == 0);

	if (spa_state(dp->dp_spa) == POOL_STATE_IO_FAILURE &&
	    spa_get_failmode(dp->dp_spa) == ZIO_FAILURE_MODE_ABORT) {
		dsl_dir_close(startdd, FTAG);
		return (EIO);
	}

	rw_enter(&dp->dp_config_rwlock, RW_READER);
	for (dd = startdd; dd != NULL; dd = dd->dd_parent) {
		zap_cursor_t basezc;
		zap_attribute_t baseza;
		nvlist_t *sp_nvp;
		uint64_t n;
		char source[MAXNAMELEN];

		if (dd->dd_phys->dd_deleg_zapobj &&
		    (zap_count(mos, dd->dd_phys->dd_deleg_zapobj,
		    &n) == 0) && n) {
			VERIFY(nvlist_alloc(&sp_nvp,
			    NV_UNIQUE_NAME, KM_SLEEP) == 0);
		} else {
			continue;
		}

		for (zap_cursor_init(&basezc, mos,
		    dd->dd_phys->dd_deleg_zapobj);
		    zap_cursor_retrieve(&basezc, &baseza) == 0;
		    zap_cursor_advance(&basezc)) {
			zap_cursor_t zc;
			zap_attribute_t za;
			nvlist_t *perms_nvp;

			ASSERT(baseza.za_integer_length ==
			    sizeof (dd->dd_phys->dd_deleg_zapobj));
			ASSERT(baseza.za_num_integers == 1);

			VERIFY(nvlist_alloc(&perms_nvp,
			    NV_UNIQUE_NAME, KM_SLEEP) == 0);
			for (zap_cursor_init(&zc, mos, baseza.za_first_integer);
			    zap_cursor_retrieve(&zc, &za) == 0;
			    zap_cursor_advance(&zc)) {
				VERIFY(nvlist_add_boolean(perms_nvp,
				    za.za_name) == 0);
			}
			zap_cursor_fini(&zc);
			VERIFY(nvlist_add_nvlist(sp_nvp, baseza.za_name,
			    perms_nvp) == 0);
			nvlist_free(perms_nvp);
		}

		zap_cursor_fini(&basezc);

		dsl_dir_name(dd, source);
		VERIFY(nvlist_add_nvlist(*nvp, source, sp_nvp) == 0);
		nvlist_free(sp_nvp);
	}
	rw_exit(&dp->dp_config_rwlock);

	dsl_dir_close(startdd, FTAG);
	return (0);
}

/*
 * Routines for dsl_deleg_access() -- access checking.
 */
typedef struct perm_set {
	avl_node_t	p_node;
	boolean_t	p_matched;
	char		p_setname[ZFS_MAX_DELEG_NAME];
} perm_set_t;

static int
perm_set_compare(const void *arg1, const void *arg2)
{
	const perm_set_t *node1 = arg1;
	const perm_set_t *node2 = arg2;
	int val;

	val = strcmp(node1->p_setname, node2->p_setname);
	if (val == 0)
		return (0);
	return (val > 0 ? 1 : -1);
}

/*
 * Determine whether a specified permission exists.
 *
 * First the base attribute has to be retrieved.  i.e. ul$12
 * Once the base object has been retrieved the actual permission
 * is lookup up in the zap object the base object points to.
 *
 * Return 0 if permission exists, ENOENT if there is no whokey, EPERM if
 * there is no perm in that jumpobj.
 */
static int
dsl_check_access(objset_t *mos, objid_t zapobj,
    char type, char checkflag, void *valp, const char *perm)
{
	int error;
	objid_t jumpobj;
	uint64_t zero;
	char whokey[ZFS_MAX_DELEG_NAME];

	zfs_deleg_whokey(whokey, type, checkflag, valp);
	error = zap_lookup(mos, zapobj, whokey, sizeof (jumpobj), 1, &jumpobj);
	if (error == 0) {
		error = zap_lookup(mos, jumpobj, perm, 8, 1, &zero);
		if (error == ENOENT)
			error = EPERM;
	}
	return (error);
}

/*
 * check a specified user/group for a requested permission
 */
static int
dsl_check_user_access(objset_t *mos, objid_t zapobj, const char *perm,
    int checkflag, cred_t *cr)
{
	const	gid_t *gids;
	int	ngids;
	int	i;
	uint64_t id;

	/* check for user */
	id = crgetuid(cr);
	if (dsl_check_access(mos, zapobj,
	    ZFS_DELEG_USER, checkflag, &id, perm) == 0)
		return (0);

	/* check for users primary group */
	id = crgetgid(cr);
	if (dsl_check_access(mos, zapobj,
	    ZFS_DELEG_GROUP, checkflag, &id, perm) == 0)
		return (0);

	/* check for everyone entry */
	id = -1;
	if (dsl_check_access(mos, zapobj,
	    ZFS_DELEG_EVERYONE, checkflag, &id, perm) == 0)
		return (0);

	/* check each supplemental group user is a member of */
	ngids = crgetngroups(cr);
	gids = crgetgroups(cr);
	for (i = 0; i != ngids; i++) {
		id = gids[i];
		if (dsl_check_access(mos, zapobj,
		    ZFS_DELEG_GROUP, checkflag, &id, perm) == 0)
			return (0);
	}

	return (EPERM);
}

/*
 * Iterate over the sets specified in the specified zapobj
 * and load them into the permsets avl tree.
 */
static int
dsl_load_sets(objset_t *mos, objid_t zapobj,
    char type, char checkflag, void *valp, avl_tree_t *avl)
{
	zap_cursor_t zc;
	zap_attribute_t za;
	perm_set_t *permnode;
	avl_index_t idx;
	objid_t jumpobj;
	int error;
	char whokey[ZFS_MAX_DELEG_NAME];

	zfs_deleg_whokey(whokey, type, checkflag, valp);

	error = zap_lookup(mos, zapobj, whokey, sizeof (jumpobj), 1, &jumpobj);
	if (error != 0)
		return (error);

	for (zap_cursor_init(&zc, mos, jumpobj);
	    zap_cursor_retrieve(&zc, &za) == 0;
	    zap_cursor_advance(&zc)) {
		permnode = kmem_alloc(sizeof (perm_set_t), KM_SLEEP);
		(void) strlcpy(permnode->p_setname, za.za_name,
		    sizeof (permnode->p_setname));
		permnode->p_matched = B_FALSE;

		if (avl_find(avl, permnode, &idx) == NULL) {
			avl_insert(avl, permnode, idx);
		} else {
			kmem_free(permnode, sizeof (perm_set_t));
		}
	}
	zap_cursor_fini(&zc);
	return (0);
}

/*
 * Load all permissions user based on cred belongs to.
 */
static void
dsl_load_user_sets(objset_t *mos, objid_t zapobj, avl_tree_t *avl,
    char checkflag, cred_t *cr)
{
	const	gid_t *gids;
	int	ngids, i;
	uint64_t id;

	id = crgetuid(cr);
	(void) dsl_load_sets(mos, zapobj,
	    ZFS_DELEG_USER_SETS, checkflag, &id, avl);

	id = crgetgid(cr);
	(void) dsl_load_sets(mos, zapobj,
	    ZFS_DELEG_GROUP_SETS, checkflag, &id, avl);

	(void) dsl_load_sets(mos, zapobj,
	    ZFS_DELEG_EVERYONE_SETS, checkflag, NULL, avl);

	ngids = crgetngroups(cr);
	gids = crgetgroups(cr);
	for (i = 0; i != ngids; i++) {
		id = gids[i];
		(void) dsl_load_sets(mos, zapobj,
		    ZFS_DELEG_GROUP_SETS, checkflag, &id, avl);
	}
}

/*
 * Check if user has requested permission.
 */
int
dsl_deleg_access(const char *ddname, const char *perm, cred_t *cr)
{
	dsl_dir_t *dd, *startdd;
	dsl_pool_t *dp;
	void *cookie;
	int	error;
	char	checkflag = ZFS_DELEG_LOCAL;
	const char *tail;
	objset_t *mos;
	avl_tree_t permsets;
	perm_set_t *setnode;

	/*
	 * Use tail so that zfs_ioctl() code doesn't have
	 * to always to to figure out parent name in order
	 * to do access check.  for example renaming a snapshot
	 */
	error = dsl_dir_open(ddname, FTAG, &startdd, &tail);
	if (error)
		return (error);

	if (tail && tail[0] != '@') {
		dsl_dir_close(startdd, FTAG);
		return (ENOENT);
	}
	dp = startdd->dd_pool;
	mos = dp->dp_meta_objset;

	if (dsl_delegation_on(mos) == B_FALSE) {
		dsl_dir_close(startdd, FTAG);
		return (ECANCELED);
	}

	if (spa_version(dmu_objset_spa(dp->dp_meta_objset)) <
	    SPA_VERSION_DELEGATED_PERMS) {
		dsl_dir_close(startdd, FTAG);
		return (EPERM);
	}

	avl_create(&permsets, perm_set_compare, sizeof (perm_set_t),
	    offsetof(perm_set_t, p_node));

	if (spa_state(dp->dp_spa) == POOL_STATE_IO_FAILURE &&
	    spa_get_failmode(dp->dp_spa) == ZIO_FAILURE_MODE_ABORT) {
		dsl_dir_close(startdd, FTAG);
		return (EIO);
	}

	rw_enter(&dp->dp_config_rwlock, RW_READER);
	for (dd = startdd; dd != NULL; dd = dd->dd_parent,
	    checkflag = ZFS_DELEG_DESCENDENT) {
		objid_t zapobj;
		boolean_t expanded;

		/*
		 * If not in global zone then make sure
		 * the zoned property is set
		 */
		if (!INGLOBALZONE(curproc)) {
			uint64_t zoned;

			if (dsl_prop_get_ds_locked(dd,
			    zfs_prop_to_name(ZFS_PROP_ZONED),
			    8, 1, &zoned, NULL) != 0)
				break;
			if (!zoned)
				break;
		}
		zapobj = dd->dd_phys->dd_deleg_zapobj;

		if (zapobj == 0)
			continue;

		dsl_load_user_sets(mos, zapobj, &permsets, checkflag, cr);
again:
		expanded = B_FALSE;
		for (setnode = avl_first(&permsets); setnode;
		    setnode = AVL_NEXT(&permsets, setnode)) {
			if (setnode->p_matched == B_TRUE)
				continue;

			/* See if this set directly grants this permission */
			error = dsl_check_access(mos, zapobj,
			    ZFS_DELEG_NAMED_SET, 0, setnode->p_setname, perm);
			if (error == 0)
				goto success;
			if (error == EPERM)
				setnode->p_matched = B_TRUE;

			/* See if this set includes other sets */
			error = dsl_load_sets(mos, zapobj,
			    ZFS_DELEG_NAMED_SET_SETS, 0,
			    setnode->p_setname, &permsets);
			if (error == 0)
				setnode->p_matched = expanded = B_TRUE;
		}
		/*
		 * If we expanded any sets, that will define more sets,
		 * which we need to check.
		 */
		if (expanded)
			goto again;

		error = dsl_check_user_access(mos, zapobj, perm, checkflag, cr);
		if (error == 0)
			goto success;
	}
	error = EPERM;
success:
	rw_exit(&dp->dp_config_rwlock);
	dsl_dir_close(startdd, FTAG);

	cookie = NULL;
	while ((setnode = avl_destroy_nodes(&permsets, &cookie)) != NULL)
		kmem_free(setnode, sizeof (perm_set_t));

	return (error);
}

/*
 * Other routines.
 */

static void
copy_create_perms(dsl_dir_t *dd, objid_t pzapobj,
    boolean_t dosets, uint64_t uid, dmu_tx_t *tx)
{
	objset_t *mos = dd->dd_pool->dp_meta_objset;
	objid_t jumpobj, pjumpobj;
	objid_t zapobj = dd->dd_phys->dd_deleg_zapobj;
	zap_cursor_t zc;
	zap_attribute_t za;
	char whokey[ZFS_MAX_DELEG_NAME];

	zfs_deleg_whokey(whokey,
	    dosets ? ZFS_DELEG_CREATE_SETS : ZFS_DELEG_CREATE,
	    ZFS_DELEG_LOCAL, NULL);
	if (zap_lookup(mos, pzapobj, whokey, sizeof (pjumpobj), 1,
	    &pjumpobj) != 0)
		return;

	if (zapobj == 0) {
		dmu_buf_will_dirty(dd->dd_dbuf, tx);
		zapobj = dd->dd_phys->dd_deleg_zapobj = zap_create(mos,
		    DMU_OT_DSL_PERMS, DMU_OT_NONE, 0, tx);
	}

	zfs_deleg_whokey(whokey,
	    dosets ? ZFS_DELEG_USER_SETS : ZFS_DELEG_USER,
	    ZFS_DELEG_LOCAL, &uid);
	if (zap_lookup(mos, zapobj, whokey, sizeof (jumpobj), 1,
	    &jumpobj) == ENOENT) {
		jumpobj = zap_create(mos, DMU_OT_DSL_PERMS, DMU_OT_NONE, 0, tx);
		VERIFY(zap_add(mos, zapobj, whokey, sizeof (jumpobj), 1,
		    &jumpobj, tx) == 0);
	}

	for (zap_cursor_init(&zc, mos, pjumpobj);
	    zap_cursor_retrieve(&zc, &za) == 0;
	    zap_cursor_advance(&zc)) {
		uint64_t zero = 0;
		ASSERT(za.za_integer_length == sizeof (pjumpobj) &&
		    za.za_num_integers == 1);

		VERIFY(zap_update(mos, jumpobj, za.za_name,
		    8, 1, &zero, tx) == 0);
	}
	zap_cursor_fini(&zc);
}

/*
 * set all create time permission on new dataset.
 */
void
dsl_deleg_set_create_perms(dsl_dir_t *sdd, dmu_tx_t *tx, cred_t *cr)
{
	dsl_dir_t *dd;
	uint64_t uid = crgetuid(cr);

	if (spa_version(dmu_objset_spa(sdd->dd_pool->dp_meta_objset)) <
	    SPA_VERSION_DELEGATED_PERMS)
		return;

	for (dd = sdd->dd_parent; dd != NULL; dd = dd->dd_parent) {
		objid_t pzapobj = dd->dd_phys->dd_deleg_zapobj;

		if (pzapobj == 0)
			continue;

		copy_create_perms(sdd, pzapobj, B_FALSE, uid, tx);
		copy_create_perms(sdd, pzapobj, B_TRUE, uid, tx);
	}
}

int
dsl_deleg_destroy(objset_t *mos, objid_t zapobj, dmu_tx_t *tx)
{
	zap_cursor_t zc;
	zap_attribute_t za;

	if (zapobj == 0)
		return (0);

	for (zap_cursor_init(&zc, mos, zapobj);
	    zap_cursor_retrieve(&zc, &za) == 0;
	    zap_cursor_advance(&zc)) {
		ASSERT(za.za_integer_length == sizeof (zapobj) &&
		    za.za_num_integers == 1);
		VERIFY(0 == zap_destroy(mos, za.za_first_integer, tx));
	}
	zap_cursor_fini(&zc);
	VERIFY(0 == zap_destroy(mos, zapobj, tx));
	return (0);
}

boolean_t
dsl_delegation_on(objset_t *os)
{
	return (os->os->os_spa->spa_delegation);
}
