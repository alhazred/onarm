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

#pragma ident	"@(#)getuserattr.c	1.3	06/09/28 SMI"

#include <sys/types.h>
#include <user_attr.h>
#include <stdlib.h>
#include <string.h>
#include "nisplus_common.h"
#include "nisplus_tables.h"


static nss_status_t
getbynam(nisplus_backend_ptr_t be, void *a)
{
	nss_XbyY_args_t *argp = (nss_XbyY_args_t *)a;

	return (_nss_nisplus_lookup(be,
	    argp, USERATTR_TAG_NAME, argp->key.name));
}


/*
 * Returns NSS_STR_PARSE_{SUCCESS, ERANGE, PARSE}
 */
/*ARGSUSED*/
static int
nis_object2userstr(int nobj, nis_object *obj,
		nisplus_backend_ptr_t be,
		nss_XbyY_args_t *argp)
{
	char			*buffer, *name, *qual, *res1, *res2;
	char			*attr;
	int			buflen, namelen, quallen, res1len;
	int			res2len, attrlen;
	struct entry_col	*ecol;

	/*
	 * If we got more than one nis_object, we just ignore object(s)
	 * except the first. Although it should never have happened.
	 *
	 * ASSUMPTION: All the columns in the NIS+ tables are
	 * null terminated.
	 */

	if (obj->zo_data.zo_type != ENTRY_OBJ ||
	    obj->EN_data.en_cols.en_cols_len < USERATTR_COL) {
	    /* namespace/table/object is curdled */
		return (NSS_STR_PARSE_PARSE);
	}
	ecol = obj->EN_data.en_cols.en_cols_val;

	/* user name */
	__NISPLUS_GETCOL_OR_RETURN(ecol, USERATTR_NDX_NAME, namelen, name);

	/* qualifier: reserved for future use */
	__NISPLUS_GETCOL_OR_EMPTY(ecol, USERATTR_NDX_QUALIFIER,
		quallen, qual);

	/* res1: reserved field 1 */
	__NISPLUS_GETCOL_OR_EMPTY(ecol, USERATTR_NDX_RES1, res1len, res1);

	/* userstr->res2: reserved field 2 */
	__NISPLUS_GETCOL_OR_EMPTY(ecol, USERATTR_NDX_RES2, res2len, res2);

	/* attrs: key-value pairs of attributes */
	__NISPLUS_GETCOL_OR_EMPTY(ecol, USERATTR_NDX_ATTR, attrlen, attr);

	buflen = namelen + quallen + res1len + res2len + attrlen + 5;
	if (argp->buf.result != NULL) {
		if ((be->buffer = calloc(1, buflen)) == NULL)
			return (NSS_STR_PARSE_PARSE);
		/* exclude trailing null from length */
		be->buflen = buflen - 1;
		buffer = be->buffer;
	} else {
		if (buflen > argp->buf.buflen)
			return (NSS_STR_PARSE_ERANGE);
		buflen = argp->buf.buflen;
		buffer = argp->buf.buffer;
		(void) memset(buffer, 0, buflen);
	}
	(void) snprintf(buffer, buflen, "%s:%s:%s:%s:%s",
		name, qual, res1, res2, attr);
#ifdef DEBUG
	(void) fprintf(stdout, "userattr [%s]\n", buffer);
	(void) fflush(stdout);
#endif  /* DEBUG */
	return (NSS_STR_PARSE_SUCCESS);
}

static nisplus_backend_op_t userattr_ops[] = {
	_nss_nisplus_destr,
	_nss_nisplus_endent,
	_nss_nisplus_setent,
	_nss_nisplus_getent,
	getbynam
};

/*ARGSUSED*/
nss_backend_t *
_nss_nisplus_user_attr_constr(const char *dummy1,
    const char *dummy2,
    const char *dummy3,
    const char *dummy4,
    const char *dummy5)
{
	return (_nss_nisplus_constr(userattr_ops,
	    sizeof (userattr_ops)/sizeof (userattr_ops[0]),
	    USERATTR_TBLNAME,
	    nis_object2userstr));
}
