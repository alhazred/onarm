/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: smb_rq.c,v 1.29 2005/02/11 01:44:17 lindak Exp $
 */
/*
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#pragma ident	"@(#)smb_rq.c	1.1	08/02/13 SMI"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <sys/cmn_err.h>
#include <sys/sdt.h>

#ifdef APPLE
#include <sys/smb_apple.h>
#else
#include <netsmb/smb_osdep.h>
#endif

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_tran.h>
#include <netsmb/smb_rq.h>

static int  smb_rq_reply(struct smb_rq *rqp);
static int  smb_rq_enqueue(struct smb_rq *rqp);
static int  smb_rq_getenv(struct smb_connobj *layer,
		struct smb_vc **vcpp, struct smb_share **sspp);
static int  smb_rq_new(struct smb_rq *rqp, uchar_t cmd);
static int  smb_t2_reply(struct smb_t2rq *t2p);
static int  smb_nt_reply(struct smb_ntrq *ntp);



int
smb_rq_alloc(struct smb_connobj *layer, uchar_t cmd, struct smb_cred *scred,
	struct smb_rq **rqpp)
{
	struct smb_rq *rqp;
	int error;

	rqp = (struct smb_rq *)kmem_alloc(sizeof (struct smb_rq), KM_SLEEP);
	if (rqp == NULL)
		return (ENOMEM);
	error = smb_rq_init(rqp, layer, cmd, scred);
	if (error) {
		smb_rq_done(rqp);
		return (error);
	}
	rqp->sr_flags |= SMBR_ALLOCED;
	*rqpp = rqp;
	return (0);
}


int
smb_rq_init(struct smb_rq *rqp, struct smb_connobj *layer, uchar_t cmd,
	struct smb_cred *scred)
{
	int error;

	bzero(rqp, sizeof (*rqp));
	mutex_init(&rqp->sr_lock, NULL,  MUTEX_DRIVER, NULL);
	cv_init(&rqp->sr_cond, NULL, CV_DEFAULT, NULL);

	error = smb_rq_getenv(layer, &rqp->sr_vc, &rqp->sr_share);
	if (error)
		return (error);

	rqp->sr_rexmit = SMBMAXRESTARTS;
	rqp->sr_cred = scred;	/* XXX no ref hold */
	rqp->sr_mid = smb_vc_nextmid(rqp->sr_vc);
	error = smb_rq_new(rqp, cmd);
	if (!error) {
		rqp->sr_flags |= SMBR_VCREF;
		smb_vc_hold(rqp->sr_vc);
	}
	return (error);
}

static int
smb_rq_new(struct smb_rq *rqp, uchar_t cmd)
{
	struct smb_vc *vcp = rqp->sr_vc;
	struct mbchain *mbp = &rqp->sr_rq;
	int error;
	static char tzero[12];
	caddr_t ptr;
	pid_t   pid;

	ASSERT(rqp != NULL);
	ASSERT(rqp->sr_cred != NULL);
	pid = rqp->sr_cred->vc_pid;
	rqp->sr_sendcnt = 0;
	rqp->sr_cmd = cmd;
	mb_done(mbp);
	md_done(&rqp->sr_rp);
	error = mb_init(mbp);
	if (error)
		return (error);
	mb_put_mem(mbp, SMB_SIGNATURE, SMB_SIGLEN, MB_MSYSTEM);
	mb_put_uint8(mbp, cmd);
	mb_put_uint32le(mbp, 0);
	mb_put_uint8(mbp, vcp->vc_hflags);
	if (cmd == SMB_COM_TRANSACTION || cmd == SMB_COM_TRANSACTION_SECONDARY)
		mb_put_uint16le(mbp, (vcp->vc_hflags2 & ~SMB_FLAGS2_UNICODE));
	else
		mb_put_uint16le(mbp, vcp->vc_hflags2);
	mb_put_mem(mbp, tzero, 12, MB_MSYSTEM);
	ptr = mb_reserve(mbp, sizeof (u_int16_t));
	/*LINTED*/
	ASSERT(ptr == (caddr_t)((u_int16_t *)ptr));
	/*LINTED*/
	rqp->sr_rqtid = (u_int16_t *)ptr;
	mb_put_uint16le(mbp, (u_int16_t)(pid));
	ptr = mb_reserve(mbp, sizeof (u_int16_t));
	/*LINTED*/
	ASSERT(ptr == (caddr_t)((u_int16_t *)ptr));
	/*LINTED*/
	rqp->sr_rquid = (u_int16_t *)ptr;
	mb_put_uint16le(mbp, rqp->sr_mid);
	return (0);
}

void
smb_rq_done(struct smb_rq *rqp)
{
	/* No locks.  Last ref. here. */
	if (rqp->sr_flags & SMBR_VCREF) {
		rqp->sr_flags &= ~SMBR_VCREF;
		smb_vc_rele(rqp->sr_vc);
	}
	mb_done(&rqp->sr_rq);
	md_done(&rqp->sr_rp);
	mutex_destroy(&rqp->sr_lock);
	cv_destroy(&rqp->sr_cond);
	if (rqp->sr_flags & SMBR_ALLOCED)
		kmem_free(rqp, sizeof (*rqp));
}

/*
 * Simple request-reply exchange
 */
int
smb_rq_simple_timed(struct smb_rq *rqp, int timeout)
{
	int error = EINVAL;

	for (; ; ) {
		/*
		 * Don't send any new requests if force unmount is underway.
		 * This check was moved into smb_rq_enqueue.
		 */
		rqp->sr_flags &= ~SMBR_RESTART;
		rqp->sr_timo = timeout;	/* in seconds */
		rqp->sr_state = SMBRQ_NOTSENT;
		error = smb_rq_enqueue(rqp);
		if (error) {
			break;
		}
		error = smb_rq_reply(rqp);
		if (!error)
			break;
		if ((rqp->sr_flags & (SMBR_RESTART | SMBR_NORESTART)) !=
		    SMBR_RESTART)
			break;
		if (rqp->sr_rexmit <= 0)
			break;
		SMBRQ_LOCK(rqp);
		if (rqp->sr_share && rqp->sr_share->ss_mount) {
			cv_timedwait(&rqp->sr_cond, &(rqp)->sr_lock,
			    lbolt + (hz * SMB_RCNDELAY));

		} else {
			delay(lbolt + (hz * SMB_RCNDELAY));
		}
		SMBRQ_UNLOCK(rqp);
		rqp->sr_rexmit--;
#ifdef XXX
		timeout *= 2;
#endif
	}
	return (error);
}


