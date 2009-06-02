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
 * Copyright (c) 2006-2008 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)common/os/modstatic.c"

/*
 * This file contains code to manage static-linked kernel and modules.
 * All codes are activated only if STATIC_UNIX is defined.
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/modctl.h>
#include <sys/kobj.h>
#include <sys/kobj_impl.h>
#include <sys/elf.h>
#include <sys/modstatic.h>
#include <sys/tnf_probe.h>
#include <sys/bootconf.h>
#include <sys/prom_debug.h>
#include <sys/kmem.h>
#include <sys/hwconf.h>
#include <sys/autoconf.h>
#include <sys/dacf_impl.h>
#include <sys/sysmacros.h>
#include <sys/cmn_err.h>
#include <sys/ddipropdefs.h>
#include <sys/sunddi.h>
#include <sys/autoconf.h>
#include <sys/atomic.h>
#include <sys/sunndi.h>
#include <vm/seg_kmem.h>

#ifdef	STATIC_UNIX

#define	PROBE_MARKER_SYMBOL	"__tnf_probe_version_1"
#define	TAG_MARKER_SYMBOL	"__tnf_tag_version_1"

#if	defined(__arm)

#include <sys/elf_ARM.h>

#define	UNIX_ELF_MACHINE	EM_ARM
#define	UNIX_ELF_CLASS		ELFCLASS32
#define	UNIX_ELF_DATA		ELFDATA2LSB
#define	UNIX_ELF_ENTRY		KERNELTEXTBASE

#ifdef	__ARM_EABI__
#define	UNIX_ELF_FLAGS		(EF_ARM_HASENTRY|EF_ARM_EABI_VER4)
#else	/* __ARM_EABI__ */
#define	UNIX_ELF_FLAGS		(EF_ARM_HASENTRY|EF_ARM_SOFT_FLOAT)
#endif	/* __ARM_EABI__ */

#else	/* !defined(__arm) */

#error	"ISA not supported"

#endif	/* defined(__arm) */

#define	MOD_IS_STATIC_MODULE(mp)	((mp)->text == module_unix.text)

/* Static objects for unix module */
static struct module	module_unix;
static Shdr		shdr_unix[2];

extern int	last_module_id;
extern struct modctl_list	*kobj_linkmaps[];

extern struct hwc_class	*hcl_head;

/* Embedded /etc/driver_aliases */
extern const int static_drvalias_count;
extern const static_bind_t static_drvalias[];

/* Embedded /etc/driver_classes */
extern const int static_drvclass_count;
extern const static_strbind_t static_drvclass[];

/* Embedded /etc/name_to_major */
extern const int static_majbind_count;
extern const static_bind_t static_majbind[];

/* Embedded /etc/name_to_sysnum */
extern const int static_sysbind_count;
extern const static_bind_t static_sysbind[];

/* Embedded /etc/dacf.conf */
extern const int static_posta_devname_count;
extern const dacf_rule_t static_posta_devname[];
extern const int static_posta_mname_count;
extern const dacf_rule_t static_posta_mname[];
extern const int static_posta_mntype_count;
extern const dacf_rule_t static_posta_mntype[];
extern const int static_pred_devname_count;
extern const dacf_rule_t static_pred_devname[];
extern const int static_pred_mname_count;
extern const dacf_rule_t static_pred_mname[];
extern const int static_pred_mntype_count;
extern const dacf_rule_t static_pred_mntype[];

/* Static-linked modules */
extern mod_static_t	*static_modules[];
extern int		static_modules_count;

/* This structure is used to keep major number of module. */
typedef struct cfmajor {
	major_t		c_major;
	struct cfmajor	*c_next;
} cfmajor_t;

static cfmajor_t	*rescan_major;

/* Keep module name to be loaded at boot time */
struct mod_forceload;
typedef struct mod_forceload	mod_forceload_t;
struct mod_forceload {
	char		*mf_name;	/* module name (contains directory) */
	size_t		mf_namesize;	/* size of mf_name buffer */
	mod_forceload_t	*mf_next;
};

#define	BOP_FORCELOAD_SEP	':'

/*
 * List of module names to be loaded at boot time.
 * Modules in this list will be loaded at post_startup(), that is,
 * they will be loaded after vfs_mountroot().
 */
static mod_forceload_t	*mod_forceload_list;

/* Base address of unix CTF data. */
caddr_t	unix_ctf_address;

/* Internal prototypes */
static mod_static_t	*mod_static_lookup(char *filename);
static void		mod_static_rescan_devinfo(struct devnames *dnp);
static void		mod_static_rescan_pseudo(void);
static mod_forceload_t	*mod_static_forceload_get(char *propname);
static void		mod_static_do_forceload(mod_forceload_t *list);
static char		*local_strdup(char *str);
static struct hwc_spec	*hwc_spec_copy(struct hwc_spec *hwcp);
static ddi_prop_t	*ddi_prop_copy(ddi_prop_t *prop);
static ddi_prop_t	*ddi_prop_list_copy(ddi_prop_t *prop);

/*
 * void
 * mod_static_init(void)
 *	Initialize unix module information.
 *	This function must be called while BOP_ALLOC() is available.
 */
