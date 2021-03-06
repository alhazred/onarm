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

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * Service routines
 */

#include "idmapd.h"
#include "idmap_priv.h"
#include "nldaputils.h"
#include <signal.h>
#include <thread.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ucred.h>
#include <pwd.h>
#include <auth_attr.h>
#include <secdb.h>
#include <sys/u8_textprep.h>

#define	_VALIDATE_LIST_CB_DATA(col, val, siz)\
	retcode = validate_list_cb_data(cb_data, argc, argv, col,\
			(uchar_t **)val, siz);\
	if (retcode == IDMAP_NEXT) {\
		result->retcode = IDMAP_NEXT;\
		return (0);\
	} else if (retcode < 0) {\
		result->retcode = retcode;\
		return (1);\
	}

#define	PROCESS_LIST_SVC_SQL(rcode, db, dbname, sql, limit, cb, res, len)\
	rcode = process_list_svc_sql(db, dbname, sql, limit, cb, res);\
	if (rcode == IDMAP_ERR_BUSY)\
		res->retcode = IDMAP_ERR_BUSY;\
	else if (rcode == IDMAP_SUCCESS && len == 0)\
		res->retcode = IDMAP_ERR_NOTFOUND;


#define	STRDUP_OR_FAIL(to, from) \
	if ((from) == NULL) \
		to = NULL; \
	else { \
		if ((to = strdup(from)) == NULL) \
			return (1); \
	}

/* ARGSUSED */
bool_t
idmap_null_1_svc(void *result, struct svc_req *rqstp)
{
	return (TRUE);
}

/*
 * RPC layer allocates empty strings to replace NULL char *.
 * This utility function frees these empty strings.
 */
static
void
sanitize_mapping_request(idmap_mapping *req)
{
	free(req->id1name);
	req->id1name = NULL;
	free(req->id1domain);
	req->id1domain = NULL;
	free(req->id2name);
	req->id2name = NULL;
	free(req->id2domain);
	req->id2domain = NULL;
	req->direction = _IDMAP_F_DONE;
}

static
int
validate_mapped_id_by_name_req(idmap_mapping *req)
{
	int e;

	if (IS_REQUEST_UID(*req) || IS_REQUEST_GID(*req))
		return (IDMAP_SUCCESS);

	if (IS_REQUEST_SID(*req, 1)) {
		if (!EMPTY_STRING(req->id1name) &&
		    u8_validate(req->id1name, strlen(req->id1name),
		    NULL, U8_VALIDATE_ENTIRE, &e) < 0)
			return (IDMAP_ERR_BAD_UTF8);
		if (!EMPTY_STRING(req->id1domain) &&
		    u8_validate(req->id1domain, strlen(req->id1domain),
		    NULL, U8_VALIDATE_ENTIRE, &e) < 0)
			return (IDMAP_ERR_BAD_UTF8);
	}

	return (IDMAP_SUCCESS);
}

static
int
validate_rule(idmap_namerule *rule)
{
	int e;

	if (!EMPTY_STRING(rule->winname) &&
	    u8_validate(rule->winname, strlen(rule->winname),
	    NULL, U8_VALIDATE_ENTIRE, &e) < 0)
		return (IDMAP_ERR_BAD_UTF8);

	if (!EMPTY_STRING(rule->windomain) &&
	    u8_validate(rule->windomain, strlen(rule->windomain),
	    NULL, U8_VALIDATE_ENTIRE, &e) < 0)
		return (IDMAP_ERR_BAD_UTF8);

	return (IDMAP_SUCCESS);

}

static
bool_t
validate_rules(idmap_update_batch *batch)
{
	idmap_update_op	*up;
	int i;

	for (i = 0; i < batch->idmap_update_batch_len; i++) {
		up = &(batch->idmap_update_batch_val[i]);
		if (validate_rule(&(up->idmap_update_op_u.rule))
		    != IDMAP_SUCCESS)
			return (IDMAP_ERR_BAD_UTF8);
	}

	return (IDMAP_SUCCESS);
}

