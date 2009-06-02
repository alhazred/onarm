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
 *	File that has code which is common between pci(7d) and npe(7d)
 *	It shares the following:
 *	- interrupt code
 *	- ioctl code
 *	- name_child code
 *	- set_parent_private_data code
 */

#include <sys/conf.h>
#include <sys/pci.h>
#include <sys/sunndi.h>
#include <sys/mach_intr.h>
#include <sys/pci_intr_lib.h>
#include <sys/cpuvar.h>
#include <sys/gic.h>
#include <sys/policy.h>
#include <sys/sysmacros.h>
#include <sys/clock.h>
#include <io/pci/pci_var.h>
#include <io/pci/pci_common.h>
#include <sys/pci_cfgspace.h>
#include <sys/pci_impl_arm.h>
#include <sys/pci_cap.h>
#include <sys/avintr.h>
#include <sys/platform.h>

/*
 * Function prototypes
 */
static int	pci_get_priority(dev_info_t *, ddi_intr_handle_impl_t *, int *);
static int	pci_enable_intr(dev_info_t *, dev_info_t *,
		    ddi_intr_handle_impl_t *, uint32_t);
static void	pci_disable_intr(dev_info_t *, dev_info_t *,
		    ddi_intr_handle_impl_t *, uint32_t);

/* Extern decalration for pcplusmp module */
extern int	gic_intr_ops(dev_info_t *, ddi_intr_handle_impl_t *,
			gic_intr_op_t, int *);


/*
 * pci_name_child:
 *
 *	Assign the address portion of the node name
 */
int
pci_common_name_child(dev_info_t *child, char *name, int namelen)
{
	int		dev, func, length;
	char		**unit_addr;
	uint_t		n;
	pci_regspec_t	*pci_rp;

	if (ndi_dev_is_persistent_node(child) == 0) {
		/*
		 * For .conf node, use "unit-address" property
		 */
		if (ddi_prop_lookup_string_array(DDI_DEV_T_ANY, child,
		    DDI_PROP_DONTPASS, "unit-address", &unit_addr, &n) !=
		    DDI_PROP_SUCCESS) {
			cmn_err(CE_WARN, "cannot find unit-address in %s.conf",
			    ddi_get_name(child));
			return (DDI_FAILURE);
		}
		if (n != 1 || *unit_addr == NULL || **unit_addr == 0) {
			cmn_err(CE_WARN, "unit-address property in %s.conf"
			    " not well-formed", ddi_get_name(child));
			ddi_prop_free(unit_addr);
			return (DDI_FAILURE);
		}
		(void) snprintf(name, namelen, "%s", *unit_addr);
		ddi_prop_free(unit_addr);
		return (DDI_SUCCESS);
	}

	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, child, DDI_PROP_DONTPASS,
	    "reg", (int **)&pci_rp, (uint_t *)&length) != DDI_PROP_SUCCESS) {
		cmn_err(CE_WARN, "cannot find reg property in %s",
		    ddi_get_name(child));
		return (DDI_FAILURE);
	}

	/* copy the device identifications */
	dev = PCI_REG_DEV_G(pci_rp->pci_phys_hi);
	func = PCI_REG_FUNC_G(pci_rp->pci_phys_hi);

	/*
	 * free the memory allocated by ddi_prop_lookup_int_array
	 */
	ddi_prop_free(pci_rp);

	if (func != 0) {
		(void) snprintf(name, namelen, "%x,%x", dev, func);
	} else {
		(void) snprintf(name, namelen, "%x", dev);
	}

	return (DDI_SUCCESS);
}

/*
 * Interrupt related code:
 *
 * The following busop is common to npe and pci drivers
 *	bus_introp
 */

/*
 * Create the ddi_parent_private_data for a pseudo child.
 */
void
pci_common_set_parent_private_data(dev_info_t *dip)
{
	struct ddi_parent_private_data *pdptr;

	pdptr = (struct ddi_parent_private_data *)kmem_zalloc(
	    (sizeof (struct ddi_parent_private_data) +
	    sizeof (struct intrspec)), KM_SLEEP);
	pdptr->par_intr = (struct intrspec *)(pdptr + 1);
	pdptr->par_nintr = 1;
	ddi_set_parent_data(dip, pdptr);
}

/*
 * pci_get_priority:
 *	Figure out the priority of the device
 */
static int
pci_get_priority(dev_info_t *dip, ddi_intr_handle_impl_t *hdlp, int *pri)
{
	struct intrspec *ispec;

	DDI_INTR_NEXDBG((CE_CONT, "pci_get_priority: dip = 0x%p, hdlp = %p\n",
	    (void *)dip, (void *)hdlp));

	if ((ispec = (struct intrspec *)pci_intx_get_ispec(dip, dip,
	    hdlp->ih_inum)) == NULL) {
		if (DDI_INTR_IS_MSI_OR_MSIX(hdlp->ih_type)) {
			int class = ddi_prop_get_int(DDI_DEV_T_ANY, dip,
			    DDI_PROP_DONTPASS, "class-code", -1);

			*pri = (class == -1) ? 1 : pci_devclass_to_ipl(class);
			pci_common_set_parent_private_data(hdlp->ih_dip);
			ispec = (struct intrspec *)pci_intx_get_ispec(dip, dip,
			    hdlp->ih_inum);
			return (DDI_SUCCESS);
		}
		return (DDI_FAILURE);
	}

	*pri = ispec->intrspec_pri;
	return (DDI_SUCCESS);
}



static int pcie_pci_intr_pri_counter = 0;