void
mod_static_init(void)
{
	struct module	*mp = &module_unix;
	modctl_t	*cp = &modules;
	symid_t		*hashptr;
	int		i;
	Ehdr		*ehdr;
	struct modctl_list	*lp;
	extern char	*symtab_start_address;
	extern char	*symtab_end_address;
	extern char	*strtab_start_address;
	extern char	*strtab_end_address;
	extern char	*symhash_start_address;
	extern char	*symhash_end_address;
	extern int	symtab_locals;
	extern sdt_probedesc_t	*static_sdt_probedesc;
	extern tnf_tag_data_t	*static_tnf_taglist;
	extern tnf_probe_control_t	*static_tnf_probelist;
	extern char	__bss_start__[];
	extern char	__bss_end__[];

	/* Initialize modctl for unix. */
	cp->mod_filename = STATIC_UNIX_FILENAME;
	cp->mod_modname = STATIC_UNIX_MODNAME;
	cp->mod_prim = 1;
	cp->mod_loaded = 1;
	cp->mod_installed = 1;
	cp->mod_loadcnt = 1;
	cp->mod_loadflags = MOD_NOAUTOUNLOAD|MOD_NOUNLOAD;
	cp->mod_prev = cp->mod_next = cp;
	cp->mod_mp = mp;
	cp->mod_text = s_text;
	cp->mod_text_size = (size_t)(e_text - s_text);
	cp->mod_id = last_module_id;
	last_module_id++;

	/*
	 * Initialize module structure.
	 */
	mp->symhdr = &shdr_unix[0];
	mp->strhdr = &shdr_unix[1];

	/* Setup symbol table entries. */
	mp->symspace = mp->symtbl = symtab_start_address;
	mp->symhdr->sh_addr = (Addr)mp->symspace;
	mp->symhdr->sh_size = symtab_end_address - symtab_start_address;
	mp->symhdr->sh_entsize = sizeof(Sym);
	mp->nsyms = mp->symhdr->sh_size / sizeof(Sym);
	BOOT_DPRINTF("symtab: addr = 0x%p, size = 0x%p\n",
		     (void *)mp->symhdr->sh_addr, (void *)mp->symhdr->sh_size);
	BOOT_DPRINTF("nsyms = %d, entsize = %d, locals = %d\n",
		     mp->nsyms, mp->symhdr->sh_entsize, symtab_locals);

	/* Setup string table. */
	mp->strings = strtab_start_address;
	mp->strhdr->sh_addr = (Addr)mp->strings;
	mp->strhdr->sh_size = strtab_end_address - strtab_start_address;
	BOOT_DPRINTF("strtab: addr = 0x%p, size = 0x%p\n",
		     (void *)mp->strhdr->sh_addr, (void *)mp->symhdr->sh_size);

	/*
	 * Setup symbol hash table.
	 * We don't need to reconstruct symbol hash because it should be
	 * done by build environment.
	 */
	hashptr = (symid_t *)symhash_start_address;
	ASSERT(*(hashptr + 1) == mp->nsyms);
	mp->hashsize = *hashptr;
	mp->buckets = hashptr + 2;
	mp->chains = mp->buckets + mp->hashsize;
	ASSERT((char *)(mp->chains + mp->nsyms) == symhash_end_address);
	BOOT_DPRINTF("hashsize = %d, buckets = 0x%p, chains = 0x%p\n",
		     mp->hashsize, mp->buckets, mp->chains);

	mp->symsize = mp->symhdr->sh_size + mp->strhdr->sh_size +
		(symhash_end_address - symhash_start_address);

	mp->flags = KOBJ_EXEC|KOBJ_PRIM|KOBJ_RELOCATED|KOBJ_EXPORTED|
		KOBJ_RESOLVED;

	mp->shdrs = (char *)shdr_unix;
	mp->symtbl_section = 0;

	mp->symhdr->sh_type = SHT_SYMTAB;
	mp->symhdr->sh_link = 1;
	mp->symhdr->sh_info = symtab_locals;
	mp->symhdr->sh_addralign = sizeof(Addr);

	mp->strhdr->sh_type = SHT_STRTAB;
	mp->strhdr->sh_addralign = 1;

	/*
	 * Construct ELF header by hand.
	 * This is required to dump kernel symbol via /dev/ksyms.
	 */
	ehdr = &mp->hdr;
	ehdr->e_ident[EI_MAG0] = ELFMAG0;
	ehdr->e_ident[EI_MAG1] = ELFMAG1;
	ehdr->e_ident[EI_MAG2] = ELFMAG2;
	ehdr->e_ident[EI_MAG3] = ELFMAG3;
	ehdr->e_ident[EI_CLASS] = UNIX_ELF_CLASS;
	ehdr->e_ident[EI_DATA] = UNIX_ELF_DATA;
	ehdr->e_ident[EI_VERSION] = EV_CURRENT;
	ehdr->e_ident[EI_OSABI] = 0;
	ehdr->e_ident[EI_ABIVERSION] = 0;
	for (i = EI_PAD; i < EI_NIDENT; i++) {
		ehdr->e_ident[i] = 0;
	}
	ehdr->e_type = ET_EXEC;
	ehdr->e_machine = UNIX_ELF_MACHINE;
	ehdr->e_version = 1;
	ehdr->e_entry = UNIX_ELF_ENTRY;
	ehdr->e_phoff = 0;
	ehdr->e_shoff = 0;
	ehdr->e_flags = UNIX_ELF_FLAGS;
	ehdr->e_ehsize = sizeof(Ehdr);
	ehdr->e_phentsize = sizeof(Phdr);
	ehdr->e_shentsize = sizeof(Shdr);
	ehdr->e_shnum = 2;
	ehdr->e_shstrndx = 1;

	/* CTF data will be initialized later. */
	mp->ctfdata = NULL;
	mp->ctfsize = 0;

	mp->text = cp->mod_text;
	mp->text_size = cp->mod_text_size;
	mp->data = s_data;
	mp->data_size = (size_t)(e_data - s_data);
	mp->bss_align = 8;	/* BSS is 8 bytes aligned. */
	mp->bss = (uintptr_t)__bss_start__;
	mp->bss_size = __bss_end__ - __bss_start__;

	BOOT_DPRINTF("mp->text = 0x%08lx\n", (uintptr_t)mp->text);
	BOOT_DPRINTF("mp->text_size = 0x%08lx\n", mp->text_size);
	BOOT_DPRINTF("mp->data = 0x%08lx\n", (uintptr_t)mp->data);
	BOOT_DPRINTF("mp->data_size = 0x%08lx\n", mp->data_size);

	mp->filename = cp->mod_filename;

	/* Setup sdt and tnf probe list. */
	mp->sdt_probes = static_sdt_probedesc;
	if (tnf_splice_probes(1, static_tnf_probelist, static_tnf_taglist)) {
		mp->flags |= KOBJ_TNF_PROBE;
	}

	/*
	 * Append modctl to kobj_linkmaps.
	 * Note that we can't use kobj_lm_append() because kobj_zalloc()
	 * is not ready.
	 */
	BOOT_ALLOC(lp, struct modctl_list *, sizeof(*lp), BO_NO_ALIGN,
		   "Failed to allocate modctl_list");
	lp->modl_modp = cp;
	lp->modl_next = NULL;
	kobj_linkmaps[KOBJ_LM_PRIMARY] = lp;
}

