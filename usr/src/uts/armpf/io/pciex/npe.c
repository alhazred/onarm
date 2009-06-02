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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007-2008 NEC Corporation
 */

/*
 *	Host to PCI-Express local bus driver
 */

#include <sys/conf.h>
#include <sys/modctl.h>
#include <sys/pcie.h>
#include <sys/pci_impl_arm.h>
#include <sys/sysmacros.h>
#include <sys/ddi_intr.h>
#include <sys/sunndi.h>
#include <sys/sunddi.h>
#include <sys/ddifm.h>
#include <sys/ndifm.h>
#include <sys/fm/util.h>
#include <io/pci/pci_common.h>
#include <io/pciex/pcie_error.h>

/*
 * Bus Operation functions
 */
static int	npe_bus_map(dev_info_t *, dev_info_t *, ddi_map_req_t *,
		    off_t, off_t, caddr_t *);
static int	npe_ctlops(dev_info_t *, dev_info_t *, ddi_ctl_enum_t,
		    void *, void *);
static int	npe_intr_ops(dev_info_t *, dev_info_t *, ddi_intr_op_t,
		    ddi_intr_handle_impl_t *, void *);
static int	npe_fm_init(dev_info_t *, dev_info_t *, int,
		    ddi_iblock_cookie_t *);

static int	npe_fm_callback(dev_info_t *, ddi_fm_error_t *, const void *);

struct bus_ops npe_bus_ops = {
	BUSO_REV,
	npe_bus_map,
	NULL,
	NULL,
	NULL,
	i_ddi_map_fault,
	ddi_dma_map,
	ddi_dma_allochdl,
	ddi_dma_freehdl,
	ddi_dma_bindhdl,
	ddi_dma_unbindhdl,
	ddi_dma_flush,
	ddi_dma_win,
	ddi_dma_mctl,
	npe_ctlops,
	ddi_bus_prop_op,
	0,		/* (*bus_get_eventcookie)();	*/
	0,		/* (*bus_add_eventcall)();	*/
	0,		/* (*bus_remove_eventcall)();	*/
	0,		/* (*bus_post_event)();		*/
	0,		/* (*bus_intr_ctl)(); */
	0,		/* (*bus_config)(); */
	0,		/* (*bus_unconfig)(); */
	npe_fm_init,	/* (*bus_fm_init)(); */
	NULL,		/* (*bus_fm_fini)(); */
	NULL,		/* (*bus_fm_access_enter)(); */
	NULL,		/* (*bus_fm_access_exit)(); */
	NULL,		/* (*bus_power)(); */
	npe_intr_ops	/* (*bus_intr_op)(); */
};


/*
 * Device Node Operation functions
 */
static int	npe_attach(dev_info_t *devi, ddi_attach_cmd_t cmd);
static int	npe_detach(dev_info_t *devi, ddi_detach_cmd_t cmd);

struct dev_ops npe_ops = {
	DEVO_REV,		/* devo_rev */
	0,			/* refcnt  */
	nulldev,		/* info */
	nulldev,		/* identify */
	nulldev,		/* probe */
	npe_attach,		/* attach */
	npe_detach,		/* detach */
	nulldev,		/* reset */
	NULL,			/* driver operations */
	&npe_bus_ops		/* bus operations */
};

/*
 * Internal routines in support of particular npe_ctlops.
 */
static int npe_removechild(dev_info_t *child);
static int npe_initchild(dev_info_t *child);

/*
 * External support routine
 */
extern void	npe_query_acpi_mcfg(dev_info_t *dip);
extern void	npe_ck804_fix_aer_ptr(ddi_acc_handle_t cfg_hdl);
extern int	npe_disable_empty_bridges_workaround(dev_info_t *child);

/*
 * Module linkage information for the kernel.
 */
static struct modldrv modldrv = {
	&mod_driverops, /* Type of module */
	"Host to PCIe nexus driver",
	&npe_ops,	/* driver ops */
};

static struct modlinkage modlinkage = {
	MODREV_1,
	(void *)&modldrv,
	NULL
};

/* Save minimal state. */
void *npe_statep;

int
MODDRV_ENTRY_INIT(void)
{
	int e;

	/*
	 * Initialize per-pci bus soft state pointer.
	 */
	e = ddi_soft_state_init(&npe_statep, sizeof (pci_state_t), 1);
	if (e != 0)
		return (e);

	if ((e = mod_install(&modlinkage)) != 0)
		ddi_soft_state_fini(&npe_statep);

	return (e);
}