/* ARGSUSED */
bool_t
idmap_get_mapped_ids_1_svc(idmap_mapping_batch batch,
		idmap_ids_res *result, struct svc_req *rqstp)
{
	sqlite		*cache = NULL, *db = NULL;
	lookup_state_t	state;
	idmap_retcode	retcode;
	uint_t		i;

	/* Init */
	(void) memset(result, 0, sizeof (*result));
	(void) memset(&state, 0, sizeof (state));

	/* Return success if nothing was requested */
	if (batch.idmap_mapping_batch_len < 1)
		goto out;

	/* Get cache handle */
	result->retcode = get_cache_handle(&cache);
	if (result->retcode != IDMAP_SUCCESS)
		goto out;

	/* Get db handle */
	result->retcode = get_db_handle(&db);
	if (result->retcode != IDMAP_SUCCESS)
		goto out;

	/* Allocate result array */
	result->ids.ids_val = calloc(batch.idmap_mapping_batch_len,
	    sizeof (idmap_id_res));
	if (result->ids.ids_val == NULL) {
		idmapdlog(LOG_ERR, "Out of memory");
		result->retcode = IDMAP_ERR_MEMORY;
		goto out;
	}
	result->ids.ids_len = batch.idmap_mapping_batch_len;

	/* Allocate hash table to check for duplicate sids */
	state.sid_history = calloc(batch.idmap_mapping_batch_len,
	    sizeof (*state.sid_history));
	if (state.sid_history == NULL) {
		idmapdlog(LOG_ERR, "Out of memory");
		result->retcode = IDMAP_ERR_MEMORY;
		goto out;
	}
	state.sid_history_size = batch.idmap_mapping_batch_len;
	for (i = 0; i < state.sid_history_size; i++) {
		state.sid_history[i].key = state.sid_history_size;
		state.sid_history[i].next = state.sid_history_size;
	}
	state.batch = &batch;
	state.result = result;

	/* Get directory-based name mapping info */
	result->retcode = get_ds_namemap_type(&state);
	if (result->retcode != IDMAP_SUCCESS)
		goto out;

	/* Init our 'done' flags */
	state.sid2pid_done = state.pid2sid_done = TRUE;

	/* First stage */
	for (i = 0; i < batch.idmap_mapping_batch_len; i++) {
		state.curpos = i;
		(void) sanitize_mapping_request(
		    &batch.idmap_mapping_batch_val[i]);
		if (IS_BATCH_SID(batch, i)) {
			retcode = sid2pid_first_pass(
			    &state,
			    cache,
			    &batch.idmap_mapping_batch_val[i],
			    &result->ids.ids_val[i]);
		} else if (IS_BATCH_UID(batch, i)) {
			retcode = pid2sid_first_pass(
			    &state,
			    cache,
			    &batch.idmap_mapping_batch_val[i],
			    &result->ids.ids_val[i], 1, 0);
		} else if (IS_BATCH_GID(batch, i)) {
			retcode = pid2sid_first_pass(
			    &state,
			    cache,
			    &batch.idmap_mapping_batch_val[i],
			    &result->ids.ids_val[i], 0, 0);
		} else {
			result->ids.ids_val[i].retcode = IDMAP_ERR_IDTYPE;
			continue;
		}
		if (IDMAP_FATAL_ERROR(retcode)) {
			result->retcode = retcode;
			goto out;
		}
	}

	/* Check if we are done */
	if (state.sid2pid_done == TRUE && state.pid2sid_done == TRUE)
		goto out;

	/*
	 * native LDAP lookups:
	 * If nldap or mixed mode is enabled then pid2sid mapping requests
	 * need to lookup native LDAP directory service by uid/gid to get
	 * winname and unixname.
	 */
	if (state.nldap_nqueries) {
		retcode = nldap_lookup_batch(&state, &batch, result);
		if (IDMAP_FATAL_ERROR(retcode)) {
			result->retcode = retcode;
			goto out;
		}
	}

	/*
	 * AD lookups:
	 * 1. The pid2sid requests in the preceding step which successfully
	 *    retrieved winname from native LDAP objects will now need to
	 *    lookup AD by winname to get sid.
	 * 2. The sid2pid requests will need to lookup AD by sid to get
	 *    winname and unixname (AD or mixed mode).
	 * 3. If AD-based name mapping is enabled then pid2sid mapping
	 *    requests need to lookup AD by unixname to get winname and sid.
	 */
	if (state.ad_nqueries) {
		retcode = ad_lookup_batch(&state, &batch, result);
		if (IDMAP_FATAL_ERROR(retcode)) {
			result->retcode = retcode;
			goto out;
		}
	}

	/*
	 * native LDAP lookups:
	 * If nldap mode is enabled then sid2pid mapping requests
	 * which successfully retrieved winname from AD objects in the
	 * preceding step, will now need to lookup native LDAP directory
	 * service by winname to get unixname and pid.
	 */
	if (state.nldap_nqueries) {
		retcode = nldap_lookup_batch(&state, &batch, result);
		if (IDMAP_FATAL_ERROR(retcode)) {
			result->retcode = retcode;
			goto out;
		}
	}

	/* Reset 'done' flags */
	state.sid2pid_done = state.pid2sid_done = TRUE;

	/* Second stage */
	for (i = 0; i < batch.idmap_mapping_batch_len; i++) {
		state.curpos = i;
		if (IS_BATCH_SID(batch, i)) {
			retcode = sid2pid_second_pass(
			    &state,
			    cache,
			    db,
			    &batch.idmap_mapping_batch_val[i],
			    &result->ids.ids_val[i]);
		} else if (IS_BATCH_UID(batch, i)) {
			retcode = pid2sid_second_pass(
			    &state,
			    cache,
			    db,
			    &batch.idmap_mapping_batch_val[i],
			    &result->ids.ids_val[i], 1);
		} else if (IS_BATCH_GID(batch, i)) {
			retcode = pid2sid_second_pass(
			    &state,
			    cache,
			    db,
			    &batch.idmap_mapping_batch_val[i],
			    &result->ids.ids_val[i], 0);
		} else {
			/* First stage has already set the error */
			continue;
		}
		if (IDMAP_FATAL_ERROR(retcode)) {
			result->retcode = retcode;
			goto out;
		}
	}

	/* Check if we are done */
	if (state.sid2pid_done == TRUE && state.pid2sid_done == TRUE)
		goto out;

	/* Reset our 'done' flags */
	state.sid2pid_done = state.pid2sid_done = TRUE;

	/* Update cache in a single transaction */
	if (sql_exec_no_cb(cache, IDMAP_CACHENAME, "BEGIN TRANSACTION;")
	    != IDMAP_SUCCESS)
		goto out;

	for (i = 0; i < batch.idmap_mapping_batch_len; i++) {
		state.curpos = i;
		if (IS_BATCH_SID(batch, i)) {
			(void) update_cache_sid2pid(
			    &state,
			    cache,
			    &batch.idmap_mapping_batch_val[i],
			    &result->ids.ids_val[i]);
		} else if ((IS_BATCH_UID(batch, i)) ||
		    (IS_BATCH_GID(batch, i))) {
			(void) update_cache_pid2sid(
			    &state,
			    cache,
			    &batch.idmap_mapping_batch_val[i],
			    &result->ids.ids_val[i]);
		}
	}

	/* Commit if we have at least one successful update */
	if (state.sid2pid_done == FALSE || state.pid2sid_done == FALSE)
		(void) sql_exec_no_cb(cache, IDMAP_CACHENAME,
		    "COMMIT TRANSACTION;");
	else
		(void) sql_exec_no_cb(cache, IDMAP_CACHENAME,
		    "END TRANSACTION;");

out:
	cleanup_lookup_state(&state);
	if (IDMAP_ERROR(result->retcode)) {
		xdr_free(xdr_idmap_ids_res, (caddr_t)result);
		result->ids.ids_len = 0;
		result->ids.ids_val = NULL;
	}
	result->retcode = idmap_stat4prot(result->retcode);
	return (TRUE);
}


