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
 * zoneadm is a command interpreter for zone administration.  It is all in
 * C (i.e., no lex/yacc), and all the argument passing is argc/argv based.
 * main() calls parse_and_run() which calls cmd_match(), then invokes the
 * appropriate command's handler function.  The rest of the program is the
 * handler functions and their helper functions.
 *
 * Some of the helper functions are used largely to simplify I18N: reducing
 * the need for translation notes.  This is particularly true of many of
 * the zerror() calls: doing e.g. zerror(gettext("%s failed"), "foo") rather
 * than zerror(gettext("foo failed")) with a translation note indicating
 * that "foo" need not be translated.
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include <zone.h>
#include <priv.h>
#include <locale.h>
#include <libintl.h>
#include <libzonecfg.h>
#include <bsm/adt.h>
#include <sys/brand.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <assert.h>
#include <sys/sockio.h>
#include <sys/mntent.h>
#include <limits.h>
#include <dirent.h>
#include <uuid/uuid.h>
#include <libdlpi.h>

#include <fcntl.h>
#include <door.h>
#include <macros.h>
#include <libgen.h>
#include <fnmatch.h>
#include <sys/modctl.h>
#include <libbrand.h>
#include <libscf.h>
#include <procfs.h>
#include <strings.h>

#include <pool.h>
#include <sys/pool.h>
#include <sys/priocntl.h>
#include <sys/fsspriocntl.h>

#include "zoneadm.h"

#define	MAXARGS	8

/* Reflects kernel zone entries */
typedef struct zone_entry {
	zoneid_t	zid;
	char		zname[ZONENAME_MAX];
	char		*zstate_str;
	zone_state_t	zstate_num;
	char		zbrand[MAXNAMELEN];
	char		zroot[MAXPATHLEN];
	char		zuuid[UUID_PRINTABLE_STRING_LENGTH];
	zone_iptype_t	ziptype;
} zone_entry_t;

#define	CLUSTER_BRAND_NAME	"cluster"

static zone_entry_t *zents;
static size_t nzents;
static boolean_t is_native_zone = B_TRUE;
static boolean_t is_cluster_zone = B_FALSE;

#define	LOOPBACK_IF	"lo0"
#define	SOCKET_AF(af)	(((af) == AF_UNSPEC) ? AF_INET : (af))

struct net_if {
	char	*name;
	int	af;
};

/* 0755 is the default directory mode. */
#define	DEFAULT_DIR_MODE \
	(S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH)

struct cmd {
	uint_t	cmd_num;				/* command number */
	char	*cmd_name;				/* command name */
	char	*short_usage;				/* short form help */
	int	(*handler)(int argc, char *argv[]);	/* function to call */

};

#define	SHELP_HELP	"help"
#define	SHELP_BOOT	"boot [-- boot_arguments]"
#define	SHELP_HALT	"halt"
#define	SHELP_READY	"ready"
#define	SHELP_REBOOT	"reboot [-- boot_arguments]"
#define	SHELP_LIST	"list [-cipv]"
#define	SHELP_VERIFY	"verify"
#define	SHELP_INSTALL	"install [-x nodataset] [brand-specific args]"
#define	SHELP_UNINSTALL	"uninstall [-F]"
#define	SHELP_CLONE	"clone [-m method] [-s <ZFS snapshot>] zonename"
#define	SHELP_MOVE	"move zonepath"
#define	SHELP_DETACH	"detach [-n]"
#define	SHELP_ATTACH	"attach [-F] [-n <path>] [-u]"
#define	SHELP_MARK	"mark incomplete"

#define	EXEC_PREFIX	"exec "
#define	EXEC_LEN	(strlen(EXEC_PREFIX))
#define	RMCOMMAND	"/usr/bin/rm -rf"

static int cleanup_zonepath(char *, boolean_t);


static int help_func(int argc, char *argv[]);
static int ready_func(int argc, char *argv[]);
static int boot_func(int argc, char *argv[]);
static int halt_func(int argc, char *argv[]);
static int reboot_func(int argc, char *argv[]);
static int list_func(int argc, char *argv[]);
static int verify_func(int argc, char *argv[]);
static int install_func(int argc, char *argv[]);
static int uninstall_func(int argc, char *argv[]);
static int mount_func(int argc, char *argv[]);
static int unmount_func(int argc, char *argv[]);
static int clone_func(int argc, char *argv[]);
static int move_func(int argc, char *argv[]);
static int detach_func(int argc, char *argv[]);
static int attach_func(int argc, char *argv[]);
static int mark_func(int argc, char *argv[]);
static int apply_func(int argc, char *argv[]);
static int sanity_check(char *zone, int cmd_num, boolean_t running,
    boolean_t unsafe_when_running, boolean_t force);
static int cmd_match(char *cmd);
static int verify_details(int, char *argv[]);
static int verify_brand(zone_dochandle_t, int, char *argv[]);
static int invoke_brand_handler(int, char *argv[]);

static struct cmd cmdtab[] = {
	{ CMD_HELP,		"help",		SHELP_HELP,	help_func },
	{ CMD_BOOT,		"boot",		SHELP_BOOT,	boot_func },
	{ CMD_HALT,		"halt",		SHELP_HALT,	halt_func },
	{ CMD_READY,		"ready",	SHELP_READY,	ready_func },
	{ CMD_REBOOT,		"reboot",	SHELP_REBOOT,	reboot_func },
	{ CMD_LIST,		"list",		SHELP_LIST,	list_func },
	{ CMD_VERIFY,		"verify",	SHELP_VERIFY,	verify_func },
	{ CMD_INSTALL,		"install",	SHELP_INSTALL,	install_func },
	{ CMD_UNINSTALL,	"uninstall",	SHELP_UNINSTALL,
	    uninstall_func },
	/* mount and unmount are private commands for admin/install */
	{ CMD_MOUNT,		"mount",	NULL,		mount_func },
	{ CMD_UNMOUNT,		"unmount",	NULL,		unmount_func },
	{ CMD_CLONE,		"clone",	SHELP_CLONE,	clone_func },
	{ CMD_MOVE,		"move",		SHELP_MOVE,	move_func },
	{ CMD_DETACH,		"detach",	SHELP_DETACH,	detach_func },
	{ CMD_ATTACH,		"attach",	SHELP_ATTACH,	attach_func },
	{ CMD_MARK,		"mark",		SHELP_MARK,	mark_func },
	{ CMD_APPLY,		"apply",	NULL,		apply_func }
};

/* global variables */

/* set early in main(), never modified thereafter, used all over the place */
static char *execname;
static char target_brand[MAXNAMELEN];
static char *locale;
char *target_zone;
static char *target_uuid;

/* used in do_subproc() and signal handler */
static volatile boolean_t child_killed;
/* used in attach_func() and signal handler */
static volatile boolean_t attach_interupted;
static int do_subproc_cnt = 0;

/*
 * Used to indicate whether this zoneadm instance has another zoneadm
 * instance in its ancestry.
 */
static boolean_t zoneadm_is_nested = B_FALSE;

/* used to track nested zone-lock operations */
static int zone_lock_cnt = 0;

/* used to communicate lock status to children */
#define	LOCK_ENV_VAR	"_ZONEADM_LOCK_HELD"
static char zoneadm_lock_held[] = LOCK_ENV_VAR"=1";
static char zoneadm_lock_not_held[] = LOCK_ENV_VAR"=0";

char *
cmd_to_str(int cmd_num)
{
	assert(cmd_num >= CMD_MIN && cmd_num <= CMD_MAX);
	return (cmdtab[cmd_num].cmd_name);
}

/* This is a separate function because of gettext() wrapping. */
static char *
long_help(int cmd_num)
{
	assert(cmd_num >= CMD_MIN && cmd_num <= CMD_MAX);
	switch (cmd_num) {
	case CMD_HELP:
		return (gettext("Print usage message."));
	case CMD_BOOT:
		return (gettext("Activates (boots) specified zone.  See "
		    "zoneadm(1m) for valid boot\n\targuments."));
	case CMD_HALT:
		return (gettext("Halts specified zone, bypassing shutdown "
		    "scripts and removing runtime\n\tresources of the zone."));
	case CMD_READY:
		return (gettext("Prepares a zone for running applications but "
		    "does not start any user\n\tprocesses in the zone."));
	case CMD_REBOOT:
		return (gettext("Restarts the zone (equivalent to a halt / "
		    "boot sequence).\n\tFails if the zone is not active.  "
		    "See zoneadm(1m) for valid boot\n\targuments."));
	case CMD_LIST:
		return (gettext("Lists the current zones, or a "
		    "specific zone if indicated.  By default,\n\tall "
		    "running zones are listed, though this can be "
		    "expanded to all\n\tinstalled zones with the -i "
		    "option or all configured zones with the\n\t-c "
		    "option.  When used with the general -z <zone> and/or -u "
		    "<uuid-match>\n\toptions, lists only the specified "
		    "matching zone, but lists it\n\tregardless of its state, "
		    "and the -i and -c options are disallowed.  The\n\t-v "
		    "option can be used to display verbose information: zone "
		    "name, id,\n\tcurrent state, root directory and options.  "
		    "The -p option can be used\n\tto request machine-parsable "
		    "output.  The -v and -p options are mutually\n\texclusive."
		    "  If neither -v nor -p is used, just the zone name is "
		    "listed."));
	case CMD_VERIFY:
		return (gettext("Check to make sure the configuration "
		    "can safely be instantiated\n\ton the machine: "
		    "physical network interfaces exist, etc."));
	case CMD_INSTALL:
		return (gettext("Install the configuration on to the system.  "
		    "The -x nodataset option\n\tcan be used to prevent the "
		    "creation of a new ZFS file system for the\n\tzone "
		    "(assuming the zonepath is within a ZFS file system).\n\t"
		    "All other arguments are passed to the brand installation "
		    "function;\n\tsee brand(4) for more information."));
	case CMD_UNINSTALL:
		return (gettext("Uninstall the configuration from the system.  "
		    "The -F flag can be used\n\tto force the action."));
	case CMD_CLONE:
		return (gettext("Clone the installation of another zone.  "
		    "The -m option can be used to\n\tspecify 'copy' which "
		    "forces a copy of the source zone.  The -s option\n\t"
		    "can be used to specify the name of a ZFS snapshot "
		    "that was taken from\n\ta previous clone command.  The "
		    "snapshot will be used as the source\n\tinstead of "
		    "creating a new ZFS snapshot."));
	case CMD_MOVE:
		return (gettext("Move the zone to a new zonepath."));
	case CMD_DETACH:
		return (gettext("Detach the zone from the system. The zone "
		    "state is changed to\n\t'configured' (but the files under "
		    "the zonepath are untouched).\n\tThe zone can subsequently "
		    "be attached, or can be moved to another\n\tsystem and "
		    "attached there.  The -n option can be used to specify\n\t"
		    "'no-execute' mode.  When -n is used, the information "
		    "needed to attach\n\tthe zone is sent to standard output "
		    "but the zone is not actually\n\tdetached."));
	case CMD_ATTACH:
		return (gettext("Attach the zone to the system.  The zone "
		    "state must be 'configured'\n\tprior to attach; upon "
		    "successful completion, the zone state will be\n\t"
		    "'installed'.  The system software on the current "
		    "system must be\n\tcompatible with the software on the "
		    "zone's original system or use\n\tthe -u option to update "
		    "the zone to the current system software.\n\tSpecify -F "
		    "to force the attach and skip software compatibility "
		    "tests.\n\tThe -n option can be used to specify "
		    "'no-execute' mode.  When -n is\n\tused, the information "
		    "needed to attach the zone is read from the\n\tspecified "
		    "path and the configuration is only validated.  The path "
		    "can\n\tbe '-' to specify standard input.  The -F, -n and "
		    "-u options are\n\tmutually exclusive."));
	case CMD_MARK:
		return (gettext("Set the state of the zone.  This can be used "
		    "to force the zone\n\tstate to 'incomplete' "
		    "administratively if some activity has rendered\n\tthe "
		    "zone permanently unusable.  The only valid state that "
		    "may be\n\tspecified is 'incomplete'."));
	default:
		return ("");
	}
	/* NOTREACHED */
	return (NULL);
}

/*
 * Called with explicit B_TRUE when help is explicitly requested, B_FALSE for
 * unexpected errors.
 */

static int
usage(boolean_t explicit)
{
	int i;
	FILE *fd = explicit ? stdout : stderr;

	(void) fprintf(fd, "%s:\t%s help\n", gettext("usage"), execname);
	(void) fprintf(fd, "\t%s [-z <zone>] [-u <uuid-match>] list\n",
	    execname);
	(void) fprintf(fd, "\t%s {-z <zone>|-u <uuid-match>} <%s>\n", execname,
	    gettext("subcommand"));
	(void) fprintf(fd, "\n%s:\n\n", gettext("Subcommands"));
	for (i = CMD_MIN; i <= CMD_MAX; i++) {
		if (cmdtab[i].short_usage == NULL)
			continue;
		(void) fprintf(fd, "%s\n", cmdtab[i].short_usage);
		if (explicit)
			(void) fprintf(fd, "\t%s\n\n", long_help(i));
	}
	if (!explicit)
		(void) fputs("\n", fd);
	return (Z_USAGE);
}

static void
sub_usage(char *short_usage, int cmd_num)
{
	(void) fprintf(stderr, "%s:\t%s\n", gettext("usage"), short_usage);
	(void) fprintf(stderr, "\t%s\n", long_help(cmd_num));
}

/*
 * zperror() is like perror(3c) except that this also prints the executable
 * name at the start of the message, and takes a boolean indicating whether
 * to call libc'c strerror() or that from libzonecfg.
 */

void
zperror(const char *str, boolean_t zonecfg_error)
{
	(void) fprintf(stderr, "%s: %s: %s\n", execname, str,
	    zonecfg_error ? zonecfg_strerror(errno) : strerror(errno));
}

/*
 * zperror2() is very similar to zperror() above, except it also prints a
 * supplied zone name after the executable.
 *
 * All current consumers of this function want libzonecfg's strerror() rather
 * than libc's; if this ever changes, this function can be made more generic
 * like zperror() above.
 */

void
zperror2(const char *zone, const char *str)
{
	(void) fprintf(stderr, "%s: %s: %s: %s\n", execname, zone, str,
	    zonecfg_strerror(errno));
}

/* PRINTFLIKE1 */
void
zerror(const char *fmt, ...)
{
	va_list alist;

	va_start(alist, fmt);
	(void) fprintf(stderr, "%s: ", execname);
	if (target_zone != NULL)
		(void) fprintf(stderr, "zone '%s': ", target_zone);
	(void) vfprintf(stderr, fmt, alist);
	(void) fprintf(stderr, "\n");
	va_end(alist);
}

static void *
safe_calloc(size_t nelem, size_t elsize)
{
	void *r = calloc(nelem, elsize);

	if (r == NULL) {
		zerror(gettext("failed to allocate %lu bytes: %s"),
		    (ulong_t)nelem * elsize, strerror(errno));
		exit(Z_ERR);
	}
	return (r);
}

static void
zone_print(zone_entry_t *zent, boolean_t verbose, boolean_t parsable)
{
	static boolean_t firsttime = B_TRUE;
	char *ip_type_str;

	if (zent->ziptype == ZS_EXCLUSIVE)
		ip_type_str = "excl";
	else
		ip_type_str = "shared";

	assert(!(verbose && parsable));
	if (firsttime && verbose) {
		firsttime = B_FALSE;
		(void) printf("%*s %-16s %-10s %-30s %-8s %-6s\n",
		    ZONEID_WIDTH, "ID", "NAME", "STATUS", "PATH", "BRAND",
		    "IP");
	}
	if (!verbose) {
		char *cp, *clim;

		if (!parsable) {
			(void) printf("%s\n", zent->zname);
			return;
		}
		if (zent->zid == ZONE_ID_UNDEFINED)
			(void) printf("-");
		else
			(void) printf("%lu", zent->zid);
		(void) printf(":%s:%s:", zent->zname, zent->zstate_str);
		cp = zent->zroot;
		while ((clim = strchr(cp, ':')) != NULL) {
			(void) printf("%.*s\\:", clim - cp, cp);
			cp = clim + 1;
		}
		(void) printf("%s:%s:%s:%s\n", cp, zent->zuuid, zent->zbrand,
		    ip_type_str);
		return;
	}
	if (zent->zstate_str != NULL) {
		if (zent->zid == ZONE_ID_UNDEFINED)
			(void) printf("%*s", ZONEID_WIDTH, "-");
		else
			(void) printf("%*lu", ZONEID_WIDTH, zent->zid);
		(void) printf(" %-16s %-10s %-30s %-8s %-6s\n", zent->zname,
		    zent->zstate_str, zent->zroot, zent->zbrand, ip_type_str);
	}
}

static int
lookup_zone_info(const char *zone_name, zoneid_t zid, zone_entry_t *zent)
{
	char root[MAXPATHLEN], *cp;
	int err;
	uuid_t uuid;

	(void) strlcpy(zent->zname, zone_name, sizeof (zent->zname));
	(void) strlcpy(zent->zroot, "???", sizeof (zent->zroot));
	(void) strlcpy(zent->zbrand, "???", sizeof (zent->zbrand));
	zent->zstate_str = "???";

	zent->zid = zid;

	if (zonecfg_get_uuid(zone_name, uuid) == Z_OK &&
	    !uuid_is_null(uuid))
		uuid_unparse(uuid, zent->zuuid);
	else
		zent->zuuid[0] = '\0';

	/*
	 * For labeled zones which query the zone path of lower-level
	 * zones, the path needs to be adjusted to drop the final
	 * "/root" component. This adjusted path is then useful
	 * for reading down any exported directories from the
	 * lower-level zone.
	 */
	if (is_system_labeled() && zent->zid != ZONE_ID_UNDEFINED) {
		if (zone_getattr(zent->zid, ZONE_ATTR_ROOT, zent->zroot,
		    sizeof (zent->zroot)) == -1) {
			zperror2(zent->zname,
			    gettext("could not get zone path."));
			return (Z_ERR);
		}
		cp = zent->zroot + strlen(zent->zroot) - 5;
		if (cp > zent->zroot && strcmp(cp, "/root") == 0)
			*cp = 0;
	} else {
		if ((err = zone_get_zonepath(zent->zname, root,
		    sizeof (root))) != Z_OK) {
			errno = err;
			zperror2(zent->zname,
			    gettext("could not get zone path."));
			return (Z_ERR);
		}
		(void) strlcpy(zent->zroot, root, sizeof (zent->zroot));
	}

	if ((err = zone_get_state(zent->zname, &zent->zstate_num)) != Z_OK) {
		errno = err;
		zperror2(zent->zname, gettext("could not get state"));
		return (Z_ERR);
	}
	zent->zstate_str = zone_state_str(zent->zstate_num);

	/*
	 * A zone's brand is only available in the .xml file describing it,
	 * which is only visible to the global zone.  This causes
	 * zone_get_brand() to fail when called from within a non-global
	 * zone.  Fortunately we only do this on labeled systems, where we
	 * know all zones are native.
	 */
	if (getzoneid() != GLOBAL_ZONEID) {
		assert(is_system_labeled() != 0);
		(void) strlcpy(zent->zbrand, NATIVE_BRAND_NAME,
		    sizeof (zent->zbrand));
	} else if (zone_get_brand(zent->zname, zent->zbrand,
	    sizeof (zent->zbrand)) != Z_OK) {
		zperror2(zent->zname, gettext("could not get brand name"));
		return (Z_ERR);
	}

	/*
	 * Get ip type of the zone.
	 * Note for global zone, ZS_SHARED is set always.
	 */
	if (zid == GLOBAL_ZONEID) {
		zent->ziptype = ZS_SHARED;
	} else {

		if (zent->zstate_num == ZONE_STATE_RUNNING) {
			ushort_t flags;

			if (zone_getattr(zid, ZONE_ATTR_FLAGS, &flags,
			    sizeof (flags)) < 0) {
				zperror2(zent->zname,
				    gettext("could not get zone flags"));
				return (Z_ERR);
			}
			if (flags & ZF_NET_EXCL)
				zent->ziptype = ZS_EXCLUSIVE;
			else
				zent->ziptype = ZS_SHARED;
		} else {
			zone_dochandle_t handle;

			if ((handle = zonecfg_init_handle()) == NULL) {
				zperror2(zent->zname,
				    gettext("could not init handle"));
				return (Z_ERR);
			}
			if ((err = zonecfg_get_handle(zent->zname, handle))
			    != Z_OK) {
				zperror2(zent->zname,
				    gettext("could not get handle"));
				zonecfg_fini_handle(handle);
				return (Z_ERR);
			}

			if ((err = zonecfg_get_iptype(handle, &zent->ziptype))
			    != Z_OK) {
				zperror2(zent->zname,
				    gettext("could not get ip-type"));
				zonecfg_fini_handle(handle);
				return (Z_ERR);
			}
			zonecfg_fini_handle(handle);
		}
	}

	return (Z_OK);
}

/*
 * fetch_zents() calls zone_list(2) to find out how many zones are running
 * (which is stored in the global nzents), then calls zone_list(2) again
 * to fetch the list of running zones (stored in the global zents).  This
 * function may be called multiple times, so if zents is already set, we
 * return immediately to save work.
 */

static int
fetch_zents(void)
{
	zoneid_t *zids = NULL;
	uint_t nzents_saved;
	int i, retv;
	FILE *fp;
	boolean_t inaltroot;
	zone_entry_t *zentp;

	if (nzents > 0)
		return (Z_OK);

	if (zone_list(NULL, &nzents) != 0) {
		zperror(gettext("failed to get zoneid list"), B_FALSE);
		return (Z_ERR);
	}

again:
	if (nzents == 0)
		return (Z_OK);

	zids = safe_calloc(nzents, sizeof (zoneid_t));
	nzents_saved = nzents;

	if (zone_list(zids, &nzents) != 0) {
		zperror(gettext("failed to get zone list"), B_FALSE);
		free(zids);
		return (Z_ERR);
	}
	if (nzents != nzents_saved) {
		/* list changed, try again */
		free(zids);
		goto again;
	}

	zents = safe_calloc(nzents, sizeof (zone_entry_t));

	inaltroot = zonecfg_in_alt_root();
	if (inaltroot)
		fp = zonecfg_open_scratch("", B_FALSE);
	else
		fp = NULL;
	zentp = zents;
	retv = Z_OK;
	for (i = 0; i < nzents; i++) {
		char name[ZONENAME_MAX];
		char altname[ZONENAME_MAX];

		if (getzonenamebyid(zids[i], name, sizeof (name)) < 0) {
			zperror(gettext("failed to get zone name"), B_FALSE);
			retv = Z_ERR;
			continue;
		}
		if (zonecfg_is_scratch(name)) {
			/* Ignore scratch zones by default */
			if (!inaltroot)
				continue;
			if (fp == NULL ||
			    zonecfg_reverse_scratch(fp, name, altname,
			    sizeof (altname), NULL, 0) == -1) {
				zerror(gettext("could not resolve scratch "
				    "zone %s"), name);
				retv = Z_ERR;
				continue;
			}
			(void) strcpy(name, altname);
		} else {
			/* Ignore non-scratch when in an alternate root */
			if (inaltroot && strcmp(name, GLOBAL_ZONENAME) != 0)
				continue;
		}
		if (lookup_zone_info(name, zids[i], zentp) != Z_OK) {
			zerror(gettext("failed to get zone data"));
			retv = Z_ERR;
			continue;
		}
		zentp++;
	}
	nzents = zentp - zents;
	if (fp != NULL)
		zonecfg_close_scratch(fp);

	free(zids);
	return (retv);
}

static int
zone_print_list(zone_state_t min_state, boolean_t verbose, boolean_t parsable)
{
	int i;
	zone_entry_t zent;
	FILE *cookie;
	char *name;

	/*
	 * First get the list of running zones from the kernel and print them.
	 * If that is all we need, then return.
	 */
	if ((i = fetch_zents()) != Z_OK) {
		/*
		 * No need for error messages; fetch_zents() has already taken
		 * care of this.
		 */
		return (i);
	}
	for (i = 0; i < nzents; i++)
		zone_print(&zents[i], verbose, parsable);
	if (min_state >= ZONE_STATE_RUNNING)
		return (Z_OK);
	/*
	 * Next, get the full list of zones from the configuration, skipping
	 * any we have already printed.
	 */
	cookie = setzoneent();
	while ((name = getzoneent(cookie)) != NULL) {
		for (i = 0; i < nzents; i++) {
			if (strcmp(zents[i].zname, name) == 0)
				break;
		}
		if (i < nzents) {
			free(name);
			continue;
		}
		if (lookup_zone_info(name, ZONE_ID_UNDEFINED, &zent) != Z_OK) {
			free(name);
			continue;
		}
		free(name);
		if (zent.zstate_num >= min_state)
			zone_print(&zent, verbose, parsable);
	}
	endzoneent(cookie);
	return (Z_OK);
}

