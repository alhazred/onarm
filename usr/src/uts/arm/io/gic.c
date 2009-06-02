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
 * Copyright (c) 2006-2009 NEC Corporation
 * All rights reserved.
 */

#include <sys/types.h>
#include <sys/avintr.h>
#include <sys/mpcore.h>
#include <sys/platform.h>
#include <asm/cpufunc.h>
#include <sys/machlock.h>
#include <sys/cpuvar.h>
#include <sys/gic.h>
#include <sys/bootconf.h>
#include <sys/smp_impldefs.h>

void gic_dist_init(void);
void gic_cpuif_init(void);
void gic_mask_irq(uint32_t irq);
void gic_unmask_irq(uint32_t irq);
void gic_ack_irq(uint32_t irq);
static void gic_prio_mask(uint32_t ipl);
void gic_set_ipl(uint32_t irq, uint32_t ipl);
void gic_softintr(uint32_t irq, uint32_t cpulist);
void gic_send_ipi(cpuset_t cpuset, uint32_t irq);
int setlvl(int irq);
int gic_addspl(int irq, int ipl, int min_ipl, int max_ipl);
int gic_delspl(int irq, int ipl, int min_ipl, int max_ipl);
int gic_intr_ops(dev_info_t *dip, ddi_intr_handle_impl_t *hdlp,
		 gic_intr_op_t intr_op, int *result);
void gic_enable_intr(processorid_t cpun);
int gic_disable_intr(processorid_t cpun);
extern processorid_t irq_to_bound_cpuid(int);

#define	IRQ_MASK_VALUE(irq)		(1 << ((irq) % 32))
#define	IRQ_MASK_OFFSET(irq)		(((irq) / 32) * 4)

/* 
 * Convert IPL to DIC-IPL.
 * Solaris IPL : DIC IPL
 * 0x0         : 0xf Interrupts with priority 0x0-0xE are not masked.
 * :           :  :
 * 0xf         : 0x0 All interrupts are masked.
 */
#define IPL_TO_DICIPL(ipl)	(0x0000000f & ~(ipl))

#define	GIC_NIRQ_REG	4	/* Number of IRQ of earch registers */
#define	GIC_BIND_IDX(x)	(x - IRQ_GIC_START)
#define	GIC_BIND_UNINIT	(-1)
/* calculate target register address of specified IRQ */
#define GIC_IRQ_TARGET_ADDR(irq)	(char *)(MPCORE_DIST_VADDR( \
					TARGET + ((irq / GIC_NIRQ_REG) * 4)) \
					+ (irq % GIC_NIRQ_REG))

#if	CPUSET_WORDS <= 1
/* remove unusable CPUs from cpuset */
#define	GIC_MASK_CPUSET(cpuset)		((cpuset) &= mp_cpus)
#else	/* CPUSET_WORDS <= 1 */
	/* explicitly error occurs */
	please fix IPI target mask (NCPU is larger than 32. is that true?)
#endif	/* CPUSET_WORDS <= 1 */

extern struct av_head autovect[];

static unsigned int gic_maxirq;
static processorid_t *gic_bind_table;
static cpu_t *gic_next_bind_cpu;	/* For round robin assignment */

#define	GIC_INIT_START()	writel(0, MPCORE_DIST_VADDR(CTRL))
#define	GIC_INIT_END()		writel(1, MPCORE_DIST_VADDR(CTRL))