/* ARGSUSED */
static
int
list_mappings_cb(void *parg, int argc, char **argv, char **colnames)
{
	list_cb_data_t		*cb_data;
	char			*str;
	idmap_mappings_res	*result;
	idmap_retcode		retcode;
	int			w2u, u2w;
	char			*end;
	static int		validated_column_names = 0;

	if (!validated_column_names) {
		assert(strcmp(colnames[0], "rowid") == 0);
		assert(strcmp(colnames[1], "sidprefix") == 0);
		assert(strcmp(colnames[2], "rid") == 0);
		assert(strcmp(colnames[3], "pid") == 0);
		assert(strcmp(colnames[4], "w2u") == 0);
		assert(strcmp(colnames[5], "u2w") == 0);
		assert(strcmp(colnames[6], "windomain") == 0);
		assert(strcmp(colnames[7], "canon_winname") == 0);
		assert(strcmp(colnames[8], "unixname") == 0);
		assert(strcmp(colnames[9], "is_user") == 0);
		assert(strcmp(colnames[10], "is_wuser") == 0);
		validated_column_names = 1;
	}


	cb_data = (list_cb_data_t *)parg;
	result = (idmap_mappings_res *)cb_data->result;

	_VALIDATE_LIST_CB_DATA(11, &result->mappings.mappings_val,
	    sizeof (idmap_mapping));

	result->mappings.mappings_len++;

	if ((str = strdup(argv[1])) == NULL)
		return (1);
	result->mappings.mappings_val[cb_data->next].id1.idmap_id_u.sid.prefix =
	    str;
	result->mappings.mappings_val[cb_data->next].id1.idmap_id_u.sid.rid =
	    strtoul(argv[2], &end, 10);
	result->mappings.mappings_val[cb_data->next].id1.idtype =
	    strtol(argv[10], &end, 10) ? IDMAP_USID : IDMAP_GSID;

	result->mappings.mappings_val[cb_data->next].id2.idmap_id_u.uid =
	    strtoul(argv[3], &end, 10);
	result->mappings.mappings_val[cb_data->next].id2.idtype =
	    strtol(argv[9], &end, 10) ? IDMAP_UID : IDMAP_GID;

	w2u = argv[4] ? strtol(argv[4], &end, 10) : 0;
	u2w = argv[5] ? strtol(argv[5], &end, 10) : 0;

	if (w2u > 0 && u2w == 0)
		result->mappings.mappings_val[cb_data->next].direction =
		    IDMAP_DIRECTION_W2U;
	else if (w2u == 0 && u2w > 0)
		result->mappings.mappings_val[cb_data->next].direction =
		    IDMAP_DIRECTION_U2W;
	else
		result->mappings.mappings_val[cb_data->next].direction =
		    IDMAP_DIRECTION_BI;

	STRDUP_OR_FAIL(result->mappings.mappings_val[cb_data->next].id1domain,
	    argv[6]);

	STRDUP_OR_FAIL(result->mappings.mappings_val[cb_data->next].id1name,
	    argv[7]);

	STRDUP_OR_FAIL(result->mappings.mappings_val[cb_data->next].id2name,
	    argv[8]);


	result->lastrowid = strtoll(argv[0], &end, 10);
	cb_data->next++;
	result->retcode = IDMAP_SUCCESS;
	return (0);
}


