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

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioccom.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>
#include <wait.h>
#include <signal.h>
#include <libscf.h>
#include <limits.h>
#include <priv_utils.h>
#include <door.h>
#include <errno.h>
#include <syslog.h>
#include <pthread.h>
#include <time.h>
#include <libscf.h>
#include <zone.h>
#include <tzfile.h>
#include <libgen.h>
#include <pwd.h>
#include <grp.h>

#include <smbsrv/smb_door_svc.h>
#include <smbsrv/smb_ioctl.h>
#include <smbsrv/libsmb.h>
#include <smbsrv/libsmbns.h>
#include <smbsrv/libsmbrdr.h>
#include <smbsrv/libmlsvc.h>

#include "smbd.h"

#define	DRV_DEVICE_PATH	"/devices/pseudo/smbsrv@0:smbsrv"
#define	SMB_DBDIR "/var/smb"

extern void *smbd_nbt_listener(void *);
extern void *smbd_tcp_listener(void *);

static int smbd_daemonize_init(void);
static void smbd_daemonize_fini(int, int);

static int smbd_kernel_bind(int, int, int);
static void smbd_kernel_unbind(void);
static int smbd_already_running(void);

static int smbd_service_init(void);
static void smbd_service_fini(void);

static int smbd_setup_options(int argc, char *argv[]);
static void smbd_usage(FILE *fp);
static void smbd_report(const char *fmt, ...);

static void smbd_sig_handler(int sig);

static int smbd_localtime_init(void);
static void *smbd_localtime_monitor(void *arg);


static pthread_t localtime_thr;

static int smbd_refresh_init(void);
static void smbd_refresh_fini(void);
static void *smbd_refresh_monitor(void *);
static pthread_t nbt_listener;
static pthread_t tcp_listener;
static pthread_t refresh_thr;
static pthread_cond_t refresh_cond;
static pthread_mutex_t refresh_mutex;

smbd_t smbd;

/*
 * smbd user land daemon
 *
 * Use SMF error codes only on return or exit.
 */
int
main(int argc, char *argv[])
{
	struct sigaction act;
	sigset_t set;
	uid_t uid;
	int pfd = -1;

	smbd.s_pname = basename(argv[0]);
	openlog(smbd.s_pname, LOG_PID | LOG_NOWAIT, LOG_DAEMON);

	if (smbd_setup_options(argc, argv) != 0)
		return (SMF_EXIT_ERR_FATAL);

	if ((uid = getuid()) != smbd.s_uid) {
		smbd_report("user %d: %s", uid, strerror(EPERM));
		return (SMF_EXIT_ERR_FATAL);
	}

	if (getzoneid() != GLOBAL_ZONEID) {
		smbd_report("non-global zones are not supported");
		return (SMF_EXIT_ERR_FATAL);
	}

	if (is_system_labeled()) {
		smbd_report("Trusted Extensions not supported");
		return (SMF_EXIT_ERR_FATAL);
	}

	if (smbd_already_running())
		return (SMF_EXIT_OK);

	(void) sigfillset(&set);
	(void) sigdelset(&set, SIGABRT);

	(void) sigfillset(&act.sa_mask);
	act.sa_handler = smbd_sig_handler;
	act.sa_flags = 0;

	(void) sigaction(SIGTERM, &act, NULL);
	(void) sigaction(SIGHUP, &act, NULL);
	(void) sigaction(SIGINT, &act, NULL);
	(void) sigaction(SIGPIPE, &act, NULL);

	(void) sigdelset(&set, SIGTERM);
	(void) sigdelset(&set, SIGHUP);
	(void) sigdelset(&set, SIGINT);
	(void) sigdelset(&set, SIGPIPE);

	if (smbd.s_fg) {
		(void) sigdelset(&set, SIGTSTP);
		(void) sigdelset(&set, SIGTTIN);
		(void) sigdelset(&set, SIGTTOU);

		if (smbd_service_init() != 0) {
			smbd_report("service initialization failed");
			exit(SMF_EXIT_ERR_FATAL);
		}
	} else {
		/*
		 * "pfd" is a pipe descriptor -- any fatal errors
		 * during subsequent initialization of the child
		 * process should be written to this pipe and the
		 * parent will report this error as the exit status.
		 */
		pfd = smbd_daemonize_init();

		if (smbd_service_init() != 0) {
			smbd_report("daemon initialization failed");
			exit(SMF_EXIT_ERR_FATAL);
		}

		smbd_daemonize_fini(pfd, SMF_EXIT_OK);
	}

	(void) atexit(smbd_service_fini);

	while (!smbd.s_shutdown_flag) {
		(void) sigsuspend(&set);

		switch (smbd.s_sigval) {
		case 0:
			break;

		case SIGPIPE:
			break;

		case SIGHUP:
			/* Refresh config was triggered */
			if (smbd.s_fg)
				smbd_report("reconfiguration requested");
			(void) pthread_cond_signal(&refresh_cond);
			break;

		default:
			/*
			 * Typically SIGINT or SIGTERM.
			 */
			smbd.s_shutdown_flag = 1;
			break;
		}

		smbd.s_sigval = 0;
	}

	smbd_service_fini();
	closelog();
	return (SMF_EXIT_OK);
}