void
gic_dist_init(void)
{
	unsigned int i, j;
#ifndef MARINE4_PMUIRQ_ERRATA
	static const int gtol_irq_table[] = {
		IRQ_PMU_CPU0,
		IRQ_PMU_CPU1,
		IRQ_PMU_CPU2,
		IRQ_PMU_CPU3
	};
#endif /* !MARINE4_PMUIRQ_ERRATA */

	/* Disable GIC. */
	GIC_INIT_START();

	/* How many interrupts are supported? */
	/* First, Check IT lines number. */
	gic_maxirq = readl(MPCORE_DIST_VADDR(CTRL_TYPE)) & 0x1f;
	/* 
	 * IRQ0-31 are reserved for software and MP11 CPU private
	 * interrupts, so gic_maxirq=((IT lines number) + 1) * 32.
	 */
	gic_maxirq = (gic_maxirq + 1) * 32;
	BOOT_ALLOC(gic_bind_table, processorid_t *,
		   (gic_maxirq - IRQ_GIC_START) * sizeof(processorid_t),
		   BO_NO_ALIGN, "Failed to allocate gic_bind_table");
	for (i = IRQ_GIC_START; i < gic_maxirq; i++) {
		gic_bind_table[GIC_BIND_IDX(i)] = GIC_BIND_UNINIT;
	}

	gic_next_bind_cpu = CPU_GLOBAL;

	/* 
	 * Initialize the interrupt configuration registers.  
	 * The interrupt line is level high active, and it uses
	 * the N-N software model.
	 */	
	for (i = IRQ_GIC_START; i < gic_maxirq; i += 16) {
		writel(0, MPCORE_DIST_VADDR(CONFIG) + i * 4 / 16);
	}

	/*
	 * Initialize the interrupt priority registers.
	 * Set the same priority level on all interrupts.
	 * highest priority(0x0), lowest priority(0xf)
	 */
	for (i = 0; i < gic_maxirq; i += 4) {
		writel(0xf0f0f0f0, MPCORE_DIST_VADDR(PRI) + i);
	}

	/* Disable all interrupts. */
	for (i = 0; i < gic_maxirq; i+=32) {
		writel(0xffffffff, MPCORE_DIST_VADDR(ENABLE_CLR) + i * 4 / 32);
	}

	/* Enable GIC. */
	GIC_INIT_END();
}

/*
 * Initialize CPU Interface.
 */
void
gic_cpuif_init(void)
{
	/* 
	 * Initialize the priority mask register of CPU interface.
	 * Set interrupt priority mask to 0xf. bit[7:4].
	 */
	writel(0xf0, MPCORE_CPUIF_VADDR(PRIMASK));

	/* Enable CPU interface. */
	writel(1, MPCORE_CPUIF_VADDR(CTRL));

}

/*
 * Mask IRQ.
 */
void
gic_mask_irq(uint32_t irq)
{
	uint32_t mask = IRQ_MASK_VALUE(irq);
	writel(mask, MPCORE_DIST_VADDR(ENABLE_CLR) + IRQ_MASK_OFFSET(irq));
}

/*
 * Unmask IRQ.
 */
void
gic_unmask_irq(uint32_t irq)
{
	uint32_t mask = IRQ_MASK_VALUE(irq);
	writel(mask, MPCORE_DIST_VADDR(ENABLE_SET) + IRQ_MASK_OFFSET(irq));
}

/*
 * Acknowledge IRQ.
 */
void 
gic_ack_irq(uint32_t irq)
{
	writel(irq, MPCORE_CPUIF_VADDR(EOI));
}


/*
 * Mask all interrupts below or equal given ipl.
 */
static inline void
gic_prio_mask(uint32_t ipl)
{
	uint32_t mask = (IPL_TO_DICIPL(ipl) << 4);
	writel(mask, MPCORE_CPUIF_VADDR(PRIMASK));
}

/*
 * Set IPL at given IRQ.
 */
void
gic_set_ipl(uint32_t irq, uint32_t ipl)
{
	uint32_t index, offset;
	uint32_t mask, r, clear_bits;

	index = irq / 4;	
	offset = irq % 4;

	r = readl(MPCORE_DIST_VADDR(PRI) + index * 4);
	clear_bits = ~(0xff << (offset * 8));
	r &= clear_bits;
	mask = ((IPL_TO_DICIPL(ipl) << 4) << (offset * 8));
	mask |= r;
	writel(mask, MPCORE_DIST_VADDR(PRI) + index * 4);
}

/*
 * Trigger an interrupt(identified with its ID) to a list of MP11 CPUs.
 *   irq     : Interrupt ID.
 *   cpulist : CPU targets list.
 */
void
gic_softintr(uint32_t irq, uint32_t cpulist)
{
	uint32_t val = ((cpulist & 0xff) << 16) | irq;
	writel(val, MPCORE_DIST_VADDR(SOFTINT));
}

/*
 * void
 * gic_send_ipi(cpuset_t cpuset, uint32_t irq)
 *	Generates an Inter Processor Interrupt to another CPU.
 *
 * Calling/Exit State:
 *	cpuset: Set of target CPUs
 *	irq:    Interrupt ID
 *	return: none
 *
 * Description:
 *	Send IPI to specified CPUs.
 *	This function can not send IPI to other CPUs that runs other OS.
 *
 * Remarks:
 *	This function is used for inner-OS communication.
 */