#ifndef STATIC_DRIVER
int
MODDRV_ENTRY_FINI(void)
{
	int rc;

	rc = mod_remove(&modlinkage);
	if (rc != 0)
		return (rc);

	ddi_soft_state_fini(&npe_statep);
	return (rc);
}
#endif /* !STATIC_DRIVER */


int
MODDRV_ENTRY_INFO(struct modinfo *modinfop)
{
	return (mod_info(&modlinkage, modinfop));
}

/*ARGSUSED*/
static int
npe_attach(dev_info_t *devi, ddi_attach_cmd_t cmd)
{
	/*
	 * Use the minor number as constructed by pcihp, as the index value to
	 * ddi_soft_state_zalloc.
	 */
	int instance = ddi_get_instance(devi);
	pci_state_t *pcip = NULL;

	if (cmd == DDI_RESUME)
		return (DDI_SUCCESS);

	if (ddi_prop_update_string(DDI_DEV_T_NONE, devi, "device_type",
	    "pciex") != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN, "npe:  'device_type' prop create failed");
	}

	if (ddi_soft_state_zalloc(npe_statep, instance) == DDI_SUCCESS)
		pcip = ddi_get_soft_state(npe_statep, instance);

	if (pcip == NULL)
		return (DDI_FAILURE);

	pcip->pci_dip = devi;

	pcip->pci_fmcap = DDI_FM_EREPORT_CAPABLE | DDI_FM_ERRCB_CAPABLE |
	    DDI_FM_ACCCHK_CAPABLE | DDI_FM_DMACHK_CAPABLE;
	ddi_fm_init(devi, &pcip->pci_fmcap, &pcip->pci_fm_ibc);

	if (pcip->pci_fmcap & DDI_FM_ERRCB_CAPABLE)
		ddi_fm_handler_register(devi, npe_fm_callback, NULL);

	npe_query_acpi_mcfg(devi);
	ddi_report_dev(devi);

	return (DDI_SUCCESS);
}

/*ARGSUSED*/
static int
npe_detach(dev_info_t *devi, ddi_detach_cmd_t cmd)
{
	int instance = ddi_get_instance(devi);
	pci_state_t *pcip;

	pcip = ddi_get_soft_state(npe_statep, ddi_get_instance(devi));

	switch (cmd) {
	case DDI_DETACH:
		if (pcip->pci_fmcap & DDI_FM_ERRCB_CAPABLE)
			ddi_fm_handler_unregister(devi);

		ddi_fm_fini(devi);
		ddi_soft_state_free(npe_statep, instance);
		return (DDI_SUCCESS);

	case DDI_SUSPEND:
		return (DDI_SUCCESS);
	default:
		return (DDI_FAILURE);
	}
}


