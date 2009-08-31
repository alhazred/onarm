/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006 NEC Corporation
 */

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 * av1394 driver
 */

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/cred.h>
#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/stat.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#include <sys/1394/targets/av1394/av1394_impl.h>

/* DDI/DKI entry points */
static int	av1394_getinfo(dev_info_t *, ddi_info_cmd_t, void *, void **);
static int	av1394_attach(dev_info_t *, ddi_attach_cmd_t);
static int	av1394_detach(dev_info_t *, ddi_detach_cmd_t);
static int 	av1394_open(dev_t *, int, int, cred_t *);
static int	av1394_close(dev_t, int, int, cred_t *);
static int	av1394_read(dev_t, struct uio *, cred_t *);
static int	av1394_write(dev_t, struct uio *, cred_t *);
static int	av1394_ioctl(dev_t, int, intptr_t, int, cred_t *, int *);
static int	av1394_devmap(dev_t, devmap_cookie_t, offset_t, size_t,
		size_t *, uint_t);
static int	av1394_poll(dev_t, short, int, short *, struct pollhead **);

/* configuration routines */
static void	av1394_cleanup(av1394_inst_t *, int);
static int	av1394_t1394_attach(av1394_inst_t *, dev_info_t *);
static void	av1394_t1394_detach(av1394_inst_t *);
static int	av1394_add_events(av1394_inst_t *);
static void	av1394_remove_events(av1394_inst_t *);

/* CPR */
static int	av1394_cpr_suspend(av1394_inst_t *);
static int 	av1394_cpr_resume(av1394_inst_t *);

/* callbacks */
static void	av1394_bus_reset(dev_info_t *, ddi_eventcookie_t, void *,
		void *);
static void	av1394_disconnect(dev_info_t *, ddi_eventcookie_t, void *,
		void *);
static void	av1394_reconnect(dev_info_t *, ddi_eventcookie_t, void *,
		void *);

extern struct mod_ops mod_driverops;

struct cb_ops av1394_cb_ops = {
	av1394_open,		/* open  */
	av1394_close,		/* close */
	nulldev,		/* strategy */
	nulldev,		/* print */
	nulldev,		/* dump */
	av1394_read,		/* read */
	av1394_write,		/* write */
	av1394_ioctl,		/* ioctl */
	av1394_devmap,		/* devmap */
	nulldev,		/* mmap */
	nulldev,		/* segmap */
	av1394_poll,		/* poll */
	ddi_prop_op,		/* cb_prop_op */
	NULL,			/* streamtab  */
	D_MP | D_NEW | D_HOTPLUG | D_DEVMAP
};

static struct dev_ops av1394_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* refcnt  */
	av1394_getinfo,		/* getinfo */
	nulldev,		/* identify */
	nulldev,		/* probe */
	av1394_attach,		/* attach */
	av1394_detach,		/* detach */
	nodev,			/* reset */
	&av1394_cb_ops,		/* driver operations */
	NULL,			/* bus operations */
	NULL			/* power */
};

static struct modldrv av1394_modldrv =	{
	&mod_driverops,
	"IEEE 1394 AV driver %I%",
	&av1394_ops
};

static struct modlinkage av1394_modlinkage = {
	MODREV_1,
	&av1394_modldrv,
	NULL,
};

static void *av1394_statep;

#ifndef NPROBE
extern int tnf_mod_load(void);
extern int tnf_mod_unload(struct modlinkage *mlp);
#endif

#define	AV1394_INST2STATE(inst)	(ddi_get_soft_state(av1394_statep, inst))
#define	AV1394_DEV2STATE(dev)	\
		(ddi_get_soft_state(av1394_statep, AV1394_DEV2INST(dev)))