/*
 * void
 * mod_static_ctf_init(void *addr, size_t size)
 *	Initialize CTF data for static kernel.
 *	The specified address must be contained by heap_arena because
 *	mod_static_ctf_free() will release address to heap_arena.
 */
void
mod_static_ctf_init(void *addr, size_t size)
{
	struct module	*mp = &module_unix;

	mp->ctfdata = addr;
	mp->ctfsize = size;

	PRM_PRINTF("UNIX CTF data: addr=0x%p, size=0x%lx\n", addr, size);

	unix_ctf_address = addr;
	membar_producer();
}

/*
 * boolean_t
 * mod_static_ctf_update(struct module *mp, void *newaddr, size_t newsize)
 *	Update CTF data for static kernel.
 *	Release pages that contains old CTF data, and set new data for
 *	static modules.
 *
 * Calling/Exit State:
 *	mod_static_ctf_update() returns B_TRUE if the specified module is
 *	static-linked module. Otherwise it returns B_FALSE.
 *
 *	The caller must guarantee that mp->ctfdata is not NULL.
 */
boolean_t
mod_static_ctf_update(struct module *mp, void *newaddr, size_t newsize)
{
	void		*addr = mp->ctfdata;
	size_t		size = mp->ctfsize, mapsize;
	modctl_t	*modp;

	ASSERT(MUTEX_HELD(&mod_lock));
	ASSERT(addr != NULL);

	if (!MOD_IS_STATIC_MODULE(mp)) {
		return B_FALSE;
	}

	if (atomic_cas_ptr(&unix_ctf_address, addr, NULL) != addr) {
		/* No CTF data, or already released. */
		return B_TRUE;
	}

	/* Free old CTF pages. */
	mapsize = P2ROUNDUP(size, PAGESIZE);
	segkmem_free(heap_arena, addr, mapsize);

	/* Install new CTF data for all static modules. */
	modp = &modules;
	do {
		if (modp == &modules || modp->mod_static != NULL) {
			struct module	*mp = modp->mod_mp;

			if (mp != NULL) {
				ASSERT(MOD_IS_STATIC_MODULE(mp));
				mp->ctfdata = NULL;
				membar_producer();
				mp->ctfsize = newsize;
				mp->ctfdata = newaddr;
				membar_producer();
			}
		}
	} while ((modp = modp->mod_next) != &modules);

	return B_TRUE;
}

/*
 * STAITC_UNIX version of make_aliases().
 * This function read static_drvalias[] instead of /etc/driver_aliases.
 * This replaces make_aliases in modsysfile.c because only called from
 * startup sequence.
 */
void
make_aliases(struct bind **bhash)
{
	int	i;

	static char dupwarn[] = "!Driver alias \"%s\" conflicts with "
		"an existing driver name or alias.";

	for (i = 0; i < static_drvalias_count; i++) {
		const char *aliasname = static_drvalias[i].sb_name;
		if (make_mbind((char *)aliasname, static_drvalias[i].sb_number,
				NULL, bhash) != 0) {
			cmn_err(CE_WARN, dupwarn, aliasname);
		}
	}
}

/*
 * STAITC_UNIX version of read_class_file().
 * This function read static_drvclass[] instead of /etc/driver_classes.
 * This replaces read_class_file in modsysfile.c because only called from
 * startup sequence.
 */
void
read_class_file(void)
{
	struct hwc_class *hcl, *hcl1;
	int	i;

	if (hcl_head != NULL) {
		hcl = hcl_head;
		while (hcl != NULL) {
			kmem_free(hcl->class_exporter,
			    strlen(hcl->class_exporter) + 1);
			hcl1 = hcl;
			hcl = hcl->class_next;
			kmem_free(hcl1, sizeof (struct hwc_class));
		}
		hcl_head = NULL;
	}

	for (i = 0; i < static_drvclass_count; i++) {
		add_class((char *)static_drvclass[i].ss_name,
			(char *)static_drvclass[i].ss_value);
	}
}