int
smb_rq_simple(struct smb_rq *rqp)
{
	return (smb_rq_simple_timed(rqp, smb_timo_default));
}

static int
smb_rq_enqueue(struct smb_rq *rqp)
{
	struct smb_vc *vcp = rqp->sr_vc;
	struct smb_share *ssp = rqp->sr_share;
	int error = 0;

	/*
	 * Unfortunate special case needed for
	 * tree disconnect, which needs sr_share
	 * but should skip the reconnect check.
	 */
	if (rqp->sr_cmd == SMB_COM_TREE_DISCONNECT)
		ssp = NULL;

	/*
	 * If this is an "internal" request, bypass any
	 * wait for connection state changes, etc.
	 * This request is making those changes.
	 */
	if (rqp->sr_flags & SMBR_INTERNAL) {
		ASSERT(ssp == NULL);
		goto just_doit;
	}

	/*
	 * Wait for VC reconnect to finish...
	 * XXX: Deal with reconnect later.
	 * Just bail out for now.
	 *
	 * MacOS might check vfs_isforce() here.
	 */
	if (vcp->vc_state != SMBIOD_ST_VCACTIVE) {
		SMBSDEBUG("bad vc_state=%d\n", vcp->vc_state);
		return (ENOTCONN);
	}

	/*
	 * If this request has a "share" object:
	 * 1: Deny access if share is _GONE (unmounted)
	 * 2: Wait for state changes in that object,
	 *    Initiate share (re)connect if needed.
	 * XXX: Not really doing 2 yet.
	 */
	if (ssp) {
		if (ssp->ss_flags & SMBS_GONE)
			return (ENOTCONN);
		SMB_SS_LOCK(ssp);
		if (!smb_share_valid(ssp)) {
			error = smb_share_tcon(ssp);
		}
		SMB_SS_UNLOCK(ssp);
	}

	if (!error) {
	just_doit:
		error = smb_iod_addrq(rqp);
	}

	return (error);
}

/*
 * Mark location of the word count, which is filled in later by
 * smb_rw_wend().  Also initialize the counter that it uses
 * to figure out what value to fill in.
 *
 * Note that the word count happens to be 8-bit.
 */
void
smb_rq_wstart(struct smb_rq *rqp)
{
	rqp->sr_wcount = mb_reserve(&rqp->sr_rq, sizeof (uint8_t));
	rqp->sr_rq.mb_count = 0;
}

void
smb_rq_wend(struct smb_rq *rqp)
{
	uint_t wcnt;

	if (rqp->sr_wcount == NULL) {
		SMBSDEBUG("no wcount\n");
		return;
	}
	wcnt = rqp->sr_rq.mb_count;
	if (wcnt > 0x1ff)
		SMBSDEBUG("word count too large (%d)\n", wcnt);
	if (wcnt & 1)
		SMBSDEBUG("odd word count\n");
	/* Fill in the word count (8-bits) */
	*rqp->sr_wcount = (wcnt >> 1);
}

/*
 * Mark location of the byte count, which is filled in later by
 * smb_rw_bend().  Also initialize the counter that it uses
 * to figure out what value to fill in.
 *
 * Note that the byte count happens to be 16-bit.
 */
void
smb_rq_bstart(struct smb_rq *rqp)
{
	rqp->sr_bcount = mb_reserve(&rqp->sr_rq, sizeof (uint16_t));
	rqp->sr_rq.mb_count = 0;
}

void
smb_rq_bend(struct smb_rq *rqp)
{
	uint_t bcnt;

	if (rqp->sr_bcount == NULL) {
		SMBSDEBUG("no bcount\n");
		return;
	}
	bcnt = rqp->sr_rq.mb_count;
	if (bcnt > 0xffff)
		SMBSDEBUG("byte count too large (%d)\n", bcnt);
	/*
	 * Fill in the byte count (16-bits)
	 * The pointer is char * type due to
	 * typical off-by-one alignment.
	 */
	rqp->sr_bcount[0] = bcnt & 0xFF;
	rqp->sr_bcount[1] = (bcnt >> 8);
}

int
smb_rq_intr(struct smb_rq *rqp)
{
	if (rqp->sr_flags & SMBR_INTR)
		return (EINTR);

	return (0);
#ifdef APPLE
	return (smb_sigintr(rqp->sr_cred->scr_vfsctx));
#endif
}

int
smb_rq_getrequest(struct smb_rq *rqp, struct mbchain **mbpp)
{
	*mbpp = &rqp->sr_rq;
	return (0);
}

int
smb_rq_getreply(struct smb_rq *rqp, struct mdchain **mbpp)
{
	*mbpp = &rqp->sr_rp;
	return (0);
}

static int
smb_rq_getenv(struct smb_connobj *co,
	struct smb_vc **vcpp, struct smb_share **sspp)
{
	struct smb_vc *vcp = NULL;
	struct smb_share *ssp = NULL;
	int error = 0;

	if (co->co_flags & SMBO_GONE) {
		SMBSDEBUG("zombie CO\n");
		error = EINVAL;
		goto out;
	}

	switch (co->co_level) {
	case SMBL_VC:
		vcp = CPTOVC(co);
		if (co->co_parent == NULL) {
			SMBSDEBUG("zombie VC %s\n", vcp->vc_srvname);
			error = EINVAL;
			break;
		}
		break;

	case SMBL_SHARE:
		ssp = CPTOSS(co);
		if (co->co_parent == NULL) {
			SMBSDEBUG("zombie share %s\n", ssp->ss_name);
			error = EINVAL;
			break;
		}
		error = smb_rq_getenv(co->co_parent, &vcp, NULL);
		break;
	default:
		SMBSDEBUG("invalid level %d passed\n", co->co_level);
		error = EINVAL;
	}

out:
	if (!error) {
		if (vcpp)
			*vcpp = vcp;
		if (sspp)
			*sspp = ssp;
	}