static zone_entry_t *
lookup_running_zone(char *str)
{
	zoneid_t zoneid;
	char *cp;
	int i;

	if (fetch_zents() != Z_OK)
		return (NULL);

	for (i = 0; i < nzents; i++) {
		if (strcmp(str, zents[i].zname) == 0)
			return (&zents[i]);
	}
	errno = 0;
	zoneid = strtol(str, &cp, 0);
	if (zoneid < MIN_ZONEID || zoneid > MAX_ZONEID ||
	    errno != 0 || *cp != '\0')
		return (NULL);
	for (i = 0; i < nzents; i++) {
		if (zoneid == zents[i].zid)
			return (&zents[i]);
	}
	return (NULL);
}

/*
 * Check a bit in a mode_t: if on is B_TRUE, that bit should be on; if
 * B_FALSE, it should be off.  Return B_TRUE if the mode is bad (incorrect).
 */
static boolean_t
bad_mode_bit(mode_t mode, mode_t bit, boolean_t on, char *file)
{
	char *str;

	assert(bit == S_IRUSR || bit == S_IWUSR || bit == S_IXUSR ||
	    bit == S_IRGRP || bit == S_IWGRP || bit == S_IXGRP ||
	    bit == S_IROTH || bit == S_IWOTH || bit == S_IXOTH);
	/*
	 * TRANSLATION_NOTE
	 * The strings below will be used as part of a larger message,
	 * either:
	 * (file name) must be (owner|group|world) (read|writ|execut)able
	 * or
	 * (file name) must not be (owner|group|world) (read|writ|execut)able
	 */
	switch (bit) {
	case S_IRUSR:
		str = gettext("owner readable");
		break;
	case S_IWUSR:
		str = gettext("owner writable");
		break;
	case S_IXUSR:
		str = gettext("owner executable");
		break;
	case S_IRGRP:
		str = gettext("group readable");
		break;
	case S_IWGRP:
		str = gettext("group writable");
		break;
	case S_IXGRP:
		str = gettext("group executable");
		break;
	case S_IROTH:
		str = gettext("world readable");
		break;
	case S_IWOTH:
		str = gettext("world writable");
		break;
	case S_IXOTH:
		str = gettext("world executable");
		break;
	}
	if ((mode & bit) == (on ? 0 : bit)) {
		/*
		 * TRANSLATION_NOTE
		 * The first parameter below is a file name; the second
		 * is one of the "(owner|group|world) (read|writ|execut)able"
		 * strings from above.
		 */
		/*
		 * The code below could be simplified but not in a way
		 * that would easily translate to non-English locales.
		 */
		if (on) {
			(void) fprintf(stderr, gettext("%s must be %s.\n"),
			    file, str);
		} else {
			(void) fprintf(stderr, gettext("%s must not be %s.\n"),
			    file, str);
		}
		return (B_TRUE);
	}
	return (B_FALSE);
}

/*
 * We want to make sure that no zone has its zone path as a child node
 * (in the directory sense) of any other.  We do that by comparing this
 * zone's path to the path of all other (non-global) zones.  The comparison
 * in each case is simple: add '/' to the end of the path, then do a
 * strncmp() of the two paths, using the length of the shorter one.
 */

static int
crosscheck_zonepaths(char *path)
{
	char rpath[MAXPATHLEN];		/* resolved path */
	char path_copy[MAXPATHLEN];	/* copy of original path */
	char rpath_copy[MAXPATHLEN];	/* copy of original rpath */
	struct zoneent *ze;
	int res, err;
	FILE *cookie;

	cookie = setzoneent();
	while ((ze = getzoneent_private(cookie)) != NULL) {
		/* Skip zones which are not installed. */
		if (ze->zone_state < ZONE_STATE_INSTALLED) {
			free(ze);
			continue;
		}
		/* Skip the global zone and the current target zone. */
		if (strcmp(ze->zone_name, GLOBAL_ZONENAME) == 0 ||
		    strcmp(ze->zone_name, target_zone) == 0) {
			free(ze);
			continue;
		}
		if (strlen(ze->zone_path) == 0) {
			/* old index file without path, fall back */
			if ((err = zone_get_zonepath(ze->zone_name,
			    ze->zone_path, sizeof (ze->zone_path))) != Z_OK) {
				errno = err;
				zperror2(ze->zone_name,
				    gettext("could not get zone path"));
				free(ze);
				continue;
			}
		}
		(void) snprintf(path_copy, sizeof (path_copy), "%s%s",
		    zonecfg_get_root(), ze->zone_path);
		res = resolvepath(path_copy, rpath, sizeof (rpath));
		if (res == -1) {
			if (errno != ENOENT) {
				zperror(path_copy, B_FALSE);
				free(ze);
				return (Z_ERR);
			}
			(void) printf(gettext("WARNING: zone %s is installed, "
			    "but its %s %s does not exist.\n"), ze->zone_name,
			    "zonepath", path_copy);
			free(ze);
			continue;
		}
		rpath[res] = '\0';
		(void) snprintf(path_copy, sizeof (path_copy), "%s/", path);
		(void) snprintf(rpath_copy, sizeof (rpath_copy), "%s/", rpath);
		if (strncmp(path_copy, rpath_copy,
		    min(strlen(path_copy), strlen(rpath_copy))) == 0) {
			/*
			 * TRANSLATION_NOTE
			 * zonepath is a literal that should not be translated.
			 */
			(void) fprintf(stderr, gettext("%s zonepath (%s) and "
			    "%s zonepath (%s) overlap.\n"),
			    target_zone, path, ze->zone_name, rpath);
			free(ze);
			return (Z_ERR);
		}
		free(ze);
	}
	endzoneent(cookie);
	return (Z_OK);
}

static int
validate_zonepath(char *path, int cmd_num)
{
	int res;			/* result of last library/system call */
	boolean_t err = B_FALSE;	/* have we run into an error? */
	struct stat stbuf;
	struct statvfs64 vfsbuf;
	char rpath[MAXPATHLEN];		/* resolved path */
	char ppath[MAXPATHLEN];		/* parent path */
	char rppath[MAXPATHLEN];	/* resolved parent path */
	char rootpath[MAXPATHLEN];	/* root path */
	zone_state_t state;

	if (path[0] != '/') {
		(void) fprintf(stderr,
		    gettext("%s is not an absolute path.\n"), path);
		return (Z_ERR);
	}
	if ((res = resolvepath(path, rpath, sizeof (rpath))) == -1) {
		if ((errno != ENOENT) ||
		    (cmd_num != CMD_VERIFY && cmd_num != CMD_INSTALL &&
		    cmd_num != CMD_CLONE && cmd_num != CMD_MOVE)) {
			zperror(path, B_FALSE);
			return (Z_ERR);
		}
		if (cmd_num == CMD_VERIFY) {
			/*
			 * TRANSLATION_NOTE
			 * zoneadm is a literal that should not be translated.
			 */
			(void) fprintf(stderr, gettext("WARNING: %s does not "
			    "exist, so it could not be verified.\nWhen "
			    "'zoneadm %s' is run, '%s' will try to create\n%s, "
			    "and '%s' will be tried again,\nbut the '%s' may "
			    "fail if:\nthe parent directory of %s is group- or "
			    "other-writable\nor\n%s overlaps with any other "
			    "installed zones.\n"), path,
			    cmd_to_str(CMD_INSTALL), cmd_to_str(CMD_INSTALL),
			    path, cmd_to_str(CMD_VERIFY),
			    cmd_to_str(CMD_VERIFY), path, path);
			return (Z_OK);
		}
		/*
		 * The zonepath is supposed to be mode 700 but its
		 * parent(s) 755.  So use 755 on the mkdirp() then
		 * chmod() the zonepath itself to 700.
		 */
		if (mkdirp(path, DEFAULT_DIR_MODE) < 0) {
			zperror(path, B_FALSE);
			return (Z_ERR);
		}
		/*
		 * If the chmod() fails, report the error, but might
		 * as well continue the verify procedure.
		 */
		if (chmod(path, S_IRWXU) != 0)
			zperror(path, B_FALSE);
		/*
		 * Since the mkdir() succeeded, we should not have to
		 * worry about a subsequent ENOENT, thus this should
		 * only recurse once.
		 */
		return (validate_zonepath(path, cmd_num));
	}
	rpath[res] = '\0';
	if (strcmp(path, rpath) != 0) {
		errno = Z_RESOLVED_PATH;
		zperror(path, B_TRUE);
		return (Z_ERR);
	}
	if ((res = stat(rpath, &stbuf)) != 0) {
		zperror(rpath, B_FALSE);
		return (Z_ERR);
	}
	if (!S_ISDIR(stbuf.st_mode)) {
		(void) fprintf(stderr, gettext("%s is not a directory.\n"),
		    rpath);
		return (Z_ERR);
	}
	if (strcmp(stbuf.st_fstype, MNTTYPE_TMPFS) == 0) {
		(void) printf(gettext("WARNING: %s is on a temporary "
		    "file system.\n"), rpath);
	}
	if (crosscheck_zonepaths(rpath) != Z_OK)
		return (Z_ERR);
	/*
	 * Try to collect and report as many minor errors as possible
	 * before returning, so the user can learn everything that needs
	 * to be fixed up front.
	 */
	if (stbuf.st_uid != 0) {
		(void) fprintf(stderr, gettext("%s is not owned by root.\n"),
		    rpath);
		err = B_TRUE;
	}
	err |= bad_mode_bit(stbuf.st_mode, S_IRUSR, B_TRUE, rpath);
	err |= bad_mode_bit(stbuf.st_mode, S_IWUSR, B_TRUE, rpath);
	err |= bad_mode_bit(stbuf.st_mode, S_IXUSR, B_TRUE, rpath);
	err |= bad_mode_bit(stbuf.st_mode, S_IRGRP, B_FALSE, rpath);
	err |= bad_mode_bit(stbuf.st_mode, S_IWGRP, B_FALSE, rpath);
	err |= bad_mode_bit(stbuf.st_mode, S_IXGRP, B_FALSE, rpath);
	err |= bad_mode_bit(stbuf.st_mode, S_IROTH, B_FALSE, rpath);
	err |= bad_mode_bit(stbuf.st_mode, S_IWOTH, B_FALSE, rpath);
	err |= bad_mode_bit(stbuf.st_mode, S_IXOTH, B_FALSE, rpath);

	(void) snprintf(ppath, sizeof (ppath), "%s/..", path);
	if ((res = resolvepath(ppath, rppath, sizeof (rppath))) == -1) {
		zperror(ppath, B_FALSE);
		return (Z_ERR);
	}
	rppath[res] = '\0';
	if ((res = stat(rppath, &stbuf)) != 0) {
		zperror(rppath, B_FALSE);
		return (Z_ERR);
	}
	/* theoretically impossible */
	if (!S_ISDIR(stbuf.st_mode)) {
		(void) fprintf(stderr, gettext("%s is not a directory.\n"),
		    rppath);
		return (Z_ERR);
	}
	if (stbuf.st_uid != 0) {
		(void) fprintf(stderr, gettext("%s is not owned by root.\n"),
		    rppath);
		err = B_TRUE;
	}
	err |= bad_mode_bit(stbuf.st_mode, S_IRUSR, B_TRUE, rppath);
	err |= bad_mode_bit(stbuf.st_mode, S_IWUSR, B_TRUE, rppath);
	err |= bad_mode_bit(stbuf.st_mode, S_IXUSR, B_TRUE, rppath);
	err |= bad_mode_bit(stbuf.st_mode, S_IWGRP, B_FALSE, rppath);
	err |= bad_mode_bit(stbuf.st_mode, S_IWOTH, B_FALSE, rppath);
	if (strcmp(rpath, rppath) == 0) {
		(void) fprintf(stderr, gettext("%s is its own parent.\n"),
		    rppath);
		err = B_TRUE;
	}

	if (statvfs64(rpath, &vfsbuf) != 0) {
		zperror(rpath, B_FALSE);
		return (Z_ERR);
	}
	if (strcmp(vfsbuf.f_basetype, MNTTYPE_NFS) == 0) {
		/*
		 * TRANSLATION_NOTE
		 * Zonepath and NFS are literals that should not be translated.
		 */
		(void) fprintf(stderr, gettext("Zonepath %s is on an NFS "
		    "mounted file system.\n"
		    "\tA local file system must be used.\n"), rpath);
		return (Z_ERR);
	}
	if (vfsbuf.f_flag & ST_NOSUID) {
		/*
		 * TRANSLATION_NOTE
		 * Zonepath and nosuid are literals that should not be
		 * translated.
		 */
		(void) fprintf(stderr, gettext("Zonepath %s is on a nosuid "
		    "file system.\n"), rpath);
		return (Z_ERR);
	}

	if ((res = zone_get_state(target_zone, &state)) != Z_OK) {
		errno = res;
		zperror2(target_zone, gettext("could not get state"));
		return (Z_ERR);
	}
	/*
	 * The existence of the root path is only bad in the configured state,
	 * as it is *supposed* to be there at the installed and later states.
	 * However, the root path is expected to be there if the zone is
	 * detached.
	 * State/command mismatches are caught earlier in verify_details().
	 */
	if (state == ZONE_STATE_CONFIGURED && cmd_num != CMD_ATTACH) {
		if (snprintf(rootpath, sizeof (rootpath), "%s/root", rpath) >=
		    sizeof (rootpath)) {
			/*
			 * TRANSLATION_NOTE
			 * Zonepath is a literal that should not be translated.
			 */
			(void) fprintf(stderr,
			    gettext("Zonepath %s is too long.\n"), rpath);
			return (Z_ERR);
		}
		if ((res = stat(rootpath, &stbuf)) == 0) {
			if (zonecfg_detached(rpath))
				(void) fprintf(stderr,
				    gettext("Cannot %s detached "
				    "zone.\nUse attach or remove %s "
				    "directory.\n"), cmd_to_str(cmd_num),
				    rpath);
			else
				(void) fprintf(stderr,
				    gettext("Rootpath %s exists; "
				    "remove or move aside prior to %s.\n"),
				    rootpath, cmd_to_str(cmd_num));
			return (Z_ERR);
		}
	}

	return (err ? Z_ERR : Z_OK);
}

/*
 * The following two routines implement a simple locking mechanism to
 * ensure that only one instance of zoneadm at a time is able to manipulate
 * a given zone.  The lock is built on top of an fcntl(2) lock of
 * [<altroot>]/var/run/zones/<zonename>.zoneadm.lock.  If a zoneadm instance
 * can grab that lock, it is allowed to manipulate the zone.
 *
 * Since zoneadm may call external applications which in turn invoke
 * zoneadm again, we introduce the notion of "lock inheritance".  Any
 * instance of zoneadm that has another instance in its ancestry is assumed
 * to be acting on behalf of the original zoneadm, and is thus allowed to
 * manipulate its zone.
 *
 * This inheritance is implemented via the _ZONEADM_LOCK_HELD environment
 * variable.  When zoneadm is granted a lock on its zone, this environment
 * variable is set to 1.  When it releases the lock, the variable is set to
 * 0.  Since a child process inherits its parent's environment, checking
 * the state of this variable indicates whether or not any ancestor owns
 * the lock.
 */
static void
release_lock_file(int lockfd)
{
	/*
	 * If we are cleaning up from a failed attempt to lock the zone for
	 * the first time, we might have a zone_lock_cnt of 0.  In that
	 * error case, we don't want to do anything but close the lock
	 * file.
	 */
	assert(zone_lock_cnt >= 0);
	if (zone_lock_cnt > 0) {
		assert(getenv(LOCK_ENV_VAR) != NULL);
		assert(atoi(getenv(LOCK_ENV_VAR)) == 1);
		if (--zone_lock_cnt > 0) {
			assert(lockfd == -1);
			return;
		}
		if (putenv(zoneadm_lock_not_held) != 0) {
			zperror(target_zone, B_TRUE);
			exit(Z_ERR);
		}
	}
	assert(lockfd >= 0);
	(void) close(lockfd);
}

static int
grab_lock_file(const char *zone_name, int *lockfd)
{
	char pathbuf[PATH_MAX];
	struct flock flock;

	/*
	 * If we already have the lock, we can skip this expensive song
	 * and dance.
	 */
	if (zone_lock_cnt > 0) {
		zone_lock_cnt++;
		*lockfd = -1;
		return (Z_OK);
	}
	assert(getenv(LOCK_ENV_VAR) != NULL);
	assert(atoi(getenv(LOCK_ENV_VAR)) == 0);

	if (snprintf(pathbuf, sizeof (pathbuf), "%s%s", zonecfg_get_root(),
	    ZONES_TMPDIR) >= sizeof (pathbuf)) {
		zerror(gettext("alternate root path is too long"));
		return (Z_ERR);
	}
	if (mkdir(pathbuf, S_IRWXU) < 0 && errno != EEXIST) {
		zerror(gettext("could not mkdir %s: %s"), pathbuf,
		    strerror(errno));
		return (Z_ERR);
	}
	(void) chmod(pathbuf, S_IRWXU);

	/*
	 * One of these lock files is created for each zone (when needed).
	 * The lock files are not cleaned up (except on system reboot),
	 * but since there is only one per zone, there is no resource
	 * starvation issue.
	 */
	if (snprintf(pathbuf, sizeof (pathbuf), "%s%s/%s.zoneadm.lock",
	    zonecfg_get_root(), ZONES_TMPDIR, zone_name) >= sizeof (pathbuf)) {
		zerror(gettext("alternate root path is too long"));
		return (Z_ERR);
	}
	if ((*lockfd = open(pathbuf, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR)) < 0) {
		zerror(gettext("could not open %s: %s"), pathbuf,
		    strerror(errno));
		return (Z_ERR);
	}
	/*
	 * Lock the file to synchronize with other zoneadmds
	 */
	flock.l_type = F_WRLCK;
	flock.l_whence = SEEK_SET;
	flock.l_start = (off_t)0;
	flock.l_len = (off_t)0;
	if ((fcntl(*lockfd, F_SETLKW, &flock) < 0) ||
	    (putenv(zoneadm_lock_held) != 0)) {
		zerror(gettext("unable to lock %s: %s"), pathbuf,
		    strerror(errno));
		release_lock_file(*lockfd);
		return (Z_ERR);
	}
	zone_lock_cnt = 1;
	return (Z_OK);
}

static boolean_t
get_doorname(const char *zone_name, char *buffer)
{
	return (snprintf(buffer, PATH_MAX, "%s" ZONE_DOOR_PATH,
	    zonecfg_get_root(), zone_name) < PATH_MAX);
}

/*
 * system daemons are not audited.  For the global zone, this occurs
 * "naturally" since init is started with the default audit
 * characteristics.  Since zoneadmd is a system daemon and it starts
 * init for a zone, it is necessary to clear out the audit
 * characteristics inherited from whomever started zoneadmd.  This is
 * indicated by the audit id, which is set from the ruid parameter of
 * adt_set_user(), below.
 */

static void
prepare_audit_context()
{
	adt_session_data_t	*ah;
	char			*failure = gettext("audit failure: %s");

	if (adt_start_session(&ah, NULL, 0)) {
		zerror(failure, strerror(errno));
		return;
	}
	if (adt_set_user(ah, ADT_NO_AUDIT, ADT_NO_AUDIT,
	    ADT_NO_AUDIT, ADT_NO_AUDIT, NULL, ADT_NEW)) {
		zerror(failure, strerror(errno));
		(void) adt_end_session(ah);
		return;
	}
	if (adt_set_proc(ah))
		zerror(failure, strerror(errno));

	(void) adt_end_session(ah);
}

static int
start_zoneadmd(const char *zone_name)
{
	char doorpath[PATH_MAX];
	pid_t child_pid;
	int error = Z_ERR;
	int doorfd, lockfd;
	struct door_info info;

	if (!get_doorname(zone_name, doorpath))
		return (Z_ERR);

	if (grab_lock_file(zone_name, &lockfd) != Z_OK)
		return (Z_ERR);

	/*
	 * Now that we have the lock, re-confirm that the daemon is
	 * *not* up and working fine.  If it is still down, we have a green
	 * light to start it.
	 */
	if ((doorfd = open(doorpath, O_RDONLY)) < 0) {
		if (errno != ENOENT) {
			zperror(doorpath, B_FALSE);
			goto out;
		}
	} else {
		if (door_info(doorfd, &info) == 0 &&
		    ((info.di_attributes & DOOR_REVOKED) == 0)) {
			error = Z_OK;
			(void) close(doorfd);
			goto out;
		}
		(void) close(doorfd);
	}

	if ((child_pid = fork()) == -1) {
		zperror(gettext("could not fork"), B_FALSE);
		goto out;
	} else if (child_pid == 0) {
		const char *argv[6], **ap;

		/* child process */
		prepare_audit_context();

		ap = argv;
		*ap++ = "zoneadmd";
		*ap++ = "-z";
		*ap++ = zone_name;
		if (zonecfg_in_alt_root()) {
			*ap++ = "-R";
			*ap++ = zonecfg_get_root();
		}
		*ap = NULL;

		(void) execv("/usr/lib/zones/zoneadmd", (char * const *)argv);
		/*
		 * TRANSLATION_NOTE
		 * zoneadmd is a literal that should not be translated.
		 */
		zperror(gettext("could not exec zoneadmd"), B_FALSE);
		_exit(Z_ERR);
	} else {
		/* parent process */
		pid_t retval;
		int pstatus = 0;

		do {
			retval = waitpid(child_pid, &pstatus, 0);
		} while (retval != child_pid);
		if (WIFSIGNALED(pstatus) || (WIFEXITED(pstatus) &&
		    WEXITSTATUS(pstatus) != 0)) {
			zerror(gettext("could not start %s"), "zoneadmd");
			goto out;
		}
	}
	error = Z_OK;
out:
	release_lock_file(lockfd);
	return (error);
}

static int
ping_zoneadmd(const char *zone_name)
{
	char doorpath[PATH_MAX];
	int doorfd;
	struct door_info info;

	if (!get_doorname(zone_name, doorpath))
		return (Z_ERR);

	if ((doorfd = open(doorpath, O_RDONLY)) < 0) {
		return (Z_ERR);
	}
	if (door_info(doorfd, &info) == 0 &&
	    ((info.di_attributes & DOOR_REVOKED) == 0)) {
		(void) close(doorfd);
		return (Z_OK);
	}
	(void) close(doorfd);
	return (Z_ERR);
}

static int
call_zoneadmd(const char *zone_name, zone_cmd_arg_t *arg)
{
	char doorpath[PATH_MAX];
	int doorfd, result;
	door_arg_t darg;

	zoneid_t zoneid;
	uint64_t uniqid = 0;

	zone_cmd_rval_t *rvalp;
	size_t rlen;
	char *cp, *errbuf;

	rlen = getpagesize();
	if ((rvalp = malloc(rlen)) == NULL) {
		zerror(gettext("failed to allocate %lu bytes: %s"), rlen,
		    strerror(errno));
		return (-1);
	}

	if ((zoneid = getzoneidbyname(zone_name)) != ZONE_ID_UNDEFINED) {
		(void) zone_getattr(zoneid, ZONE_ATTR_UNIQID, &uniqid,
		    sizeof (uniqid));
	}
	arg->uniqid = uniqid;
	(void) strlcpy(arg->locale, locale, sizeof (arg->locale));
	if (!get_doorname(zone_name, doorpath)) {
		zerror(gettext("alternate root path is too long"));
		free(rvalp);
		return (-1);
	}

	/*
	 * Loop trying to start zoneadmd; if something goes seriously
	 * wrong we break out and fail.
	 */
	for (;;) {
		if (start_zoneadmd(zone_name) != Z_OK)
			break;

		if ((doorfd = open(doorpath, O_RDONLY)) < 0) {
			zperror(gettext("failed to open zone door"), B_FALSE);
			break;
		}

		darg.data_ptr = (char *)arg;
		darg.data_size = sizeof (*arg);
		darg.desc_ptr = NULL;
		darg.desc_num = 0;
		darg.rbuf = (char *)rvalp;
		darg.rsize = rlen;
		if (door_call(doorfd, &darg) != 0) {
			(void) close(doorfd);
			/*
			 * We'll get EBADF if the door has been revoked.
			 */
			if (errno != EBADF) {
				zperror(gettext("door_call failed"), B_FALSE);
				break;
			}
			continue;	/* take another lap */
		}
		(void) close(doorfd);

		if (darg.data_size == 0) {
			/* Door server is going away; kick it again. */
			continue;
		}

		errbuf = rvalp->errbuf;
		while (*errbuf != '\0') {
			/*
			 * Remove any newlines since zerror()
			 * will append one automatically.
			 */
			cp = strchr(errbuf, '\n');
			if (cp != NULL)
				*cp = '\0';
			zerror("%s", errbuf);
			if (cp == NULL)
				break;
			errbuf = cp + 1;
		}
		result = rvalp->rval == 0 ? 0 : -1;
		free(rvalp);
		return (result);
	}

	free(rvalp);
	return (-1);
}