static int
npe_bus_map(dev_info_t *dip, dev_info_t *rdip, ddi_map_req_t *mp,
    off_t offset, off_t len, caddr_t *vaddrp)
{
	int 		rnumber;
	int		length;
	int		space;
	ddi_acc_impl_t	*ap;
	ddi_acc_hdl_t	*hp;
	ddi_map_req_t	mr;
	pci_regspec_t	pci_reg;
	pci_regspec_t	*pci_rp;
	struct regspec	reg;
	pci_acc_cfblk_t	*cfp;
	int		retval;

	mr = *mp; /* Get private copy of request */
	mp = &mr;

	/*
	 * check for register number
	 */
	switch (mp->map_type) {
	case DDI_MT_REGSPEC:
		pci_reg = *(pci_regspec_t *)(mp->map_obj.rp);
		pci_rp = &pci_reg;
		if (pci_common_get_reg_prop(rdip, pci_rp) != DDI_SUCCESS)
			return (DDI_FAILURE);
		break;
	case DDI_MT_RNUMBER:
		rnumber = mp->map_obj.rnumber;
		/*
		 * get ALL "reg" properties for dip, select the one of
		 * of interest. In x86, "assigned-addresses" property
		 * is identical to the "reg" property, so there is no
		 * need to cross check the two to determine the physical
		 * address of the registers.
		 * This routine still performs some validity checks to
		 * make sure that everything is okay.
		 */
		if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, rdip,
		    DDI_PROP_DONTPASS, "reg", (int **)&pci_rp,
		    (uint_t *)&length) != DDI_PROP_SUCCESS)
			return (DDI_FAILURE);

		/*
		 * validate the register number.
		 */
		length /= (sizeof (pci_regspec_t) / sizeof (int));
		if (rnumber >= length) {
			ddi_prop_free(pci_rp);
			return (DDI_FAILURE);
		}

		/*
		 * copy the required entry.
		 */
		pci_reg = pci_rp[rnumber];

		/*
		 * free the memory allocated by ddi_prop_lookup_int_array
		 */
		ddi_prop_free(pci_rp);

		pci_rp = &pci_reg;
		if (pci_common_get_reg_prop(rdip, pci_rp) != DDI_SUCCESS)
			return (DDI_FAILURE);
		mp->map_type = DDI_MT_REGSPEC;
		break;
	default:
		return (DDI_ME_INVAL);
	}

	space = pci_rp->pci_phys_hi & PCI_REG_ADDR_M;

	/*
	 * check for unmap and unlock of address space
	 */
	if ((mp->map_op == DDI_MO_UNMAP) || (mp->map_op == DDI_MO_UNLOCK)) {
		switch (space) {
		case PCI_ADDR_CONFIG:
			/*
			 * Check for any PCI device.
			 *
			 * This is a workaround fix
			 * to disable MMCFG for any PCI device.
			 *
			 * If a device is *not* found to have PCIe
			 * capability, then assume it is a PCI device.
			 */

			if (ddi_prop_get_int(DDI_DEV_T_ANY, rdip,
			    DDI_PROP_DONTPASS, "pcie-capid-pointer",
			    PCI_CAP_NEXT_PTR_NULL) == PCI_CAP_NEXT_PTR_NULL) {
				if (DDI_FM_ACC_ERR_CAP(ddi_fm_capable(rdip)) &&
				    mp->map_handlep->ah_acc.devacc_attr_access
				    != DDI_DEFAULT_ACC) {
					ndi_fmc_remove(rdip, ACC_HANDLE,
					    (void *)mp->map_handlep);
				}
				return (DDI_SUCCESS);
			}


			/* FALLTHROUGH */
		case PCI_ADDR_MEM64:
			/*
			 * MEM64 requires special treatment on map, to check
			 * that the device is below 4G.  On unmap, however,
			 * we can assume that everything is OK... the map
			 * must have succeeded.
			 */
			/* FALLTHROUGH */
		case PCI_ADDR_MEM32:
			reg.regspec_bustype = 0;
			break;

		default:
			return (DDI_FAILURE);
		}

		/*
		 * Adjust offset and length
		 * A non-zero length means override the one in the regspec.
		 */
		pci_rp->pci_phys_low += (uint_t)offset;
		if (len != 0)
			pci_rp->pci_size_low = len;

		reg.regspec_addr = pci_rp->pci_phys_low;
		reg.regspec_size = pci_rp->pci_size_low;

		mp->map_obj.rp = &reg;
		retval = ddi_map(dip, mp, (off_t)0, (off_t)0, vaddrp);
		if (DDI_FM_ACC_ERR_CAP(ddi_fm_capable(rdip)) &&
		    mp->map_handlep->ah_acc.devacc_attr_access !=
		    DDI_DEFAULT_ACC) {
			ndi_fmc_remove(rdip, ACC_HANDLE,
			    (void *)mp->map_handlep);
		}
		return (retval);

	}

	/* check for user mapping request - not legal for Config */
	if (mp->map_op == DDI_MO_MAP_HANDLE && space == PCI_ADDR_CONFIG) {
		cmn_err(CE_NOTE, "npe: Config mapping request from user\n");
		return (DDI_FAILURE);
	}


	/*
	 * Note that pci_fm_acc_setup() is called to serve two purposes
	 * i) enable legacy PCI I/O style config space access
	 * ii) register with FMA
	 */
	if (space == PCI_ADDR_CONFIG) {
		/* Can't map config space without a handle */
		hp = (ddi_acc_hdl_t *)mp->map_handlep;
		if (hp == NULL)
			return (DDI_FAILURE);

		/* record the device address for future reference */
		cfp = (pci_acc_cfblk_t *)&hp->ah_bus_private;
		cfp->c_busnum = PCI_REG_BUS_G(pci_rp->pci_phys_hi);
		cfp->c_devnum = PCI_REG_DEV_G(pci_rp->pci_phys_hi);
		cfp->c_funcnum = PCI_REG_FUNC_G(pci_rp->pci_phys_hi);

		*vaddrp = (caddr_t)offset;

		/*
		 * Check for pci devices and devices underneath a pci bridge.
		 * This is to setup I/O based config space access.
		 */
		if (ddi_prop_get_int(DDI_DEV_T_ANY, rdip, DDI_PROP_DONTPASS,
		    "pcie-capid-pointer", PCI_CAP_NEXT_PTR_NULL) ==
		    PCI_CAP_NEXT_PTR_NULL) {
			int ret;
			if ((ret = pci_fm_acc_setup(hp, offset, len)) ==
			    DDI_SUCCESS) {
				if (DDI_FM_ACC_ERR_CAP(ddi_fm_capable(rdip)) &&
				    mp->map_handlep->ah_acc.devacc_attr_access
				    != DDI_DEFAULT_ACC) {
					ndi_fmc_insert(rdip, ACC_HANDLE,
					    (void *)mp->map_handlep, NULL);
				}
			}
			return (ret);
		}

		pci_rp->pci_phys_low = ddi_prop_get_int64(DDI_DEV_T_ANY,
		    rdip, 0, "ecfga-base-address", 0);

		pci_rp->pci_phys_low += ((cfp->c_busnum << 20) |
		    (cfp->c_devnum) << 15 | (cfp->c_funcnum << 12));

		pci_rp->pci_size_low = PCIE_CONF_HDR_SIZE;
	}

	length = pci_rp->pci_size_low;

	/*
	 * range check
	 */
	if ((offset >= length) || (len > length) || (offset + len > length))
		return (DDI_FAILURE);

	/*
	 * Adjust offset and length
	 * A non-zero length means override the one in the regspec.
	 */
	pci_rp->pci_phys_low += (uint_t)offset;
	if (len != 0)
		pci_rp->pci_size_low = len;

	/*
	 * convert the pci regsec into the generic regspec used by the
	 * parent root nexus driver.
	 */
	switch (space) {
	case PCI_ADDR_CONFIG:
	case PCI_ADDR_MEM64:
		/*
		 * We can't handle 64-bit devices that are mapped above
		 * 4G or that are larger than 4G.
		 */
		if (pci_rp->pci_phys_mid != 0 || pci_rp->pci_size_hi != 0)
			return (DDI_FAILURE);
		/*
		 * Other than that, we can treat them as 32-bit mappings
		 */
		/* FALLTHROUGH */
	case PCI_ADDR_MEM32:
		reg.regspec_bustype = 0;
		break;
	default:
		return (DDI_FAILURE);
	}

	reg.regspec_addr = pci_rp->pci_phys_low;
	reg.regspec_size = pci_rp->pci_size_low;

	mp->map_obj.rp = &reg;
	retval = ddi_map(dip, mp, (off_t)0, (off_t)0, vaddrp);

	if (retval == DDI_SUCCESS) {
		/*
		 * For config space gets force use of cautious access routines.
		 * These will handle default and protected mode accesses too.
		 */
		if (space == PCI_ADDR_CONFIG) {
			ap = (ddi_acc_impl_t *)mp->map_handlep;
			ap->ahi_acc_attr &= ~DDI_ACCATTR_DIRECT;
			ap->ahi_acc_attr |= DDI_ACCATTR_CONFIG_SPACE;
			ap->ahi_get8 = i_ddi_caut_get8;
			ap->ahi_get16 = i_ddi_caut_get16;
			ap->ahi_get32 = i_ddi_caut_get32;
			ap->ahi_get64 = i_ddi_caut_get64;
			ap->ahi_rep_get8 = i_ddi_caut_rep_get8;
			ap->ahi_rep_get16 = i_ddi_caut_rep_get16;
			ap->ahi_rep_get32 = i_ddi_caut_rep_get32;
			ap->ahi_rep_get64 = i_ddi_caut_rep_get64;
		}
		if (DDI_FM_ACC_ERR_CAP(ddi_fm_capable(rdip)) &&
		    mp->map_handlep->ah_acc.devacc_attr_access !=
		    DDI_DEFAULT_ACC) {
			ndi_fmc_insert(rdip, ACC_HANDLE,
			    (void *)mp->map_handlep, NULL);
		}
	}
	return (retval);
}

