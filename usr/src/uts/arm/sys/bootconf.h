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

/*
 * Copyright (c) 2006-2008 NEC Corporation
 */

#ifndef	_SYS_BOOTCONF_H
#define	_SYS_BOOTCONF_H

#pragma ident	"@(#)bootconf.h	1.51	08/05/20 SMI"

/*
 * Boot time configuration information objects
 */

#include <sys/types.h>
#include <sys/bootregs.h>		/* for struct bop_regs */
#include <sys/bootstat.h>
#include <sys/dirent.h>			/* for struct dirent */
#include <sys/memlist.h>
#include <sys/obpdefs.h>
#include <net/if.h>			/* for IFNAMSIZ */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * masks to hand to bsys_alloc memory allocator
 * XXX	These names shouldn't really be srmmu derived.
 */
#define	BO_NO_ALIGN	sizeof(char *)

/* return values for the newer bootops */
#define	BOOT_SUCCESS	0
#define	BOOT_FAILURE	(-1)

#ifndef	STATIC_UNIX
/* top of boot scratch memory: 15 MB; multiboot loads at 16 MB */
#define	MAGIC_PHYS	0xF00000
#endif	/* !STATIC_UNIX */

/*
 *  We pass a ptr to the space that boot has been using
 *  for its memory lists.
 */
struct bsys_mem {
	struct memlist	*physinstalled;	/* amt of physmem installed */
	struct memlist	*physavail;	/* amt of physmem avail for use */
	struct memlist	*virtavail;	/* amt of virtmem avail for use */
	struct memlist	*pcimem;	/* amt of pcimem avail for use */
	uint_t		extent; 	/* number of bytes in the space */
};

/*
 * Warning: Changing BO_VERSION blows compatibility between booters
 *          and older kernels.  If you want to change the struct bootops,
 *          please consider adding new stuff to the end and using the
 *          "bootops-extensions" mechanism described below.
 */
#define	BO_VERSION	10		/* bootops interface revision # */

typedef struct bootops {
	/*
	 * the ubiquitous version number
	 */
	uint_t	bsys_version;

	/*
	 * the area containing boot's memlists
	 */
	struct 	bsys_mem *boot_mem;

	/*
	 * have boot allocate size bytes at virthint
	 */
	caddr_t	(*bsys_alloc)(struct bootops *, caddr_t virthint, size_t size,
		int align);

	/*
	 * free size bytes allocated at virt - put the
	 * address range back onto the avail lists.
	 */
	void	(*bsys_free)(struct bootops *, caddr_t virt, size_t size);

	/*
	 * to find the size of the buffer to allocate
	 */
	int	(*bsys_getproplen)(struct bootops *, const char *);

	/*
	 * get the value associated with this name
	 */
	int	(*bsys_getprop)(struct bootops *, const char *, void *);

	/*
	 * get the name of the next property in succession
	 * from the standalone
	 */
	char	*(*bsys_nextprop)(struct bootops *, char *prevprop);

	/*
	 * print formatted output
	 */
	void	(*bsys_printf)(struct bootops *, const char *, ...);

	/*
	 * Do a real mode interrupt
	 */
	void	(*bsys_doint)(struct bootops *, int, struct bop_regs *);

	/* end of bootops which exist if (bootops-extensions >= 1) */
} bootops_t;

#define	BOP_GETVERSION(bop)		((bop)->bsys_version)
#define	BOP_ALLOC(bop, virthint, size, align)	\
				((bop)->bsys_alloc)(bop, virthint, size, align)
#define	BOP_FREE(bop, virt, size)	((bop)->bsys_free)(bop, virt, size)
#define	BOP_GETPROPLEN(bop, name)	((bop)->bsys_getproplen)(bop, name)
#define	BOP_GETPROP(bop, name, buf)	((bop)->bsys_getprop)(bop, name, buf)
#define	BOP_NEXTPROP(bop, prev)		((bop)->bsys_nextprop)(bop, prev)
#define	BOP_DOINT(bop, intnum, rp)	((bop)->bsys_doint)(bop, intnum, rp)

