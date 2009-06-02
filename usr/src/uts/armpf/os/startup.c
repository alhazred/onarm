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

#pragma ident	"@(#)startup.c	1.212	06/04/18 SMI"

#ifndef	STATIC_UNIX
#error	"Port me!"
#endif	/* !STATIC_UNIX */

#include <sys/types.h>
#include <sys/t_lock.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/mman.h>
#include <sys/vm.h>
#include <sys/conf.h>
#include <sys/avintr.h>
#include <sys/autoconf.h>
#include <sys/disp.h>
#include <sys/class.h>
#include <sys/prom_debug.h>
#include <sys/cachectl.h>
#include <sys/cache_l220.h>

#include <sys/privregs.h>

#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/kmem.h>
#include <sys/kstat.h>

#include <sys/reboot.h>

#include <sys/cred.h>
#include <sys/vnode.h>
#include <sys/file.h>

#include <sys/procfs.h>

#include <sys/vfs.h>
#include <sys/cmn_err.h>
#include <sys/utsname.h>
#include <sys/debug.h>
#include <sys/kdi.h>

#include <sys/dumphdr.h>
#include <sys/bootconf.h>
#include <sys/varargs.h>
#include <sys/promif.h>
#include <sys/modctl.h>		/* for "procfs" hack */
#include <sys/modstatic.h>

#include <sys/sunddi.h>
#include <sys/sunndi.h>
#include <sys/ndi_impldefs.h>
#include <sys/ddidmareq.h>
#include <sys/ddi_timer.h>
#include <sys/regset.h>
#include <sys/clock.h>
#include <sys/pte.h>
#include <sys/stack.h>
#include <sys/trap.h>
#include <vm/anon.h>
#include <vm/as.h>
#include <vm/page.h>
#include <vm/seg.h>
#include <vm/seg_dev.h>
#include <vm/seg_kmem.h>
#include <vm/seg_kpm.h>
#include <vm/seg_map.h>
#include <vm/seg_vn.h>
#include <vm/seg_kp.h>
#include <sys/memnode.h>
#include <vm/vm_dep.h>
#include <sys/thread.h>
#include <sys/sysconf.h>
#include <sys/vm_machparam.h>
#include <sys/archsystm.h>
#include <sys/machsystm.h>
#include <vm/hat.h>
#include <vm/hat_arm.h>
#include <vm/hat_machdep.h>
#include <sys/smp_impldefs.h>
#include <sys/clconf.h>
#include <sys/kobj.h>
#include <sys/kobj_lex.h>
#include <sys/cpc_impl.h>
#include <sys/platform.h>
#include <asm/cpufunc.h>
#include <sys/dumpadm.h>
#include <sys/dumphdr.h>
#include <sys/fs/swapnode.h>
#include <sys/xramdev_impl.h>
#include <sys/mach_boot.h>
#include <vm/kvlayout.h>

/*
 * XXX make declaration below "static" when drivers no longer use this
 * interface.
 */

/*
 * segkp
 */
extern int segkp_fromheap;

#if	SEGKP_SIZE != 0

#if	(SEGKP_SIZE & MMU_PAGEOFFSET) != 0
#error	SEGKP_SIZE is not page-aligned.
#endif	/* (SEGKP_SIZE & MMU_PAGEOFFSET) != 0 */

caddr_t	segkp_base;
size_t	segkp_size;

#endif	/* SEGKP_SIZE != 0 */

#if	SEGZIO_SIZE != 0

/*
 * ZFS zio segment. This allows us to exclude large portions of ZFS data that
 * gets cached in kmem caches on the heap. If SEGZIO_SIZE is definde as zero,
 * zio buffers are allocated from the kernel heap, otherwise they are allocated
 * from their own segment.
 */

#if	(SEGZIO_SIZE & MMU_PAGEOFFSET) != 0
#error	SEGZIO_SIZE is not page-aligned.
#endif	/* (SEGZIO_SIZE & MMU_PAGEOFFSET) != 0 */

caddr_t	segzio_base;
size_t	segzio_size;

#endif	/* SEGZIO_SIZE != 0 */

/*
 * taskq parameters for ddi cyclic timer
 */
extern int timer_taskq_num;
extern int timer_taskq_min_num;
extern int timer_taskq_max_num;
#if	(TIMER_TASKQ_NUM < TIMER_TASKQ_MIN_NUM) ||	\
	(TIMER_TASKQ_NUM > TIMER_TASKQ_MAX_NUM)
#error	TIMER_TASKQ_NUM is out of range.
#endif

static void kvm_init(void);
static void startup_init(void);
static void startup_memlist(void);
static void startup_modules(void);
static void startup_vm(void);
static void startup_end(void);
static void get_system_configuration(void);

extern void prdc_timer_init(void);

/*
 * Declare these as initialized data so we can patch them.
 */
pgcnt_t physmem = 0;	/* memory size in pages, patch if you want less */
pgcnt_t obp_pages;	/* Memory used by PROM for its text and data */

char *kobj_file_buf;
int kobj_file_bufsize;	/* set in /etc/system */

/*
 * Configuration parameters set at boot time.
 */

caddr_t econtig;		/* end of first block of contiguous kernel */

#ifdef	XRAMDEV_CONFIG
caddr_t	e_kstatic;		/* End of kernel static area. */
#endif	/* XRAMDEV_CONFIG */

struct bootops		*bootops = 0;	/* passed in from boot */
struct boot_syscalls	*sysp;		/* passed in from boot */

/*
 * VM data structures
 */
long	page_hashsz;		/* Size of page hash table (power of two) */
page_t	**page_hash;		/* Page hash table */
pad_mutex_t	*pse_mutex;	/* Locks protecting pp->p_selock */
size_t	pse_table_size;		/* Number of mutexes in pse_mutex[] */
int	pse_shift;		/* log2(pse_table_size) */
struct seg ktextseg;		/* Segment used for kernel executable image */
struct seg kmodtextseg;		/* Segment used for module text */
struct seg kpseg;		/* Segment used for pageable kernel virt mem */
struct seg kmapseg;		/* Segment used for generic kernel mappings */
struct seg kdebugseg;		/* Segment used for the kernel debugger */

struct seg *segkmap = &kmapseg;	/* Kernel generic mapping segment */
struct seg *segkp = &kpseg;	/* Pageable kernel virtual memory segment */

struct seg *segkpm = NULL;	/* Unused on ARM */

caddr_t s_text;		/* start of kernel text segment */
caddr_t e_text;		/* end of kernel text segment */
caddr_t s_data;		/* start of kernel data segment */
caddr_t e_data;		/* end of kernel data segment */

/*
 * VA range available to the debugger
 */
const caddr_t	kdi_segdebugbase = (const caddr_t)SEGDEBUGBASE;
const size_t	kdi_segdebugsize = SEGDEBUGSIZE;

/* Keep KERNELPHYSBASE value in memory. */
const uintptr_t	kernelphysbase = KERNELPHYSBASE;

/* Keep backup DRAM range in memory. */
const uintptr_t	backup_dram_base = ARMPF_BACKUP_DRAM_PADDR;
const size_t	backup_dram_size = ARMPF_BACKUP_DRAM_SIZE;

struct memlist	*phys_install;	/* Total installed physical memory */
struct memlist	*phys_avail;	/* Total available physical memory */

static int	memseg_curindex;

/*
 * Allocate new memlist and memseg from reserved area.
 * No lock is required because only startup() uses this macro.
 */
#define	MEMSEG_ALLOC(mlp, msegp)			\
	do {						\
		int	__idx = memseg_curindex;	\
							\
		ASSERT(__idx < ARMPF_MAX_MEMSEG_COUNT);	\
		(mlp) = &phys_avail_buf[__idx];		\
		(msegp) = &memseg_buf[__idx];		\
		memseg_curindex = __idx + 1;		\
	} while (0)

static struct memlist	phys_avail_buf[ARMPF_MAX_MEMSEG_COUNT];
static struct memseg	memseg_buf[ARMPF_MAX_MEMSEG_COUNT];

/* Flags for page_chunk_init(). */
#define	PGCHUNK_NO_PAGEALLOC	0x1	/* don't allocate struct page chunk */

