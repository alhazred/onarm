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

/*	Copyright (c) 1988 AT&T */
/*	All Rights Reserved   */

#pragma ident	"@(#)svc_generic.c	1.42	06/09/11 SMI"

/*
 * svc_generic.c, Server side for RPC.
 *
 */

#include "mt.h"
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <inttypes.h>
#include "rpc_mt.h"
#include <stdio.h>
#include <rpc/rpc.h>
#include <sys/types.h>
#include <errno.h>
#include <syslog.h>
#include <rpc/nettype.h>
#include <malloc.h>
#include <string.h>
#include <stropts.h>
#include <tsol/label.h>
#include <nfs/nfs.h>
#include <nfs/nfs_acl.h>
#include <rpcsvc/mount.h>
#include <rpcsvc/nsm_addr.h>
#include <rpcsvc/rquota.h>
#include <rpcsvc/sm_inter.h>
#include <rpcsvc/nlm_prot.h>

extern int __svc_vc_setflag(SVCXPRT *, int);

extern SVCXPRT *svc_dg_create_private(int, uint_t, uint_t);
extern SVCXPRT *svc_vc_create_private(int, uint_t, uint_t);
extern SVCXPRT *svc_fd_create_private(int, uint_t, uint_t);

extern bool_t __svc_add_to_xlist(SVCXPRT_LIST **, SVCXPRT *, mutex_t *);
extern void __svc_free_xlist(SVCXPRT_LIST **, mutex_t *);

extern bool_t __rpc_try_doors(const char *, bool_t *);

extern int use_portmapper;

/*
 * The highest level interface for server creation.
 * It tries for all the nettokens in that particular class of token
 * and returns the number of handles it can create and/or find.
 *
 * It creates a link list of all the handles it could create.
 * If svc_create() is called multiple times, it uses the handle
 * created earlier instead of creating a new handle every time.
 */

/* VARIABLES PROTECTED BY xprtlist_lock: xprtlist */

SVCXPRT_LIST *_svc_xprtlist = NULL;
extern mutex_t xprtlist_lock;

static SVCXPRT * svc_tli_create_common(int, const struct netconfig *,
    const struct t_bind *, uint_t, uint_t, boolean_t);

boolean_t
is_multilevel(rpcprog_t prognum)
{
	/* This is a list of identified multilevel service provider */
	if ((prognum == MOUNTPROG) || (prognum == NFS_PROGRAM) ||
	    (prognum == NFS_ACL_PROGRAM) || (prognum == NLM_PROG) ||
	    (prognum == NSM_ADDR_PROGRAM) || (prognum == RQUOTAPROG) ||
	    (prognum == SM_PROG))
		return (B_TRUE);

	return (B_FALSE);
}

void
__svc_free_xprtlist(void)
{
	__svc_free_xlist(&_svc_xprtlist, &xprtlist_lock);
}

