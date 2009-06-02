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
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

/*
 * Show bootargs.
 *
 *   Usage:  eeprom [-r]
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <sys/openpromio.h>
#include "message.h"

#define	DEV_PROM	"/dev/openprom"
#define	MAX_BOOTARGLEN	1024

typedef union {
	char	op_buf[MAX_BOOTARGLEN + sizeof(uint_t)];
	struct openpromio	op_io;
} op_barg_t;

extern void	setprogname(char *prog);

static void	usage(char *progname);
static void	barg_read(op_barg_t *barg);
static void	barg_print(op_barg_t *barg, int raw);

int
main(int argc, char **argv)
{
	int		c, raw = 0;
	char		*progname = *argv;
	op_barg_t	barg;

	setprogname(progname);

	while ((c = getopt(argc, argv, "r")) != EOF) {
		switch (c) {
		case 'r':
			raw = 1;
			break;

		default:
			usage(progname);
			/* NOTREACHED */
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 0) {
		usage(progname);
		/* NOTREACHED */
	}

	barg_read(&barg);
	barg_print(&barg, raw);

	return 0;
}

static void
usage(char *progname)
{
	static const char	msg[] = "Usage: eeprom [-r]";

	exit(_error(0, msg, progname));
}

static void
barg_read(op_barg_t *barg)
{
	int	fd;
	struct openpromio	*opp;

	if ((fd = open(DEV_PROM, O_RDONLY)) == -1) {
		exit(_error(1, DEV_PROM " open failed"));
	}

	memset(barg, 0, sizeof(*barg));
	opp = &(barg->op_io);
	opp->oprom_size = sizeof(*barg) -
		offsetof(struct openpromio, oprom_array);
	if (ioctl(fd, OPROMGETBOOTARGS, opp) < 0) {
		exit(_error(1, "failed to get bootargs"));
	}

	(void)close(fd);
}

static void
barg_print(op_barg_t *barg, int raw)
{
	struct openpromio	*opp = &(barg->op_io);
	char	*bootargs = opp->oprom_array, *cp;
	size_t	barglen = opp->oprom_size;


	if (raw) {
		(void)fputs(bootargs, stdout);
		(void)fputc('\n', stdout);
		return;
	}

	cp = bootargs;
	while (cp && cp < bootargs + barglen && *cp) {
		char	*name, *val;

		name = (char *)strtok(cp, "=");
		val = (char *)strtok(NULL, "");
		if (val == NULL) {
			val = "true";
			cp = NULL;
		}
		else if (*val != '\'' && *val != '\"') {
			if (*val == ',') {
				cp = val + 1;
				val = "";
			}
			else {
				cp = (char *)strtok(val, ",");
				cp = (char *)strtok(NULL, "");
			}
		}
		else {
			cp = val + 1;
			while (cp && *cp != *val) {
				++cp;
			}

			if (cp == NULL) {
				(void)printf("missing %c in property %s.\n",
					     *val, name);
			}
			else {
				*cp++ = '\0';
				if (*cp == ',') {
					cp++;
				}
				else if (*cp != '\0') {
					(void)printf("syntax error in "
						     "bootargs: ignore %s\n",
						     cp);
					cp = NULL;
				}
			}
			val++;
		}

		(void)printf("%s=%s\n", name, val);
	}

	free(bootargs);
}