/*ARGSUSED*/
static int
npe_ctlops(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t ctlop, void *arg, void *result)
{
	int		rn;
	int		totreg;
	uint_t		reglen;
	pci_regspec_t	*drv_regp;
	struct	attachspec *asp;

	switch (ctlop) {
	case DDI_CTLOPS_REPORTDEV:
		if (rdip == (dev_info_t *)0)
			return (DDI_FAILURE);
		cmn_err(CE_CONT, "?PCI Express-device: %s@%s, %s%d\n",
		    ddi_node_name(rdip), ddi_get_name_addr(rdip),
		    ddi_driver_name(rdip), ddi_get_instance(rdip));
		return (DDI_SUCCESS);

	case DDI_CTLOPS_INITCHILD:
		return (npe_initchild((dev_info_t *)arg));

	case DDI_CTLOPS_UNINITCHILD:
		return (npe_removechild((dev_info_t *)arg));

	case DDI_CTLOPS_SIDDEV:
		return (DDI_SUCCESS);

	case DDI_CTLOPS_REGSIZE:
	case DDI_CTLOPS_NREGS:
		if (rdip == (dev_info_t *)0)
			return (DDI_FAILURE);

		*(int *)result = 0;
		if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, rdip,
		    DDI_PROP_DONTPASS, "reg", (int **)&drv_regp,
		    &reglen) != DDI_PROP_SUCCESS) {
			return (DDI_FAILURE);
		}

		totreg = (reglen * sizeof (int)) / sizeof (pci_regspec_t);
		if (ctlop == DDI_CTLOPS_NREGS)
			*(int *)result = totreg;
		else if (ctlop == DDI_CTLOPS_REGSIZE) {
			rn = *(int *)arg;
			if (rn >= totreg) {
				ddi_prop_free(drv_regp);
				return (DDI_FAILURE);
			}
			*(off_t *)result = drv_regp[rn].pci_size_low;
		}
		ddi_prop_free(drv_regp);

		return (DDI_SUCCESS);

	case DDI_CTLOPS_POWER:
	{
		power_req_t	*reqp = (power_req_t *)arg;
		/*
		 * We currently understand reporting of PCI_PM_IDLESPEED
		 * capability. Everything else is passed up.
		 */
		if ((reqp->request_type == PMR_REPORT_PMCAP) &&
		    (reqp->req.report_pmcap_req.cap ==  PCI_PM_IDLESPEED))
			return (DDI_SUCCESS);

		break;
	}

	case DDI_CTLOPS_PEEK:
	case DDI_CTLOPS_POKE:
		return (pci_common_peekpoke(dip, rdip, ctlop, arg, result));

	/* X86 systems support PME wakeup from suspended state */
	case DDI_CTLOPS_ATTACH:
		asp = (struct attachspec *)arg;
		/* only do this for immediate children */
		if (asp->cmd == DDI_RESUME && asp->when == DDI_PRE &&
		    ddi_get_parent(rdip) == dip)
			if (pci_pre_resume(rdip) != DDI_SUCCESS) {
				/* Not good, better stop now. */
				cmn_err(CE_PANIC,
				    "Couldn't pre-resume device %p",
				    (void *) dip);
				/* NOTREACHED */
			}
		return (ddi_ctlops(dip, rdip, ctlop, arg, result));

	case DDI_CTLOPS_DETACH:
		asp = (struct attachspec *)arg;
		/* only do this for immediate children */
		if (asp->cmd == DDI_SUSPEND && asp->when == DDI_POST &&
		    ddi_get_parent(rdip) == dip)
			if (pci_post_suspend(rdip) != DDI_SUCCESS)
				return (DDI_FAILURE);
		return (ddi_ctlops(dip, rdip, ctlop, arg, result));

	default:
		break;
	}

	return (ddi_ctlops(dip, rdip, ctlop, arg, result));

}


