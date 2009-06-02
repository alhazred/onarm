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
 * Copyright 2002-2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/xramdisk.h>
#include <sys/stat.h>
#include <sys/mkdev.h>
#include <sys/sunddi.h>
#include <sys/ccompile.h>
#include <libdevinfo.h>
#include <stdio.h>
#include <fcntl.h>
#include <locale.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stropts.h>
#include <ctype.h>
#include "utils.h"

#define	XRD_CHAR_DEV_PFX		"/dev/" XRD_CHAR_NAME "/"

#define	HEADING	"%-*.*s %20s  %-10s\n"
#define	FORMAT	"%-*.*s %20llu    %s\n"
#define	FW	(sizeof (XRD_CHAR_DEV_PFX) - 1 + XRD_NAME_LEN)

#ifndef XRD_NBLOCKS_PROP_NAME
#define	XRD_NBLOCKS_PROP_NAME	"Nblocks"
#endif

#ifndef XRD_SIZE_PROP_NAME
#define	XRD_SIZE_PROP_NAME	"Size"
#endif

#ifndef XRD_BASEPAGE_PROP_NAME
#define XRD_BASEPAGE_PROP_NAME	"BasePage"
#endif

#ifndef XRD_KVA_PROP_NAME
#define XRD_KVA_PROP_NAME	"KVA"
#endif

static char *pname;

static void
usage(void)
{
	(void) fprintf(stderr,
	    gettext("Usage: %s [ -a <name> <numpages>[k] | -d <name> ]\n"),
	    pname);
	exit(E_USAGE);
}

/*
 * This might be the first time we've used this minor device. If so,
 * it might also be that the /dev links are in the process of being created
 * by devfsadmd (or that they'll be created "soon"). We cannot return
 * until they're there or the invoker of xramdiskadm might try to use them
 * and not find them. This can happen if a shell script is running on
 * an MP.
 */
static void
wait_until_dev_complete(char *name)
{
	di_devlink_handle_t hdl;

	hdl = di_devlink_init(XRD_DRIVER_NAME, DI_MAKE_LINK);
	if (hdl == NULL) {
		die(gettext("couldn't create device link for \"%s\""), name);
	}
	(void) di_devlink_fini(&hdl);
}

/*
 * Create a named xramdisk.
 */
static void
alloc_xramdisk(int ctl_fd, char *name, uint64_t numpages)
{
	struct xrd_ioctl	xri;

	(void) strlcpy(xri.xri_name, name, sizeof (xri.xri_name));
	xri.xri_numpages = numpages;

	if (ioctl(ctl_fd, XRD_CREATE_DISK, &xri) == -1) {
		die(gettext("couldn't create xramdisk \"%s\""), name);
	}
#ifdef XRD_DEVFSADM_SUPPORT
	wait_until_dev_complete(name);
#endif

	(void) printf(XRD_CHAR_DEV_PFX "%s\n", name);
}

/*
 * Delete a named xramdisk.
 */
static void
delete_xramdisk(int ctl_fd, char *name)
{
	struct xrd_ioctl	xri;

	(void) strlcpy(xri.xri_name, name, sizeof (xri.xri_name));

	if (ioctl(ctl_fd, XRD_DELETE_DISK, &xri) == -1) {
		die(gettext("couldn't delete xramdisk \"%s\""), name);
	}
}