static char binding_dupwarn[] = "!The binding file entry \"%s %u\" conflicts "
		"with a previous entry";

/*
 * STAITC_UNIX version of read_binding_file() for major binding.
 * This function read static_majbind instead of /etc/name_to_major.
 */
int
make_major_binding(struct bind **hashtab,
	int (*line_parser)(char *, int, char *, struct bind **))
{
	int	maxnum = 0;
	int	i;

	if (hashtab != NULL) {
		clear_binding_hash(hashtab);
	}
	if (line_parser == NULL) {
		return 0;
	}

	for (i = 0; i < static_majbind_count; i++) {
		const char *name = static_majbind[i].sb_name;
		int val = static_majbind[i].sb_number;
		if ((*line_parser)((char *)name, val, NULL, hashtab) == 0) {
			maxnum = MAX(val, maxnum);
		} else {
			cmn_err(CE_WARN, binding_dupwarn, name, val);
		}
	}
	return maxnum;
}

/*
 * STAITC_UNIX version of read_binding_file() for sysnum binding.
 * This function read static_majbind instead of /etc/name_to_sysnum.
 */
int
make_sysnum_binding(struct bind **hashtab,
	int (*line_parser)(char *, int, char *, struct bind **))
{
	int	maxnum = 0;
	int	i;

	if (hashtab != NULL) {
		clear_binding_hash(hashtab);
	}
	if (line_parser == NULL) {
		return 0;
	}

	for (i = 0; i < static_sysbind_count; i++) {
		const char *name = static_sysbind[i].sb_name;
		int val = static_sysbind[i].sb_number;
		if ((*line_parser)((char *)name, val, NULL, hashtab) == 0) {
			maxnum = MAX(val, maxnum);
		} else {
			cmn_err(CE_WARN, binding_dupwarn, name, val);
		}
	}
	return maxnum;
}

static void
insert_dacf(dacf_devspec_t spec_type, int count, const dacf_rule_t rules[])
{
	int	i;
	static char w_insert[] = "failed to register rule";

	for (i = 0; i < count; i++) {
		const dacf_rule_t *rp = &rules[i];
		if (dacf_rule_insert(spec_type, rp->r_devspec_data,
				rp->r_module, rp->r_opset, rp->r_opid,
				rp->r_opts, rp->r_args) < 0) {
			cmn_err(CE_WARN, w_insert);
		}
	}
}

/*
 * STAITC_UNIX version of read_dacf_binding_file().
 * This function read pre-build structures instead of /etc/dacf.conf file,
 * and build the dacf_rule_t database.
 */
int
make_dacf_binding()
{
	if (dacfdebug & DACF_DBG_MSGS) {
		printf("dacf debug: clearing rules database\n");
	}

	mutex_enter(&dacf_lock);
	dacf_clear_rules();

	if (dacfdebug & DACF_DBG_MSGS) {
		printf("dacf debug: parsing embedded dacf.conf\n");
	}

	insert_dacf(DACF_DS_MIN_NT,
		static_posta_mntype_count, static_posta_mntype);
	insert_dacf(DACF_DS_MIN_NT,
		static_pred_mntype_count, static_pred_mntype);
	insert_dacf(DACF_DS_DRV_MNAME,
		static_posta_mname_count, static_posta_mname);
	insert_dacf(DACF_DS_DRV_MNAME,
		static_pred_mname_count, static_pred_mname);
	insert_dacf(DACF_DS_DEV_PATH,
		static_posta_devname_count, static_posta_devname);
	insert_dacf(DACF_DS_DEV_PATH,
		static_pred_devname_count, static_pred_devname);

	if (dacfdebug & DACF_DBG_MSGS) {
		printf("\ndacf debug: done!\n");
	}

	mutex_exit(&dacf_lock);

	return 0;
}

/*
 * static mod_static_t *
 * mod_static_lookup(char *filename)
 *	If the specified module path is linked statically, return mod_static
 *	structure pointer associated with the module. Otherwise NULL.
 */
static mod_static_t *
mod_static_lookup(char *filename)
{
	mod_static_t	**mspp;
	int	i;
	char	*sep, *namep;
	size_t	nslen;

	if ((sep = strrchr(filename, '/')) == NULL) {
		/* No subdirectory specified. */
		for (mspp = static_modules, i = 0; i < static_modules_count;
		     i++, mspp++) {
			mod_static_t	*msp = *mspp;

			if (strcmp(msp->ms_modname, filename) == 0) {
				return msp;
			}
		}

		return NULL;
	}

	/* Check module name and namespaces. */
	namep = sep + 1;
	nslen = sep - filename;
	for (mspp = static_modules, i = 0; i < static_modules_count;
	     i++, mspp++) {
		mod_static_t	*msp = *mspp;
		const char	**ns;

		for (ns = msp->ms_namespace; *ns != NULL; ns++) {
			if (strncmp(filename, *ns, nslen) == 0 &&
			    strcmp(msp->ms_modname, namep) == 0) {
				return msp;
			}
		}
	}

	return NULL;
}

/*
 * boolean_t
 * mod_is_static_linked(char *filename)
 *	Determine whether the specified module is static-linked.
 *
 * Calling/Exit State:
 *	Return B_TRUE if the specified module is static-linked.
 *	Otherwise B_FALSE.
 */
boolean_t
mod_is_static_linked(char *filename)
{
	boolean_t ret = (mod_static_lookup(filename) != NULL);

	return ret;
}