/*
 * This function will fork off a child process,
 * from which only the child will return.
 *
 * Use SMF error codes only on exit.
 */
static int
smbd_daemonize_init(void)
{
	int status, pfds[2];
	sigset_t set, oset;
	pid_t pid;
	int rc;

	/*
	 * Reset privileges to the minimum set required. We continue
	 * to run as root to create and access files in /var.
	 */
	rc = __init_daemon_priv(PU_RESETGROUPS | PU_LIMITPRIVS,
	    smbd.s_uid, smbd.s_gid,
	    PRIV_NET_MAC_AWARE, PRIV_NET_PRIVADDR, PRIV_PROC_AUDIT,
	    PRIV_SYS_DEVICES, PRIV_SYS_SMB, NULL);

	if (rc != 0) {
		smbd_report("insufficient privileges");
		exit(SMF_EXIT_ERR_FATAL);
	}

	/*
	 * Block all signals prior to the fork and leave them blocked in the
	 * parent so we don't get in a situation where the parent gets SIGINT
	 * and returns non-zero exit status and the child is actually running.
	 * In the child, restore the signal mask once we've done our setsid().
	 */
	(void) sigfillset(&set);
	(void) sigdelset(&set, SIGABRT);
	(void) sigprocmask(SIG_BLOCK, &set, &oset);

	if (pipe(pfds) == -1) {
		smbd_report("unable to create pipe");
		exit(SMF_EXIT_ERR_FATAL);
	}

	closelog();

	if ((pid = fork()) == -1) {
		openlog(smbd.s_pname, LOG_PID | LOG_NOWAIT, LOG_DAEMON);
		smbd_report("unable to fork");
		closelog();
		exit(SMF_EXIT_ERR_FATAL);
	}

	/*
	 * If we're the parent process, wait for either the child to send us
	 * the appropriate exit status over the pipe or for the read to fail
	 * (presumably with 0 for EOF if our child terminated abnormally).
	 * If the read fails, exit with either the child's exit status if it
	 * exited or with SMF_EXIT_ERR_FATAL if it died from a fatal signal.
	 */
	if (pid != 0) {
		(void) close(pfds[1]);

		if (read(pfds[0], &status, sizeof (status)) == sizeof (status))
			_exit(status);

		if (waitpid(pid, &status, 0) == pid && WIFEXITED(status))
			_exit(WEXITSTATUS(status));

		_exit(SMF_EXIT_ERR_FATAL);
	}

	openlog(smbd.s_pname, LOG_PID | LOG_NOWAIT, LOG_DAEMON);
	smbd.s_pid = getpid();
	(void) setsid();
	(void) sigprocmask(SIG_SETMASK, &oset, NULL);
	(void) chdir("/");
	(void) umask(022);
	(void) close(pfds[0]);

	return (pfds[1]);
}

static void
smbd_daemonize_fini(int fd, int exit_status)
{
	/*
	 * Now that we're running, if a pipe fd was specified, write an exit
	 * status to it to indicate that our parent process can safely detach.
	 * Then proceed to loading the remaining non-built-in modules.
	 */
	if (fd >= 0)
		(void) write(fd, &exit_status, sizeof (exit_status));

	(void) close(fd);

	if ((fd = open("/dev/null", O_RDWR)) >= 0) {
		(void) fcntl(fd, F_DUP2FD, STDIN_FILENO);
		(void) fcntl(fd, F_DUP2FD, STDOUT_FILENO);
		(void) fcntl(fd, F_DUP2FD, STDERR_FILENO);
		(void) close(fd);
	}

	__fini_daemon_priv(PRIV_PROC_FORK, PRIV_PROC_EXEC, PRIV_PROC_SESSION,
	    PRIV_FILE_LINK_ANY, PRIV_PROC_INFO, NULL);
}