/*
 * pci_common_intr_ops: bus_intr_op() function for interrupt support
 */
int
pci_common_intr_ops(dev_info_t *pdip, dev_info_t *rdip, ddi_intr_op_t intr_op,
    ddi_intr_handle_impl_t *hdlp, void *result)
{
	int			priority = 0;
	int			gic_status = 0;
	int			pci_status = 0;
	int			pci_rval, gic_rval = GIC_FAILURE;
	int			types = 0;
	int			pciepci = 0;
	int			i, j, count;
	int			behavior;
	int			cap_ptr;
	ddi_intrspec_t		isp;
	struct intrspec		*ispec;
	ddi_intr_handle_impl_t	tmp_hdl;
	ddi_intr_msix_t		*msix_p;
	ihdl_plat_t		*ihdl_plat_datap;
	ddi_intr_handle_t	*h_array;
	ddi_acc_handle_t	handle;

	DDI_INTR_NEXDBG((CE_CONT,
	    "pci_common_intr_ops: pdip 0x%p, rdip 0x%p, op %x handle 0x%p\n",
	    (void *)pdip, (void *)rdip, intr_op, (void *)hdlp));

	/* Process the request */
	switch (intr_op) {
	case DDI_INTROP_SUPPORTED_TYPES:
		/* Fixed supported by default */
		types = DDI_INTR_TYPE_FIXED;

		if (pci_config_setup(rdip, &handle) != DDI_SUCCESS)
			return (DDI_FAILURE);

		DDI_INTR_NEXDBG((CE_CONT, "pci_common_intr_ops: "
		    "rdip: 0x%p supported types: 0x%x\n", (void *)rdip,
		    *(int *)result));

		*(int *)result = types;
		pci_config_teardown(&handle);
		return (DDI_SUCCESS);
	case DDI_INTROP_NAVAIL:
	case DDI_INTROP_NINTRS:
		*(int *)result = i_ddi_get_intx_nintrs(hdlp->ih_dip);
		if (*(int *)result == 0)
			return (DDI_FAILURE);
		break;
	case DDI_INTROP_ALLOC:
		if (hdlp->ih_type == DDI_INTR_TYPE_FIXED) {
			/* Figure out if this device supports MASKING */
			pci_rval = pci_intx_get_cap(rdip, &pci_status);
			if (pci_rval == DDI_SUCCESS && pci_status)
				hdlp->ih_cap |= pci_status;
			*(int *)result = 1;	/* DDI_INTR_TYPE_FIXED */
		} else
			return (DDI_FAILURE);
		break;
	case DDI_INTROP_FREE:
		break;
	case DDI_INTROP_GETPRI:
		/* Get the priority */
		if (pci_get_priority(rdip, hdlp, &priority) != DDI_SUCCESS)
			return (DDI_FAILURE);
		DDI_INTR_NEXDBG((CE_CONT, "pci_common_intr_ops: "
		    "priority = 0x%x\n", priority));
		*(int *)result = priority;
		break;
	case DDI_INTROP_SETPRI:
		/* Validate the interrupt priority passed */
		if (*(int *)result > LOCK_LEVEL)
			return (DDI_FAILURE);

		/* Change the priority */
		if (gic_intr_ops(rdip, hdlp, GIC_INTR_OP_SET_PRI, result) ==
		    GIC_FAILURE)
			return (DDI_FAILURE);

		/* update ispec */
		isp = pci_intx_get_ispec(pdip, rdip, (int)hdlp->ih_inum);
		ispec = (struct intrspec *)isp;
		if (ispec)
			ispec->intrspec_pri = *(int *)result;
		break;
	case DDI_INTROP_ADDISR:
		/* update ispec */
		isp = pci_intx_get_ispec(pdip, rdip, (int)hdlp->ih_inum);
		ispec = (struct intrspec *)isp;
		if (ispec) {
			ispec->intrspec_func = hdlp->ih_cb_func;
			ihdl_plat_datap = (ihdl_plat_t *)hdlp->ih_private;
			pci_kstat_create(&ihdl_plat_datap->ip_ksp, pdip, hdlp);
		}
		break;
	case DDI_INTROP_REMISR:
		/* Get the interrupt structure pointer */
		isp = pci_intx_get_ispec(pdip, rdip, (int)hdlp->ih_inum);
		ispec = (struct intrspec *)isp;
		if (ispec) {
			ispec->intrspec_func = (uint_t (*)()) 0;
			ihdl_plat_datap = (ihdl_plat_t *)hdlp->ih_private;
			if (ihdl_plat_datap->ip_ksp != NULL)
				pci_kstat_delete(ihdl_plat_datap->ip_ksp);
		}
		break;
	case DDI_INTROP_GETCAP:
		if (hdlp->ih_type == DDI_INTR_TYPE_FIXED)
			pci_rval = pci_intx_get_cap(rdip, &pci_status);

		/* next check with pcplusmp */
		gic_rval = gic_intr_ops(rdip, hdlp,
			GIC_INTR_OP_GET_CAP, &gic_status);

		DDI_INTR_NEXDBG((CE_CONT, "pci: GETCAP returned gic_rval = %x, "
		    "gic_status = %x, pci_rval = %x, pci_status = %x\n",
		    gic_rval, gic_status, pci_rval, pci_status));

		if (gic_rval == GIC_FAILURE && pci_rval == DDI_FAILURE) {
			*(int *)result = 0;
			return (DDI_FAILURE);
		}

		if (gic_rval == GIC_SUCCESS)
			*(int *)result = gic_status;

		if (pci_rval == DDI_SUCCESS)
			*(int *)result |= pci_status;

		DDI_INTR_NEXDBG((CE_CONT, "pci: GETCAP returned = %x\n",
		    *(int *)result));
		break;
	case DDI_INTROP_SETCAP:
		DDI_INTR_NEXDBG((CE_CONT, "pci_common_intr_ops: "
		    "SETCAP cap=0x%x\n", *(int *)result));
		if (gic_intr_ops(rdip, hdlp, GIC_INTR_OP_SET_CAP, result)) {
			DDI_INTR_NEXDBG((CE_CONT, "GETCAP: gic_intr_ops"
			    " returned failure\n"));
			return (DDI_FAILURE);
		}
		break;
	case DDI_INTROP_ENABLE:
		DDI_INTR_NEXDBG((CE_CONT, "pci_common_intr_ops: ENABLE\n"));
		if (pci_enable_intr(pdip, rdip, hdlp, hdlp->ih_inum) !=
		    DDI_SUCCESS)
			return (DDI_FAILURE);

		DDI_INTR_NEXDBG((CE_CONT, "pci_common_intr_ops: ENABLE "
		    "vector=0x%x\n", hdlp->ih_vector));
		break;
	case DDI_INTROP_DISABLE:
		DDI_INTR_NEXDBG((CE_CONT, "pci_common_intr_ops: DISABLE\n"));
		pci_disable_intr(pdip, rdip, hdlp, hdlp->ih_inum);
		DDI_INTR_NEXDBG((CE_CONT, "pci_common_intr_ops: DISABLE "
		    "vector = %x\n", hdlp->ih_vector));
		break;
	case DDI_INTROP_BLOCKENABLE:
		DDI_INTR_NEXDBG((CE_CONT, "pci_common_intr_ops: "
		    "BLOCKENABLE\n"));
		if (hdlp->ih_type != DDI_INTR_TYPE_MSI) {
			DDI_INTR_NEXDBG((CE_CONT, "BLOCKENABLE: not MSI\n"));
			return (DDI_FAILURE);
		}

		count = hdlp->ih_scratch1;
		h_array = (ddi_intr_handle_t *)hdlp->ih_scratch2;
		for (i = 0; i < count; i++) {
			hdlp = (ddi_intr_handle_impl_t *)h_array[i];
			if (pci_enable_intr(pdip, rdip, hdlp,
			    hdlp->ih_inum) != DDI_SUCCESS) {
				DDI_INTR_NEXDBG((CE_CONT, "BLOCKENABLE: "
				    "pci_enable_intr failed for %d\n", i));
				for (j = 0; j < i; j++) {
					hdlp = (ddi_intr_handle_impl_t *)
					    h_array[j];
					pci_disable_intr(pdip, rdip, hdlp,
					    hdlp->ih_inum);
				}
				return (DDI_FAILURE);
			}
			DDI_INTR_NEXDBG((CE_CONT, "pci_common_intr_ops: "
			    "BLOCKENABLE inum %x done\n", hdlp->ih_inum));
		}
		break;
	case DDI_INTROP_BLOCKDISABLE:
		DDI_INTR_NEXDBG((CE_CONT, "pci_common_intr_ops: "
		    "BLOCKDISABLE\n"));
		if (hdlp->ih_type != DDI_INTR_TYPE_MSI) {
			DDI_INTR_NEXDBG((CE_CONT, "BLOCKDISABLE: not MSI\n"));
			return (DDI_FAILURE);
		}

		count = hdlp->ih_scratch1;
		h_array = (ddi_intr_handle_t *)hdlp->ih_scratch2;
		for (i = 0; i < count; i++) {
			hdlp = (ddi_intr_handle_impl_t *)h_array[i];
			pci_disable_intr(pdip, rdip, hdlp, hdlp->ih_inum);
			DDI_INTR_NEXDBG((CE_CONT, "pci_common_intr_ops: "
			    "BLOCKDISABLE inum %x done\n", hdlp->ih_inum));
		}
		break;
	case DDI_INTROP_SETMASK:
	case DDI_INTROP_CLRMASK:
		/*
		 * First handle in the config space
		 */
		if (intr_op == DDI_INTROP_SETMASK) {
			if (DDI_INTR_IS_MSI_OR_MSIX(hdlp->ih_type))
				pci_status = pci_msi_set_mask(rdip,
				    hdlp->ih_type, hdlp->ih_inum);
			else if (hdlp->ih_type == DDI_INTR_TYPE_FIXED)
				pci_status = pci_intx_set_mask(rdip);
		} else {
			if (DDI_INTR_IS_MSI_OR_MSIX(hdlp->ih_type))
				pci_status = pci_msi_clr_mask(rdip,
				    hdlp->ih_type, hdlp->ih_inum);
			else if (hdlp->ih_type == DDI_INTR_TYPE_FIXED)
				pci_status = pci_intx_clr_mask(rdip);
		}

		/* For MSI/X; no need to check with pcplusmp */
		if (hdlp->ih_type != DDI_INTR_TYPE_FIXED)
			return (pci_status);

		/* For fixed interrupts only: handle config space first */
		if (hdlp->ih_type == DDI_INTR_TYPE_FIXED &&
		    pci_status == DDI_SUCCESS)
			break;

		/* For fixed interrupts only: confer with pcplusmp next */
		/* If interrupt is shared; do nothing */
		gic_rval = gic_intr_ops(rdip, hdlp,
		    GIC_INTR_OP_GET_SHARED, &gic_status);

		if (gic_rval == GIC_FAILURE || gic_status == 1)
			return (pci_status);

		/* Now, pcplusmp should try to set/clear the mask */
		if (intr_op == DDI_INTROP_SETMASK)
			gic_rval = gic_intr_ops(rdip, hdlp,
			    GIC_INTR_OP_SET_MASK, NULL);
		else
			gic_rval = gic_intr_ops(rdip, hdlp,
			    GIC_INTR_OP_CLEAR_MASK, NULL);

		return ((gic_rval == GIC_FAILURE) ? DDI_FAILURE : DDI_SUCCESS);
	case DDI_INTROP_GETPENDING:
		/*
		 * First check the config space and/or
		 * MSI capability register(s)
		 */
		if (DDI_INTR_IS_MSI_OR_MSIX(hdlp->ih_type))
			pci_rval = pci_msi_get_pending(rdip, hdlp->ih_type,
			    hdlp->ih_inum, &pci_status);
		else if (hdlp->ih_type == DDI_INTR_TYPE_FIXED)
			pci_rval = pci_intx_get_pending(rdip, &pci_status);

		/* On failure; next try with pcplusmp */
		if (pci_rval != DDI_SUCCESS)
			gic_rval = gic_intr_ops(rdip, hdlp,
			    GIC_INTR_OP_GET_PENDING, &gic_status);

		DDI_INTR_NEXDBG((CE_CONT, "pci: GETPENDING returned "
		    "gic_rval = %x, gic_status = %x, pci_rval = %x, "
		    "pci_status = %x\n", gic_rval, gic_status, pci_rval,
		    pci_status));
		if (gic_rval == GIC_FAILURE && pci_rval == DDI_FAILURE) {
			*(int *)result = 0;
			return (DDI_FAILURE);
		}

		if (gic_rval != GIC_FAILURE)
			*(int *)result = gic_status;
		else if (pci_rval != DDI_FAILURE)
			*(int *)result = pci_status;
		DDI_INTR_NEXDBG((CE_CONT, "pci: GETPENDING returned = %x\n",
		    *(int *)result));
		break;
	default:
		return (i_ddi_intr_ops(pdip, rdip, intr_op, hdlp, result));
	}

	return (DDI_SUCCESS);
}