/*
 * boolean_t
 * mod_is_static_module(struct module *mp)
 *	Determine whether the specified module is static-linked module.
 *
 * Calling/Exit State:
 *	Return B_TRUE if the specified module is static-linked.
 *	Otherwise B_FALSE.
 */
boolean_t
mod_is_static_module(struct module *mp)
{
	return (boolean_t)MOD_IS_STATIC_MODULE(mp);
}

/*
 * int
 * mod_static_load(modctl_t *modp)
 *	Load static-linked module.
 *	We assume that this function is called from kobj_load_module().
 *
 * Calling/Exit State:
 *	Upon successful completion, mod_static_load() returns 0.
 *	If the specified module is not statically linked, return -1.
 *	Otherwise, return error number that indicates cause of error.
 */
int
mod_static_load(modctl_t *modp)
{
	mod_static_t	*msp;
	struct module	*mp;
	extern void	kobj_notify(int type, modctl_t *modp);

	if ((msp = mod_static_lookup(modp->mod_filename)) == NULL) {
		/* Module is not statically linked. */
		return -1;
	}

	PRM_PRINTF("modname = %s\n", modp->mod_filename);
	modp->mod_static = msp;

	mp = kobj_zalloc(sizeof(struct module), KM_WAIT);

	kobj_static_notify(KOBJ_NOTIFY_MODLOADING, modp);

	/* Copy unix module information. */
	bcopy(&module_unix, mp, sizeof(struct module));

	mp->filename = modp->mod_filename;
	mp->fbt_tab = NULL;
	mp->fbt_size = 0;
	mp->fbt_nentries = 0;
	mp->sdt_probes = NULL;
	mp->sdt_nprobes = 0;
	mp->sdt_tab = NULL;
	mp->sdt_size = 0;

	mutex_enter(&mod_lock);
	modp->mod_mp = mp;
	modp->mod_gencount++;
	mutex_exit(&mod_lock);

	modp->mod_text = mp->text;
	modp->mod_text_size = mp->text_size;
	modp->mod_loadflags = MOD_NOAUTOUNLOAD|MOD_NOUNLOAD;

	if (msp->ms_depends != NULL) {
		const mod_static_t	**mspp;

		/* Load dependant modules. */
		for (mspp = msp->ms_depends; *mspp != NULL; mspp++) {
			const mod_static_t	*depend = *mspp;
			const char	*ns = *(depend->ms_namespace);
			char		modpath[MODMAXNAMELEN];
			modctl_t	*req;

			ASSERT(ns);
			ASSERT(strlen(ns) + strlen(depend->ms_modname) <
			       MODMAXNAMELEN - 2);
			snprintf(modpath, MODMAXNAMELEN, "%s/%s", ns,
				 depend->ms_modname);
			req = mod_load_requisite(modp, modpath);
			if (req == NULL) {
				cmn_err(CE_PANIC,
					"Can't resolve static module "
					"dependency: %s: %s",
					modp->mod_filename,
					modpath);
				/* NOTREACHED */
			}

			kobj_static_add_dependent(mp, req->mod_mp);
			mod_release_mod(req);
		}
	}

	mp->flags |= KOBJ_RELOCATED|KOBJ_EXPORTED|KOBJ_RESOLVED;

	kobj_static_notify(KOBJ_NOTIFY_MODLOADED, modp);

	return 0;
}

/*
 * int
 * mod_static_install(modctl_t *modp)
 *	Call module _init() entry.
 *
 * Calling/Exit State:
 *	Upon successful completion, mod_static_install() returns 0.
 *	If the specified module is not statically linked, return -1.
 *	Otherwise, return error number that indicates cause of error.
 */
int
mod_static_install(modctl_t *modp)
{
	mod_static_t	*msp = modp->mod_static;
	int		ret;

	if (msp == NULL) {
		return -1;
	}

	ASSERT(msp->ms_init);
	ret = msp->ms_init();	/* Call entry point. */

	if (ret == 0) {
		install_stubs(modp);
		modp->mod_installed = 1;
	}

	PRM_PRINTF("%s:_init() => %d\n", modp->mod_filename, ret);

	return ret;
}

/*
 * int
 * mod_static_getinfo(modctl_t *modp, struct modinfo *modinfop, int *rvalp)
 *	Call _info() entry for the specified module.
 *
 * Calling/Exit State:
 *	Upon successful completion, mod_static_getinfo() returns 0 and
 *	set return value of _info() entry to *rval;
 *	If the specified module is not statically linked, return -1.
 */
int
mod_static_getinfo(modctl_t *modp, struct modinfo *modinfop, int *rvalp)
{
	mod_static_t	*msp = modp->mod_static;
	int		ret;

	if (msp == NULL) {
		return -1;
	}

	if (msp->ms_info == NULL) {
		cmn_err(CE_WARN, "_info() not defined properly in %s",
			modp->mod_filename);
		ret = 0;
	}
	else {
		ret = msp->ms_info(modinfop);
	}

	*rvalp = ret;
	return 0;
}

/*
 * int
 * mod_static_unload(modctl_t *modp)
 *	Unload static-linked module.
 *	This function is called only from error handler.
 *
 * Calling/Exit State:
 *	If the specified module is not statically linked, return -1.
 *	Otherwise 0.
 */