/*
 * npe_intr_ops
 */
static int
npe_intr_ops(dev_info_t *pdip, dev_info_t *rdip, ddi_intr_op_t intr_op,
    ddi_intr_handle_impl_t *hdlp, void *result)
{
	return (pci_common_intr_ops(pdip, rdip, intr_op, hdlp, result));
}


static int
npe_initchild(dev_info_t *child)
{
	char			name[80];
	ddi_acc_handle_t	cfg_hdl;

	/*
	 * Do not bind drivers to empty bridges.
	 * Fail above, if the bridge is found to be hotplug capable
	 */
	if (npe_disable_empty_bridges_workaround(child) == 1)
		return (DDI_FAILURE);

	if (pci_common_name_child(child, name, 80) != DDI_SUCCESS)
		return (DDI_FAILURE);

	ddi_set_name_addr(child, name);

	/*
	 * Pseudo nodes indicate a prototype node with per-instance
	 * properties to be merged into the real h/w device node.
	 * The interpretation of the unit-address is DD[,F]
	 * where DD is the device id and F is the function.
	 */
	if (ndi_dev_is_persistent_node(child) == 0) {
		extern int pci_allow_pseudo_children;

		ddi_set_parent_data(child, NULL);

		/*
		 * Try to merge the properties from this prototype
		 * node into real h/w nodes.
		 */
		if (ndi_merge_node(child, pci_common_name_child) ==
		    DDI_SUCCESS) {
			/*
			 * Merged ok - return failure to remove the node.
			 */
			ddi_set_name_addr(child, NULL);
			return (DDI_FAILURE);
		}

		/* workaround for DDIVS to run under PCI Express */
		if (pci_allow_pseudo_children) {
			/*
			 * If the "interrupts" property doesn't exist,
			 * this must be the ddivs no-intr case, and it returns
			 * DDI_SUCCESS instead of DDI_FAILURE.
			 */
			if (ddi_prop_get_int(DDI_DEV_T_ANY, child,
			    DDI_PROP_DONTPASS, "interrupts", -1) == -1)
				return (DDI_SUCCESS);
			/*
			 * Create the ddi_parent_private_data for a pseudo
			 * child.
			 */
			pci_common_set_parent_private_data(child);
			return (DDI_SUCCESS);
		}

		/*
		 * The child was not merged into a h/w node,
		 * but there's not much we can do with it other
		 * than return failure to cause the node to be removed.
		 */
		cmn_err(CE_WARN, "!%s@%s: %s.conf properties not merged",
		    ddi_get_name(child), ddi_get_name_addr(child),
		    ddi_get_name(child));
		ddi_set_name_addr(child, NULL);
		return (DDI_NOT_WELL_FORMED);
	}

	if (ddi_prop_get_int(DDI_DEV_T_ANY, child, DDI_PROP_DONTPASS,
	    "interrupts", -1) != -1)
		pci_common_set_parent_private_data(child);
	else
		ddi_set_parent_data(child, NULL);

	/*
	 * Enable AER next pointer being displayed and PCIe Error initilization
	 */
	if (pci_config_setup(child, &cfg_hdl) == DDI_SUCCESS) {
		npe_ck804_fix_aer_ptr(cfg_hdl);
		(void) pcie_error_enable(child, cfg_hdl);
		pci_config_teardown(&cfg_hdl);
	}

	return (DDI_SUCCESS);
}


