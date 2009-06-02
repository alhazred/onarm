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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007 NEC Corporation
 */

#pragma ident	"@(#)__failed_count.c	1.3	06/01/25 SMI"

#include <string.h>
#include <syslog.h>
#include "passwdutil.h"

int
__incr_failed_count(char *username, char *repname, int max_failures)
{
	int ret;
	void *buf;
	attrlist items[1];
	repops_t *ops = rops[REP_FILES];

	/* account locking only defined for files */
	if (strcmp(repname, "files") != 0)
		return (PWU_SUCCESS);

	if (ops->lock != NULL) {
		if ((ret = ops->lock()) != PWU_SUCCESS)
			return (ret);
	}

	items[0].type = ATTR_INCR_FAILED_LOGINS;
	items[0].next = NULL;
	if ((ret = ops->getpwnam(username, items, NULL, &buf)) != PWU_SUCCESS)
		goto out;

	/* We increment the failed count by one */
	if ((ret = ops->update(items, NULL, buf)) != PWU_SUCCESS)
		goto out;

	/* Did we just exceed "max_failures" ? */
	if (items[0].data.val_i == max_failures) {
		syslog(LOG_AUTH|LOG_NOTICE,
		    "Excessive (%d) login failures for %s: locking account.",
		    max_failures, username);

		items[0].type = ATTR_LOCK_ACCOUNT;
		if ((ret = ops->update(items, NULL, buf)) != PWU_SUCCESS)
			goto out;
	}
	ret = ops->putpwnam(username, NULL, NULL, NULL, buf);

out:
	if (ops->unlock != NULL) {
		ops->unlock();
	}

	return (ret);
}

/*
 * reset the failed count.
 * returns the number of failed logins before the reset, or an error (< 0)
 */
int
__rst_failed_count(char *username, char *repname)
{
	int ret;
	void *buf;
	attrlist items[1];
	repops_t *ops = rops[REP_FILES];

	/* account locking only defined for files */
	if (strcmp(repname, "files") != 0)
		return (PWU_SUCCESS);

	if (ops->lock != NULL) {
		if ((ret = ops->lock()) != PWU_SUCCESS)
			return (ret);
	}

	items[0].type = ATTR_RST_FAILED_LOGINS;
	items[0].next = NULL;
	if ((ret = ops->getpwnam(username, items, NULL, &buf)) != PWU_SUCCESS)
		goto out;
	if ((ret = ops->update(items, NULL, buf)) != PWU_SUCCESS)
		goto out;
	ret = ops->putpwnam(username, NULL, NULL, NULL, buf);
out:
	if (ops->unlock != NULL) {
		ops->unlock();
	}

	return (ret != PWU_SUCCESS ? ret : items[0].data.val_i);
}