int
pci_get_cpu_from_vecirq(int vecirq, boolean_t is_irq)
{
	/* not support */
	return (-1);
}


static int
pci_enable_intr(dev_info_t *pdip, dev_info_t *rdip,
    ddi_intr_handle_impl_t *hdlp, uint32_t inum)
{
	struct intrspec	*ispec;
	int		irq;
	ihdl_plat_t	*ihdl_plat_datap = (ihdl_plat_t *)hdlp->ih_private;

	DDI_INTR_NEXDBG((CE_CONT, "pci_enable_intr: hdlp %p inum %x\n",
	    (void *)hdlp, inum));

	/* Translate the interrupt if needed */
	ispec = (struct intrspec *)pci_intx_get_ispec(pdip, rdip, (int)inum);
	if (ispec == NULL)
		return (DDI_FAILURE);

	ihdl_plat_datap->ip_ispecp = ispec;

	/* Set interrupt number */
	if (strcmp(DEVI(rdip)->devi_name, "pciclass,0c0310") == 0) {
		irq = ispec->intrspec_vec = IRQ_USBHINTA;	/* OHCI */
	} else if (strcmp(DEVI(rdip)->devi_name, "pciclass,0c0320") == 0) {
		irq = ispec->intrspec_vec = IRQ_USBHINTB;	/* EHCI */
	} else {
		return (DDI_FAILURE);
	}

	DDI_INTR_NEXDBG((CE_CONT, "pci_enable_intr: priority=%x irq=%x\n",
	    hdlp->ih_pri, irq));

	/* Add the interrupt handler */
	if (!add_avintr((void *)hdlp, hdlp->ih_pri, hdlp->ih_cb_func,
	    DEVI(rdip)->devi_name, irq, hdlp->ih_cb_arg1,
	    hdlp->ih_cb_arg2, &ihdl_plat_datap->ip_ticks, rdip))
		return (DDI_FAILURE);

	/* Note this really is an irq. */
	hdlp->ih_vector = (ushort_t)irq;

	return (DDI_SUCCESS);
}