/* Parameters used to determine size of available physical memory. */
#define	MAX_PAGE_INIT_LOOP		256
#define	MAX_PAGE_INIT_CONVERGE_LOOP	4

/* Calculate number of struct page */
#define	PAGE_NPAGES(npgs, pmem, notavail)				\
	do {								\
		if ((pmem) <= (notavail)) {				\
			panic("Failed to reserve memory for struct page: " \
			      "physmem=0x%lx, notavail=0x%lx",		\
			      (pmem), (notavail));			\
		}							\
		(npgs) = (pmem) - (notavail);				\
	} while (0)

#ifndef	XRAMDEV_CONFIG
/* Page list that contains CTF data */
static page_t	*unix_ctf_pages;

/* Base physical address of CTF data. */
static caddr_t	ctf_start_paddr;
#endif	/* !XRAMDEV_CONFIG */

/* Address range of CTF data */
extern char	*ctf_start_address;
extern char	*ctf_end_address;

/* Initialize page structures */
static void	page_init(void);
static page_t	*page_chunk_init(pfn_t pfn, pgcnt_t npages, page_t *pp,
				 uint_t flags, struct memlist ***availppp,
				 struct memseg ***msegppp);

/* Initialize unix CTF data */
static void	unix_ctf_init(void);

static caddr_t	startup_malloc(struct bootops *bops, caddr_t virthint,
			       size_t size, int align);
static caddr_t	startup_page_create(caddr_t vaddr, size_t size,
				    const boolean_t dopanic);

static void	hw_serial_init(void);

static void	swap_conf(void);

size_t		segmapsize;
uintptr_t	segkmap_start;
int		segmapfreelists;

/* Address range for module text. */
uintptr_t	heaptext_base;
size_t		heaptext_size;

/*
 * Enable some debugging messages concerning memory usage...
 *
 * XX64 There should only be one print routine once memlist usage between
 * vmx and the kernel is cleaned up and there is a single memlist structure
 * shared between kernel and boot.
 */
static void
print_boot_memlist(char *title, struct memlist *mp)
{
	prom_printf("MEMLIST: %s:\n", title);
	while (mp != NULL)  {
		prom_printf("\tAddress 0x%" PRIx64 ", size 0x%" PRIx64 "\n",
		    mp->address, mp->size);
		mp = mp->next;
	}
}

int	prom_debug;
lock_t	prom_debug_lock;

/*
 * Our world looks like this at startup time.
 *
 * Bootloader loads the whole kernel image at 0xc0002000.
 * This address is fixed in the binary at link time.
 *
 */

/*
 * Machine-dependent startup code
 */
void
startup(void)
{
	kpm_enable = 0;

	startup_init();
	startup_memlist();
	startup_modules();
	startup_vm();
	startup_end();
}

static void
startup_init()
{
	extern void plat_irq_init(void);

	PRM_POINT("startup_init() starting...");

	/*
	 * Complete the extraction of cpuid data
	 */
	cpuid_pass2(CPU);

#ifdef DEBUG
	(void) check_boot_version(BOP_GETVERSION(bootops));
#endif /* DEBUG */

	/*
	 * Check for prom_debug in boot environment
	 */
	if (BOP_GETPROPLEN(bootops, "prom_debug") >= 0) {
		++prom_debug;
		PRM_POINT("prom_debug found in boot enviroment");
	}

	/*
	 * Collect node, cpu and memory configuration information.
	 */
	get_system_configuration();

	/*
	 * Initialize interrupt controller.
	 */
	plat_irq_init();

	/*
	 * SCU Counter start.
	 */
	scucnt_init();

	PRM_POINT("startup_init() done");
}

/*
 * static void
 * startup_memlist(void)
 *	Initialize kernel static data.
 *
 *	The purpose of startup_memlist() is to get the system to the
 *	point where it can use kmem_alloc() that operate correctly
 *	relying on BOP_ALLOC().
 *
 *	At this point, the kernel still uses startup page table.
 *	startup_memlist() builds a new kernel page table, and
 *	swtiches to it.
 */