int
mod_static_unload(modctl_t *modp)
{
	struct module		*mp;
	struct module_list	*lp, *nextlp;

	if (modp->mod_static == NULL) {
		return -1;
	}

	/* Clear mod_mp first. */
	mp = modp->mod_mp;
	mutex_enter(&mod_lock);
	modp->mod_mp = NULL;
	mutex_exit(&mod_lock);

	modp->mod_static = NULL;

	kobj_static_notify(KOBJ_NOTIFY_MODUNLOADED, modp);

	/* Free up module_list. */
	for (lp = mp->head; lp != NULL; lp = nextlp) {
		nextlp = lp->next;
		kobj_free((char *)lp, sizeof(*lp));
	}

	kobj_free(mp, sizeof(struct module));
	return 0;
}

/*
 * void
 * mod_static_load_all(boolean_t stub)
 *	Load all static-linked modules.
 *	If stub is true, load all static-linked modules that have stub entries
 *	in modstubs.s.
 */
void
mod_static_load_all(boolean_t stub)
{
	mod_static_t	**mspp;
	int	i;

	for (mspp = static_modules, i = 0; i < static_modules_count;
	     i++, mspp++) {
		mod_static_t	*msp = *mspp;
		const char	**ns = msp->ms_namespace;

		if (stub && !(msp->ms_flags & MS_STUB)) {
			continue;
		}
		PRM_PRINTF("load_all(%s): %s\n", (stub) ? "stub" : "all",
			   msp->ms_modname);
		(void)modload((char *)*ns, (char *)msp->ms_modname);
	}
}

/*
 * int
 * mod_static_load_drvconf(char *fname, struct par_list **pl,
 *			   ddi_prop_t **props)
 *	Load driver.conf file embedded in static kernel.
 *
 * Calling/Exit State:
 *	Upon successful completion, mod_static_load_drvconf() returns 0.
 *	If the specified driver.conf file is not embedded in kernel,
 *	returns -1.
 */
int
mod_static_load_drvconf(char *fname, struct par_list **pl, ddi_prop_t **props,
			hwc_addspec_t addspec)
{
	char	modname[MODMAXNAMELEN], *p, *first = NULL, *last = NULL;
	size_t	len = strlen(fname);
	mod_static_t	*msp;
	struct hwc_spec	*hwcp;
	ddi_prop_t	**tailpp;
	int	i;

	/* Parse module name. */
	for (p = fname + len - 1;
	     p >= fname && (first == NULL || last == NULL); p--) {
		if (*p == '.') {
			last = p;
		}
		else if (*p == '/') {
			first = p + 1;
		}
	}
	if (first == NULL || last == NULL || first >= last) {
		return -1;
	}

	len = last - first;
	if (len >= MODMAXNAMELEN) {
		len = MODMAXNAMELEN - 1;
	}
	strncpy(modname, first, len);
	modname[len] = '\0';

	msp = mod_static_lookup(modname);
	if (msp == NULL) {
		if (!modrootloaded) {
			major_t	major = mod_name_to_major(modname);

			/*
			 * Keep major number of this module.
			 * mod_static_rescan() will try to load again after
			 * root filesystem is loaded.
			 */
			if (major != (major_t)-1) {
				cfmajor_t	*cmp;

				cmp = (cfmajor_t *)kmem_alloc(sizeof(*cmp),
							      KM_SLEEP);
				cmp->c_major = major;
				cmp->c_next = rescan_major;
				rescan_major = cmp;
			}
		}
		return -1;
	}

	if (msp->ms_hwc == NULL || msp->ms_nhwc == 0) {
		return -1;
	}

	/* Global property should be linked to the tail of the list. */
	tailpp = props;
	while (*tailpp) {
		tailpp = &((*tailpp)->prop_next);
	}

	for (i = 0, hwcp = (struct hwc_spec *)msp->ms_hwc;
	     i < msp->ms_nhwc; i++, hwcp++) {
		struct hwc_spec	*newhwcp;
		ddi_prop_t	*prop;

		if (hwcp->hwc_devi_name == NULL) {
			if (hwcp->hwc_parent_name || hwcp->hwc_class_name) {
				cmn_err(CE_WARN, "%s: missing name attribute",
					fname);
				continue;
			}

			prop = hwcp->hwc_devi_sys_prop_ptr;
			if (prop) {
				ddi_prop_t	*newprop, **pp;

				/* Add to global property list */
				newprop = ddi_prop_list_copy(prop);
				*tailpp = newprop;
				pp = &(newprop->prop_next);
				while (*pp) {
					pp = &((*pp)->prop_next);
				}
				tailpp = pp;
			}
			continue;
		}

		/*
		 * This is a node spec, either parent or class must be
		 * specified.
		 */
		if (hwcp->hwc_parent_name == NULL &&
		    hwcp->hwc_class_name == NULL) {
			cmn_err(CE_WARN, "%s: missing parent or class "
				"attribute", fname);
			continue;
		}

		/* Add to node spec list */
		newhwcp = hwc_spec_copy(hwcp);
		(*addspec)(newhwcp, pl);
	}

	while (*props) {
		props = &((*props)->prop_next);
	}

	PRM_PRINTF("%s has been loaded.\n", fname);

	return 0;
}

/*
 * static void
 * mod_static_rescan_devinfo(struct devnames *dnp)
 *	Reconfigure device node if new driver.conf is loaded from root
 *	filesystem.
 */
