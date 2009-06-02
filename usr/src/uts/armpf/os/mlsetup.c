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

/*
 * Copyright (c) 2006-2009 NEC Corporation
 */

#pragma ident	"@(#)mlsetup.c	1.56	06/05/18 SMI"

#include <sys/types.h>
#include <sys/disp.h>
#include <sys/promif.h>
#include <sys/clock.h>
#include <sys/cpuvar.h>
#include <sys/stack.h>
#include <vm/as.h>
#include <vm/hat.h>
#include <vm/vm_dep.h>
#include <sys/reboot.h>
#include <sys/avintr.h>
#include <sys/vtrace.h>
#include <sys/proc.h>
#include <sys/thread.h>
#include <sys/cpupart.h>
#include <sys/pset.h>
#include <sys/copyops.h>
#include <sys/debug.h>
#include <sys/sunddi.h>
#include <sys/privregs.h>
#include <sys/machsystm.h>
#include <sys/ontrap.h>
#include <sys/bootconf.h>
#include <sys/kdi.h>
#include <sys/archsystm.h>
#include <sys/kobj.h>
#include <sys/kobj_lex.h>
#include <sys/trap.h>
#include <asm/cpufunc.h>
#include <sys/mp_cpu.h>

/* Hardware processor ID of boot processor. */
uint_t	boot_cpuid;

extern void	fiq_mlsetup(cpu_t *cp);

#ifdef	STATIC_UNIX
extern void	boot_static_init(void);
#else	/* !STATIC_UNIX */
#define	boot_static_init()
#endif	/* STATIC_UNIX */

static uint32_t
bootprop_getval(char *name)
{
	char prop[32];
	u_longlong_t ll;
	extern struct bootops *bootops;
	if ((BOP_GETPROPLEN(bootops, name) > sizeof (prop)) ||
	    (BOP_GETPROP(bootops, name, prop) < 0) ||
	    (kobj_getvalue(prop, &ll) == -1))
		return (0);
	return ((uint32_t)ll);
}

/*
 * Setup routine called right before main(). Interposing this function
 * before main() allows us to call it in a machine-independent fashion.
 */
void
mlsetup(struct regs *rp)
{
	extern struct classfuncs sys_classfuncs;
	extern disp_t cpu0_disp;
	extern char t0stack[];
	int boot_ncpus;
	cpu_t *cpu0;
	uint_t	cpuid;
	extern void scucnt_mlsetup(void);

	ASSERT_STACK_ALIGNED();

	cpuid = boot_cpuid = HARD_PROCESSOR_ID();

	/* Early bootstrap initialization for static-linked unix. */
	boot_static_init();

	boot_ncpus = bootprop_getval("boot-ncpus");

	/* MPCore has 4 cores. */
	if (boot_ncpus <= 0 || boot_ncpus > NCPU) {
		boot_ncpus = 4;
	}

	max_ncpus = boot_max_ncpus = boot_ncpus;
	hat_mlsetup();

	/* Allocate buffer for struct cpu. */
	MP_CPU_BOOT_ALLOC(cpu0, boot_ncpus);
	cpu_boot = cpu0;

	/* Early initialization of SCU performance monitor counter. */
	scucnt_mlsetup();

	/*
	 * initialize cpu_self
	 */
	cpu0->cpu_self = cpu0;

	(void)cpuid_pass1(cpu0);

	/*
	 * initialize t0
	 */
	t0.t_stk = (caddr_t)rp - MINFRAME;
	t0.t_stkbase = t0stack;
	t0.t_pri = maxclsyspri - 3;
	t0.t_schedflag = TS_LOAD | TS_DONT_SWAP;
	t0.t_procp = &p0;
	t0.t_plockp = &p0lock.pl_lock;
	t0.t_lwp = &lwp0;
	t0.t_forw = &t0;
	t0.t_back = &t0;
	t0.t_next = &t0;
	t0.t_prev = &t0;
	t0.t_cpu = cpu0;
	t0.t_disp_queue = &cpu0_disp;
	t0.t_bind_cpu = PBIND_NONE;
	t0.t_bind_pset = PS_NONE;
	t0.t_cpupart = &cp_default;
	t0.t_clfuncs = &sys_classfuncs.thread;
	t0.t_copyops = NULL;

	/* We must initialize current thread pointer before CPU dereference. */
	threadp_set(&t0);

	THREAD_ONPROC(&t0, CPU);

	lwp0.lwp_thread = &t0;
	lwp0.lwp_regs = (void *) rp;
	lwp0.lwp_procp = &p0;
	t0.t_tid = p0.p_lwpcnt = p0.p_lwprcnt = p0.p_lwpid = 1;

	p0.p_exec = NULL;
	p0.p_stat = SRUN;
	p0.p_flag = SSYS;
	p0.p_tlist = &t0;
	p0.p_stksize = 2*PAGESIZE;
	p0.p_stkpageszc = 0;
	p0.p_as = &kas;
	p0.p_lockp = &p0lock;
	p0.p_brkpageszc = 0;
	p0.p_t1_lgrpid = LGRP_NONE;
	p0.p_tr_lgrpid = LGRP_NONE;
	sigorset(&p0.p_ignore, &ignoredefault);

	CPU->cpu_thread = &t0;
	bzero(&cpu0_disp, sizeof (disp_t));
	CPU->cpu_disp = &cpu0_disp;
	CPU->cpu_disp->disp_cpu = cpu0;
	CPU->cpu_dispthread = &t0;
	CPU->cpu_idle_thread = &t0;
	CPU->cpu_flags = CPU_READY | CPU_RUNNING | CPU_EXISTS | CPU_ENABLE;
	CPU->cpu_dispatch_pri = t0.t_pri;

	CPU->cpu_id = cpuid;
	cpu[cpuid] = cpu0;

	CPU->cpu_pri = 12;		/* initial PIL for the boot CPU */

	/* Setup stack for FIQ mode. */
	fiq_mlsetup(CPU);

	/*
	 * Initialize thread/cpu microstate accounting here
	 */
	init_mstate(&t0, LMS_SYSTEM);
	init_cpu_mstate(CPU, CMS_SYSTEM);

	/*
	 * Initialize lists of available and active CPUs.
	 */
	cpu_list_init(cpu0);

	cpu_vm_data_init(cpu0);

	/*
	 * Initialize the lgrp framework
	 */
	lgrp_init();

	prom_init("kernel", (void *)NULL);

	if (boothowto & RB_HALT) {
		prom_printf("unix: kernel halted by -h flag\n");
		prom_enter_mon();
	}

	ASSERT_STACK_ALIGNED();
}