	return (error);
}

/*
 * Wait for reply on the request
 */
static int
smb_rq_reply(struct smb_rq *rqp)
{
	struct mdchain *mdp = &rqp->sr_rp;
	u_int32_t tdw;
	u_int8_t tb;
	int error, rperror = 0;

	if (rqp->sr_timo == SMBNOREPLYWAIT)
		return (smb_iod_removerq(rqp));

	error = smb_iod_waitrq(rqp);
	if (error)
		return (error);
	error = md_get_uint32(mdp, &tdw);
	if (error)
		return (error);
	error = md_get_uint8(mdp, &tb);
	error = md_get_uint32le(mdp, &rqp->sr_error);
	error = md_get_uint8(mdp, &rqp->sr_rpflags);
	error = md_get_uint16le(mdp, &rqp->sr_rpflags2);
	if (rqp->sr_rpflags2 & SMB_FLAGS2_ERR_STATUS) {
		/*
		 * Do a special check for STATUS_BUFFER_OVERFLOW;
		 * it's not an error.
		 */
		if (rqp->sr_error == NT_STATUS_BUFFER_OVERFLOW) {
			/*
			 * Don't report it as an error to our caller;
			 * they can look at rqp->sr_error if they
			 * need to know whether we got a
			 * STATUS_BUFFER_OVERFLOW.
			 * XXX - should we do that for all errors
			 * where (error & 0xC0000000) is 0x80000000,
			 * i.e. all warnings?
			 */
			rperror = 0;
		} else
			rperror = smb_maperr32(rqp->sr_error);
	} else {
		rqp->sr_errclass = rqp->sr_error & 0xff;
		rqp->sr_serror = rqp->sr_error >> 16;
		rperror = smb_maperror(rqp->sr_errclass, rqp->sr_serror);
	}
	if (rperror == EMOREDATA) {
		rperror = E2BIG;
		rqp->sr_flags |= SMBR_MOREDATA;
	} else
		rqp->sr_flags &= ~SMBR_MOREDATA;

	error = md_get_uint32(mdp, &tdw);
	error = md_get_uint32(mdp, &tdw);
	error = md_get_uint32(mdp, &tdw);

	error = md_get_uint16le(mdp, &rqp->sr_rptid);
	error = md_get_uint16le(mdp, &rqp->sr_rppid);
	error = md_get_uint16le(mdp, &rqp->sr_rpuid);
	error = md_get_uint16le(mdp, &rqp->sr_rpmid);

	SMBSDEBUG("M:%04x, P:%04x, U:%04x, T:%04x, E: %d:%d\n",
	    rqp->sr_rpmid, rqp->sr_rppid, rqp->sr_rpuid, rqp->sr_rptid,
	    rqp->sr_errclass, rqp->sr_serror);

	return ((error) ? error : rperror);
}


#define	ALIGN4(a)	(((a) + 3) & ~3)

/*
 * TRANS2 request implementation
 * TRANS implementation is in the "t2" routines
 * NT_TRANSACTION implementation is the separate "nt" stuff
 */
int
smb_t2_alloc(struct smb_connobj *layer, ushort_t setup, struct smb_cred *scred,
	struct smb_t2rq **t2pp)
{
	struct smb_t2rq *t2p;
	int error;

	t2p = (struct smb_t2rq *)kmem_alloc(sizeof (*t2p), KM_SLEEP);
	if (t2p == NULL)
		return (ENOMEM);
	error = smb_t2_init(t2p, layer, &setup, 1, scred);
	mutex_init(&t2p->t2_lock, NULL, MUTEX_DRIVER, NULL);
	cv_init(&t2p->t2_cond, NULL, CV_DEFAULT, NULL);
	t2p->t2_flags |= SMBT2_ALLOCED;
	if (error) {
		smb_t2_done(t2p);
		return (error);
	}
	*t2pp = t2p;
	return (0);
}

int
smb_nt_alloc(struct smb_connobj *layer, ushort_t fn, struct smb_cred *scred,
	struct smb_ntrq **ntpp)
{
	struct smb_ntrq *ntp;
	int error;

	ntp = (struct smb_ntrq *)kmem_alloc(sizeof (*ntp), KM_SLEEP);
	if (ntp == NULL)
		return (ENOMEM);
	error = smb_nt_init(ntp, layer, fn, scred);
	mutex_init(&ntp->nt_lock, NULL, MUTEX_DRIVER, NULL);
	cv_init(&ntp->nt_cond, NULL, CV_DEFAULT, NULL);
	ntp->nt_flags |= SMBT2_ALLOCED;
	if (error) {
		smb_nt_done(ntp);
		return (error);
	}
	*ntpp = ntp;
	return (0);
}

int
smb_t2_init(struct smb_t2rq *t2p, struct smb_connobj *source, ushort_t *setup,
	int setupcnt, struct smb_cred *scred)
{
	int i;
	int error;

	bzero(t2p, sizeof (*t2p));
	t2p->t2_source = source;
	t2p->t2_setupcount = (u_int16_t)setupcnt;
	t2p->t2_setupdata = t2p->t2_setup;
	for (i = 0; i < setupcnt; i++)
		t2p->t2_setup[i] = setup[i];
	t2p->t2_fid = 0xffff;
	t2p->t2_cred = scred;
	t2p->t2_share = (source->co_level == SMBL_SHARE ?
	    CPTOSS(source) : NULL); /* for smb up/down */
	error = smb_rq_getenv(source, &t2p->t2_vc, NULL);
	if (error)
		return (error);
	return (0);
}

int
smb_nt_init(struct smb_ntrq *ntp, struct smb_connobj *source, ushort_t fn,
	struct smb_cred *scred)
{
	int error;

	bzero(ntp, sizeof (*ntp));
	ntp->nt_source = source;
	ntp->nt_function = fn;
	ntp->nt_cred = scred;
	ntp->nt_share = (source->co_level == SMBL_SHARE ?
	    CPTOSS(source) : NULL); /* for smb up/down */
	error = smb_rq_getenv(source, &ntp->nt_vc, NULL);
	if (error)
		return (error);
	return (0);
}