static void
mod_static_rescan_devinfo(struct devnames *dnp)
{
	dev_info_t	*dip, **table, **dipp;
	size_t		cnt, size;

	if (dnp->dn_global_prop_ptr == NULL) {
		/* No property to be copied. */
		return;
	}

	/*
	 * Count number of driver instances.
	 * We can iterate dev_info list without devnames lock because
	 * mod_static_rescan_devinfo() is called while single-threaded.
	 */
	for (dip = dnp->dn_head, cnt = 0; dip != NULL;
	     dip = (dev_info_t *)DEVI(dip)->devi_next, cnt++);
	if (cnt == 0) {
		return;
	}

	/*
	 * We need to preserve all dev_info address because the following
	 * procedure may change dev_info list.
	 */
	size = sizeof(dev_info_t *) * cnt;
	table = (dev_info_t **)kmem_alloc(size, KM_SLEEP);
	for (dipp = table, dip = dnp->dn_head; dip != NULL;
	     *dipp = dip, dipp++, dip = (dev_info_t *)DEVI(dip)->devi_next);
	ASSERT(dipp == table + cnt);

	for (dipp = table; dipp < table + cnt; dipp++) {
		ddi_node_state_t	state;
		int		circ;
		dev_info_t	*pdip;

		dip = *dipp;
		pdip = ddi_get_parent(dip);
		ndi_devi_enter(pdip, &circ);

		state = i_ddi_node_state(dip);
		PRM_PRINTF("rescan_devinfo: dip=0x%p, state=%d\n",
			   dip, state);
		if (state < DS_BOUND) {
			(void)ndi_devi_bind_driver(dip, 0);
		}
		else if (state <= DS_INITIALIZED) {
			int	ret;

			/*
			 * We must downgrade driver state to DS_LINKED once
			 * because driver's global property list will be copied
			 * to each instance when its state is changed to
			 * DS_BOUND.
			 */
			ret = i_ndi_unconfig_node(dip, DS_LINKED, 0);
			if (ret == DDI_SUCCESS) {
				(void)i_ndi_config_node(dip, state, 0);
			}
		}
		ndi_devi_exit(pdip, circ);
	}

	kmem_free(table, size);
}

/*
 * static void
 * mod_static_rescan_pseudo(void)
 *	Reconfigure pseudo drivers according to driver.conf in root filesystem.
 *
 *	This function derives pseudo drivers from .conf spec, and create
 *	driver instance if it is not yet created.
 */
static void
mod_static_rescan_pseudo(void)
{
	dev_info_t	*pdip = pseudo_dip;
	int		circ;
	struct hwc_spec	*list, *spec;

	ndi_devi_enter(pdip, &circ);
	list = hwc_get_child_spec(pdip, (major_t)-1);
	for (spec = list; spec != NULL; spec = spec->hwc_next) {
		dev_info_t	*dip;
		char		*name;
		major_t		major;
		struct devnames	*dnp;

		if ((name = spec->hwc_devi_name) == NULL) {
			continue;
		}
		if ((major = ddi_name_to_major(name)) == (major_t)-1) {
			continue;
		}

		dnp = &devnamesp[major];
		if (dnp->dn_head != NULL) {
			/* Already initialized. */
			continue;
		}

		/* Allocate new driver instance. */
		dip = i_ddi_alloc_node(pdip, name, (pnode_t)DEVI_PSEUDO_NODEID,
				       -1, spec->hwc_devi_sys_prop_ptr,
				       KM_SLEEP);
		if (dip == NULL) {
			continue;
		}

		if (ddi_initchild(pdip, dip) != DDI_SUCCESS) {
			(void)ddi_remove_child(dip, 0);
		}
	}
	hwc_free_spec_list(list);
	ndi_devi_exit(pdip, circ);
}

/*
 * void
 * mod_static_rescan(void)
 *	Scan driver.conf again after root filesystem is loaded.
 *
 *	mod_static_load_drvconf() is called before root filesystem is loaded,
 *	and it keeps major number of drivers that was not loaded.
 *	mod_static_rescan() will scan those major numbers and try to load
 *	driver.conf files from root filesystem.
 */
void
mod_static_rescan(void)
{
	cfmajor_t	*cmp, *next;
	boolean_t	rescan = B_FALSE;

	ASSERT(modrootloaded);

	for (cmp = rescan_major; cmp != NULL; cmp = next) {
		major_t	major = cmp->c_major;
		struct devnames	*dnp = &devnamesp[major];

		next = cmp->c_next;

		if (!(dnp->dn_flags & DN_CONF_PARSED)) {
			(void)i_ddi_load_drvconf(major);
			if (dnp->dn_flags & DN_CONF_PARSED) {
				rescan = B_TRUE;

				/*
				 * We need to sync driver's global property
				 * list.
				 */
				mod_static_rescan_devinfo(dnp);
			}
		}

		kmem_free(cmp, sizeof(*cmp));
	}

	if (rescan) {
		/* Need to rescan pseudo drivers. */
		mod_static_rescan_pseudo();
	}

	rescan_major = NULL;
}

/*
 * void
 * mod_static_forceload_setup(void)
 *	Initialize module forceload for STATIC_UNIX environment.
 *
 *	On STATIC_UNIX kernel, forceload option is passed via boot option.
 *	mod_static_forceload_setup() constructs module list to be loaded
 *	at boot time from boot option.
 *
 * Remarks:
 *	Modules specified by "forceload-early" option will be loaded here.
 */
void
mod_static_forceload_setup(void)
{
	mod_forceload_t	*early;

	/* Derive modules to be loaded at boot time from boot option. */
	early = mod_static_forceload_get("forceload-early");
	mod_forceload_list = mod_static_forceload_get("forceload");

	/* Load modules specified by "forceunload-early". */
	mod_static_do_forceload(early);
}

/*
 * void
 * mod_static_forceload(void)
 *	Load modules specified by "forceload" boot option.
 */
void
mod_static_forceload(void)
{
	mod_static_do_forceload(mod_forceload_list);
	mod_forceload_list = NULL;
}

