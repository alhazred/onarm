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
 * Copyright (c) 2006-2007 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_MODSTATIC_H
#define	_SYS_MODSTATIC_H

#ident	"@(#)common/sys/modstatic.h"

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */

/*
 * Definitions for static-linked kernel.
 * This is build environment private header.
 */

#ifdef	STATIC_UNIX

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/kobj.h>
#include <sys/hwconf.h>

typedef void	(*hwc_addspec_t)(struct hwc_spec *, struct par_list **);

#define	STATIC_UNIX_FILENAME	"vmunix"
#define	STATIC_UNIX_MODNAME	"unix"

#define	LDI_ANON_MODNAME	STATIC_UNIX_MODNAME

#define	MOD_STATIC_UNIX()	(1)

#define	BUILD_MAJOR_BINDING(bindfile, hashtab, line_parser) \
		make_major_binding((hashtab), (line_parser))
#define	BUILD_SYSNUM_BINDING(bindfile, hashtab, line_parser) \
		make_sysnum_binding((hashtab), (line_parser))
#define	BUILD_DACF_BINDING(bindfile)	make_dacf_binding()

/* for e_ddi_instance_init() */
#define	IN_GET_INFILE(f)	(PTI_REBUILD)

/* Prototypes */
extern void		mod_static_init(void);
extern void		mod_static_ctf_init(void *addr, size_t size);
extern boolean_t	mod_static_ctf_update(struct module *mp, void *newaddr,
					      size_t newsize);
extern boolean_t	mod_is_static_linked(char *filename);
extern boolean_t	mod_is_static_module(struct module *mp);
extern int		mod_static_load(modctl_t *modp);
extern int		mod_static_install(modctl_t *modp);
extern int		mod_static_getinfo(modctl_t *modp,
					   struct modinfo *modinfop,
					   int *rvalp);
extern int		mod_static_unload(modctl_t *modp);
extern void		mod_static_load_all(boolean_t stub);
extern int		mod_static_load_drvconf(char *fname,
						struct par_list **pl,
						ddi_prop_t **props,
						hwc_addspec_t addspec);
extern void		mod_static_rescan(void);
extern void		mod_static_forceload_setup(void);
extern void		mod_static_forceload(void);

extern void		kobj_static_init(void);
extern void		kobj_static_printf_init(void);
extern void		kobj_static_notify(int type, struct modctl *modp);
extern void		kobj_static_add_dependent(struct module *mp,
						  struct module *dep);

extern int	make_major_binding(struct bind **,
		int (*line_parser)(char *, int, char *, struct bind **));
extern int	make_sysnum_binding(struct bind **,
		int (*line_parser)(char *, int, char *, struct bind **));
extern int	make_dacf_binding(void);

#else	/* !STATIC_UNIX */

#define	LDI_ANON_MODNAME	"genunix"

#define	MOD_STATIC_UNIX()	(0)

/* Stub macros to inactivate STATIC_UNIX features. */
#define	mod_static_init()
#define mod_static_ctf_init(addr, size)
#define	mod_static_ctf_update(mp, newaddr, newsize)	(B_FALSE)
#define	mod_is_static_linked(filename)			(B_FALSE)
#define	mod_is_static_module(filename)			(B_FALSE)
#define	mod_static_load(modp)				(-1)
#define	mod_static_install(modp)			(-1)
#define	mod_static_getinfo(modp, modinfop, rvalp)	(-1)
#define	mod_static_unload(modp)				(-1)
#define	mod_static_load_all(stub)
#define	mod_static_load_drvconf(fname, pl, props, addspec)	(-1)
#define	mod_static_rescan()
#define	mod_static_forceload_setup()
#define	mod_static_forceload()

#define	BUILD_MAJOR_BINDING(bindfile, hashtab, line_parser) \
		read_binding_file((bindfile), (hashtab), (line_parser))
#define	BUILD_SYSNUM_BINDING(bindfile, hashtab, line_parser) \
		read_binding_file((bindfile), (hashtab), (line_parser))
#define	BUILD_DACF_BINDING(bindfile)	read_dacf_binding_file(bindfile)

/* for e_ddi_instance_init() */
#define	IN_GET_INFILE(f)	in_get_infile(f)

#endif	/* STATIC_UNIX */

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* !_SYS_MODSTATIC_H */