static int
invoke_brand_handler(int cmd_num, char *argv[])
{
	zone_dochandle_t handle;
	int err;

	if ((handle = zonecfg_init_handle()) == NULL) {
		zperror(cmd_to_str(cmd_num), B_TRUE);
		return (Z_ERR);
	}
	if ((err = zonecfg_get_handle(target_zone, handle)) != Z_OK) {
		errno = err;
		zperror(cmd_to_str(cmd_num), B_TRUE);
		zonecfg_fini_handle(handle);
		return (Z_ERR);
	}
	if (verify_brand(handle, cmd_num, argv) != Z_OK) {
		zonecfg_fini_handle(handle);
		return (Z_ERR);
	}
	zonecfg_fini_handle(handle);
	return (Z_OK);
}

static int
ready_func(int argc, char *argv[])
{
	zone_cmd_arg_t zarg;
	int arg;

	if (zonecfg_in_alt_root()) {
		zerror(gettext("cannot ready zone in alternate root"));
		return (Z_ERR);
	}

	optind = 0;
	if ((arg = getopt(argc, argv, "?")) != EOF) {
		switch (arg) {
		case '?':
			sub_usage(SHELP_READY, CMD_READY);
			return (optopt == '?' ? Z_OK : Z_USAGE);
		default:
			sub_usage(SHELP_READY, CMD_READY);
			return (Z_USAGE);
		}
	}
	if (argc > optind) {
		sub_usage(SHELP_READY, CMD_READY);
		return (Z_USAGE);
	}
	if (sanity_check(target_zone, CMD_READY, B_FALSE, B_FALSE, B_FALSE)
	    != Z_OK)
		return (Z_ERR);
	if (verify_details(CMD_READY, argv) != Z_OK)
		return (Z_ERR);

	zarg.cmd = Z_READY;
	if (call_zoneadmd(target_zone, &zarg) != 0) {
		zerror(gettext("call to %s failed"), "zoneadmd");
		return (Z_ERR);
	}
	return (Z_OK);
}

static int
boot_func(int argc, char *argv[])
{
	zone_cmd_arg_t zarg;
	boolean_t force = B_FALSE;
	int arg;

	if (zonecfg_in_alt_root()) {
		zerror(gettext("cannot boot zone in alternate root"));
		return (Z_ERR);
	}

	zarg.bootbuf[0] = '\0';

	/*
	 * The following getopt processes arguments to zone boot; that
	 * is to say, the [here] portion of the argument string:
	 *
	 *	zoneadm -z myzone boot [here] -- -v -m verbose
	 *
	 * Where [here] can either be nothing, -? (in which case we bail
	 * and print usage), -f (a private option to indicate that the
	 * boot operation should be 'forced'), or -s.  Support for -s is
	 * vestigal and obsolete, but is retained because it was a
	 * documented interface and there are known consumers including
	 * admin/install; the proper way to specify boot arguments like -s
	 * is:
	 *
	 *	zoneadm -z myzone boot -- -s -v -m verbose.
	 */
	optind = 0;
	while ((arg = getopt(argc, argv, "?fs")) != EOF) {
		switch (arg) {
		case '?':
			sub_usage(SHELP_BOOT, CMD_BOOT);
			return (optopt == '?' ? Z_OK : Z_USAGE);
		case 's':
			(void) strlcpy(zarg.bootbuf, "-s",
			    sizeof (zarg.bootbuf));
			break;
		case 'f':
			force = B_TRUE;
			break;
		default:
			sub_usage(SHELP_BOOT, CMD_BOOT);
			return (Z_USAGE);
		}
	}

	for (; optind < argc; optind++) {
		if (strlcat(zarg.bootbuf, argv[optind],
		    sizeof (zarg.bootbuf)) >= sizeof (zarg.bootbuf)) {
			zerror(gettext("Boot argument list too long"));
			return (Z_ERR);
		}
		if (optind < argc - 1)
			if (strlcat(zarg.bootbuf, " ", sizeof (zarg.bootbuf)) >=
			    sizeof (zarg.bootbuf)) {
				zerror(gettext("Boot argument list too long"));
				return (Z_ERR);
			}
	}
	if (sanity_check(target_zone, CMD_BOOT, B_FALSE, B_FALSE, force)
	    != Z_OK)
		return (Z_ERR);
	if (verify_details(CMD_BOOT, argv) != Z_OK)
		return (Z_ERR);
	zarg.cmd = force ? Z_FORCEBOOT : Z_BOOT;
	if (call_zoneadmd(target_zone, &zarg) != 0) {
		zerror(gettext("call to %s failed"), "zoneadmd");
		return (Z_ERR);
	}

	return (Z_OK);
}

static void
fake_up_local_zone(zoneid_t zid, zone_entry_t *zeptr)
{
	ssize_t result;
	uuid_t uuid;
	FILE *fp;
	ushort_t flags;

	(void) memset(zeptr, 0, sizeof (*zeptr));

	zeptr->zid = zid;

	/*
	 * Since we're looking up our own (non-global) zone name,
	 * we can be assured that it will succeed.
	 */
	result = getzonenamebyid(zid, zeptr->zname, sizeof (zeptr->zname));
	assert(result >= 0);
	if (zonecfg_is_scratch(zeptr->zname) &&
	    (fp = zonecfg_open_scratch("", B_FALSE)) != NULL) {
		(void) zonecfg_reverse_scratch(fp, zeptr->zname, zeptr->zname,
		    sizeof (zeptr->zname), NULL, 0);
		zonecfg_close_scratch(fp);
	}

	if (is_system_labeled()) {
		(void) zone_getattr(zid, ZONE_ATTR_ROOT, zeptr->zroot,
		    sizeof (zeptr->zroot));
		(void) strlcpy(zeptr->zbrand, NATIVE_BRAND_NAME,
		    sizeof (zeptr->zbrand));
	} else {
		(void) strlcpy(zeptr->zroot, "/", sizeof (zeptr->zroot));
		(void) zone_getattr(zid, ZONE_ATTR_BRAND, zeptr->zbrand,
		    sizeof (zeptr->zbrand));
	}

	zeptr->zstate_str = "running";
	if (zonecfg_get_uuid(zeptr->zname, uuid) == Z_OK &&
	    !uuid_is_null(uuid))
		uuid_unparse(uuid, zeptr->zuuid);

	if (zone_getattr(zid, ZONE_ATTR_FLAGS, &flags, sizeof (flags)) < 0) {
		zperror2(zeptr->zname, gettext("could not get zone flags"));
		exit(Z_ERR);
	}
	if (flags & ZF_NET_EXCL)
		zeptr->ziptype = ZS_EXCLUSIVE;
	else
		zeptr->ziptype = ZS_SHARED;
}

static int
list_func(int argc, char *argv[])
{
	zone_entry_t *zentp, zent;
	int arg, retv;
	boolean_t output = B_FALSE, verbose = B_FALSE, parsable = B_FALSE;
	zone_state_t min_state = ZONE_STATE_RUNNING;
	zoneid_t zone_id = getzoneid();

	if (target_zone == NULL) {
		/* all zones: default view to running but allow override */
		optind = 0;
		while ((arg = getopt(argc, argv, "?cipv")) != EOF) {
			switch (arg) {
			case '?':
				sub_usage(SHELP_LIST, CMD_LIST);
				return (optopt == '?' ? Z_OK : Z_USAGE);
				/*
				 * The 'i' and 'c' options are not mutually
				 * exclusive so if 'c' is given, then min_state
				 * is set to 0 (ZONE_STATE_CONFIGURED) which is
				 * the lowest possible state.  If 'i' is given,
				 * then min_state is set to be the lowest state
				 * so far.
				 */
			case 'c':
				min_state = ZONE_STATE_CONFIGURED;
				break;
			case 'i':
				min_state = min(ZONE_STATE_INSTALLED,
				    min_state);

				break;
			case 'p':
				parsable = B_TRUE;
				break;
			case 'v':
				verbose = B_TRUE;
				break;
			default:
				sub_usage(SHELP_LIST, CMD_LIST);
				return (Z_USAGE);
			}
		}
		if (parsable && verbose) {
			zerror(gettext("%s -p and -v are mutually exclusive."),
			    cmd_to_str(CMD_LIST));
			return (Z_ERR);
		}
		if (zone_id == GLOBAL_ZONEID || is_system_labeled()) {
			retv = zone_print_list(min_state, verbose, parsable);
		} else {
			fake_up_local_zone(zone_id, &zent);
			retv = Z_OK;
			zone_print(&zent, verbose, parsable);
		}
		return (retv);
	}

	/*
	 * Specific target zone: disallow -i/-c suboptions.
	 */
	optind = 0;
	while ((arg = getopt(argc, argv, "?pv")) != EOF) {
		switch (arg) {
		case '?':
			sub_usage(SHELP_LIST, CMD_LIST);
			return (optopt == '?' ? Z_OK : Z_USAGE);
		case 'p':
			parsable = B_TRUE;
			break;
		case 'v':
			verbose = B_TRUE;
			break;
		default:
			sub_usage(SHELP_LIST, CMD_LIST);
			return (Z_USAGE);
		}
	}
	if (parsable && verbose) {
		zerror(gettext("%s -p and -v are mutually exclusive."),
		    cmd_to_str(CMD_LIST));
		return (Z_ERR);
	}
	if (argc > optind) {
		sub_usage(SHELP_LIST, CMD_LIST);
		return (Z_USAGE);
	}
	if (zone_id != GLOBAL_ZONEID && !is_system_labeled()) {
		fake_up_local_zone(zone_id, &zent);
		/*
		 * main() will issue a Z_NO_ZONE error if it cannot get an
		 * id for target_zone, which in a non-global zone should
		 * happen for any zone name except `zonename`.  Thus we
		 * assert() that here but don't otherwise check.
		 */
		assert(strcmp(zent.zname, target_zone) == 0);
		zone_print(&zent, verbose, parsable);
		output = B_TRUE;
	} else if ((zentp = lookup_running_zone(target_zone)) != NULL) {
		zone_print(zentp, verbose, parsable);
		output = B_TRUE;
	} else if (lookup_zone_info(target_zone, ZONE_ID_UNDEFINED,
	    &zent) == Z_OK) {
		zone_print(&zent, verbose, parsable);
		output = B_TRUE;
	}

	/*
	 * Invoke brand-specific handler. Note that we do this
	 * only if we're in the global zone, and target_zone is specified
	 * and it is not the global zone.
	 */
	if (zone_id == GLOBAL_ZONEID && target_zone != NULL &&
	    strcmp(target_zone, GLOBAL_ZONENAME) != 0)
		if (invoke_brand_handler(CMD_LIST, argv) != Z_OK)
			return (Z_ERR);

	return (output ? Z_OK : Z_ERR);
}

static void
sigterm(int sig)
{
	/*
	 * Ignore SIG{INT,TERM}, so we don't end up in an infinite loop,
	 * then propagate the signal to our process group.
	 */
	assert(sig == SIGINT || sig == SIGTERM);
	(void) sigset(SIGINT, SIG_IGN);
	(void) sigset(SIGTERM, SIG_IGN);
	(void) kill(0, sig);
	child_killed = B_TRUE;
}

static int
do_subproc(char *cmdbuf)
{
	char inbuf[1024];	/* arbitrary large amount */
	FILE *file;

	do_subproc_cnt++;
	child_killed = B_FALSE;
	/*
	 * We use popen(3c) to launch child processes for [un]install;
	 * this library call does not return a PID, so we have to kill
	 * the whole process group.  To avoid killing our parent, we
	 * become a process group leader here.  But doing so can wreak
	 * havoc with reading from stdin when launched by a non-job-control
	 * shell, so we close stdin and reopen it as /dev/null first.
	 */
	(void) close(STDIN_FILENO);
	(void) openat(STDIN_FILENO, "/dev/null", O_RDONLY);
	if (!zoneadm_is_nested)
		(void) setpgid(0, 0);
	(void) sigset(SIGINT, sigterm);
	(void) sigset(SIGTERM, sigterm);
	file = popen(cmdbuf, "r");
	for (;;) {
		if (child_killed || fgets(inbuf, sizeof (inbuf), file) == NULL)
			break;
		(void) fputs(inbuf, stdout);
	}
	(void) sigset(SIGINT, SIG_DFL);
	(void) sigset(SIGTERM, SIG_DFL);
	return (pclose(file));
}

static int
do_subproc_interactive(char *cmdbuf)
{
	void (*saveint)(int);
	void (*saveterm)(int);
	void (*savequit)(int);
	void (*savehup)(int);
	int pid, child, status;

	/*
	 * do_subproc() links stdin to /dev/null, which would break any
	 * interactive subprocess we try to launch here.  Similarly, we
	 * can't have been launched as a subprocess ourselves.
	 */
	assert(do_subproc_cnt == 0 && !zoneadm_is_nested);

	if ((child = vfork()) == 0) {
		(void) execl("/bin/sh", "sh", "-c", cmdbuf, (char *)NULL);
	}

	if (child == -1)
		return (-1);

	saveint = sigset(SIGINT, SIG_IGN);
	saveterm = sigset(SIGTERM, SIG_IGN);
	savequit = sigset(SIGQUIT, SIG_IGN);
	savehup = sigset(SIGHUP, SIG_IGN);

	while ((pid = waitpid(child, &status, 0)) != child && pid != -1)
		;

	(void) sigset(SIGINT, saveint);
	(void) sigset(SIGTERM, saveterm);
	(void) sigset(SIGQUIT, savequit);
	(void) sigset(SIGHUP, savehup);

	return (pid == -1 ? -1 : status);
}

static int
subproc_status(const char *cmd, int status, boolean_t verbose_failure)
{
	if (WIFEXITED(status)) {
		int exit_code = WEXITSTATUS(status);

		if ((verbose_failure) && (exit_code != ZONE_SUBPROC_OK))
			zerror(gettext("'%s' failed with exit code %d."), cmd,
			    exit_code);

		return (exit_code);
	} else if (WIFSIGNALED(status)) {
		int signal = WTERMSIG(status);
		char sigstr[SIG2STR_MAX];

		if (sig2str(signal, sigstr) == 0) {
			zerror(gettext("'%s' terminated by signal SIG%s."), cmd,
			    sigstr);
		} else {
			zerror(gettext("'%s' terminated by an unknown signal."),
			    cmd);
		}
	} else {
		zerror(gettext("'%s' failed for unknown reasons."), cmd);
	}

	/*
	 * Assume a subprocess that died due to a signal or an unknown error
	 * should be considered an exit code of ZONE_SUBPROC_FATAL, as the
	 * user will likely need to do some manual cleanup.
	 */
	return (ZONE_SUBPROC_FATAL);
}

/*
 * Various sanity checks; make sure:
 * 1. We're in the global zone.
 * 2. The calling user has sufficient privilege.
 * 3. The target zone is neither the global zone nor anything starting with
 *    "SUNW".
 * 4a. If we're looking for a 'not running' (i.e., configured or installed)
 *     zone, the name service knows about it.
 * 4b. For some operations which expect a zone not to be running, that it is
 *     not already running (or ready).
 */
static int
sanity_check(char *zone, int cmd_num, boolean_t running,
    boolean_t unsafe_when_running, boolean_t force)
{
	zone_entry_t *zent;
	priv_set_t *privset;
	zone_state_t state, min_state;
	char kernzone[ZONENAME_MAX];
	FILE *fp;

	if (getzoneid() != GLOBAL_ZONEID) {
		switch (cmd_num) {
		case CMD_HALT:
			zerror(gettext("use %s to %s this zone."), "halt(1M)",
			    cmd_to_str(cmd_num));
			break;
		case CMD_REBOOT:
			zerror(gettext("use %s to %s this zone."),
			    "reboot(1M)", cmd_to_str(cmd_num));
			break;
		default:
			zerror(gettext("must be in the global zone to %s a "
			    "zone."), cmd_to_str(cmd_num));
			break;
		}
		return (Z_ERR);
	}

	if ((privset = priv_allocset()) == NULL) {
		zerror(gettext("%s failed"), "priv_allocset");
		return (Z_ERR);
	}

	if (getppriv(PRIV_EFFECTIVE, privset) != 0) {
		zerror(gettext("%s failed"), "getppriv");
		priv_freeset(privset);
		return (Z_ERR);
	}

	if (priv_isfullset(privset) == B_FALSE) {
		zerror(gettext("only a privileged user may %s a zone."),
		    cmd_to_str(cmd_num));
		priv_freeset(privset);
		return (Z_ERR);
	}
	priv_freeset(privset);

	if (zone == NULL) {
		zerror(gettext("no zone specified"));
		return (Z_ERR);
	}

	if (strcmp(zone, GLOBAL_ZONENAME) == 0) {
		zerror(gettext("%s operation is invalid for the global zone."),
		    cmd_to_str(cmd_num));
		return (Z_ERR);
	}

	if (strncmp(zone, "SUNW", 4) == 0) {
		zerror(gettext("%s operation is invalid for zones starting "
		    "with SUNW."), cmd_to_str(cmd_num));
		return (Z_ERR);
	}

	if (!is_native_zone && !is_cluster_zone && cmd_num == CMD_MOUNT) {
		zerror(gettext("%s operation is invalid for branded zones."),
		    cmd_to_str(cmd_num));
			return (Z_ERR);
	}

	if (!zonecfg_in_alt_root()) {
		zent = lookup_running_zone(zone);
	} else if ((fp = zonecfg_open_scratch("", B_FALSE)) == NULL) {
		zent = NULL;
	} else {
		if (zonecfg_find_scratch(fp, zone, zonecfg_get_root(),
		    kernzone, sizeof (kernzone)) == 0)
			zent = lookup_running_zone(kernzone);
		else
			zent = NULL;
		zonecfg_close_scratch(fp);
	}

	/*
	 * Look up from the kernel for 'running' zones.
	 */
	if (running && !force) {
		if (zent == NULL) {
			zerror(gettext("not running"));
			return (Z_ERR);
		}
	} else {
		int err;

		if (unsafe_when_running && zent != NULL) {
			/* check whether the zone is ready or running */
			if ((err = zone_get_state(zent->zname,
			    &zent->zstate_num)) != Z_OK) {
				errno = err;
				zperror2(zent->zname,
				    gettext("could not get state"));
				/* can't tell, so hedge */
				zent->zstate_str = "ready/running";
			} else {
				zent->zstate_str =
				    zone_state_str(zent->zstate_num);
			}
			zerror(gettext("%s operation is invalid for %s zones."),
			    cmd_to_str(cmd_num), zent->zstate_str);
			return (Z_ERR);
		}
		if ((err = zone_get_state(zone, &state)) != Z_OK) {
			errno = err;
			zperror2(zone, gettext("could not get state"));
			return (Z_ERR);
		}
		switch (cmd_num) {
		case CMD_UNINSTALL:
			if (state == ZONE_STATE_CONFIGURED) {
				zerror(gettext("is already in state '%s'."),
				    zone_state_str(ZONE_STATE_CONFIGURED));
				return (Z_ERR);
			}
			break;
		case CMD_ATTACH:
		case CMD_CLONE:
		case CMD_INSTALL:
			if (state == ZONE_STATE_INSTALLED) {
				zerror(gettext("is already %s."),
				    zone_state_str(ZONE_STATE_INSTALLED));
				return (Z_ERR);
			} else if (state == ZONE_STATE_INCOMPLETE) {
				zerror(gettext("zone is %s; %s required."),
				    zone_state_str(ZONE_STATE_INCOMPLETE),
				    cmd_to_str(CMD_UNINSTALL));
				return (Z_ERR);
			}
			break;
		case CMD_DETACH:
		case CMD_MOVE:
		case CMD_READY:
		case CMD_BOOT:
		case CMD_MOUNT:
		case CMD_MARK:
			if ((cmd_num == CMD_BOOT || cmd_num == CMD_MOUNT) &&
			    force)
				min_state = ZONE_STATE_INCOMPLETE;
			else
				min_state = ZONE_STATE_INSTALLED;

			if (force && cmd_num == CMD_BOOT && is_native_zone) {
				zerror(gettext("Only branded zones may be "
				    "force-booted."));
				return (Z_ERR);
			}

			if (state < min_state) {
				zerror(gettext("must be %s before %s."),
				    zone_state_str(min_state),
				    cmd_to_str(cmd_num));
				return (Z_ERR);
			}
			break;
		case CMD_VERIFY:
			if (state == ZONE_STATE_INCOMPLETE) {
				zerror(gettext("zone is %s; %s required."),
				    zone_state_str(ZONE_STATE_INCOMPLETE),
				    cmd_to_str(CMD_UNINSTALL));
				return (Z_ERR);
			}
			break;
		case CMD_UNMOUNT:
			if (state != ZONE_STATE_MOUNTED) {
				zerror(gettext("must be %s before %s."),
				    zone_state_str(ZONE_STATE_MOUNTED),
				    cmd_to_str(cmd_num));
				return (Z_ERR);
			}
			break;
		}
	}
	return (Z_OK);
}

static int
halt_func(int argc, char *argv[])
{
	zone_cmd_arg_t zarg;
	int arg;

	if (zonecfg_in_alt_root()) {
		zerror(gettext("cannot halt zone in alternate root"));
		return (Z_ERR);
	}

	optind = 0;
	if ((arg = getopt(argc, argv, "?")) != EOF) {
		switch (arg) {
		case '?':
			sub_usage(SHELP_HALT, CMD_HALT);
			return (optopt == '?' ? Z_OK : Z_USAGE);
		default:
			sub_usage(SHELP_HALT, CMD_HALT);
			return (Z_USAGE);
		}
	}
	if (argc > optind) {
		sub_usage(SHELP_HALT, CMD_HALT);
		return (Z_USAGE);
	}
	/*
	 * zoneadmd should be the one to decide whether or not to proceed,
	 * so even though it seems that the fourth parameter below should
	 * perhaps be B_TRUE, it really shouldn't be.
	 */
	if (sanity_check(target_zone, CMD_HALT, B_FALSE, B_FALSE, B_FALSE)
	    != Z_OK)
		return (Z_ERR);

	/*
	 * Invoke brand-specific handler.
	 */
	if (invoke_brand_handler(CMD_HALT, argv) != Z_OK)
		return (Z_ERR);

	zarg.cmd = Z_HALT;
	return ((call_zoneadmd(target_zone, &zarg) == 0) ? Z_OK : Z_ERR);
}

static int
reboot_func(int argc, char *argv[])
{
	zone_cmd_arg_t zarg;
	int arg;

	if (zonecfg_in_alt_root()) {
		zerror(gettext("cannot reboot zone in alternate root"));
		return (Z_ERR);
	}

	optind = 0;
	if ((arg = getopt(argc, argv, "?")) != EOF) {
		switch (arg) {
		case '?':
			sub_usage(SHELP_REBOOT, CMD_REBOOT);
			return (optopt == '?' ? Z_OK : Z_USAGE);
		default:
			sub_usage(SHELP_REBOOT, CMD_REBOOT);
			return (Z_USAGE);
		}
	}

	zarg.bootbuf[0] = '\0';
	for (; optind < argc; optind++) {
		if (strlcat(zarg.bootbuf, argv[optind],
		    sizeof (zarg.bootbuf)) >= sizeof (zarg.bootbuf)) {
			zerror(gettext("Boot argument list too long"));
			return (Z_ERR);
		}
		if (optind < argc - 1)
			if (strlcat(zarg.bootbuf, " ", sizeof (zarg.bootbuf)) >=
			    sizeof (zarg.bootbuf)) {
				zerror(gettext("Boot argument list too long"));
				return (Z_ERR);
			}
	}


	/*
	 * zoneadmd should be the one to decide whether or not to proceed,
	 * so even though it seems that the fourth parameter below should
	 * perhaps be B_TRUE, it really shouldn't be.
	 */
	if (sanity_check(target_zone, CMD_REBOOT, B_TRUE, B_FALSE, B_FALSE)
	    != Z_OK)
		return (Z_ERR);
	if (verify_details(CMD_REBOOT, argv) != Z_OK)
		return (Z_ERR);

	zarg.cmd = Z_REBOOT;
	return ((call_zoneadmd(target_zone, &zarg) == 0) ? Z_OK : Z_ERR);
}