/* ARGSUSED */
bool_t
idmap_list_mappings_1_svc(int64_t lastrowid, uint64_t limit,
    idmap_mappings_res *result, struct svc_req *rqstp)
{
	sqlite		*cache = NULL;
	char		lbuf[30], rbuf[30];
	uint64_t	maxlimit;
	idmap_retcode	retcode;
	char		*sql = NULL;

	(void) memset(result, 0, sizeof (*result));
	lbuf[0] = rbuf[0] = 0;

	RDLOCK_CONFIG();
	maxlimit = _idmapdstate.cfg->pgcfg.list_size_limit;
	UNLOCK_CONFIG();

	/* Get cache handle */
	result->retcode = get_cache_handle(&cache);
	if (result->retcode != IDMAP_SUCCESS)
		goto out;

	result->retcode = IDMAP_ERR_INTERNAL;

	/* Create LIMIT expression. */
	if (limit == 0 || (maxlimit > 0 && maxlimit < limit))
		limit = maxlimit;
	if (limit > 0)
		(void) snprintf(lbuf, sizeof (lbuf),
		    "LIMIT %" PRIu64, limit + 1ULL);

	(void) snprintf(rbuf, sizeof (rbuf), "rowid > %" PRIu64, lastrowid);

	/*
	 * Combine all the above into a giant SELECT statement that
	 * will return the requested mappings
	 */
	sql = sqlite_mprintf("SELECT rowid, sidprefix, rid, pid, w2u, u2w, "
	    "windomain, canon_winname, unixname, is_user, is_wuser "
	    " FROM idmap_cache WHERE "
	    " %s %s;",
	    rbuf, lbuf);
	if (sql == NULL) {
		idmapdlog(LOG_ERR, "Out of memory");
		goto out;
	}

	/* Execute the SQL statement and update the return buffer */
	PROCESS_LIST_SVC_SQL(retcode, cache, IDMAP_CACHENAME, sql, limit,
	    list_mappings_cb, result, result->mappings.mappings_len);

out:
	if (sql)
		sqlite_freemem(sql);
	if (IDMAP_ERROR(result->retcode))
		(void) xdr_free(xdr_idmap_mappings_res, (caddr_t)result);
	result->retcode = idmap_stat4prot(result->retcode);
	return (TRUE);
}


