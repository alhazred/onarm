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

#ifndef	_RTLD_H
#define	_RTLD_H

#pragma ident	"@(#)rtld.h	1.128	08/03/03 SMI"

/*
 * Global include file for the runtime linker.
 */
#include <time.h>
#include <sgs.h>
#include <thread.h>
#include <synch.h>
#include <machdep.h>
#include <sys/avl.h>
#include <alist.h>
#include <libc_int.h>

#ifdef	_SYSCALL32
#include <inttypes.h>
#endif

#ifdef	__cplusplus
extern "C" {
#endif


/*
 * Linked list of directories or filenames (built from colon separated string).
 */
typedef struct pnode {
	const char	*p_name;
	const char	*p_oname;
	size_t		p_len;
	uint_t		p_orig;
	void		*p_info;
	struct pnode	*p_next;
} Pnode;

typedef struct rt_map	Rt_map;

/*
 * A binding descriptor.  Establishes the binding relationship between two
 * objects, the caller (originator) and the dependency (destination).
 */
typedef struct {
	Rt_map		*b_caller;	/* caller (originator) of a binding */
	Rt_map		*b_depend;	/* dependency (destination) of a */
					/*	binding */
	uint_t		b_flags;	/* relationship of caller to the */
					/*	dependency */
} Bnd_desc;

#define	BND_NEEDED	0x0001		/* caller NEEDED the dependency */
#define	BND_REFER	0x0002		/* caller relocation references the */
					/*	dependency */
#define	BND_FILTER	0x0004		/* pseudo binding to identify filter */

/*
 * Private structure for communication between rtld_db and rtld.
 *
 * We must bump the version number when ever an update in one of the
 * structures/fields that rtld_db reads is updated.  This hopefully permits
 * rtld_db implementations of the future to recognize core files produced on
 * older systems and deal with these core files accordingly.
 *
 * As of version 'RTLD_DB_VERSION <= 2' the following fields were valid for core
 * file examination (basically the public Link_map):
 *
 *		ADDR()
 *		NAME()
 *		DYN()
 *		NEXT()
 *		PREV()
 *
 * Valid fields for RTLD_DB_VERSION3
 *
 *		PATHNAME()
 *		PADSTART()
 *		PADIMLEN()
 *		MSIZE()
 *		FLAGS()
 *		FLAGS1()
 *
 * Valid fields for RTLD_DB_VERSION4
 *
 *		TLSMODID()
 *
 * Valid fields for RTLD_DB_VERSION5
 *
 *		Added rtld_flags & FLG_RT_RELOCED to stable flags range
 *
 */
#define	R_RTLDDB_VERSION1	1	/* base version level - used for core */
					/*	file examination */
#define	R_RTLDDB_VERSION2	2	/* minor revision - not relevant for */
					/*	core files */
#define	R_RTLDDB_VERSION3	3
#define	R_RTLDDB_VERSION4	4
#define	R_RTLDDB_VERSION5	5
#define	R_RTLDDB_VERSION	R_RTLDDB_VERSION5	/* current version */

typedef struct rtld_db_priv {
	struct r_debug	rtd_rdebug;	/* original r_debug structure */
	Word		rtd_version;	/* version no. */
	size_t		rtd_objpad;	/* padding around mmap()ed objects */
	List *		rtd_dynlmlst;	/* pointer to Dynlm_list */
} Rtld_db_priv;

#ifdef _SYSCALL32
typedef struct rtld_db_priv32 {
	struct r_debug32 rtd_rdebug;	/* original r_debug structure */
	Elf32_Word	rtd_version;	/* version no. */
	Elf32_Word	rtd_objpad;	/* padding around mmap()ed objects */
	Elf32_Addr	rtd_dynlmlst;	/* pointer to Dynlm_list */
} Rtld_db_priv32;
#endif	/* _SYSCALL32 */

/*
 * External function definitions.  ld.so.1 must convey information to libc in
 * regards to threading.  libc also provides routines for atexit() and message
 * localization.  libc provides the necessary interfaces via its RTLDINFO
 * structure and/or later _ld_libc() calls.
 *
 * These external functions are maintained for each link-map list, and used
 * where appropriate.  The functions are associated with the object that
 * provided them, so that should the object be deleted (say, from an alternative
 * link-map), the functions can be removed.
 */
typedef struct {
	Rt_map	*lc_lmp;			/* function provider */
	union {
		int		(*lc_func)();	/* external function pointer */
		uintptr_t	lc_val;		/* external value */
		char    	*lc_ptr;	/* external character pointer */
	} lc_un;
} Lc_desc;

/*
 * Link map list definition.  Link-maps are used to describe each loaded object.
 * Lists of these link-maps describe the various namespaces within a process.
 * The process executable and its dependencies are maintained on the lml_main
 * list.  The runtime linker, and its dependencies are maintained on the
 * lml_rtld list.  Additional lists can be created (see dlmopen()) for such
 * things as auditors and their dependencies.
 *
 * Each link-map list maintains an Alist of one, or more, linked lists of
 * link-maps.  For backward compatibility, the lm_head/lm_tail elements are
 * initialized to the first linked-list of link-maps:
 *
 *      Lm_list
 *    ----------
 *   | lm_tail  | ------------------------------------
 *   | lm_head  | --------------------                |
 *   |          |                     |     Rt_map    |     Rt_map
 *   |          |                     |     ------    |     ------
 *   |          |          Alist       --> |      |   |--> |      |
 *   |          |        ---------    |    |      | --     |      |
 *   | lm_lists | ----> |         |   |    |      |    --> |      |
 *   |          |       |---------|   |    |      |   |    |      |
 *   |          |       | lc_head | --      ------    |     ------
 *   |          |       | lc_tail | ------------------
 *   |          |       |---------|
 *                      | lc_head |
 *                      | lc_tail |
 *                      |---------|
 *
 * Multiple link-map lists exist to support the addition of lazy loaded
 * families, filtee families, and dlopen() families.  The intent of these
 * lists is to insure that a family of objects that are to be loaded are
 * fully relocatable, and hence usable, before they become part of the main
 * (al_data[0]) link-map control list.  This main link-map control list is
 * the only list in existence when control is transferred to user code.
 *
 * During process initialization, the dynamic executable and its non-lazy
 * dependencies are maintained on al_data[0].  If a new object is loaded, then
 * this object is added to the next available control list [1], typically
 * al_data[1].  Any dependencies of this object that have not already been
 * loaded are added to the same control list.  Once all of the objects on the
 * new control list have been successfully relocated, the objects are moved from
 * the new control list to the highest control list to which objects of the new
 * control list bound to, typically al_data[1] to al_data[0].
 *
 * Each loading scenario can be broken down as follows:
 *
 *  setup() - only the initial link-map control list is used:
 *   i.	  create al_data[0]
 *   ii.  add new link-map for main on al_data[0]
 *   iii. analyze al_data[0] to add all non-lazy dependencies
 *   iv.  relocate al_data[0] dependencies.
 *
 *  dlopen() - the initiator can only be the initial link-map control list:
 *   i.   create al_data[1] from caller al_data[0]
 *   ii.  add new link-map for the dlopen'ed object on al_data[1]
 *   iii. analyze al_data[1] to add all non-lazy dependencies
 *   iv.  relocate al_data[1] dependencies, and move to al_data[0].
 *
 *  filtee and lazy loading processing - the initiator can be any link-map
 *  control list that is being relocated:
 *   i.   create al_data[y] from caller al_data[x]
 *   ii.  add new link-map for the new object on al_data[y]
 *   iii. analyze al_data[y] to add all non-lazy dependencies
 *   iv.  relocate al_data[y] dependencies, and move to al_data[x].
 *
 * This Alist therefore maintains a stack of link-map control lists.  The newest
 * link-map control list can locate symbols within any of the former lists,
 * however, control is not passed to a former list until the newest lists
 * processing is complete.  Thus, objects can't bind to new objects until they
 * have been fully analyzed and relocated.
 *
 * [1]  Note, additional link-map control list creation occurs after the head
 * link-map object (typically the dynamic executable) has been relocated.  This
 * staging is required to satisfy the binding requirements of copy relocations.
 * Copy relocations, effectively, transfer the bindings of the copied data
 * (say _iob in libc.so.1) to the copy location (_iob in the application).
 * Thus an object that might bind to the original copy data must be redirected
 * to the copy reference.  As the knowledge of a copy relocation having taken
 * place is only known after relocating the application, link-map control list
 * additions are suspended until after this relocation has completed.
 */
typedef struct {
	Rt_map		*lc_head;
	Rt_map		*lc_tail;
	APlist		*lc_now;	/* pending promoted bind-now objects */
	uint_t		lc_flags;
} Lm_cntl;

#define	LMC_FLG_ANALYZING	0x01	/* control list is being analyzed */
#define	LMC_FLG_RELOCATING	0x02	/* control list is being relocated */
#define	LMC_FLG_REANALYZE	0x04	/* repeat analysis (established when */
					/*	interposers are added */

struct lm_list {
	/*
	 * BEGIN: Exposed to rtld_db - don't move, don't delete
	 */
	Rt_map		*lm_head;	/* linked list pointers to active */
	Rt_map		*lm_tail;	/*	link-map list */
	APlist		*lm_handle;	/* not used by rtld_db - but spacing */
					/*	is required for flags */
	Word		lm_flags;
	/*
	 * END: Exposed to rtld_db - don't move, don't delete
	 */
	Alist		*lm_rti;	/* list of RTLDINFO tables */
	Audit_list	*lm_alp;	/* audit list descriptor */
	avl_tree_t	*lm_fpavl;	/* avl tree of objects loaded */
	Alist		*lm_lists;	/* active and pending link-map lists */
	char		***lm_environ;	/* pointer to environment array */
	Word		lm_tflags;	/* transferable flags */
	uint_t		lm_obj;		/* total number of objs on link-map */
	uint_t		lm_init;	/* new obj since last init processing */
	uint_t		lm_lazy;	/* obj with pending lazy dependencies */
	uint_t		lm_tls;		/* new obj that require TLS */
	uint_t		lm_lmid;	/* unique link-map list identifier, */
	char		*lm_lmidstr;	/* and associated diagnostic string */
	APlist		*lm_actaudit;	/* list of pending audit activity */
	Lc_desc		lm_lcs[CI_MAX];	/* external libc functions */
};

#ifdef	_SYSCALL32
struct lm_list32 {
	/*
	 * BEGIN: Exposed to rtld_db - don't move, don't delete
	 */
	Elf32_Addr	lm_head;
	Elf32_Addr	lm_tail;
	Elf32_Addr	lm_handle;
	Elf32_Word	lm_flags;
	/*
	 * END: Exposed to rtld_db - don't move, don't delete
	 */
	Elf32_Addr	lm_rti;
	Elf32_Addr	lm_fpavl;
	Elf32_Addr	lm_lists;
	Elf32_Addr	lm_environ;
	Elf32_Word	lm_tflags;
	uint_t		lm_obj;
	uint_t		lm_init;
	uint_t		lm_lazy;
	uint_t		lm_tls;
	uint_t		lm_lmid;
	Elf32_Addr	lm_lmidstr;
	Elf32_Addr	lm_actaudit;
	Elf32_Addr	lm_lcs[CI_MAX];
};
#endif /* _SYSCALL32 */

/*
 * Possible Link_map list flags (Lm_list.lm_flags)
 */
/*
 * BEGIN: Exposed to rtld_db - don't move, don't delete
 */
#define	LML_FLG_BASELM		0x00000001	/* primary link-map */
#define	LML_FLG_RTLDLM		0x00000002	/* rtld link-map */
/*
 * END: Exposed to rtld_db - don't move, don't delete
 */
#define	LML_FLG_NOAUDIT		0x00000004	/* symbol auditing disabled */
#define	LML_FLG_PLTREL		0x00000008	/* deferred plt relocation */
						/* 	initialization */
						/*	(ld.so.1 only) */
#define	LML_FLG_HOLDLOCK	0x00000010	/* hold the rtld mutex lock */
#define	LML_FLG_ENVIRON		0x00000020	/* environ var initialized */
#define	LML_FLG_INTRPOSE	0x00000040	/* interposing objs on list */
#define	LML_FLG_LOCAUDIT	0x00000080	/* local auditors exists for */
						/*	this link-map list */
#define	LML_FLG_LOADAVAIL	0x00000100	/* load anything available */
#define	LML_FLG_IGNRELERR	0x00000200	/* ignore relocation errors - */
						/*	internal for crle(1) */
#define	LML_FLG_DBNOTIF		0x00000400	/* binding activity going on */
#define	LML_FLG_STARTREL	0x00000800	/* relocation started */
#define	LML_FLG_ATEXIT		0x00001000	/* atexit processing */
#define	LML_FLG_OBJADDED	0x00002000	/* object(s) added */
#define	LML_FLG_OBJDELETED	0x00004000	/* object(s) deleted */
#define	LML_FLG_OBJREEVAL	0x00008000	/* existing object(s) needs */
						/*	tsort reevaluation */
#define	LML_FLG_NOPENDGLBLAZY	0x00010000	/* no pending, global, lazy */
						/*	dependencies remain */
#define	LML_FLG_INTRPOSETSORT	0x00020000	/* interpose tsorting done */
#define	LML_FLG_AUDITNOTIFY	0x00040000	/* audit consistent required */
#define	LML_FLG_GROUPSEXIST	0x00080000	/* local groups exist */

#define	LML_FLG_TRC_LDDSTUB	0x00100000	/* identify lddstub */
#define	LML_FLG_TRC_ENABLE	0x00200000	/* tracing enabled (ldd) */
#define	LML_FLG_TRC_WARN	0x00400000	/* print warnings for undefs */
#define	LML_FLG_TRC_VERBOSE	0x00800000	/* verbose (versioning) trace */
#define	LML_FLG_TRC_SEARCH	0x01000000	/* trace search paths */
#define	LML_FLG_TRC_UNREF	0x02000000	/* trace unreferenced */
						/*	dependencies */
#define	LML_FLG_TRC_UNUSED	0x04000000	/* trace unused dependencies */
#define	LML_FLG_TRC_INIT	0x08000000	/* print .init order */
#define	LML_FLG_TRC_NOUNRESWEAK	0x10000000	/* unresolved weak references */
						/*	are not allowed */
#define	LML_FLG_TRC_NOPAREXT	0x20000000	/* unresolved PARENT/EXTERN */
						/*	references are not */
						/*	allowed */
#define	LML_MSK_TRC		0xfff00000	/* tracing mask */

/*
 * Possible Link_map transferable flags (Lm_list.lm_tflags), i.e., link-map
 * list flags that can be propagated to any new link-map list created.
 */
#define	LML_TFLG_NOLAZYLD	0x00000001	/* lazy loading disabled */
#define	LML_TFLG_NODIRECT	0x00000002	/* direct bindings disabled */

#define	LML_TFLG_LOADFLTR	0x00000008	/* trigger filtee loading */

#define	LML_TFLG_AUD_PREINIT	0x00100000	/* preinit (audit) exists */
#define	LML_TFLG_AUD_OBJSEARCH	0x00200000	/* objsearch (audit) exists */
#define	LML_TFLG_AUD_OBJOPEN	0x00400000	/* objopen (audit) exists */
#define	LML_TFLG_AUD_OBJFILTER	0x00800000	/* objfilter (audit) exists */
#define	LML_TFLG_AUD_OBJCLOSE	0x01000000	/* objclose (audit) exists */
#define	LML_TFLG_AUD_SYMBIND	0x02000000	/* symbind (audit) exists */
#define	LML_TFLG_AUD_PLTENTER	0x04000000	/* pltenter (audit) exists */
#define	LML_TFLG_AUD_PLTEXIT	0x08000000	/* pltexit (audit) exists */
#define	LML_TFLG_AUD_ACTIVITY	0x10000000	/* activity (audit) exists */

/*
 * NOTE: Audit flags have duplicated FLAGS1() values.  If more audit flags are
 * added, update the FLAGS1() reservation FL1_AUD_RS_STR to FL1_AUD_RS_END
 * defined later.
 */
#define	LML_TFLG_AUD_MASK	0xfff00000	/* audit interfaces mask */

/*
 * Define a Group Handle.
 *
 * The capability of ld.so.1 to associate a group of objects, look for symbols
 * within that group, ensure that groups are isolated from one another (with
 * regard to relocations), and to unload a group, centers around a handle.  This
 * data structure is tracked from the link-map HANDLE(), and is the structure
 * returned from dlopen(), and similar object loading capabilities such as
 * filter/filtee processing.
 *
 * A handle keeps track of all the dependencies of the associated object.
 * These dependencies may be added as objects are lazily loaded.  The core
 * dependencies on the handle are the ldd(1) list of the associated object.
 * The object assigned the handle, and the parent (or caller) who requested the
 * handle are also maintained as dependencies on the handle.
 *
 * Presently, an object may have two handles, one requested with RTLD_FIRST
 * and one without.
 *
 * A handle may be referenced by any number of parents (callers).  A reference
 * count tracks the number.  A dlclose() operation drops the reference count,
 * and when the count is zero, the handle is used to determine the family of
 * objects to unload.  As bindings may occur to objects on the handle from
 * other handles, it may not be possible to remove a complete family of
 * objects or that handle itself.  Handles in this state are moved to an orphan
 * list.  A handle on the orphan list is taken off the orphan list if the
 * associated object is reopened.  Otherwise, the handle remains on the orphan
 * list for the duration of the process.  The orphan list is inspected any time
 * objects are unloaded, to determine if the orphaned objects can also be
 * unloaded.
 *
 * Objects can be dlopened using RTLD_NOW.  This attribute requires that all
 * relocations of the object, and its dependencies are processed immediately,
 * before return to the caller.  Typically, an object is loaded without
 * RTLD_NOW, and procedure linkage relocations are satisfied when their
 * associated function is first called.  If an object is already loaded, and an
 * RTLD_NOW request is made, then the object, and its dependencies, most undergo
 * additional relocation processing.   This promotion from lazy binding to
 * immediate binding is carried out using handles, as the handle defines the
 * dependencies that must be processed.  A temporary handle is created for this
 * purpose, and is discarded immediately after the promotion operation has been
 * completed.
 */
typedef struct {
	Alist		*gh_depends;	/* handle dependency list */
	Rt_map		*gh_ownlmp;	/* handle owners link-map */
	Lm_list		*gh_ownlml;	/* handle owners link-map list */
	uint_t		gh_refcnt;	/* handle reference count */
	uint_t		gh_flags;	/* handle flags (GPH_ values) */
} Grp_hdl;

#define	GPH_ZERO	0x0001		/* special handle for dlopen(0) */
#define	GPH_LDSO	0x0002		/* special handle for ld.so.1 */
#define	GPH_FIRST	0x0004		/* dlsym() can only use originating */
					/*	dependency */
#define	GPH_FILTEE	0x0008		/* handle used to specify a filtee */
#define	GPH_INITIAL	0x0010		/* handle is initialized */
#define	GPH_NOPENDLAZY	0x0020		/* no pending lazy dependencies */
					/*	remain for this handle */

/*
 * Define a Group Descriptor.
 *
 * Each dependency associated with a group handle is maintained by a group
 * descriptor.  The descriptor defines the associated dependency together with
 * flags that indicate how the dependency can be used.
 */
typedef struct {
	Rt_map *	gd_depend;	/* dependency */
	uint_t		gd_flags;	/* dependency flags (GPD_ values) */
} Grp_desc;

#define	GPD_DLSYM	0x0001		/* dependency available to dlsym() */
#define	GPD_RELOC	0x0002		/* dependency available to satisfy */
					/*	relocation binding */
#define	GPD_ADDEPS	0x0004		/* dependencies of this dependency */
					/*	should be added to handle */
#define	GPD_PARENT	0x0008		/* dependency is a parent */
#define	GPD_FILTER	0x0010		/* dependency is our filter */
#define	GPD_PROMOTE	0x0020		/* dependency is our RTLD_NOW */
					/*	promoter */
#define	GPD_REMOVE	0x1000		/* descriptor is a candidate for */
					/*	removal from the group */

/*
 * Define threading structures.  For compatibility with libthread (T1_VERSION 1
 * and TI_VERSION 2) our locking structure is sufficient to hold a mutex or a
 * readers/writers lock.
 */
typedef struct {
	union {
		mutex_t		l_mutex;
		rwlock_t	l_rwlock;
	} u;
} Rt_lock;

typedef	cond_t	Rt_cond;

/*
 * Define a dynamic section information descriptor.  This parallels the entries
 * in the .dynamic section and holds auxiliary information to implement lazy
 * loading and filtee processing.
 */
typedef struct {
	uint_t	di_flags;
	void	*di_info;
} Dyninfo;

#define	FLG_DI_STDFLTR	0x00001		/* .dynamic entry for DT_FILTER */
#define	FLG_DI_AUXFLTR	0x00002		/* .dynamic entry for DT_AUXILIARY */
#define	FLG_DI_SYMFLTR	0x00004		/* .dynamic entry for DT_SYMFILTER */
					/*	and DT_SYMAUXILIARY */
#define	MSK_DI_FILTER	0x0000f		/* mask for all filter possibilities */

#define	FLG_DI_POSFLAG1	0x00010		/* .dynamic entry for DT_POSFLAG_1 */
#define	FLG_DI_NEEDED	0x00020		/* .dynamic entry for DT_NEEDED */
#define	FLG_DI_LAZY	0x00100		/* lazy needed entry - preceded by */
					/*    DF_P1_LAZYLOAD (DT_POSFLAG_1) */
#define	FLG_DI_GROUP	0x00200		/* group needed entry - preceded by */
					/*    DF_P1_GROUPPERM (DT_POSFLAG_1) */

#define	FLG_DI_LDD_DONE	0x01000		/* entry has been processed (ldd) */
#define	FLG_DI_LAZYFAIL	0x02000		/* the lazy loading of this entry */
					/*    failed */
/*
 * Data Structure to track AVL tree for pathnames of objects
 * loaded into memory
 */
typedef struct {
	const char	*fpn_name;	/* object name */
	Rt_map		*fpn_lmp;	/* object link-map */
	avl_node_t	fpn_avl;	/* avl book-keeping (see SGSOFFSETOF) */
	uint_t		fpn_hash;	/* object name hash value */
} FullpathNode;

/*
 * Define a mapping structure, which is maintained to describe each mapping
 * of an object, ie. the text segment, data segment, bss segment, etc.
 */
typedef struct {
	caddr_t		m_vaddr;	/* mapping address */
	size_t		m_fsize;	/* backing file size */
	size_t		m_msize;	/* mapping size */
	int		m_perm;		/* mapping permissions */
} Mmap;

/*
 * A given link-map can hold either a supplier or receiver copy
 * relocation list, but not both. This union is used to overlap
 * the space used for the two lists.
 */
typedef union {
	Alist	*rtc_r;		/* receiver list (Rel_copy) */
	APlist	*rtc_s;		/* supplier list (Rt_map *) */
} Rt_map_copy;


/*
 * Link-map definition.
 */
struct rt_map {
	/*
	 * BEGIN: Exposed to rtld_db - don't move, don't delete
	 */
	Link_map	rt_public;	/* public data */
	char		*rt_pathname;	/* full pathname of loaded object */
	ulong_t		rt_padstart;	/* start of image (including padding) */
	ulong_t		rt_padimlen;	/* size of image (including padding */
	ulong_t		rt_msize;	/* total memory mapped */
	uint_t		rt_flags;	/* state flags, see FLG below */
	uint_t		rt_flags1;	/* state flags1, see FL1 below */
	ulong_t		rt_tlsmodid;	/* TLS module id */
	/*
	 * END: Exposed to rtld_db - don't move, don't delete
	 */
	APlist		*rt_alias;	/* list of linked file names */
	APlist		*rt_fpnode;	/* list of FullpathNode AVL nodes */
	void		(*rt_init)();	/* address of _init */
	void		(*rt_fini)();	/* address of _fini */
	char		*rt_runpath;	/* LD_RUN_PATH and its equivalent */
	Pnode		*rt_runlist;	/*	Pnode structures */
	APlist		*rt_depends;	/* list of dependencies */
	APlist		*rt_callers;	/* list of callers */
	APlist		*rt_handles;	/* dlopen handles */
	APlist		*rt_groups;	/* groups we're a member of */
	ulong_t		rt_etext;	/* etext address */
	struct fct	*rt_fct;	/* file class table for this object */
	Sym		*(*rt_symintp)(); /* link map symbol interpreter */
	void		*rt_priv;	/* private data, object type specific */
	Lm_list		*rt_list;	/* link map list we belong to */
	uint_t		rt_objfltrndx;	/* object filtees .dynamic index */
	uint_t		rt_symsfltrcnt;	/* number of standard symbol filtees */
	uint_t		rt_symafltrcnt;	/* number of auxiliary symbol filtees */
	int		rt_mode;	/* usage mode, see RTLD mode flags */
	int		rt_sortval;	/* temporary buffer to traverse graph */
	uint_t		rt_cycgroup;	/* cyclic group */
	dev_t		rt_stdev;	/* device id and inode number for .so */
	ino_t		rt_stino;	/*	multiple inclusion checks */
	char		*rt_origname;	/* original pathname of loaded object */
	size_t		rt_dirsz;	/*	and its size */
	Rt_map_copy	rt_copy;	/* list of copy relocations */
	Audit_desc	*rt_auditors;	/* audit descriptor array */
	Audit_info	*rt_audinfo;	/* audit information descriptor */
	Syminfo		*rt_syminfo;	/* elf .syminfo section - here */
					/*	because it is checked in */
					/*	common code */
	Addr		*rt_initarray;	/* .initarray table */
	Addr		*rt_finiarray;	/* .finiarray table */
	Addr		*rt_preinitarray; /* .preinitarray table */
	Mmap		*rt_mmaps;	/* array of mapping information */
	uint_t		rt_mmapcnt;	/*	and associated number */
	uint_t		rt_initarraysz;	/* size of .initarray table */
	uint_t		rt_finiarraysz;	/* size of .finiarray table */
	uint_t		rt_preinitarraysz; /* size of .preinitarray table */
	Dyninfo		*rt_dyninfo;	/* .dynamic information descriptors */
	uint_t		rt_dyninfocnt;	/* count of dyninfo entries */
	uint_t		rt_relacount;	/* no. of RELATIVE relocations */
	uint_t		rt_idx;		/* hold index within linkmap list */
	uint_t		rt_lazy;	/* lazy dependencies pending */
	Rt_cond		*rt_condvar;	/*	variables */
	Xword		rt_hwcap;	/* hardware capabilities */
	Xword		rt_sfcap;	/* software capabilities */
	thread_t	rt_threadid;	/* thread init/fini synchronization */
	uint_t		rt_cntl;	/* link-map control list we belong to */
	uint_t		rt_flags2;	/* state flags2, see FL2 below */
};


#ifdef _SYSCALL32
/*
 * Structure to allow 64-bit rtld_db to read 32-bit processes out of procfs.
 */
typedef union {
	uint32_t	rtc_r;
	uint32_t	rtc_s;
} Rt_map_copy32;

typedef struct rt_map32 {
	/*
	 * BEGIN: Exposed to rtld_db - don't move, don't delete
	 */
	Link_map32	rt_public;
	uint32_t	rt_pathname;
	uint32_t	rt_padstart;
	uint32_t	rt_padimlen;
	uint32_t	rt_msize;
	uint32_t	rt_flags;
	uint32_t	rt_flags1;
	uint32_t	rt_tlsmodid;
	/*
	 * END: Exposed to rtld_db - don't move, don't delete
	 */
	uint32_t	rt_alias;
	uint32_t	rt_fpnode;
	uint32_t 	rt_init;
	uint32_t	rt_fini;
	uint32_t	rt_runpath;
	uint32_t	rt_runlist;
	uint32_t	rt_depends;
	uint32_t	rt_callers;
	uint32_t	rt_handles;
	uint32_t	rt_groups;
	uint32_t	rt_etext;
	uint32_t	rt_fct;
	uint32_t	rt_symintp;
	uint32_t	rt_priv;
	uint32_t 	rt_list;
	uint32_t 	rt_objfltrndx;
	uint32_t 	rt_symsfltrcnt;
	uint32_t 	rt_symafltrcnt;
	int32_t		rt_mode;
	int32_t		rt_sortval;
	uint32_t	rt_cycgroup;
	uint32_t	rt_stdev;
	uint32_t	rt_stino;
	uint32_t	rt_origname;
	uint32_t	rt_dirsz;
	Rt_map_copy32	rt_copy;
	uint32_t 	rt_auditors;
	uint32_t 	rt_audinfo;
	uint32_t	rt_syminfo;
	uint32_t	rt_initarray;
	uint32_t	rt_finiarray;
	uint32_t	rt_preinitarray;
	uint32_t	rt_mmaps;
	uint32_t	rt_mmapcnt;
	uint32_t	rt_initarraysz;
	uint32_t	rt_finiarraysz;
	uint32_t	rt_preinitarraysz;
	uint32_t 	rt_dyninfo;
	uint32_t 	rt_dyninfocnt;
	uint32_t	rt_relacount;
	uint32_t	rt_idx;
	uint32_t	rt_lazy;
	uint32_t	rt_condvar;
	uint32_t	rt_hwcap;
	uint32_t	rt_sfcap;
	uint32_t	rt_threadid;
	uint32_t	rt_cntl;
	uint32_t	rt_flags2;
} Rt_map32;

#endif	/* _SYSCALL32 */

/*
 * Link map state flags.
 */
/*
 * BEGIN: Exposed to rtld_db - don't move, don't delete
 */
#define	FLG_RT_ISMAIN	0x00000001	/* object represents main executable */
#define	FLG_RT_IMGALLOC	0x00000002	/* image is allocated (not mmap'ed) */
	/*
	 * Available for r_debug version >= RTLD_DB_VERSION5
	 */
#define	FLG_RT_RELOCED	0x00000004	/* object has been relocated */
/*
 * END: Exposed to rtld_db - don't move, don't delete
 */
#define	FLG_RT_SETGROUP	0x00000008	/* group establishment required */
#define	FLG_RT_HWCAP	0x00000010	/* process $HWCAP expansion */
#define	FLG_RT_OBJECT	0x00000020	/* object processing (ie. .o's) */
#define	FLG_RT_NEWLOAD	0x00000040	/* object is newly loaded */
#define	FLG_RT_NODUMP	0x00000080	/* object can't be dldump(3x)'ed */
#define	FLG_RT_DELETE	0x00000100	/* object can be deleted */
#define	FLG_RT_ANALYZED	0x00000200	/* object has been analyzed */
#define	FLG_RT_INITDONE	0x00000400	/* objects .init has been completed */
#define	FLG_RT_TRANS	0x00000800	/* object is acting as a translator */
#define	FLG_RT_FIXED	0x00001000	/* image location is fixed */
#define	FLG_RT_PRELOAD	0x00002000	/* object was preloaded */
#define	FLG_RT_ALTER	0x00004000	/* alternative object used */
#define	FLG_RT_LOADFLTR	0x00008000	/* trigger filtee loading */
#define	FLG_RT_AUDIT	0x00010000	/* object is an auditor */
#define	FLG_RT_MODESET	0x00020000	/* MODE() has been initialized */
#define	FLG_RT_ANALZING	0x00040000	/* object is being analyzed */
#define	FLG_RT_INITFRST 0x00080000	/* execute .init first */
#define	FLG_RT_NOOPEN	0x00100000	/* dlopen() not allowed */
#define	FLG_RT_FINICLCT	0x00200000	/* fini has been collected (tsort) */
#define	FLG_RT_INITCALL	0x00400000	/* objects .init has been called */
#define	FLG_RT_SUNWBSS	0x00800000	/* object with PT_SUNWBSS, not mapped */
#define	FLG_RT_OBJINTPO	0x01000000	/* object is a global interposer */
#define	FLG_RT_SYMINTPO	0x02000000	/* object contains symbol interposer */
#define	MSK_RT_INTPOSE	0x03000000	/* mask for all interposer */
					/*	possibilities */
#define	FLG_RT_MOVE	0x04000000	/* object needs move operation */
#define	FLG_RT_TMPLIST	0x08000000	/* object is part of a temporary list */
#define	FLG_RT_REGSYMS	0x10000000	/* object has DT_REGISTER entries */
#define	FLG_RT_INITCLCT	0x20000000	/* init has been collected (tsort) */
#define	FLG_RT_HANDLE	0x40000000	/* generate a handle for this object */
#define	FLG_RT_RELOCING	0x80000000	/* object is being relocated */

#define	FL1_RT_COPYTOOK	0x00000001	/* copy relocation taken */
#define	FL1_RT_RELATIVE	0x00000002	/* relative path expansion required */
#define	FL1_RT_CONFSET	0x00000004	/* object was loaded by crle(1) */
#define	FL1_RT_NODEFLIB	0x00000008	/* ignore default library search */
#define	FL1_RT_ENDFILTE	0x00000010	/* filtee terminates filters search */
#define	FL1_RT_DISPREL	0x00000020	/* object has *disp* relocation */
#define	FL1_RT_TEXTREL	0x00000040	/* DT_TEXTREL set in object */
#define	FL1_RT_INITWAIT	0x00000080	/* threads are waiting on .init */
#define	FL1_RT_LDDSTUB	0x00000100	/* identify lddstub */
#define	FL1_RT_NOINIFIN	0x00000200	/* no .init or .fini exists */
#define	FL1_RT_USED	0x00000400	/* symbol referenced from this object */
#define	FL1_RT_SYMBOLIC	0x00000800	/* DF_SYMBOLIC was set - use */
					/*	symbolic sym resolution */
#define	FL1_RT_OBJSFLTR	0x00001000	/* object is acting as a standard */
#define	FL1_RT_OBJAFLTR	0x00002000	/*	or auxiliary filter */
#define	FL1_RT_SYMSFLTR	0x00004000	/* symbol is acting as a standard */
#define	FL1_RT_SYMAFLTR	0x00008000	/*	or auxiliary filter */
#define	MSK_RT_FILTER	0x0000f000	/* mask for all filter possibilities */

#define	FL1_RT_TLSADD	0x00010000	/* objects TLS has been registered */
#define	FL1_RT_TLSSTAT	0x00020000	/* object requires static TLS */
#define	FL1_RT_DIRECT	0x00040000	/* object has DIRECT bindings enabled */
#define	FL1_RT_GLOBAUD	0x00080000	/* establish global auditing */

/*
 * The following range of bits are reserved to hold LML_TFLG_AUD_ values
 * (although the definitions themselves aren't used anywhere).
 */
#define	FL1_AUD_RS_STR	0x00100000	/* RESERVATION start for AU flags */
#define	FL1_AUD_RS_END	0x80000000	/* RESERVATION end for AU flags */

#define	FL2_RT_DTFLAGS	0x00000001	/* DT_FLAGS element exists */

/*
 * Flags for the tls_modactivity() routine
 */
#define	TM_FLG_MODADD	0x01		/* call tls_modadd() interface */
#define	TM_FLG_MODREM	0x02		/* call tls_modrem() interface */

/*
 * Macros for getting to link_map data.
 */
#define	ADDR(X)		((X)->rt_public.l_addr)
#define	NAME(X)		((X)->rt_public.l_name)
#define	DYN(X)		((X)->rt_public.l_ld)
#define	NEXT(X)		((X)->rt_public.l_next)
#define	PREV(X)		((X)->rt_public.l_prev)
#define	REFNAME(X)	((X)->rt_public.l_refname)

/*
 * Macros for getting to linker private data.
 */
#define	PATHNAME(X)	((X)->rt_pathname)
#define	PADSTART(X)	((X)->rt_padstart)
#define	PADIMLEN(X)	((X)->rt_padimlen)
#define	MSIZE(X)	((X)->rt_msize)
#define	FLAGS(X)	((X)->rt_flags)
#define	FLAGS1(X)	((X)->rt_flags1)
#define	FLAGS2(X)	((X)->rt_flags2)
#define	TLSMODID(X)	((X)->rt_tlsmodid)

#define	ALIAS(X)	((X)->rt_alias)
#define	FPNODE(X)	((X)->rt_fpnode)
#define	INIT(X)		((X)->rt_init)
#define	FINI(X)		((X)->rt_fini)
#define	RPATH(X)	((X)->rt_runpath)
#define	RLIST(X)	((X)->rt_runlist)
#define	DEPENDS(X)	((X)->rt_depends)
#define	CALLERS(X)	((X)->rt_callers)
#define	HANDLES(X)	((X)->rt_handles)
#define	GROUPS(X)	((X)->rt_groups)
#define	ETEXT(X)	((X)->rt_etext)
#define	FCT(X)		((X)->rt_fct)
#define	SYMINTP(X)	((X)->rt_symintp)
#define	LIST(X)		((X)->rt_list)
#define	OBJFLTRNDX(X)	((X)->rt_objfltrndx)
#define	SYMSFLTRCNT(X)	((X)->rt_symsfltrcnt)
#define	SYMAFLTRCNT(X)	((X)->rt_symafltrcnt)
#define	MODE(X)		((X)->rt_mode)
#define	SORTVAL(X)	((X)->rt_sortval)
#define	CYCGROUP(X)	((X)->rt_cycgroup)
#define	STDEV(X)	((X)->rt_stdev)
#define	STINO(X)	((X)->rt_stino)
#define	ORIGNAME(X)	((X)->rt_origname)
#define	DIRSZ(X)	((X)->rt_dirsz)
#define	COPY_R(X)	((X)->rt_copy.rtc_r)
#define	COPY_S(X)	((X)->rt_copy.rtc_s)
#define	AUDITORS(X)	((X)->rt_auditors)
#define	AUDINFO(X)	((X)->rt_audinfo)
#define	SYMINFO(X)	((X)->rt_syminfo)
#define	INITARRAY(X)	((X)->rt_initarray)
#define	FINIARRAY(X)	((X)->rt_finiarray)
#define	PREINITARRAY(X)	((X)->rt_preinitarray)
#define	MMAPS(X)	((X)->rt_mmaps)
#define	MMAPCNT(X)	((X)->rt_mmapcnt)
#define	INITARRAYSZ(X)	((X)->rt_initarraysz)
#define	FINIARRAYSZ(X)	((X)->rt_finiarraysz)
#define	PREINITARRAYSZ(X) ((X)->rt_preinitarraysz)
#define	DYNINFO(X)	((X)->rt_dyninfo)
#define	DYNINFOCNT(X)	((X)->rt_dyninfocnt)
#define	RELACOUNT(X)	((X)->rt_relacount)
#define	IDX(X)		((X)->rt_idx)
#define	LAZY(X)		((X)->rt_lazy)
#define	CONDVAR(X)	((X)->rt_condvar)
#define	CNTL(X)		((X)->rt_cntl)
#define	HWCAP(X)	((X)->rt_hwcap)
#define	SFCAP(X)	((X)->rt_sfcap)
#define	THREADID(X)	((X)->rt_threadid)

/*
 * Flags for tsorting.
 */
#define	RT_SORT_FWD	0x01		/* topological sort (.fini) */
#define	RT_SORT_REV	0x02		/* reverse topological sort (.init) */
#define	RT_SORT_DELETE	0x10		/* process FLG_RT_DELNEED objects */
					/*	only (called via dlclose()) */
#define	RT_SORT_INTPOSE	0x20		/* process interposer objects */

/*
 * Flags for lookup_sym (and hence find_sym) routines.
 */
#define	LKUP_DEFT	0x0000		/* simple lookup request */
#define	LKUP_SPEC	0x0001		/* special ELF lookup (allows address */
					/*	resolutions to plt[] entries) */
#define	LKUP_LDOT	0x0002		/* indicates the original A_OUT */
					/*	symbol had a leading `.' */
#define	LKUP_FIRST	0x0004		/* lookup symbol in first link map */
					/*	only */
#define	LKUP_COPY	0x0008		/* lookup symbol for a COPY reloc, do */
					/*	not bind to symbol at head */
#define	LKUP_STDRELOC	0x0010		/* lookup originates from a standard */
					/*	relocation (elf_reloc()) */
#define	LKUP_SELF	0x0020		/* lookup symbol in ourself - undef */
					/*	is valid */
#define	LKUP_WEAK	0x0040		/* relocation reference is weak */
#define	LKUP_NEXT	0x0080		/* request originates from RTLD_NEXT */
#define	LKUP_NODESCENT	0x0100		/* don't descend through dependencies */
#define	LKUP_NOFALLBACK	0x0200		/* don't fall back to loading */
					/*	pending lazy dependencies */
#define	LKUP_DIRECT	0x0400		/* direct binding request */
#define	LKUP_SYMNDX	0x0800		/* establish symbol index */
#define	LKUP_SINGLETON	0x1000		/* search for a singleton symbol */
#define	LKUP_STANDARD	0x2000		/* standard lookup - originated from */
					/* 	head link-map element */

/*
 * For the runtime linker to perform a symbol search, a number of data items
 * related to the search are required.  An Slookup data structure is used to
 * convey this data to lookup_sym(), and in special cases, to other core
 * routines that provide the implementation details for lookup_sym()
 *
 * The symbol name (sl_name), the caller (sl_cmap), and the link-map from which
 * to start the search (sl_imap) are fundamental to the symbol search.  The
 * initial search link-map might get modified by the core routines that provide
 * the implementation details for lookup_sym().  This modification accommodates
 * requirements such as processing a handle, direct binding and interposition.
 * The association between the caller and the potential destination also
 * determines whether the destination is a candidate to search.
 *
 * The lookup identifier (sl_id) is used to identify a runtime linker operation.
 * Within this operation, any lazy loads that fail are not re-examined.  This
 * technique keeps the overhead of processing a failed lazy load to a minimum.
 *
 * Symbol searches that originate from a relocation record are accompanied by
 * the relocation index (sl_rsymndx), the symbol reference (sl_rsym) and
 * possibly the relocation type (sl_rtype).  This data provides for determining
 * lazy loading, direct binding, and special symbol processing requirements
 * such as copy relocations and singleton lookup.
 *
 * The symbols hash value is computed by lookup_sym, and propagated throughout
 * the search engine.  Note, occasionally the Slookup data is passed to a core
 * routine that provides the implementation details for lookup_sym(), ie.
 * elf_find_sym(), in which case the caller must initialize the hash value.
 *
 * The symbols binding information is established by lookup_sym() when the
 * symbols relocation type is supplied.  Weak bindings allow relocations to
 * be set to zero should a symbol lookup fail.
 *
 * The flags allow the caller to control aspects of the search, including the
 * interpretation of copy relocations, etc.  Note, a number of flag settings
 * are established in lookup_sym() from attributes of the symbol reference.
 */
typedef struct {
	const char	*sl_name;	/* symbol name */
	Rt_map		*sl_cmap;	/* callers link-map */
	Rt_map		*sl_imap;	/* initial link-map to search */
	ulong_t		sl_id;		/* identifier for this lookup */
	ulong_t		sl_hash;	/* symbol hash value */
	ulong_t		sl_rsymndx;	/* referencing reloc symndx */
	Sym		*sl_rsym;	/* referencing symbol */
	uchar_t		sl_rtype;	/* relocation type associate with */
					/*    symbol */
	uchar_t		sl_bind;	/* symbols binding (returned) */
	uint_t		sl_flags;	/* lookup flags */
} Slookup;

#define	SLOOKUP_INIT(sl, name, cmap, imap, id, hash, rsymndx, rsym, rtype, \
    flags) \
	(void) (sl.sl_name = (name), sl.sl_cmap = (cmap), sl.sl_imap = (imap), \
	    sl.sl_id = (id), sl.sl_hash = (hash), sl.sl_rsymndx = (rsymndx), \
	    sl.sl_rsym = (rsym), sl.sl_rtype = (rtype), sl.sl_bind = 0, \
	    sl.sl_flags = (flags))

/*
 * Define a number of .plt lookup outcomes, for use in binding diagnostics.
 */
typedef	enum {
	PLT_T_NONE = 0,
	PLT_T_21D,
	PLT_T_24D,
	PLT_T_U32,
	PLT_T_U44,
	PLT_T_FULL,
	PLT_T_FAR,
	PLT_T_NUM			/* Must be last */
} Pltbindtype;

/*
 * Prototypes.
 */
extern ulong_t		ld_entry_cnt;	/* counter bumped on each entry to */
					/*    ld.so.1. */
extern Lm_list		lml_main;	/* main's link map list */
extern Lm_list		lml_rtld;	/* rtld's link map list */
extern Lm_list		*lml_list[];

extern Pltbindtype	elf_plt_write(uintptr_t, uintptr_t, void *, uintptr_t,
			    Xword);
extern Rt_map		*is_so_loaded(Lm_list *, const char *);
extern Sym		*lookup_sym(Slookup *, Rt_map **, uint_t *);
extern int		rt_dldump(Rt_map *, const char *, int, Addr);

#ifdef	__cplusplus
}
#endif

#endif /* _RTLD_H */