static void
startup_memlist(void)
{
	int	memblocks;
	caddr_t	pagecolor_mem;
	size_t	pagecolor_memsz;
	caddr_t	page_ctrs_mem;
	size_t	page_ctrs_size;
	size_t	pse_table_alloc_size;
	int	i;
	caddr_t	base;
	caddr_t	ksegend;
	extern void	startup_build_mem_nodes(struct memlist *);
	extern void	page_coloring_init(void);
	extern caddr_t	page_coloring_alloc(caddr_t base);
	extern caddr_t	vm_startup(void);
	extern void	install_exception_vectors(caddr_t va);
	extern void	page_set_colorequiv_arr(void);

	PRM_POINT("startup_memlist() starting...");

	/*
	 * Examine the boot loaders physical memory map to find out:
	 * - total memory in system - physinstalled
	 * - the max physical address - physmax
	 * - the number of segments the intsalled memory comes in
	 */
	if (prom_debug) {
		print_boot_memlist("boot physinstalled",
				   bootops->boot_mem->physinstalled);
	}
	installed_top_size(bootops->boot_mem->physinstalled, &physmax,
			   &physinstalled, &memblocks);
	PRM_DEBUG(physmax);
	PRM_DEBUG(physinstalled);
	PRM_DEBUG(memblocks);
	ASSERT(memblocks == ARMPF_SDRAM_COUNT);

	startup_build_mem_nodes(bootops->boot_mem->physinstalled);

	/* Initialize MMU related data. */
	hat_mmu_init();

	/* Use memlist in bootops for phys_install. */
	phys_install = bootops->boot_mem->physinstalled;
	ASSERT(phys_install);

	/*
	 * If physmem is patched to be non-zero, check whether its value
	 * is valid.
	 */
	if (physmem == 0 || physmem > physinstalled) {
		physmem = physinstalled;
	}
	else if (physmem < PLATFORM_MIN_PHYSMEM) {
		physmem = PLATFORM_MIN_PHYSMEM;
	}

	/* Early initialization of HAT layer. */
	hat_bootstrap();

	/* Initialize page coloring. */
	page_coloring_init();

	/*
	 * Allocate page freelists.
	 */

	/* fpc_mutex, cpc_mutex */
	pagecolor_memsz = (max_mem_nodes * sizeof(kmutex_t) * NPC_MUTEX * 2);

	/* page_freelists */
	pagecolor_memsz += (sizeof(page_t **) * mmu_page_sizes);
	for (i = 0; i < mmu_page_sizes; i++) {
		int	colors = page_get_pagecolors(i);
		pagecolor_memsz += (sizeof(page_t *) * colors);
	}

	/* page_cachelists */
	pagecolor_memsz += (page_colors * sizeof (page_t *));
	PRM_DEBUG(pagecolor_memsz);

	BOOT_ALLOC(pagecolor_mem, caddr_t, pagecolor_memsz, BO_NO_ALIGN,
		   "Failed to allocate page coloring data");
	FAST_BZERO_ALIGNED(pagecolor_mem, pagecolor_memsz);
	PRM_DEBUG(pagecolor_mem);

	if (page_coloring_alloc(pagecolor_mem) >
	    pagecolor_mem + pagecolor_memsz) {
		panic("page_coloring_setup: buffer overflow");
	}

	/* Allocate per page size free list counters. */
	page_ctrs_size = page_ctrs_sz();
	PRM_DEBUG(page_ctrs_size);
	BOOT_ALLOC(page_ctrs_mem, caddr_t, page_ctrs_size, BO_NO_ALIGN,
		   "Failed to allocate page free list counters");
	FAST_BZERO_ALIGNED(page_ctrs_mem, page_ctrs_size);
	if (page_ctrs_alloc(page_ctrs_mem) > page_ctrs_mem + page_ctrs_size) {
		panic("page_ctrs_alloc: buffer overflow");
	}

	/*
	 * Allocate the array that protects pp->p_selock.
	 */
	pse_shift = size_pse_array(physmem, max_ncpus);
	pse_table_size = 1 << pse_shift;
	pse_table_alloc_size = pse_table_size * sizeof (pad_mutex_t);
	BOOT_ALLOC(pse_mutex, pad_mutex_t *, pse_table_alloc_size,
		   BO_NO_ALIGN, "Failed to allocate pse_mutex array");
	PRM_DEBUG(pse_shift);
	PRM_DEBUG(pse_mutex);
	FAST_BZERO_ALIGNED(pse_mutex, pse_table_alloc_size);

	/*
	 * Prepare L2PT for high vector page.
	 * This L2PT is also used to map PCI config and I/O space.
	 */
	hatpt_boot_linkl2pt(ARM_VECTORS_HIGH, MMU_PAGESIZE, B_TRUE);

	/*
	 * Initialize page arrays.
	 */
	page_init();

	/*
	 * Initialize L1 Cache.
	 * We can't use BOP_ALLOC() from here.
	 */
	cache_init();

	/* Call platform-specific VM initializer. */
	ksegend = vm_startup();

	/* We can't use BOP_ALLOC() from here. */

	/* Switch to the official L1PT. */
	PRM_POINT("Calling hat_kernpt_init()...");
	hat_kernpt_init();
	PRM_POINT("hat_kernpt_init() done");

	/*
	 * All VA == PA mappings have been invalidated.
	 */

	/* Install trap vectors. */
	fast_bzero((void *)ARM_VECTORS_HIGH, MMU_PAGESIZE);
	PRM_POINT("Calling install_exception_vectors()...");
	install_exception_vectors((caddr_t)ARM_VECTORS_HIGH);
	PRM_POINT("install_exception_vectors() done");

	/* Reserve space for module text. */
	HEAPTEXT_RESERVE(heaptext_base, heaptext_size);
	PRM_DEBUG(heaptext_base);
	PRM_DEBUG(heaptext_size);

	/* Reserve virtual address for kernel L2PT. */
	base = hat_plat_reserve_space((caddr_t)HEAPTEXT_ADDR_LIMIT);
	PRM_DEBUG(base);

	/* Initialize L220 L2 cache. */
	CACHE_L220_INIT();

#if	SEGKP_SIZE == 0
	/* Use kernelheap for segkp. */
	segkp_fromheap = 1;
#endif	/* SEGKP_SIZE == 0 */

	/* We have to leave room to place segmap below the kernel heap. */
	SEGMAP_RESERVE(base, segkmap_start, segmapsize);
	PRM_DEBUG(segkmap_start);
	PRM_DEBUG(segmapsize);

	/* Prepare mappings for segkmap. */
	hat_kmap_init(segkmap_start, segmapsize);

	/*
	 * Setup kernelheap range with one empty page as red zone.
	 */
	kernelheap = KERNELHEAP_FIXUP(segkmap_start + segmapsize);

#if	SEGKP_SIZE != 0
	/* Determine address range of segkp space. */
	segkp_base = ksegend - SEGKP_SIZE;
	segkp_size = SEGKP_SIZE;
	ksegend = segkp_base - MMU_PAGESIZE;
	PRM_DEBUG(segkp_base);
	PRM_DEBUG(segkp_size);
#endif	/* SEGKP_SIZE != 0 */

#if	SEGZIO_SIZE != 0

	/*
	 * Determine address range of segzio space.
	 *
	 * segzio is used for ZFS cached data. It uses a distinct VA
	 * segment (from kernel heap) so that we can easily tell not to
	 * include it in kernel crash dumps. The trick is to give it lots
	 * of VA, but not constrain the kernel heap.
	 */
	segzio_base = ksegend - SEGZIO_SIZE;
	segzio_size = SEGZIO_SIZE;
	ksegend = segzio_base - MMU_PAGESIZE;
	PRM_DEBUG(segzio_base);
	PRM_DEBUG(segzio_size);
#endif	/* SEGZIO_SIZE != 0 */

	/* Rest of available space can be used for kernel heap. */
	ekernelheap = ksegend;

	PRM_DEBUG(kernelheap);
	PRM_DEBUG(ekernelheap);

	/*
	 * Reserve virtual space for xramfs devices.
	 * Note that xramdev_reserve_vaddr() will update ekernelheap.
	 */
	xramdev_impl_reserve_vaddr();
	PRM_DEBUG(ekernelheap);

	/*
	 * Update variables that were initialized with a value of
	 * KERNELBASE (in common/conf/param.c).
	 *
	 * XXX	The problem with this sort of hackery is that the
	 *	compiler just may feel like putting the const declarations
	 *	(in param.c) into the .text section.  Perhaps they should
	 *	just be declared as variables there?
	 */
	*(uintptr_t *)&_kernelbase = KERNELBASE;
	*(uintptr_t *)&_userlimit = USERLIMIT;
	*(uintptr_t *)&_userlimit32 = _userlimit;

	PRM_DEBUG(_kernelbase);
	PRM_DEBUG(_userlimit);
	PRM_DEBUG(_userlimit32);

	/* Install boot memory allocator used until VM initialization. */
	bootops->bsys_alloc = startup_malloc;
	hat_boot_finish();

	/*
	 * Initialize the kernel heap. Note 3rd argument must be > 1st.
	 */
	kernelheap_init(kernelheap, ekernelheap, kernelheap + MMU_PAGESIZE,
			NULL, NULL);
	PRM_DEBUG(ekernelheap);

	page_lock_init();	/* currently a no-op */

	/* Initialize CTF data for unix module. */
	unix_ctf_init();

	/*
	 * Create xramfs device mapping.
	 */
	xramdev_impl_mapinit();

	/*
	 * Initialize kernel memory allocator.
	 */
	kmem_init();

	/*
	 * Factor in colorequiv to check additional 'equivalent' bins
	 */
	page_set_colorequiv_arr();

	/* Initialize _kobj_printf. */
	kobj_static_printf_init();

	/*
	 * Initialize bp_mapin().
	 */
	bp_init(MMU_PAGESIZE, HAT_STORECACHING_OK);

	PRM_POINT("startup_memlist() done");
}

#define	CONFIG_SWAPFS_PARAM(param, var)				\
	do {							\
		if ((param) != 0 && (param) >= availrmem) {	\
			(var) = (availrmem * 3) >> 2;		\
		}						\
		else {						\
			(var) = (param);			\
		}						\
	} while (0)

static void
startup_modules(void)
{
	unsigned int i;
	extern int system_taskq_size;
	extern void prom_setup(void);

	PRM_POINT("startup_modules() starting...");

	/*
	 * Calculate default settings of system parameters based upon
	 * maxusers.
	 */
	maxusers = DEFAULT_MAXUSERS;
	param_calc(PLATFORM_MAX_NPROCS);

	mod_setup();

	/*
	 * Initialize system parameters.
	 */
	param_init();

	/*
	 * maxmem is the amount of physical memory we're playing with.
	 */
	maxmem = physmem;

	/*
	 * Initialize the hat layer.
	 */
	hat_init();

	/*
	 * Initialize segment management stuff.
	 */
	seg_init();

	if (modload("fs", "specfs") == -1)
		halt("Can't load specfs");

	if (modload("fs", "devfs") == -1)
		halt("Can't load devfs");

	if (modload("misc", "swapgeneric") == -1) {
		halt("Can't load swapgeneric");
	}

	(void) modloadonly("sys", "lbl_edition");

	dispinit();

	/* Initialse hw_serial[]. */
	hw_serial_init();

	/* Read cluster configuration data. */
	clconf_init();

	/*
	 * Create a kernel device tree. First, create rootnex and
	 * then invoke bus specific code to probe devices.
	 */
	setup_ddi();

	/*
	 * Fake a prom tree such that /dev/openprom continues to work
	 */
	prom_setup();

	/*
	 * Setup force loading of modules specified by boot option.
	 * Modules specified by "forceload-early" will be loaded here.
	 */
	mod_static_forceload_setup();

	/*
	 * Lets take this opportunity to load the root device.
	 */
	if (loadrootmodules() != 0) {
		panic("Can't load the root filesystem");
	}

	/*
	 * Initialize parameters for swapfs.
	 * SWAPFS_MINFREE, SWAPFS_DESFREE, and SWAPFS_RESERVE must be smaller
	 * than amount of physical memory. If larger than availrmem,
	 * we choose availrmem * 3/4.
	 */
	CONFIG_SWAPFS_PARAM(SWAPFS_MINFREE, swapfs_minfree);
	CONFIG_SWAPFS_PARAM(SWAPFS_DESFREE, swapfs_desfree);
	CONFIG_SWAPFS_PARAM(SWAPFS_RESERVE, swapfs_reserve);

	/* Set user-defined parameters. */
	system_taskq_size = PLATFORM_SYSTEM_TASKQ_SIZE;
	lotsfree = LOTSFREE;
	desfree = DESFREE;
	timer_taskq_num = TIMER_TASKQ_NUM;
	timer_taskq_min_num = TIMER_TASKQ_MIN_NUM;
	timer_taskq_max_num = TIMER_TASKQ_MAX_NUM;

	PRM_POINT("startup_modules() done");
}