static int
verify_brand(zone_dochandle_t handle, int cmd_num, char *argv[])
{
	char cmdbuf[MAXPATHLEN];
	int err;
	char zonepath[MAXPATHLEN];
	brand_handle_t bh = NULL;
	int status, i;

	/*
	 * Fetch the verify command from the brand configuration.
	 * "exec" the command so that the returned status is that of
	 * the command and not the shell.
	 */
	if ((err = zonecfg_get_zonepath(handle, zonepath, sizeof (zonepath))) !=
	    Z_OK) {
		errno = err;
		zperror(cmd_to_str(cmd_num), B_TRUE);
		return (Z_ERR);
	}
	if ((bh = brand_open(target_brand)) == NULL) {
		zerror(gettext("missing or invalid brand"));
		return (Z_ERR);
	}

	/*
	 * If the brand has its own verification routine, execute it now.
	 * The verification routine validates the intended zoneadm
	 * operation for the specific brand. The zoneadm subcommand and
	 * all its arguments are passed to the routine.
	 */
	(void) strcpy(cmdbuf, EXEC_PREFIX);
	err = brand_get_verify_adm(bh, target_zone, zonepath,
	    cmdbuf + EXEC_LEN, sizeof (cmdbuf) - EXEC_LEN, 0, NULL);
	brand_close(bh);
	if (err != 0)
		return (Z_BRAND_ERROR);
	if (strlen(cmdbuf) <= EXEC_LEN)
		return (Z_OK);

	if (strlcat(cmdbuf, cmd_to_str(cmd_num),
	    sizeof (cmdbuf)) >= sizeof (cmdbuf))
		return (Z_ERR);

	/* Build the argv string */
	i = 0;
	while (argv[i] != NULL) {
		if ((strlcat(cmdbuf, " ",
		    sizeof (cmdbuf)) >= sizeof (cmdbuf)) ||
		    (strlcat(cmdbuf, argv[i++],
		    sizeof (cmdbuf)) >= sizeof (cmdbuf)))
			return (Z_ERR);
	}

	if (zoneadm_is_nested)
		status = do_subproc(cmdbuf);
	else
		status = do_subproc_interactive(cmdbuf);
	err = subproc_status(gettext("brand-specific verification"),
	    status, B_FALSE);

	return ((err == ZONE_SUBPROC_OK) ? Z_OK : Z_BRAND_ERROR);
}

static int
verify_rctls(zone_dochandle_t handle)
{
	struct zone_rctltab rctltab;
	size_t rbs = rctlblk_size();
	rctlblk_t *rctlblk;
	int error = Z_INVAL;

	if ((rctlblk = malloc(rbs)) == NULL) {
		zerror(gettext("failed to allocate %lu bytes: %s"), rbs,
		    strerror(errno));
		return (Z_NOMEM);
	}

	if (zonecfg_setrctlent(handle) != Z_OK) {
		zerror(gettext("zonecfg_setrctlent failed"));
		free(rctlblk);
		return (error);
	}

	rctltab.zone_rctl_valptr = NULL;
	while (zonecfg_getrctlent(handle, &rctltab) == Z_OK) {
		struct zone_rctlvaltab *rctlval;
		const char *name = rctltab.zone_rctl_name;

		if (!zonecfg_is_rctl(name)) {
			zerror(gettext("WARNING: Ignoring unrecognized rctl "
			    "'%s'."),  name);
			zonecfg_free_rctl_value_list(rctltab.zone_rctl_valptr);
			rctltab.zone_rctl_valptr = NULL;
			continue;
		}

		for (rctlval = rctltab.zone_rctl_valptr; rctlval != NULL;
		    rctlval = rctlval->zone_rctlval_next) {
			if (zonecfg_construct_rctlblk(rctlval, rctlblk)
			    != Z_OK) {
				zerror(gettext("invalid rctl value: "
				    "(priv=%s,limit=%s,action%s)"),
				    rctlval->zone_rctlval_priv,
				    rctlval->zone_rctlval_limit,
				    rctlval->zone_rctlval_action);
				goto out;
			}
			if (!zonecfg_valid_rctl(name, rctlblk)) {
				zerror(gettext("(priv=%s,limit=%s,action=%s) "
				    "is not a valid value for rctl '%s'"),
				    rctlval->zone_rctlval_priv,
				    rctlval->zone_rctlval_limit,
				    rctlval->zone_rctlval_action,
				    name);
				goto out;
			}
		}
		zonecfg_free_rctl_value_list(rctltab.zone_rctl_valptr);
	}
	rctltab.zone_rctl_valptr = NULL;
	error = Z_OK;
out:
	zonecfg_free_rctl_value_list(rctltab.zone_rctl_valptr);
	(void) zonecfg_endrctlent(handle);
	free(rctlblk);
	return (error);
}

static int
verify_pool(zone_dochandle_t handle)
{
	char poolname[MAXPATHLEN];
	pool_conf_t *poolconf;
	pool_t *pool;
	int status;
	int error;

	/*
	 * This ends up being very similar to the check done in zoneadmd.
	 */
	error = zonecfg_get_pool(handle, poolname, sizeof (poolname));
	if (error == Z_NO_ENTRY || (error == Z_OK && strlen(poolname) == 0)) {
		/*
		 * No pool specified.
		 */
		return (0);
	}
	if (error != Z_OK) {
		zperror(gettext("Unable to retrieve pool name from "
		    "configuration"), B_TRUE);
		return (error);
	}
	/*
	 * Don't do anything if pools aren't enabled.
	 */
	if (pool_get_status(&status) != PO_SUCCESS || status != POOL_ENABLED) {
		zerror(gettext("WARNING: pools facility not active; "
		    "zone will not be bound to pool '%s'."), poolname);
		return (Z_OK);
	}
	/*
	 * Try to provide a sane error message if the requested pool doesn't
	 * exist.  It isn't clear that pools-related failures should
	 * necessarily translate to a failure to verify the zone configuration,
	 * hence they are not considered errors.
	 */
	if ((poolconf = pool_conf_alloc()) == NULL) {
		zerror(gettext("WARNING: pool_conf_alloc failed; "
		    "using default pool"));
		return (Z_OK);
	}
	if (pool_conf_open(poolconf, pool_dynamic_location(), PO_RDONLY) !=
	    PO_SUCCESS) {
		zerror(gettext("WARNING: pool_conf_open failed; "
		    "using default pool"));
		pool_conf_free(poolconf);
		return (Z_OK);
	}
	pool = pool_get_pool(poolconf, poolname);
	(void) pool_conf_close(poolconf);
	pool_conf_free(poolconf);
	if (pool == NULL) {
		zerror(gettext("WARNING: pool '%s' not found. "
		    "using default pool"), poolname);
	}

	return (Z_OK);
}

static int
verify_ipd(zone_dochandle_t handle)
{
	int return_code = Z_OK;
	struct zone_fstab fstab;
	struct stat st;
	char specdir[MAXPATHLEN];

	if (zonecfg_setipdent(handle) != Z_OK) {
		/*
		 * TRANSLATION_NOTE
		 * inherit-pkg-dirs is a literal that should not be translated.
		 */
		(void) fprintf(stderr, gettext("could not verify "
		    "inherit-pkg-dirs: unable to enumerate mounts\n"));
		return (Z_ERR);
	}
	while (zonecfg_getipdent(handle, &fstab) == Z_OK) {
		/*
		 * Verify fs_dir exists.
		 */
		(void) snprintf(specdir, sizeof (specdir), "%s%s",
		    zonecfg_get_root(), fstab.zone_fs_dir);
		if (stat(specdir, &st) != 0) {
			/*
			 * TRANSLATION_NOTE
			 * inherit-pkg-dir is a literal that should not be
			 * translated.
			 */
			(void) fprintf(stderr, gettext("could not verify "
			    "inherit-pkg-dir %s: %s\n"),
			    fstab.zone_fs_dir, strerror(errno));
			return_code = Z_ERR;
		}
		if (strcmp(st.st_fstype, MNTTYPE_NFS) == 0) {
			/*
			 * TRANSLATION_NOTE
			 * inherit-pkg-dir and NFS are literals that should
			 * not be translated.
			 */
			(void) fprintf(stderr, gettext("cannot verify "
			    "inherit-pkg-dir %s: NFS mounted file system.\n"
			    "\tA local file system must be used.\n"),
			    fstab.zone_fs_dir);
			return_code = Z_ERR;
		}
	}
	(void) zonecfg_endipdent(handle);

	return (return_code);
}

/*
 * Verify that the special device/file system exists and is valid.
 */
static int
verify_fs_special(struct zone_fstab *fstab)
{
	struct stat st;

	/*
	 * This validation is really intended for standard zone administration.
	 * If we are in a mini-root or some other upgrade situation where
	 * we are using the scratch zone, just by-pass this.
	 */
	if (zonecfg_in_alt_root())
		return (Z_OK);

	if (strcmp(fstab->zone_fs_type, MNTTYPE_ZFS) == 0)
		return (verify_fs_zfs(fstab));

	if (stat(fstab->zone_fs_special, &st) != 0) {
		(void) fprintf(stderr, gettext("could not verify fs "
		    "%s: could not access %s: %s\n"), fstab->zone_fs_dir,
		    fstab->zone_fs_special, strerror(errno));
		return (Z_ERR);
	}

	if (strcmp(st.st_fstype, MNTTYPE_NFS) == 0) {
		/*
		 * TRANSLATION_NOTE
		 * fs and NFS are literals that should
		 * not be translated.
		 */
		(void) fprintf(stderr, gettext("cannot verify "
		    "fs %s: NFS mounted file system.\n"
		    "\tA local file system must be used.\n"),
		    fstab->zone_fs_special);
		return (Z_ERR);
	}

	return (Z_OK);
}

static int
verify_filesystems(zone_dochandle_t handle)
{
	int return_code = Z_OK;
	struct zone_fstab fstab;
	char cmdbuf[MAXPATHLEN];
	struct stat st;

	/*
	 * No need to verify inherit-pkg-dir fs types, as their type is
	 * implicitly lofs, which is known.  Therefore, the types are only
	 * verified for regular file systems below.
	 *
	 * Since the actual mount point is not known until the dependent mounts
	 * are performed, we don't attempt any path validation here: that will
	 * happen later when zoneadmd actually does the mounts.
	 */
	if (zonecfg_setfsent(handle) != Z_OK) {
		(void) fprintf(stderr, gettext("could not verify file systems: "
		    "unable to enumerate mounts\n"));
		return (Z_ERR);
	}
	while (zonecfg_getfsent(handle, &fstab) == Z_OK) {
		if (!zonecfg_valid_fs_type(fstab.zone_fs_type)) {
			(void) fprintf(stderr, gettext("cannot verify fs %s: "
			    "type %s is not allowed.\n"), fstab.zone_fs_dir,
			    fstab.zone_fs_type);
			return_code = Z_ERR;
			goto next_fs;
		}
		/*
		 * Verify /usr/lib/fs/<fstype>/mount exists.
		 */
		if (snprintf(cmdbuf, sizeof (cmdbuf), "/usr/lib/fs/%s/mount",
		    fstab.zone_fs_type) > sizeof (cmdbuf)) {
			(void) fprintf(stderr, gettext("cannot verify fs %s: "
			    "type %s is too long.\n"), fstab.zone_fs_dir,
			    fstab.zone_fs_type);
			return_code = Z_ERR;
			goto next_fs;
		}
		if (stat(cmdbuf, &st) != 0) {
			(void) fprintf(stderr, gettext("could not verify fs "
			    "%s: could not access %s: %s\n"), fstab.zone_fs_dir,
			    cmdbuf, strerror(errno));
			return_code = Z_ERR;
			goto next_fs;
		}
		if (!S_ISREG(st.st_mode)) {
			(void) fprintf(stderr, gettext("could not verify fs "
			    "%s: %s is not a regular file\n"),
			    fstab.zone_fs_dir, cmdbuf);
			return_code = Z_ERR;
			goto next_fs;
		}
		/*
		 * Verify /usr/lib/fs/<fstype>/fsck exists iff zone_fs_raw is
		 * set.
		 */
		if (snprintf(cmdbuf, sizeof (cmdbuf), "/usr/lib/fs/%s/fsck",
		    fstab.zone_fs_type) > sizeof (cmdbuf)) {
			(void) fprintf(stderr, gettext("cannot verify fs %s: "
			    "type %s is too long.\n"), fstab.zone_fs_dir,
			    fstab.zone_fs_type);
			return_code = Z_ERR;
			goto next_fs;
		}
		if (fstab.zone_fs_raw[0] == '\0' && stat(cmdbuf, &st) == 0) {
			(void) fprintf(stderr, gettext("could not verify fs "
			    "%s: must specify 'raw' device for %s "
			    "file systems\n"),
			    fstab.zone_fs_dir, fstab.zone_fs_type);
			return_code = Z_ERR;
			goto next_fs;
		}
		if (fstab.zone_fs_raw[0] != '\0' &&
		    (stat(cmdbuf, &st) != 0 || !S_ISREG(st.st_mode))) {
			(void) fprintf(stderr, gettext("cannot verify fs %s: "
			    "'raw' device specified but "
			    "no fsck executable exists for %s\n"),
			    fstab.zone_fs_dir, fstab.zone_fs_type);
			return_code = Z_ERR;
			goto next_fs;
		}

		/* Verify fs_special. */
		if ((return_code = verify_fs_special(&fstab)) != Z_OK)
			goto next_fs;

		/* Verify fs_raw. */
		if (fstab.zone_fs_raw[0] != '\0' &&
		    stat(fstab.zone_fs_raw, &st) != 0) {
			/*
			 * TRANSLATION_NOTE
			 * fs is a literal that should not be translated.
			 */
			(void) fprintf(stderr, gettext("could not verify fs "
			    "%s: could not access %s: %s\n"), fstab.zone_fs_dir,
			    fstab.zone_fs_raw, strerror(errno));
			return_code = Z_ERR;
			goto next_fs;
		}
next_fs:
		zonecfg_free_fs_option_list(fstab.zone_fs_options);
	}
	(void) zonecfg_endfsent(handle);

	return (return_code);
}

static int
verify_limitpriv(zone_dochandle_t handle)
{
	char *privname = NULL;
	int err;
	priv_set_t *privs;

	if ((privs = priv_allocset()) == NULL) {
		zperror(gettext("failed to allocate privilege set"), B_FALSE);
		return (Z_NOMEM);
	}
	err = zonecfg_get_privset(handle, privs, &privname);
	switch (err) {
	case Z_OK:
		break;
	case Z_PRIV_PROHIBITED:
		(void) fprintf(stderr, gettext("privilege \"%s\" is not "
		    "permitted within the zone's privilege set\n"), privname);
		break;
	case Z_PRIV_REQUIRED:
		(void) fprintf(stderr, gettext("required privilege \"%s\" is "
		    "missing from the zone's privilege set\n"), privname);
		break;
	case Z_PRIV_UNKNOWN:
		(void) fprintf(stderr, gettext("unknown privilege \"%s\" "
		    "specified in the zone's privilege set\n"), privname);
		break;
	default:
		zperror(
		    gettext("failed to determine the zone's privilege set"),
		    B_TRUE);
		break;
	}
	free(privname);
	priv_freeset(privs);
	return (err);
}

static void
free_local_netifs(int if_cnt, struct net_if **if_list)
{
	int		i;

	for (i = 0; i < if_cnt; i++) {
		free(if_list[i]->name);
		free(if_list[i]);
	}
	free(if_list);
}

/*
 * Get a list of the network interfaces, along with their address families,
 * that are plumbed in the global zone.  See if_tcp(7p) for a description
 * of the ioctls used here.
 */
static int
get_local_netifs(int *if_cnt, struct net_if ***if_list)
{
	int		s;
	int		i;
	int		res = Z_OK;
	int		space_needed;
	int		cnt = 0;
	struct		lifnum if_num;
	struct		lifconf if_conf;
	struct		lifreq *if_reqp;
	char		*if_buf;
	struct net_if	**local_ifs = NULL;

	*if_cnt = 0;
	*if_list = NULL;

	if ((s = socket(SOCKET_AF(AF_INET), SOCK_DGRAM, 0)) < 0)
		return (Z_ERR);

	/*
	 * Come back here in the unlikely event that the number of interfaces
	 * increases between the time we get the count and the time we do the
	 * SIOCGLIFCONF ioctl.
	 */
retry:
	/* Get the number of interfaces. */
	if_num.lifn_family = AF_UNSPEC;
	if_num.lifn_flags = LIFC_NOXMIT;
	if (ioctl(s, SIOCGLIFNUM, &if_num) < 0) {
		(void) close(s);
		return (Z_ERR);
	}

	/* Get the interface configuration list. */
	space_needed = if_num.lifn_count * sizeof (struct lifreq);
	if ((if_buf = malloc(space_needed)) == NULL) {
		(void) close(s);
		return (Z_ERR);
	}
	if_conf.lifc_family = AF_UNSPEC;
	if_conf.lifc_flags = LIFC_NOXMIT;
	if_conf.lifc_len = space_needed;
	if_conf.lifc_buf = if_buf;
	if (ioctl(s, SIOCGLIFCONF, &if_conf) < 0) {
		free(if_buf);
		/*
		 * SIOCGLIFCONF returns EINVAL if the buffer we passed in is
		 * too small.  In this case go back and get the new if cnt.
		 */
		if (errno == EINVAL)
			goto retry;

		(void) close(s);
		return (Z_ERR);
	}
	(void) close(s);

	/* Get the name and address family for each interface. */
	if_reqp = if_conf.lifc_req;
	for (i = 0; i < (if_conf.lifc_len / sizeof (struct lifreq)); i++) {
		struct net_if	**p;
		struct lifreq	req;

		if (strcmp(LOOPBACK_IF, if_reqp->lifr_name) == 0) {
			if_reqp++;
			continue;
		}

		if ((s = socket(SOCKET_AF(if_reqp->lifr_addr.ss_family),
		    SOCK_DGRAM, 0)) == -1) {
			res = Z_ERR;
			break;
		}

		(void) strncpy(req.lifr_name, if_reqp->lifr_name,
		    sizeof (req.lifr_name));
		if (ioctl(s, SIOCGLIFADDR, &req) < 0) {
			(void) close(s);
			if_reqp++;
			continue;
		}

		if ((p = (struct net_if **)realloc(local_ifs,
		    sizeof (struct net_if *) * (cnt + 1))) == NULL) {
			res = Z_ERR;
			break;
		}
		local_ifs = p;

		if ((local_ifs[cnt] = malloc(sizeof (struct net_if))) == NULL) {
			res = Z_ERR;
			break;
		}

		if ((local_ifs[cnt]->name = strdup(if_reqp->lifr_name))
		    == NULL) {
			free(local_ifs[cnt]);
			res = Z_ERR;
			break;
		}
		local_ifs[cnt]->af = req.lifr_addr.ss_family;
		cnt++;

		(void) close(s);
		if_reqp++;
	}

	free(if_buf);

	if (res != Z_OK) {
		free_local_netifs(cnt, local_ifs);
	} else {
		*if_cnt = cnt;
		*if_list = local_ifs;
	}

	return (res);
}

static char *
af2str(int af)
{
	switch (af) {
	case AF_INET:
		return ("IPv4");
	case AF_INET6:
		return ("IPv6");
	default:
		return ("Unknown");
	}
}

/*
 * Cross check the network interface name and address family with the
 * interfaces that are set up in the global zone so that we can print the
 * appropriate error message.
 */
static void
print_net_err(char *phys, char *addr, int af, char *msg)
{
	int		i;
	int		local_if_cnt = 0;
	struct net_if	**local_ifs = NULL;
	boolean_t	found_if = B_FALSE;
	boolean_t	found_af = B_FALSE;

	if (get_local_netifs(&local_if_cnt, &local_ifs) != Z_OK) {
		(void) fprintf(stderr,
		    gettext("could not verify %s %s=%s %s=%s\n\t%s\n"),
		    "net", "address", addr, "physical", phys, msg);
		return;
	}

	for (i = 0; i < local_if_cnt; i++) {
		if (strcmp(phys, local_ifs[i]->name) == 0) {
			found_if = B_TRUE;
			if (af == local_ifs[i]->af) {
				found_af = B_TRUE;
				break;
			}
		}
	}

	free_local_netifs(local_if_cnt, local_ifs);

	if (!found_if) {
		(void) fprintf(stderr,
		    gettext("could not verify %s %s=%s\n\t"
		    "network interface %s is not plumbed in the global zone\n"),
		    "net", "physical", phys, phys);
		return;
	}

	/*
	 * Print this error if we were unable to find the address family
	 * for this interface.  If the af variable is not initialized to
	 * to something meaningful by the caller (not AF_UNSPEC) then we
	 * also skip this message since it wouldn't be informative.
	 */
	if (!found_af && af != AF_UNSPEC) {
		(void) fprintf(stderr,
		    gettext("could not verify %s %s=%s %s=%s\n\tthe %s address "
		    "family is not configured on this network interface in "
		    "the\n\tglobal zone\n"),
		    "net", "address", addr, "physical", phys, af2str(af));
		return;
	}

	(void) fprintf(stderr,
	    gettext("could not verify %s %s=%s %s=%s\n\t%s\n"),
	    "net", "address", addr, "physical", phys, msg);
}

static int
verify_handle(int cmd_num, zone_dochandle_t handle, char *argv[])
{
	struct zone_nwiftab nwiftab;
	int return_code = Z_OK;
	int err;
	boolean_t in_alt_root;
	zone_iptype_t iptype;
	dlpi_handle_t dh;

	in_alt_root = zonecfg_in_alt_root();
	if (in_alt_root)
		goto no_net;

	if ((err = zonecfg_get_iptype(handle, &iptype)) != Z_OK) {
		errno = err;
		zperror(cmd_to_str(cmd_num), B_TRUE);
		zonecfg_fini_handle(handle);
		return (Z_ERR);
	}
	if ((err = zonecfg_setnwifent(handle)) != Z_OK) {
		errno = err;
		zperror(cmd_to_str(cmd_num), B_TRUE);
		zonecfg_fini_handle(handle);
		return (Z_ERR);
	}
	while (zonecfg_getnwifent(handle, &nwiftab) == Z_OK) {
		struct lifreq lifr;
		sa_family_t af = AF_UNSPEC;
		char dl_owner_zname[ZONENAME_MAX];
		zoneid_t dl_owner_zid;
		zoneid_t target_zid;
		int res;

		/* skip any loopback interfaces */
		if (strcmp(nwiftab.zone_nwif_physical, "lo0") == 0)
			continue;
		switch (iptype) {
		case ZS_SHARED:
			if ((res = zonecfg_valid_net_address(
			    nwiftab.zone_nwif_address, &lifr)) != Z_OK) {
				print_net_err(nwiftab.zone_nwif_physical,
				    nwiftab.zone_nwif_address, af,
				    zonecfg_strerror(res));
				return_code = Z_ERR;
				continue;
			}
			af = lifr.lifr_addr.ss_family;
			if (!zonecfg_ifname_exists(af,
			    nwiftab.zone_nwif_physical)) {
				/*
				 * The interface failed to come up. We continue
				 * on anyway for the sake of consistency: a
				 * zone is not shut down if the interface fails
				 * any time after boot, nor does the global zone
				 * fail to boot if an interface fails.
				 */
				(void) fprintf(stderr,
				    gettext("WARNING: skipping network "
				    "interface '%s' which may not be "
				    "present/plumbed in the global "
				    "zone.\n"),
				    nwiftab.zone_nwif_physical);
			}
			break;
		case ZS_EXCLUSIVE:
			/* Warning if it exists for either IPv4 or IPv6 */

			if (zonecfg_ifname_exists(AF_INET,
			    nwiftab.zone_nwif_physical) ||
			    zonecfg_ifname_exists(AF_INET6,
			    nwiftab.zone_nwif_physical)) {
				(void) fprintf(stderr,
				    gettext("WARNING: skipping network "
				    "interface '%s' which is used in the "
				    "global zone.\n"),
				    nwiftab.zone_nwif_physical);
				break;
			}

			/*
			 * Verify that the physical interface can be opened.
			 */
			err = dlpi_open(nwiftab.zone_nwif_physical, &dh, 0);
			if (err != DLPI_SUCCESS) {
				(void) fprintf(stderr,
				    gettext("WARNING: skipping network "
				    "interface '%s' which cannot be opened: "
				    "dlpi error (%s).\n"),
				    nwiftab.zone_nwif_physical,
				    dlpi_strerror(err));
				break;
			} else {
				dlpi_close(dh);
			}
			/*
			 * Verify whether the physical interface is already
			 * used by a zone.
			 */
			dl_owner_zid = ALL_ZONES;
			if (zone_check_datalink(&dl_owner_zid,
			    nwiftab.zone_nwif_physical) != 0)
				break;

			/*
			 * If the zone being verified is
			 * running and owns the interface
			 */
			target_zid = getzoneidbyname(target_zone);
			if (target_zid == dl_owner_zid)
				break;

			/* Zone id match failed, use name to check */
			if (getzonenamebyid(dl_owner_zid, dl_owner_zname,
			    ZONENAME_MAX) < 0) {
				/* No name, show ID instead */
				(void) snprintf(dl_owner_zname, ZONENAME_MAX,
				    "<%d>", dl_owner_zid);
			} else if (strcmp(dl_owner_zname, target_zone) == 0)
				break;

			/*
			 * Note here we only report a warning that
			 * the interface is already in use by another
			 * running zone, and the verify process just
			 * goes on, if the interface is still in use
			 * when this zone really boots up, zoneadmd
			 * will find it. If the name of the zone which
			 * owns this interface cannot be determined,
			 * then it is not possible to determine if there
			 * is a conflict so just report it as a warning.
			 */
			(void) fprintf(stderr,
			    gettext("WARNING: skipping network interface "
			    "'%s' which is used by the non-global zone "
			    "'%s'.\n"), nwiftab.zone_nwif_physical,
			    dl_owner_zname);
			break;
		}
	}
	(void) zonecfg_endnwifent(handle);
no_net:

	/* verify that lofs has not been excluded from the kernel */
	if (!(cmd_num == CMD_DETACH || cmd_num == CMD_ATTACH ||
	    cmd_num == CMD_MOVE || cmd_num == CMD_CLONE) &&
	    modctl(MODLOAD, 1, "fs/lofs", NULL) != 0) {
		if (errno == ENXIO)
			(void) fprintf(stderr, gettext("could not verify "
			    "lofs(7FS): possibly excluded in /etc/system\n"));
		else
			(void) fprintf(stderr, gettext("could not verify "
			    "lofs(7FS): %s\n"), strerror(errno));
		return_code = Z_ERR;
	}

	if (verify_filesystems(handle) != Z_OK)
		return_code = Z_ERR;
	if (verify_ipd(handle) != Z_OK)
		return_code = Z_ERR;
	if (!in_alt_root && verify_rctls(handle) != Z_OK)
		return_code = Z_ERR;
	if (!in_alt_root && verify_pool(handle) != Z_OK)
		return_code = Z_ERR;
	if (!in_alt_root && verify_brand(handle, cmd_num, argv) != Z_OK)
		return_code = Z_ERR;
	if (!in_alt_root && verify_datasets(handle) != Z_OK)
		return_code = Z_ERR;

	/*
	 * As the "mount" command is used for patching/upgrading of zones
	 * or other maintenance processes, the zone's privilege set is not
	 * checked in this case.  Instead, the default, safe set of
	 * privileges will be used when this zone is created in the
	 * kernel.
	 */
	if (!in_alt_root && cmd_num != CMD_MOUNT &&
	    verify_limitpriv(handle) != Z_OK)
		return_code = Z_ERR;

	return (return_code);
}