/*
 * smbd_service_init
 */
static int
smbd_service_init(void)
{
	int		id_winpipe_door;
	int		id_lmshr_door;
	int		id_srv_door;
	int		rc;
	uint32_t	mode;
	char		resource_domain[SMB_PI_MAX_DOMAIN];
	char		fqdn[MAXHOSTNAMELEN];
	smbd.s_drv_fd = -1;


	if ((mkdir(SMB_DBDIR, 0700) < 0) && (errno != EEXIST)) {
		smbd_report("mkdir %s: %s", SMB_DBDIR, strerror(errno));
		return (1);
	}

	if ((rc = smb_ccache_init(SMB_VARRUN_DIR, SMB_CCACHE_FILE)) != 0) {
		if (rc == -1)
			smbd_report("mkdir %s: %s", SMB_VARRUN_DIR,
			    strerror(errno));
		else
			smbd_report("unable to set KRB5CCNAME");
		return (1);
	}


	(void) oem_language_set("english");

	if (!nt_builtin_init()) {
		smbd_report("out of memory");
		return (1);
	}

	(void) smb_nicmon_start();
	if (dns_msgid_init() != 0) {
		smbd_report("DNS message id initialization failed");
		return (1);
	}

	smbrdr_init();

	if (smb_netbios_start() != 0)
		smbd_report("NetBIOS services failed to start");
	else
		smbd_report("NetBIOS services started");

	if (smb_netlogon_init() != 0) {
		smbd_report("netlogon initialization failed");
		return (1);
	}

	(void) smb_getdomainname(resource_domain, SMB_PI_MAX_DOMAIN);
	(void) utf8_strupr(resource_domain);

	/* Get the ID map client handle */
	if ((rc = smb_idmap_start()) != 0) {
		smbd_report("no idmap handle");
		return (rc);
	}

	mode = smb_config_get_secmode();
	if ((rc = nt_domain_init(resource_domain, mode)) != 0) {
		if (rc == SMB_DOMAIN_NOMACHINE_SID) {
			smbd_report(
			    "no machine SID: check idmap configuration");
			return (rc);
		}
	}

	ads_init();
	if ((rc = mlsvc_init()) != 0) {
		smbd_report("msrpc initialization failed");
		return (rc);
	}

	if (mode == SMB_SECMODE_DOMAIN) {
		if (!smb_match_netlogon_seqnum())
			smb_set_netlogon_cred();
		else
			(void) smbd_locate_dc(resource_domain, "");

		(void) lsa_query_primary_domain_info();
	}

	id_lmshr_door = smb_lmshrd_srv_start();
	if (id_lmshr_door < 0) {
		smbd_report("share initialization failed");
	}

	id_srv_door = smb_door_srv_start();
	if (id_srv_door < 0)
		return (rc);

	if ((rc = smbd_refresh_init()) != 0)
		return (rc);

	if (smb_getfqdomainname(fqdn, MAXHOSTNAMELEN) == 0)
		(void) dyndns_update(fqdn, B_FALSE);

	(void) smbd_localtime_init();

	id_winpipe_door = smb_winpipe_doorsvc_start();
	if (id_winpipe_door < 0) {
		smbd_report("winpipe initialization failed %s",
		    strerror(errno));
		return (rc);
	}

	(void) smb_lgrp_start();

	(void) smb_pwd_init();

	rc = smbd_kernel_bind(id_lmshr_door, id_srv_door, id_winpipe_door);
	if (rc != 0)
		smbd_report("kernel bind error: %s", strerror(errno));

	return (lmshare_start());
}

/*
 * Close the kernel service and shutdown smbd services.
 * This function is registered with atexit(): ensure that anything
 * called from here is safe to be called multiple times.
 */
static void
smbd_service_fini(void)
{
	smb_winpipe_doorsvc_stop();
	nt_builtin_fini();
	smbd_refresh_fini();
	smbd_kernel_unbind();
	smb_door_srv_stop();
	smb_lmshrd_srv_stop();
	lmshare_stop();
	smb_nicmon_stop();
	smb_idmap_stop();
	smb_lgrp_stop();
	smb_ccache_remove(SMB_CCACHE_PATH);
	smb_pwd_fini();

}