void
smb_t2_done(struct smb_t2rq *t2p)
{
	mb_done(&t2p->t2_tparam);
	mb_done(&t2p->t2_tdata);
	md_done(&t2p->t2_rparam);
	md_done(&t2p->t2_rdata);
	mutex_destroy(&t2p->t2_lock);
	cv_destroy(&t2p->t2_cond);
#ifdef NOTYETRESOLVED
	if (t2p->t2_flags & SMBT2_ALLOCED)
		kmem_free(t2p, sizeof (*t2p));
#endif
	if (t2p) {
		kmem_free(t2p, sizeof (*t2p));
	}
}

u_int32_t
smb_t2_err(struct smb_t2rq *t2p)
{
	/* mask off "severity" and the "component"  bit */
	return (t2p->t2_sr_error & ~(0xe0000000));
}

void
smb_nt_done(struct smb_ntrq *ntp)
{
	mb_done(&ntp->nt_tsetup);
	mb_done(&ntp->nt_tparam);
	mb_done(&ntp->nt_tdata);
	md_done(&ntp->nt_rparam);
	md_done(&ntp->nt_rdata);
	cv_destroy(&ntp->nt_cond);
	mutex_destroy(&ntp->nt_lock);
	if (ntp->nt_flags & SMBT2_ALLOCED)
		kmem_free(ntp, sizeof (*ntp));
}

/*
 * Extract data [offset,count] from mtop and add to mdp.
 */
static int
smb_t2_placedata(mblk_t *mtop, u_int16_t offset, u_int16_t count,
	struct mdchain *mdp)
{
	mblk_t *n;

	n = m_copym(mtop, offset, count, M_WAITOK);
	if (n == NULL)
		return (EBADRPC);

	if (mdp->md_top == NULL) {
		md_initm(mdp, n);
	} else
		m_cat(mdp->md_top, n);

	return (0);
}

static int
smb_t2_reply(struct smb_t2rq *t2p)
{
	struct mdchain *mdp;
	struct smb_rq *rqp = t2p->t2_rq;
	int error, error2, totpgot, totdgot;
	u_int16_t totpcount, totdcount, pcount, poff, doff, pdisp, ddisp;
	u_int16_t tmp, bc, dcount;
	u_int8_t wc;

	t2p->t2_flags &= ~SMBT2_MOREDATA;

	error = smb_rq_reply(rqp);
	if (rqp->sr_flags & SMBR_MOREDATA)
		t2p->t2_flags |= SMBT2_MOREDATA;
	t2p->t2_sr_errclass = rqp->sr_errclass;
	t2p->t2_sr_serror = rqp->sr_serror;
	t2p->t2_sr_error = rqp->sr_error;
	t2p->t2_sr_rpflags2 = rqp->sr_rpflags2;
	if (error && !(rqp->sr_flags & SMBR_MOREDATA))
		return (error);
	/*
	 * Now we have to get all subseqent responses, if any.
	 * The CIFS specification says that they can be misordered,
	 * which is weird.
	 * TODO: timo
	 */
	totpgot = totdgot = 0;
	totpcount = totdcount = 0xffff;
	mdp = &rqp->sr_rp;
	for (;;) {
		DTRACE_PROBE2(smb_trans_reply,
		    (smb_rq_t *), rqp, (mblk_t *), mdp->md_top);
		m_dumpm(mdp->md_top);

		if ((error2 = md_get_uint8(mdp, &wc)) != 0)
			break;
		if (wc < 10) {
			error2 = ENOENT;
			break;
		}
		if ((error2 = md_get_uint16le(mdp, &tmp)) != 0)
			break;
		if (totpcount > tmp)
			totpcount = tmp;
		if ((error2 = md_get_uint16le(mdp, &tmp)) != 0)
			break;
		if (totdcount > tmp)
			totdcount = tmp;
		if ((error2 = md_get_uint16le(mdp, &tmp)) != 0 || /* reserved */
		    (error2 = md_get_uint16le(mdp, &pcount)) != 0 ||
		    (error2 = md_get_uint16le(mdp, &poff)) != 0 ||
		    (error2 = md_get_uint16le(mdp, &pdisp)) != 0)
			break;
		if (pcount != 0 && pdisp != totpgot) {
			SMBSDEBUG("Can't handle misordered parameters %d:%d\n",
			    pdisp, totpgot);
			error2 = EINVAL;
			break;
		}
		if ((error2 = md_get_uint16le(mdp, &dcount)) != 0 ||
		    (error2 = md_get_uint16le(mdp, &doff)) != 0 ||
		    (error2 = md_get_uint16le(mdp, &ddisp)) != 0)
			break;
		if (dcount != 0 && ddisp != totdgot) {
			SMBSDEBUG("Can't handle misordered data: dcount %d\n",
			    dcount);
			error2 = EINVAL;
			break;
		}

		/* XXX: Skip setup words?  We don't save them? */
		md_get_uint8(mdp, &wc);  /* SetupCount */
		md_get_uint8(mdp, NULL); /* Reserved2 */
		tmp = wc;
		while (tmp--)
			md_get_uint16(mdp, NULL);

		if ((error2 = md_get_uint16le(mdp, &bc)) != 0)
			break;

		/*
		 * There are pad bytes here, and the poff value
		 * indicates where the next data are found.
		 * No need to guess at the padding size.
		 */
		if (pcount) {
			error2 = smb_t2_placedata(mdp->md_top, poff,
			    pcount, &t2p->t2_rparam);
			if (error2)
				break;
		}
		totpgot += pcount;

		if (dcount) {
			error2 = smb_t2_placedata(mdp->md_top, doff,
			    dcount, &t2p->t2_rdata);
			if (error2)
				break;
		}
		totdgot += dcount;

		if (totpgot >= totpcount && totdgot >= totdcount) {
			error2 = 0;
			t2p->t2_flags |= SMBT2_ALLRECV;
			break;
		}
		/*
		 * We're done with this reply, look for the next one.
		 */
		SMBRQ_LOCK(rqp);
		md_next_record(&rqp->sr_rp);
		SMBRQ_UNLOCK(rqp);
		error2 = smb_rq_reply(rqp);
		if (rqp->sr_flags & SMBR_MOREDATA)
			t2p->t2_flags |= SMBT2_MOREDATA;
		if (!error2)
			continue;
		t2p->t2_sr_errclass = rqp->sr_errclass;
		t2p->t2_sr_serror = rqp->sr_serror;
		t2p->t2_sr_error = rqp->sr_error;
		t2p->t2_sr_rpflags2 = rqp->sr_rpflags2;
		error = error2;
		if (!(rqp->sr_flags & SMBR_MOREDATA))
			break;
	}
	return (error ? error : error2);
}