/* ARGSUSED */
static
int
list_namerules_cb(void *parg, int argc, char **argv, char **colnames)
{
	list_cb_data_t		*cb_data;
	idmap_namerules_res	*result;
	idmap_retcode		retcode;
	int			w2u_order, u2w_order;
	char			*end;
	static int		validated_column_names = 0;

	if (!validated_column_names) {
		assert(strcmp(colnames[0], "rowid") == 0);
		assert(strcmp(colnames[1], "is_user") == 0);
		assert(strcmp(colnames[2], "is_wuser") == 0);
		assert(strcmp(colnames[3], "windomain") == 0);
		assert(strcmp(colnames[4], "winname_display") == 0);
		assert(strcmp(colnames[5], "is_nt4") == 0);
		assert(strcmp(colnames[6], "unixname") == 0);
		assert(strcmp(colnames[7], "w2u_order") == 0);
		assert(strcmp(colnames[8], "u2w_order") == 0);
		validated_column_names = 1;
	}

	cb_data = (list_cb_data_t *)parg;
	result = (idmap_namerules_res *)cb_data->result;

	_VALIDATE_LIST_CB_DATA(9, &result->rules.rules_val,
	    sizeof (idmap_namerule));

	result->rules.rules_len++;

	result->rules.rules_val[cb_data->next].is_user =
	    strtol(argv[1], &end, 10);

	result->rules.rules_val[cb_data->next].is_wuser =
	    strtol(argv[2], &end, 10);

	STRDUP_OR_FAIL(result->rules.rules_val[cb_data->next].windomain,
	    argv[3]);

	STRDUP_OR_FAIL(result->rules.rules_val[cb_data->next].winname,
	    argv[4]);

	result->rules.rules_val[cb_data->next].is_nt4 =
	    strtol(argv[5], &end, 10);

	STRDUP_OR_FAIL(result->rules.rules_val[cb_data->next].unixname,
	    argv[6]);

	w2u_order = argv[7] ? strtol(argv[7], &end, 10) : 0;
	u2w_order = argv[8] ? strtol(argv[8], &end, 10) : 0;

	if (w2u_order > 0 && u2w_order == 0)
		result->rules.rules_val[cb_data->next].direction =
		    IDMAP_DIRECTION_W2U;
	else if (w2u_order == 0 && u2w_order > 0)
		result->rules.rules_val[cb_data->next].direction =
		    IDMAP_DIRECTION_U2W;
	else
		result->rules.rules_val[cb_data->next].direction =
		    IDMAP_DIRECTION_BI;

	result->lastrowid = strtoll(argv[0], &end, 10);
	cb_data->next++;
	result->retcode = IDMAP_SUCCESS;
	return (0);
}