/*
 * smbd_refresh_init()
 *
 * SMB service refresh thread initialization.  This thread waits for a
 * refresh event and updates the daemon's view of the configuration
 * before going back to sleep.
 */
static int
smbd_refresh_init()
{
	pthread_attr_t tattr;
	pthread_condattr_t cattr;
	int rc;

	(void) pthread_condattr_init(&cattr);
	(void) pthread_cond_init(&refresh_cond, &cattr);
	(void) pthread_condattr_destroy(&cattr);

	(void) pthread_mutex_init(&refresh_mutex, NULL);

	(void) pthread_attr_init(&tattr);
	(void) pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&refresh_thr, &tattr, smbd_refresh_monitor, 0);
	(void) pthread_attr_destroy(&tattr);
	return (rc);

}

/*
 * smbd_refresh_fini()
 *
 * Stop the refresh thread.
 */
static void
smbd_refresh_fini()
{
	(void) pthread_cancel(refresh_thr);

	(void) pthread_cond_destroy(&refresh_cond);
	(void) pthread_mutex_destroy(&refresh_mutex);
}

/*
 * smbd_refresh_monitor()
 *
 * Wait for a refresh event. When this thread wakes up, update the
 * smbd configuration from the SMF config information then go back to
 * wait for the next refresh.
 */
/*ARGSUSED*/
static void *
smbd_refresh_monitor(void *arg)
{
	int dummy = 0;

	(void) pthread_mutex_lock(&refresh_mutex);
	while (pthread_cond_wait(&refresh_cond, &refresh_mutex) == 0) {
		/*
		 * We've been woken up by a refresh event so go do
		 * what is necessary.
		 */
		ads_refresh();
		smb_ccache_remove(SMB_CCACHE_PATH);
		smb_nicmon_reconfig();
		smb_set_netlogon_cred();
		if (ioctl(smbd.s_drv_fd, SMB_IOC_CONFIG, &dummy) < 0) {
			smbd_report("configuration update ioctl: %s",
			    strerror(errno));
		}
	}
	return (NULL);
}


/*
 * If the door has already been opened by another process (non-zero pid
 * in target), we assume that another smbd is already running.  If there
 * is a race here, it will be caught later when smbsrv is opened because
 * only one process is allowed to open the device at a time.
 */
static int
smbd_already_running(void)
{
	door_info_t info;
	int door;

	if ((door = open(SMB_DR_SVC_NAME, O_RDONLY)) < 0)
		return (0);

	if (door_info(door, &info) < 0)
		return (0);

	if (info.di_target > 0) {
		smbd_report("already running: pid %ld\n", info.di_target);
		(void) close(door);
		return (1);
	}

	(void) close(door);
	return (0);
}

/*
 * smbd_kernel_bind
 */
static int
smbd_kernel_bind(int id_lmshr_door, int id_srv_door, int id_winpipe_door)
{
	smb_io_t	smb_io;
	int		rc;

	bzero(&smb_io, sizeof (smb_io));
	smb_io.sio_version = SMB_IOC_VERSION;

	if (smbd.s_drv_fd != -1)
		(void) close(smbd.s_drv_fd);

	if ((smbd.s_drv_fd = open(DRV_DEVICE_PATH, 0)) < 0) {
		smbd.s_drv_fd = -1;
		return (errno);
	}
	smb_load_kconfig(&smb_io.sio_data.cfg);
	if (ioctl(smbd.s_drv_fd, SMB_IOC_CONFIG, &smb_io) < 0) {
		(void) close(smbd.s_drv_fd);
		smbd.s_drv_fd = -1;
		return (errno);
	}
	smb_io.sio_data.gmtoff = (uint32_t)(-altzone);
	if (ioctl(smbd.s_drv_fd, SMB_IOC_GMTOFF, &smb_io) < 0) {
		(void) close(smbd.s_drv_fd);
		smbd.s_drv_fd = -1;
		return (errno);
	}
	smb_io.sio_data.start.winpipe = id_winpipe_door;
	smb_io.sio_data.start.lmshrd = id_lmshr_door;
	smb_io.sio_data.start.udoor = id_srv_door;
	if (ioctl(smbd.s_drv_fd, SMB_IOC_START, &smb_io) < 0) {
		(void) close(smbd.s_drv_fd);
		smbd.s_drv_fd = -1;
		return (errno);
	}

	rc = pthread_create(&nbt_listener, NULL, smbd_nbt_listener, NULL);
	if (rc == 0) {
		rc = pthread_create(&tcp_listener, NULL, smbd_tcp_listener,
		    NULL);
		if (rc == 0)
			return (0);
	}
	(void) close(smbd.s_drv_fd);
	smbd.s_drv_fd = -1;
	return (rc);
}