static int
verify_details(int cmd_num, char *argv[])
{
	zone_dochandle_t handle;
	char zonepath[MAXPATHLEN], checkpath[MAXPATHLEN];
	int return_code = Z_OK;
	int err;

	if ((handle = zonecfg_init_handle()) == NULL) {
		zperror(cmd_to_str(cmd_num), B_TRUE);
		return (Z_ERR);
	}
	if ((err = zonecfg_get_handle(target_zone, handle)) != Z_OK) {
		errno = err;
		zperror(cmd_to_str(cmd_num), B_TRUE);
		zonecfg_fini_handle(handle);
		return (Z_ERR);
	}
	if ((err = zonecfg_get_zonepath(handle, zonepath, sizeof (zonepath))) !=
	    Z_OK) {
		errno = err;
		zperror(cmd_to_str(cmd_num), B_TRUE);
		zonecfg_fini_handle(handle);
		return (Z_ERR);
	}
	/*
	 * zonecfg_get_zonepath() gets its data from the XML repository.
	 * Verify this against the index file, which is checked first by
	 * zone_get_zonepath().  If they don't match, bail out.
	 */
	if ((err = zone_get_zonepath(target_zone, checkpath,
	    sizeof (checkpath))) != Z_OK) {
		errno = err;
		zperror2(target_zone, gettext("could not get zone path"));
		zonecfg_fini_handle(handle);
		return (Z_ERR);
	}
	if (strcmp(zonepath, checkpath) != 0) {
		/*
		 * TRANSLATION_NOTE
		 * XML and zonepath are literals that should not be translated.
		 */
		(void) fprintf(stderr, gettext("The XML repository has "
		    "zonepath '%s',\nbut the index file has zonepath '%s'.\n"
		    "These must match, so fix the incorrect entry.\n"),
		    zonepath, checkpath);
		zonecfg_fini_handle(handle);
		return (Z_ERR);
	}
	if (validate_zonepath(zonepath, cmd_num) != Z_OK) {
		(void) fprintf(stderr, gettext("could not verify zonepath %s "
		    "because of the above errors.\n"), zonepath);
		return_code = Z_ERR;
	}

	if (verify_handle(cmd_num, handle, argv) != Z_OK)
		return_code = Z_ERR;

	zonecfg_fini_handle(handle);
	if (return_code == Z_ERR)
		(void) fprintf(stderr,
		    gettext("%s: zone %s failed to verify\n"),
		    execname, target_zone);
	return (return_code);
}

static int
verify_func(int argc, char *argv[])
{
	int arg;

	optind = 0;
	if ((arg = getopt(argc, argv, "?")) != EOF) {
		switch (arg) {
		case '?':
			sub_usage(SHELP_VERIFY, CMD_VERIFY);
			return (optopt == '?' ? Z_OK : Z_USAGE);
		default:
			sub_usage(SHELP_VERIFY, CMD_VERIFY);
			return (Z_USAGE);
		}
	}
	if (argc > optind) {
		sub_usage(SHELP_VERIFY, CMD_VERIFY);
		return (Z_USAGE);
	}
	if (sanity_check(target_zone, CMD_VERIFY, B_FALSE, B_FALSE, B_FALSE)
	    != Z_OK)
		return (Z_ERR);
	return (verify_details(CMD_VERIFY, argv));
}

static int
addopt(char *buf, int opt, char *optarg, size_t bufsize)
{
	char optstring[4];

	if (opt > 0)
		(void) sprintf(optstring, " -%c", opt);
	else
		(void) strcpy(optstring, " ");

	if ((strlcat(buf, optstring, bufsize) > bufsize))
		return (Z_ERR);
	if ((optarg != NULL) && (strlcat(buf, optarg, bufsize) > bufsize))
		return (Z_ERR);
	return (Z_OK);
}

static int
install_func(int argc, char *argv[])
{
	char cmdbuf[MAXPATHLEN];
	char postcmdbuf[MAXPATHLEN];
	int lockfd;
	int arg, err, subproc_err;
	char zonepath[MAXPATHLEN];
	brand_handle_t bh = NULL;
	int status;
	boolean_t nodataset = B_FALSE;
	boolean_t do_postinstall = B_FALSE;
	char opts[128];

	if (target_zone == NULL) {
		sub_usage(SHELP_INSTALL, CMD_INSTALL);
		return (Z_USAGE);
	}

	if (zonecfg_in_alt_root()) {
		zerror(gettext("cannot install zone in alternate root"));
		return (Z_ERR);
	}

	if ((err = zone_get_zonepath(target_zone, zonepath,
	    sizeof (zonepath))) != Z_OK) {
		errno = err;
		zperror2(target_zone, gettext("could not get zone path"));
		return (Z_ERR);
	}

	/* Fetch the install command from the brand configuration.  */
	if ((bh = brand_open(target_brand)) == NULL) {
		zerror(gettext("missing or invalid brand"));
		return (Z_ERR);
	}

	(void) strcpy(cmdbuf, EXEC_PREFIX);
	if (brand_get_install(bh, target_zone, zonepath, cmdbuf + EXEC_LEN,
	    sizeof (cmdbuf) - EXEC_LEN, 0, NULL) != 0) {
		zerror("invalid brand configuration: missing install resource");
		brand_close(bh);
		return (Z_ERR);
	}

	(void) strcpy(postcmdbuf, EXEC_PREFIX);
	if (brand_get_postinstall(bh, target_zone, zonepath,
	    postcmdbuf + EXEC_LEN, sizeof (postcmdbuf) - EXEC_LEN, 0, NULL)
	    != 0) {
		zerror("invalid brand configuration: missing postinstall "
		    "resource");
		brand_close(bh);
		return (Z_ERR);
	} else if (strlen(postcmdbuf) > EXEC_LEN) {
		do_postinstall = B_TRUE;
	}

	(void) strcpy(opts, "?x:");
	if (!is_native_zone) {
		/*
		 * Fetch the list of recognized command-line options from
		 * the brand configuration file.
		 */
		if (brand_get_installopts(bh, opts + strlen(opts),
		    sizeof (opts) - strlen(opts)) != 0) {
			zerror("invalid brand configuration: missing "
			    "install options resource");
			brand_close(bh);
			return (Z_ERR);
		}
	}
	brand_close(bh);

	optind = 0;
	while ((arg = getopt(argc, argv, opts)) != EOF) {
		switch (arg) {
		case '?':
			sub_usage(SHELP_INSTALL, CMD_INSTALL);
			return (optopt == '?' ? Z_OK : Z_USAGE);
		case 'x':
			if (strcmp(optarg, "nodataset") != 0) {
				sub_usage(SHELP_INSTALL, CMD_INSTALL);
				return (Z_USAGE);
			}
			nodataset = B_TRUE;
			break;
		default:
			if (is_native_zone) {
				sub_usage(SHELP_INSTALL, CMD_INSTALL);
				return (Z_USAGE);
			}

			/*
			 * This option isn't for zoneadm, so append it to
			 * the command line passed to the brand-specific
			 * install and postinstall routines.
			 */
			if (addopt(cmdbuf, optopt, optarg,
			    sizeof (cmdbuf)) != Z_OK) {
				zerror("Install command line too long");
				return (Z_ERR);
			}
			if (addopt(postcmdbuf, optopt, optarg,
			    sizeof (postcmdbuf)) != Z_OK) {
				zerror("Post-Install command line too long");
				return (Z_ERR);
			}
			break;
		}
	}

	if (!is_native_zone) {
		for (; optind < argc; optind++) {
			if (addopt(cmdbuf, 0, argv[optind],
			    sizeof (cmdbuf)) != Z_OK) {
				zerror("Install command line too long");
				return (Z_ERR);
			}
			if (addopt(postcmdbuf, 0, argv[optind],
			    sizeof (postcmdbuf)) != Z_OK) {
				zerror("Post-Install command line too long");
				return (Z_ERR);
			}
		}
	}

	if (sanity_check(target_zone, CMD_INSTALL, B_FALSE, B_TRUE, B_FALSE)
	    != Z_OK)
		return (Z_ERR);
	if (verify_details(CMD_INSTALL, argv) != Z_OK)
		return (Z_ERR);

	if (grab_lock_file(target_zone, &lockfd) != Z_OK) {
		zerror(gettext("another %s may have an operation in progress."),
		    "zoneadm");
		return (Z_ERR);
	}
	err = zone_set_state(target_zone, ZONE_STATE_INCOMPLETE);
	if (err != Z_OK) {
		errno = err;
		zperror2(target_zone, gettext("could not set state"));
		goto done;
	}

	if (!nodataset)
		create_zfs_zonepath(zonepath);

	/*
	 * According to the Application Packaging Developer's Guide, a
	 * "checkinstall" script when included in a package is executed as
	 * the user "install", if such a user exists, or by the user
	 * "nobody".  In order to support this dubious behavior, the path
	 * to the zone being constructed is opened up during the life of
	 * the command laying down the zone's root file system.  Once this
	 * has completed, regardless of whether it was successful, the
	 * path to the zone is again restricted.
	 */
	if (chmod(zonepath, DEFAULT_DIR_MODE) != 0) {
		zperror(zonepath, B_FALSE);
		err = Z_ERR;
		goto done;
	}

	if (is_native_zone)
		status = do_subproc(cmdbuf);
	else
		status = do_subproc_interactive(cmdbuf);

	if (chmod(zonepath, S_IRWXU) != 0) {
		zperror(zonepath, B_FALSE);
		err = Z_ERR;
		goto done;
	}
	if ((subproc_err =
	    subproc_status(gettext("brand-specific installation"), status,
	    B_FALSE)) != ZONE_SUBPROC_OK) {
		err = Z_ERR;
		goto done;
	}

	if ((err = zone_set_state(target_zone, ZONE_STATE_INSTALLED)) != Z_OK) {
		errno = err;
		zperror2(target_zone, gettext("could not set state"));
		goto done;
	}

	if (do_postinstall) {
		status = do_subproc(postcmdbuf);

		if ((subproc_err =
		    subproc_status(gettext("brand-specific post-install"),
		    status, B_FALSE)) != ZONE_SUBPROC_OK) {
			err = Z_ERR;
			(void) zone_set_state(target_zone,
			    ZONE_STATE_INCOMPLETE);
		}
	}

done:
	/*
	 * If the install script exited with ZONE_SUBPROC_USAGE or
	 * ZONE_SUBPROC_NOTCOMPLETE, try to clean up the zone and leave the
	 * zone in the CONFIGURED state so that another install can be
	 * attempted without requiring an uninstall first.
	 */
	if ((subproc_err == ZONE_SUBPROC_USAGE) ||
	    (subproc_err == ZONE_SUBPROC_NOTCOMPLETE)) {
		if ((err = cleanup_zonepath(zonepath, B_FALSE)) != Z_OK) {
			errno = err;
			zperror2(target_zone,
			    gettext("cleaning up zonepath failed"));
		} else if ((err = zone_set_state(target_zone,
		    ZONE_STATE_CONFIGURED)) != Z_OK) {
			errno = err;
			zperror2(target_zone, gettext("could not set state"));
		}
	}

	release_lock_file(lockfd);
	return ((err == Z_OK) ? Z_OK : Z_ERR);
}

/*
 * Check that the inherited pkg dirs are the same for the clone and its source.
 * The easiest way to do that is check that the list of ipds is the same
 * by matching each one against the other.  This algorithm should be fine since
 * the list of ipds should not be that long.
 */
static int
valid_ipd_clone(zone_dochandle_t s_handle, char *source_zone,
	zone_dochandle_t t_handle, char *target_zone)
{
	int err;
	int res = Z_OK;
	int s_cnt = 0;
	int t_cnt = 0;
	struct zone_fstab s_fstab;
	struct zone_fstab t_fstab;

	/*
	 * First check the source of the clone against the target.
	 */
	if ((err = zonecfg_setipdent(s_handle)) != Z_OK) {
		errno = err;
		zperror2(source_zone, gettext("could not enumerate "
		    "inherit-pkg-dirs"));
		return (Z_ERR);
	}

	while (zonecfg_getipdent(s_handle, &s_fstab) == Z_OK) {
		boolean_t match = B_FALSE;

		s_cnt++;

		if ((err = zonecfg_setipdent(t_handle)) != Z_OK) {
			errno = err;
			zperror2(target_zone, gettext("could not enumerate "
			    "inherit-pkg-dirs"));
			(void) zonecfg_endipdent(s_handle);
			return (Z_ERR);
		}

		while (zonecfg_getipdent(t_handle, &t_fstab) == Z_OK) {
			if (strcmp(s_fstab.zone_fs_dir, t_fstab.zone_fs_dir)
			    == 0) {
				match = B_TRUE;
				break;
			}
		}
		(void) zonecfg_endipdent(t_handle);

		if (!match) {
			(void) fprintf(stderr, gettext("inherit-pkg-dir "
			    "'%s' is not configured in zone %s.\n"),
			    s_fstab.zone_fs_dir, target_zone);
			res = Z_ERR;
		}
	}

	(void) zonecfg_endipdent(s_handle);

	/* skip the next check if we already have errors */
	if (res == Z_ERR)
		return (res);

	/*
	 * Now check the number of ipds in the target so we can verify
	 * that the source is not a subset of the target.
	 */
	if ((err = zonecfg_setipdent(t_handle)) != Z_OK) {
		errno = err;
		zperror2(target_zone, gettext("could not enumerate "
		    "inherit-pkg-dirs"));
		return (Z_ERR);
	}

	while (zonecfg_getipdent(t_handle, &t_fstab) == Z_OK)
		t_cnt++;

	(void) zonecfg_endipdent(t_handle);

	if (t_cnt != s_cnt) {
		(void) fprintf(stderr, gettext("Zone %s is configured "
		    "with inherit-pkg-dirs that are not configured in zone "
		    "%s.\n"), target_zone, source_zone);
		res = Z_ERR;
	}

	return (res);
}

static void
warn_dev_match(zone_dochandle_t s_handle, char *source_zone,
	zone_dochandle_t t_handle, char *target_zone)
{
	int err;
	struct zone_devtab s_devtab;
	struct zone_devtab t_devtab;

	if ((err = zonecfg_setdevent(t_handle)) != Z_OK) {
		errno = err;
		zperror2(target_zone, gettext("could not enumerate devices"));
		return;
	}

	while (zonecfg_getdevent(t_handle, &t_devtab) == Z_OK) {
		if ((err = zonecfg_setdevent(s_handle)) != Z_OK) {
			errno = err;
			zperror2(source_zone,
			    gettext("could not enumerate devices"));
			(void) zonecfg_enddevent(t_handle);
			return;
		}

		while (zonecfg_getdevent(s_handle, &s_devtab) == Z_OK) {
			/*
			 * Use fnmatch to catch the case where wildcards
			 * were used in one zone and the other has an
			 * explicit entry (e.g. /dev/dsk/c0t0d0s6 vs.
			 * /dev/\*dsk/c0t0d0s6).
			 */
			if (fnmatch(t_devtab.zone_dev_match,
			    s_devtab.zone_dev_match, FNM_PATHNAME) == 0 ||
			    fnmatch(s_devtab.zone_dev_match,
			    t_devtab.zone_dev_match, FNM_PATHNAME) == 0) {
				(void) fprintf(stderr,
				    gettext("WARNING: device '%s' "
				    "is configured in both zones.\n"),
				    t_devtab.zone_dev_match);
				break;
			}
		}
		(void) zonecfg_enddevent(s_handle);
	}

	(void) zonecfg_enddevent(t_handle);
}

/*
 * Check if the specified mount option (opt) is contained within the
 * options string.
 */
static boolean_t
opt_match(char *opt, char *options)
{
	char *p;
	char *lastp;

	if ((p = strtok_r(options, ",", &lastp)) != NULL) {
		if (strcmp(p, opt) == 0)
			return (B_TRUE);
		while ((p = strtok_r(NULL, ",", &lastp)) != NULL) {
			if (strcmp(p, opt) == 0)
				return (B_TRUE);
		}
	}

	return (B_FALSE);
}

#define	RW_LOFS	"WARNING: read-write lofs file system on '%s' is configured " \
	"in both zones.\n"

static void
print_fs_warnings(struct zone_fstab *s_fstab, struct zone_fstab *t_fstab)
{
	/*
	 * It is ok to have shared lofs mounted fs but we want to warn if
	 * either is rw since this will effect the other zone.
	 */
	if (strcmp(t_fstab->zone_fs_type, "lofs") == 0) {
		zone_fsopt_t *optp;

		/* The default is rw so no options means rw */
		if (t_fstab->zone_fs_options == NULL ||
		    s_fstab->zone_fs_options == NULL) {
			(void) fprintf(stderr, gettext(RW_LOFS),
			    t_fstab->zone_fs_special);
			return;
		}

		for (optp = s_fstab->zone_fs_options; optp != NULL;
		    optp = optp->zone_fsopt_next) {
			if (opt_match("rw", optp->zone_fsopt_opt)) {
				(void) fprintf(stderr, gettext(RW_LOFS),
				    s_fstab->zone_fs_special);
				return;
			}
		}

		for (optp = t_fstab->zone_fs_options; optp != NULL;
		    optp = optp->zone_fsopt_next) {
			if (opt_match("rw", optp->zone_fsopt_opt)) {
				(void) fprintf(stderr, gettext(RW_LOFS),
				    t_fstab->zone_fs_special);
				return;
			}
		}

		return;
	}

	/*
	 * TRANSLATION_NOTE
	 * The first variable is the file system type and the second is
	 * the file system special device.  For example,
	 * WARNING: ufs file system on '/dev/dsk/c0t0d0s0' ...
	 */
	(void) fprintf(stderr, gettext("WARNING: %s file system on '%s' "
	    "is configured in both zones.\n"), t_fstab->zone_fs_type,
	    t_fstab->zone_fs_special);
}

static void
warn_fs_match(zone_dochandle_t s_handle, char *source_zone,
	zone_dochandle_t t_handle, char *target_zone)
{
	int err;
	struct zone_fstab s_fstab;
	struct zone_fstab t_fstab;

	if ((err = zonecfg_setfsent(t_handle)) != Z_OK) {
		errno = err;
		zperror2(target_zone,
		    gettext("could not enumerate file systems"));
		return;
	}

	while (zonecfg_getfsent(t_handle, &t_fstab) == Z_OK) {
		if ((err = zonecfg_setfsent(s_handle)) != Z_OK) {
			errno = err;
			zperror2(source_zone,
			    gettext("could not enumerate file systems"));
			(void) zonecfg_endfsent(t_handle);
			return;
		}

		while (zonecfg_getfsent(s_handle, &s_fstab) == Z_OK) {
			if (strcmp(t_fstab.zone_fs_special,
			    s_fstab.zone_fs_special) == 0) {
				print_fs_warnings(&s_fstab, &t_fstab);
				break;
			}
		}
		(void) zonecfg_endfsent(s_handle);
	}

	(void) zonecfg_endfsent(t_handle);
}

/*
 * We don't catch the case where you used the same IP address but
 * it is not an exact string match.  For example, 192.9.0.128 vs. 192.09.0.128.
 * However, we're not going to worry about that but we will check for
 * a possible netmask on one of the addresses (e.g. 10.0.0.1 and 10.0.0.1/24)
 * and handle that case as a match.
 */
static void
warn_ip_match(zone_dochandle_t s_handle, char *source_zone,
	zone_dochandle_t t_handle, char *target_zone)
{
	int err;
	struct zone_nwiftab s_nwiftab;
	struct zone_nwiftab t_nwiftab;

	if ((err = zonecfg_setnwifent(t_handle)) != Z_OK) {
		errno = err;
		zperror2(target_zone,
		    gettext("could not enumerate network interfaces"));
		return;
	}

	while (zonecfg_getnwifent(t_handle, &t_nwiftab) == Z_OK) {
		char *p;

		/* remove an (optional) netmask from the address */
		if ((p = strchr(t_nwiftab.zone_nwif_address, '/')) != NULL)
			*p = '\0';

		if ((err = zonecfg_setnwifent(s_handle)) != Z_OK) {
			errno = err;
			zperror2(source_zone,
			    gettext("could not enumerate network interfaces"));
			(void) zonecfg_endnwifent(t_handle);
			return;
		}

		while (zonecfg_getnwifent(s_handle, &s_nwiftab) == Z_OK) {
			/* remove an (optional) netmask from the address */
			if ((p = strchr(s_nwiftab.zone_nwif_address, '/'))
			    != NULL)
				*p = '\0';

			/* For exclusive-IP zones, address is not specified. */
			if (strlen(s_nwiftab.zone_nwif_address) == 0)
				continue;

			if (strcmp(t_nwiftab.zone_nwif_address,
			    s_nwiftab.zone_nwif_address) == 0) {
				(void) fprintf(stderr,
				    gettext("WARNING: network address '%s' "
				    "is configured in both zones.\n"),
				    t_nwiftab.zone_nwif_address);
				break;
			}
		}
		(void) zonecfg_endnwifent(s_handle);
	}

	(void) zonecfg_endnwifent(t_handle);
}

static void
warn_dataset_match(zone_dochandle_t s_handle, char *source,
	zone_dochandle_t t_handle, char *target)
{
	int err;
	struct zone_dstab s_dstab;
	struct zone_dstab t_dstab;

	if ((err = zonecfg_setdsent(t_handle)) != Z_OK) {
		errno = err;
		zperror2(target, gettext("could not enumerate datasets"));
		return;
	}

	while (zonecfg_getdsent(t_handle, &t_dstab) == Z_OK) {
		if ((err = zonecfg_setdsent(s_handle)) != Z_OK) {
			errno = err;
			zperror2(source,
			    gettext("could not enumerate datasets"));
			(void) zonecfg_enddsent(t_handle);
			return;
		}

		while (zonecfg_getdsent(s_handle, &s_dstab) == Z_OK) {
			if (strcmp(t_dstab.zone_dataset_name,
			    s_dstab.zone_dataset_name) == 0) {
				target_zone = source;
				zerror(gettext("WARNING: dataset '%s' "
				    "is configured in both zones.\n"),
				    t_dstab.zone_dataset_name);
				break;
			}
		}
		(void) zonecfg_enddsent(s_handle);
	}

	(void) zonecfg_enddsent(t_handle);
}

/*
 * Check that the clone and its source have the same brand type.
 */
static int
valid_brand_clone(char *source_zone, char *target_zone)
{
	brand_handle_t bh;
	char source_brand[MAXNAMELEN];

	if ((zone_get_brand(source_zone, source_brand,
	    sizeof (source_brand))) != Z_OK) {
		(void) fprintf(stderr, "%s: zone '%s': %s\n",
		    execname, source_zone, gettext("missing or invalid brand"));
		return (Z_ERR);
	}

	if (strcmp(source_brand, target_brand) != NULL) {
		(void) fprintf(stderr,
		    gettext("%s: Zones '%s' and '%s' have different brand "
		    "types.\n"), execname, source_zone, target_zone);
		return (Z_ERR);
	}

	if ((bh = brand_open(target_brand)) == NULL) {
		zerror(gettext("missing or invalid brand"));
		return (Z_ERR);
	}
	brand_close(bh);
	return (Z_OK);
}

