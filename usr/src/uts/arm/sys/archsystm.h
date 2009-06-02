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
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ifndef _SYS_ARCHSYSTM_H
#define	_SYS_ARCHSYSTM_H

#pragma ident	"@(#)archsystm.h	1.32	06/02/11 SMI"

/*
 * A selection of ISA-dependent interfaces
 */

#include <vm/seg_enum.h>
#include <vm/page.h>
#include <vm/as.h>
#include <sys/vnode.h>
#include <sys/cpuvar.h>
#include <sys/ddidmareq.h>
#include <sys/sysmacros.h>
#include <sys/kmem.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

extern greg_t getfp(void);
extern int getpil(void);

extern void tenmicrosec(void);

extern void syscall_handler();

extern void bind_hwcap(void);

extern void reset(void) __NORETURN;
extern int goany(void);

extern void setgregs(klwp_t *, gregset_t);
extern void getgregs(klwp_t *, gregset_t);
extern void setfpregs(klwp_t *, fpregset_t *);
extern void getfpregs(klwp_t *, fpregset_t *);

#if defined(_SYSCALL32_IMPL)
extern void getgregs32(klwp_t *, gregset32_t);
extern void setfpregs32(klwp_t *, fpregset32_t *);
extern void getfpregs32(klwp_t *, fpregset32_t *);
#endif

struct regs;

extern int instr_size(struct regs *, caddr_t *, enum seg_rw);

extern void realsigprof(int, int);

extern int enable_cbcp; /* patchable in /etc/system */

extern uint_t cpu_hwcap_flags;
extern uint_t cpu_freq;
extern uint64_t cpu_freq_hz;

extern page_t *page_numtopp_alloc(pfn_t pfnum);

extern void hwblkclr(void *, size_t);
extern void hwblkpagecopy(const void *, void *);

extern void (*kcpc_hw_enable_cpc_intr)(void);

extern void setup_mca(void);
extern void setup_mtrr(void);
extern void patch_tsc(void);

/*
 * Warning: these routines do -not- use normal calling conventions!
 */
extern void setup_121_andcall(void (*)(ulong_t), ulong_t);
extern void enable_big_page_support(ulong_t);
extern void enable_pae(ulong_t);

extern void (*gethrestimef)(timestruc_t *);

extern uint_t cpuid_pass1(cpu_t *);
extern void cpuid_pass2(cpu_t *);
extern void cpuid_pass3(cpu_t *);
extern uint_t cpuid_pass4(cpu_t *);
extern int cpuid_getbrandstr(cpu_t *, char *, size_t);
extern const char *cpuid_getimplstr(int);
extern int cpuid_getidcode(cpu_t *);
extern int cpuid_getidstr(cpu_t *, char *, size_t);
extern void add_cpunode2devtree(cpu_t *);

/*
 * Fast version of bcopy() and bzero().
 *   - Source and destination addresses must be aligned in 4 bytes boundary.
 *   - Size must not be zero, and be a multiple of 512 bytes.
 */

#define	FB_ALIGN	4
#define	FB_BLOCKSHIFT	9
#define	FB_BLOCKSIZE	(1U << FB_BLOCKSHIFT)
#define	FB_BLOCKMASK	(FB_BLOCKSIZE - 1)

extern void	fast_bcopy(const void *from, void *to, size_t size);
extern void	fast_bzero(void *addr, size_t size);

#define	_FAST_BZERO(addr, size, aligned)				\
	do {								\
		if ((size) >= FB_BLOCKSIZE && (aligned)) {		\
			caddr_t	__addr = (caddr_t)(addr);		\
			size_t	__size = (size_t)(size);		\
			size_t	__sz = P2ALIGN(__size, FB_BLOCKSIZE);	\
									\
			fast_bzero(__addr, __sz);			\
			__size -= __sz;					\
			if (__size > 0) {				\
				bzero(__addr + __sz, __size);		\
			}						\
		}							\
		else {							\
			bzero((addr), (size));				\
		}							\
	} while (0)

#define	_FAST_BCOPY(from, to, size, faligned, taligned)			\
	do {								\
		if ((size) >= FB_BLOCKSIZE && (faligned) && (taligned)) { \
			caddr_t	__from = (caddr_t)(from);		\
			caddr_t	__to = (caddr_t)(to);			\
			size_t	__size = (size_t)(size);		\
			size_t	__sz = P2ALIGN(__size, FB_BLOCKSIZE);	\
									\
			fast_bcopy(__from, __to, __sz);			\
			__size -= __sz;					\
			if (__size > 0) {				\
				bcopy(__from + __sz, __to + __sz,	\
				      __size);				\
			}						\
		}							\
		else {							\
			bcopy((from), (to), (size));			\
		}							\
	} while (0)

/*
 * Wrapper for fast_bzero().
 * The given address must be 4 bytes aligned.
 */
#define	FAST_BZERO_ALIGNED(addr, size)	_FAST_BZERO(addr, size, 1)

/*
 * Wrapper for fast_bzero().
 */
#define	FAST_BZERO(addr, size)					\
	_FAST_BZERO(addr, size, IS_P2ALIGNED(addr, FB_ALIGN))

/*
 * Wrapper for fast_bcopy().
 * The given addresses must be 4 bytes aligned.
 */
#define	FAST_BCOPY_ALIGNED(from, to, size)	\
	_FAST_BCOPY(from, to, size, 1, 1)

/*
 * Wrapper for fast_bcopy().
 */
#define	FAST_BCOPY(from, to, size)					\
	_FAST_BCOPY(from, to, size, IS_P2ALIGNED(from, FB_ALIGN),	\
		    IS_P2ALIGNED(to, FB_ALIGN))

/* Allocate physically-contiguous memory. */
extern void	*contig_alloc(size_t size, ddi_dma_attr_t *mattr,
			      uintptr_t align, uint_t attr, int flags);
extern void	contig_free(void *addr, size_t size);
extern page_t	*page_create_io(struct vnode *vp, u_offset_t off, uint_t bytes,
				uint_t flags, struct as *as, caddr_t vaddr,
				ddi_dma_attr_t *mattr);

/* Flags for contig_alloc(). */
#define	CA_DMA		0x10000		/* allocate DMA buffer */

/* Keep track of toxic mappings. */
extern boolean_t	toxic_range_alloc(uintptr_t addr, size_t size,
					  int vmflags);
extern void		toxic_range_free(uintptr_t addr, size_t size);
extern void		toxic_range_iterate(void (*func)(uintptr_t base,
							 uintptr_t limit));

extern void	add_physmem_dontfree(page_t *pp, pgcnt_t count, pfn_t pfn);

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_ARCHSYSTM_H */
