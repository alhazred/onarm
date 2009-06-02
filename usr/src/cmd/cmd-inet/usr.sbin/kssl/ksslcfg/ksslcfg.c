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

#pragma ident	"@(#)ksslcfg.c	1.2	06/05/03 SMI"

#include <arpa/inet.h> /* inet_addr() */
#include <ctype.h>
#include <libscf.h>
#include <netdb.h> /* hostent */
#include <netinet/in.h> /* ip_addr_t */
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/varargs.h>
#include "ksslcfg.h"

/*
 * ksslcfg(1M)
 *
 * ksslcfg manages smf(5) instances for the Kernel SSL proxy module.
 * It makes use of kssladm(1M) which does the grunt work.
 */

#define	KSSLCFG_VERSION "v1.2"

boolean_t verbose = B_FALSE;
const char *SERVICE_NAME = "network/ssl/proxy";

void
KSSL_DEBUG(const char *format, ...)
{
	va_list ap;

	if (verbose) {
		va_start(ap, format);
		(void) vprintf(format, ap);
		va_end(ap);
	}
}

int
get_portnum(const char *s, ushort_t *rport)
{
	unsigned long port;

	errno = 0;
	port = strtoul(s, NULL, 10);
	if (port > USHRT_MAX || port == 0 || errno != 0) {
		return (0);
	}

	if (rport != NULL)
		*rport = (ushort_t)port;
	return (1);
}

#define	ANY_ADDR	"INADDR_ANY"

/*
 * An instance name is formed using either the host name in the fully
 * qualified domain name form (FQDN) which should map to a specific IP address
 * or using INADDR_ANY which means all IP addresses.
 *
 * We do a lookup or reverse lookup to get the host name. It is assumed that
 * the returned name is in the FQDN form. i.e. DNS is used.
 */
char *
create_instance_name(const char *arg, char **inaddr_any_name,
    boolean_t is_create)
{
	int len;
	uint16_t port;
	char *cname;
	in_addr_t addr;
	char *instance_name;
	const char *prefix = "kssl-";
	char *first_space = strchr(arg, ' ');

	if (first_space == NULL) {
		if (get_portnum(arg, &port) == 0) {
			(void) fprintf(stderr,
			    gettext("Error: Invalid port value -- %s\n"),
			    arg);
			return (NULL);
		}
		KSSL_DEBUG("port=%d\n", port);
		if ((cname = strdup(ANY_ADDR)) == NULL)
			return (NULL);
	} else {
		char *temp_str;
		char *ptr;
		struct hostent *hp;
		boolean_t do_warn;

		if (get_portnum(first_space + 1, &port) == 0) {
			(void) fprintf(stderr,
			    gettext("Error: Invalid port value -- %s\n"),
			    first_space + 1);
			return (NULL);
		}
		KSSL_DEBUG("port=%d\n", port);

		if ((temp_str = strdup(arg)) == NULL)
			return (NULL);
		*(strchr(temp_str, ' ')) = '\0';

		if ((int)(addr = inet_addr(temp_str)) == -1) {
			if ((hp = gethostbyname(temp_str)) == NULL) {
				(void) fprintf(stderr,
				    gettext("Error: Unknown host -- %s\n"),
				    temp_str);
				free(temp_str);
				return (NULL);
			}
		} else {
			/* This is an IP address. Do a reverse lookup. */
			if ((hp = gethostbyaddr((char *)&addr, 4, AF_INET))
			    == NULL) {
				(void) fprintf(stderr,
				    gettext("Error: Unknown host -- %s\n"),
				    temp_str);
				free(temp_str);
				return (NULL);
			}
		}

		if ((ptr = cname = strdup(hp->h_name)) == NULL) {
			free(temp_str);
			return (NULL);
		}
		do_warn = B_TRUE;
		/* "s/./-/g" */
		while ((ptr = strchr(ptr, '.')) != NULL) {
			if (do_warn)
				do_warn = B_FALSE;
			*ptr = '-';
			ptr++;
		}

		if (do_warn && is_create) {
			(void) fprintf(stderr,
			    gettext("Warning: %s does not appear to have a"
			    " registered DNS name.\n"), temp_str);
		}

		free(temp_str);
	}

	KSSL_DEBUG("Cannonical host name =%s\n", cname);

	len = strlen(prefix) + strlen(cname) + 10;
	if ((instance_name = malloc(len)) == NULL) {
		(void) fprintf(stderr,
		    gettext("Error: memory allocation failure.\n"));
		return (NULL);
	}
	(void) snprintf(instance_name, len, "%s%s-%d", prefix, cname, port);

	if (is_create) {
		len = strlen(prefix) + strlen(ANY_ADDR) + 10;
		if ((*inaddr_any_name = malloc(len)) == NULL) {
			(void) fprintf(stderr,
			    gettext("Error: memory allocation failure.\n"));
			free(cname);
			return (NULL);
		}

		(void) snprintf(*inaddr_any_name, len,
		    "%s%s-%d", prefix, ANY_ADDR, port);
	}

	free(cname);
	KSSL_DEBUG("instance_name=%s\n", instance_name);
	return (instance_name);
}

static void
usage_all(void)
{
	(void) fprintf(stderr, gettext("Usage:\n"));
	usage_create(B_FALSE);
	usage_delete(B_FALSE);
	(void) fprintf(stderr, "ksslcfg -V\n");
	(void) fprintf(stderr, "ksslcfg -?\n");
}


int
main(int argc, char **argv)
{
	int rv = SUCCESS;

	(void) setlocale(LC_ALL, "");
#if !defined(TEXT_DOMAIN)	/* Should be defined by cc -D */
#define	TEXT_DOMAIN "SYS_TEST"	/* Use this only if it weren't */
#endif
	(void) textdomain(TEXT_DOMAIN);

	if (argc < 2) {
		usage_all();
		return (ERROR_USAGE);
	}

	if (strcmp(argv[1], "create") == 0) {
		rv = do_create(argc, argv);
	} else if (strcmp(argv[1], "delete") == 0) {
		rv = do_delete(argc, argv);
	} else if (strcmp(argv[1], "-V") == 0) {
		(void) printf("%s\n", KSSLCFG_VERSION);
	} else if (strcmp(argv[1], "-?") == 0) {
		usage_all();
	} else {
		(void) fprintf(stderr,
		    gettext("Error: Unknown subcommand -- %s\n"), argv[1]);
		usage_all();
		rv = ERROR_USAGE;
	}

	return (rv);
}