static void
pci_disable_intr(dev_info_t *pdip, dev_info_t *rdip,
    ddi_intr_handle_impl_t *hdlp, uint32_t inum)
{
	int		irq;
	struct intrspec	*ispec;
	ihdl_plat_t	*ihdl_plat_datap = (ihdl_plat_t *)hdlp->ih_private;

	DDI_INTR_NEXDBG((CE_CONT, "pci_disable_intr: \n"));
	ispec = (struct intrspec *)pci_intx_get_ispec(pdip, rdip, (int)inum);
	if (ispec == NULL)
		return;
	if (DDI_INTR_IS_MSI_OR_MSIX(hdlp->ih_type))
		ispec->intrspec_vec = inum;
	ihdl_plat_datap->ip_ispecp = ispec;

	irq = ispec->intrspec_vec;

	/* Disable the interrupt handler */
	rem_avintr((void *)hdlp, hdlp->ih_pri, hdlp->ih_cb_func, irq);
	ihdl_plat_datap->ip_ispecp = NULL;
}

/*
 * Miscellaneous library function
 */
int
pci_common_get_reg_prop(dev_info_t *dip, pci_regspec_t *pci_rp)
{
	int		i;
	int 		number;
	int		assigned_addr_len;
	uint_t		phys_hi = pci_rp->pci_phys_hi;
	pci_regspec_t	*assigned_addr;

	if (((phys_hi & PCI_REG_ADDR_M) == PCI_ADDR_CONFIG) ||
	    (phys_hi & PCI_RELOCAT_B))
		return (DDI_SUCCESS);

	/*
	 * the "reg" property specifies relocatable, get and interpret the
	 * "assigned-addresses" property.
	 */
	if (ddi_prop_lookup_int_array(DDI_DEV_T_ANY, dip, DDI_PROP_DONTPASS,
	    "assigned-addresses", (int **)&assigned_addr,
	    (uint_t *)&assigned_addr_len) != DDI_PROP_SUCCESS)
		return (DDI_FAILURE);

	/*
	 * Scan the "assigned-addresses" for one that matches the specified
	 * "reg" property entry.
	 */
	phys_hi &= PCI_CONF_ADDR_MASK;
	number = assigned_addr_len / (sizeof (pci_regspec_t) / sizeof (int));
	for (i = 0; i < number; i++) {
		if ((assigned_addr[i].pci_phys_hi & PCI_CONF_ADDR_MASK) ==
		    phys_hi) {
			pci_rp->pci_phys_mid = assigned_addr[i].pci_phys_mid;
			pci_rp->pci_phys_low = assigned_addr[i].pci_phys_low;
			ddi_prop_free(assigned_addr);
			return (DDI_SUCCESS);
		}
	}

	ddi_prop_free(assigned_addr);
	return (DDI_FAILURE);
}