static int
validate_clone(char *source_zone, char *target_zone)
{
	int err = Z_OK;
	zone_dochandle_t s_handle;
	zone_dochandle_t t_handle;

	if ((t_handle = zonecfg_init_handle()) == NULL) {
		zperror(cmd_to_str(CMD_CLONE), B_TRUE);
		return (Z_ERR);
	}
	if ((err = zonecfg_get_handle(target_zone, t_handle)) != Z_OK) {
		errno = err;
		zperror(cmd_to_str(CMD_CLONE), B_TRUE);
		zonecfg_fini_handle(t_handle);
		return (Z_ERR);
	}

	if ((s_handle = zonecfg_init_handle()) == NULL) {
		zperror(cmd_to_str(CMD_CLONE), B_TRUE);
		zonecfg_fini_handle(t_handle);
		return (Z_ERR);
	}
	if ((err = zonecfg_get_handle(source_zone, s_handle)) != Z_OK) {
		errno = err;
		zperror(cmd_to_str(CMD_CLONE), B_TRUE);
		goto done;
	}

	/* verify new zone has same brand type */
	err = valid_brand_clone(source_zone, target_zone);
	if (err != Z_OK)
		goto done;

	/* verify new zone has same inherit-pkg-dirs */
	err = valid_ipd_clone(s_handle, source_zone, t_handle, target_zone);

	/* warn about imported fs's which are the same */
	warn_fs_match(s_handle, source_zone, t_handle, target_zone);

	/* warn about imported IP addresses which are the same */
	warn_ip_match(s_handle, source_zone, t_handle, target_zone);

	/* warn about imported devices which are the same */
	warn_dev_match(s_handle, source_zone, t_handle, target_zone);

	/* warn about imported datasets which are the same */
	warn_dataset_match(s_handle, source_zone, t_handle, target_zone);

done:
	zonecfg_fini_handle(t_handle);
	zonecfg_fini_handle(s_handle);

	return ((err == Z_OK) ? Z_OK : Z_ERR);
}

static int
copy_zone(char *src, char *dst)
{
	boolean_t out_null = B_FALSE;
	int status;
	char *outfile;
	char cmdbuf[MAXPATHLEN * 2 + 128];

	if ((outfile = tempnam("/var/log", "zone")) == NULL) {
		outfile = "/dev/null";
		out_null = B_TRUE;
	}

	/*
	 * Use find to get the list of files to copy.  We need to skip
	 * files of type "socket" since cpio can't handle those but that
	 * should be ok since the app will recreate the socket when it runs.
	 * We also need to filter out anything under the .zfs subdir.  Since
	 * find is running depth-first, we need the extra egrep to filter .zfs.
	 */
	(void) snprintf(cmdbuf, sizeof (cmdbuf),
	    "cd %s && /usr/bin/find . -type s -prune -o -depth -print | "
	    "/usr/bin/egrep -v '^\\./\\.zfs$|^\\./\\.zfs/' | "
	    "/usr/bin/cpio -pdmuP@ %s > %s 2>&1",
	    src, dst, outfile);

	status = do_subproc(cmdbuf);

	if (subproc_status("copy", status, B_TRUE) != ZONE_SUBPROC_OK) {
		if (!out_null)
			(void) fprintf(stderr, gettext("\nThe copy failed.\n"
			    "More information can be found in %s\n"), outfile);
		return (Z_ERR);
	}

	if (!out_null)
		(void) unlink(outfile);

	return (Z_OK);
}

static int
zone_postclone(char *zonepath)
{
	char cmdbuf[MAXPATHLEN];
	int status;
	brand_handle_t bh;
	int err = Z_OK;

	/*
	 * Fetch the post-clone command, if any, from the brand
	 * configuration.
	 */
	if ((bh = brand_open(target_brand)) == NULL) {
		zerror(gettext("missing or invalid brand"));
		return (Z_ERR);
	}
	(void) strcpy(cmdbuf, EXEC_PREFIX);
	err = brand_get_postclone(bh, target_zone, zonepath, cmdbuf + EXEC_LEN,
	    sizeof (cmdbuf) - EXEC_LEN, 0, NULL);
	brand_close(bh);

	if (err == 0 && strlen(cmdbuf) > EXEC_LEN) {
		status = do_subproc(cmdbuf);
		if ((err = subproc_status("postclone", status, B_FALSE))
		    != ZONE_SUBPROC_OK) {
			zerror(gettext("post-clone configuration failed."));
			err = Z_ERR;
		}
	}

	return (err);
}

/* ARGSUSED */
static int
zfm_print(const char *p, void *r) {
	zerror("  %s\n", p);
	return (0);
}

int
clone_copy(char *source_zonepath, char *zonepath)
{
	int err;

	/* Don't clone the zone if anything is still mounted there */
	if (zonecfg_find_mounts(source_zonepath, NULL, NULL)) {
		zerror(gettext("These file systems are mounted on "
		    "subdirectories of %s.\n"), source_zonepath);
		(void) zonecfg_find_mounts(source_zonepath, zfm_print, NULL);
		return (Z_ERR);
	}

	/*
	 * Attempt to create a ZFS fs for the zonepath.  As usual, we don't
	 * care if this works or not since we always have the default behavior
	 * of a simple directory for the zonepath.
	 */
	create_zfs_zonepath(zonepath);

	(void) printf(gettext("Copying %s..."), source_zonepath);
	(void) fflush(stdout);

	err = copy_zone(source_zonepath, zonepath);

	(void) printf("\n");

	return (err);
}

static int
clone_func(int argc, char *argv[])
{
	char *source_zone = NULL;
	int lockfd;
	int err, arg;
	char zonepath[MAXPATHLEN];
	char source_zonepath[MAXPATHLEN];
	zone_state_t state;
	zone_entry_t *zent;
	char *method = NULL;
	char *snapshot = NULL;

	if (zonecfg_in_alt_root()) {
		zerror(gettext("cannot clone zone in alternate root"));
		return (Z_ERR);
	}

	optind = 0;
	if ((arg = getopt(argc, argv, "?m:s:")) != EOF) {
		switch (arg) {
		case '?':
			sub_usage(SHELP_CLONE, CMD_CLONE);
			return (optopt == '?' ? Z_OK : Z_USAGE);
		case 'm':
			method = optarg;
			break;
		case 's':
			snapshot = optarg;
			break;
		default:
			sub_usage(SHELP_CLONE, CMD_CLONE);
			return (Z_USAGE);
		}
	}
	if (argc != (optind + 1) ||
	    (method != NULL && strcmp(method, "copy") != 0)) {
		sub_usage(SHELP_CLONE, CMD_CLONE);
		return (Z_USAGE);
	}
	source_zone = argv[optind];
	if (sanity_check(target_zone, CMD_CLONE, B_FALSE, B_TRUE, B_FALSE)
	    != Z_OK)
		return (Z_ERR);
	if (verify_details(CMD_CLONE, argv) != Z_OK)
		return (Z_ERR);

	/*
	 * We also need to do some extra validation on the source zone.
	 */
	if (strcmp(source_zone, GLOBAL_ZONENAME) == 0) {
		zerror(gettext("%s operation is invalid for the global zone."),
		    cmd_to_str(CMD_CLONE));
		return (Z_ERR);
	}

	if (strncmp(source_zone, "SUNW", 4) == 0) {
		zerror(gettext("%s operation is invalid for zones starting "
		    "with SUNW."), cmd_to_str(CMD_CLONE));
		return (Z_ERR);
	}

	zent = lookup_running_zone(source_zone);
	if (zent != NULL) {
		/* check whether the zone is ready or running */
		if ((err = zone_get_state(zent->zname, &zent->zstate_num))
		    != Z_OK) {
			errno = err;
			zperror2(zent->zname, gettext("could not get state"));
			/* can't tell, so hedge */
			zent->zstate_str = "ready/running";
		} else {
			zent->zstate_str = zone_state_str(zent->zstate_num);
		}
		zerror(gettext("%s operation is invalid for %s zones."),
		    cmd_to_str(CMD_CLONE), zent->zstate_str);
		return (Z_ERR);
	}

	if ((err = zone_get_state(source_zone, &state)) != Z_OK) {
		errno = err;
		zperror2(source_zone, gettext("could not get state"));
		return (Z_ERR);
	}
	if (state != ZONE_STATE_INSTALLED) {
		(void) fprintf(stderr,
		    gettext("%s: zone %s is %s; %s is required.\n"),
		    execname, source_zone, zone_state_str(state),
		    zone_state_str(ZONE_STATE_INSTALLED));
		return (Z_ERR);
	}

	/*
	 * The source zone checks out ok, continue with the clone.
	 */

	if (validate_clone(source_zone, target_zone) != Z_OK)
		return (Z_ERR);

	if (grab_lock_file(target_zone, &lockfd) != Z_OK) {
		zerror(gettext("another %s may have an operation in progress."),
		    "zoneadm");
		return (Z_ERR);
	}

	if ((err = zone_get_zonepath(source_zone, source_zonepath,
	    sizeof (source_zonepath))) != Z_OK) {
		errno = err;
		zperror2(source_zone, gettext("could not get zone path"));
		goto done;
	}

	if ((err = zone_get_zonepath(target_zone, zonepath, sizeof (zonepath)))
	    != Z_OK) {
		errno = err;
		zperror2(target_zone, gettext("could not get zone path"));
		goto done;
	}

	if ((err = zone_set_state(target_zone, ZONE_STATE_INCOMPLETE))
	    != Z_OK) {
		errno = err;
		zperror2(target_zone, gettext("could not set state"));
		goto done;
	}

	if (snapshot != NULL) {
		err = clone_snapshot_zfs(snapshot, zonepath);
	} else {
		/*
		 * We always copy the clone unless the source is ZFS and a
		 * ZFS clone worked.  We fallback to copying if the ZFS clone
		 * fails for some reason.
		 */
		err = Z_ERR;
		if (method == NULL && is_zonepath_zfs(source_zonepath))
			err = clone_zfs(source_zone, source_zonepath, zonepath);

		if (err != Z_OK)
			err = clone_copy(source_zonepath, zonepath);
	}

	/*
	 * Trusted Extensions requires that cloned zones use the same sysid
	 * configuration, so it is not appropriate to perform any
	 * post-clone reconfiguration.
	 */
	if ((err == Z_OK) && !is_system_labeled())
		err = zone_postclone(zonepath);

done:
	/*
	 * If everything went well, we mark the zone as installed.
	 */
	if (err == Z_OK) {
		err = zone_set_state(target_zone, ZONE_STATE_INSTALLED);
		if (err != Z_OK) {
			errno = err;
			zperror2(target_zone, gettext("could not set state"));
		}
	}
	release_lock_file(lockfd);
	return ((err == Z_OK) ? Z_OK : Z_ERR);
}

/*
 * Used when removing a zonepath after uninstalling or cleaning up after
 * the move subcommand.  This handles a zonepath that has non-standard
 * contents so that we will only cleanup the stuff we know about and leave
 * any user data alone.
 *
 * If the "all" parameter is true then we should remove the whole zonepath
 * even if it has non-standard files/directories in it.  This can be used when
 * we need to cleanup after moving the zonepath across file systems.
 *
 * We "exec" the RMCOMMAND so that the returned status is that of RMCOMMAND
 * and not the shell.
 */
static int
cleanup_zonepath(char *zonepath, boolean_t all)
{
	int		status;
	int		i;
	boolean_t	non_std = B_FALSE;
	struct dirent	*dp;
	DIR		*dirp;
			/*
			 * The SUNWattached.xml file is expected since it might
			 * exist if the zone was force-attached after a
			 * migration.
			 */
	char		*std_entries[] = {"dev", "lu", "root",
			    "SUNWattached.xml", NULL};
			/* (MAXPATHLEN * 3) is for the 3 std_entries dirs */
	char		cmdbuf[sizeof (RMCOMMAND) + (MAXPATHLEN * 3) + 64];

	/*
	 * We shouldn't need these checks but lets be paranoid since we
	 * could blow away the whole system here if we got the wrong zonepath.
	 */
	if (*zonepath == NULL || strcmp(zonepath, "/") == 0) {
		(void) fprintf(stderr, "invalid zonepath '%s'\n", zonepath);
		return (Z_INVAL);
	}

	/*
	 * If the dirpath is already gone (maybe it was manually removed) then
	 * we just return Z_OK so that the cleanup is successful.
	 */
	if ((dirp = opendir(zonepath)) == NULL)
		return (Z_OK);

	/*
	 * Look through the zonepath directory to see if there are any
	 * non-standard files/dirs.  Also skip .zfs since that might be
	 * there but we'll handle ZFS file systems as a special case.
	 */
	while ((dp = readdir(dirp)) != NULL) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0 ||
		    strcmp(dp->d_name, ".zfs") == 0)
			continue;

		for (i = 0; std_entries[i] != NULL; i++)
			if (strcmp(dp->d_name, std_entries[i]) == 0)
				break;

		if (std_entries[i] == NULL)
			non_std = B_TRUE;
	}
	(void) closedir(dirp);

	if (!all && non_std) {
		/*
		 * There are extra, non-standard directories/files in the
		 * zonepath so we don't want to remove the zonepath.  We
		 * just want to remove the standard directories and leave
		 * the user data alone.
		 */
		(void) snprintf(cmdbuf, sizeof (cmdbuf), "exec " RMCOMMAND);

		for (i = 0; std_entries[i] != NULL; i++) {
			char tmpbuf[MAXPATHLEN];

			if (snprintf(tmpbuf, sizeof (tmpbuf), " %s/%s",
			    zonepath, std_entries[i]) >= sizeof (tmpbuf) ||
			    strlcat(cmdbuf, tmpbuf, sizeof (cmdbuf)) >=
			    sizeof (cmdbuf)) {
				(void) fprintf(stderr,
				    gettext("path is too long\n"));
				return (Z_INVAL);
			}
		}

		status = do_subproc(cmdbuf);

		(void) fprintf(stderr, gettext("WARNING: Unable to completely "
		    "remove %s\nbecause it contains additional user data.  "
		    "Only the standard directory\nentries have been "
		    "removed.\n"),
		    zonepath);

		return ((subproc_status(RMCOMMAND, status, B_TRUE) ==
		    ZONE_SUBPROC_OK) ? Z_OK : Z_ERR);
	}

	/*
	 * There is nothing unexpected in the zonepath, try to get rid of the
	 * whole zonepath directory.
	 *
	 * If the zonepath is its own zfs file system, try to destroy the
	 * file system.  If that fails for some reason (e.g. it has clones)
	 * then we'll just remove the contents of the zonepath.
	 */
	if (is_zonepath_zfs(zonepath)) {
		if (destroy_zfs(zonepath) == Z_OK)
			return (Z_OK);
		(void) snprintf(cmdbuf, sizeof (cmdbuf), "exec " RMCOMMAND
		    " %s/*", zonepath);
		status = do_subproc(cmdbuf);
		return ((subproc_status(RMCOMMAND, status, B_TRUE) ==
		    ZONE_SUBPROC_OK) ? Z_OK : Z_ERR);
	}

	(void) snprintf(cmdbuf, sizeof (cmdbuf), "exec " RMCOMMAND " %s",
	    zonepath);
	status = do_subproc(cmdbuf);

	return ((subproc_status(RMCOMMAND, status, B_TRUE) == ZONE_SUBPROC_OK)
	    ? Z_OK : Z_ERR);
}

static int
move_func(int argc, char *argv[])
{
	char *new_zonepath = NULL;
	int lockfd;
	int err, arg;
	char zonepath[MAXPATHLEN];
	zone_dochandle_t handle;
	boolean_t fast;
	boolean_t is_zfs = B_FALSE;
	struct dirent *dp;
	DIR *dirp;
	boolean_t empty = B_TRUE;
	boolean_t revert;
	struct stat zonepath_buf;
	struct stat new_zonepath_buf;

	if (zonecfg_in_alt_root()) {
		zerror(gettext("cannot move zone in alternate root"));
		return (Z_ERR);
	}

	optind = 0;
	if ((arg = getopt(argc, argv, "?")) != EOF) {
		switch (arg) {
		case '?':
			sub_usage(SHELP_MOVE, CMD_MOVE);
			return (optopt == '?' ? Z_OK : Z_USAGE);
		default:
			sub_usage(SHELP_MOVE, CMD_MOVE);
			return (Z_USAGE);
		}
	}
	if (argc != (optind + 1)) {
		sub_usage(SHELP_MOVE, CMD_MOVE);
		return (Z_USAGE);
	}
	new_zonepath = argv[optind];
	if (sanity_check(target_zone, CMD_MOVE, B_FALSE, B_TRUE, B_FALSE)
	    != Z_OK)
		return (Z_ERR);
	if (verify_details(CMD_MOVE, argv) != Z_OK)
		return (Z_ERR);

	/*
	 * Check out the new zonepath.  This has the side effect of creating
	 * a directory for the new zonepath.  We depend on this later when we
	 * stat to see if we are doing a cross file system move or not.
	 */
	if (validate_zonepath(new_zonepath, CMD_MOVE) != Z_OK)
		return (Z_ERR);

	if ((err = zone_get_zonepath(target_zone, zonepath, sizeof (zonepath)))
	    != Z_OK) {
		errno = err;
		zperror2(target_zone, gettext("could not get zone path"));
		return (Z_ERR);
	}

	if (stat(zonepath, &zonepath_buf) == -1) {
		zperror(gettext("could not stat zone path"), B_FALSE);
		return (Z_ERR);
	}

	if (stat(new_zonepath, &new_zonepath_buf) == -1) {
		zperror(gettext("could not stat new zone path"), B_FALSE);
		return (Z_ERR);
	}

	/*
	 * Check if the destination directory is empty.
	 */
	if ((dirp = opendir(new_zonepath)) == NULL) {
		zperror(gettext("could not open new zone path"), B_FALSE);
		return (Z_ERR);
	}
	while ((dp = readdir(dirp)) != (struct dirent *)0) {
		if (strcmp(dp->d_name, ".") == 0 ||
		    strcmp(dp->d_name, "..") == 0)
			continue;
		empty = B_FALSE;
		break;
	}
	(void) closedir(dirp);

	/* Error if there is anything in the destination directory. */
	if (!empty) {
		(void) fprintf(stderr, gettext("could not move zone to %s: "
		    "directory not empty\n"), new_zonepath);
		return (Z_ERR);
	}

	/* Don't move the zone if anything is still mounted there */
	if (zonecfg_find_mounts(zonepath, NULL, NULL)) {
		zerror(gettext("These file systems are mounted on "
		    "subdirectories of %s.\n"), zonepath);
		(void) zonecfg_find_mounts(zonepath, zfm_print, NULL);
		return (Z_ERR);
	}

	/*
	 * Check if we are moving in the same file system and can do a fast
	 * move or if we are crossing file systems and have to copy the data.
	 */
	fast = (zonepath_buf.st_dev == new_zonepath_buf.st_dev);

	if ((handle = zonecfg_init_handle()) == NULL) {
		zperror(cmd_to_str(CMD_MOVE), B_TRUE);
		return (Z_ERR);
	}

	if ((err = zonecfg_get_handle(target_zone, handle)) != Z_OK) {
		errno = err;
		zperror(cmd_to_str(CMD_MOVE), B_TRUE);
		zonecfg_fini_handle(handle);
		return (Z_ERR);
	}

	if (grab_lock_file(target_zone, &lockfd) != Z_OK) {
		zerror(gettext("another %s may have an operation in progress."),
		    "zoneadm");
		zonecfg_fini_handle(handle);
		return (Z_ERR);
	}

	/*
	 * We're making some file system changes now so we have to clean up
	 * the file system before we are done.  This will either clean up the
	 * new zonepath if the zonecfg update failed or it will clean up the
	 * old zonepath if everything is ok.
	 */
	revert = B_TRUE;

	if (is_zonepath_zfs(zonepath) &&
	    move_zfs(zonepath, new_zonepath) != Z_ERR) {
		is_zfs = B_TRUE;

	} else if (fast) {
		/* same file system, use rename for a quick move */

		/*
		 * Remove the new_zonepath directory that got created above
		 * during the validation.  It gets in the way of the rename.
		 */
		if (rmdir(new_zonepath) != 0) {
			zperror(gettext("could not rmdir new zone path"),
			    B_FALSE);
			zonecfg_fini_handle(handle);
			release_lock_file(lockfd);
			return (Z_ERR);
		}

		if (rename(zonepath, new_zonepath) != 0) {
			/*
			 * If this fails we don't need to do all of the
			 * cleanup that happens for the rest of the code
			 * so just return from this error.
			 */
			zperror(gettext("could not move zone"), B_FALSE);
			zonecfg_fini_handle(handle);
			release_lock_file(lockfd);
			return (Z_ERR);
		}

	} else {
		/*
		 * Attempt to create a ZFS fs for the new zonepath.  As usual,
		 * we don't care if this works or not since we always have the
		 * default behavior of a simple directory for the zonepath.
		 */
		create_zfs_zonepath(new_zonepath);

		(void) printf(gettext(
		    "Moving across file systems; copying zonepath %s..."),
		    zonepath);
		(void) fflush(stdout);

		err = copy_zone(zonepath, new_zonepath);

		(void) printf("\n");
		if (err != Z_OK)
			goto done;
	}

	if ((err = zonecfg_set_zonepath(handle, new_zonepath)) != Z_OK) {
		errno = err;
		zperror(gettext("could not set new zonepath"), B_TRUE);
		goto done;
	}

	if ((err = zonecfg_save(handle)) != Z_OK) {
		errno = err;
		zperror(gettext("zonecfg save failed"), B_TRUE);
		goto done;
	}

	revert = B_FALSE;

done:
	zonecfg_fini_handle(handle);
	release_lock_file(lockfd);

	/*
	 * Clean up the file system based on how things went.  We either
	 * clean up the new zonepath if the operation failed for some reason
	 * or we clean up the old zonepath if everything is ok.
	 */
	if (revert) {
		/* The zonecfg update failed, cleanup the new zonepath. */
		if (is_zfs) {
			if (move_zfs(new_zonepath, zonepath) == Z_ERR) {
				(void) fprintf(stderr, gettext("could not "
				    "restore zonepath, the zfs mountpoint is "
				    "set as:\n%s\n"), new_zonepath);
				/*
				 * err is already != Z_OK since we're reverting
				 */
			}

		} else if (fast) {
			if (rename(new_zonepath, zonepath) != 0) {
				zperror(gettext("could not restore zonepath"),
				    B_FALSE);
				/*
				 * err is already != Z_OK since we're reverting
				 */
			}
		} else {
			(void) printf(gettext("Cleaning up zonepath %s..."),
			    new_zonepath);
			(void) fflush(stdout);
			err = cleanup_zonepath(new_zonepath, B_TRUE);
			(void) printf("\n");

			if (err != Z_OK) {
				errno = err;
				zperror(gettext("could not remove new "
				    "zonepath"), B_TRUE);
			} else {
				/*
				 * Because we're reverting we know the mainline
				 * code failed but we just reused the err
				 * variable so we reset it back to Z_ERR.
				 */
				err = Z_ERR;
			}
		}

	} else {
		/* The move was successful, cleanup the old zonepath. */
		if (!is_zfs && !fast) {
			(void) printf(
			    gettext("Cleaning up zonepath %s..."), zonepath);
			(void) fflush(stdout);
			err = cleanup_zonepath(zonepath, B_TRUE);
			(void) printf("\n");

			if (err != Z_OK) {
				errno = err;
				zperror(gettext("could not remove zonepath"),
				    B_TRUE);
			}
		}
	}

	return ((err == Z_OK) ? Z_OK : Z_ERR);
}

