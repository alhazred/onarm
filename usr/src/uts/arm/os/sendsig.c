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

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc. */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T   */
/*	All Rights Reserved   */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#pragma ident	"@(#)sendsig.c	1.6	06/03/14 SMI"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/class.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/buf.h>
#include <sys/kmem.h>
#include <sys/cred.h>
#include <sys/archsystm.h>
#include <sys/vmparam.h>
#include <sys/prsystm.h>
#include <sys/reboot.h>
#include <sys/uadmin.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/file.h>
#include <sys/session.h>
#include <sys/ucontext.h>
#include <sys/dnlc.h>
#include <sys/var.h>
#include <sys/cmn_err.h>
#include <sys/thread.h>
#include <sys/vtrace.h>
#include <sys/consdev.h>
#include <sys/regset.h>
#include <sys/frame.h>

#include <sys/privregs.h>

#include <sys/stack.h>
#include <sys/swap.h>
#include <vm/hat.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_kmem.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <sys/exec.h>
#include <sys/acct.h>
#include <sys/core.h>
#include <sys/corectl.h>
#include <sys/modctl.h>
#include <sys/tuneable.h>
#include <c2/audit.h>
#include <sys/bootconf.h>
#include <sys/dumphdr.h>
#include <sys/promif.h>
#include <sys/systeminfo.h>
#include <sys/kdi.h>
#include <sys/contract_impl.h>

#if	STACK_ENTRY_ALIGN == 8

/*
 * SA() macro adjust value to STACK_ENTRY boundary, but stack address at
 * the function call must be aligned to STACK_ENTRY_ALIGN boundary.
 * The code in this file utterly depends on STACK_ALIGN being 4 and
 * STACK_ENTRY_ALIGN being 8.
 */

#define	STACK_ENTRY_ROUNDUP(x, type)					\
	do {								\
		(x) = (type)P2ROUNDUP_TYPED(x, STACK_ENTRY_ALIGN,	\
					    uintptr_t);			\
	} while (0)
#define	STACK_ENTRY_ROUNDDOWN(x, type)					\
	do {								\
		(x) = (type)P2ALIGN_TYPED(x, STACK_ENTRY_ALIGN,		\
					  uintptr_t);			\
	} while (0)
#define	STACK_ENTRY_CHECK(x)	ASSERT(IS_P2ALIGNED(x, STACK_ENTRY_ALIGN))

#else	/* STACK_ENTRY_ALIGN != 8 */

/*
 * Currently, STACK_ENTRY_ROUNDUP() is used to align min stack size required
 * to catch signal. We can define it as NOP because min stack size is already
 * aligned to STACK_ALIGN boundary by SA() macro.
 */
#define	STACK_ENTRY_ROUNDUP(x, type)
#define	STACK_ENTRY_ROUNDDOWN(x, type)					\
	do {								\
		(x) = (type)P2ALIGN_TYPED(x, STACK_ALIGN, uintptr_t);	\
	} while (0)
#define	STACK_ENTRY_CHECK(x)	ASSERT(IS_P2ALIGNED(x, STACK_ALIGN))

#endif	/* STACK_ENTRY_ALIGN == 8 */

/*
 * Construct the execution environment for the user's signal
 * handler and arrange for control to be given to it on return
 * to userland.  The library code now calls setcontext() to
 * clean up after the signal handler, so sigret() is no longer
 * needed.
 *
 * (The various 'volatile' declarations are need to ensure that values
 * are correct on the error return from on_fault().)
 */