#define	AV1394_TNF_ENTER(func)	\
	TNF_PROBE_0_DEBUG(func##_enter, AV1394_TNF_INST_STACK, "");

#define	AV1394_TNF_EXIT(func)	\
	TNF_PROBE_0_DEBUG(func##_exit, AV1394_TNF_INST_STACK, "");

/*
 *
 * --- DDI/DKI entry points
 *
 */
int
MODDRV_ENTRY_INIT(void)
{
	int    error;

#ifndef NPROBE
	(void) tnf_mod_load();
#endif
	error = ddi_soft_state_init(&av1394_statep, sizeof (av1394_inst_t), 1);
	if (error != 0) {
#ifndef NPROBE
		(void) tnf_mod_unload(&av1394_modlinkage);
#endif
		return (error);
	}

	if ((error = mod_install(&av1394_modlinkage)) != 0) {
		ddi_soft_state_fini(&av1394_statep);
#ifndef NPROBE
		(void) tnf_mod_unload(&av1394_modlinkage);
#endif
	}

	return (error);
}

#ifndef	STATIC_DRIVER
int
MODDRV_ENTRY_FINI(void)
{
	int    error;

	if ((error = mod_remove(&av1394_modlinkage)) == 0) {
		ddi_soft_state_fini(&av1394_statep);
#ifndef NPROBE
		(void) tnf_mod_unload(&av1394_modlinkage);
#endif
	}

	return (error);
}
#endif	/* !STATIC_DRIVER */

int
MODDRV_ENTRY_INFO(struct modinfo *modinfop)
{
	return (mod_info(&av1394_modlinkage, modinfop));
}

/*
 * attach
 */
static int
av1394_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
	int		instance = ddi_get_instance(dip);
	av1394_inst_t	*avp;

	AV1394_TNF_ENTER(av1394_attach);

	switch (cmd) {
	case DDI_ATTACH:
		break;
	case DDI_RESUME:
		if ((avp = AV1394_INST2STATE(instance)) == NULL) {
			return (DDI_FAILURE);
		}
		return (av1394_cpr_resume(avp));
	default:
		AV1394_TNF_EXIT(av1394_attach);
		return (DDI_FAILURE);
	}

	if (ddi_soft_state_zalloc(av1394_statep, instance) != 0) {
		TNF_PROBE_0(av1394_attach_error_soft_state_zalloc,
		    AV1394_TNF_INST_ERROR, "");
		AV1394_TNF_EXIT(av1394_attach);
		return (DDI_FAILURE);
	}
	avp = AV1394_INST2STATE(instance);

	if (av1394_t1394_attach(avp, dip) != DDI_SUCCESS) {
		av1394_cleanup(avp, 1);
		AV1394_TNF_EXIT(av1394_attach);
		return (DDI_FAILURE);
	}

	mutex_init(&avp->av_mutex, NULL, MUTEX_DRIVER,
			avp->av_attachinfo.iblock_cookie);

#ifndef __lock_lint
	avp->av_dip = dip;
	avp->av_instance = instance;
#endif

	if (av1394_add_events(avp) != DDI_SUCCESS) {
		av1394_cleanup(avp, 2);
		AV1394_TNF_EXIT(av1394_attach);
		return (DDI_FAILURE);
	}

	if (av1394_isoch_attach(avp) != DDI_SUCCESS) {
		av1394_cleanup(avp, 3);
		AV1394_TNF_EXIT(av1394_attach);
		return (DDI_FAILURE);
	}

	if (av1394_async_attach(avp) != DDI_SUCCESS) {
		av1394_cleanup(avp, 4);
		AV1394_TNF_EXIT(av1394_attach);
		return (DDI_FAILURE);
	}

#ifndef __lock_lint
	avp->av_dev_state = AV1394_DEV_ONLINE;
#endif

	ddi_report_dev(dip);

	AV1394_TNF_EXIT(av1394_attach);
	return (DDI_SUCCESS);
}

static int
av1394_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
	int		instance = ddi_get_instance(dip);
	av1394_inst_t	*avp;

	AV1394_TNF_ENTER(av1394_detach);

	if ((avp = AV1394_INST2STATE(instance)) == NULL) {
		TNF_PROBE_0(av1394_detach_error_instance,
		    AV1394_TNF_INST_ERROR, "");
		AV1394_TNF_EXIT(av1394_detach);
		return (DDI_FAILURE);
	}

	switch (cmd) {
	case DDI_DETACH:
		av1394_cleanup(avp, AV1394_CLEANUP_LEVEL_MAX);
		AV1394_TNF_EXIT(av1394_detach);
		return (DDI_SUCCESS);
	case DDI_SUSPEND:
		return (av1394_cpr_suspend(avp));
	default:
		AV1394_TNF_EXIT(av1394_detach);
		return (DDI_FAILURE);
	}
}

/*ARGSUSED*/
static int
av1394_getinfo(dev_info_t *dip, ddi_info_cmd_t infocmd, void *arg,
		void **result)
{
	dev_t		dev = (dev_t)arg;
	av1394_inst_t	*avp;
	int		rval = DDI_FAILURE;

	switch (infocmd) {
	case DDI_INFO_DEVT2DEVINFO:
		if ((avp = AV1394_DEV2STATE(dev)) != NULL) {
			*result = avp->av_dip;
			rval = DDI_SUCCESS;
		} else {
			*result = NULL;
		}
		break;
	case DDI_INFO_DEVT2INSTANCE:
		*result = (void *)(uintptr_t)AV1394_DEV2INST(dev);
		rval = DDI_SUCCESS;
		break;
	}

	return (rval);
}