static int
smb_nt_reply(struct smb_ntrq *ntp)
{
	struct mdchain *mdp;
	struct smb_rq *rqp = ntp->nt_rq;
	int error, error2;
	u_int32_t totpcount, totdcount, pcount, poff, doff, pdisp, ddisp;
	u_int32_t tmp, dcount, totpgot, totdgot;
	u_int16_t bc;
	u_int8_t wc;

	ntp->nt_flags &= ~SMBT2_MOREDATA;

	error = smb_rq_reply(rqp);
	if (rqp->sr_flags & SMBR_MOREDATA)
		ntp->nt_flags |= SMBT2_MOREDATA;
	ntp->nt_sr_error = rqp->sr_error;
	ntp->nt_sr_rpflags2 = rqp->sr_rpflags2;
	if (error && !(rqp->sr_flags & SMBR_MOREDATA))
		return (error);
	/*
	 * Now we have to get all subseqent responses. The CIFS specification
	 * says that they can be misordered which is weird.
	 * TODO: timo
	 */
	totpgot = totdgot = 0;
	totpcount = totdcount = 0xffffffff;
	mdp = &rqp->sr_rp;
	for (;;) {
		DTRACE_PROBE2(smb_trans_reply,
		    (smb_rq_t *), rqp, (mblk_t *), mdp->md_top);
		m_dumpm(mdp->md_top);

		if ((error2 = md_get_uint8(mdp, &wc)) != 0)
			break;
		if (wc < 18) {
			error2 = ENOENT;
			break;
		}
		md_get_mem(mdp, NULL, 3, MB_MSYSTEM); /* reserved */
		if ((error2 = md_get_uint32le(mdp, &tmp)) != 0)
			break;
		if (totpcount > tmp)
			totpcount = tmp;
		if ((error2 = md_get_uint32le(mdp, &tmp)) != 0)
			break;
		if (totdcount > tmp)
			totdcount = tmp;
		if ((error2 = md_get_uint32le(mdp, &pcount)) != 0 ||
		    (error2 = md_get_uint32le(mdp, &poff)) != 0 ||
		    (error2 = md_get_uint32le(mdp, &pdisp)) != 0)
			break;
		if (pcount != 0 && pdisp != totpgot) {
			SMBSDEBUG("Can't handle misordered parameters %d:%d\n",
			    pdisp, totpgot);
			error2 = EINVAL;
			break;
		}
		if ((error2 = md_get_uint32le(mdp, &dcount)) != 0 ||
		    (error2 = md_get_uint32le(mdp, &doff)) != 0 ||
		    (error2 = md_get_uint32le(mdp, &ddisp)) != 0)
			break;
		if (dcount != 0 && ddisp != totdgot) {
			SMBSDEBUG("Can't handle misordered data: dcount %d\n",
			    dcount);
			error2 = EINVAL;
			break;
		}

		/* XXX: Skip setup words?  We don't save them? */
		md_get_uint8(mdp, &wc);  /* SetupCount */
		tmp = wc;
		while (tmp--)
			md_get_uint16(mdp, NULL);

		if ((error2 = md_get_uint16le(mdp, &bc)) != 0)
			break;

		/*
		 * There are pad bytes here, and the poff value
		 * indicates where the next data are found.
		 * No need to guess at the padding size.
		 */
		if (pcount) {
			error2 = smb_t2_placedata(mdp->md_top, poff, pcount,
			    &ntp->nt_rparam);
			if (error2)
				break;
		}
		totpgot += pcount;

		if (dcount) {
			error2 = smb_t2_placedata(mdp->md_top, doff, dcount,
			    &ntp->nt_rdata);
			if (error2)
				break;
		}
		totdgot += dcount;

		if (totpgot >= totpcount && totdgot >= totdcount) {
			error2 = 0;
			ntp->nt_flags |= SMBT2_ALLRECV;
			break;
		}
		/*
		 * We're done with this reply, look for the next one.
		 */
		SMBRQ_LOCK(rqp);
		md_next_record(&rqp->sr_rp);
		SMBRQ_UNLOCK(rqp);
		error2 = smb_rq_reply(rqp);
		if (rqp->sr_flags & SMBR_MOREDATA)
			ntp->nt_flags |= SMBT2_MOREDATA;
		if (!error2)
			continue;
		ntp->nt_sr_error = rqp->sr_error;
		ntp->nt_sr_rpflags2 = rqp->sr_rpflags2;
		error = error2;
		if (!(rqp->sr_flags & SMBR_MOREDATA))
			break;
	}
	return (error ? error : error2);
}

int md_get_mbuf(struct mdchain *mdp, int size, mblk_t **ret);
int mb_put_mbuf(struct mbchain *mbp, mblk_t *m);

/*
 * Perform a full round of TRANS2 request
 */