int
pci_common_ctlops_poke(peekpoke_ctlops_t *in_args)
{
	size_t size = in_args->size;
	uintptr_t dev_addr = in_args->dev_addr;
	uintptr_t host_addr = in_args->host_addr;
	ddi_acc_impl_t *hp = (ddi_acc_impl_t *)in_args->handle;
	ddi_acc_hdl_t *hdlp = (ddi_acc_hdl_t *)in_args->handle;
	size_t repcount = in_args->repcount;
	uint_t flags = in_args->flags;
	int err = DDI_SUCCESS;

	/*
	 * if no handle then this is a poke. We have to return failure here
	 * as we have no way of knowing whether this is a MEM or IO space access
	 */
	if (in_args->handle == NULL)
		return (DDI_FAILURE);

	/*
	 * rest of this function is actually for cautious puts
	 */
	for (; repcount; repcount--) {
		if (hp->ahi_acc_attr == DDI_ACCATTR_CONFIG_SPACE) {
			switch (size) {
			case sizeof (uint8_t):
				pci_config_wr8(hp, (uint8_t *)dev_addr,
				    *(uint8_t *)host_addr);
				break;
			case sizeof (uint16_t):
				pci_config_wr16(hp, (uint16_t *)dev_addr,
				    *(uint16_t *)host_addr);
				break;
			case sizeof (uint32_t):
				pci_config_wr32(hp, (uint32_t *)dev_addr,
				    *(uint32_t *)host_addr);
				break;
			case sizeof (uint64_t):
				pci_config_wr64(hp, (uint64_t *)dev_addr,
				    *(uint64_t *)host_addr);
				break;
			default:
				err = DDI_FAILURE;
				break;
			}
		} else if (hp->ahi_acc_attr & DDI_ACCATTR_IO_SPACE) {
			if (hdlp->ah_acc.devacc_attr_endian_flags ==
			    DDI_STRUCTURE_BE_ACC) {
				switch (size) {
				case sizeof (uint8_t):
					i_ddi_vaddr_put8(hp,
					    (uint8_t *)dev_addr,
					    *(uint8_t *)host_addr);
					break;
				case sizeof (uint16_t):
					i_ddi_vaddr_swap_put16(hp,
					    (uint16_t *)dev_addr,
					    *(uint16_t *)host_addr);
					break;
				case sizeof (uint32_t):
					i_ddi_vaddr_swap_put32(hp,
					    (uint32_t *)dev_addr,
					    *(uint32_t *)host_addr);
					break;
				/*
				 * note the 64-bit case is a dummy
				 * function - so no need to swap
				 */
				case sizeof (uint64_t):
					i_ddi_vaddr_put64(hp,
					    (uint64_t *)dev_addr,
					    *(uint64_t *)host_addr);
					break;
				default:
					err = DDI_FAILURE;
					break;
				}
			} else {
				switch (size) {
				case sizeof (uint8_t):
					i_ddi_vaddr_put8(hp,
					    (uint8_t *)dev_addr,
					    *(uint8_t *)host_addr);
					break;
				case sizeof (uint16_t):
					i_ddi_vaddr_put16(hp,
					    (uint16_t *)dev_addr,
					    *(uint16_t *)host_addr);
					break;
				case sizeof (uint32_t):
					i_ddi_vaddr_put32(hp,
					    (uint32_t *)dev_addr,
					    *(uint32_t *)host_addr);
					break;
				case sizeof (uint64_t):
					i_ddi_vaddr_put64(hp,
					    (uint64_t *)dev_addr,
					    *(uint64_t *)host_addr);
					break;
				default:
					err = DDI_FAILURE;
					break;
				}
			}
		} else {
			if (hdlp->ah_acc.devacc_attr_endian_flags ==
			    DDI_STRUCTURE_BE_ACC) {
				switch (size) {
				case sizeof (uint8_t):
					*(uint8_t *)dev_addr =
					    *(uint8_t *)host_addr;
					break;
				case sizeof (uint16_t):
					*(uint16_t *)dev_addr =
					    ddi_swap16(*(uint16_t *)host_addr);
					break;
				case sizeof (uint32_t):
					*(uint32_t *)dev_addr =
					    ddi_swap32(*(uint32_t *)host_addr);
					break;
				case sizeof (uint64_t):
					*(uint64_t *)dev_addr =
					    ddi_swap64(*(uint64_t *)host_addr);
					break;
				default:
					err = DDI_FAILURE;
					break;
				}
			} else {
				switch (size) {
				case sizeof (uint8_t):
					*(uint8_t *)dev_addr =
					    *(uint8_t *)host_addr;
					break;
				case sizeof (uint16_t):
					*(uint16_t *)dev_addr =
					    *(uint16_t *)host_addr;
					break;
				case sizeof (uint32_t):
					*(uint32_t *)dev_addr =
					    *(uint32_t *)host_addr;
					break;
				case sizeof (uint64_t):
					*(uint64_t *)dev_addr =
					    *(uint64_t *)host_addr;
					break;
				default:
					err = DDI_FAILURE;
					break;
				}
			}
		}
		host_addr += size;
		if (flags == DDI_DEV_AUTOINCR)
			dev_addr += size;
	}
	return (err);
}


