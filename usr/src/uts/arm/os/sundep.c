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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc. */
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T   */
/*	All Rights Reserved   */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#pragma ident	"@(#)sundep.c"

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
#include <sys/fp_impl.h>
#include <asm/cpufunc.h>

/*
 * Compare the version of boot that boot says it is against
 * the version of boot the kernel expects.
 */
int
check_boot_version(int boots_version)
{
#ifdef DEBUG
	if (boots_version == BO_VERSION)
		return (0);

	prom_printf("Wrong boot interface - kernel needs v%d found v%d\n",
	    BO_VERSION, boots_version);
	prom_panic("halting");
	/*NOTREACHED*/
#else
	return (0);
#endif /* DEBUG */
}

/*
 * Process the physical installed list for boot.
 * Finds:
 * 1) the pfn of the highest installed physical page,
 * 2) the number of pages installed
 * 3) the number of distinct contiguous regions these pages fall into.
 */
void
installed_top_size(
	struct memlist *list,	/* pointer to start of installed list */
	pfn_t *high_pfn,	/* return ptr for top value */
	pgcnt_t *pgcnt,		/* return ptr for sum of installed pages */
	int	*ranges)	/* return ptr for the count of contig. ranges */
{
	pfn_t top = 0;
	pgcnt_t sumpages = 0;
	pfn_t highp;		/* high page in a chunk */
	int cnt = 0;

	for (; list; list = list->next) {
		++cnt;
		highp = (list->address + list->size - 1) >> PAGESHIFT;
		if (top < highp)
			top = highp;
		sumpages += btop(list->size);
	}

	*high_pfn = top;
	*pgcnt = sumpages;
	*ranges = cnt;
}

/*
 * Copy in a memory list from boot to kernel, with a filter function
 * to remove pages. The filter function can increase the address and/or
 * decrease the size to filter out pages.  It will also align addresses and
 * sizes to PAGESIZE.
 */
void
copy_memlist_filter(
	struct memlist *src,
	struct memlist **dstp,
	void (*filter)(uint64_t *, uint64_t *))
{
	struct memlist *dst, *prev;
	uint64_t addr;
	uint64_t size;
	uint64_t eaddr;

	dst = *dstp;
	prev = dst;

	/*
	 * Move through the memlist applying a filter against
	 * each range of memory. Note that we may apply the
	 * filter multiple times against each memlist entry.
	 */
	for (; src; src = src->next) {
		addr = P2ROUNDUP(src->address, PAGESIZE);
		eaddr = P2ALIGN(src->address + src->size, PAGESIZE);
		while (addr < eaddr) {
			size = eaddr - addr;
			if (filter != NULL)
				filter(&addr, &size);
			if (size == 0)
				break;
			dst->address = addr;
			dst->size = size;
			dst->next = 0;
			if (prev == dst) {
				dst->prev = 0;
				dst++;
			} else {
				dst->prev = prev;
				prev->next = dst;
				dst++;
				prev++;
			}
			addr += size;
		}
	}

	*dstp = dst;
}

/*
 * Kernel setup code, called from startup().
 */
void
kern_setup1(void)
{
	proc_t *pp;

	pp = &p0;

	proc_sched = pp;

	/*
	 * Initialize process 0 data structures
	 */
	pp->p_stat = SRUN;
	pp->p_flag = SSYS;

	pp->p_pidp = &pid0;
	pp->p_pgidp = &pid0;
	pp->p_sessp = &session0;
	pp->p_tlist = &t0;
	pid0.pid_pglink = pp;
	pid0.pid_pgtail = pp;

	/*
	 * XXX - we asssume that the u-area is zeroed out except for
	 * ttolwp(curthread)->lwp_regs.
	 */
	PTOU(curproc)->u_cmask = (mode_t)CMASK;

	thread_init();		/* init thread_free list */
	pid_init();		/* initialize pid (proc) table */
	contract_init();	/* initialize contracts */

	init_pages_pp_maximum();
}

/*
 * Load a procedure into a thread.
 */