/* ARGSUSED */
bool_t
idmap_list_namerules_1_svc(idmap_namerule rule, uint64_t lastrowid,
		uint64_t limit, idmap_namerules_res *result,
		struct svc_req *rqstp)
{

	sqlite		*db = NULL;
	char		w2ubuf[15], u2wbuf[15];
	char		lbuf[30], rbuf[30];
	char		*sql = NULL;
	char		*expr = NULL;
	uint64_t	maxlimit;
	idmap_retcode	retcode;

	(void) memset(result, 0, sizeof (*result));
	lbuf[0] = rbuf[0] = 0;

	result->retcode = validate_rule(&rule);
	if (result->retcode != IDMAP_SUCCESS)
		goto out;

	RDLOCK_CONFIG();
	maxlimit = _idmapdstate.cfg->pgcfg.list_size_limit;
	UNLOCK_CONFIG();

	/* Get db handle */
	result->retcode = get_db_handle(&db);
	if (result->retcode != IDMAP_SUCCESS)
		goto out;

	result->retcode = IDMAP_ERR_INTERNAL;

	w2ubuf[0] = u2wbuf[0] = 0;
	if (rule.direction == IDMAP_DIRECTION_BI) {
		(void) snprintf(w2ubuf, sizeof (w2ubuf), "AND w2u_order > 0");
		(void) snprintf(u2wbuf, sizeof (u2wbuf), "AND u2w_order > 0");
	} else if (rule.direction == IDMAP_DIRECTION_W2U) {
		(void) snprintf(w2ubuf, sizeof (w2ubuf), "AND w2u_order > 0");
		(void) snprintf(u2wbuf, sizeof (u2wbuf),
		    "AND (u2w_order = 0 OR u2w_order ISNULL)");
	} else if (rule.direction == IDMAP_DIRECTION_U2W) {
		(void) snprintf(w2ubuf, sizeof (w2ubuf),
		    "AND (w2u_order = 0 OR w2u_order ISNULL)");
		(void) snprintf(u2wbuf, sizeof (u2wbuf), "AND u2w_order > 0");
	}

	result->retcode = gen_sql_expr_from_rule(&rule, &expr);
	if (result->retcode != IDMAP_SUCCESS)
		goto out;

	/* Create LIMIT expression. */
	if (limit == 0 || (maxlimit > 0 && maxlimit < limit))
		limit = maxlimit;
	if (limit > 0)
		(void) snprintf(lbuf, sizeof (lbuf),
		    "LIMIT %" PRIu64, limit + 1ULL);

	(void) snprintf(rbuf, sizeof (rbuf), "rowid > %" PRIu64, lastrowid);

	/*
	 * Combine all the above into a giant SELECT statement that
	 * will return the requested rules
	 */
	sql = sqlite_mprintf("SELECT rowid, is_user, is_wuser, windomain, "
	    "winname_display, is_nt4, unixname, w2u_order, u2w_order "
	    "FROM namerules WHERE "
	    " %s %s %s %s %s;",
	    rbuf, expr, w2ubuf, u2wbuf, lbuf);