/*
 * Not use it. Assume it alias to BOP_ALLOC().
 */
#define	BOP_EALLOC(bop, virthint, size, align, flags)     \
                         BOP_ALLOC(bop, virthint, size, align)

#define	BOP_PUTSARG(bop, msg, arg)	((bop)->bsys_printf)(bop, msg, arg)

#if defined(_KERNEL) && !defined(_BOOT)

/*
 * Boot configuration information
 */

#define	BO_MAXFSNAME	16
#define	BO_MAXOBJNAME	256

struct bootobj {
	char	bo_fstype[BO_MAXFSNAME];	/* vfs type name (e.g. nfs) */
	char	bo_name[BO_MAXOBJNAME];		/* name of object */
	int	bo_flags;			/* flags, see below */
	int	bo_size;			/* number of blocks */
	struct vnode *bo_vp;			/* vnode of object */
	char	bo_devname[BO_MAXOBJNAME];
	char	bo_ifname[BO_MAXOBJNAME];
	int	bo_ppa;
};

/*
 * flags
 */
#define	BO_VALID	0x01	/* all information in object is valid */
#define	BO_BUSY		0x02	/* object is busy */

extern struct bootobj rootfs;
extern struct bootobj swapfile;

extern char obp_bootpath[BO_MAXOBJNAME];
extern char svm_bootpath[BO_MAXOBJNAME];

extern dev_t getrootdev(void);
extern void getfsname(char *, char *, size_t);
extern int loadrootmodules(void);

extern int strplumb(void);
extern int strplumb_load(void);
extern char *strplumb_get_netdev_path(void);

extern void consconfig(void);

extern void param_check(void);
extern int octet_to_hexascii(const void *, uint_t, char *, uint_t *);

extern int dhcpinit(void);

/* release_bootstrap() doesn't need to be implemented. */
#define	release_bootstrap()

/*
 * XXX	The memlist stuff belongs in a header of its own
 */
extern int check_boot_version(int);
extern void size_physavail(struct memlist *, pgcnt_t *, int *, pfn_t);
extern int copy_physavail(struct memlist *, struct memlist **,
    uint_t, uint_t);
extern void installed_top_size(struct memlist *, pfn_t *, pgcnt_t *, int *);
extern int check_memexp(struct memlist *, uint_t);
extern void copy_memlist_filter(struct memlist *, struct memlist **,
    void (*filter)(uint64_t *, uint64_t *));

extern struct bootops *bootops;
extern int netboot;
extern int swaploaded;
extern int modrootloaded;
extern char kern_bootargs[];
extern char *kobj_module_path;
extern char *default_path;
extern char *dhcack;
extern int dhcacklen;
extern char dhcifname[IFNAMSIZ];
extern char *netdev_path;

#if	defined(STATIC_UNIX) && defined(BOOT_DEBUG)
#include <sys/promif.h>
#define	BOOT_DPRINTF(...)	prom_printf(__VA_ARGS__)
#else	/* !(defined(STATIC_UNIX) && defined(BOOT_DEBUG)) */
#define	BOOT_DPRINTF(...)
#endif	/* defined(STATIC_UNIX) && defined(BOOT_DEBUG) */

/* Wrapper for BOP_ALLOC() */
#define	BOOT_ALLOC(ptr, type, size, align, ...)				\
	do {								\
		(ptr) = (type)BOP_ALLOC(bootops, NULL, (size), (align)); \
		if ((ptr) == (type)NULL) {				\
			panic(__VA_ARGS__);				\
		}							\
	} while (0)

#endif /* _KERNEL && !_BOOT */

#ifdef __cplusplus
}
#endif

#endif	/* _SYS_BOOTCONF_H */