void
thread_load(kthread_t *t, void (*start)(), caddr_t arg, size_t len)
{
	caddr_t sp;
	size_t framesz;
	caddr_t argp;
	long *p;
	extern void thread_start();

	/*
	 * Push a "c" call frame onto the stack to represent
	 * the caller of "start".
	 */
	sp = t->t_stk;
	ASSERT(((uintptr_t)t->t_stk & (STACK_ENTRY_ALIGN - 1)) == 0);
	if (len != 0) {
		/*
		 * the object that arg points at is copied into the
		 * caller's frame.
		 */
		framesz = SA(len);
		sp -= framesz;
		ASSERT(sp > t->t_stkbase);
		argp = sp;
		bcopy(arg, argp, len);
		arg = argp;
	}
	/*
	 * Set up arguments (arg and len) on the caller's stack frame.
	 */
#if	STACK_ENTRY_ALIGN == 8
	/* We must append padding to align entry address of stack. */
	p = (long *)((uintptr_t)sp - 4);
#else	/* STACK_ENTRY_ALIGN != 8 */
	p = (long *)sp;
#endif	/* STACK_ENTRY_ALIGN == 8 */

	*--p = (intptr_t)start;
	*--p = (long)len;
	*--p = (intptr_t)arg;

	/*
	 * initialize thread to resume at thread_start() which will
	 * turn around and invoke (*start)(arg, len).
	 */
	t->t_pc = (uintptr_t)thread_start;
	t->t_sp = (uintptr_t)p;

	ASSERT((t->t_sp & (STACK_ENTRY_ALIGN - 1)) == 0);
}

/*
 * load user registers into lwp.
 * thrptr ignored for ARM.
 */
/*ARGSUSED2*/
void
lwp_load(klwp_t *lwp, gregset_t grp, uintptr_t thrptr)
{
	struct regs *rp = lwptoregs(lwp);

	setgregs(lwp, grp);
	rp->r_cpsr = PSR_USERINIT;

	FP_INIT(lwp);

	lwp->lwp_eosys = JUSTRETURN;
	lwptot(lwp)->t_post_sys = 1;
}

/*
 * set syscall()'s return values for a lwp.
 */
void
lwp_setrval(klwp_t *lwp, int v1, int v2)
{
	struct regs *rp = lwptoregs(lwp);
	rp->r_cpsr &= ~PSR_C_BIT;
	rp->r_r0 = v1;
	rp->r_r1 = v2;
}

/*
 * set syscall()'s return values for a lwp.
 */
void
lwp_setsp(klwp_t *lwp, caddr_t sp)
{
	lwptoregs(lwp)->r_sp = (intptr_t)sp;
}

/*
 * Copy regs from parent to child.
 */
void
lwp_forkregs(klwp_t *lwp, klwp_t *clwp)
{
	bcopy(lwp->lwp_regs, clwp->lwp_regs, sizeof (struct regs));
	if (lwp == ttolwp(curthread)) {
		/*
		 * If parent 'lwp' is current lwp, copy usertp to child's pcb.
		 * Otherwise, usertp is already saved in parent's pcb.
		 */
		clwp->lwp_pcb.pcb_usertp = READ_CP15(0, c13, c0, 2);
	}
	FP_FORK(lwp, clwp);
}

/*
 * Free lwp fpu regs.
 */
/*ARGSUSED*/
void
lwp_freeregs(klwp_t *lwp, int isexec)
{
	FP_FREE(lwp, isexec);
}

/*
 * This function is currently unused on ARM.
 */
void
lwp_pcb_exit(void)
{}

/*
 * Clear registers on exec(2).
 */
void
setregs(uarg_t *args)
{
	struct regs *rp;
	kthread_t *t = curthread;
	klwp_t *lwp = ttolwp(t);
	pcb_t *pcb = &lwp->lwp_pcb;
	greg_t sp;

	/*
	 * Initialize user registers
	 */
	(void) save_syscall_args();	/* copy args from registers first */
	rp = lwptoregs(lwp);
	sp = rp->r_sp;
	bzero(rp, sizeof (*rp));

	rp->r_sp = sp;
	rp->r_pc = args->entry;
	WRITE_CP15(0, c13, c0, 2, args->thrptr);
	rp->r_cpsr = PSR_USERINIT;

	lwp->lwp_eosys = JUSTRETURN;
	t->t_post_sys = 1;
}

#if !defined(lwp_getdatamodel)

/*
 * Return the datamodel of the given lwp.
 */
/*ARGSUSED*/
model_t
lwp_getdatamodel(klwp_t *lwp)
{
	return (lwp->lwp_procp->p_model);
}

#endif	/* !lwp_getdatamodel */

#if !defined(get_udatamodel)

model_t
get_udatamodel(void)
{
	return (curproc->p_model);
}

#endif	/* !get_udatamodel */