static int
smb_t2_request_int(struct smb_t2rq *t2p)
{
	struct smb_vc *vcp = t2p->t2_vc;
	struct smb_cred *scred = t2p->t2_cred;
	struct mbchain *mbp;
	struct mdchain *mdp, mbparam, mbdata;
	mblk_t *m;
	struct smb_rq *rqp;
	int totpcount, leftpcount, totdcount, leftdcount, len, txmax, i;
	int error, doff, poff, txdcount, txpcount, nmlen;

	m = t2p->t2_tparam.mb_top;
	if (m) {
		md_initm(&mbparam, m);	/* do not free it! */
		totpcount = m_fixhdr(m);
		if (totpcount > 0xffff)		/* maxvalue for ushort_t */
			return (EINVAL);
	} else
		totpcount = 0;
	m = t2p->t2_tdata.mb_top;
	if (m) {
		md_initm(&mbdata, m);	/* do not free it! */
		totdcount =  m_fixhdr(m);
		if (totdcount > 0xffff)
			return (EINVAL);
	} else
		totdcount = 0;
	leftdcount = totdcount;
	leftpcount = totpcount;
	txmax = vcp->vc_txmax;
	error = smb_rq_alloc(t2p->t2_source, t2p->t_name[0] ?
	    SMB_COM_TRANSACTION : SMB_COM_TRANSACTION2, scred, &rqp);
	if (error)
		return (error);
	rqp->sr_timo = smb_timo_default;
	rqp->sr_flags |= SMBR_MULTIPACKET;
	t2p->t2_rq = rqp;
	mbp = &rqp->sr_rq;
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, totpcount);
	mb_put_uint16le(mbp, totdcount);
	mb_put_uint16le(mbp, t2p->t2_maxpcount);
	mb_put_uint16le(mbp, t2p->t2_maxdcount);
	mb_put_uint8(mbp, t2p->t2_maxscount);
	mb_put_uint8(mbp, 0);			/* reserved */
	mb_put_uint16le(mbp, 0);			/* flags */
	mb_put_uint32le(mbp, 0);			/* Timeout */
	mb_put_uint16le(mbp, 0);			/* reserved 2 */
	len = mb_fixhdr(mbp);

	/*
	 * now we have known packet size as
	 * ALIGN4(len + 5 * 2 + setupcount * 2 + 2 + strlen(name) + 1),
	 * and need to decide which parts should go into the first request
	 */
	nmlen = t2p->t_name ? strlen(t2p->t_name) : 0;
	len = ALIGN4(len + 5 * 2 + t2p->t2_setupcount * 2 + 2 + nmlen + 1);
	if (len + leftpcount > txmax) {
		txpcount = min(leftpcount, txmax - len);
		poff = len;
		txdcount = 0;
		doff = 0;
	} else {
		txpcount = leftpcount;
		poff = txpcount ? len : 0;
		/*
		 * Other client traffic seems to "ALIGN2" here.  The extra
		 * 2 byte pad we use has no observed downside and may be
		 * required for some old servers(?)
		 */
		len = ALIGN4(len + txpcount);
		txdcount = min(leftdcount, txmax - len);
		doff = txdcount ? len : 0;
	}
	leftpcount -= txpcount;
	leftdcount -= txdcount;
	mb_put_uint16le(mbp, txpcount);
	mb_put_uint16le(mbp, poff);
	mb_put_uint16le(mbp, txdcount);
	mb_put_uint16le(mbp, doff);
	mb_put_uint8(mbp, t2p->t2_setupcount);
	mb_put_uint8(mbp, 0);
	for (i = 0; i < t2p->t2_setupcount; i++) {
		mb_put_uint16le(mbp, t2p->t2_setupdata[i]);
	}
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	/* TDUNICODE */
	if (t2p->t_name)
		mb_put_mem(mbp, t2p->t_name, nmlen, MB_MSYSTEM);
	mb_put_uint8(mbp, 0);	/* terminating zero */
	len = mb_fixhdr(mbp);
	if (txpcount) {
		mb_put_mem(mbp, NULL, ALIGN4(len) - len, MB_MZERO);
		error = md_get_mbuf(&mbparam, txpcount, &m);
		SMBSDEBUG("%d:%d:%d\n", error, txpcount, txmax);
		if (error)
			goto freerq;
		mb_put_mbuf(mbp, m);
	}
	len = mb_fixhdr(mbp);
	if (txdcount) {
		mb_put_mem(mbp, NULL, ALIGN4(len) - len, MB_MZERO);
		error = md_get_mbuf(&mbdata, txdcount, &m);
		if (error)
			goto freerq;
		mb_put_mbuf(mbp, m);
	}
	smb_rq_bend(rqp);	/* incredible, but thats it... */
	error = smb_rq_enqueue(rqp);
	if (error)
		goto freerq;
	if (leftpcount || leftdcount) {
		error = smb_rq_reply(rqp);
		if (error)
			goto bad;
		/*
		 * this is an interim response, ignore it.
		 */
		SMBRQ_LOCK(rqp);
		md_next_record(&rqp->sr_rp);
		SMBRQ_UNLOCK(rqp);
	}
	while (leftpcount || leftdcount) {
		error = smb_rq_new(rqp, t2p->t_name ?
		    SMB_COM_TRANSACTION_SECONDARY :
		    SMB_COM_TRANSACTION2_SECONDARY);
		if (error)
			goto bad;
		mbp = &rqp->sr_rq;
		smb_rq_wstart(rqp);
		mb_put_uint16le(mbp, totpcount);
		mb_put_uint16le(mbp, totdcount);
		len = mb_fixhdr(mbp);
		/*
		 * now we have known packet size as
		 * ALIGN4(len + 7 * 2 + 2) for T2 request, and -2 for T one,
		 * and need to decide which parts should go into request
		 */
		len = ALIGN4(len + 6 * 2 + 2);
		if (t2p->t_name == NULL)
			len += 2;
		if (len + leftpcount > txmax) {
			txpcount = min(leftpcount, txmax - len);
			poff = len;
			txdcount = 0;
			doff = 0;
		} else {
			txpcount = leftpcount;
			poff = txpcount ? len : 0;
			len = ALIGN4(len + txpcount);
			txdcount = min(leftdcount, txmax - len);
			doff = txdcount ? len : 0;
		}
		mb_put_uint16le(mbp, txpcount);
		mb_put_uint16le(mbp, poff);
		mb_put_uint16le(mbp, totpcount - leftpcount);
		mb_put_uint16le(mbp, txdcount);
		mb_put_uint16le(mbp, doff);
		mb_put_uint16le(mbp, totdcount - leftdcount);
		leftpcount -= txpcount;
		leftdcount -= txdcount;
		if (t2p->t_name == NULL)
			mb_put_uint16le(mbp, t2p->t2_fid);
		smb_rq_wend(rqp);
		smb_rq_bstart(rqp);
		mb_put_uint8(mbp, 0);	/* name */
		len = mb_fixhdr(mbp);
		if (txpcount) {
			mb_put_mem(mbp, NULL, ALIGN4(len) - len, MB_MZERO);
			error = md_get_mbuf(&mbparam, txpcount, &m);
			if (error)
				goto bad;
			mb_put_mbuf(mbp, m);
		}
		len = mb_fixhdr(mbp);
		if (txdcount) {
			mb_put_mem(mbp, NULL, ALIGN4(len) - len, MB_MZERO);
			error = md_get_mbuf(&mbdata, txdcount, &m);
			if (error)
				goto bad;
			mb_put_mbuf(mbp, m);
		}
		smb_rq_bend(rqp);
		error = smb_iod_multirq(rqp);
		if (error)
			goto bad;
	}	/* while left params or data */
	error = smb_t2_reply(t2p);
	if (error && !(t2p->t2_flags & SMBT2_MOREDATA))
		goto bad;
	mdp = &t2p->t2_rdata;
	if (mdp->md_top) {
		m_fixhdr(mdp->md_top);
		md_initm(mdp, mdp->md_top);
	}
	mdp = &t2p->t2_rparam;
	if (mdp->md_top) {
		m_fixhdr(mdp->md_top);
		md_initm(mdp, mdp->md_top);
	}
