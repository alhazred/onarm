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
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

/*
 * Kernel "misc" module that contains kiconv and unicode functionalities.
 */

#include <sys/types.h>
#include <sys/modctl.h>
#include <sys/errno.h>
#include <sys/kiconv.h>

static struct modlmisc modlmisc = {
	&mod_miscops,
	"kiconv/unicode base module",
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modlmisc,
	NULL
};

extern int	mod_kiconv_install(int (*regfunc)(kiconv_module_info_t *),
				   int (*unregfunc)(kiconv_module_info_t *));
extern int	mod_kiconv_uninstall(void);

/*
 * int
 * MODDRV_ENTRY_INIT(void)
 *	_init(9E) entry for kiconv module.
 */
int
MODDRV_ENTRY_INIT(void)
{
#ifdef	KICONV_LOADABLE
	int	err;

	kiconv_init();

	/* Install kiconv register/unregister function. */
	err = mod_kiconv_install(kiconv_register_module,
				 kiconv_unregister_module);
	if (err != 0) {
		return err;
	}
#endif	/* KICONV_LOADABLE */

	return mod_install(&modlinkage);
}

#ifdef	KICONV_LOADABLE

extern boolean_t	kiconv_is_unloadable(void);

/*
 * int
 * MODDRV_ENTRY_FINI(void)
 *	_fini(9E) entry for uartdump module.
 */
int
MODDRV_ENTRY_FINI(void)
{
	int	err;

	if (!kiconv_is_unloadable()) {
		/* Someone uses kiconv module. */
		return EBUSY;
	}

	/* Uninstall kiconv register/unregister function. */
	if ((err = mod_kiconv_uninstall()) != 0) {
		return err;
	}

	return mod_remove(&modlinkage);
}
#endif	/* KICONV_LOADABLE */

/*
 * int
 * MODDRV_ENTRY_INFO(struct modinfo *modinfop)
 *	_info(9E) entry for uartdump module.
 */
int
MODDRV_ENTRY_INFO(struct modinfo *modinfop)
{
	return mod_info(&modlinkage, modinfop);
}