int
pci_fm_acc_setup(ddi_acc_hdl_t *hp, off_t offset, off_t len)
{
	ddi_acc_impl_t	*ap = (ddi_acc_impl_t *)hp->ah_platform_private;

	/* endian-ness check */
	if (hp->ah_acc.devacc_attr_endian_flags == DDI_STRUCTURE_BE_ACC)
		return (DDI_FAILURE);

	/*
	 * range check
	 */
	if ((offset >= PCI_CONF_HDR_SIZE) ||
	    (len > PCI_CONF_HDR_SIZE) ||
	    (offset + len > PCI_CONF_HDR_SIZE))
		return (DDI_FAILURE);

	ap->ahi_acc_attr |= DDI_ACCATTR_CONFIG_SPACE;
	/*
	 * always use cautious mechanism for config space gets
	 */
	ap->ahi_get8 = i_ddi_caut_get8;
	ap->ahi_get16 = i_ddi_caut_get16;
	ap->ahi_get32 = i_ddi_caut_get32;
	ap->ahi_get64 = i_ddi_caut_get64;
	ap->ahi_rep_get8 = i_ddi_caut_rep_get8;
	ap->ahi_rep_get16 = i_ddi_caut_rep_get16;
	ap->ahi_rep_get32 = i_ddi_caut_rep_get32;
	ap->ahi_rep_get64 = i_ddi_caut_rep_get64;
	if (hp->ah_acc.devacc_attr_access == DDI_CAUTIOUS_ACC) {
		ap->ahi_put8 = i_ddi_caut_put8;
		ap->ahi_put16 = i_ddi_caut_put16;
		ap->ahi_put32 = i_ddi_caut_put32;
		ap->ahi_put64 = i_ddi_caut_put64;
		ap->ahi_rep_put8 = i_ddi_caut_rep_put8;
		ap->ahi_rep_put16 = i_ddi_caut_rep_put16;
		ap->ahi_rep_put32 = i_ddi_caut_rep_put32;
		ap->ahi_rep_put64 = i_ddi_caut_rep_put64;
	} else {
		ap->ahi_put8 = pci_config_wr8;
		ap->ahi_put16 = pci_config_wr16;
		ap->ahi_put32 = pci_config_wr32;
		ap->ahi_put64 = pci_config_wr64;
		ap->ahi_rep_put8 = pci_config_rep_wr8;
		ap->ahi_rep_put16 = pci_config_rep_wr16;
		ap->ahi_rep_put32 = pci_config_rep_wr32;
		ap->ahi_rep_put64 = pci_config_rep_wr64;
	}

	/* Initialize to default check/notify functions */
	ap->ahi_fault_check = i_ddi_acc_fault_check;
	ap->ahi_fault_notify = i_ddi_acc_fault_notify;
	ap->ahi_fault = 0;
	impl_acc_err_init(hp);
	return (DDI_SUCCESS);
}