static void
startup_vm(void)
{
	struct segmap_crargs a;
	pgcnt_t pages_left;
#ifndef	LPG_DISABLE
	extern int use_brk_lpg, use_stk_lpg;
#endif	/* !LPG_DISABLE */

	PRM_POINT("startup_vm() starting...");

	/*
	 * It is no longer safe to call BOP_ALLOC(), so make sure we don't.
	 */
	bootops->bsys_alloc = NULL;

	hat_cpu_online(CPU);

	/*
	 * Initialize VM system
	 */
	PRM_POINT("Calling kvm_init()...");
	kvm_init();
	PRM_POINT("kvm_init() done");

	/*
	 * Tell kmdb that the VM system is now working
	 */
	if (boothowto & RB_DEBUG) {
		kdi_dvec_vmready();
	}

	/*
	 * Mangle the brand string etc.
	 */
	cpuid_pass3(CPU);

	/*
	 * Now that we've got more VA, as well as the ability to allocate from
	 * it, tell the debugger.
	 */
	if (boothowto & RB_DEBUG) {
		kdi_dvec_memavail();
	}

	cmn_err(CE_CONT, "?mem = %luK (0x%lx)\n",
	    physinstalled << (MMU_PAGESHIFT - 10), ptob(physinstalled));

#ifndef	LPG_DISABLE
	/* Disable automatic large pages. */
	use_brk_lpg = 0;
	use_stk_lpg = 0;
#endif	/* !LPG_DISABLE */

	/*
	 * Initialize the segkp segment type.
	 */
	rw_enter(&kas.a_lock, RW_WRITER);

#if	SEGKP_SIZE == 0
	/*
	 * segkp space is put under the kernel heap.
	 * We need to set kernel as into seg structure before segkp_create()
	 * call.
	 */
	segkp->s_as = &kas;
#else	/* SEGKP_SIZE != 0 */
	/* Attach segkp segment. */
	if (seg_attach(&kas, segkp_base, segkp_size, segkp) < 0) {
		panic("cannot attach segkp");
		/* NOTREACHED */
	}
#endif	/* SEGKP_SIZE == 0 */


	if (segkp_create(segkp) != 0) {
		panic("startup: segkp_create failed");
		/* NOTREACHED */
	}
	PRM_DEBUG(segkp);

#if	SEGZIO_SIZE != 0
	/* Attach segzio segment. */
	if (seg_attach(&kas, segzio_base, segzio_size, &kzioseg) < 0) {
		panic("cannot attach segzio");
		/* NOTREACHED */
	}
	(void)segkmem_zio_create(&kzioseg);

	/* Create zio area covering new segment. */
	segkmem_zio_init(segzio_base, segzio_size);
#endif	/* SEGZIO_SIZE != 0 */

	/* We don't use kpm segment. */
	segmap_kpm = 0;

	/*
	 * Now create segmap segment.
	 */
	if (seg_attach(&kas, (caddr_t)segkmap_start, segmapsize, segkmap) < 0) {
		panic("cannot attach segkmap");
		/* NOTREACHED */
	}
	PRM_DEBUG(segkmap);

	a.prot = PROT_READ|PROT_WRITE;
	a.shmsize = 0;
	a.nfreelist = segmapfreelists;

	if (segmap_create(segkmap, (caddr_t)&a) != 0) {
		panic("segmap_create segkmap");
		/* NOTREACHED */
	}
	rw_exit(&kas.a_lock);

	hat_plat_cpu_init(CPU);

	segdev_init();
	PRM_POINT("startup_vm() done");
}

static void
startup_end(void)
{
	extern void	plat_rtc_init(void);

	PRM_POINT("startup_end() starting...");

	/*
	 * Perform tasks that get done after most of the VM
	 * initialization has been done but before the clock
	 * and other devices get started.
	 */
	kern_setup1();

	/*
	 * Configure the system.
	 */
	PRM_POINT("Calling configure()...");
	configure();		/* set up devices */
	PRM_POINT("configure() done");

	cpu_intr_init(CPU_GLOBAL, NINTR_THREADS);
	
	/* Initialize RTC. */
	armpf_rtc_init();

	mach_init();

	/*
	 * Perform CPC initialization for this CPU.
	 */
	kcpc_hw_init();

	/*
	 * We're done with bootops.  We don't unmap the bootstrap yet because
	 * we're still using bootsvcs.
	 */
	PRM_POINT("zeroing out bootops");
	bootops = (struct bootops *)NULL;

	PRM_POINT("Enabling interrupts");
	ENABLE_INTR();


	/* XXX to be moved later */
	(void)add_avsoftintr((void *)&softlevel1_hdl, 1, softlevel1,
			     "softlevel1", NULL, NULL);

	/*
	 * Initialize periodic timer.
	 */
	prdc_timer_init();

	PRM_POINT("startup_end() done");
}

extern char hw_serial[];

void
post_startup(void)
{
	/*
	 * Load all static-linked modules that have stub entries.
	 * This is required to load depended modules.
	 */
	mod_static_load_all(B_TRUE);

	/*
	 * Set the system wide, processor-specific flags to be passed
	 * to userland via the aux vector for performance hints and
	 * instruction set extensions.
	 */
	bind_hwcap();

	/*
	 * Startup memory scrubber.
	 */
	memscrub_init();

	/*
	 * Perform forceloading tasks for boot option.
	 * Note that we can't see forceloading in /etc/system because
	 * STATIC_UNIX environment doesn't support /etc/system.
	 */
	mod_static_forceload();

	/*
	 * ON4.0: Force /proc module in until clock interrupt handle fixed
	 * ON4.0: This must be fixed or restated in /etc/systems.
	 */
	(void) modload("fs", "procfs");

	maxmem = freemem;

	add_cpunode2devtree(CPU_GLOBAL);

	/* Configure default swap device. */
	swap_conf();
}

/*
 * Initialize the platform-specific parts of a page_t.
 */
void
add_physmem_cb(page_t *pp, pfn_t pnum)
{
	pp->p_pagenum = pnum;

	/*
	 * No need to zero other members because already zeroed out
	 * on allocation.
	 */
}

/*
 * Kernel VM initialization.
 */
static void
kvm_init(void)
{
	ASSERT((((uintptr_t)s_text) & MMU_PAGEOFFSET) == 0);

	/*
	 * Put the kernel segments in kernel address space.
	 */
	rw_enter(&kas.a_lock, RW_WRITER);
	as_avlinit(&kas);

	(void)seg_attach(&kas, s_text, KSTATIC_END - s_text, &ktextseg);
	(void)segkmem_create(&ktextseg);

	(void)seg_attach(&kas, (caddr_t)heaptext_base, heaptext_size,
			 &kmodtextseg);
	(void)segkmem_create(&kmodtextseg);

	(void)seg_attach(&kas, kernelheap, ekernelheap - kernelheap, &kvseg);
	(void)segkmem_create(&kvseg);

	(void)seg_attach(&kas, kdi_segdebugbase, kdi_segdebugsize, &kdebugseg);
	(void)segkmem_create(&kdebugseg);

	rw_exit(&kas.a_lock);
}