void
gic_send_ipi(cpuset_t cpuset, uint32_t irq)
{
	/* send IPI to CPUs that runs our OS instead of other OS */
	GIC_MASK_CPUSET(cpuset);
	gic_send_ipi_without_mask(cpuset, irq);
}

/*
 * void
 * gic_send_ipi_without_mask(cpuset_t cpuset, uint32_t irq)
 *	Generates an Inter Processor Interrupt to another CPU without
 *	restriction.
 *
 * Calling/Exit State:
 *	cpuset: Set of target CPUs
 *	irq:    Interrupt ID
 *	return: none
 *
 * Description:
 *	Send IPI to specified CPUs.
 *	There is no guard to prevent to send IPI to CPUs that runs
 *	other OS.
 *
 * Remarks:
 *	This function is used for both of inner-OS communication and
 *	inter-OS communication.
 */
void
gic_send_ipi_without_mask(cpuset_t cpuset, uint32_t irq)
{
	uint32_t	x;

	/*  Disable global IRQ */
	x = DISABLE_IRQ_SAVE();

	/* Trigger a Soft interrupt to a cpu */
	gic_softintr(irq, (uint32_t)cpuset);

	/* Enable global IRQ */
	RESTORE_INTR(x);
}

int
setlvl(int irq)
{
	int new_ipl;
	/* 
	 * Get a new interrupt priority level.  
	 * In registration of the H/W interrupt handler, it is
	 * guaranteed to have same IPL for the same IRQ number
	 * interrupt.  So, we can get a new IPL from
	 * avh_hi_pri(==avh_lo_pri) at given irq number.
	 */
	new_ipl = (int)autovect[irq].avh_hi_pri;

	if (new_ipl != 0) {
		/* 
		 * Mask all interrupts equal to and below the new interrupt
		 * priority level.
		 */
		gic_prio_mask((uint32_t)new_ipl);
	}

	return (new_ipl);

}

/* Set intr priority to specified level. */
void
setlvlx(int ipl)
{
	gic_prio_mask((uint32_t)ipl);
}

/* Set IPL at given IRQ. */
int
gic_addspl(int irq, int ipl, int min_ipl, int max_ipl)
{
	cpu_t	*bind_cpu;
	processorid_t bind_cpu_id;

	if (irq < 0 || irq > gic_maxirq) {
		return (-1);
	}

	if ((irq >= IRQ_GIC_START) && 
	    (gic_bind_table[GIC_BIND_IDX(irq)] == GIC_BIND_UNINIT)) {

		/*
		 * Choose cpu that this irq should be bound to.
		 */
		mutex_enter(&cpu_lock);
		if ((bind_cpu_id = irq_to_bound_cpuid(irq)) == -1) {
			bind_cpu = cpu_intr_next(gic_next_bind_cpu);
			if (bind_cpu) {
				gic_next_bind_cpu = bind_cpu;
			}

			ASSERT(gic_next_bind_cpu);
			bind_cpu_id = gic_next_bind_cpu->cpu_id;
			gic_bind_table[GIC_BIND_IDX(irq)] = bind_cpu_id;
		}
		gic_bind_intr(irq, bind_cpu_id);
		mutex_exit(&cpu_lock);
	}

	gic_set_ipl((uint32_t)irq, (uint32_t)ipl);
	/* Unmask IRQ */
	gic_unmask_irq((uint32_t)irq);

	return (0);
}

/* For given IRQ, set default priority level. */
int
gic_delspl(int irq, int ipl, int min_ipl, int max_ipl)
{

	if (irq < 0 || irq > gic_maxirq) {
		return (-1);
	}

	if (autovect[irq].avh_hi_pri == 0) {
		/*
		 * No handlers exist for this irq. For this irq, mask and
		 * set default priority (==lowest priority).  IPL0 will be
		 * converted into GIC-IPL(==0x0f) in gic_set_ipl().
		 */
		gic_mask_irq((uint32_t)irq);
		gic_set_ipl((uint32_t)irq, (uint32_t)0x0);

		if (irq >= IRQ_GIC_START) {
			gic_bind_table[GIC_BIND_IDX(irq)] = GIC_BIND_UNINIT;
			/*
			 * This irq cannot raise until a handler for this
			 * irq is registered. gic_bind_intr() does not have
			 * to be called.
			 */
		}
	}

	return (0);
}