/*ARGSUSED*/
static int
av1394_open(dev_t *dev, int flag, int otyp, cred_t *cr)
{
	av1394_inst_t	*avp = AV1394_DEV2STATE(*dev);
	int		ret = ENXIO;

	AV1394_TNF_ENTER(av1394_open);
	if (avp != NULL) {
		if (AV1394_DEV_IS_ISOCH(*dev)) {
			ret = 0;
		} else if (AV1394_DEV_IS_ASYNC(*dev)) {
			ret = av1394_async_open(avp, flag);
		}
	}
	AV1394_TNF_EXIT(av1394_open);
	return (ret);
}

/*ARGSUSED*/
static int
av1394_close(dev_t dev, int flag, int otyp, cred_t *cr)
{
	av1394_inst_t	*avp = AV1394_DEV2STATE(dev);
	int		ret = ENXIO;

	AV1394_TNF_ENTER(av1394_close);
	if (avp != NULL) {
		if (AV1394_DEV_IS_ISOCH(dev)) {
			ret = av1394_isoch_close(avp, flag);
		} else if (AV1394_DEV_IS_ASYNC(dev)) {
			ret = av1394_async_close(avp, flag);
		}
	}
	AV1394_TNF_EXIT(av1394_close);
	return (ret);
}

/*ARGSUSED*/
static int
av1394_read(dev_t dev, struct uio *uiop, cred_t *cr)
{
	av1394_inst_t	*avp = AV1394_DEV2STATE(dev);
	int		ret = ENXIO;

	AV1394_TNF_ENTER(av1394_read);
	if (avp != NULL) {
		if (AV1394_DEV_IS_ISOCH(dev)) {
			ret = av1394_isoch_read(avp, uiop);
		} else if (AV1394_DEV_IS_ASYNC(dev)) {
			ret = av1394_async_read(avp, uiop);
		}
	}
	AV1394_TNF_EXIT(av1394_read);
	return (ret);
}

/*ARGSUSED*/
static int
av1394_write(dev_t dev, struct uio *uiop, cred_t *cr)
{
	av1394_inst_t	*avp = AV1394_DEV2STATE(dev);
	int		ret = ENXIO;

	AV1394_TNF_ENTER(av1394_write);
	if (avp != NULL) {
		if (AV1394_DEV_IS_ISOCH(dev)) {
			ret = av1394_isoch_write(avp, uiop);
		} else if (AV1394_DEV_IS_ASYNC(dev)) {
			ret = av1394_async_write(avp, uiop);
		}
	}
	AV1394_TNF_EXIT(av1394_write);
	return (ret);
}

/*ARGSUSED*/
static int
av1394_ioctl(dev_t dev, int cmd, intptr_t arg, int mode, cred_t *cr, int *rvalp)
{
	av1394_inst_t	*avp = AV1394_DEV2STATE(dev);
	int		ret = ENXIO;

	AV1394_TNF_ENTER(av1394_ioctl);
	if (avp != NULL) {
		if (AV1394_DEV_IS_ISOCH(dev)) {
			ret = av1394_isoch_ioctl(avp, cmd, arg, mode, rvalp);
		} else if (AV1394_DEV_IS_ASYNC(dev)) {
			ret = av1394_async_ioctl(avp, cmd, arg, mode, rvalp);
		}
	}
	AV1394_TNF_EXIT(av1394_ioctl);
	return (ret);
}

/*ARGSUSED*/
static int
av1394_devmap(dev_t dev, devmap_cookie_t dhp, offset_t off, size_t len,
	size_t *maplen, uint_t model)
{
	av1394_inst_t	*avp = AV1394_DEV2STATE(dev);
	int		ret = ENXIO;

	AV1394_TNF_ENTER(av1394_devmap);
	if ((avp != NULL) && (AV1394_DEV_IS_ISOCH(dev))) {
		ret = av1394_isoch_devmap(avp, dhp, off, len, maplen, model);
	}
	AV1394_TNF_EXIT(av1394_devmap);
	return (ret);
}

static int
av1394_poll(dev_t dev, short events, int anyyet, short *reventsp,
		struct pollhead **phpp)
{
	av1394_inst_t	*avp = AV1394_DEV2STATE(dev);
	int		ret = ENXIO;