static void
get_system_configuration(void)
{
	char	prop[32];
	u_longlong_t lvalue;

	if ((BOP_GETPROPLEN(bootops, "segmapsize") > sizeof (prop)) ||
	    (BOP_GETPROP(bootops, "segmapsize", prop) < 0) ||
	    (kobj_getvalue(prop, &lvalue) == -1)) {
		segmapsize = SEGMAPDEFAULT;
	} else {
		segmapsize = (uintptr_t)lvalue;
	}

	if ((BOP_GETPROPLEN(bootops, "segmapfreelists") > sizeof (prop)) ||
	    (BOP_GETPROP(bootops, "segmapfreelists", prop) < 0) ||
	    (kobj_getvalue(prop, &lvalue) == -1)) {
		segmapfreelists = 0;	/* use segmap driver default */
	} else {
		segmapfreelists = (int)lvalue;
	}

	if ((BOP_GETPROPLEN(bootops, "physmem") <= sizeof (prop)) &&
	    (BOP_GETPROP(bootops, "physmem", prop) >= 0) &&
	    (kobj_getvalue(prop, &lvalue) != -1)) {
		physmem = (uintptr_t)lvalue;
	}
#ifdef	DEBUG
	if ((BOP_GETPROPLEN(bootops, "ddidebug") <= sizeof (prop)) &&
	    (BOP_GETPROP(bootops, "ddidebug", prop) >= 0) &&
	    (kobj_getvalue(prop, &lvalue) != -1)) {
		ddidebug = (long)lvalue;
	}
#endif	/* DEBUG */

	PLAT_BOOTPROP_INIT(prop, sizeof(prop), &lvalue);
}

void
kobj_vmem_init(vmem_t **text_arena, vmem_t **data_arena)
{
	*text_arena = vmem_create("module_text", NULL, 0, 1,
				  segkmem_alloc, segkmem_free, heaptext_arena,
				  0, VM_SLEEP);
	*data_arena = vmem_create("module_data", NULL, 0, 1,
				  segkmem_alloc, segkmem_free, heap32_arena,
				  0, VM_SLEEP);
}

caddr_t
kobj_text_alloc(vmem_t *arena, size_t size)
{
	return vmem_alloc(arena, size, VM_SLEEP|VM_BESTFIT);
}

/*ARGSUSED*/
caddr_t
kobj_texthole_alloc(caddr_t addr, size_t size)
{
	extern vmem_t *text_arena;
	return kobj_text_alloc(text_arena, size);
}

/*ARGSUSED*/
void
kobj_texthole_free(caddr_t addr, size_t size)
{
	extern vmem_t *text_arena;
	vmem_free(text_arena, addr, size);
}

void *
device_arena_alloc(size_t size, int vm_flag)
{
	caddr_t	vaddr;

	vaddr = vmem_alloc(heap_arena, size, vm_flag);
	if (vaddr == NULL) {
		return NULL;
	}

	if (!toxic_range_alloc((uintptr_t)vaddr, size, vm_flag)) {
		vmem_free(heap_arena, vaddr, size);
		return NULL;
	}

	return vaddr;
}

void
device_arena_free(void *vaddr, size_t size)
{
	toxic_range_free((uintptr_t)vaddr, size);
	vmem_free(heap_arena, vaddr, size);
}

/*
 * static page_t *
 * page_chunk_init(pfn_t pfn, pgcnt_t npages, page_t *pp, uint_t flags,
 *		   struct memlist ***availppp, struct memseg ***msegppp)
 *	Add a physical chunk of memory of the specified range to the system.
 *	"pfn" is a base page frame number, and "npages" is number of struct
 *	page. The caller must allocate struct page array, and specify to
 *	"pp".
 *
 *	If PGCHUNK_NO_PAGEALLOC is set in "flags":
 *	- page_chunk_init() doesn't call add_physmem().
 *	- availrmem_initial is updated, but availrmem is not.
 *
 * Calling/Exit State:
 *	page_chunk_init() returns end boundary address of new struct
 *	page array.
 */
static page_t *
page_chunk_init(pfn_t pfn, pgcnt_t npages, page_t *pp, uint_t flags,
		struct memlist ***availppp, struct memseg ***msegppp)
{
	struct memlist	*mlp, **mlpp = *availppp;
	struct memseg	*msegp, **msegpp = *msegppp;
	page_t	*nextpp;

	/* Allocate struct memseg and memlist for new segment. */
	MEMSEG_ALLOC(mlp, msegp);

	/* Initialize new memseg. */
	msegp->pages = pp;
	msegp->epages = nextpp = pp + npages;
	msegp->pages_base = pfn;
	msegp->pages_end = pfn + npages;

#ifdef	DEBUG
	PRM_PRINTF("memsegs[%d]: page = 0x%lx, epages = 0x%lx\n",
		   memseg_curindex - 1,
		   (uintptr_t)msegp->pages, (uintptr_t)msegp->epages);
	PRM_PRINTF("memsegs[%d]: base = 0x%lx, end = 0x%lx, npgs = 0x%lx\n",
		   memseg_curindex - 1,
		   msegp->pages_base, msegp->pages_end, npages);
#endif	/* DEBUG */

	/* Initialize new memlist. */
	mlp->address = (uint64_t)mmu_ptob(pfn);
	mlp->size = (uint64_t)mmu_ptob(npages);

	/*
	 * Append new struct memlist to phys_avail list, and update next
	 * pointer for next allocation.
	 */
	*mlpp = mlp;
	*availppp = &(mlp->next);

	/*
	 * Append new struct memseg to memsegs list, and update next pointer
	 * for next allocation.
	 */
	*msegpp = msegp;
	*msegppp = &(msegp->next);

	if (!(flags & PGCHUNK_NO_PAGEALLOC)) {
		/*
		 * add_physmem() initializes the PSM part of the page struct
		 * by calling the PSM back with add_physmem_cb().
		 * In addition it coalesces pages into larger pages as it
		 * initializes them.
		 */
		add_physmem(pp, npages, pfn);
		availrmem += npages;
	}

	availrmem_initial += npages;

	return nextpp;
}

/*
 * static void
 * page_init(void)
 *	Initialize page structures that manages physical pages.
 *
 * Remarks:
 *	Boottime memory allocator is disabled in this function call.
 */