/* Check pending register at given IRQ */
int
gic_get_pending(int irq)
{
	int      pending;
	uint32_t r;

	r = readl(MPCORE_DIST_VADDR(PENDING_SET) + IRQ_MASK_OFFSET(irq));
	pending = (r & IRQ_MASK_VALUE(irq)) ? 1 : 0;

	return (pending);
}

/*
 * static uint_t
 * gic_find_irq(dev_info_t *rdip, int inum)
 *	convert an interrupt number to IRQ number.
 *	The interrupt number determines which interrupt spec will be
 *	returned if more than one exists.
 *
 *	Look into the parent private data area of the 'rdip' to find out
 *	the interrupt specification.  First check to make sure there is
 *	one that matchs "inumber" and then return a pointer to it.
 *
 *	Return NULL if validate interrupt number could not be found.
 *
 *	NOTE: This is needed for gic_intr_ops()
 */

static uint_t
gic_find_irq(dev_info_t *rdip, int inum)
{
	struct ddi_parent_private_data *pdp = ddi_get_parent_data(rdip);

	/* Validate the interrupt number */
	if (inum >= pdp->par_nintr)
		return (NULL);

	/* Get the IRQ number */
	return (((struct intrspec *)&pdp->par_intr[inum])->intrspec_vec);
}

/*
 * This function provides external interface to the nexus for all
 * functionalities related to the new DDI interrupt framework.
 *
 * Input:
 * dip     - pointer to the dev_info structure of the requested device
 * hdlp    - pointer to the internal interrupt handle structure for the
 *	     requested interrupt
 * intr_op - opcode for this call
 * result  - pointer to the integer that will hold the result to be
 *	     passed back if return value is GIC_SUCCESS
 *
 * Output:
 * return value is either GIC_SUCCESS or GIC_FAILURE
 */

int
gic_intr_ops(dev_info_t *dip, ddi_intr_handle_impl_t *hdlp,
    gic_intr_op_t intr_op, int *result)
{
	int		cap;
	int		new_priority;
	int		irq;
	struct autovec *vec;
	extern struct av_head autovect[];

	switch (intr_op) {
	case GIC_INTR_OP_GET_PENDING:
		/* convert an interrupt number to IRQ number */
		if (!(irq = gic_find_irq(dip, hdlp->ih_inum))) {
			return (GIC_FAILURE);
		}

		/* check set pending register */
		*result = gic_get_pending(irq);
		break;
	case GIC_INTR_OP_CLEAR_MASK:
		if (hdlp->ih_type != DDI_INTR_TYPE_FIXED) {
			return (GIC_FAILURE);
		}

		/* convert an interrupt number to IRQ number */
		if (!(irq = gic_find_irq(dip, hdlp->ih_inum))) {
			return (GIC_FAILURE);
		}
		/* unmask irq */
		gic_unmask_irq(irq);
		break;
	case GIC_INTR_OP_SET_MASK:
		if (hdlp->ih_type != DDI_INTR_TYPE_FIXED) {
			return (GIC_FAILURE);
		}

		/* convert an interrupt number to IRQ number */
		if (!(irq = gic_find_irq(dip, hdlp->ih_inum))) {
			return (GIC_FAILURE);
		}

		/* mask irq */
		gic_mask_irq(irq);
		break;
	case GIC_INTR_OP_GET_CAP:
		cap = DDI_INTR_FLAG_PENDING;
		if (hdlp->ih_type == DDI_INTR_TYPE_FIXED) {
			cap |= DDI_INTR_FLAG_MASKABLE;
		}
		*result = cap;
		break;
	case GIC_INTR_OP_GET_SHARED:
		if (hdlp->ih_type != DDI_INTR_TYPE_FIXED) {
			return (GIC_FAILURE);
		}

		/* convert an interrupt number to IRQ number */
		if (!(irq = gic_find_irq(dip, hdlp->ih_inum))) {
			return (GIC_FAILURE);
		}

		/* This IRQ is shared ? */
		vec = (&autovect[irq])->avh_link;
		if (vec->av_link) {
			*result = 1;
		} else {
			*result = 0;
		}
		break;
	case GIC_INTR_OP_SET_PRI:
		/* convert an interrupt number to IRQ number */
		if (!(irq = gic_find_irq(dip, hdlp->ih_inum))) {
			return (GIC_FAILURE);
		}

		/* try the new value */
		new_priority = *(int *)result;

		/* Check new_priority value */
		if (new_priority < 1 || 15 < new_priority) {
			return (GIC_FAILURE);
		}

		/* Set IPL at given IRQ */
		gic_set_ipl(irq, new_priority);
		break;
	case GIC_INTR_OP_SET_CAP:
	default:
		return (GIC_FAILURE);
	}
	return (GIC_SUCCESS);
}