	AV1394_TNF_ENTER(av1394_poll);
	if ((avp != NULL) && AV1394_DEV_IS_ASYNC(dev)) {
		ret = av1394_async_poll(avp, events, anyyet, reventsp, phpp);
	}
	AV1394_TNF_EXIT(av1394_poll);
	return (ret);
}


/*
 *
 * --- configuration routines
 *
 * av1394_cleanup()
 *    Cleanup after attach
 */
static void
av1394_cleanup(av1394_inst_t *avp, int level)
{
	ASSERT((level > 0) && (level <= AV1394_CLEANUP_LEVEL_MAX));

	AV1394_TNF_ENTER(av1394_cleanup);
	switch (level) {
	default:
		av1394_async_detach(avp);
		/* FALLTHRU */
	case 4:
		av1394_isoch_detach(avp);
		/* FALLTHRU */
	case 3:
		av1394_remove_events(avp);
		/* FALLTHRU */
	case 2:
		av1394_t1394_detach(avp);
		mutex_destroy(&avp->av_mutex);
		/* FALLTHRU */
	case 1:
		ddi_soft_state_free(av1394_statep, avp->av_instance);
	}
	AV1394_TNF_EXIT(av1394_cleanup);
}

static int
av1394_t1394_attach(av1394_inst_t *avp, dev_info_t *dip)
{
	int	ret;

	AV1394_TNF_ENTER(av1394_t1394_attach);

	ret = t1394_attach(dip, T1394_VERSION_V1, 0, &avp->av_attachinfo,
			&avp->av_t1394_hdl);

	if (ret != DDI_SUCCESS) {
		TNF_PROBE_1(av1394_t1394_attach_error, AV1394_TNF_INST_ERROR,
		    "", tnf_int, ret, ret);
	}

	AV1394_TNF_EXIT(av1394_t1394_attach);
	return (ret);
}

static void
av1394_t1394_detach(av1394_inst_t *avp)
{
	AV1394_TNF_ENTER(av1394_t1394_detach);

	(void) t1394_detach(&avp->av_t1394_hdl, 0);

	AV1394_TNF_EXIT(av1394_t1394_detach);
}

static int
av1394_add_events(av1394_inst_t *avp)
{
	ddi_eventcookie_t	br_evc, rem_evc, ins_evc;

	if (ddi_get_eventcookie(avp->av_dip, DDI_DEVI_BUS_RESET_EVENT,
	    &br_evc) != DDI_SUCCESS) {
		TNF_PROBE_0(av1394_add_events_error_bus_reset_cookie,
		    AV1394_TNF_INST_ERROR, "");
		return (DDI_FAILURE);
	}
	if (ddi_add_event_handler(avp->av_dip, br_evc, av1394_bus_reset,
	    avp, &avp->av_reset_cb) != DDI_SUCCESS) {
		TNF_PROBE_0(av1394_add_events_error_bus_reset_event,
		    AV1394_TNF_INST_ERROR, "");
		return (DDI_FAILURE);
	}

	if (ddi_get_eventcookie(avp->av_dip, DDI_DEVI_REMOVE_EVENT,
	    &rem_evc) != DDI_SUCCESS) {
		(void) ddi_remove_event_handler(avp->av_reset_cb);
		TNF_PROBE_0(av1394_add_events_error_remove_cookie,
		    AV1394_TNF_INST_ERROR, "");
		return (DDI_FAILURE);
	}
	if (ddi_add_event_handler(avp->av_dip, rem_evc, av1394_disconnect,
	    avp, &avp->av_remove_cb) != DDI_SUCCESS) {
		(void) ddi_remove_event_handler(avp->av_reset_cb);
		TNF_PROBE_0(av1394_add_events_error_remove_event,
		    AV1394_TNF_INST_ERROR, "");
		return (DDI_FAILURE);
	}

	if (ddi_get_eventcookie(avp->av_dip, DDI_DEVI_INSERT_EVENT,
	    &ins_evc) != DDI_SUCCESS) {
		(void) ddi_remove_event_handler(avp->av_remove_cb);
		(void) ddi_remove_event_handler(avp->av_reset_cb);
		TNF_PROBE_0(av1394_add_events_error_insert_cookie,
		    AV1394_TNF_INST_ERROR, "");
		return (DDI_FAILURE);
	}
	if (ddi_add_event_handler(avp->av_dip, ins_evc, av1394_reconnect,
	    avp, &avp->av_insert_cb) != DDI_SUCCESS) {
		(void) ddi_remove_event_handler(avp->av_remove_cb);
		(void) ddi_remove_event_handler(avp->av_reset_cb);
		TNF_PROBE_0(av1394_add_events_error_insert_event,
		    AV1394_TNF_INST_ERROR, "");
		return (DDI_FAILURE);
	}

	return (DDI_SUCCESS);
}