int
pci_common_ctlops_peek(peekpoke_ctlops_t *in_args)
{
	size_t size = in_args->size;
	uintptr_t dev_addr = in_args->dev_addr;
	uintptr_t host_addr = in_args->host_addr;
	ddi_acc_impl_t *hp = (ddi_acc_impl_t *)in_args->handle;
	ddi_acc_hdl_t *hdlp = (ddi_acc_hdl_t *)in_args->handle;
	size_t repcount = in_args->repcount;
	uint_t flags = in_args->flags;
	int err = DDI_SUCCESS;

	/*
	 * if no handle then this is a peek. We have to return failure here
	 * as we have no way of knowing whether this is a MEM or IO space access
	 */
	if (in_args->handle == NULL)
		return (DDI_FAILURE);

	for (; repcount; repcount--) {
		if (hp->ahi_acc_attr == DDI_ACCATTR_CONFIG_SPACE) {
			switch (size) {
			case sizeof (uint8_t):
				*(uint8_t *)host_addr = pci_config_rd8(hp,
				    (uint8_t *)dev_addr);
				break;
			case sizeof (uint16_t):
				*(uint16_t *)host_addr = pci_config_rd16(hp,
				    (uint16_t *)dev_addr);
				break;
			case sizeof (uint32_t):
				*(uint32_t *)host_addr = pci_config_rd32(hp,
				    (uint32_t *)dev_addr);
				break;
			case sizeof (uint64_t):
				*(uint64_t *)host_addr = pci_config_rd64(hp,
				    (uint64_t *)dev_addr);
				break;
			default:
				err = DDI_FAILURE;
				break;
			}
		} else if (hp->ahi_acc_attr & DDI_ACCATTR_IO_SPACE) {
			if (hdlp->ah_acc.devacc_attr_endian_flags ==
			    DDI_STRUCTURE_BE_ACC) {
				switch (size) {
				case sizeof (uint8_t):
					*(uint8_t *)host_addr =
					    i_ddi_vaddr_get8(hp,
					    (uint8_t *)dev_addr);
					break;
				case sizeof (uint16_t):
					*(uint16_t *)host_addr =
					    i_ddi_vaddr_swap_get16(hp,
					    (uint16_t *)dev_addr);
					break;
				case sizeof (uint32_t):
					*(uint32_t *)host_addr =
					    i_ddi_vaddr_swap_get32(hp,
					    (uint32_t *)dev_addr);
					break;
				/*
				 * note the 64-bit case is a dummy
				 * function - so no need to swap
				 */
				case sizeof (uint64_t):
					*(uint64_t *)host_addr =
					    i_ddi_vaddr_get64(hp,
					    (uint64_t *)dev_addr);
					break;
				default:
					err = DDI_FAILURE;
					break;
				}
			} else {
				switch (size) {
				case sizeof (uint8_t):
					*(uint8_t *)host_addr =
					    i_ddi_vaddr_get8(hp,
					    (uint8_t *)dev_addr);
					break;
				case sizeof (uint16_t):
					*(uint16_t *)host_addr =
					    i_ddi_vaddr_get16(hp,
					    (uint16_t *)dev_addr);
					break;
				case sizeof (uint32_t):
					*(uint32_t *)host_addr =
					    i_ddi_vaddr_get32(hp,
					    (uint32_t *)dev_addr);
					break;
				case sizeof (uint64_t):
					*(uint64_t *)host_addr =
					    i_ddi_vaddr_get64(hp,
					    (uint64_t *)dev_addr);
					break;
				default:
					err = DDI_FAILURE;
					break;
				}
			}
		} else {
			if (hdlp->ah_acc.devacc_attr_endian_flags ==
			    DDI_STRUCTURE_BE_ACC) {
				switch (in_args->size) {
				case sizeof (uint8_t):
					*(uint8_t *)host_addr =
					    *(uint8_t *)dev_addr;
					break;
				case sizeof (uint16_t):
					*(uint16_t *)host_addr =
					    ddi_swap16(*(uint16_t *)dev_addr);
					break;
				case sizeof (uint32_t):
					*(uint32_t *)host_addr =
					    ddi_swap32(*(uint32_t *)dev_addr);
					break;
				case sizeof (uint64_t):
					*(uint64_t *)host_addr =
					    ddi_swap64(*(uint64_t *)dev_addr);
					break;
				default:
					err = DDI_FAILURE;
					break;
				}
			} else {
				switch (in_args->size) {
				case sizeof (uint8_t):
					*(uint8_t *)host_addr =
					    *(uint8_t *)dev_addr;
					break;
				case sizeof (uint16_t):
					*(uint16_t *)host_addr =
					    *(uint16_t *)dev_addr;
					break;
				case sizeof (uint32_t):
					*(uint32_t *)host_addr =
					    *(uint32_t *)dev_addr;
					break;
				case sizeof (uint64_t):
					*(uint64_t *)host_addr =
					    *(uint64_t *)dev_addr;
					break;
				default:
					err = DDI_FAILURE;
					break;
				}
			}
		}
		host_addr += size;
		if (flags == DDI_DEV_AUTOINCR)
			dev_addr += size;
	}
	return (err);
}

/*ARGSUSED*/
int
pci_common_peekpoke(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t ctlop, void *arg, void *result)
{
	if (ctlop == DDI_CTLOPS_PEEK)
		return (pci_common_ctlops_peek((peekpoke_ctlops_t *)arg));
	else
		return (pci_common_ctlops_poke((peekpoke_ctlops_t *)arg));
}

/*
 * These are the get and put functions to be shared with drivers. The
 * mutex locking is done inside the functions referenced, rather than
 * here, and is thus shared across PCI child drivers and any other
 * consumers of PCI config space (such as the ACPI subsystem).
 *
 * The configuration space addresses come in as pointers.  This is fine on
 * a 32-bit system, where the VM space and configuration space are the same
 * size.  It's not such a good idea on a 64-bit system, where memory
 * addresses are twice as large as configuration space addresses.  At some
 * point in the call tree we need to take a stand and say "you are 32-bit
 * from this time forth", and this seems like a nice self-contained place.
 */

uint8_t
pci_config_rd8(ddi_acc_impl_t *hdlp, uint8_t *addr)
{
	pci_acc_cfblk_t *cfp;
	uint8_t	rval;
	int reg;

	ASSERT64(((uintptr_t)addr >> 32) == 0);

	reg = (int)(uintptr_t)addr;

	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;

	rval = (*pci_getb_func)(cfp->c_busnum, cfp->c_devnum, cfp->c_funcnum,
	    reg);

	return (rval);
}

void
pci_config_rep_rd8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags)
{
	uint8_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*h++ = pci_config_rd8(hdlp, d++);
	else
		for (; repcount; repcount--)
			*h++ = pci_config_rd8(hdlp, d);
}