int
svc_create(void (*dispatch)(), const rpcprog_t prognum, const rpcvers_t versnum,
							const char *nettype)
{
	SVCXPRT_LIST *l;
	int num = 0;
	SVCXPRT *xprt;
	struct netconfig *nconf;
	void *handle;
	bool_t try_others;

	/*
	 * Check if service should register over doors transport.
	 */
	if (__rpc_try_doors(nettype, &try_others)) {
		if (svc_door_create(dispatch, prognum, versnum, 0) == NULL)
			(void) syslog(LOG_ERR,
				"svc_create: could not register over doors");
		else
			num++;
	}
	if (!try_others)
		return (num);
	if ((handle = __rpc_setconf((char *)nettype)) == NULL) {
		(void) syslog(LOG_ERR, "svc_create: unknown protocol");
		return (0);
	}
	while (nconf = __rpc_getconf(handle)) {
		(void) mutex_lock(&xprtlist_lock);
		for (l = _svc_xprtlist; l; l = l->next) {
			if (strcmp(l->xprt->xp_netid, nconf->nc_netid) == 0) {
				/*
				 * Note that if we're using a portmapper
				 * instead of rpcbind then we can't do an
				 * unregister operation here.
				 *
				 * The reason is that the portmapper unset
				 * operation removes all the entries for a
				 * given program/version regardelss of
				 * transport protocol.
				 *
				 * The caller of this routine needs to ensure
				 * that __pmap_unset() has been called for all
				 * program/version service pairs they plan
				 * to support before they start registering
				 * each program/version/protocol triplet.
				 */
				if (!use_portmapper)
					(void) rpcb_unset(prognum,
					    versnum, nconf);
				if (svc_reg(l->xprt, prognum, versnum,
					dispatch, nconf) == FALSE)
					(void) syslog(LOG_ERR,
		"svc_create: could not register prog %d vers %d on %s",
					prognum, versnum, nconf->nc_netid);
				else
					num++;
				break;
			}
		}
		(void) mutex_unlock(&xprtlist_lock);
		if (l == NULL) {
			/* It was not found. Now create a new one */
			xprt = svc_tp_create(dispatch, prognum, versnum, nconf);
			if (xprt) {
				if (!__svc_add_to_xlist(&_svc_xprtlist, xprt,
							&xprtlist_lock)) {
					(void) syslog(LOG_ERR,
						"svc_create: no memory");
					return (0);
				}
				num++;
			}
		}
	}
	__rpc_endconf(handle);
	/*
	 * In case of num == 0; the error messages are generated by the
	 * underlying layers; and hence not needed here.
	 */
	return (num);
}

/*
 * The high level interface to svc_tli_create().
 * It tries to create a server for "nconf" and registers the service
 * with the rpcbind. It calls svc_tli_create();
 */
SVCXPRT *
svc_tp_create(void (*dispatch)(), const rpcprog_t prognum,
			const rpcvers_t versnum, const struct netconfig *nconf)
{
	SVCXPRT *xprt;
	boolean_t anon_mlp = B_FALSE;

	if (nconf == NULL) {
		(void) syslog(LOG_ERR,
	"svc_tp_create: invalid netconfig structure for prog %d vers %d",
				prognum, versnum);
		return (NULL);
	}

	/* Some programs need to allocate MLP for multilevel services */
	if (is_system_labeled() && is_multilevel(prognum))
		anon_mlp = B_TRUE;
	xprt = svc_tli_create_common(RPC_ANYFD, nconf, NULL, 0, 0, anon_mlp);
	if (xprt == NULL)
		return (NULL);

	/*
	 * Note that if we're using a portmapper
	 * instead of rpcbind then we can't do an
	 * unregister operation here.
	 *
	 * The reason is that the portmapper unset
	 * operation removes all the entries for a
	 * given program/version regardelss of
	 * transport protocol.
	 *
	 * The caller of this routine needs to ensure
	 * that __pmap_unset() has been called for all
	 * program/version service pairs they plan
	 * to support before they start registering
	 * each program/version/protocol triplet.
	 */
	if (!use_portmapper)
		(void) rpcb_unset(prognum, versnum, (struct netconfig *)nconf);
	if (svc_reg(xprt, prognum, versnum, dispatch, nconf) == FALSE) {
		(void) syslog(LOG_ERR,
		"svc_tp_create: Could not register prog %d vers %d on %s",
				prognum, versnum, nconf->nc_netid);
		SVC_DESTROY(xprt);
		return (NULL);
	}
	return (xprt);
}

SVCXPRT *
svc_tli_create(const int fd, const struct netconfig *nconf,
    const struct t_bind *bindaddr, const uint_t sendsz, const uint_t recvsz)
{
	return (svc_tli_create_common(fd, nconf, bindaddr, sendsz, recvsz, 0));
}

/*
 * If fd is RPC_ANYFD, then it opens a fd for the given transport
 * provider (nconf cannot be NULL then). If the t_state is T_UNBND and
 * bindaddr is NON-NULL, it performs a t_bind using the bindaddr. For
 * NULL bindadr and Connection oriented transports, the value of qlen
 * is set arbitrarily.
 *
 * If sendsz or recvsz are zero, their default values are chosen.
 */