static void
av1394_remove_events(av1394_inst_t *avp)
{
	ddi_eventcookie_t	evc;

	if (ddi_get_eventcookie(avp->av_dip, DDI_DEVI_INSERT_EVENT,
	    &evc) == DDI_SUCCESS) {
		(void) ddi_remove_event_handler(avp->av_insert_cb);
	}

	if (ddi_get_eventcookie(avp->av_dip, DDI_DEVI_REMOVE_EVENT,
	    &evc) == DDI_SUCCESS) {
		(void) ddi_remove_event_handler(avp->av_remove_cb);
	}

	if (ddi_get_eventcookie(avp->av_dip, DDI_DEVI_BUS_RESET_EVENT,
	    &evc) == DDI_SUCCESS) {
		(void) ddi_remove_event_handler(avp->av_reset_cb);
	}
}

/*
 *
 * --- CPR
 *
 */
static int
av1394_cpr_suspend(av1394_inst_t *avp)
{
	int	ret;

	AV1394_TNF_ENTER(av1394_cpr_suspend);

	ret = av1394_isoch_cpr_suspend(avp);

	if (ret == DDI_SUCCESS) {
		mutex_enter(&avp->av_mutex);
		avp->av_prev_dev_state = avp->av_dev_state;
		avp->av_dev_state = AV1394_DEV_SUSPENDED;
		mutex_exit(&avp->av_mutex);
	}

	AV1394_TNF_EXIT(av1394_cpr_suspend);
	return (ret);
}

/*
 * CPR resume should always succeed
 */
static int
av1394_cpr_resume(av1394_inst_t *avp)
{
	AV1394_TNF_ENTER(av1394_cpr_resume);

	mutex_enter(&avp->av_mutex);
	avp->av_dev_state = avp->av_prev_dev_state;
	mutex_exit(&avp->av_mutex);

	(void) av1394_async_cpr_resume(avp);

	AV1394_TNF_EXIT(av1394_cpr_resume);
	return (DDI_SUCCESS);
}

/*
 *
 * --- callbacks
 *
 */
/*ARGSUSED*/
static void
av1394_bus_reset(dev_info_t *dip, ddi_eventcookie_t evc, void *arg, void *data)
{
	av1394_inst_t	*avp = arg;

	AV1394_TNF_ENTER(av1394_bus_reset);

	if (avp == NULL) {
		AV1394_TNF_EXIT(av1394_bus_reset);
		return;
	}

	mutex_enter(&avp->av_mutex);
	avp->av_attachinfo.localinfo = *(t1394_localinfo_t *)data;
	mutex_exit(&avp->av_mutex);

	av1394_async_bus_reset(avp);
	av1394_cmp_bus_reset(avp);

	AV1394_TNF_EXIT(av1394_bus_reset);
}

/*ARGSUSED*/
static void
av1394_disconnect(dev_info_t *dip, ddi_eventcookie_t evc, void *arg, void *data)
{
	av1394_inst_t	*avp = arg;

	AV1394_TNF_ENTER(av1394_disconnect);

	if (avp == NULL) {
		AV1394_TNF_EXIT(av1394_disconnect);
		return;
	}

	mutex_enter(&avp->av_mutex);
	avp->av_dev_state = AV1394_DEV_DISCONNECTED;
	mutex_exit(&avp->av_mutex);

	AV1394_TNF_EXIT(av1394_disconnect);
}

/*ARGSUSED*/
static void
av1394_reconnect(dev_info_t *dip, ddi_eventcookie_t evc, void *arg, void *data)
{
	av1394_inst_t	*avp = arg;

	AV1394_TNF_ENTER(av1394_disconnect);

	if (avp == NULL) {
		AV1394_TNF_EXIT(av1394_disconnect);
		return;
	}

	mutex_enter(&avp->av_mutex);
	avp->av_dev_state = AV1394_DEV_ONLINE;
	avp->av_attachinfo.localinfo = *(t1394_localinfo_t *)data;
	mutex_exit(&avp->av_mutex);

	av1394_async_reconnect(avp);

	AV1394_TNF_EXIT(av1394_disconnect);
}