	if (sql == NULL) {
		idmapdlog(LOG_ERR, "Out of memory");
		goto out;
	}

	/* Execute the SQL statement and update the return buffer */
	PROCESS_LIST_SVC_SQL(retcode, db, IDMAP_DBNAME, sql, limit,
	    list_namerules_cb, result, result->rules.rules_len);

out:
	if (expr)
		sqlite_freemem(expr);
	if (sql)
		sqlite_freemem(sql);
	if (IDMAP_ERROR(result->retcode))
		(void) xdr_free(xdr_idmap_namerules_res, (caddr_t)result);
	result->retcode = idmap_stat4prot(result->retcode);
	return (TRUE);
}

#define	IDMAP_RULES_AUTH	"solaris.admin.idmap.rules"
static int
verify_rules_auth(struct svc_req *rqstp)
{
	ucred_t		*uc = NULL;
	uid_t		uid;
	char		buf[1024];
	struct passwd	pwd;

	if (svc_getcallerucred(rqstp->rq_xprt, &uc) != 0) {
		idmapdlog(LOG_ERR, "svc_getcallerucred failed during "
		    "authorization (%s)", strerror(errno));
		return (-1);
	}

	uid = ucred_geteuid(uc);
	if (uid == (uid_t)-1) {
		idmapdlog(LOG_ERR, "ucred_geteuid failed during "
		    "authorization (%s)", strerror(errno));
		ucred_free(uc);
		return (-1);
	}

	if (getpwuid_r(uid, &pwd, buf, sizeof (buf)) == NULL) {
		idmapdlog(LOG_ERR, "getpwuid_r(%u) failed during "
		    "authorization (%s)", uid, strerror(errno));
		ucred_free(uc);
		return (-1);
	}

	if (chkauthattr(IDMAP_RULES_AUTH, pwd.pw_name) != 1) {
		idmapdlog(LOG_INFO, "%s is not authorized (%s)",
		    pwd.pw_name, IDMAP_RULES_AUTH);
		ucred_free(uc);
		return (-1);
	}

	ucred_free(uc);
	return (1);
}

/*
 * Meaning of the return values is the following: For retcode ==
 * IDMAP_SUCCESS, everything went OK and error_index is
 * undefined. Otherwise, error_index >=0 shows the failed batch
 * element. errro_index == -1 indicates failure at the beginning,
 * error_index == -2 at the end.
 */

/* ARGSUSED */
bool_t
idmap_update_1_svc(idmap_update_batch batch, idmap_update_res *res,
		struct svc_req *rqstp)
{
	sqlite		*db = NULL;
	idmap_update_op	*up;
	int		i;
	int		trans = FALSE;

	res->error_index = -1;
	(void) memset(&res->error_rule, 0, sizeof (res->error_rule));
	(void) memset(&res->conflict_rule, 0, sizeof (res->conflict_rule));

	if (verify_rules_auth(rqstp) < 0) {
		res->retcode = IDMAP_ERR_PERMISSION_DENIED;
		goto out;
	}

	if (batch.idmap_update_batch_len == 0 ||
	    batch.idmap_update_batch_val == NULL) {
		res->retcode = IDMAP_SUCCESS;
		goto out;
	}

	res->retcode = validate_rules(&batch);
	if (res->retcode != IDMAP_SUCCESS)
		goto out;

	/* Get db handle */
	res->retcode = get_db_handle(&db);
	if (res->retcode != IDMAP_SUCCESS)
		goto out;

	res->retcode = sql_exec_no_cb(db, IDMAP_DBNAME, "BEGIN TRANSACTION;");
	if (res->retcode != IDMAP_SUCCESS)
		goto out;
	trans = TRUE;