/*ARGSUSED*/
static int
di_callback(di_node_t node, di_minor_t diminor, void *arg)
{
	static boolean_t	heading_done = B_FALSE;
	boolean_t		obp_xramdisk;
	boolean_t		verbose = B_TRUE;
	char			*name;
	char			devnm[MAXNAMELEN];
	uint64_t		*sizep;
	uint64_t		*nblocksp;
	uint64_t		*basepagep;
	uint64_t		*kvap;
	char			chrpath[MAXPATHLEN];

	/*
	 * Only consider block nodes bound to the xramdisk driver.
	 */
	if (strcmp(di_driver_name(node), XRD_DRIVER_NAME) == 0 &&
	    di_minor_spectype(diminor) == S_IFCHR) {
		if (minor(di_minor_devt(diminor)) == XRD_CTL_MINOR) {
			return (DI_WALK_CONTINUE);
		}

		/*
		 * Determine whether this xramdisk is pseudo or OBP-created.
		 */
		obp_xramdisk = (di_nodeid(node) == DI_PROM_NODEID);

		/*
		 * If this is an OBP-created xramdisk use the node name, having
		 * first stripped the "xramdisk-" prefix.  For pseudo xramdisks
		 * use the minor name, having first stripped any ",raw" suffix.
		 */
		if (obp_xramdisk) {
			XRD_STRIP_PREFIX(name, di_node_name(node));
			(void) strlcpy(devnm, name, sizeof (devnm));
		} else {
			(void) strlcpy(devnm, di_minor_name(diminor),
			    sizeof (devnm));
			XRD_STRIP_SUFFIX(devnm);
		}

		/*
		 * Get the size of the xramdisk.
		 */
		if (di_prop_lookup_int64(di_minor_devt(diminor), node,
		    XRD_SIZE_PROP_NAME, (int64_t **)&sizep) == -1) {
			die(gettext("couldn't obtain size of xramdisk"));
		}

		if (di_prop_lookup_int64(di_minor_devt(diminor), node,
		    XRD_NBLOCKS_PROP_NAME, (int64_t **)&nblocksp) == -1) {
			die(gettext("couldn't obtain nblocks of xramdisk"));
		}

		if (di_prop_lookup_int64(di_minor_devt(diminor), node,
		    XRD_BASEPAGE_PROP_NAME, (int64_t **)&basepagep) == -1) {
			die(gettext("couldn't obtain basepage of xramdisk"));
		}

		if (di_prop_lookup_int64(di_minor_devt(diminor), node,
		    XRD_KVA_PROP_NAME, (int64_t **)&kvap) == -1) {
			die(gettext("couldn't obtain KVA of xramdisk"));
		}

		/*
		 * Print information about the xramdisk.  Prepend a heading
		 * if this is the first/only one.
		 */
		if (!heading_done) {
			(void) printf(HEADING, FW, FW, gettext("Block Device"),
			    gettext("Size"), gettext("Removable"));
			heading_done = B_TRUE;
		}
		(void) snprintf(chrpath, sizeof (chrpath),
		    XRD_CHAR_DEV_PFX "%s", devnm);
		(void) printf(FORMAT, FW, FW, chrpath, *sizep,
		    obp_xramdisk ? gettext("No") : gettext("Yes"));

		if (verbose) {
			(void) printf("    Nblocks = %llu,  BasePage = %llu,  "
				      "KernelVA = %p\n",
				      (uint64_t)*nblocksp,
				      (uint64_t)*basepagep,
				      (caddr_t)((int)*kvap));
		}
	}

	return (DI_WALK_CONTINUE);
}

/*
 * Print the list of all the xramdisks, their size, and whether they
 * are removable (i.e. non-OBP xramdisks).
 */
static void
print_xramdisk(void)
{
	di_node_t	root;

	/*
	 * Create a snapshot of the device tree, then walk it looking
	 * for, and printing information about, xramdisk nodes.
	 */
	if ((root = di_init("/", DINFOCPYALL)) == DI_NODE_NIL) {
		die(gettext("couldn't create device tree snapshot"));
	}

	if (di_walk_minor(root, DDI_PSEUDO, 0, NULL, di_callback) == -1) {
		di_fini(root);
		die(gettext("device tree walk failure"));
	}

	di_fini(root);
}


/*
 * main routine.
 */
int
main(int argc, char *argv[])
{
	int		c;
	char		*name = NULL;
	int		allocflag = 0;
	int		deleteflag = 0;
	int		errflag = 0;
	char		*suffix;
	uint64_t	numpages;
	int		openflag;
	int		ctl_fd = 0;
	static char	xrd_ctl[] = "/dev/" XRD_CTL_NAME;

	pname = getpname(argv[0]);

	(void) setlocale(LC_ALL, "");
	(void) textdomain(TEXT_DOMAIN);

	while ((c = getopt(argc, argv, "a:d:")) != EOF) {
		switch (c) {
		case 'a':
			/* To allocate xramdisk. */
			allocflag = 1;
			name = optarg;

			if (((argc - optind) <= 0) || (*argv[optind] == '-')) {
				warn(gettext("<numpages> missing\n"));
				usage();
				/*NOTREACHED*/
			}
			errno = 0;
			numpages = strtoll(argv[optind], &suffix, 0);
			if (errno == ERANGE) {
				warn(gettext("Illegal number \"%s\"\n"),
				     argv[optind]);
				usage();
				/*NOTREACHED*/
			}
			if (strcmp(suffix, "k") == 0) {
				numpages *= 1024;
				++suffix;
			}
			if (numpages == 0 || *suffix != '\0') {
				warn(gettext("Illegal <numpages> \"%s\"\n"),
				    argv[optind]);
				usage();
				/*NOTREACHED*/
			}
			++optind;
			break;

		case 'd':
			/* To delete xramdisk. */
			deleteflag = 1;
			name = optarg;
			break;

		default:
			errflag = 1;
			break;
		}
	}
	if (errflag || (allocflag && deleteflag) || (argc - optind) > 0) {
		usage();
		/*NOTREACHED*/
	}

	/*
	 * Now do the real work.
	 */
	openflag = O_EXCL;
	if (allocflag || deleteflag)
		openflag |= O_RDWR;
	else
		openflag |= O_RDONLY;
	ctl_fd = open(xrd_ctl, openflag);
	if (ctl_fd == -1) {
		if ((errno == EPERM) || (errno == EACCES)) {
			die(gettext("you do not have permission to perform "
			    "that operation.\n"));
		} else {
			die("%s", xrd_ctl);
		}
		/*NOTREACHED*/
	}

	if (allocflag) {
		alloc_xramdisk(ctl_fd, name, numpages);
	} else if (deleteflag) {
		delete_xramdisk(ctl_fd, name);
	} else {
		print_xramdisk();
	}

	return (E_SUCCESS);
}