static void
page_init(void)
{
	uintptr_t	lastaddr, lastpaddr, sdram0end;
	pgcnt_t		npages, npgs, notavail, prev_notavail = 0, pmem;
	int		loop = 0;
	size_t		pagehash_sz, pagesz;
	page_t		*ppbase, *nextpp;
	pgcnt_t		prevdiff = 0;
	uint_t		samediff = 0;
	const pgcnt_t	xdnpgs = XRAMDEV_IMPL_NPAGES();
	const pgcnt_t	xdevpgs = XRAMDEV_IMPL_NDEVPAGES();
	const pgcnt_t	bknpgs = XRAMDEV_IMPL_BACKUP_NPAGES();
	const pgcnt_t	resv = xdevpgs + bknpgs;
	struct memseg	**msegpp;
	struct memlist	*mlp, *pmlp, **mlpp, *physp;
	extern void	boottmp_alloc_disable(void);
	extern uintptr_t	bmem_getbrk(void);
#ifdef	ARMPF_SDRAM0_SPLIT
	extern int	unix_ctf_exists;
#endif	/* ARMPF_SDRAM0_SPLIT */

	/*
	 * Calculate number of page structures enough to manage all
	 * physical pages.
	 */

	lastaddr = bmem_getbrk();
	PRM_DEBUG(lastaddr);
	if (lastaddr >= BOP_ALLOC_LIMIT) {
		panic("Too large kernel: lastaddr=0x%lx", lastaddr);
	}
#ifdef	DEBUG
	lastpaddr = KVTOP_DATA(lastaddr);
	PRM_DEBUG(lastpaddr);
#endif	/* DEBUG */

	/*
	 * Each virtual page in [KERNELBASE, PAGE_ROUNDUP(lastaddr)) has a
	 * unique physical page mapping, and these pages must not be put
	 * under struct page management.
	 */
	notavail = mmu_btopr(lastaddr - KERNELBASE);
	PRM_DEBUG(notavail);

	pmem = physmem;
	if (resv > 0) {
		if (pmem <= resv) {
			panic("Failed to reserve memory for device: "
			      "physmem=0x%lx, resv=0x%lx", pmem, resv);
		}
		pmem -= resv;
	}

	XRAMDEV_PRM_DEBUG(bknpgs);
	XRAMDEV_PRM_DEBUG(xdevpgs);
	XRAMDEV_PRM_DEBUG(xdnpgs);

	do {
		PAGE_NPAGES(npgs, pmem, notavail);

		/*
		 * The page structure hash table size is a power of 2
		 * such that the average hash chain length is PAGE_HASHAVELEN.
		 */
		page_hashsz = npgs / PAGE_HASHAVELEN;
		page_hashsz = 1 << highbit(page_hashsz);
		pagehash_sz = sizeof (struct page *) * page_hashsz;

		pagesz = sizeof(struct page) * npgs;

		prev_notavail = notavail;
		notavail = mmu_btopr(lastaddr - KERNELBASE +
				     pagehash_sz + pagesz);
		loop++;
		if (loop >= MAX_PAGE_INIT_LOOP) {
			prom_printf("WARNING: Give up to fit page array: "
				    "notavail = 0x%lx, prev = 0x%lx\n",
				    notavail, prev_notavail);
			if (notavail < prev_notavail) {
				notavail = prev_notavail;
				PAGE_NPAGES(npgs, pmem, notavail);
				pagesz = sizeof(struct page) * npgs;
			}
			break;
		}

		if (notavail != prev_notavail) {
			pgcnt_t	diff, nva;

			if (notavail > prev_notavail) {
				diff = notavail - prev_notavail;
				nva = notavail;
			}
			else {
				diff = prev_notavail - notavail;
				nva = prev_notavail;
			}
			if (diff == prevdiff) {
				samediff++;
				if (samediff >= MAX_PAGE_INIT_CONVERGE_LOOP) {
					/*
					 * The number of available page didn't
					 * converge.
					 */
					notavail = nva;
					PAGE_NPAGES(npgs, pmem, notavail);
					pagesz = sizeof(struct page) * npgs;
					break;
				}
			}
			else {
				samediff = 0;
			}
			prevdiff = diff;
		}
	} while (notavail != prev_notavail);
	PRM_DEBUG(loop);

	if (npgs < xdnpgs) {
		panic("Failed to reserve xramdev pages: npgs=0x%lx, "
		      "xdnpgs=0x%lx", npgs, xdnpgs);
	}

	npages = npgs;
	PRM_DEBUG(npages);
	PRM_DEBUG(notavail);
	lastaddr = KERNELBASE + mmu_ptob(notavail);
	PRM_DEBUG(lastaddr);
	lastpaddr = KVTOP_DATA(lastaddr);
	PRM_DEBUG(lastpaddr);

	/* econtig points the last kernel static data. */
#ifdef	XRAMDEV_CONFIG
	econtig = (caddr_t)PAGE_ROUNDDOWN((uintptr_t)e_text);
	e_kstatic = (caddr_t)lastaddr;
#else	/* !XRAMDEV_CONFIG */
	econtig = (caddr_t)lastaddr;
#endif	/* XRAMDEV_CONFIG */
	PRM_DEBUG(econtig);

	/* Allocate page hash table. */
	PRM_DEBUG(page_hashsz);
	BOOT_ALLOC(page_hash, page_t **, pagehash_sz, BO_NO_ALIGN,
		   "Failed to allocate page_hash");
	FAST_BZERO_ALIGNED(page_hash, pagehash_sz);
	PRM_DEBUG(page_hash);
	PRM_DEBUG(pagehash_sz);

	/* Allocate page array. */
	BOOT_ALLOC(ppbase, page_t *, pagesz, BO_NO_ALIGN,
		   "Failed to allocate page_t array");
	FAST_BZERO_ALIGNED(ppbase, pagesz);
	PRM_DEBUG(ppbase);
	PRM_DEBUG(pagesz);

	ASSERT(bmem_getbrk() <= lastaddr);

	/* Disable boottime allocator. */
	boottmp_alloc_disable();
	bootops->bsys_alloc = NULL;

	availrmem_initial = availrmem = freemem = 0;
	msegpp = &memsegs;
	mlpp = &phys_avail;
	nextpp = ppbase;
	npgs -= xdnpgs;

	/*
	 * Initialize struct page array for each memory bank.
	 */

#ifdef	ARMPF_SDRAM0_SPLIT
	/*
	 * Create struct page array for memory hole above KERNELPHYSBASE.
	 */
	{
		const uintptr_t	paddr = ARMPF_SDRAM0_PADDR;
		pgcnt_t	n = mmu_btop(KERNELPHYSBASE - paddr);

		ASSERT(n != 0);
		if (npgs < n) {
			n = npgs;
			npgs = 0;
		}
		else {
			npgs -= n;
		}
		nextpp = page_chunk_init(mmu_btop(paddr), n, nextpp, 0,
					 &mlpp, &msegpp);
	}
#endif	/* ARMPF_SDRAM0_SPLIT */

	for (physp = phys_install; physp != NULL; physp = physp->next) {
		size_t		size, psize;
		uintptr_t	paddr, physend, start, end;
		pgcnt_t		n;

		paddr = (uintptr_t)physp->address;
		psize = (size_t)physp->size;
		physend = paddr + psize;

#ifdef	XRAMDEV_CONFIG
		if (xramdev_pagestart_paddr >= paddr &&
		    xramdev_pagestart_paddr <= physend) {
			uintptr_t	endaddr;
			int		nextseg;
			pgcnt_t		nn;

			/*
			 * We assume that pages in
			 * [xramdev_pagestart_paddr, physend) should be
			 * put under struct page management.
			 */
			if (data_paddr >= paddr && data_paddr < physend) {
				endaddr = data_paddr;
				nextseg = 0;
			}
			else {
				endaddr = physend;
				nextseg = 1;
			}
			nn = mmu_btop(endaddr - xramdev_end_paddr);
			if (npgs < nn) {
				nn = npgs;
				npgs = 0;
			}
			else {
				npgs -= nn;
			}
			n = xdnpgs + nn;
			if (n > 0) {
				const pfn_t	xdstart =
					mmu_btop(xramdev_pagestart_paddr);
				page_t	*xppbase = nextpp;

				nextpp = page_chunk_init(xdstart, n, nextpp,
							 PGCHUNK_NO_PAGEALLOC,
							 &mlpp, &msegpp);
				if (xdnpgs > 0) {
					/*
					 * Initialize page structures for
					 * xramfs device pages. These pages
					 * are not linked to free list.
					 */
					add_physmem_dontfree(xppbase, xdnpgs,
							     xdstart);
				}
				if (nn > 0) {
					const pfn_t	spfn =
						mmu_btop(xramdev_end_paddr);

					add_physmem(xppbase + xdnpgs, nn, spfn);
					availrmem += nn;
					if (npgs == 0) {
						break;
					}
				}
			}
			if (nextseg) {
				continue;
			}
		}
		else if (data_paddr >= paddr && data_paddr < physend) {
			/*
			 * The kernel data section is located in this segment.
			 */
			start = paddr;
			n = mmu_btop(data_paddr - paddr);
			if (n != 0) {
				/*
				 * Initialize pages just above the kernel
				 * data section.
				 */
				if (npgs < n) {
					n = npgs;
					npgs = 0;
				}
				else {
					npgs -= n;
				}
				nextpp = page_chunk_init(mmu_btop(start), n,
							 nextpp, 0, &mlpp,
							 &msegpp);
				if (npgs == 0) {
					break;
				}
			}
		}
#endif	/* XRAMDEV_CONFIG */

		if (lastpaddr >= paddr && lastpaddr <= physend) {
			/*
			 * This area contains lastpaddr.
			 * We assume that [paddr, lastpaddr) is out of
			 * struct page management, or already initialized.
			 */
			start = lastpaddr;
			size = physend - lastpaddr;
		}
		else {
			start = paddr;
			size = psize;
		}

		n = mmu_btop(size);
		if (n == 0) {
			continue;
		}

		if (npgs < n) {
			n = npgs;
			npgs = 0;
		}
		else {
			npgs -= n;
		}
		nextpp = page_chunk_init(mmu_btop(start), n, nextpp, 0,
					 &mlpp, &msegpp);
		if (npgs == 0) {
			break;
		}
	}

	ASSERT(nextpp + npgs == ppbase + npages);
	ASSERT((uintptr_t)nextpp <= lastaddr);
	PRM_DEBUG(npgs);

	/* Terminate memseg list. */
	*msegpp = NULL;

	/* Terminate memlist list, and adjust prev link. */
	*mlpp = NULL;
	for (mlp = phys_avail, pmlp = NULL; mlp != NULL;
	     pmlp = mlp, mlp = mlp->next) {
		mlp->prev = pmlp;
	}

	/* Finally, adjust physmem. */
	physmem = availrmem_initial;
	PRM_DEBUG(physmem);

	build_pfn_hash();

	if (prom_debug) {
		print_boot_memlist("phys_avail", phys_avail);
	}

#ifndef	XRAMDEV_CONFIG
	/* Preserve pages that contains CTF data. */

#ifdef	ARMPF_SDRAM0_SPLIT
	if (unix_ctf_exists == 0) {
		prom_printf("WARNING: CTF data is lost\n");
		ctf_start_address = ctf_end_address = NULL;
		return;
	}
#endif	/* ARMPF_SDRAM0_SPLIT */

	if (ctf_start_address) {
		pfn_t	pfn, epfn;
		uintptr_t	spaddr, epaddr;
		struct memseg	*msp;

		PRM_DEBUG(ctf_start_address);
		PRM_DEBUG(ctf_end_address);
		ASSERT(IS_PAGEALIGNED(ctf_start_address));

#ifdef	ARMPF_SDRAM0_SPLIT
		/*
		 * ctf_start_address and ctf_end_address keep physical address.
		 */
		spaddr = (uintptr_t)ctf_start_address;
		epaddr = (uintptr_t)ctf_end_address;
#else	/* ARMPF_SDRAM0_SPLIT */
		/*
		 * ctf_start_address and ctf_end_address keep virtual address.
		 * So we need to convert each of them into physical address.
		 * Note that CTF data is located in physically contiguous
		 * area.
		 */
		ASSERT((uintptr_t)ctf_end_address <=
		       KERNELBASE + mmu_ptob(PLATFORM_MIN_PHYSMEM));
		spaddr = KVTOP_DATA(ctf_start_address);
		epaddr = KVTOP_DATA(ctf_end_address);
#endif	/* ARMPF_SDRAM0_SPLIT */

		pfn = mmu_btop(spaddr);
		epfn = mmu_btopr(epaddr);

		/* Check whether all CTF data pages have struct page. */
		for (msp = memsegs;
		     msp != NULL && (msp->pages_base > pfn ||
				     msp->pages_end < epfn);
		     msp = msp->next);
		if (msp == NULL) {
			prom_printf("WARNING: CTF data is lost due to "
				    "large kernel\n");
			ctf_start_address = ctf_end_address = NULL;
		}
		else {
			page_t	*pp;

			ctf_start_paddr = (caddr_t)spaddr;
			PRM_PRINTF("CTF data: pfn=0x%lx, epfn=0x%lx\n",
				   pfn, epfn);
			ASSERT(pfn < epfn);

			npgs = epfn - pfn;
			if (page_resv(npgs, KM_NOSLEEP) == 0) {
				panic("page_init: can't reserve CTF pages: "
				      "%ld", npgs);
			}
			if (page_create_wait(npgs, 0) == 0) {
				panic("page_init: can't allocate CTF pages: "
				      "%ld", npgs);
			}

			pp = page_numtopp_nolock(pfn);
			ASSERT(pp);
			for (; pfn < epfn; pfn++, pp++) {
				int	locked;

				/* Acquire exclusive page lock. */
				ASSERT(pp->p_pagenum == pfn);
				ASSERT(PP_ISAGED(pp));
				locked = page_trylock(pp, SE_EXCL);
				ASSERT(locked);

				page_list_sub(pp, PG_FREE_LIST);
				page_list_concat(&unix_ctf_pages, &pp);
			}
		}
	}
#endif	/* !XRAMDEV_CONFIG */
}

