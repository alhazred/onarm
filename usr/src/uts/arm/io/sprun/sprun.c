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

/*                                                              */
/*
 * Copyright (c) 2007 NEC Corporation
 * All rights reserved.
 */
/*                                                              */

#include <sys/devops.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/open.h>
#include <sys/cred.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/cmn_err.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ksynch.h>

extern int strplumb(void);	/* io/strplumb.c */
extern int netboot;		/* os/space.c */
static kmutex_t sprun_lock;	/* sprun lock */

static int sprun_attach(dev_info_t *dip, ddi_attach_cmd_t cmd);
static int sprun_detach(dev_info_t *dip, ddi_detach_cmd_t cmd);
static int sprun_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **resultp);
static int sprun_prop_op(dev_t dev, dev_info_t *dip, ddi_prop_op_t prop_op, int flags, char *name, caddr_t valuep, int *lengthp);
static int sprun_open(dev_t *devp, int flag, int otyp, cred_t *cred);
static int sprun_close(dev_t dev, int flag, int otyp, cred_t *cred);
static int sprun_read(dev_t dev, struct uio *uiop, cred_t *cred);
static int sprun_write(dev_t dev, struct uio *uiop, cred_t *cred);

/* cb_ops structure */
static struct cb_ops sprun_cb_ops = {
	sprun_open,
	sprun_close,
	nodev,
	nodev,
	nodev,
	sprun_read,
	sprun_write,
	nodev,
	nodev,
	nodev,
	nodev,
	nochpoll,
	sprun_prop_op,
	NULL,
	D_NEW | D_MP,
	CB_REV,
	nodev,
	nodev
};

/* dev_ops structure */
static struct dev_ops sprun_dev_ops = {
	DEVO_REV,
	0,
	sprun_getinfo,
	nulldev,
	nulldev,
	sprun_attach,
	sprun_detach,
	nodev,
	&sprun_cb_ops,
	(struct bus_ops *)NULL,
	nodev
};

/* modldrv strucure */
static struct modldrv md = {
	&mod_driverops,
	"strplumb run driver",
	&sprun_dev_ops
};

/* modlinkage structure */
static struct modlinkage ml = {
	MODREV_1,
	&md,
	NULL
};

/* dev_info structure */
dev_info_t *sprun_dip;

/* Loadable module configuration entry points */
int
_init(void)
{
	return (mod_install(&ml));
}

int
_info(struct modinfo *modinfop)
{
	return (mod_info(&ml, modinfop));
}

int
_fini(void)
{
	return(mod_remove(&ml));
}

/* Device configuration entry points */
static int
sprun_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{

	switch(cmd) {
	case DDI_ATTACH:
		sprun_dip = dip;
		if (ddi_create_minor_node(dip, "0", S_IFCHR, ddi_get_instance(dip), DDI_PSEUDO, 0) != DDI_SUCCESS) {
			cmn_err(CE_NOTE, "%s%d: attach: could not add charater nocde.", "sprun", 0);
			return DDI_FAILURE;
		} else {
			mutex_init(&sprun_lock, NULL, MUTEX_DRIVER, NULL);
			return DDI_SUCCESS;
		}
	default:
		return DDI_FAILURE;
	}
}

static int
sprun_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{

	switch(cmd) {
	case DDI_DETACH:
		sprun_dip = 0;
		ddi_remove_minor_node(dip, NULL);
		mutex_destroy(&sprun_lock);
		return DDI_SUCCESS;
	default:
		return DDI_FAILURE;
	}
}
	
static int 
sprun_getinfo(dev_info_t *dip, ddi_info_cmd_t cmd, void *arg, void **resultp)
{
	switch(cmd) {
	case DDI_INFO_DEVT2DEVINFO:
		*resultp = sprun_dip;
		return DDI_SUCCESS;

	case DDI_INFO_DEVT2INSTANCE:
		*resultp = 0;
		return DDI_SUCCESS;
	default:
		return DDI_FAILURE;
	}
}

/* Main entry points */
static int
sprun_prop_op(dev_t dev, dev_info_t *dip, ddi_prop_op_t prop_op, int flags, char *name, caddr_t valuep, int *lengthp)
{
	return (ddi_prop_op(dev, dip, prop_op, flags, name, valuep, lengthp));
}

static int
sprun_open(dev_t *devp, int flag, int otyp, cred_t *cred)
{
	return DDI_SUCCESS;
}

static int 
sprun_close(dev_t dev, int flag, int otyp, cred_t *cred)
{
	return DDI_SUCCESS;
}

static int 
sprun_read(dev_t dev, struct uio *uiop, cred_t *cred)
{
	return DDI_SUCCESS;
}

static int
sprun_write(dev_t dev, struct uio *uiop, cred_t *cred)
{
	int ret = DDI_SUCCESS;

#ifdef BOOT_STRPLUMB_DISABLE

	mutex_enter(&sprun_lock);

	/* already strplumb() called */
	if (netboot != 0) {
		ret = EIO;
		goto done;
	}

	if (uiop->uio_resid > 0) {
		int value;
		value = uwritec(uiop);
		if (value == -1){
			ret = EFAULT;
			goto done;
		}
		if (value == '1')  {
			strplumb();
			netboot = 1;
		} else {
			ret = EINVAL;
			goto done;
		}
	} else {
		ret = EIO;
		goto done;
	}
done:
	mutex_exit(&sprun_lock);
#endif

	return ret;
}