static int
detach_func(int argc, char *argv[])
{
	int lockfd;
	int err, arg;
	char zonepath[MAXPATHLEN];
	char cmdbuf[MAXPATHLEN];
	zone_dochandle_t handle;
	boolean_t execute = B_TRUE;
	brand_handle_t bh = NULL;

	if (zonecfg_in_alt_root()) {
		zerror(gettext("cannot detach zone in alternate root"));
		return (Z_ERR);
	}

	optind = 0;
	if ((arg = getopt(argc, argv, "?n")) != EOF) {
		switch (arg) {
		case '?':
			sub_usage(SHELP_DETACH, CMD_DETACH);
			return (optopt == '?' ? Z_OK : Z_USAGE);
		case 'n':
			execute = B_FALSE;
			break;
		default:
			sub_usage(SHELP_DETACH, CMD_DETACH);
			return (Z_USAGE);
		}
	}

	if (execute) {
		if (sanity_check(target_zone, CMD_DETACH, B_FALSE, B_TRUE,
		    B_FALSE) != Z_OK)
			return (Z_ERR);
		if (verify_details(CMD_DETACH, argv) != Z_OK)
			return (Z_ERR);
	} else {
		/*
		 * We want a dry-run to work for a non-privileged user so we
		 * only do minimal validation.
		 */
		if (target_zone == NULL) {
			zerror(gettext("no zone specified"));
			return (Z_ERR);
		}

		if (strcmp(target_zone, GLOBAL_ZONENAME) == 0) {
			zerror(gettext("%s operation is invalid for the "
			    "global zone."), cmd_to_str(CMD_DETACH));
			return (Z_ERR);
		}
	}

	if ((err = zone_get_zonepath(target_zone, zonepath, sizeof (zonepath)))
	    != Z_OK) {
		errno = err;
		zperror2(target_zone, gettext("could not get zone path"));
		return (Z_ERR);
	}

	/* Don't detach the zone if anything is still mounted there */
	if (execute && zonecfg_find_mounts(zonepath, NULL, NULL)) {
		zerror(gettext("These file systems are mounted on "
		    "subdirectories of %s.\n"), zonepath);
		(void) zonecfg_find_mounts(zonepath, zfm_print, NULL);
		return (Z_ERR);
	}

	if ((handle = zonecfg_init_handle()) == NULL) {
		zperror(cmd_to_str(CMD_DETACH), B_TRUE);
		return (Z_ERR);
	}

	if ((err = zonecfg_get_handle(target_zone, handle)) != Z_OK) {
		errno = err;
		zperror(cmd_to_str(CMD_DETACH), B_TRUE);
		zonecfg_fini_handle(handle);
		return (Z_ERR);
	}

	/* Fetch the predetach hook from the brand configuration.  */
	if ((bh = brand_open(target_brand)) == NULL) {
		zerror(gettext("missing or invalid brand"));
		return (Z_ERR);
	}

	(void) strcpy(cmdbuf, EXEC_PREFIX);
	if (brand_get_predetach(bh, target_zone, zonepath, cmdbuf + EXEC_LEN,
	    sizeof (cmdbuf) - EXEC_LEN, 0, NULL) != 0) {
		zerror("invalid brand configuration: missing predetach "
		    "resource");
		brand_close(bh);
		return (Z_ERR);
	}
	brand_close(bh);

	/* If we have a brand predetach hook, run it. */
	if (strlen(cmdbuf) > EXEC_LEN) {
		int status;

		/* If this is a dry-run, pass that flag to the hook. */
		if (!execute && addopt(cmdbuf, 0, "-n", sizeof (cmdbuf))
		    != Z_OK) {
			zerror("Predetach command line too long");
			return (Z_ERR);
		}

		status = do_subproc(cmdbuf);
		if (subproc_status(gettext("brand-specific predetach"),
		    status, B_FALSE) != ZONE_SUBPROC_OK) {
			return (Z_ERR);
		}
	}

	if (execute && grab_lock_file(target_zone, &lockfd) != Z_OK) {
		zerror(gettext("another %s may have an operation in progress."),
		    "zoneadm");
		zonecfg_fini_handle(handle);
		return (Z_ERR);
	}

	if ((err = zonecfg_get_detach_info(handle, B_TRUE)) != Z_OK) {
		errno = err;
		zperror(gettext("getting the detach information failed"),
		    B_TRUE);
		goto done;
	}

	if ((err = zonecfg_detach_save(handle, (execute ? 0 : ZONE_DRY_RUN)))
	    != Z_OK) {
		errno = err;
		zperror(gettext("saving the detach manifest failed"), B_TRUE);
		goto done;
	}

	/*
	 * Set the zone state back to configured unless we are running with the
	 * no-execute option.
	 */
	if (execute && (err = zone_set_state(target_zone,
	    ZONE_STATE_CONFIGURED)) != Z_OK) {
		errno = err;
		zperror(gettext("could not reset state"), B_TRUE);
	}

done:
	zonecfg_fini_handle(handle);
	if (execute)
		release_lock_file(lockfd);

	return ((err == Z_OK) ? Z_OK : Z_ERR);
}

/*
 * During attach we go through and fix up the /dev entries for the zone
 * we are attaching.  In order to regenerate /dev with the correct devices,
 * the old /dev will be removed, the zone readied (which generates a new
 * /dev) then halted, then we use the info from the manifest to update
 * the modes, owners, etc. on the new /dev.
 */
static int
dev_fix(zone_dochandle_t handle)
{
	int			res;
	int			err;
	int			status;
	struct zone_devpermtab	devtab;
	zone_cmd_arg_t		zarg;
	char			devpath[MAXPATHLEN];
				/* 6: "exec " and " " */
	char			cmdbuf[sizeof (RMCOMMAND) + MAXPATHLEN + 6];

	if ((res = zonecfg_get_zonepath(handle, devpath, sizeof (devpath)))
	    != Z_OK)
		return (res);

	if (strlcat(devpath, "/dev", sizeof (devpath)) >= sizeof (devpath))
		return (Z_TOO_BIG);

	/*
	 * "exec" the command so that the returned status is that of
	 * RMCOMMAND and not the shell.
	 */
	(void) snprintf(cmdbuf, sizeof (cmdbuf), EXEC_PREFIX RMCOMMAND " %s",
	    devpath);
	status = do_subproc(cmdbuf);
	if ((err = subproc_status(RMCOMMAND, status, B_TRUE)) !=
	    ZONE_SUBPROC_OK) {
		(void) fprintf(stderr,
		    gettext("could not remove existing /dev\n"));
		return (Z_ERR);
	}

	/* In order to ready the zone, it must be in the installed state */
	if ((err = zone_set_state(target_zone, ZONE_STATE_INSTALLED)) != Z_OK) {
		errno = err;
		zperror(gettext("could not reset state"), B_TRUE);
		return (Z_ERR);
	}

	/* We have to ready the zone to regen the dev tree */
	zarg.cmd = Z_READY;
	if (call_zoneadmd(target_zone, &zarg) != 0) {
		zerror(gettext("call to %s failed"), "zoneadmd");
		/* attempt to restore zone to configured state */
		(void) zone_set_state(target_zone, ZONE_STATE_CONFIGURED);
		return (Z_ERR);
	}

	zarg.cmd = Z_HALT;
	if (call_zoneadmd(target_zone, &zarg) != 0) {
		zerror(gettext("call to %s failed"), "zoneadmd");
		/* attempt to restore zone to configured state */
		(void) zone_set_state(target_zone, ZONE_STATE_CONFIGURED);
		return (Z_ERR);
	}

	/* attempt to restore zone to configured state */
	(void) zone_set_state(target_zone, ZONE_STATE_CONFIGURED);

	if (zonecfg_setdevperment(handle) != Z_OK) {
		(void) fprintf(stderr,
		    gettext("unable to enumerate device entries\n"));
		return (Z_ERR);
	}

	while (zonecfg_getdevperment(handle, &devtab) == Z_OK) {
		int err;

		if ((err = zonecfg_devperms_apply(handle,
		    devtab.zone_devperm_name, devtab.zone_devperm_uid,
		    devtab.zone_devperm_gid, devtab.zone_devperm_mode,
		    devtab.zone_devperm_acl)) != Z_OK && err != Z_INVAL)
			(void) fprintf(stderr, gettext("error updating device "
			    "%s: %s\n"), devtab.zone_devperm_name,
			    zonecfg_strerror(err));

		free(devtab.zone_devperm_acl);
	}

	(void) zonecfg_enddevperment(handle);

	return (Z_OK);
}

/*
 * Validate attaching a zone but don't actually do the work.  The zone
 * does not have to exist, so there is some complexity getting a new zone
 * configuration set up so that we can perform the validation.  This is
 * handled within zonecfg_attach_manifest() which returns two handles; one
 * for the the full configuration to validate (rem_handle) and the other
 * (local_handle) containing only the zone configuration derived from the
 * manifest.
 */
static int
dryrun_attach(char *manifest_path, char *argv[])
{
	int fd;
	int err;
	int res;
	zone_dochandle_t local_handle;
	zone_dochandle_t rem_handle = NULL;

	if (strcmp(manifest_path, "-") == 0) {
		fd = 0;
	} else if ((fd = open(manifest_path, O_RDONLY)) < 0) {
		zperror(gettext("could not open manifest path"), B_FALSE);
		return (Z_ERR);
	}

	if ((local_handle = zonecfg_init_handle()) == NULL) {
		zperror(cmd_to_str(CMD_ATTACH), B_TRUE);
		res = Z_ERR;
		goto done;
	}

	if ((rem_handle = zonecfg_init_handle()) == NULL) {
		zperror(cmd_to_str(CMD_ATTACH), B_TRUE);
		res = Z_ERR;
		goto done;
	}

	if ((err = zonecfg_attach_manifest(fd, local_handle, rem_handle))
	    != Z_OK) {
		res = Z_ERR;

		if (err == Z_INVALID_DOCUMENT) {
			struct stat st;
			char buf[6];

			if (strcmp(manifest_path, "-") == 0) {
				zerror(gettext("Input is not a valid XML "
				    "file"));
				goto done;
			}

			if (fstat(fd, &st) == -1 || !S_ISREG(st.st_mode)) {
				zerror(gettext("%s is not an XML file"),
				    manifest_path);
				goto done;
			}

			bzero(buf, sizeof (buf));
			(void) lseek(fd, 0L, SEEK_SET);
			if (read(fd, buf, sizeof (buf) - 1) < 0 ||
			    strncmp(buf, "<?xml", 5) != 0)
				zerror(gettext("%s is not an XML file"),
				    manifest_path);
			else
				zerror(gettext("Cannot attach to an earlier "
				    "release of the operating system"));
		} else {
			zperror(cmd_to_str(CMD_ATTACH), B_TRUE);
		}
		goto done;
	}

	/*
	 * Retrieve remote handle brand type and determine whether it is
	 * native or not.
	 */
	if (zonecfg_get_brand(rem_handle, target_brand, sizeof (target_brand))
	    != Z_OK) {
		zerror(gettext("missing or invalid brand"));
		exit(Z_ERR);
	}
	is_native_zone = (strcmp(target_brand, NATIVE_BRAND_NAME) == 0);
	is_cluster_zone =
	    (strcmp(target_brand, CLUSTER_BRAND_NAME) == 0);

	res = verify_handle(CMD_ATTACH, local_handle, argv);

	/* Get the detach information for the locally defined zone. */
	if ((err = zonecfg_get_detach_info(local_handle, B_FALSE)) != Z_OK) {
		errno = err;
		zperror(gettext("getting the attach information failed"),
		    B_TRUE);
		res = Z_ERR;
	} else {
		/* sw_cmp prints error msgs as necessary */
		if (sw_cmp(local_handle, rem_handle, SW_CMP_NONE) != Z_OK)
			res = Z_ERR;
	}

done:
	if (strcmp(manifest_path, "-") != 0)
		(void) close(fd);

	zonecfg_fini_handle(local_handle);
	zonecfg_fini_handle(rem_handle);

	return ((res == Z_OK) ? Z_OK : Z_ERR);
}

/*
 * Attempt to generate the information we need to make the zone look like
 * it was properly detached by using the pkg information contained within
 * the zone itself.
 *
 * We will perform a dry-run detach within the zone to generate the xml file.
 * To do this we need to be able to get a handle on the zone so we can see
 * how it is configured.  In order to get a handle, we need a copy of the
 * zone configuration within the zone.  Since the zone's configuration is
 * not available within the zone itself, we need to temporarily copy it into
 * the zone.
 *
 * The sequence of actions we are doing is this:
 *	[set zone state to installed]
 *	[mount zone]
 *	zlogin {zone} </etc/zones/{zone}.xml 'cat >/etc/zones/{zone}.xml'
 *	zlogin {zone} 'zoneadm -z {zone} detach -n' >{zonepath}/SUNWdetached.xml
 *	zlogin {zone} 'rm -f /etc/zones/{zone}.xml'
 *	[unmount zone]
 *	[set zone state to configured]
 *
 * The successful result of this function is that we will have a
 * SUNWdetached.xml file in the zonepath and we can use that to attach the zone.
 */
static boolean_t
gen_detach_info(char *zonepath)
{
	int		status;
	boolean_t	mounted = B_FALSE;
	boolean_t	res = B_FALSE;
	char		cmdbuf[2 * MAXPATHLEN];

	/*
	 * The zone has to be installed to mount and zlogin.  Temporarily set
	 * the state to 'installed'.
	 */
	if (zone_set_state(target_zone, ZONE_STATE_INSTALLED) != Z_OK)
		return (B_FALSE);

	/* Mount the zone so we can zlogin. */
	if (mount_func(0, NULL) != Z_OK)
		goto cleanup;
	mounted = B_TRUE;

	/*
	 * We need to copy the zones xml configuration file into the
	 * zone so we can get a handle for the zone while running inside
	 * the zone.
	 */
	if (snprintf(cmdbuf, sizeof (cmdbuf), "/usr/sbin/zlogin -S %s "
	    "</etc/zones/%s.xml '/usr/bin/cat >/etc/zones/%s.xml'",
	    target_zone, target_zone, target_zone) >= sizeof (cmdbuf))
		goto cleanup;

	status = do_subproc_interactive(cmdbuf);
	if (subproc_status("copy", status, B_TRUE) != ZONE_SUBPROC_OK)
		goto cleanup;

	/* Now run the detach command within the mounted zone. */
	if (snprintf(cmdbuf, sizeof (cmdbuf), "/usr/sbin/zlogin -S %s "
	    "'/usr/sbin/zoneadm -z %s detach -n' >%s/SUNWdetached.xml",
	    target_zone, target_zone, zonepath) >= sizeof (cmdbuf))
		goto cleanup;

	status = do_subproc_interactive(cmdbuf);
	if (subproc_status("detach", status, B_TRUE) != ZONE_SUBPROC_OK)
		goto cleanup;

	res = B_TRUE;

cleanup:
	/* Cleanup from the previous actions. */
	if (mounted) {
		if (snprintf(cmdbuf, sizeof (cmdbuf),
		    "/usr/sbin/zlogin -S %s '/usr/bin/rm -f /etc/zones/%s.xml'",
		    target_zone, target_zone) >= sizeof (cmdbuf)) {
			res = B_FALSE;
		} else {
			status = do_subproc_interactive(cmdbuf);
			if (subproc_status("rm", status, B_TRUE)
			    != ZONE_SUBPROC_OK)
				res = B_FALSE;
		}

		if (unmount_func(0, NULL) != Z_OK)
			res =  B_FALSE;
	}

	if (zone_set_state(target_zone, ZONE_STATE_CONFIGURED) != Z_OK)
		res = B_FALSE;

	return (res);
}

/*
 * The zone needs to be updated so set it up for the update and initiate the
 * update within the scratch zone.  First set the state to incomplete so we can
 * force-mount the zone for the update operation.  We pass the -U option to the
 * mount so that the scratch zone is mounted without the zone's /etc and /var
 * being lofs mounted back into the scratch zone root.  This is done by
 * overloading the bootbuf string in the zone_cmd_arg_t to pass -U as an option
 * to the mount cmd.
 */
static int
attach_update(zone_dochandle_t handle, char *zonepath)
{
	int err;
	int update_res;
	int status;
	zone_cmd_arg_t zarg;
	FILE *fp;
	struct zone_fstab fstab;
	char cmdbuf[(4 * MAXPATHLEN) + 20];

	if ((err = zone_set_state(target_zone, ZONE_STATE_INCOMPLETE))
	    != Z_OK) {
		errno = err;
		zperror(gettext("could not set state"), B_TRUE);
		return (Z_ERR);
	}

	zarg.cmd = Z_FORCEMOUNT;
	(void) strlcpy(zarg.bootbuf, "-U",  sizeof (zarg.bootbuf));
	if (call_zoneadmd(target_zone, &zarg) != 0) {
		zerror(gettext("could not mount zone"));

		/* We reset the state since the zone wasn't modified yet. */
		if ((err = zone_set_state(target_zone, ZONE_STATE_CONFIGURED))
		    != Z_OK) {
			errno = err;
			zperror(gettext("could not reset state"), B_TRUE);
		}
		return (Z_ERR);
	}

	/*
	 * Move data files generated by sw_up_to_date() into the scratch
	 * zone's /tmp.
	 */
	(void) snprintf(cmdbuf, sizeof (cmdbuf), "exec /usr/bin/mv "
	    "%s/pkg_add %s/pkg_rm %s/lu/tmp",
	    zonepath, zonepath, zonepath);

	status = do_subproc_interactive(cmdbuf);
	if (subproc_status("mv", status, B_TRUE) != ZONE_SUBPROC_OK) {
		zperror(gettext("could not mv data files"), B_FALSE);
		goto fail;
	}

	/*
	 * Save list of inherit-pkg-dirs into zone.  Since the file is in
	 * /tmp we don't have to worry about deleting it.
	 */
	(void) snprintf(cmdbuf, sizeof (cmdbuf), "%s/lu/tmp/inherited",
	    zonepath);
	if ((fp = fopen(cmdbuf, "w")) == NULL) {
		zperror(gettext("could not save inherit-pkg-dirs"), B_FALSE);
		goto fail;
	}
	if (zonecfg_setipdent(handle) != Z_OK) {
		zperror(gettext("could not enumerate inherit-pkg-dirs"),
		    B_TRUE);
		goto fail;
	}
	while (zonecfg_getipdent(handle, &fstab) == Z_OK) {
		if (fprintf(fp, "%s\n", fstab.zone_fs_dir) < 0) {
			zperror(gettext("could not save inherit-pkg-dirs"),
			    B_FALSE);
			(void) fclose(fp);
			goto fail;
		}
	}
	(void) zonecfg_endipdent(handle);
	if (fclose(fp) != 0) {
		zperror(gettext("could not save inherit-pkg-dirs"), B_FALSE);
		goto fail;
	}

	/* run the updater inside the scratch zone */
	(void) snprintf(cmdbuf, sizeof (cmdbuf),
	    "exec /usr/sbin/zlogin -S %s "
	    "/usr/lib/brand/native/attach_update %s", target_zone, target_zone);

	update_res = Z_OK;
	status = do_subproc_interactive(cmdbuf);
	if (subproc_status("attach_update", status, B_TRUE)
	    != ZONE_SUBPROC_OK) {
		zerror(gettext("could not update zone"));
		update_res = Z_ERR;
	}

	zarg.cmd = Z_UNMOUNT;
	if (call_zoneadmd(target_zone, &zarg) != 0) {
		zerror(gettext("could not unmount zone"));
		return (Z_ERR);
	}

	/*
	 * If the update script within the scratch zone failed for some reason
	 * we will now leave the zone in the incomplete state since we no
	 * longer know the state of the files within the zonepath.
	 */
	if (update_res == Z_ERR)
		return (Z_ERR);

	zonecfg_rm_detached(handle, B_FALSE);

	if ((err = zone_set_state(target_zone, ZONE_STATE_INSTALLED)) != Z_OK) {
		errno = err;
		zperror(gettext("could not set state"), B_TRUE);
		return (Z_ERR);
	}

	return (Z_OK);

fail:
	zarg.cmd = Z_UNMOUNT;
	if (call_zoneadmd(target_zone, &zarg) != 0)
		zerror(gettext("could not unmount zone"));

	/* We reset the state since the zone wasn't modified yet. */
	if ((err = zone_set_state(target_zone, ZONE_STATE_CONFIGURED))
	    != Z_OK) {
		errno = err;
		zperror(gettext("could not reset state"), B_TRUE);
	}

	return (Z_ERR);
}

/* ARGSUSED */
static void
sigcleanup(int sig)
{
	attach_interupted = B_TRUE;
}


static int
attach_func(int argc, char *argv[])
{
	int lockfd;
	int err, arg;
	boolean_t force = B_FALSE;
	zone_dochandle_t handle;
	zone_dochandle_t athandle = NULL;
	char zonepath[MAXPATHLEN];
	char brand[MAXNAMELEN], atbrand[MAXNAMELEN];
	char cmdbuf[MAXPATHLEN];
	boolean_t execute = B_TRUE;
	boolean_t retried = B_FALSE;
	boolean_t update = B_FALSE;
	char *manifest_path;
	brand_handle_t bh = NULL;

	if (zonecfg_in_alt_root()) {
		zerror(gettext("cannot attach zone in alternate root"));
		return (Z_ERR);
	}

	optind = 0;
	if ((arg = getopt(argc, argv, "?Fn:u")) != EOF) {
		switch (arg) {
		case '?':
			sub_usage(SHELP_ATTACH, CMD_ATTACH);
			return (optopt == '?' ? Z_OK : Z_USAGE);
		case 'F':
			force = B_TRUE;
			break;
		case 'n':
			execute = B_FALSE;
			manifest_path = optarg;
			break;
		case 'u':
			update = B_TRUE;
			break;
		default:
			sub_usage(SHELP_ATTACH, CMD_ATTACH);
			return (Z_USAGE);
		}
	}

	/* dry-run and update flags are mutually exclusive */
	if (!execute && update) {
		zerror(gettext("-n and -u flags are mutually exclusive"));
		return (Z_ERR);
	}

	/*
	 * If the no-execute option was specified, we need to branch down
	 * a completely different path since there is no zone required to be
	 * configured for this option.
	 */
	if (!execute)
		return (dryrun_attach(manifest_path, argv));

	if (sanity_check(target_zone, CMD_ATTACH, B_FALSE, B_TRUE, B_FALSE)
	    != Z_OK)
		return (Z_ERR);
	if (verify_details(CMD_ATTACH, argv) != Z_OK)
		return (Z_ERR);

	if ((err = zone_get_zonepath(target_zone, zonepath, sizeof (zonepath)))
	    != Z_OK) {
		errno = err;
		zperror2(target_zone, gettext("could not get zone path"));
		return (Z_ERR);
	}

	if ((handle = zonecfg_init_handle()) == NULL) {
		zperror(cmd_to_str(CMD_ATTACH), B_TRUE);
		return (Z_ERR);
	}

	if ((err = zonecfg_get_handle(target_zone, handle)) != Z_OK) {
		errno = err;
		zperror(cmd_to_str(CMD_ATTACH), B_TRUE);
		zonecfg_fini_handle(handle);
		return (Z_ERR);
	}

	/* Fetch the postattach hook from the brand configuration.  */
	if ((bh = brand_open(target_brand)) == NULL) {
		zerror(gettext("missing or invalid brand"));
		return (Z_ERR);
	}

	(void) strcpy(cmdbuf, EXEC_PREFIX);
	if (brand_get_postattach(bh, target_zone, zonepath, cmdbuf + EXEC_LEN,
	    sizeof (cmdbuf) - EXEC_LEN, 0, NULL) != 0) {
		zerror("invalid brand configuration: missing postattach "
		    "resource");
		brand_close(bh);
		return (Z_ERR);
	}
	brand_close(bh);

	/* If we have a brand postattach hook and the force flag, append it. */
	if (strlen(cmdbuf) > EXEC_LEN && force) {
		if (addopt(cmdbuf, 0, "-F", sizeof (cmdbuf)) != Z_OK) {
			zerror("Postattach command line too long");
			return (Z_ERR);
		}
	}

	if (grab_lock_file(target_zone, &lockfd) != Z_OK) {
		zerror(gettext("another %s may have an operation in progress."),
		    "zoneadm");
		zonecfg_fini_handle(handle);
		return (Z_ERR);
	}

	if (force)
		goto forced;

	if ((athandle = zonecfg_init_handle()) == NULL) {
		zperror(cmd_to_str(CMD_ATTACH), B_TRUE);
		goto done;
	}

retry:
	if ((err = zonecfg_get_attach_handle(zonepath, target_zone, B_TRUE,
	    athandle)) != Z_OK) {
		if (err == Z_NO_ZONE) {
			/*
			 * Zone was not detached.  Try to fall back to getting
			 * the needed information from within the zone.
			 * However, we can only try to generate the attach
			 * information for native branded zones, so fail if the
			 * zone is not native.
			 */
			if (!is_native_zone) {
				zerror(gettext("Not a detached zone."));
				goto done;
			}

			if (!retried) {
				zerror(gettext("The zone was not properly "
				    "detached.\n\tAttempting to attach "
				    "anyway."));
				if (gen_detach_info(zonepath)) {
					retried = B_TRUE;
					goto retry;
				}
			}
			zerror(gettext("Cannot generate the information "
			    "needed to attach this zone."));
		} else if (err == Z_INVALID_DOCUMENT) {
			zerror(gettext("Cannot attach to an earlier release "
			    "of the operating system"));
		} else {
			zperror(cmd_to_str(CMD_ATTACH), B_TRUE);
		}
		goto done;
	}

	/* Get the detach information for the locally defined zone. */
	if ((err = zonecfg_get_detach_info(handle, B_FALSE)) != Z_OK) {
		errno = err;
		zperror(gettext("getting the attach information failed"),
		    B_TRUE);
		goto done;
	}

	/*
	 * Ensure that the detached and locally defined zones are both of
	 * the same brand.
	 */
	if ((zonecfg_get_brand(handle, brand, sizeof (brand)) != 0) ||
	    (zonecfg_get_brand(athandle, atbrand, sizeof (atbrand)) != 0)) {
		err = Z_ERR;
		zerror(gettext("missing or invalid brand"));
		goto done;
	}

	if (strcmp(atbrand, brand) != NULL) {
		err = Z_ERR;
		zerror(gettext("Trying to attach a '%s' zone to a '%s' "
		    "configuration."), atbrand, brand);
		goto done;
	}

	/*
	 * If we're doing an update on attach, and the zone does need to be
	 * updated, then run the update.
	 */
	if (update) {
		char fname[MAXPATHLEN];

		(void) sigset(SIGINT, sigcleanup);

		if ((err = sw_up_to_date(handle, athandle, zonepath)) != Z_OK) {
			if (err != Z_FATAL && !attach_interupted) {
				err = Z_FATAL;
				err = attach_update(handle, zonepath);
			}
			if (!attach_interupted || err == Z_OK)
				goto done;
		}

		(void) sigset(SIGINT, SIG_DFL);

		/* clean up data files left behind by sw_up_to_date() */
		(void) snprintf(fname, sizeof (fname), "%s/pkg_add", zonepath);
		(void) unlink(fname);
		(void) snprintf(fname, sizeof (fname), "%s/pkg_rm", zonepath);
		(void) unlink(fname);

		if (attach_interupted) {
			err = Z_FATAL;
			goto done;
		}

	} else {
		/* sw_cmp prints error msgs as necessary */
		if ((err = sw_cmp(handle, athandle, SW_CMP_NONE)) != Z_OK)
			goto done;

		if ((err = dev_fix(athandle)) != Z_OK)
			goto done;
	}

forced:

	zonecfg_rm_detached(handle, force);

	if ((err = zone_set_state(target_zone, ZONE_STATE_INSTALLED)) != Z_OK) {
		errno = err;
		zperror(gettext("could not reset state"), B_TRUE);
	}

done:
	zonecfg_fini_handle(handle);
	release_lock_file(lockfd);
	if (athandle != NULL)
		zonecfg_fini_handle(athandle);

	/* If we have a brand postattach hook, run it. */
	if (err == Z_OK && strlen(cmdbuf) > EXEC_LEN) {
		int status;

		status = do_subproc(cmdbuf);
		if (subproc_status(gettext("brand-specific postattach"),
		    status, B_FALSE) != ZONE_SUBPROC_OK) {
			if ((err = zone_set_state(target_zone,
			    ZONE_STATE_CONFIGURED)) != Z_OK) {
				errno = err;
				zperror(gettext("could not reset state"),
				    B_TRUE);
			}
			return (Z_ERR);
		}
	}

	return ((err == Z_OK) ? Z_OK : Z_ERR);
}