#ifdef	XRAMDEV_CONFIG

/*
 * static void
 * unix_ctf_init(void)
 *	Initialize unix CTF data.
 *
 *	If XRAMDEV_CONFIG is defined, page_init() doesn't care about CTF data.
 *	We need to allocate pages for unix CTF data here, and copy CTF data
 *	from backup data.
 */
static void
unix_ctf_init(void)
{
	uintptr_t	rstart, soff;
	size_t		ctfsize, mapsize, bksize;
	void		*ctfaddr, *bkaddr;
	caddr_t		src;
	struct hat	*khat = &hat_kas;

	/*
	 * ctf_start_address and ctf_end_address keeps start and end physical
	 * boundary address of CTF data in backup area.
	 */
	if (ctf_start_address == NULL || ctf_end_address == NULL) {
		/* No CTF data. */
		return;
	}

	/* Allocate virtual address space. */
	ASSERT(ctf_end_address > ctf_start_address);
	ctfsize = ctf_end_address - ctf_start_address;
	mapsize = PAGE_ROUNDUP(ctfsize);
	if ((ctfaddr = vmem_alloc(heap_arena, mapsize, VM_NOSLEEP)) == NULL) {
		cmn_err(CE_WARN, "Failed to allocate space for unix CTF data: "
			"0x%lx", mapsize);
		return;
	}

	/* Allocate memory for unix CTF data. */
	if (startup_page_create(ctfaddr, mapsize, B_FALSE) == NULL) {
		cmn_err(CE_WARN, "Failed to allocate unix CTF data page.");
		goto errout_vmem;
	}

	/* Create temporary mapping for CTF data in backup section. */
	rstart = PAGE_ROUNDDOWN((uintptr_t)ctf_start_address);
	bksize = PAGE_ROUNDUP((uintptr_t)ctf_end_address) - rstart;
	if ((bkaddr = vmem_alloc(heap_arena, bksize, VM_NOSLEEP)) == NULL) {
		cmn_err(CE_WARN, "Failed to allocate space for CTF data "
			"backup: 0x%lx", bksize);
		goto errout_page;
	}

	hat_devload(khat, bkaddr, bksize, btop(rstart),
		    PROT_READ|HAT_STORECACHING_OK,
		    HAT_LOAD_LOCK|HAT_LOAD_NOCONSIST);

	/* Copy CTF data. */
	soff = (uintptr_t)ctf_start_address - rstart;
	src = (caddr_t)bkaddr + soff;
	FAST_BCOPY_ALIGNED(src, ctfaddr, ctfsize);

	/* Destroy temporary mapping. */
	hat_unload(khat, bkaddr, bksize, HAT_UNLOAD_UNLOCK);
	vmem_free(heap_arena, bkaddr, bksize);

	/* Set CTF data to unix module. */
	mod_static_ctf_init(ctfaddr, ctfsize);
	return;

errout_page:
	segkmem_free(heap_arena, ctfaddr, mapsize);

errout_vmem:
	vmem_free(heap_arena, ctfaddr, mapsize);
}

#else	/* !XRAMDEV_CONFIG */

/*
 * static void
 * unix_ctf_init(void)
 *	Initialize mapping for unix CTF data.
 *	unix_ctf_mapinit() allocates virtual space for unix CTF data,
 *	and establish mapping.
 */