static int
npe_removechild(dev_info_t *dip)
{
	ddi_acc_handle_t		cfg_hdl;
	struct ddi_parent_private_data	*pdptr;

	/*
	 * Do it way early.
	 * Otherwise ddi_map() call form pcie_error_fini crashes
	 */
	if (pci_config_setup(dip, &cfg_hdl) == DDI_SUCCESS) {
		pcie_error_disable(dip, cfg_hdl);
		pci_config_teardown(&cfg_hdl);
	}

	if ((pdptr = ddi_get_parent_data(dip)) != NULL) {
		kmem_free(pdptr, (sizeof (*pdptr) + sizeof (struct intrspec)));
		ddi_set_parent_data(dip, NULL);
	}
	ddi_set_name_addr(dip, NULL);

	/*
	 * Strip the node to properly convert it back to prototype form
	 */
	ddi_remove_minor_node(dip, NULL);

	ddi_prop_remove_all(dip);

	return (DDI_SUCCESS);
}



/*ARGSUSED*/
static int
npe_fm_init(dev_info_t *dip, dev_info_t *tdip, int cap,
    ddi_iblock_cookie_t *ibc)
{
	pci_state_t  *pcip = ddi_get_soft_state(npe_statep,
	    ddi_get_instance(dip));

	ASSERT(ibc != NULL);
	*ibc = pcip->pci_fm_ibc;

	return (pcip->pci_fmcap);
}

/*ARGSUSED*/
static int
npe_fm_callback(dev_info_t *dip, ddi_fm_error_t *derr, const void *no_used)
{
	return (ndi_fm_handler_dispatch(dip, NULL, derr));
}