/*
 * void
 * gic_preshutdown(int cmd, int fcn)
 *	Called early in shutdown whilst we can still access
 *	filesystems to do things like loading modules which will be
 *	required to complete shutdown after filesystems are all
 *	unmounted.
 *
 * Calling/Exit State:
 *	none
 */
/*ARGSUSED*/
void
gic_preshutdown(int cmd, int fcn)
{
	/* 
	 * If you have anything to do in that case mentioned above,
	 * you should implement here.
	 */
	return;
}

/*
 * void
 * gic_shutdown(int cmd, int fcn)
 *	disable interrupts handled by us
 *
 * Calling/Exit State:
 *	cmd: dummy for argument comatibility
 *	fcn: dummy for argument comatibility
 *
 * Description:
 *	disable interrupt delivered to CPUs that we used.
 *	this is a part of system shutdown sequence.
 */
/*ARGSUSED*/
void
gic_shutdown(int cmd, int fcn)
{
	unsigned int i;
	volatile char	*addr;

	/* Disable all interrupts. */
	for (i = 0; i < gic_maxirq; i+=32) {
		writel(0xffffffff, MPCORE_DIST_VADDR(ENABLE_CLR) + i * 4 / 32);
	}
}

/*
 * void
 * gic_bind_intr(int irq, processorid_t bind_cpu)
 *	Bind irq to bind_cpu. gic_bind_intr() writes to Interrupt CPU target
 *	registers in order to change cpu which receives interrupt specified
 *	by irq. The caller must hold cpu_lock.
 */
void
gic_bind_intr(int irq, processorid_t bind_cpu)
{
	volatile char	*addr;

	ASSERT(MUTEX_HELD(&cpu_lock));

	addr = GIC_IRQ_TARGET_ADDR(irq);
	*addr = 1 << bind_cpu;
}

/*
 * void
 * gic_enable_intr(processorid_t cpun)
 *	gic_enable_intr() is called when enabling interrupts on the CPU
 *	specified by cpun. e.g. when cpu_online() is called.
 */
void
gic_enable_intr(processorid_t cpun)
{
	int	irq;

	ASSERT(MUTEX_HELD(&cpu_lock));

	for (irq = IRQ_GIC_START; irq < gic_maxirq; irq++) {
		/*
		 * Rebind if this intr should be bound to this cpu.
		 */
		if (gic_bind_table[GIC_BIND_IDX(irq)] == cpun) {
			gic_bind_intr(irq, cpun);
		}
	}
}

/*
 * int
 * gic_disable_intr(processorid_t cpun)
 *	gic_disable_intr() is called when disabling interrupts on the CPU
 *	specified by cpun. e.g. when cpu_offline() is called.
 */
int
gic_disable_intr(processorid_t cpun)
{
	cpu_t		*cp;
	uint32_t	target, clear_cpu;
	int		i, j;

	/*
	 * If there are interrupts that are bound to this cpu, migrate to
	 * other cpu.
	 */

	ASSERT(MUTEX_HELD(&cpu_lock));

	cp = cpu_intr_next(cpu[cpun]);
	ASSERT(cp);

	if (gic_next_bind_cpu == cpu[cpun]) {
		gic_next_bind_cpu = cp;
	}

	/*
	 * IRQ 0-31 are private interrupts of each MP11 CPUs. 
	 * these interrupts cannot be migrated to any other CPUs.
	 */
	for (i = IRQ_GIC_START; i < gic_maxirq; i += GIC_NIRQ_REG) {
		target = readl(MPCORE_DIST_VADDR(TARGET +
						 ((i / GIC_NIRQ_REG) * 4)));

		for (j = 0; j < GIC_NIRQ_REG; j++) {
			clear_cpu = (1 << cpun) << (j * 8);

			if (target & clear_cpu) {
				if (irq_to_bound_cpuid(i + j) == -1) {
					gic_bind_intr((i + j), cp->cpu_id);
				}
			}
		}
	}

	return (GIC_SUCCESS);
}