bad:
	smb_iod_removerq(rqp);
freerq:
	if (error && !(t2p->t2_flags & SMBT2_MOREDATA)) {
		if (rqp->sr_flags & SMBR_RESTART)
			t2p->t2_flags |= SMBT2_RESTART;
		md_done(&t2p->t2_rparam);
		md_done(&t2p->t2_rdata);
	}
	smb_rq_done(rqp);
	return (error);
}


/*
 * Perform a full round of NT_TRANSACTION request
 */
static int
smb_nt_request_int(struct smb_ntrq *ntp)
{
	struct smb_vc *vcp = ntp->nt_vc;
	struct smb_cred *scred = ntp->nt_cred;
	struct mbchain *mbp;
	struct mdchain *mdp, mbsetup, mbparam, mbdata;
	mblk_t *m;
	struct smb_rq *rqp;
	int totpcount, leftpcount, totdcount, leftdcount, len, txmax;
	int error, doff, poff, txdcount, txpcount;
	int totscount;

	m = ntp->nt_tsetup.mb_top;
	if (m) {
		md_initm(&mbsetup, m);	/* do not free it! */
		totscount = m_fixhdr(m);
		if (totscount > 2 * 0xff)
			return (EINVAL);
	} else
		totscount = 0;
	m = ntp->nt_tparam.mb_top;
	if (m) {
		md_initm(&mbparam, m);	/* do not free it! */
		totpcount = m_fixhdr(m);
		if (totpcount > 0x7fffffff)
			return (EINVAL);
	} else
		totpcount = 0;
	m = ntp->nt_tdata.mb_top;
	if (m) {
		md_initm(&mbdata, m);	/* do not free it! */
		totdcount =  m_fixhdr(m);
		if (totdcount > 0x7fffffff)
			return (EINVAL);
	} else
		totdcount = 0;
	leftdcount = totdcount;
	leftpcount = totpcount;
	txmax = vcp->vc_txmax;
	error = smb_rq_alloc(ntp->nt_source, SMB_COM_NT_TRANSACT, scred, &rqp);
	if (error)
		return (error);
	rqp->sr_timo = smb_timo_default;
	rqp->sr_flags |= SMBR_MULTIPACKET;
	ntp->nt_rq = rqp;
	mbp = &rqp->sr_rq;
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, ntp->nt_maxscount);
	mb_put_uint16le(mbp, 0);	/* reserved (flags?) */
	mb_put_uint32le(mbp, totpcount);
	mb_put_uint32le(mbp, totdcount);
	mb_put_uint32le(mbp, ntp->nt_maxpcount);
	mb_put_uint32le(mbp, ntp->nt_maxdcount);
	len = mb_fixhdr(mbp);
	/*
	 * now we have known packet size as
	 * ALIGN4(len + 4 * 4 + 1 + 2 + ((totscount+1)&~1) + 2),
	 * and need to decide which parts should go into the first request
	 */
	len = ALIGN4(len + 4 * 4 + 1 + 2 + ((totscount+1)&~1) + 2);
	if (len + leftpcount > txmax) {
		txpcount = min(leftpcount, txmax - len);
		poff = len;
		txdcount = 0;
		doff = 0;
	} else {
		txpcount = leftpcount;
		poff = txpcount ? len : 0;
		len = ALIGN4(len + txpcount);
		txdcount = min(leftdcount, txmax - len);
		doff = txdcount ? len : 0;
	}
	leftpcount -= txpcount;
	leftdcount -= txdcount;
	mb_put_uint32le(mbp, txpcount);
	mb_put_uint32le(mbp, poff);
	mb_put_uint32le(mbp, txdcount);
	mb_put_uint32le(mbp, doff);
	mb_put_uint8(mbp, (totscount+1)/2);
	mb_put_uint16le(mbp, ntp->nt_function);
	if (totscount) {
		error = md_get_mbuf(&mbsetup, totscount, &m);
		SMBSDEBUG("%d:%d:%d\n", error, totscount, txmax);
		if (error)
			goto freerq;
		mb_put_mbuf(mbp, m);
		if (totscount & 1)
			mb_put_uint8(mbp, 0); /* setup is in words */
	}
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	len = mb_fixhdr(mbp);
	if (txpcount) {
		mb_put_mem(mbp, NULL, ALIGN4(len) - len, MB_MZERO);
		error = md_get_mbuf(&mbparam, txpcount, &m);
		SMBSDEBUG("%d:%d:%d\n", error, txpcount, txmax);
		if (error)
			goto freerq;
		mb_put_mbuf(mbp, m);
	}
	len = mb_fixhdr(mbp);
	if (txdcount) {
		mb_put_mem(mbp, NULL, ALIGN4(len) - len, MB_MZERO);
		error = md_get_mbuf(&mbdata, txdcount, &m);
		if (error)
			goto freerq;
		mb_put_mbuf(mbp, m);
	}
	smb_rq_bend(rqp);	/* incredible, but thats it... */
	error = smb_rq_enqueue(rqp);
	if (error)
		goto freerq;
	if (leftpcount || leftdcount) {
		error = smb_rq_reply(rqp);
		if (error)
			goto bad;
		/*
		 * this is an interim response, ignore it.
		 */
		SMBRQ_LOCK(rqp);
		md_next_record(&rqp->sr_rp);
		SMBRQ_UNLOCK(rqp);
	}
	while (leftpcount || leftdcount) {
		error = smb_rq_new(rqp, SMB_COM_NT_TRANSACT_SECONDARY);
		if (error)
			goto bad;
		mbp = &rqp->sr_rq;
		smb_rq_wstart(rqp);
		mb_put_mem(mbp, NULL, 3, MB_MZERO);
		mb_put_uint32le(mbp, totpcount);
		mb_put_uint32le(mbp, totdcount);
		len = mb_fixhdr(mbp);
		/*
		 * now we have known packet size as
		 * ALIGN4(len + 6 * 4  + 2)
		 * and need to decide which parts should go into request
		 */
		len = ALIGN4(len + 6 * 4 + 2);
		if (len + leftpcount > txmax) {
			txpcount = min(leftpcount, txmax - len);
			poff = len;
			txdcount = 0;
			doff = 0;
		} else {
			txpcount = leftpcount;
			poff = txpcount ? len : 0;
			len = ALIGN4(len + txpcount);
			txdcount = min(leftdcount, txmax - len);
			doff = txdcount ? len : 0;
		}
		mb_put_uint32le(mbp, txpcount);
		mb_put_uint32le(mbp, poff);
		mb_put_uint32le(mbp, totpcount - leftpcount);
		mb_put_uint32le(mbp, txdcount);
		mb_put_uint32le(mbp, doff);
		mb_put_uint32le(mbp, totdcount - leftdcount);
		leftpcount -= txpcount;
		leftdcount -= txdcount;
		smb_rq_wend(rqp);
		smb_rq_bstart(rqp);
		len = mb_fixhdr(mbp);
		if (txpcount) {
			mb_put_mem(mbp, NULL, ALIGN4(len) - len, MB_MZERO);
			error = md_get_mbuf(&mbparam, txpcount, &m);
			if (error)
				goto bad;
			mb_put_mbuf(mbp, m);
		}
		len = mb_fixhdr(mbp);
		if (txdcount) {
			mb_put_mem(mbp, NULL, ALIGN4(len) - len, MB_MZERO);
			error = md_get_mbuf(&mbdata, txdcount, &m);
			if (error)
				goto bad;
			mb_put_mbuf(mbp, m);
		}
		smb_rq_bend(rqp);
		error = smb_iod_multirq(rqp);
		if (error)
			goto bad;
	}	/* while left params or data */
	error = smb_nt_reply(ntp);
	if (error && !(ntp->nt_flags & SMBT2_MOREDATA))
		goto bad;
	mdp = &ntp->nt_rdata;
	if (mdp->md_top) {
		m_fixhdr(mdp->md_top);
		md_initm(mdp, mdp->md_top);
	}
	mdp = &ntp->nt_rparam;
	if (mdp->md_top) {
		m_fixhdr(mdp->md_top);
		md_initm(mdp, mdp->md_top);
	}