static void
unix_ctf_init(void)
{
	void		*ctfaddr;
	size_t		ctfsize, mapsize;
	u_offset_t	off;
	page_t		*plist;
#ifdef	DEBUG
	pgcnt_t		rest;
#endif	/* DEBUG */

	/*
	 * Pages for CTF data has already been reserved by page_init().
	 */
	if (unix_ctf_pages == NULL) {
		/* No CTF data. */
		return;
	}

	/* Allocate virtual address space. */
	ctfsize = ctf_end_address - ctf_start_address;
	mapsize = PAGE_ROUNDUP(ctfsize);
	ctfaddr = vmem_alloc(heap_arena, mapsize, VM_NOSLEEP|VM_PANIC);

	/* Establish mapping. */
	plist = unix_ctf_pages;
	hat_devload(kas.a_hat, ctfaddr, mapsize, plist->p_pagenum,
		    PROT_READ|PROT_WRITE|HAT_STORECACHING_OK,
		    HAT_LOAD_LOCK|HAT_LOAD_NOCONSIST);
	off = (u_offset_t)(uintptr_t)ctfaddr;
#ifdef	DEBUG
	rest = btopr(mapsize);
#endif	/* DEBUG */
	while (plist) {
		page_t	*pp = plist;
		extern uint_t	page_create_new;

		/* I/O lock must not be held. */
		ASSERT(!page_iolock_assert(pp));

		if (!page_hashin(pp, &kvp, off, NULL)) {
			panic("page_hashin() for CTF data failed: "
			      "pp=%p, off=0x%llx", pp, off);
		}
		VM_STAT_ADD(page_create_new);
		off += MMU_PAGESIZE;
		PP_CLRFREE(pp);
		PP_CLRAGED(pp);
		pp->p_lckcnt = 1;
		page_set_props(pp, P_REF);

		page_sub(&plist, pp);
		page_unlock(pp);
#ifdef	DEBUG
		ASSERT(rest > 0);
		rest--;
#endif	/* DEBUG */
	}
#ifdef	DEBUG
	ASSERT(rest == 0);
#endif	/* DEBUG */

	/* Set CTF data to unix module. */
	mod_static_ctf_init(ctfaddr, ctfsize);
}

#endif	/* XRAMDEV_CONFIG */

/*
 * static caddr_t
 * startup_malloc(struct bootops *bops, caddr_t virthint, size_t size,
 *		  int align)
 *	Memory allocator for startup process.
 *	This function is called as BOP_ALLOC(), and it is used after
 *	page_t initialization, until VM initialization.
 *
 * Remarks:
 *	We assume that only segkmem_alloc() calls this function.
 */
static caddr_t
startup_malloc(struct bootops *bops, caddr_t virthint, size_t size, int align)
{
	ASSERT(virthint >= kernelheap && virthint < ekernelheap);
	ASSERT(IS_PAGEALIGNED(virthint));
	ASSERT(IS_PAGEALIGNED(size));
	ASSERT(align == BO_NO_ALIGN);

	return startup_page_create(virthint, size, B_TRUE);
}

/*
 * static caddr_t
 * startup_page_create(caddr_t vaddr, size_t size, const boolean_t dopanic)
 *	Create page and map them to the specified virtual space.
 *	If dopanic is true, it causes system panic when it fails.
 */
static caddr_t
startup_page_create(caddr_t vaddr, size_t size, const boolean_t dopanic)
{
	page_t	*plist;
	pgcnt_t	npages = btopr(size);
#ifdef	DEBUG
	pgcnt_t	npgs = 0;
	caddr_t	va;
#endif	/* DEBUG */
	struct hat	*khat = &hat_kas;

	if (page_resv(npages, KM_NOSLEEP) == 0) {
		int	ce = (dopanic) ? CE_PANIC : CE_WARN;

		cmn_err(ce, "startup_page_create: "
			"Failed to reserve page: %lu", npages);
		return NULL;
	}

	plist = segkmem_page_create(vaddr, size, VM_NOSLEEP, NULL);
	if (plist == NULL) {
		int	ce = (dopanic) ? CE_PANIC : CE_WARN;
		
		cmn_err(ce, "startup_page_create: "
			"Failed to allocate page: 0x%lx", size);
		page_unresv(npages);
		return NULL;
	}

	while (plist != NULL) {
		page_t	*pp = plist;

		page_sub(&plist, pp);
		ASSERT(page_iolock_assert(pp));
		ASSERT(PAGE_EXCL(pp));
		page_io_unlock(pp);
		hat_memload(khat, (caddr_t)(uintptr_t)pp->p_offset, pp,
			    HAT_PROT_KERNEL|HAT_NOSYNC, HAT_LOAD_LOCK);
		pp->p_lckcnt = 1;
		page_unlock(pp);
#ifdef	DEBUG
		npgs++;
#endif	/* DEBUG */
	}

#ifdef	DEBUG
	ASSERT(npgs == npages);
	for (va = vaddr; va < vaddr + size; va += PAGESIZE) {
		pfn_t	pfn = hat_getkpfnum(va);
		page_t	*pp;

		ASSERT(pfn != PFN_INVALID);
		pp = page_numtopp_nolock(pfn);
		ASSERT(pp != NULL);
		ASSERT(pp->p_vnode == &kvp);
		ASSERT(va == (caddr_t)(uintptr_t)pp->p_offset);
		
	}
#endif	/* DEBUG */

	return vaddr;
}

#define HW_SERIAL_FIXED_INIT_VALUE "1211256659\0"
#define HW_SERIAL_FIXED_INIT_BYTES 11
/*
 * static void
 * hw_serial_init(void)
 *	Initialize hw_serial[].  
 *      Load fixed value into hw_serial[].
 * Note: 
 *      Unique identification value is not needed for the system.
 */
static void
hw_serial_init(void)
{
	uint32_t addrl, addrh;
	uint32_t tmp = 0;

	bcopy((const void *)HW_SERIAL_FIXED_INIT_VALUE, (void *)hw_serial,
		(size_t)HW_SERIAL_FIXED_INIT_BYTES);
	PRM_PRINTF("hw_serial = \"%s\"\n", hw_serial);
}

/*
 * static void
 * swap_conf(void)
 *	Configure default swap device.
 */
static void
swap_conf(void)
{
	char	*swapname;
	char	*dumpconf;
	vnode_t	*vp;
	int	err, dflag;
	extern int	swap_kern_add(struct vnode *vp, char *swapname);

	/*
	 * Set dump configuration flag.
	 */
	if (ddi_prop_lookup_string(DDI_DEV_T_ANY, ddi_root_node(),
				   DDI_PROP_DONTPASS, "dumpconf",
				   &dumpconf) == DDI_SUCCESS) {
		if (strcmp(dumpconf, "kernel") == 0) {
			dflag = DUMP_KERNEL;
		}
		else if (strcmp(dumpconf, "all") == 0) {
			dflag = DUMP_ALL;
		}
		else {
			/* Default is "curproc" */
			dflag = DUMP_CURPROC;
		}
		ddi_prop_free(dumpconf);

		mutex_enter(&dump_lock);
		dump_conflags = dflag;
		mutex_exit(&dump_lock);
	}

	/* Lookup default swap device. */
	if (ddi_prop_lookup_string(DDI_DEV_T_ANY, ddi_root_node(),
				   DDI_PROP_DONTPASS, "swapdev", &swapname)
	    != DDI_SUCCESS) {
		return;
	}

	/* Add this device as swap. */
	err = lookupname(swapname, UIO_SYSSPACE, FOLLOW, NULLVPP, &vp);
	if (err == 0) {
		if (vp->v_flag & (VNOMAP|VNOSWAP)) {
			cmn_err(CE_WARN, "Invalid swap device: %s",
				swapname);
			VN_RELE(vp);
			goto out;
		}
		if (vp->v_type != VREG && vp->v_type != VBLK) {
			cmn_err(CE_WARN,
				"Invalid file type for swap device: %s",
				swapname);
			VN_RELE(vp);
			goto out;
		}

		err = swap_kern_add(vp, swapname);
		VN_RELE(vp);
		if (err != 0) {
			cmn_err(CE_WARN, "Can't add swap device. err = %d",
				err);
			goto out;
		}

		PRM_PRINTF("Default swap device: %s\n", swapname);
	}
	else {
		cmn_err(CE_WARN, "Can't find swap device(%s). err = %d",
			swapname, err);
	}

 out:
	ddi_prop_free(swapname);
}

#ifdef	DEBUG
const char *
prom_debug_filepath(const char *file)
{
	while (strncmp(file, "../", 3) == 0) {
		file += 3;
	}

	return file;
}
#endif	/* DEBUG */