	for (i = 0; i < batch.idmap_update_batch_len; i++) {
		up = &batch.idmap_update_batch_val[i];
		switch (up->opnum) {
		case OP_NONE:
			res->retcode = IDMAP_SUCCESS;
			break;
		case OP_ADD_NAMERULE:
			res->retcode = add_namerule(db,
			    &up->idmap_update_op_u.rule);
			break;
		case OP_RM_NAMERULE:
			res->retcode = rm_namerule(db,
			    &up->idmap_update_op_u.rule);
			break;
		case OP_FLUSH_NAMERULES:
			res->retcode = flush_namerules(db);
			break;
		default:
			res->retcode = IDMAP_ERR_NOTSUPPORTED;
			break;
		};

		if (res->retcode != IDMAP_SUCCESS) {
			res->error_index = i;
			if (up->opnum == OP_ADD_NAMERULE ||
			    up->opnum == OP_RM_NAMERULE) {
				idmap_stat r2 =
				    idmap_namerule_cpy(&res->error_rule,
				    &up->idmap_update_op_u.rule);
				if (r2 != IDMAP_SUCCESS)
					res->retcode = r2;
			}
			goto out;
		}
	}

out:
	if (trans) {
		if (res->retcode == IDMAP_SUCCESS) {
			res->retcode =
			    sql_exec_no_cb(db, IDMAP_DBNAME,
			    "COMMIT TRANSACTION;");
			if (res->retcode !=  IDMAP_SUCCESS)
				res->error_index = -2;
		}
		else
			(void) sql_exec_no_cb(db, IDMAP_DBNAME,
			    "ROLLBACK TRANSACTION;");
	}

	res->retcode = idmap_stat4prot(res->retcode);

	return (TRUE);
}


/* ARGSUSED */
bool_t
idmap_get_mapped_id_by_name_1_svc(idmap_mapping request,
		idmap_mappings_res *result, struct svc_req *rqstp)
{
	sqlite		*cache = NULL, *db = NULL;

	/* Init */
	(void) memset(result, 0, sizeof (*result));

	result->retcode = validate_mapped_id_by_name_req(&request);
	if (result->retcode != IDMAP_SUCCESS)
		goto out;

	/* Get cache handle */
	result->retcode = get_cache_handle(&cache);
	if (result->retcode != IDMAP_SUCCESS)
		goto out;

	/* Get db handle */
	result->retcode = get_db_handle(&db);
	if (result->retcode != IDMAP_SUCCESS)
		goto out;

	/* Allocate result */
	result->mappings.mappings_val = calloc(1, sizeof (idmap_mapping));
	if (result->mappings.mappings_val == NULL) {
		idmapdlog(LOG_ERR, "Out of memory");
		result->retcode = IDMAP_ERR_MEMORY;
		goto out;
	}
	result->mappings.mappings_len = 1;


	if (IS_REQUEST_SID(request, 1)) {
		result->retcode = get_w2u_mapping(
		    cache,
		    db,
		    &request,
		    result->mappings.mappings_val);
	} else if (IS_REQUEST_UID(request)) {
		result->retcode = get_u2w_mapping(
		    cache,
		    db,
		    &request,
		    result->mappings.mappings_val,
		    1);
	} else if (IS_REQUEST_GID(request)) {
		result->retcode = get_u2w_mapping(
		    cache,
		    db,
		    &request,
		    result->mappings.mappings_val,
		    0);
	} else {
		result->retcode = IDMAP_ERR_IDTYPE;
	}

out:
	if (IDMAP_FATAL_ERROR(result->retcode)) {
		xdr_free(xdr_idmap_mappings_res, (caddr_t)result);
		result->mappings.mappings_len = 0;
		result->mappings.mappings_val = NULL;
	}
	result->retcode = idmap_stat4prot(result->retcode);
	return (TRUE);
}


/* ARGSUSED */
int
idmap_prog_1_freeresult(SVCXPRT *transp, xdrproc_t xdr_result,
		caddr_t result)
{
	(void) xdr_free(xdr_result, result);
	return (TRUE);
}