/*
 * On input, TRUE => yes, FALSE => no.
 * On return, TRUE => 1, FALSE => 0, could not ask => -1.
 */

static int
ask_yesno(boolean_t default_answer, const char *question)
{
	char line[64];	/* should be large enough to answer yes or no */

	if (!isatty(STDIN_FILENO))
		return (-1);
	for (;;) {
		(void) printf("%s (%s)? ", question,
		    default_answer ? "[y]/n" : "y/[n]");
		if (fgets(line, sizeof (line), stdin) == NULL ||
		    line[0] == '\n')
			return (default_answer ? 1 : 0);
		if (tolower(line[0]) == 'y')
			return (1);
		if (tolower(line[0]) == 'n')
			return (0);
	}
}

static int
uninstall_func(int argc, char *argv[])
{
	char line[ZONENAME_MAX + 128];	/* Enough for "Are you sure ..." */
	char rootpath[MAXPATHLEN], zonepath[MAXPATHLEN];
	char cmdbuf[MAXPATHLEN];
	boolean_t force = B_FALSE;
	int lockfd, answer;
	int err, arg;
	brand_handle_t bh = NULL;

	if (zonecfg_in_alt_root()) {
		zerror(gettext("cannot uninstall zone in alternate root"));
		return (Z_ERR);
	}

	optind = 0;
	while ((arg = getopt(argc, argv, "?F")) != EOF) {
		switch (arg) {
		case '?':
			sub_usage(SHELP_UNINSTALL, CMD_UNINSTALL);
			return (optopt == '?' ? Z_OK : Z_USAGE);
		case 'F':
			force = B_TRUE;
			break;
		default:
			sub_usage(SHELP_UNINSTALL, CMD_UNINSTALL);
			return (Z_USAGE);
		}
	}
	if (argc > optind) {
		sub_usage(SHELP_UNINSTALL, CMD_UNINSTALL);
		return (Z_USAGE);
	}

	if (sanity_check(target_zone, CMD_UNINSTALL, B_FALSE, B_TRUE, B_FALSE)
	    != Z_OK)
		return (Z_ERR);

	/*
	 * Invoke brand-specific handler.
	 */
	if (invoke_brand_handler(CMD_UNINSTALL, argv) != Z_OK)
		return (Z_ERR);

	if (!force) {
		(void) snprintf(line, sizeof (line),
		    gettext("Are you sure you want to %s zone %s"),
		    cmd_to_str(CMD_UNINSTALL), target_zone);
		if ((answer = ask_yesno(B_FALSE, line)) == 0) {
			return (Z_OK);
		} else if (answer == -1) {
			zerror(gettext("Input not from terminal and -F "
			    "not specified: %s not done."),
			    cmd_to_str(CMD_UNINSTALL));
			return (Z_ERR);
		}
	}

	if ((err = zone_get_zonepath(target_zone, zonepath,
	    sizeof (zonepath))) != Z_OK) {
		errno = err;
		zperror2(target_zone, gettext("could not get zone path"));
		return (Z_ERR);
	}
	if ((err = zone_get_rootpath(target_zone, rootpath,
	    sizeof (rootpath))) != Z_OK) {
		errno = err;
		zperror2(target_zone, gettext("could not get root path"));
		return (Z_ERR);
	}

	/*
	 * If there seems to be a zoneadmd running for this zone, call it
	 * to tell it that an uninstall is happening; if all goes well it
	 * will then shut itself down.
	 */
	if (ping_zoneadmd(target_zone) == Z_OK) {
		zone_cmd_arg_t zarg;
		zarg.cmd = Z_NOTE_UNINSTALLING;
		/* we don't care too much if this fails... just plow on */
		(void) call_zoneadmd(target_zone, &zarg);
	}

	if (grab_lock_file(target_zone, &lockfd) != Z_OK) {
		zerror(gettext("another %s may have an operation in progress."),
		    "zoneadm");
		return (Z_ERR);
	}

	/* Don't uninstall the zone if anything is mounted there */
	err = zonecfg_find_mounts(rootpath, NULL, NULL);
	if (err) {
		zerror(gettext("These file systems are mounted on "
		    "subdirectories of %s.\n"), rootpath);
		(void) zonecfg_find_mounts(rootpath, zfm_print, NULL);
		return (Z_ERR);
	}

	/* Fetch the uninstall hook from the brand configuration.  */
	if ((bh = brand_open(target_brand)) == NULL) {
		zerror(gettext("missing or invalid brand"));
		return (Z_ERR);
	}

	(void) strcpy(cmdbuf, EXEC_PREFIX);
	if (brand_get_preuninstall(bh, target_zone, zonepath,
	    cmdbuf + EXEC_LEN, sizeof (cmdbuf) - EXEC_LEN, 0, NULL)
	    != 0) {
		zerror("invalid brand configuration: missing preuninstall "
		    "resource");
		brand_close(bh);
		return (Z_ERR);
	}
	brand_close(bh);

	if (strlen(cmdbuf) > EXEC_LEN) {
		int status;

		/* If we have the force flag, append it. */
		if (force && addopt(cmdbuf, 0, "-F", sizeof (cmdbuf)) != Z_OK) {
			zerror("Preuninstall command line too long");
			return (Z_ERR);
		}

		status = do_subproc(cmdbuf);
		if (subproc_status(gettext("brand-specific preuninstall"),
		    status, B_FALSE) != ZONE_SUBPROC_OK) {
			return (Z_ERR);
		}
	}

	err = zone_set_state(target_zone, ZONE_STATE_INCOMPLETE);
	if (err != Z_OK) {
		errno = err;
		zperror2(target_zone, gettext("could not set state"));
		goto bad;
	}

	if ((err = cleanup_zonepath(zonepath, B_FALSE)) != Z_OK) {
		errno = err;
		zperror2(target_zone, gettext("cleaning up zonepath failed"));
		goto bad;
	}

	err = zone_set_state(target_zone, ZONE_STATE_CONFIGURED);
	if (err != Z_OK) {
		errno = err;
		zperror2(target_zone, gettext("could not reset state"));
	}
bad:
	release_lock_file(lockfd);
	return (err);
}

/* ARGSUSED */
static int
mount_func(int argc, char *argv[])
{
	zone_cmd_arg_t zarg;
	boolean_t force = B_FALSE;
	int arg;

	/*
	 * The only supported subargument to the "mount" subcommand is
	 * "-f", which forces us to mount a zone in the INCOMPLETE state.
	 */
	optind = 0;
	if ((arg = getopt(argc, argv, "f")) != EOF) {
		switch (arg) {
		case 'f':
			force = B_TRUE;
			break;
		default:
			return (Z_USAGE);
		}
	}
	if (argc > optind)
		return (Z_USAGE);

	if (sanity_check(target_zone, CMD_MOUNT, B_FALSE, B_FALSE, force)
	    != Z_OK)
		return (Z_ERR);
	if (verify_details(CMD_MOUNT, argv) != Z_OK)
		return (Z_ERR);

	zarg.cmd = force ? Z_FORCEMOUNT : Z_MOUNT;
	zarg.bootbuf[0] = '\0';
	if (call_zoneadmd(target_zone, &zarg) != 0) {
		zerror(gettext("call to %s failed"), "zoneadmd");
		return (Z_ERR);
	}
	return (Z_OK);
}

/* ARGSUSED */
static int
unmount_func(int argc, char *argv[])
{
	zone_cmd_arg_t zarg;

	if (argc > 0)
		return (Z_USAGE);
	if (sanity_check(target_zone, CMD_UNMOUNT, B_FALSE, B_FALSE, B_FALSE)
	    != Z_OK)
		return (Z_ERR);

	zarg.cmd = Z_UNMOUNT;
	if (call_zoneadmd(target_zone, &zarg) != 0) {
		zerror(gettext("call to %s failed"), "zoneadmd");
		return (Z_ERR);
	}
	return (Z_OK);
}

static int
mark_func(int argc, char *argv[])
{
	int err, lockfd;

	if (argc != 1 || strcmp(argv[0], "incomplete") != 0)
		return (Z_USAGE);
	if (sanity_check(target_zone, CMD_MARK, B_FALSE, B_FALSE, B_FALSE)
	    != Z_OK)
		return (Z_ERR);

	/*
	 * Invoke brand-specific handler.
	 */
	if (invoke_brand_handler(CMD_MARK, argv) != Z_OK)
		return (Z_ERR);

	if (grab_lock_file(target_zone, &lockfd) != Z_OK) {
		zerror(gettext("another %s may have an operation in progress."),
		    "zoneadm");
		return (Z_ERR);
	}

	err = zone_set_state(target_zone, ZONE_STATE_INCOMPLETE);
	if (err != Z_OK) {
		errno = err;
		zperror2(target_zone, gettext("could not set state"));
	}
	release_lock_file(lockfd);

	return (err);
}

/*
 * Check what scheduling class we're running under and print a warning if
 * we're not using FSS.
 */
static int
check_sched_fss(zone_dochandle_t handle)
{
	char class_name[PC_CLNMSZ];

	if (zonecfg_get_dflt_sched_class(handle, class_name,
	    sizeof (class_name)) != Z_OK) {
		zerror(gettext("WARNING: unable to determine the zone's "
		    "scheduling class"));
	} else if (strcmp("FSS", class_name) != 0) {
		zerror(gettext("WARNING: The zone.cpu-shares rctl is set but\n"
		    "FSS is not the default scheduling class for this zone.  "
		    "FSS will be\nused for processes in the zone but to get "
		    "the full benefit of FSS,\nit should be the default "
		    "scheduling class.  See dispadmin(1M) for\nmore details."));
		return (Z_SYSTEM);
	}

	return (Z_OK);
}

static int
check_cpu_shares_sched(zone_dochandle_t handle)
{
	int err;
	int res = Z_OK;
	struct zone_rctltab rctl;

	if ((err = zonecfg_setrctlent(handle)) != Z_OK) {
		errno = err;
		zperror(cmd_to_str(CMD_APPLY), B_TRUE);
		return (err);
	}

	while (zonecfg_getrctlent(handle, &rctl) == Z_OK) {
		if (strcmp(rctl.zone_rctl_name, "zone.cpu-shares") == 0) {
			if (check_sched_fss(handle) != Z_OK)
				res = Z_SYSTEM;
			break;
		}
	}

	(void) zonecfg_endrctlent(handle);

	return (res);
}

/*
 * Check if there is a mix of processes running in different pools within the
 * zone.  This is currently only going to be called for the global zone from
 * apply_func but that could be generalized in the future.
 */
static boolean_t
mixed_pools(zoneid_t zoneid)
{
	DIR *dirp;
	dirent_t *dent;
	boolean_t mixed = B_FALSE;
	boolean_t poolid_set = B_FALSE;
	poolid_t last_poolid = 0;

	if ((dirp = opendir("/proc")) == NULL) {
		zerror(gettext("could not open /proc"));
		return (B_FALSE);
	}

	while ((dent = readdir(dirp)) != NULL) {
		int procfd;
		psinfo_t ps;
		char procpath[MAXPATHLEN];

		if (dent->d_name[0] == '.')
			continue;

		(void) snprintf(procpath, sizeof (procpath), "/proc/%s/psinfo",
		    dent->d_name);

		if ((procfd = open(procpath, O_RDONLY)) == -1)
			continue;

		if (read(procfd, &ps, sizeof (ps)) == sizeof (psinfo_t)) {
			/* skip processes in other zones and system processes */
			if (zoneid != ps.pr_zoneid || ps.pr_flag & SSYS) {
				(void) close(procfd);
				continue;
			}

			if (poolid_set) {
				if (ps.pr_poolid != last_poolid)
					mixed = B_TRUE;
			} else {
				last_poolid = ps.pr_poolid;
				poolid_set = B_TRUE;
			}
		}

		(void) close(procfd);

		if (mixed)
			break;
	}

	(void) closedir(dirp);

	return (mixed);
}

/*
 * Check if a persistent or temporary pool is configured for the zone.
 * This is currently only going to be called for the global zone from
 * apply_func but that could be generalized in the future.
 */
static boolean_t
pool_configured(zone_dochandle_t handle)
{
	int err1, err2;
	struct zone_psettab pset_tab;
	char poolname[MAXPATHLEN];

	err1 = zonecfg_lookup_pset(handle, &pset_tab);
	err2 = zonecfg_get_pool(handle, poolname, sizeof (poolname));

	if (err1 == Z_NO_ENTRY &&
	    (err2 == Z_NO_ENTRY || (err2 == Z_OK && strlen(poolname) == 0)))
		return (B_FALSE);

	return (B_TRUE);
}

/*
 * This is an undocumented interface which is currently only used to apply
 * the global zone resource management settings when the system boots.
 * This function does not yet properly handle updating a running system so
 * any projects running in the zone would be trashed if this function
 * were to run after the zone had booted.  It also does not reset any
 * rctl settings that were removed from zonecfg.  There is still work to be
 * done before we can properly support dynamically updating the resource
 * management settings for a running zone (global or non-global).  Thus, this
 * functionality is undocumented for now.
 */
/* ARGSUSED */
static int
apply_func(int argc, char *argv[])
{
	int err;
	int res = Z_OK;
	priv_set_t *privset;
	zoneid_t zoneid;
	zone_dochandle_t handle;
	struct zone_mcaptab mcap;
	char pool_err[128];

	zoneid = getzoneid();

	if (zonecfg_in_alt_root() || zoneid != GLOBAL_ZONEID ||
	    target_zone == NULL || strcmp(target_zone, GLOBAL_ZONENAME) != 0)
		return (usage(B_FALSE));

	if ((privset = priv_allocset()) == NULL) {
		zerror(gettext("%s failed"), "priv_allocset");
		return (Z_ERR);
	}

	if (getppriv(PRIV_EFFECTIVE, privset) != 0) {
		zerror(gettext("%s failed"), "getppriv");
		priv_freeset(privset);
		return (Z_ERR);
	}

	if (priv_isfullset(privset) == B_FALSE) {
		(void) usage(B_FALSE);
		priv_freeset(privset);
		return (Z_ERR);
	}
	priv_freeset(privset);

	if ((handle = zonecfg_init_handle()) == NULL) {
		zperror(cmd_to_str(CMD_APPLY), B_TRUE);
		return (Z_ERR);
	}

	if ((err = zonecfg_get_handle(target_zone, handle)) != Z_OK) {
		errno = err;
		zperror(cmd_to_str(CMD_APPLY), B_TRUE);
		zonecfg_fini_handle(handle);
		return (Z_ERR);
	}

	/* specific error msgs are printed within apply_rctls */
	if ((err = zonecfg_apply_rctls(target_zone, handle)) != Z_OK) {
		errno = err;
		zperror(cmd_to_str(CMD_APPLY), B_TRUE);
		res = Z_ERR;
	}

	if ((err = check_cpu_shares_sched(handle)) != Z_OK)
		res = Z_ERR;

	if (pool_configured(handle)) {
		if (mixed_pools(zoneid)) {
			zerror(gettext("Zone is using multiple resource "
			    "pools.  The pool\nconfiguration cannot be "
			    "applied without rebooting."));
			res = Z_ERR;
		} else {

			/*
			 * The next two blocks of code attempt to set up
			 * temporary pools as well as persistent pools.  In
			 * both cases we call the functions unconditionally.
			 * Within each funtion the code will check if the zone
			 * is actually configured for a temporary pool or
			 * persistent pool and just return if there is nothing
			 * to do.
			 */
			if ((err = zonecfg_bind_tmp_pool(handle, zoneid,
			    pool_err, sizeof (pool_err))) != Z_OK) {
				if (err == Z_POOL || err == Z_POOL_CREATE ||
				    err == Z_POOL_BIND)
					zerror("%s: %s", zonecfg_strerror(err),
					    pool_err);
				else
					zerror(gettext("could not bind zone to "
					    "temporary pool: %s"),
					    zonecfg_strerror(err));
				res = Z_ERR;
			}

			if ((err = zonecfg_bind_pool(handle, zoneid, pool_err,
			    sizeof (pool_err))) != Z_OK) {
				if (err == Z_POOL || err == Z_POOL_BIND)
					zerror("%s: %s", zonecfg_strerror(err),
					    pool_err);
				else
					zerror("%s", zonecfg_strerror(err));
			}
		}
	}

	/*
	 * If a memory cap is configured, set the cap in the kernel using
	 * zone_setattr() and make sure the rcapd SMF service is enabled.
	 */
	if (zonecfg_getmcapent(handle, &mcap) == Z_OK) {
		uint64_t num;
		char smf_err[128];

		num = (uint64_t)strtoll(mcap.zone_physmem_cap, NULL, 10);
		if (zone_setattr(zoneid, ZONE_ATTR_PHYS_MCAP, &num, 0) == -1) {
			zerror(gettext("could not set zone memory cap"));
			res = Z_ERR;
		}

		if (zonecfg_enable_rcapd(smf_err, sizeof (smf_err)) != Z_OK) {
			zerror(gettext("enabling system/rcap service failed: "
			    "%s"), smf_err);
			res = Z_ERR;
		}
	}

	zonecfg_fini_handle(handle);

	return (res);
}

static int
help_func(int argc, char *argv[])
{
	int arg, cmd_num;

	if (argc == 0) {
		(void) usage(B_TRUE);
		return (Z_OK);
	}
	optind = 0;
	if ((arg = getopt(argc, argv, "?")) != EOF) {
		switch (arg) {
		case '?':
			sub_usage(SHELP_HELP, CMD_HELP);
			return (optopt == '?' ? Z_OK : Z_USAGE);
		default:
			sub_usage(SHELP_HELP, CMD_HELP);
			return (Z_USAGE);
		}
	}
	while (optind < argc) {
		/* Private commands have NULL short_usage; omit them */
		if ((cmd_num = cmd_match(argv[optind])) < 0 ||
		    cmdtab[cmd_num].short_usage == NULL) {
			sub_usage(SHELP_HELP, CMD_HELP);
			return (Z_USAGE);
		}
		sub_usage(cmdtab[cmd_num].short_usage, cmd_num);
		optind++;
	}
	return (Z_OK);
}

/*
 * Returns: CMD_MIN thru CMD_MAX on success, -1 on error
 */

static int
cmd_match(char *cmd)
{
	int i;

	for (i = CMD_MIN; i <= CMD_MAX; i++) {
		/* return only if there is an exact match */
		if (strcmp(cmd, cmdtab[i].cmd_name) == 0)
			return (cmdtab[i].cmd_num);
	}
	return (-1);
}

static int
parse_and_run(int argc, char *argv[])
{
	int i = cmd_match(argv[0]);

	if (i < 0)
		return (usage(B_FALSE));
	return (cmdtab[i].handler(argc - 1, &(argv[1])));
}

static char *
get_execbasename(char *execfullname)
{
	char *last_slash, *execbasename;

	/* guard against '/' at end of command invocation */
	for (;;) {
		last_slash = strrchr(execfullname, '/');
		if (last_slash == NULL) {
			execbasename = execfullname;
			break;
		} else {
			execbasename = last_slash + 1;
			if (*execbasename == '\0') {
				*last_slash = '\0';
				continue;
			}
			break;
		}
	}
	return (execbasename);
}

int
main(int argc, char **argv)
{
	int arg;
	zoneid_t zid;
	struct stat st;
	char *zone_lock_env;
	int err;

	if ((locale = setlocale(LC_ALL, "")) == NULL)
		locale = "C";
	(void) textdomain(TEXT_DOMAIN);
	setbuf(stdout, NULL);
	(void) sigset(SIGHUP, SIG_IGN);
	execname = get_execbasename(argv[0]);
	target_zone = NULL;
	if (chdir("/") != 0) {
		zerror(gettext("could not change directory to /."));
		exit(Z_ERR);
	}

	if (init_zfs() != Z_OK)
		exit(Z_ERR);

	while ((arg = getopt(argc, argv, "?u:z:R:")) != EOF) {
		switch (arg) {
		case '?':
			return (usage(B_TRUE));
		case 'u':
			target_uuid = optarg;
			break;
		case 'z':
			target_zone = optarg;
			break;
		case 'R':	/* private option for admin/install use */
			if (*optarg != '/') {
				zerror(gettext("root path must be absolute."));
				exit(Z_ERR);
			}
			if (stat(optarg, &st) == -1 || !S_ISDIR(st.st_mode)) {
				zerror(
				    gettext("root path must be a directory."));
				exit(Z_ERR);
			}
			zonecfg_set_root(optarg);
			break;
		default:
			return (usage(B_FALSE));
		}
	}

	if (optind >= argc)
		return (usage(B_FALSE));

	if (target_uuid != NULL && *target_uuid != '\0') {
		uuid_t uuid;
		static char newtarget[ZONENAME_MAX];

		if (uuid_parse(target_uuid, uuid) == -1) {
			zerror(gettext("illegal UUID value specified"));
			exit(Z_ERR);
		}
		if (zonecfg_get_name_by_uuid(uuid, newtarget,
		    sizeof (newtarget)) == Z_OK)
			target_zone = newtarget;
	}

	if (target_zone != NULL && zone_get_id(target_zone, &zid) != 0) {
		errno = Z_NO_ZONE;
		zperror(target_zone, B_TRUE);
		exit(Z_ERR);
	}

	/*
	 * See if we have inherited the right to manipulate this zone from
	 * a zoneadm instance in our ancestry.  If so, set zone_lock_cnt to
	 * indicate it.  If not, make that explicit in our environment.
	 */
	zone_lock_env = getenv(LOCK_ENV_VAR);
	if (zone_lock_env == NULL) {
		if (putenv(zoneadm_lock_not_held) != 0) {
			zperror(target_zone, B_TRUE);
			exit(Z_ERR);
		}
	} else {
		zoneadm_is_nested = B_TRUE;
		if (atoi(zone_lock_env) == 1)
			zone_lock_cnt = 1;
	}

	/*
	 * If we are going to be operating on a single zone, retrieve its
	 * brand type and determine whether it is native or not.
	 */
	if ((target_zone != NULL) &&
	    (strcmp(target_zone, GLOBAL_ZONENAME) != NULL)) {
		if (zone_get_brand(target_zone, target_brand,
		    sizeof (target_brand)) != Z_OK) {
			zerror(gettext("missing or invalid brand"));
			exit(Z_ERR);
		}
		is_native_zone = (strcmp(target_brand, NATIVE_BRAND_NAME) == 0);
		is_cluster_zone =
		    (strcmp(target_brand, CLUSTER_BRAND_NAME) == 0);
	}

	err = parse_and_run(argc - optind, &argv[optind]);

	return (err);
}