uint16_t
pci_config_rd16(ddi_acc_impl_t *hdlp, uint16_t *addr)
{
	pci_acc_cfblk_t *cfp;
	uint16_t rval;
	int reg;

	ASSERT64(((uintptr_t)addr >> 32) == 0);

	reg = (int)(uintptr_t)addr;

	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;

	rval = (*pci_getw_func)(cfp->c_busnum, cfp->c_devnum, cfp->c_funcnum,
	    reg);

	return (rval);
}

void
pci_config_rep_rd16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	uint16_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*h++ = pci_config_rd16(hdlp, d++);
	else
		for (; repcount; repcount--)
			*h++ = pci_config_rd16(hdlp, d);
}

uint32_t
pci_config_rd32(ddi_acc_impl_t *hdlp, uint32_t *addr)
{
	pci_acc_cfblk_t *cfp;
	uint32_t rval;
	int reg;

	ASSERT64(((uintptr_t)addr >> 32) == 0);

	reg = (int)(uintptr_t)addr;

	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;

	rval = (*pci_getl_func)(cfp->c_busnum, cfp->c_devnum,
	    cfp->c_funcnum, reg);

	return (rval);
}

void
pci_config_rep_rd32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			*h++ = pci_config_rd32(hdlp, d++);
	else
		for (; repcount; repcount--)
			*h++ = pci_config_rd32(hdlp, d);
}


void
pci_config_wr8(ddi_acc_impl_t *hdlp, uint8_t *addr, uint8_t value)
{
	pci_acc_cfblk_t *cfp;
	int reg;

	ASSERT64(((uintptr_t)addr >> 32) == 0);

	reg = (int)(uintptr_t)addr;

	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;

	(*pci_putb_func)(cfp->c_busnum, cfp->c_devnum,
	    cfp->c_funcnum, reg, value);
}

void
pci_config_rep_wr8(ddi_acc_impl_t *hdlp, uint8_t *host_addr,
	uint8_t *dev_addr, size_t repcount, uint_t flags)
{
	uint8_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			pci_config_wr8(hdlp, d++, *h++);
	else
		for (; repcount; repcount--)
			pci_config_wr8(hdlp, d, *h++);
}

void
pci_config_wr16(ddi_acc_impl_t *hdlp, uint16_t *addr, uint16_t value)
{
	pci_acc_cfblk_t *cfp;
	int reg;

	ASSERT64(((uintptr_t)addr >> 32) == 0);

	reg = (int)(uintptr_t)addr;

	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;

	(*pci_putw_func)(cfp->c_busnum, cfp->c_devnum,
	    cfp->c_funcnum, reg, value);
}

void
pci_config_rep_wr16(ddi_acc_impl_t *hdlp, uint16_t *host_addr,
	uint16_t *dev_addr, size_t repcount, uint_t flags)
{
	uint16_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			pci_config_wr16(hdlp, d++, *h++);
	else
		for (; repcount; repcount--)
			pci_config_wr16(hdlp, d, *h++);
}

void
pci_config_wr32(ddi_acc_impl_t *hdlp, uint32_t *addr, uint32_t value)
{
	pci_acc_cfblk_t *cfp;
	int reg;

	ASSERT64(((uintptr_t)addr >> 32) == 0);

	reg = (int)(uintptr_t)addr;

	cfp = (pci_acc_cfblk_t *)&hdlp->ahi_common.ah_bus_private;

	(*pci_putl_func)(cfp->c_busnum, cfp->c_devnum,
	    cfp->c_funcnum, reg, value);
}

void
pci_config_rep_wr32(ddi_acc_impl_t *hdlp, uint32_t *host_addr,
	uint32_t *dev_addr, size_t repcount, uint_t flags)
{
	uint32_t *h, *d;

	h = host_addr;
	d = dev_addr;

	if (flags == DDI_DEV_AUTOINCR)
		for (; repcount; repcount--)
			pci_config_wr32(hdlp, d++, *h++);
	else
		for (; repcount; repcount--)
			pci_config_wr32(hdlp, d, *h++);
}

uint64_t
pci_config_rd64(ddi_acc_impl_t *hdlp, uint64_t *addr)
{
	uint32_t lw_val;
	uint32_t hi_val;
	uint32_t *dp;
	uint64_t val;

	dp = (uint32_t *)addr;
	lw_val = pci_config_rd32(hdlp, dp);
	dp++;
	hi_val = pci_config_rd32(hdlp, dp);
	val = ((uint64_t)hi_val << 32) | lw_val;
	return (val);
}

void
pci_config_wr64(ddi_acc_impl_t *hdlp, uint64_t *addr, uint64_t value)
{
	uint32_t lw_val;
	uint32_t hi_val;
	uint32_t *dp;

	dp = (uint32_t *)addr;
	lw_val = (uint32_t)(value & 0xffffffff);
	hi_val = (uint32_t)(value >> 32);
	pci_config_wr32(hdlp, dp, lw_val);
	dp++;
	pci_config_wr32(hdlp, dp, hi_val);
}

void
pci_config_rep_rd64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--)
			*host_addr++ = pci_config_rd64(hdlp, dev_addr++);
	} else {
		for (; repcount; repcount--)
			*host_addr++ = pci_config_rd64(hdlp, dev_addr);
	}
}

void
pci_config_rep_wr64(ddi_acc_impl_t *hdlp, uint64_t *host_addr,
	uint64_t *dev_addr, size_t repcount, uint_t flags)
{
	if (flags == DDI_DEV_AUTOINCR) {
		for (; repcount; repcount--)
			pci_config_wr64(hdlp, host_addr++, *dev_addr++);
	} else {
		for (; repcount; repcount--)
			pci_config_wr64(hdlp, host_addr++, *dev_addr);
	}
}