SVCXPRT *
svc_tli_create_common(const int ofd, const struct netconfig *nconf,
	const struct t_bind *bindaddr, const uint_t sendsz,
	const uint_t recvsz, boolean_t mlp_flag)
{
	SVCXPRT *xprt = NULL;		/* service handle */
	struct t_info tinfo;		/* transport info */
	struct t_bind *tres = NULL;	/* bind info */
	bool_t madefd = FALSE;		/* whether fd opened here  */
	int state;			/* state of the transport provider */
	int fd = ofd;

	if (fd == RPC_ANYFD) {
		if (nconf == NULL) {
			(void) syslog(LOG_ERR,
			    "svc_tli_create: invalid netconfig");
			return (NULL);
		}
		fd = t_open(nconf->nc_device, O_RDWR, &tinfo);
		if (fd == -1) {
			char errorstr[100];

			__tli_sys_strerror(errorstr, sizeof (errorstr),
					t_errno, errno);
			(void) syslog(LOG_ERR,
			"svc_tli_create: could not open connection for %s: %s",
					nconf->nc_netid, errorstr);
			return (NULL);
		}
		madefd = TRUE;
		state = T_UNBND;
	} else {
		/*
		 * It is an open descriptor. Sync it & get the transport info.
		 */
		if ((state = t_sync(fd)) == -1) {
			char errorstr[100];

			__tli_sys_strerror(errorstr, sizeof (errorstr),
					t_errno, errno);
			(void) syslog(LOG_ERR,
		"svc_tli_create: could not do t_sync: %s",
					errorstr);
			return (NULL);
		}
		if (t_getinfo(fd, &tinfo) == -1) {
			char errorstr[100];

			__tli_sys_strerror(errorstr, sizeof (errorstr),
					t_errno, errno);
			(void) syslog(LOG_ERR,
		"svc_tli_create: could not get transport information: %s",
				    errorstr);
			return (NULL);
		}
		/* Enable options of returning the ip's for udp */
		if (nconf) {
		    int ret = 0;
		    if (strcmp(nconf->nc_netid, "udp6") == 0) {
			ret = __rpc_tli_set_options(fd, IPPROTO_IPV6,
					IPV6_RECVPKTINFO, 1);
			if (ret < 0) {
			    char errorstr[100];

			    __tli_sys_strerror(errorstr, sizeof (errorstr),
					t_errno, errno);
			    (void) syslog(LOG_ERR,
				"svc_tli_create: IPV6_RECVPKTINFO(1): %s",
					errorstr);
			    return (NULL);
			}
		    } else if (strcmp(nconf->nc_netid, "udp") == 0) {
			ret = __rpc_tli_set_options(fd, IPPROTO_IP,
					IP_RECVDSTADDR, 1);
			if (ret < 0) {
			    char errorstr[100];

			    __tli_sys_strerror(errorstr, sizeof (errorstr),
					t_errno, errno);
			    (void) syslog(LOG_ERR,
				"svc_tli_create: IP_RECVDSTADDR(1): %s",
					errorstr);
			    return (NULL);
			}
		    }
		}
	}

	/*
	 * If the fd is unbound, try to bind it.
	 * In any case, try to get its bound info in tres
	 */
/* LINTED pointer alignment */
	tres = (struct t_bind *)t_alloc(fd, T_BIND, T_ADDR);
	if (tres == NULL) {
		(void) syslog(LOG_ERR, "svc_tli_create: No memory!");
		goto freedata;
	}

	switch (state) {
		bool_t tcp, exclbind;
	case T_UNBND:
		/* If this is a labeled system, then ask for an MLP */
		if (is_system_labeled() &&
		    (strcmp(nconf->nc_protofmly, NC_INET) == 0 ||
		    strcmp(nconf->nc_protofmly, NC_INET6) == 0)) {
			(void) __rpc_tli_set_options(fd, SOL_SOCKET,
			    SO_RECVUCRED, 1);
			if (mlp_flag)
				(void) __rpc_tli_set_options(fd, SOL_SOCKET,
				    SO_ANON_MLP, 1);
		}

		/*
		 * SO_EXCLBIND has the following properties
		 *    - an fd bound to port P via IPv4 will prevent an IPv6
		 *    bind to port P (and vice versa)
		 *    - an fd bound to a wildcard IP address for port P will
		 *    prevent a more specific IP address bind to port P
		 *    (see {tcp,udp}.c for details)
		 *
		 * We use the latter property to prevent hijacking of RPC
		 * services that reside at non-privileged ports.
		 */
		tcp = nconf ? (strcmp(nconf->nc_proto, NC_TCP) == 0) : 0;
		if (nconf &&
		    (tcp || (strcmp(nconf->nc_proto, NC_UDP) == 0)) &&
		    rpc_control(__RPC_SVC_EXCLBIND_GET, &exclbind)) {
			if (exclbind) {
				if (__rpc_tli_set_options(fd, SOL_SOCKET,
				    SO_EXCLBIND, 1) < 0) {
					syslog(LOG_ERR,
			    "svc_tli_create: can't set EXCLBIND [netid='%s']",
					    nconf->nc_netid);
					goto freedata;
				}
			}
		}
		if (bindaddr) {
			if (t_bind(fd, (struct t_bind *)bindaddr,
								tres) == -1) {
				char errorstr[100];

				__tli_sys_strerror(errorstr, sizeof (errorstr),
						t_errno, errno);
				(void) syslog(LOG_ERR,
					"svc_tli_create: could not bind: %s",
					    errorstr);
				goto freedata;
			}
			/*
			 * Should compare the addresses only if addr.len
			 * was non-zero
			 */
			if (bindaddr->addr.len &&
				(memcmp(bindaddr->addr.buf, tres->addr.buf,
					(int)tres->addr.len) != 0)) {
				(void) syslog(LOG_ERR,
		"svc_tli_create: could not bind to requested address: %s",
						"address mismatch");
				goto freedata;
			}
		} else {
			tres->qlen = 64; /* Chosen Arbitrarily */
			tres->addr.len = 0;
			if (t_bind(fd, tres, tres) == -1) {
				char errorstr[100];

				__tli_sys_strerror(errorstr, sizeof (errorstr),
						t_errno, errno);
				(void) syslog(LOG_ERR,
					"svc_tli_create: could not bind: %s",
						errorstr);
				goto freedata;
			}
		}

		/* Enable options of returning the ip's for udp */
		if (nconf) {
		    int ret = 0;
		    if (strcmp(nconf->nc_netid, "udp6") == 0) {
			ret = __rpc_tli_set_options(fd, IPPROTO_IPV6,
					IPV6_RECVPKTINFO, 1);
			if (ret < 0) {
			    char errorstr[100];

			    __tli_sys_strerror(errorstr, sizeof (errorstr),
					t_errno, errno);
			    (void) syslog(LOG_ERR,
				"svc_tli_create: IPV6_RECVPKTINFO(2): %s",
					errorstr);
			    goto freedata;
			}
		    } else if (strcmp(nconf->nc_netid, "udp") == 0) {
			ret = __rpc_tli_set_options(fd, IPPROTO_IP,
					IP_RECVDSTADDR, 1);
			if (ret < 0) {
			    char errorstr[100];

			    __tli_sys_strerror(errorstr, sizeof (errorstr),
					t_errno, errno);
			    (void) syslog(LOG_ERR,
				"svc_tli_create: IP_RECVDSTADDR(2): %s",
					errorstr);
			    goto freedata;
			}
		    }
		}
		break;

	case T_IDLE:
		if (bindaddr) {
			/* Copy the entire stuff in tres */
			if (tres->addr.maxlen < bindaddr->addr.len) {
				(void) syslog(LOG_ERR,
				"svc_tli_create: illegal netbuf length");
				goto freedata;
			}
			tres->addr.len = bindaddr->addr.len;
			(void) memcpy(tres->addr.buf, bindaddr->addr.buf,
					(int)tres->addr.len);
		} else
			if (t_getname(fd, &(tres->addr), LOCALNAME) == -1)
				tres->addr.len = 0;
		break;
	case T_INREL:
		(void) t_rcvrel(fd);
		(void) t_sndrel(fd);
		(void) syslog(LOG_ERR,
			"svc_tli_create: other side wants to\
release connection");
		goto freedata;

	case T_INCON:
		/* Do nothing here. Assume this is handled in rendezvous */
		break;
	case T_DATAXFER:
		/*
		 * This takes care of the case where a fd
		 * is passed on which a connection has already
		 * been accepted.
		 */
		if (t_getname(fd, &(tres->addr), LOCALNAME) == -1)
			tres->addr.len = 0;
		break;
	default:
		(void) syslog(LOG_ERR,
		    "svc_tli_create: connection in a wierd state (%d)", state);
		goto freedata;
	}

	/*
	 * call transport specific function.
	 */
	switch (tinfo.servtype) {
		case T_COTS_ORD:
		case T_COTS:
			if (state == T_DATAXFER)
				xprt = svc_fd_create_private(fd, sendsz,
						recvsz);
			else
				xprt = svc_vc_create_private(fd, sendsz,
						recvsz);
			if (!nconf || !xprt)
				break;
			if ((tinfo.servtype == T_COTS_ORD) &&
				(state != T_DATAXFER) &&
				(strcmp(nconf->nc_protofmly, "inet") == 0))
				(void) __svc_vc_setflag(xprt, TRUE);
			break;
		case T_CLTS:
			xprt = svc_dg_create_private(fd, sendsz, recvsz);
			break;
		default:
			(void) syslog(LOG_ERR,
			    "svc_tli_create: bad service type");
			goto freedata;
	}
	if (xprt == NULL)
		/*
		 * The error messages here are spitted out by the lower layers:
		 * svc_vc_create(), svc_fd_create() and svc_dg_create().
		 */
		goto freedata;

	/* fill in the other xprt information */

	/* Assign the local bind address */
	xprt->xp_ltaddr = tres->addr;
	/* Fill in type of service */
	xprt->xp_type = tinfo.servtype;
	tres->addr.buf = NULL;
	(void) t_free((char *)tres, T_BIND);
	tres = NULL;

	xprt->xp_rtaddr.len = 0;
	xprt->xp_rtaddr.maxlen = __rpc_get_a_size(tinfo.addr);

	/* Allocate space for the remote bind info */
	if ((xprt->xp_rtaddr.buf = malloc(xprt->xp_rtaddr.maxlen)) == NULL) {
		(void) syslog(LOG_ERR, "svc_tli_create: No memory!");
		goto freedata;
	}

	if (nconf) {
		xprt->xp_netid = strdup(nconf->nc_netid);
		if (xprt->xp_netid == NULL) {
			if (xprt->xp_rtaddr.buf)
				free(xprt->xp_rtaddr.buf);
			syslog(LOG_ERR, "svc_tli_create: strdup failed!");
			goto freedata;
		}
		xprt->xp_tp = strdup(nconf->nc_device);
		if (xprt->xp_tp == NULL) {
			if (xprt->xp_rtaddr.buf)
				free(xprt->xp_rtaddr.buf);
			if (xprt->xp_netid)
				free(xprt->xp_netid);
			syslog(LOG_ERR, "svc_tli_create: strdup failed!");
			goto freedata;
		}
	}

/*
 *	if (madefd && (tinfo.servtype == T_CLTS))
 *		(void) ioctl(fd, I_POP, NULL);
 */
	xprt_register(xprt);
	return (xprt);

freedata:
	if (madefd)
		(void) t_close(fd);
	if (tres)
		(void) t_free((char *)tres, T_BIND);
	if (xprt) {
		if (!madefd) /* so that svc_destroy doesnt close fd */
			xprt->xp_fd = RPC_ANYFD;
		SVC_DESTROY(xprt);
	}
	return (NULL);
}