int
sendsig(int sig, k_siginfo_t *sip, void (*hdlr)())
{
	volatile int minstacksz; /* min stack required to catch signal */
	int newstack;		/* if true, switching to altstack */
	label_t ljb;
	volatile caddr_t sp;
	caddr_t fp;
	struct regs *rp;
	volatile greg_t upc;
	klwp_t *lwp = ttolwp(curthread);
	volatile proc_t *p = ttoproc(curthread);
	greg_t fp_addr;
	siginfo_t *sip_addr;
	ucontext_t *ucp_addr;
	ucontext_t *volatile tuc = NULL;
	volatile int watched;

	rp = lwptoregs(lwp);
	upc = rp->r_pc;

	minstacksz = SA(sizeof(struct frame)) + SA(sizeof(ucontext_t));
	if (sip != NULL) {
		minstacksz += SA(sizeof(siginfo_t));
	}
	STACK_ENTRY_ROUNDUP(minstacksz, int);
	ASSERT((minstacksz & (STACK_ALIGN - 1ul)) == 0);

	/*
	 * Figure out whether we will be handling this signal on
	 * an alternate stack specified by the user. Then allocate
	 * and validate the stack requirements for the signal handler
	 * context. on_fault will catch any faults.
	 */
	newstack = (sigismember(&PTOU(curproc)->u_sigonstack, sig) &&
	    !(lwp->lwp_sigaltstack.ss_flags & (SS_ONSTACK|SS_DISABLE)));

	if (newstack) {
		fp = (caddr_t)(SA((uintptr_t)lwp->lwp_sigaltstack.ss_sp) +
		    SA(lwp->lwp_sigaltstack.ss_size) - STACK_ALIGN);
	} else {
		fp = (caddr_t)rp->r_sp;
	}

	/*
	 * Force proper stack pointer alignment, even in the face of a
	 * misaligned stack pointer from user-level before the signal.
	 * Don't use the SA() macro because that rounds up, not down.
	 */
	STACK_ENTRY_ROUNDDOWN(fp, caddr_t);
	sp = fp - minstacksz;

	/*
	 * Make sure lwp hasn't trashed its stack.
	 */
	if (sp >= (caddr_t)USERLIMIT || fp >= (caddr_t)USERLIMIT) {
#ifdef DEBUG
		printf("sendsig: bad signal stack cmd=%s, pid=%d, sig=%d\n",
		    PTOU(p)->u_comm, p->p_pid, sig);
		printf("sigsp = 0x%p, action = 0x%p, upc = 0x%lx\n",
		    (void *)sp, (void *)hdlr, (uintptr_t)upc);
		printf("sp above USERLIMIT\n");
#endif
		return (0);
	}

	watched = watch_disable_addr((caddr_t)sp, minstacksz, S_WRITE);

	if (on_fault(&ljb))
		goto badstack;

	/* frame */
	fp_addr = (greg_t)fp - sizeof(greg_t);
	fp -= SA(sizeof(struct frame));
	uzero(fp, sizeof(struct frame));

	/* save the current context on the user stack */
	fp -= SA(sizeof(ucontext_t));
	ucp_addr = (ucontext_t *)fp;
	tuc = kmem_alloc(sizeof(ucontext_t), KM_SLEEP);
	savecontext(tuc, lwp->lwp_sigoldmask);
	copyout_noerr(tuc, ucp_addr, sizeof(ucontext_t));
	kmem_free(tuc, sizeof(ucontext_t));
	tuc = NULL;

	if (sip != NULL) {
		zoneid_t zoneid;

		fp -= SA(sizeof (siginfo_t));
		uzero(fp, sizeof (siginfo_t));
		if (SI_FROMUSER(sip) &&
		    (zoneid = p->p_zone->zone_id) != GLOBAL_ZONEID &&
		    zoneid != sip->si_zoneid) {
			k_siginfo_t sani_sip = *sip;
			sani_sip.si_pid = p->p_zone->zone_zsched->p_pid;
			sani_sip.si_uid = 0;
			sani_sip.si_ctid = -1;
			sani_sip.si_zoneid = zoneid;
			copyout_noerr(&sani_sip, fp, sizeof (sani_sip));
		} else {
			copyout_noerr(sip, fp, sizeof (*sip));
		}
		sip_addr = (siginfo_t *)fp;

		if (sig == SIGPROF &&
		    curthread->t_rprof != NULL &&
		    curthread->t_rprof->rp_anystate) {
			/*
			 * We stand on our head to deal with
			 * the real time profiling signal.
			 * Fill in the stuff that doesn't fit
			 * in a normal k_siginfo structure.
			 */
			int i = sip->si_nsysarg;
			while (--i >= 0) {
				suword32_noerr(&(sip_addr->si_sysarg[i]),
				    (uint32_t)lwp->lwp_arg[i]);
			}
			copyout_noerr(curthread->t_rprof->rp_state,
			    sip_addr->si_mstate,
			    sizeof (curthread->t_rprof->rp_state));
		}
	} else {
		sip_addr = (siginfo_t *)NULL;
	}

	lwp->lwp_oldcontext = (uintptr_t)ucp_addr;

	if (newstack != 0) {
		lwp->lwp_sigaltstack.ss_flags |= SS_ONSTACK;

		if (lwp->lwp_ustack) {
			copyout_noerr(&lwp->lwp_sigaltstack,
			    (stack_t *)lwp->lwp_ustack, sizeof (stack_t));
		}
	}

	no_fault();
	if (watched)
		watch_enable_addr((caddr_t)sp, minstacksz, S_WRITE);

	STACK_ENTRY_CHECK(sp);

	/*
	 * Set up user registers for execution of signal handler.
	 */
	rp->r_r11 = fp_addr;
	rp->r_sp = (greg_t)sp;
	rp->r_lr = (greg_t)0xffffffff;	/* never return! */
	rp->r_pc = (greg_t)hdlr;
	rp->r_r0 = (greg_t)sig;
	rp->r_r1 = (greg_t)sip_addr;
	rp->r_r2 = (greg_t)ucp_addr;
	rp->r_cpsr = PSR_USERINIT;

	/*
	 * Don't set lwp_eosys here.  sendsig() is called via psig() after
	 * lwp_eosys is handled, so setting it here would affect the next
	 * system call.
	 */
	return (1);

badstack:
	no_fault();
	if (watched)
		watch_enable_addr((caddr_t)sp, minstacksz, S_WRITE);
	if (tuc)
		kmem_free(tuc, sizeof(ucontext_t));
#ifdef DEBUG
	printf("sendsig: bad signal stack cmd=%s, pid=%d, sig=%d\n",
	    PTOU(p)->u_comm, p->p_pid, sig);
	printf("on fault, sigsp = 0x%p, action = 0x%p, upc = 0x%lx\n",
	    (void *)sp, (void *)hdlr, (uintptr_t)upc);
#endif
	return (0);
}