/*
 * static mod_forceload_t *
 * mod_static_forceload_get(char *propname)
 *	Construct mod_forceload list from the specified boot option.
 */
static mod_forceload_t *
mod_static_forceload_get(char *propname)
{
	size_t		proplen;
	char		*prop, *p, *endp, *name;
	mod_forceload_t	*list, **nextpp;

	proplen = BOP_GETPROPLEN(bootops, propname);
	if (proplen == 0 || proplen == (size_t)BOOT_FAILURE) {
		/* No module is specified. */
		return NULL;
	}

	prop = (char *)kmem_alloc(proplen + 1, KM_SLEEP);
	if (BOP_GETPROP(bootops, (char *)propname, prop) < 0) {
		cmn_err(CE_WARN, "Can't read value of \"%s\"", propname);
		return NULL;
	}
	*(prop + proplen) = '\0';

	list = NULL;
	nextpp = &list;
	for (name = prop, p = prop, endp = prop + proplen; p < endp; p++) {
		char		c = *p, *nm;
		size_t		len;
		mod_forceload_t	*mfp;

		if (c != BOP_FORCELOAD_SEP && c != '\0') {
			continue;
		}

		len = p - name;
		if (len == 0) {
			continue;
		}

		nm = kmem_alloc(len + 1, KM_SLEEP);
		bcopy(name, nm, len);
		*(nm + len) = '\0';
		mfp = (mod_forceload_t *)kmem_alloc(sizeof(*mfp), KM_SLEEP);
		mfp->mf_name = nm;
		mfp->mf_namesize = len + 1;
		*nextpp = mfp;
		nextpp = &(mfp->mf_next);
		name = p + 1;
	}
	*nextpp = NULL;

	return list;
}

/*
 * static void
 * mod_static_do_forceload(mod_forceload_t *list)
 *	Load modules in the specified module list.
 *
 * Remarks:
 *	mod_static_do_forceload() releases all resources in the specified
 *	mod_forceload list.
 */
static void
mod_static_do_forceload(mod_forceload_t *list)
{
	mod_forceload_t	*mfp, *next;

	for (mfp = list; mfp != NULL; mfp = next) {
		char	*name = mfp->mf_name;

		next = mfp->mf_next;

		/* Load module. */
		if (modload(NULL, name) != -1) {
			struct modctl	*modp;

			/* Prevent this module from autounloading. */
			modp = mod_find_by_filename(NULL, name);
			if (modp != NULL) {
				modp->mod_loadflags |= MOD_NOAUTOUNLOAD;
			}

			/* Attempt to install driver here. */
			if (strncmp(name, "drv", 3) == 0) {
				(void)ddi_install_driver(name + 4);
			}
		}
		else {
			cmn_err(CE_WARN, "forceload of %s failed", name);
		}

		/* Release mod_forceload. */
		kmem_free(name, mfp->mf_namesize);
		kmem_free(mfp, sizeof(*mfp));
	}
}

/*
 * Local version of strdup();
 */
static char *
local_strdup(char *str)
{
	char	*ret;

	if (str != NULL) {
		size_t	len = strlen(str);

		ret = kmem_alloc(len + 1, KM_SLEEP);
		strcpy(ret, str);
	}
	else {
		ret = NULL;
	}

	return ret;
}

/*
 * Create deep copy of hwc_spec.
 */
static struct hwc_spec *
hwc_spec_copy(struct hwc_spec *hwcp)
{
	struct hwc_spec	*new;
	ddi_prop_t	*prop, *nprop;

	new = (struct hwc_spec *)kmem_alloc(sizeof(struct hwc_spec), KM_SLEEP);
	new->hwc_next = NULL;
	new->hwc_parent_name = local_strdup(hwcp->hwc_parent_name);
	new->hwc_class_name = local_strdup(hwcp->hwc_class_name);
	new->hwc_devi_name = local_strdup(hwcp->hwc_devi_name);
	new->hwc_hash_next = NULL;
	new->hwc_major = 0;
	new->hwc_devi_sys_prop_ptr =
		ddi_prop_list_copy(hwcp->hwc_devi_sys_prop_ptr);

	return new;
}

/*
 * Create deep copy of ddi_prop_t.
 */
static ddi_prop_t *
ddi_prop_copy(ddi_prop_t *prop)
{
	ddi_prop_t	*new;

	new = (ddi_prop_t *)kmem_alloc(sizeof(ddi_prop_t), KM_SLEEP);
	new->prop_next = NULL;
	new->prop_dev = prop->prop_dev;
	new->prop_name = local_strdup(prop->prop_name);
	new->prop_flags = prop->prop_flags;
	new->prop_len = prop->prop_len;
	if (prop->prop_len > 0 && prop->prop_val != NULL) {
		new->prop_val = kmem_alloc(prop->prop_len, KM_SLEEP);
		bcopy(prop->prop_val, new->prop_val, prop->prop_len);
	}
	else {
		new->prop_val = NULL;
	}

	return new;
}

/*
 * Create deep copy of ddi_prop_t chain.
 */
static ddi_prop_t *
ddi_prop_list_copy(ddi_prop_t *prop)
{
	ddi_prop_t	*head = NULL, **headpp;

	headpp = &head;
	for (; prop != NULL; prop = prop->prop_next) {
		ddi_prop_t	*new;

		new = ddi_prop_copy(prop);
		*headpp = new;
		headpp = &(new->prop_next);
	}

	return head;
}
#endif	/* STATIC_UNIX */
