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
 * Copyright (c) 2007-2008 NEC Corporation
 */

#ifndef _SYS_PCI_IMPL_ARM_H
#define	_SYS_PCI_IMPL_ARM_H

#include <sys/dditypes.h>
#include <sys/memlist.h>

#ifdef __cplusplus
extern "C" {
#endif

#define	PCI_MECHANISM_UNKNOWN		-1
#define	PCI_MECHANISM_NONE		0
#define	PCI_MECHANISM_1 		1
#define	PCI_MECHANISM_2			2

#ifndef FALSE
#define	FALSE   0
#endif

#ifndef TRUE
#define	TRUE    1
#endif

#define	PCI_FUNC_MASK			0x07

/* these macros apply to Configuration Mechanism #1 */
/* Config Address Register etc... */
#define PCI_CONFADD             0xf8
#define PCI_CONFDATA            0xfc
#define	PCI_CADDR1(bus, device, function, reg) \
		( (((bus) & 0xff) << 16) | (((device & 0x1f)) << 11) \
			    | (((function) & 0x7) << 8) | ((reg) & 0xfc))

typedef struct 	pci_acc_cfblk {
	uchar_t	c_busnum;		/* bus number */
	uchar_t c_devnum;		/* device number */
	uchar_t c_funcnum;		/* function number */
	uchar_t c_fill;			/* reserve field */
} pci_acc_cfblk_t;

struct pci_memlist {
	uint32_t	address;	/* starting address of memory segment */
	uint32_t	size;		/* size of same */
	struct pci_memlist	*next;	/* link to next list element */
	struct pci_memlist	*prev;	/* link to previous list element */
};

struct pci_bus_resource {
	struct pci_memlist *io_ports;	/* available free io res */
	struct pci_memlist *io_ports_used;	/* used io res */
	struct pci_memlist *mem_space;	/* available free mem res */
	struct pci_memlist *mem_space_used;	/* used mem res */
	struct pci_memlist *pmem_space;
			/* available free prefetchable mem res */
	struct pci_memlist *pmem_space_used; /* used prefetchable mem res */
	struct pci_memlist *bus_space;	/* available free bus res */
			/* bus_space_used not needed; can read from regs */
	dev_info_t *dip;	/* devinfo node */
	void *privdata;		/* private data for configuration */
	uchar_t par_bus;	/* parent bus number */
	uchar_t sub_bus;	/* highest bus number beyond this bridge */
	uchar_t root_addr;	/* legacy peer bus address assignment */
	uchar_t num_cbb;	/* # of CardBus Bridges on the bus */
	boolean_t io_reprogram;	/* need io reprog on this bus */
	boolean_t mem_reprogram;	/* need mem reprog on this bus */
	boolean_t subtractive;	/* subtractive PPB */
};

extern struct pci_bus_resource *pci_bus_res;

/*
 * Memlist Functions
 */
extern struct pci_memlist *memlist_alloc(void);
extern void memlist_free(struct pci_memlist *);
extern void memlist_free_all(struct pci_memlist **);
extern void memlist_insert(struct pci_memlist **, uint32_t, uint32_t);
extern int memlist_remove(struct pci_memlist **, uint32_t, uint32_t);
extern uint32_t memlist_find(struct pci_memlist **, uint32_t, int);
extern uint32_t memlist_find_with_startaddr(struct pci_memlist **, uint32_t,
    uint32_t, int);
extern void memlist_dump(struct pci_memlist *);
extern void memlist_merge(struct pci_memlist **, struct pci_memlist **);
extern struct pci_memlist *memlist_dup(struct pci_memlist *);
extern int memlist_count(struct pci_memlist *);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_PCI_IMPL_ARM_H */
