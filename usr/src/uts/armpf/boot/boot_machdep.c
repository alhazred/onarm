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

#ident	"@(#)armpf/boot/boot_machdep.c"

/*
 * ARM platform specific kernel boot initialization.
 * The code in this file is used when the kernel is static-linked kernel.
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/bootconf.h>
#include <sys/promif.h>
#include <sys/pte.h>
#include <sys/platform.h>
#include <sys/modstatic.h>
#include <sys/machsystm.h>
#include <sys/mach_boot.h>

/* Highest address boundary for boottime temporary memory. */
#define	BOOTTMP_ALLOC_LIMIT					\
	(char *)(KERNEL_BOOTTMP_PADDR + KERNEL_BOOTTMP_SIZE)

/* Internal prototypes */
static void	boot_alloc_init(char *end);
static caddr_t	bmem_alloc(struct bootops *bops, caddr_t virthint,
			   size_t size, int align);
static void	bmem_free(struct bootops *bops, caddr_t virt, size_t size);

/*
 * bmem_range manages boottime memory allocation.
 */
typedef struct bmem_range {
	uintptr_t		b_start;	/* Start address */
	uintptr_t		b_end;		/* End address */
	struct bmem_range	*b_next;	/* Next pointer */
} bmem_range_t;

/* Free memory area used for boottime memory allocation. */
static bmem_range_t	*bmem_freelist;

/* Flag to denote the highest boundary of bmem_alloc() area is allocated. */
static int		bmem_highalloc;

/* Current allocation pointer used by boottmp_alloc(). */
static char	*boottmp_current;

/*
 * bootops for static unix on ARM platform.
 * boot_mem will be initialized by boot_memlist_init().
 */
static bootops_t	armpf_bootops = {
	BO_VERSION,		/* bsys_version */
	NULL,			/* boot_mem */
	bmem_alloc,		/* bsys_alloc */
	bmem_free,		/* bsys_free*/

	/* Belows are initialized by boot_init_common(). */
	NULL,			/* bsys_getproplen */
	NULL,			/* bsys_getprop */
	NULL,			/* bsys_nextprop */
	NULL,			/* bsys_printf */
	NULL,			/* bsys_doint */
};

#pragma weak	icedb_bootargs

/*
 * void
 * boot_static_init(void)
 *	Initialize boot environment for static kernel.
 */
void
boot_static_init(void)
{
	extern void	boot_sysp_init(void);
	extern void	boot_init_common(void);
	extern void	boot_memlist_init(bootops_t *bops);
	extern char	*symtab_start_address;
	extern char	*symtab_end_address;
	extern char	*strtab_start_address;
	extern char	*strtab_end_address;
	extern char	*symhash_start_address;
	extern char	*symhash_end_address;
	extern char	__text_start__[];
	extern char	__text_end__[];
	extern char	__end__[];
	extern void	icedb_bootargs(char *);

	/* At first, initialize boot_syscalls for console output. */
	boot_sysp_init();

	/* Initialize memory configuration. */
	boot_memlist_init(&armpf_bootops);

	if (&icedb_bootargs != NULL) {
		/* Override bootargs if debugger exists. */
		icedb_bootargs(kern_bootargs);
	}

	if (kern_bootargs[OBP_MAXPATHLEN - 1] != '\0') {
		prom_printf("WARNING: Too long bootargs. "
			    "Some properties may be lost.\n");
		kern_bootargs[OBP_MAXPATHLEN -1] = '\0';
	}
	BOOT_DPRINTF("kern_bootargs = \"%s\"\n", kern_bootargs);

	/* Initialize variables that keep kernel text/data segment. */
	s_text = (caddr_t)__text_start__;
	e_text = (caddr_t)__text_end__;
	s_data = e_text;
	e_data = (caddr_t)__end__;

	/* Convert section addresses into kernel virtual address. */
	symtab_start_address = (char *)PTOKV(symtab_start_address);
	symtab_end_address = (char *)PTOKV(symtab_end_address);
	strtab_start_address = (char *)PTOKV(strtab_start_address);
	strtab_end_address = (char *)PTOKV(strtab_end_address);
	symhash_start_address = (char *)PTOKV(symhash_start_address);
	symhash_end_address = (char *)PTOKV(symhash_end_address);

	/* Initialize boottime memory allocator. */
	boot_alloc_init(__end__);

	/* BOP_XXX() are available from here. */
	bootops = &armpf_bootops;

	/* Initialize static-linked unix module. */
	mod_static_init();

	/* Platform-independ Initialization */
	boot_init_common();

	/* Initialize kobj layer for static-linked unix environment. */
	kobj_static_init();
}

/*
 * void *
 * boottmp_alloc(size_t size)
 *	Allocate temporary buffer for temporary use at boottime.
 *
 *	Memory allocated by boottmp_alloc() will be invalidated by
 *	boottmp_alloc_disable() call.
 */
void *
boottmp_alloc(size_t size)
{
	char	*buf;

	if (boottmp_current == NULL) {
		prom_panic("boottmp_alloc() is called while it is disabled.");
	}

	buf = boottmp_current;
	boottmp_current += P2ROUNDUP(size, sizeof(char *));

	if (boottmp_current > BOOTTMP_ALLOC_LIMIT) {
		prom_panic("Boottime temporary memory is exhausted.");
	}

	return buf;
}

/*
 * void
 * boottmp_alloc_disable()
 *	Disable boottime temporary memory allocation.
 */