/*
 * smbd_kernel_unbind
 */
static void
smbd_kernel_unbind(void)
{
	if (smbd.s_drv_fd != -1) {
		(void) close(smbd.s_drv_fd);
		smbd.s_drv_fd = -1;
	}
}

/*
 * Initialization of the localtime thread.
 * Returns 0 on success, an error number if thread creation fails.
 */

int
smbd_localtime_init(void)
{
	pthread_attr_t tattr;
	int rc;

	(void) pthread_attr_init(&tattr);
	(void) pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
	rc = pthread_create(&localtime_thr, &tattr, smbd_localtime_monitor, 0);
	(void) pthread_attr_destroy(&tattr);
	return (rc);
}

/*
 * Local time thread to kernel land.
 * Send local gmtoff to kernel module one time at startup
 * and each time it changes (up to twice a year).
 * Local gmtoff is checked once every 15 minutes and
 * since some timezones are aligned on half and qtr hour boundaries,
 * once an hour would likely suffice.
 */

/*ARGSUSED*/
static void *
smbd_localtime_monitor(void *arg)
{
	struct tm local_tm;
	time_t secs, gmtoff;
	time_t last_gmtoff = -1;
	int timeout;

	for (;;) {
		gmtoff = -altzone;

		if ((last_gmtoff != gmtoff) && (smbd.s_drv_fd != -1)) {
			if (ioctl(smbd.s_drv_fd, SMB_IOC_GMTOFF, &gmtoff) < 0) {
				smbd_report("localtime ioctl: %s",
				    strerror(errno));
			}
		}

		/*
		 * Align the next iteration on a fifteen minute boundary.
		 */
		secs = time(0);
		(void) localtime_r(&secs, &local_tm);
		timeout = ((15 - (local_tm.tm_min % 15)) * SECSPERMIN);
		(void) sleep(timeout);

		last_gmtoff = gmtoff;
	}

	/*NOTREACHED*/
	return (NULL);
}

static void
smbd_sig_handler(int sigval)
{
	if (smbd.s_sigval == 0)
		smbd.s_sigval = sigval;
}

/*
 * Set up configuration options and parse the command line.
 * This function will determine if we will run as a daemon
 * or in the foreground.
 *
 * Failure to find a uid or gid results in using the default (0).
 */
static int
smbd_setup_options(int argc, char *argv[])
{
	struct passwd *pwd;
	struct group *grp;
	int c;

	if ((pwd = getpwnam("root")) != NULL)
		smbd.s_uid = pwd->pw_uid;

	if ((grp = getgrnam("sys")) != NULL)
		smbd.s_gid = grp->gr_gid;

	smbd.s_fg = smb_config_get_fg_flag();

	while ((c = getopt(argc, argv, ":f")) != -1) {
		switch (c) {
		case 'f':
			smbd.s_fg = 1;
			break;

		case ':':
		case '?':
		default:
			smbd_usage(stderr);
			return (-1);
		}
	}

	return (0);
}

static void
smbd_usage(FILE *fp)
{
	static char *help[] = {
		"-f  run program in foreground"
	};

	int i;

	(void) fprintf(fp, "Usage: %s [-f]\n", smbd.s_pname);

	for (i = 0; i < sizeof (help)/sizeof (help[0]); ++i)
		(void) fprintf(fp, "    %s\n", help[i]);
}

static void
smbd_report(const char *fmt, ...)
{
	char buf[128];
	va_list ap;

	if (fmt == NULL)
		return;

	va_start(ap, fmt);
	(void) vsnprintf(buf, 128, fmt, ap);
	va_end(ap);

	(void) fprintf(stderr, "smbd: %s\n", buf);
}

/*
 * Enable libumem debugging by default on DEBUG builds.
 */
#ifdef DEBUG
const char *
_umem_debug_init(void)
{
	return ("default,verbose"); /* $UMEM_DEBUG setting */
}

const char *
_umem_logging_init(void)
{
	return ("fail,contents"); /* $UMEM_LOGGING setting */
}
#endif