bad:
	smb_iod_removerq(rqp);
freerq:
	if (error && !(ntp->nt_flags & SMBT2_MOREDATA)) {
		if (rqp->sr_flags & SMBR_RESTART)
			ntp->nt_flags |= SMBT2_RESTART;
		md_done(&ntp->nt_rparam);
		md_done(&ntp->nt_rdata);
	}
	smb_rq_done(rqp);
	return (error);
}

int
smb_t2_request(struct smb_t2rq *t2p)
{
	int error = EINVAL, i;

	for (i = 0; ; ) {
		/*
		 * Don't send any new requests if force unmount is underway.
		 * This check was moved into smb_rq_enqueue, called by
		 * smb_t2_request_int()
		 */
		t2p->t2_flags &= ~SMBT2_RESTART;
		error = smb_t2_request_int(t2p);
		if (!error)
			break;
		if ((t2p->t2_flags & (SMBT2_RESTART | SMBT2_NORESTART)) !=
		    SMBT2_RESTART)
			break;
		if (++i > SMBMAXRESTARTS)
			break;
		mutex_enter(&(t2p)->t2_lock);
		if (t2p->t2_share && t2p->t2_share->ss_mount) {
			cv_timedwait(&t2p->t2_cond, &(t2p)->t2_lock,
			    lbolt + (hz * SMB_RCNDELAY));
		} else {
			delay(lbolt + (hz * SMB_RCNDELAY));
		}
		mutex_exit(&(t2p)->t2_lock);
	}
	return (error);
}


int
smb_nt_request(struct smb_ntrq *ntp)
{
	int error = EINVAL, i;

	for (i = 0; ; ) {
		/*
		 * Don't send any new requests if force unmount is underway.
		 * This check was moved into smb_rq_enqueue, called by
		 * smb_nt_request_int()
		 */
		ntp->nt_flags &= ~SMBT2_RESTART;
		error = smb_nt_request_int(ntp);
		if (!error)
			break;
		if ((ntp->nt_flags & (SMBT2_RESTART | SMBT2_NORESTART)) !=
		    SMBT2_RESTART)
			break;
		if (++i > SMBMAXRESTARTS)
			break;
		mutex_enter(&(ntp)->nt_lock);
		if (ntp->nt_share && ntp->nt_share->ss_mount) {
			cv_timedwait(&ntp->nt_cond, &(ntp)->nt_lock,
			    lbolt + (hz * SMB_RCNDELAY));

		} else {
			delay(lbolt + (hz * SMB_RCNDELAY));
		}
		mutex_exit(&(ntp)->nt_lock);
	}
	return (error);
}