void
boottmp_alloc_disable()
{
	boottmp_current = NULL;
}

/*
 * static void
 * boot_alloc_init(char *end)
 *	Initialize boottime memory allocator.
 *	The caller must pass unix end address (ie. BSS end) to end parameter.
 */
static void
boot_alloc_init(char *end)
{
	bmem_range_t	*rp;
	extern char	*boottmp_reserved;

	/*
	 * At first, initialize data related to boottmp_alloc().
	 *
	 * Our assumptions:
	 *   - The end address of reserved boottime temporary area is
	 *     kept by boottmp_reserved. So we can use the following
	 *     address range as temporary work space:
	 *       [boottmp_reserved, KERNEL_BOOTTMP_PADDR + KERNEL_BOOTTMP_SIZE)
	 *   - bootargs are already salvaged.
	 */
	boottmp_current = boottmp_reserved;
	BOOT_DPRINTF("boottmp_current = 0x%p\n", boottmp_current);

	/*
	 * Construct freelist for bmem_alloc().
	 * We can use [end, BOP_ALLOC_LIMIT) range for allocation.
	 */
	rp = (bmem_range_t *)boottmp_alloc(sizeof(*rp));
	rp->b_start = (uintptr_t)P2ROUNDUP((uintptr_t)end, sizeof(char *));
	rp->b_end = BOP_ALLOC_LIMIT;
	rp->b_next = NULL;
	bmem_freelist = rp;
}

/*
 * static caddr_t
 * bmem_alloc(struct bootops *bops, caddr_t virthint, size_t size, int align)
 *	BOP_ALLOC() implementation for ARM platform.
 *
 *	On current ARM implementation, BOP_ALLOC() doesn't obey virthint
 *	strictly. If virthint is not zero, BOP_ALLOC() tries to allocate memory
 *	at virthint. If not available, BOP_ALLOC() tries to allocate memory
 *	at address higher than virthint.
 *	If virthint is zero, a suitable virtual address is chosen by
 *	BOP_ALLOC(). Regardless of virthint value, align is valid to control
 *	virtual address alignment.
 */
static caddr_t
bmem_alloc(struct bootops *bops, caddr_t virthint, size_t size, int align)
{
	bmem_range_t	**rpp;
	uintptr_t	virt = (uintptr_t)virthint;
	uintptr_t	ret = 0;

	if (size == 0) {
		return NULL;
	}
	if (align == 0) {
		align = BO_NO_ALIGN;
	}
	if (virt) {
		virt = roundup((uintptr_t)virthint, align);
	}

	/* Round up size for efficient memory use. */
	size = P2ROUNDUP(size, BO_NO_ALIGN);

	/* Free range is linked in its address order. */
	BOOT_DPRINTF("bmme_alloc(0x%lx, 0x%lx, 0x%x), virt = 0x%lx\n",
		     (uintptr_t)virthint, size, align, virt);
	for (rpp = &bmem_freelist; *rpp != NULL; rpp = &((*rpp)->b_next)) {
		bmem_range_t	*rp = *rpp;
		uintptr_t	base, end;

		if (virt) {
			base = virt;
			end = virt + size;
			if (end >= rp->b_end) {
				continue;
			}
			if (virt < rp->b_start) {
				/*
				 * The specified address is not available.
				 * Switch to the normal allocation.
				 */
				base = roundup(rp->b_start, align);
			}
		}
		else {
			base = roundup(rp->b_start, align);
		}
		end = base + size;

		if (base == rp->b_start && end <= rp->b_end) {
			/* Found memory at the top top of free range. */
			ret = base;
			if (end == rp->b_end) {
				/* Unlink empty range */
				*rpp = rp->b_next;
			}
			else {
				rp->b_start = end;
			}
			break;
		}
		else if (base >= rp->b_start && end == rp->b_end) {
			/* Found memory at the end of free range. */
			ret = end - size;
			rp->b_end = ret;
			break;
		}
		else if (base > rp->b_start && end < rp->b_end) {
			bmem_range_t	*nrp;

			/*
			 * Found memory at the middle of free range.
			 * This range must be split in two.
			 */
			ret = base;
			nrp = boottmp_alloc(sizeof(*rp));
			nrp->b_start = rp->b_start;
			nrp->b_end = ret;
			nrp->b_next = rp;
			rp->b_start = end;
			*rpp = nrp;
			break;
		}
	}
	if (ret + size == BOP_ALLOC_LIMIT) {
		bmem_highalloc = 1;
	}

	BOOT_DPRINTF("bmem_alloc() => 0x%lx\n", ret);
	return (caddr_t)ret;
}

/*
 * static void
 * bmem_free(struct bootops *bops, caddr_t virt, size_t size)
 *	On ARM implementation, BOP_FREE() is no longer supported.
 *	It calls prom_panic().
 */
static void
bmem_free(struct bootops *bops, caddr_t virt, size_t size)
{
	prom_panic("BOP_FREE() is called.");
}

/*
 * uintptr_t
 * bmem_getbrk(void)
 *	Return the current "break value" for bmem_alloc().
 *
 *	bmem_getbrk() returns the highest address of the allocated memory.
 */
uintptr_t
bmem_getbrk(void)
{
	bmem_range_t	*rp;
	uintptr_t	ret = BOP_ALLOC_LIMIT;

	if (bmem_highalloc == 0) {
		for (rp = bmem_freelist; rp != NULL; rp = rp->b_next) {
			ret = rp->b_start;
		}
	}

	return ret;
}
