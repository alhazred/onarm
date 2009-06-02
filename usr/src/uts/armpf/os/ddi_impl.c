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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2006-2009 NEC Corporation
 */

#pragma ident	"@(#)ddi_impl.c	1.146	06/08/16 SMI"

/*
 * ARM platform specific DDI implementation
 */
#include <sys/types.h>
#include <sys/autoconf.h>
#include <sys/avintr.h>
#include <sys/bootconf.h>
#include <sys/conf.h>
#include <sys/cpuvar.h>
#include <sys/ddi_impldefs.h>
#include <sys/ddi_implfuncs.h>
#include <sys/ddi_subrdefs.h>
#include <sys/ethernet.h>
#include <sys/fp.h>
#include <sys/instance.h>
#include <sys/kmem.h>
#include <sys/machsystm.h>
#include <sys/modctl.h>
#include <sys/promif.h>
#include <sys/prom_plat.h>
#include <sys/sunndi.h>
#include <sys/ndi_impldefs.h>
#include <sys/sysmacros.h>
#include <sys/systeminfo.h>
#include <sys/utsname.h>
#include <sys/atomic.h>
#include <sys/spl.h>
#include <sys/archsystm.h>
#include <vm/seg_kmem.h>
#include <sys/ontrap.h>
#include <sys/fm/protocol.h>
#include <sys/ramdisk.h>
#include <sys/sunndi.h>
#include <sys/vmem.h>
#include <sys/pci_impl_arm.h>
#include <sys/mach_intr.h>
#include <sys/platform.h>
#include <vm/hat_arm.h>
#include <sys/mach_dma.h>
#include <sys/xramdev_impl.h>
#include <sys/builtin.h>

/*
 * DDI Boot Configuration
 */

/* The following modules MUST be present for the system to boot. */
char *impl_module_list[] = {
	"rootnex",
	"options",
	"pseudo",
	"clone",
	(char *)0
};

/*
 * No platform drivers on this platform
 */
char *platform_module_list[] = {
	(char *)0
};

/* pci bus resource maps */
struct pci_bus_resource *pci_bus_res;

/*
 * Forward declarations
 */
static int getlongprop_buf();
static void get_boot_properties(void);
static void impl_bus_initialprobe(void);
static void impl_bus_reprobe(void);

static int poke_mem(peekpoke_ctlops_t *in_args);
static int peek_mem(peekpoke_ctlops_t *in_args);

#ifdef	ARMPF_PLAT_BUILTIN_DEVINIT
/*
 * Platform specific built-in device initialization must be done.
 */
extern void	builtin_device_init(void);
#else	/* !ARMPF_PLAT_BUILTIN_DEVINIT */
static void	builtin_device_init(void);
#endif	/* ARMPF_PLAT_BUILTIN_DEVINIT */

static void
check_driver_disable(void)
{
	int proplen = 128;
	char *prop_name;
	char *drv_name, *propval;
	major_t major;

	prop_name = kmem_alloc(proplen, KM_SLEEP);
	for (major = 0; major < devcnt; major++) {
		drv_name = ddi_major_to_name(major);
		if (drv_name == NULL)
			continue;
		(void) snprintf(prop_name, proplen, "disable-%s", drv_name);
		if (ddi_prop_lookup_string(DDI_DEV_T_ANY, ddi_root_node(),
		    DDI_PROP_DONTPASS, prop_name, &propval) == DDI_SUCCESS) {
			if (strcmp(propval, "true") == 0) {
				devnamesp[major].dn_flags |= DN_DRIVER_REMOVED;
				cmn_err(CE_NOTE, "driver %s disabled",
				    drv_name);
			}
			ddi_prop_free(propval);
		}
	}
	kmem_free(prop_name, proplen);
}


/*
 * Configure the hardware on the system.
 * Called before the rootfs is mounted
 */
void
configure(void)
{
	extern void i_ddi_init_root();

	/*
	 * Determine if an FPU is attached
	 */
	fpu_probe();

	if (!fpu_exists) {
		printf("No VFP hardware found\n");
	}

	/*
	 * Initialize devices on the machine.
	 * Uses configuration tree built by the PROMs to determine what
	 * is present, and builds a tree of prototype dev_info nodes
	 * corresponding to the hardware which identified itself.
	 */
#if !defined(SAS) && !defined(MPSAS)
	/*
	 * Check for disabled drivers and initialize root node.
	 */
	check_driver_disable();
	i_ddi_init_root();

	/* reprogram devices not set up by firmware (BIOS) */
	impl_bus_reprobe();
#endif	/* !SAS && !MPSAS */
}

/*
 * The "status" property indicates the operational status of a device.
 * If this property is present, the value is a string indicating the
 * status of the device as follows:
 *
 *	"okay"		operational.
 *	"disabled"	not operational, but might become operational.
 *	"fail"		not operational because a fault has been detected,
 *			and it is unlikely that the device will become
 *			operational without repair. no additional details
 *			are available.
 *	"fail-xxx"	not operational because a fault has been detected,
 *			and it is unlikely that the device will become
 *			operational without repair. "xxx" is additional
 *			human-readable information about the particular
 *			fault condition that was detected.
 *
 * The absence of this property means that the operational status is
 * unknown or okay.
 *
 * This routine checks the status property of the specified device node
 * and returns 0 if the operational status indicates failure, and 1 otherwise.
 *
 * The property may exist on plug-in cards the existed before IEEE 1275-1994.
 * And, in that case, the property may not even be a string. So we carefully
 * check for the value "fail", in the beginning of the string, noting
 * the property length.
 */
int
status_okay(int id, char *buf, int buflen)
{
	char status_buf[OBP_MAXPROPNAME];
	char *bufp = buf;
	int len = buflen;
	int proplen;
	static const char *status = "status";
	static const char *fail = "fail";
	int fail_len = (int)strlen(fail);

	/*
	 * Get the proplen ... if it's smaller than "fail",
	 * or doesn't exist ... then we don't care, since
	 * the value can't begin with the char string "fail".
	 *
	 * NB: proplen, if it's a string, includes the NULL in the
	 * the size of the property, and fail_len does not.
	 */
	proplen = prom_getproplen((pnode_t)id, (caddr_t)status);
	if (proplen <= fail_len)	/* nonexistant or uninteresting len */
		return (1);

	/*
	 * if a buffer was provided, use it
	 */
	if ((buf == (char *)NULL) || (buflen <= 0)) {
		bufp = status_buf;
		len = sizeof (status_buf);
	}
	*bufp = (char)0;

	/*
	 * Get the property into the buffer, to the extent of the buffer,
	 * and in case the buffer is smaller than the property size,
	 * NULL terminate the buffer. (This handles the case where
	 * a buffer was passed in and the caller wants to print the
	 * value, but the buffer was too small).
	 */
	(void) prom_bounded_getprop((pnode_t)id, (caddr_t)status,
	    (caddr_t)bufp, len);
	*(bufp + len - 1) = (char)0;

	/*
	 * If the value begins with the char string "fail",
	 * then it means the node is failed. We don't care
	 * about any other values. We assume the node is ok
	 * although it might be 'disabled'.
	 */
	if (strncmp(bufp, fail, fail_len) == 0)
		return (0);

	return (1);
}

/*
 * Check the status of the device node passed as an argument.
 *
 *	if ((status is OKAY) || (status is DISABLED))
 *		return DDI_SUCCESS
 *	else
 *		print a warning and return DDI_FAILURE
 */
/*ARGSUSED1*/
int
check_status(int id, char *name, dev_info_t *parent)
{
	char status_buf[64];
	char devtype_buf[OBP_MAXPROPNAME];
	int retval = DDI_FAILURE;

	/*
	 * is the status okay?
	 */
	if (status_okay(id, status_buf, sizeof (status_buf)))
		return (DDI_SUCCESS);

	/*
	 * a status property indicating bad memory will be associated
	 * with a node which has a "device_type" property with a value of
	 * "memory-controller". in this situation, return DDI_SUCCESS
	 */
	if (getlongprop_buf(id, OBP_DEVICETYPE, devtype_buf,
	    sizeof (devtype_buf)) > 0) {
		if (strcmp(devtype_buf, "memory-controller") == 0)
			retval = DDI_SUCCESS;
	}

	/*
	 * print the status property information
	 */
	cmn_err(CE_WARN, "status '%s' for '%s'", status_buf, name);
	return (retval);
}

/*ARGSUSED*/
uint_t
softlevel1(caddr_t arg1, caddr_t arg2)
{
	softint();
	return (1);
}

/*
 * Allow for implementation specific correction of PROM property values.
 */

/*ARGSUSED*/
void
impl_fix_props(dev_info_t *dip, dev_info_t *ch_dip, char *name, int len,
    caddr_t buffer)
{
	/*
	 * There are no adjustments needed in this implementation.
	 */
}

static int
getlongprop_buf(int id, char *name, char *buf, int maxlen)
{
	int size;

	size = prom_getproplen((pnode_t)id, name);
	if (size <= 0 || (size > maxlen - 1))
		return (-1);

	if (-1 == prom_getprop((pnode_t)id, name, buf))
		return (-1);

	if (strcmp("name", name) == 0) {
		if (buf[size - 1] != '\0') {
			buf[size] = '\0';
			size += 1;
		}
	}

	return (size);
}

static int
get_prop_int_array(dev_info_t *di, char *pname, int **pval, uint_t *plen)
{
	int ret;

	if ((ret = ddi_prop_lookup_int_array(DDI_DEV_T_ANY, di,
	    DDI_PROP_DONTPASS, pname, pval, plen))
	    == DDI_PROP_SUCCESS) {
		*plen = (*plen) * (sizeof (int));
	}
	return (ret);
}


/*
 * Node Configuration
 */

struct prop_ispec {
	uint_t	pri, vec;
};

/*
 * we're prepared to claim that the interrupt string
 * is in the form of a list of <ipl,vec> specifications.
 */

#define	VEC_MIN	1
#define	VEC_MAX	255

static int
impl_xlate_intrs(dev_info_t *child, int *in,
    struct ddi_parent_private_data *pdptr)
{
	size_t size;
	int n;
	struct intrspec *new;
	caddr_t got_prop;
	int *inpri;
	int got_len;
	extern int ignore_hardware_nodes;	/* force flag from ddi_impl.c */

	static char bad_intr_fmt[] =
	    "bad interrupt spec from %s%d - ipl %d, irq %d\n";

	/*
	 * determine if the driver is expecting the new style "interrupts"
	 * property which just contains the IRQ, or the old style which
	 * contains pairs of <IPL,IRQ>.  if it is the new style, we always
	 * assign IPL 5 unless an "interrupt-priorities" property exists.
	 * in that case, the "interrupt-priorities" property contains the
	 * IPL values that match, one for one, the IRQ values in the
	 * "interrupts" property.
	 */
	inpri = NULL;
	if ((ddi_getprop(DDI_DEV_T_ANY, child, DDI_PROP_DONTPASS,
	    "ignore-hardware-nodes", -1) != -1) || ignore_hardware_nodes) {
		/* the old style "interrupts" property... */

		/*
		 * The list consists of <ipl,vec> elements
		 */
		if ((n = (*in++ >> 1)) < 1)
			return (DDI_FAILURE);

		pdptr->par_nintr = n;
		size = n * sizeof (struct intrspec);
		new = pdptr->par_intr = kmem_zalloc(size, KM_SLEEP);

		while (n--) {
			int level = *in++;
			int vec = *in++;

			if (level < 1 || level > MAXIPL ||
			    vec < VEC_MIN || vec > VEC_MAX) {
				cmn_err(CE_CONT, bad_intr_fmt,
				    DEVI(child)->devi_name,
				    DEVI(child)->devi_instance, level, vec);
				goto broken;
			}
			new->intrspec_pri = level;
			new->intrspec_vec = vec;
			new++;
		}

		return (DDI_SUCCESS);
	} else {
		/* the new style "interrupts" property... */

		/*
		 * The list consists of <vec> elements
		 */
		if ((n = (*in++)) < 1)
			return (DDI_FAILURE);

		pdptr->par_nintr = n;
		size = n * sizeof (struct intrspec);
		new = pdptr->par_intr = kmem_zalloc(size, KM_SLEEP);

		/* XXX check for "interrupt-priorities" property... */
		if (ddi_getlongprop(DDI_DEV_T_ANY, child, DDI_PROP_DONTPASS,
		    "interrupt-priorities", (caddr_t)&got_prop, &got_len)
		    == DDI_PROP_SUCCESS) {
			if (n != (got_len / sizeof (int))) {
				cmn_err(CE_CONT,
				    "bad interrupt-priorities length"
				    " from %s%d: expected %d, got %d\n",
				    DEVI(child)->devi_name,
				    DEVI(child)->devi_instance, n,
				    (int)(got_len / sizeof (int)));
				goto broken;
			}
			inpri = (int *)got_prop;
		}

		while (n--) {
			int level;
			int vec = *in++;

			if (inpri == NULL)
				level = 5;
			else
				level = *inpri++;

			if (level < 1 || level > MAXIPL ||
			    vec < VEC_MIN || vec > VEC_MAX) {
				cmn_err(CE_CONT, bad_intr_fmt,
				    DEVI(child)->devi_name,
				    DEVI(child)->devi_instance, level, vec);
				goto broken;
			}
			new->intrspec_pri = level;
			new->intrspec_vec = vec;
			new++;
		}

		if (inpri != NULL)
			kmem_free(got_prop, got_len);
		return (DDI_SUCCESS);
	}

broken:
	kmem_free(pdptr->par_intr, size);
	pdptr->par_intr = NULL;
	pdptr->par_nintr = 0;
	if (inpri != NULL)
		kmem_free(got_prop, got_len);

	return (DDI_FAILURE);
}

/*
 * Create a ddi_parent_private_data structure from the ddi properties of
 * the dev_info node.
 *
 * The "reg" and either an "intr" or "interrupts" properties are required
 * if the driver wishes to create mappings or field interrupts on behalf
 * of the device.
 *
 * The "reg" property is assumed to be a list of at least one triple
 *
 *	<bustype, address, size>*1
 *
 * The "intr" property is assumed to be a list of at least one duple
 *
 *	<SPARC ipl, vector#>*1
 *
 * The "interrupts" property is assumed to be a list of at least one
 * n-tuples that describes the interrupt capabilities of the bus the device
 * is connected to.  For SBus, this looks like
 *
 *	<SBus-level>*1
 *
 * (This property obsoletes the 'intr' property).
 *
 * The "ranges" property is optional.
 */
void
make_ddi_ppd(dev_info_t *child, struct ddi_parent_private_data **ppd)
{
	struct ddi_parent_private_data *pdptr;
	int n;
	int *reg_prop, *rng_prop, *intr_prop, *irupts_prop;
	uint_t reg_len, rng_len, intr_len, irupts_len;

	*ppd = pdptr = kmem_zalloc(sizeof (*pdptr), KM_SLEEP);

	/*
	 * Handle the 'reg' property.
	 */
	if ((get_prop_int_array(child, "reg", &reg_prop, &reg_len) ==
	    DDI_PROP_SUCCESS) && (reg_len != 0)) {
		pdptr->par_nreg = reg_len / (int)sizeof (struct regspec);
		pdptr->par_reg = (struct regspec *)reg_prop;
	}

	/*
	 * See if I have a range (adding one where needed - this
	 * means to add one for sbus node in sun4c, when romvec > 0,
	 * if no range is already defined in the PROM node.
	 * (Currently no sun4c PROMS define range properties,
	 * but they should and may in the future.)  For the SBus
	 * node, the range is defined by the SBus reg property.
	 */
	if (get_prop_int_array(child, "ranges", &rng_prop, &rng_len)
	    == DDI_PROP_SUCCESS) {
		pdptr->par_nrng = rng_len / (int)(sizeof (struct rangespec));
		pdptr->par_rng = (struct rangespec *)rng_prop;
	}

	/*
	 * Handle the 'intr' and 'interrupts' properties
	 */

	/*
	 * For backwards compatibility
	 * we first look for the 'intr' property for the device.
	 */
	if (get_prop_int_array(child, "intr", &intr_prop, &intr_len)
	    != DDI_PROP_SUCCESS) {
		intr_len = 0;
	}

	/*
	 * If we're to support bus adapters and future platforms cleanly,
	 * we need to support the generalized 'interrupts' property.
	 */
	if (get_prop_int_array(child, "interrupts", &irupts_prop,
	    &irupts_len) != DDI_PROP_SUCCESS) {
		irupts_len = 0;
	} else if (intr_len != 0) {
		/*
		 * If both 'intr' and 'interrupts' are defined,
		 * then 'interrupts' wins and we toss the 'intr' away.
		 */
		ddi_prop_free((void *)intr_prop);
		intr_len = 0;
	}

	if (intr_len != 0) {

		/*
		 * Translate the 'intr' property into an array
		 * an array of struct intrspec's.  There's not really
		 * very much to do here except copy what's out there.
		 */

		struct intrspec *new;
		struct prop_ispec *l;

		n = pdptr->par_nintr =
			intr_len / sizeof (struct prop_ispec);
		l = (struct prop_ispec *)intr_prop;
		pdptr->par_intr =
		    new = kmem_zalloc(n * sizeof (struct intrspec), KM_SLEEP);
		while (n--) {
			new->intrspec_pri = l->pri;
			new->intrspec_vec = l->vec;
			new++;
			l++;
		}
		ddi_prop_free((void *)intr_prop);

	} else if ((n = irupts_len) != 0) {
		size_t size;
		int *out;

		/*
		 * Translate the 'interrupts' property into an array
		 * of intrspecs for the rest of the DDI framework to
		 * toy with.  Only our ancestors really know how to
		 * do this, so ask 'em.  We massage the 'interrupts'
		 * property so that it is pre-pended by a count of
		 * the number of integers in the argument.
		 */
		size = sizeof (int) + n;
		out = kmem_alloc(size, KM_SLEEP);
		*out = n / sizeof (int);
		bcopy(irupts_prop, out + 1, (size_t)n);
		ddi_prop_free((void *)irupts_prop);
		if (impl_xlate_intrs(child, out, pdptr) != DDI_SUCCESS) {
			cmn_err(CE_CONT,
			    "Unable to translate 'interrupts' for %s%d\n",
			    DEVI(child)->devi_binding_name,
			    DEVI(child)->devi_instance);
		}
		kmem_free(out, size);
	}
}

/*
 * Name a child
 */
static int
impl_sunbus_name_child(dev_info_t *child, char *name, int namelen)
{
	/*
	 * Fill in parent-private data and this function returns to us
	 * an indication if it used "registers" to fill in the data.
	 */
	if (ddi_get_parent_data(child) == NULL) {
		struct ddi_parent_private_data *pdptr;
		make_ddi_ppd(child, &pdptr);
		ddi_set_parent_data(child, pdptr);
	}

	name[0] = '\0';
	if (DEVI_PD(child)->par_nreg > 0) {
		(void) snprintf(name, namelen, "%x,%x",
		    (&DEVI_PD(child)->par_reg[0])->regspec_bustype,
		    (&DEVI_PD(child)->par_reg[0])->regspec_addr);
	}

	return (DDI_SUCCESS);
}

/*
 * Called from the bus_ctl op of sunbus (sbus, obio, etc) nexus drivers
 * to implement the DDI_CTLOPS_INITCHILD operation.  That is, it names
 * the children of sun busses based on the reg spec.
 *
 * Handles the following properties (in make_ddi_ppd):
 *	Property		value
 *	  Name			type
 *	reg		register spec
 *	intr		old-form interrupt spec
 *	interrupts	new (bus-oriented) interrupt spec
 *	ranges		range spec
 */
int
impl_ddi_sunbus_initchild(dev_info_t *child)
{
	char name[MAXNAMELEN];
	void impl_ddi_sunbus_removechild(dev_info_t *);

	/*
	 * Name the child, also makes parent private data
	 */
	(void) impl_sunbus_name_child(child, name, MAXNAMELEN);
	ddi_set_name_addr(child, name);

	/*
	 * Attempt to merge a .conf node; if successful, remove the
	 * .conf node.
	 */
	if ((ndi_dev_is_persistent_node(child) == 0) &&
	    (ndi_merge_node(child, impl_sunbus_name_child) == DDI_SUCCESS)) {
		/*
		 * Return failure to remove node
		 */
		impl_ddi_sunbus_removechild(child);
		return (DDI_FAILURE);
	}
	return (DDI_SUCCESS);
}

void
impl_free_ddi_ppd(dev_info_t *dip)
{
	struct ddi_parent_private_data *pdptr;
	size_t n;

	if ((pdptr = ddi_get_parent_data(dip)) == NULL)
		return;

	if ((n = (size_t)pdptr->par_nintr) != 0)
		/*
		 * Note that kmem_free is used here (instead of
		 * ddi_prop_free) because the contents of the
		 * property were placed into a separate buffer and
		 * mucked with a bit before being stored in par_intr.
		 * The actual return value from the prop lookup
		 * was freed with ddi_prop_free previously.
		 */
		kmem_free(pdptr->par_intr, n * sizeof (struct intrspec));

	if ((n = (size_t)pdptr->par_nrng) != 0)
		ddi_prop_free((void *)pdptr->par_rng);

	if ((n = pdptr->par_nreg) != 0)
		ddi_prop_free((void *)pdptr->par_reg);

	kmem_free(pdptr, sizeof (*pdptr));
	ddi_set_parent_data(dip, NULL);
}

void
impl_ddi_sunbus_removechild(dev_info_t *dip)
{
	impl_free_ddi_ppd(dip);
	ddi_set_name_addr(dip, NULL);
	/*
	 * Strip the node to properly convert it back to prototype form
	 */
	impl_rem_dev_props(dip);
}

/*
 * DDI Interrupt
 */

/*
 * turn this on to force isa, eisa, and mca device to ignore the new
 * hardware nodes in the device tree (normally turned on only for
 * drivers that need it by setting the property "ignore-hardware-nodes"
 * in their driver.conf file).
 *
 * 7/31/96 -- Turned off globally.  Leaving variable in for the moment
 *		as safety valve.
 */
int ignore_hardware_nodes = 0;

/*
 * Local data
 */
static struct impl_bus_promops *impl_busp;


/*
 * New DDI interrupt framework
 */

/*
 * i_ddi_intr_ops:
 *
 * This is the interrupt operator function wrapper for the bus function
 * bus_intr_op.
 */
int
i_ddi_intr_ops(dev_info_t *dip, dev_info_t *rdip, ddi_intr_op_t op,
    ddi_intr_handle_impl_t *hdlp, void * result)
{
	dev_info_t	*pdip = (dev_info_t *)DEVI(dip)->devi_parent;
	int		ret = DDI_FAILURE;

	/* request parent to process this interrupt op */
	if (NEXUS_HAS_INTR_OP(pdip))
		ret = (*(DEVI(pdip)->devi_ops->devo_bus_ops->bus_intr_op))(
		    pdip, rdip, op, hdlp, result);
	else
		cmn_err(CE_WARN, "Failed to process interrupt "
		    "for %s%d due to down-rev nexus driver %s%d",
		    ddi_get_name(rdip), ddi_get_instance(rdip),
		    ddi_get_name(pdip), ddi_get_instance(pdip));
	return (ret);
}

/*
 * i_ddi_add_softint - allocate and add a soft interrupt to the system
 */
int
i_ddi_add_softint(ddi_softint_hdl_impl_t *hdlp)
{
	int ret;

	/* add soft interrupt handler */
	ret = add_avsoftintr((void *)hdlp, hdlp->ih_pri, hdlp->ih_cb_func,
	    DEVI(hdlp->ih_dip)->devi_name, hdlp->ih_cb_arg1, hdlp->ih_cb_arg2);
	return (ret ? DDI_SUCCESS : DDI_FAILURE);
}


void
i_ddi_remove_softint(ddi_softint_hdl_impl_t *hdlp)
{
	(void) rem_avsoftintr((void *)hdlp, hdlp->ih_pri, hdlp->ih_cb_func);
}


extern void (*setsoftint)(int, struct av_softinfo *);
extern boolean_t av_check_softint_pending(struct av_softinfo *, boolean_t);

int
i_ddi_trigger_softint(ddi_softint_hdl_impl_t *hdlp, void *arg2)
{
	if (av_check_softint_pending(hdlp->ih_pending, B_FALSE))
		return (DDI_EPENDING);

	update_avsoftintr_args((void *)hdlp, hdlp->ih_pri, arg2);

	(*setsoftint)(hdlp->ih_pri, hdlp->ih_pending);
	return (DDI_SUCCESS);
}

/*
 * i_ddi_set_softint_pri:
 *
 * The way this works is that it first tries to add a softint vector
 * at the new priority in hdlp. If that succeeds; then it removes the
 * existing softint vector at the old priority.
 */
int
i_ddi_set_softint_pri(ddi_softint_hdl_impl_t *hdlp, uint_t old_pri)
{
	int ret;

	/*
	 * If a softint is pending at the old priority then fail the request.
	 */
	if (av_check_softint_pending(hdlp->ih_pending, B_TRUE))
		return (DDI_FAILURE);

	ret = av_softint_movepri((void *)hdlp, old_pri);
	return (ret ? DDI_SUCCESS : DDI_FAILURE);
}

void
i_ddi_alloc_intr_phdl(ddi_intr_handle_impl_t *hdlp)
{
	hdlp->ih_private = (void *)kmem_zalloc(sizeof (ihdl_plat_t), KM_SLEEP);
}

void
i_ddi_free_intr_phdl(ddi_intr_handle_impl_t *hdlp)
{
	kmem_free(hdlp->ih_private, sizeof (ihdl_plat_t));
	hdlp->ih_private = NULL;
}

int
i_ddi_get_intx_nintrs(dev_info_t *dip)
{
	struct ddi_parent_private_data *pdp;

	if ((pdp = ddi_get_parent_data(dip)) == NULL)
		return (0);

	return (pdp->par_nintr);
}

/*
 * DDI Memory/DMA
 */

/*
 * Support for allocating DMAable memory to implement
 * ddi_dma_mem_alloc(9F) interface.
 */

#define	KA_ALIGN_SHIFT		7
#define	KA_ALIGN		(1 << KA_ALIGN_SHIFT)
#define	KA_NCACHE		(PAGESHIFT + 2 - KA_ALIGN_SHIFT)
#define	KA_CACHE_MAXSIZE	(KA_ALIGN << (KA_NCACHE - 1))

/*
 * kmem arena and cache for kalloca().
 * We always use uncached mapping for DMA memory because MPCore has
 * no ability to keep cache coherency between CPU cache and I/O data.
 */
static vmem_t		*kmem_io_arena;
static kmem_cache_t	*kmem_io_cache[KA_NCACHE];

/*
 * Hash table implementation to keep trach usage of kalloca().
 * Unlike modhash, ka_hash passes key and value pair to destructor.
 * This will reduces memory usage for hash table maintenance.
 */

/* Number of hash entry. It must be a power of 2. */
#define	KA_HASH_NENTRY		64
#define	KA_HASH_MASK		(KA_HASH_NENTRY - 1)

/* Hash function that generates hash index from address */
typedef uint_t	(*kh_func_t)(void *key);

/* Destructor for ka_hash entry */
typedef void	(*kh_dtor_t)(void *key, void *value);

/* Hash table entry */
struct ka_hashent;
typedef struct ka_hashent	ka_hashent_t;

struct ka_hashent {
	void		*khe_key;		/* Hash key */
	void		*khe_value;		/* Hash value */
	ka_hashent_t	*khe_next;		/* Next entry */
};

/*
 * Hash table itself.
 */
typedef struct ka_hash {
	/*
	 * Mutex to protect hash contents.
	 * We decided to use simple mutex because currently hash search
	 * without changing hash contents is not supproted.
	 */
	kmutex_t	kh_mutex;

	/* Hash function. */
	kh_func_t	kh_func;

	/* Destructor of key and value pair. */
	kh_dtor_t	kh_dtor;

	/* Hash entry. Each entry is a NULL terminated list. */
	ka_hashent_t	*kh_table[KA_HASH_NENTRY];
} ka_hash_t;

/* Lock/Unlock hash table. */
#define	KA_HASH_LOCK(hash)	mutex_enter(&(hash)->kh_mutex)
#define	KA_HASH_UNLOCK(hash)	mutex_exit(&(hash)->kh_mutex)

/* Hash function to determine hash index. */
#define	KA_HASH_FUNC(hash, key)				\
	((uint_t)((hash)->kh_func(key) & KA_HASH_MASK))

/*
 * We create three hash tables for kalloca().
 * These are used to keep track of usage of:
 *	- memory allocated by contig_alloc().
 *	- memory allocated by vmem_alloc().
 *	- memory allocated by kmem_cache_alloc().
 */
ka_hash_t	*kmem_iohash_contig;
ka_hash_t	*kmem_iohash_vmem;
ka_hash_t	*kmem_iohash_cache;

/*
 * static ka_hash_t *
 * ka_hash_create(kh_func_t func, kh_dtor_t dtor)
 *	Create hash table to keep track usage of kalloca().
 *
 *	"func" is a hash function to generate hash index from address.
 *
 *	"dtor" is a destructor that will be called when a key and value
 *	pair is removed from the hash table.
 */
static ka_hash_t *
ka_hash_create(kh_func_t func, kh_dtor_t dtor)
{
	ka_hash_t	*hash;
	int		i;

	/* Allocate hash table. */
	hash = (ka_hash_t *)kmem_alloc(sizeof(ka_hash_t), KM_SLEEP);

	mutex_init(&(hash->kh_mutex), NULL, MUTEX_DEFAULT, NULL);
	hash->kh_func = func;
	hash->kh_dtor = dtor;
	for (i = 0; i < KA_HASH_NENTRY; i++) {
		hash->kh_table[i] = NULL;
	}

	return hash;
}

/*
 * static boolean_t
 * ka_hash_insert(ka_hash_t *hash, void *addr, void *value, int kmflag)
 *	Insert hash entry into the specified hash table.
 *
 *	"addr" must be an address that is returned by kalloca().
 *	"value" is associated with the specified "addr".
 *	kmflag will be passed to kmem_alloc() to allocate hash entry.
 *
 * Calling/Exit State:
 *	Upon successful completion, ka_hash_insert() returns B_TRUE.
 *	Otherwise returns B_FALSE.
 */
static boolean_t
ka_hash_insert(ka_hash_t *hash, void *addr, void *value, int kmflag)
{
	uint_t		index;
	ka_hashent_t	*entry;
#ifdef	DEBUG
	ka_hashent_t	*ep;
#endif	/* DEBUG */

	/* Allocate hash entry. */
	entry = (ka_hashent_t *)kmem_alloc(sizeof(ka_hashent_t), kmflag);
	if (entry == NULL) {
		return B_FALSE;
	}

	/* Initialize hash entry. */
	entry->khe_key = addr;
	entry->khe_value = value;

	index = KA_HASH_FUNC(hash, addr);
	KA_HASH_LOCK(hash);

#ifdef	DEBUG
	/* Check whether the specified address is already inserted. */
	for (ep = hash->kh_table[index]; ep != NULL; ep = ep->khe_next) {
		if (ep->khe_key == addr) {
			panic("ka_hash_insert: duplicated: key=0x%p, "
			      "value=0x%p, index=%d", addr, value, index);
		}
	}
#endif	/* DEBUG */

	/* Insert hash entry at the index derived by hash function. */
	entry->khe_next = hash->kh_table[index];
	hash->kh_table[index] = entry;
	KA_HASH_UNLOCK(hash);

	return B_TRUE;
}

/*
 * static boolean_t
 * ka_hash_remove(ka_hash_t *hash, void *addr)
 *	Remove key and value pair from the specified hash table.
 *	Destructor is called when the value is found associated with the
 *	specified address.
 *
 * Calling/Exit State:
 *	ka_hash_remove() returns B_TRUE if the key and value pair is found.
 *	Otherwise returns B_FALSE.
 */
static boolean_t
ka_hash_remove(ka_hash_t *hash, void *addr)
{
	ka_hashent_t	**epp, *entry = NULL;
	uint_t		index, cnt = 0;

	/* Lookup hash entry and remove it. */
	index = KA_HASH_FUNC(hash, addr);
	KA_HASH_LOCK(hash);
	for (epp = &(hash->kh_table[index]); *epp != NULL;
	     epp = &((*epp)->khe_next), cnt++) {
		if ((*epp)->khe_key == addr) {
			/* Found an entry. */
			entry = *epp;
			*epp = entry->khe_next;
			break;
		}
	}
	KA_HASH_UNLOCK(hash);

	if (entry == NULL) {
		/* Not found. */
		return B_FALSE;
	}

	/* Call destructor. */
	ASSERT(addr == entry->khe_key);
	hash->kh_dtor(addr, entry->khe_value);

	/* Free hash entry. */
	kmem_free(entry, sizeof(ka_hashent_t));

	return B_TRUE;
}

/*
 * static uint_t
 * ka_hash_func_page(void *addr)
 *	Generate hash index from address.
 *	This function is used when we know all addresses in the hash are
 *	page aligned.
 */
static uint_t
ka_hash_func_page(void *addr)
{
	return btop((uintptr_t)addr);
}

/*
 * static uint_t
 * ka_hash_func_kalign(void *addr)
 *	Generate hash index from address.
 *	This function is used when we know the address in the hash is not
 *	always page aligned.
 */
static uint_t
ka_hash_func_kalign(void *addr)
{
	uintptr_t	vaddr = (uintptr_t)addr;

	return btop(vaddr) + (vaddr >> KA_ALIGN_SHIFT);
}

/*
 * static void
 * ka_hash_dtor_contig(void *addr, void *value)
 *	Destructor for kalloca() memory allocated by contig_alloc().
 *	"addr" must be an address allocated by contig_alloc(), and
 *	value" must be its size.
 */
static void
ka_hash_dtor_contig(void *addr, void *value)
{
	size_t	size = (size_t)value;

	contig_free(addr, size);
}

/*
 * static void
 * ka_hash_dtor_vmem(void *addr, void *value)
 *	Destructor for kalloca() memory allocated by vmem_alloc().
 *	"addr" must be an address allocated by vmem_alloc(), and
 *	value" must be its size.
 */
static void
ka_hash_dtor_vmem(void *addr, void *value)
{
	size_t	size = (size_t)value;

	vmem_free(kmem_io_arena, addr, size);
}

/*
 * static void
 * ka_hash_dtor_cache(void *addr, void *value)
 *	Destructor for kalloca() memory allocated by kmem_cache_alloc().
 *	"addr" must be an address allocated by kmem_cache_alloc(), and
 *	value" must be an cache address where the address comes from.
 */
static void
ka_hash_dtor_cache(void *addr, void *value)
{
	kmem_cache_t	*cache = (kmem_cache_t *)value;

	kmem_cache_free(cache, addr);
}

/*
 * void
 * ka_init(void)
 *	Initialization for kalloca().
 */
void
ka_init(void)
{
	int	i;

	/* Initialize uncached arena. */
	kmem_io_arena = vmem_create("kmem_io", NULL, 0, PAGESIZE,
				    dma_page_create, dma_page_free,
				    heap_arena, 0, VM_SLEEP);

	/* Initialize uncached kmem caches. */
	for (i = 0; i < KA_NCACHE; i++) {
		size_t	size = KA_ALIGN << i;
		size_t	align;
		char	name[32];

		align = MIN(size, PAGESIZE);
		(void)snprintf(name, sizeof(name), "kmem_io_%lu", size);
		kmem_io_cache[i] = kmem_cache_create(name, size, align,
						     NULL, NULL, NULL, NULL,
						     kmem_io_arena,
						     KMC_NOTOUCH);
	}

	/* Create hash tables to keep track usage of kalloca(). */
	kmem_iohash_contig = ka_hash_create(ka_hash_func_page,
					    ka_hash_dtor_contig);
	kmem_iohash_vmem = ka_hash_create(ka_hash_func_page,
					  ka_hash_dtor_vmem);
	kmem_iohash_cache = ka_hash_create(ka_hash_func_kalign,
					   ka_hash_dtor_cache);
}

/*
 * Allocate from the system, aligned on a specific boundary.
 * The alignment, if non-zero, must be a power of 2.
 */
static void *
kalloca(size_t size, size_t align, int cansleep, int physcontig,
	ddi_dma_attr_t *attr)
{
	caddr_t		addr;
	uintptr_t	minaddr;
	size_t		rsize;
	int		cindex, kmflag;
	kmem_cache_t	*cp;
	extern caddr_t	econtig;
	ARMPF_DMA_ATTR_DECL(lattr);

	ASSERT((align & (align - 1)) == 0);

	kmflag = (cansleep) ? KM_SLEEP : KM_NOSLEEP;
	minaddr = KVTOP(econtig);

	/* Update attr if required. */
	ARMPF_DMA_ATTR_UPDATE(attr, lattr, minaddr);

	/*
	 * We should use contig_alloc() if alignment is larger than
	 * PAGESIZE or constraints of physical address is specified,
	 * of course, physcontig is true and allocation is is larger than
	 * PAGESIZE.
	 */
	if (align > PAGESIZE ||
	    attr->dma_attr_addr_lo > (uint64_t)minaddr ||
	    attr->dma_attr_addr_hi < (uint64_t)ARMPF_DMA_MAX_PADDR ||
	    (physcontig && size > PAGESIZE)) {
		const uint_t	cattr = PROT_READ|PROT_WRITE;

		if ((addr = contig_alloc(size, attr, align, cattr,
					 kmflag|CA_DMA)) != NULL) {
			/* Preserve allocation size in the hash. */
			if (ka_hash_insert(kmem_iohash_contig, addr,
					   (void *)size, kmflag)) {
				return addr;
			}
			contig_free(addr, size);
		}
		return NULL;
	}

	/*
	 * We can round up allocation size to the requested address alignment
	 * because the aligmnent is equal or less than PAGESIZE.
	 */
	rsize = P2ROUNDUP_TYPED(size, align, size_t);

	/*
	 * To simplify picking the correct kmem_io_cache, we round up to
	 * a multiple of KA_ALIGN.
	 */
	rsize = P2ROUNDUP_TYPED(rsize, KA_ALIGN, size_t);

	if (rsize > KA_CACHE_MAXSIZE) {
		/*
		 * Allocate enough pages for the allocation size.
		 * We don't need to use vmem_xalloc() because the requested
		 * addrss alignment must be equal or less than PAGESIZE.
		 */
		addr = vmem_alloc(kmem_io_arena, rsize,
				  (cansleep) ? VM_SLEEP : VM_NOSLEEP);
		if (addr != NULL) {
			/* Preserve allocation size in the hash. */
			if (ka_hash_insert(kmem_iohash_vmem, addr,
					   (void *)rsize, kmflag)) {
				return addr;
			}
			vmem_free(kmem_io_arena, addr, rsize);
		}
		return NULL;
	}

	/* Allocate from kmem_cache. */
	cindex = highbit((rsize >> KA_ALIGN_SHIFT) - 1);
	cp = kmem_io_cache[cindex];
	addr = kmem_cache_alloc(cp, kmflag);
	if (addr == NULL) {
		int	i;

		ASSERT(cansleep == 0);

		/* Try larger caches. */
		for (i = cindex + 1; i < KA_NCACHE; i++) {
			cp = kmem_io_cache[i];
			addr = kmem_cache_alloc(cp, KM_NOSLEEP);
			if (addr) {
				goto kallocdone;
			}
		}
		return NULL;
	}

kallocdone:
	ASSERT(physcontig == 0 ||
	       !P2CROSS((uintptr_t)addr, (uintptr_t)addr + rsize - 1,
			PAGESIZE));
	ASSERT(IS_P2ALIGNED((uintptr_t)addr, align));

	/* Preserve cache address where this address comes from. */
	if (!ka_hash_insert(kmem_iohash_cache, addr, cp, kmflag)) {
		kmem_cache_free(cp, addr);
		return NULL;
	}

	return addr;
}

/*
 * static void
 * kfreea(void *addr)
 *	Release buffer allocated by kalloca().
 */
static void
kfreea(void *addr)
{
	/* Try to remove hash entry for contig_alloc(). */
	if (ka_hash_remove(kmem_iohash_contig, addr)) {
		return;
	}

	/* Try to remove hash entry for vmem_alloc(). */
	if (ka_hash_remove(kmem_iohash_vmem, addr)) {
		return;
	}

	/* Try to remove hash entry for kmem cache. */
	if (ka_hash_remove(kmem_iohash_cache, addr)) {
		return;
	}

	/* The given address must NOT be allocated by kalloca(). */
	panic("kfreea: bad address: 0x%p", addr);
}

/* set HAT endianess attributes from ddi_device_acc_attr */
void
i_ddi_devacc_to_hatacc(ddi_device_acc_attr_t *devaccp, uint_t *hataccp)
{
	if (devaccp != NULL) {
		if (devaccp->devacc_attr_endian_flags == DDI_STRUCTURE_LE_ACC) {
			*hataccp &= ~HAT_ENDIAN_MASK;
			*hataccp |= HAT_STRUCTURE_LE;
		} else if (devaccp->devacc_attr_endian_flags == DDI_STRUCTURE_BE_ACC) {
			*hataccp &= ~HAT_ENDIAN_MASK;
			*hataccp |= HAT_STRUCTURE_BE;
		} else if (devaccp->devacc_attr_endian_flags == DDI_NEVERSWAP_ACC) {
			*hataccp &= ~HAT_ENDIAN_MASK;
			*hataccp |= HAT_NEVERSWAP;
		}
	}
}

/*
 * Check if the specified cache attribute is supported on the platform.
 * This function must be called before i_ddi_cacheattr_to_hatacc().
 */
boolean_t
i_ddi_check_cache_attr(uint_t flags)
{
	/*
	 * The cache attributes are mutually exclusive. Any combination of
	 * the attributes leads to a failure.
	 */
	uint_t cache_attr = IOMEM_CACHE_ATTR(flags);
	if ((cache_attr != 0) && ((cache_attr & (cache_attr - 1)) != 0))
		return (B_FALSE);

	/* All cache attributes are supported. */
	if (cache_attr & (IOMEM_DATA_UNCACHED | IOMEM_DATA_CACHED |
	    IOMEM_DATA_UC_WR_COMBINE))
		return (B_TRUE);

	/* undefined attributes */
	return (B_FALSE);
}

/* set HAT cache attributes from the cache attributes */
void
i_ddi_cacheattr_to_hatacc(uint_t flags, uint_t *hataccp)
{
	/*
	 * Use uncached mapping because MPCore can't keep cache coherency
	 * between memory and DMA data.
	 */
	*hataccp &= ~HAT_ORDER_MASK;
	*hataccp |= (HAT_STRICTORDER | HAT_PLAT_NOCACHE);
}

/*
 * This should actually be called i_ddi_dma_mem_alloc. There should
 * also be an i_ddi_pio_mem_alloc. i_ddi_dma_mem_alloc should call
 * through the device tree with the DDI_CTLOPS_DMA_ALIGN ctl ops to
 * get alignment requirements for DMA memory. i_ddi_pio_mem_alloc
 * should use DDI_CTLOPS_PIO_ALIGN. Since we only have i_ddi_mem_alloc
 * so far which is used for both, DMA and PIO, we have to use the DMA
 * ctl ops to make everybody happy.
 */
/*ARGSUSED*/
int
i_ddi_mem_alloc(dev_info_t *dip, ddi_dma_attr_t *attr,
	size_t length, int cansleep, int flags,
	ddi_device_acc_attr_t *accattrp, caddr_t *kaddrp,
	size_t *real_length, ddi_acc_hdl_t *ap)
{
	caddr_t a;
	int iomin;
	ddi_acc_impl_t *iap;
	int physcontig = 0;
	pgcnt_t npages;
	pgcnt_t minctg;
	uint_t order;
	int e;

	/*
	 * Check legality of arguments
	 */
	if (length == 0 || kaddrp == NULL || attr == NULL) {
		return DDI_FAILURE;
	}

	if (attr->dma_attr_minxfer == 0 || attr->dma_attr_align == 0 ||
	    (attr->dma_attr_align & (attr->dma_attr_align - 1)) ||
	    (attr->dma_attr_minxfer & (attr->dma_attr_minxfer - 1)) ||
	    attr->dma_attr_sgllen == 0) {
		return DDI_FAILURE;
	}

	/*
	 * figure out most restrictive alignment requirement
	 */
	iomin = attr->dma_attr_minxfer;
	iomin = maxbit(iomin, attr->dma_attr_align);
	if (iomin == 0) {
		return DDI_FAILURE;
	}

	ASSERT((iomin & (iomin - 1)) == 0);
	ASSERT(iomin >= attr->dma_attr_minxfer);
	ASSERT(iomin >= attr->dma_attr_align);

	/*
	 * Determine if we need to satisfy the request for physically
	 * contiguous memory.
	 */
	npages = btopr(length + (attr->dma_attr_align & PAGEOFFSET));
	minctg = howmany(npages, attr->dma_attr_sgllen);

	if (minctg > 1) {
		uint64_t pfnseg = attr->dma_attr_seg >> PAGESHIFT;

		/*
		 * verify that the minimum contig requirement for the
		 * actual length does not cross segment boundary.
		 */
		length = P2ROUNDUP_TYPED(length, attr->dma_attr_minxfer,
					 size_t);
		npages = btopr(length);
		minctg = howmany(npages, attr->dma_attr_sgllen);
		if (minctg > pfnseg + 1) {
			return DDI_FAILURE;
		}
		physcontig = 1;
	}
	else {
		length = P2ROUNDUP_TYPED(length, iomin, size_t);
	}

	/*
	 * Allocate the requested amount from the system.
	 */
	a = kalloca(length, iomin, cansleep, physcontig, attr);

	if ((*kaddrp = a) == NULL) {
		return DDI_FAILURE;
	}

	if (real_length) {
		*real_length = length;
	}
	if (ap) {
		/*
		 * initialize access handle
		 */
		iap = (ddi_acc_impl_t *)ap->ah_platform_private;
		iap->ahi_acc_attr |= DDI_ACCATTR_CPU_VADDR;
		impl_acc_hdl_init(ap);
	}

	return DDI_SUCCESS;
}

/*
 * covert old DMA limits structure to DMA attribute structure
 * and continue
 */
int
i_ddi_mem_alloc_lim(dev_info_t *dip, ddi_dma_lim_t *limits,
	size_t length, int cansleep, int streaming,
	ddi_device_acc_attr_t *accattrp, caddr_t *kaddrp,
	uint_t *real_length, ddi_acc_hdl_t *ap)
{
	ddi_dma_attr_t dma_attr, *attrp;
	size_t rlen;
	int ret;

	if (limits == NULL) {
		return (DDI_FAILURE);
	}

	/*
	 * set up DMA attribute structure to pass to i_ddi_mem_alloc()
	 */
	attrp = &dma_attr;
	attrp->dma_attr_version = DMA_ATTR_V0;
	attrp->dma_attr_addr_lo = (uint64_t)limits->dlim_addr_lo;
	attrp->dma_attr_addr_hi = (uint64_t)limits->dlim_addr_hi;
	attrp->dma_attr_count_max = (uint64_t)limits->dlim_ctreg_max;
	attrp->dma_attr_align = 1;
	attrp->dma_attr_burstsizes = (uint_t)limits->dlim_burstsizes;
	attrp->dma_attr_minxfer = (uint32_t)limits->dlim_minxfer;
	attrp->dma_attr_maxxfer = (uint64_t)limits->dlim_reqsize;
	attrp->dma_attr_seg = (uint64_t)limits->dlim_adreg_max;
	attrp->dma_attr_sgllen = limits->dlim_sgllen;
	attrp->dma_attr_granular = (uint32_t)limits->dlim_granular;
	attrp->dma_attr_flags = 0;

	ret = i_ddi_mem_alloc(dip, attrp, length, cansleep, streaming,
			accattrp, kaddrp, &rlen, ap);
	if (ret == DDI_SUCCESS) {
		if (real_length)
			*real_length = (uint_t)rlen;
	}
	return (ret);
}

/* ARGSUSED */
void
i_ddi_mem_free(caddr_t kaddr, ddi_acc_hdl_t *ap)
{
	kfreea(kaddr);
}

/*
 * Access Barriers
 *
 */
/*ARGSUSED*/
int
i_ddi_ontrap(ddi_acc_handle_t hp)
{
	return (DDI_FAILURE);
}

/*ARGSUSED*/
void
i_ddi_notrap(ddi_acc_handle_t hp)
{
}


/*
 * Misc Functions
 */

/*
 * Implementation instance override functions
 */
/*ARGSUSED*/
uint_t
impl_assign_instance(dev_info_t *dip)
{
	return ((uint_t)-1);
}

/*ARGSUSED*/
int
impl_keep_instance(dev_info_t *dip)
{
	return (DDI_FAILURE);
}

/*ARGSUSED*/
int
impl_free_instance(dev_info_t *dip)
{
	return (DDI_FAILURE);
}

/*ARGSUSED*/
int
impl_check_cpu(dev_info_t *devi)
{
	return (DDI_SUCCESS);
}

/*
 * Copy name to property_name, since name
 * is in the low address range below kernelbase.
 */
static void
copy_boot_str(const char *boot_str, char *kern_str, int len)
{
	int i = 0;

	while (i < len - 1 && boot_str[i] != '\0') {
		kern_str[i] = boot_str[i];
		i++;
	}

	kern_str[i] = 0;	/* null terminate */
	if (boot_str[i] != '\0')
		cmn_err(CE_WARN,
		    "boot property string is truncated to %s", kern_str);
}

static void
get_boot_properties(void)
{
	extern char hw_provider[];
	dev_info_t *devi;
	char *name;
	int length;
	char property_name[50], property_val[50];
	void *bop_staging_area;

	bop_staging_area = kmem_zalloc(MMU_PAGESIZE, KM_NOSLEEP);

	/*
	 * Import "root" properties from the boot.
	 *
	 * We do this by invoking BOP_NEXTPROP until the list
	 * is completely copied in.
	 */

	devi = ddi_root_node();
	for (name = BOP_NEXTPROP(bootops, "");		/* get first */
	    name;					/* NULL => DONE */
	    name = BOP_NEXTPROP(bootops, name)) {	/* get next */

		/* copy string to memory above kernelbase */
		copy_boot_str(name, property_name, 50);

		/*
		 * Skip vga properties. They will be picked up later
		 * by get_vga_properties.
		 */
		if (strcmp(property_name, "display-edif-block") == 0 ||
		    strcmp(property_name, "display-edif-id") == 0) {
			continue;
		}

		length = BOP_GETPROPLEN(bootops, property_name);
		if (length == 0)
			continue;
		if (length > MMU_PAGESIZE) {
			cmn_err(CE_NOTE,
			    "boot property %s longer than 0x%x, ignored\n",
			    property_name, MMU_PAGESIZE);
			continue;
		}
		BOP_GETPROP(bootops, property_name, bop_staging_area);

		/*
		 * special properties:
		 * si-machine, si-hw-provider
		 *	goes to kernel data structures.
		 * bios-boot-device and stdout
		 *	goes to hardware property list so it may show up
		 *	in the prtconf -vp output. This is needed by
		 *	Install/Upgrade. Once we fix install upgrade,
		 *	this can be taken out.
		 */
		if (strcmp(name, "si-machine") == 0) {
			(void) strncpy(utsname.machine, bop_staging_area,
			    SYS_NMLN);
			utsname.machine[SYS_NMLN - 1] = (char)NULL;
		} else if (strcmp(name, "si-hw-provider") == 0) {
			(void) strncpy(hw_provider, bop_staging_area, SYS_NMLN);
			hw_provider[SYS_NMLN - 1] = (char)NULL;
		} else if (strcmp(name, "bios-boot-device") == 0) {
			copy_boot_str(bop_staging_area, property_val, 50);
			(void) ndi_prop_update_string(DDI_DEV_T_NONE, devi,
			    property_name, property_val);
		} else if (strcmp(name, "stdout") == 0) {
			(void) ndi_prop_update_int(DDI_DEV_T_NONE, devi,
			    property_name, *((int *)bop_staging_area));
		} else {
			/* Property type unknown, use old prop interface */
			(void) e_ddi_prop_create(DDI_DEV_T_NONE, devi,
			    DDI_PROP_CANSLEEP, property_name, bop_staging_area,
			    length);
		}
	}

	kmem_free(bop_staging_area, MMU_PAGESIZE);
}

static void
get_vga_properties(void)
{
	dev_info_t *devi;
	major_t major;
	char *name;
	int length;
	char property_val[50];
	void *bop_staging_area;

	major = ddi_name_to_major("vgatext");
	if (major == (major_t)-1)
		return;
	devi = devnamesp[major].dn_head;
	if (devi == NULL)
		return;

	bop_staging_area = kmem_zalloc(MMU_PAGESIZE, KM_SLEEP);

	/*
	 * Import "vga" properties from the boot.
	 */
	name = "display-edif-block";
	length = BOP_GETPROPLEN(bootops, name);
	if (length > 0 && length < MMU_PAGESIZE) {
		BOP_GETPROP(bootops, name, bop_staging_area);
		(void) ndi_prop_update_byte_array(DDI_DEV_T_NONE,
		    devi, name, bop_staging_area, length);
	}

	/*
	 * kdmconfig is also looking for display-type and
	 * video-adapter-type. We default to color and svga.
	 *
	 * Could it be "monochrome", "vga"?
	 * Nah, you've got to come to the 21st century...
	 * And you can set monitor type manually in kdmconfig
	 * if you are really an old junky.
	 */
	(void) ndi_prop_update_string(DDI_DEV_T_NONE,
	    devi, "display-type", "color");
	(void) ndi_prop_update_string(DDI_DEV_T_NONE,
	    devi, "video-adapter-type", "svga");

	name = "display-edif-id";
	length = BOP_GETPROPLEN(bootops, name);
	if (length > 0 && length < MMU_PAGESIZE) {
		BOP_GETPROP(bootops, name, bop_staging_area);
		copy_boot_str(bop_staging_area, property_val, length);
		(void) ndi_prop_update_string(DDI_DEV_T_NONE,
		    devi, name, property_val);
	}

	kmem_free(bop_staging_area, MMU_PAGESIZE);
}

/*
 * Perform a copy from a memory mapped device (whose devinfo pointer is devi)
 * separately mapped at devaddr in the kernel to a kernel buffer at kaddr.
 */
/*ARGSUSED*/
int
e_ddi_copyfromdev(dev_info_t *devi,
    off_t off, const void *devaddr, void *kaddr, size_t len)
{
	bcopy(devaddr, kaddr, len);
	return (0);
}

/*
 * Perform a copy to a memory mapped device (whose devinfo pointer is devi)
 * separately mapped at devaddr in the kernel from a kernel buffer at kaddr.
 */
/*ARGSUSED*/
int
e_ddi_copytodev(dev_info_t *devi,
    off_t off, const void *kaddr, void *devaddr, size_t len)
{
	bcopy(kaddr, devaddr, len);
	return (0);
}


static int
poke_mem(peekpoke_ctlops_t *in_args)
{
	int err = DDI_SUCCESS;
	on_trap_data_t otd;

	/* Set up protected environment. */
	if (!on_trap(&otd, OT_DATA_ACCESS)) {
		switch (in_args->size) {
		case sizeof (uint8_t):
			*(uint8_t *)(in_args->dev_addr) =
			    *(uint8_t *)in_args->host_addr;
			break;

		case sizeof (uint16_t):
			*(uint16_t *)(in_args->dev_addr) =
			    *(uint16_t *)in_args->host_addr;
			break;

		case sizeof (uint32_t):
			*(uint32_t *)(in_args->dev_addr) =
			    *(uint32_t *)in_args->host_addr;
			break;

		case sizeof (uint64_t):
			*(uint64_t *)(in_args->dev_addr) =
			    *(uint64_t *)in_args->host_addr;
			break;

		default:
			err = DDI_FAILURE;
			break;
		}
	} else
		err = DDI_FAILURE;

	/* Take down protected environment. */
	no_trap();

	return (err);
}


static int
peek_mem(peekpoke_ctlops_t *in_args)
{
	int err = DDI_SUCCESS;
	on_trap_data_t otd;

	if (!on_trap(&otd, OT_DATA_ACCESS)) {
		switch (in_args->size) {
		case sizeof (uint8_t):
			*(uint8_t *)in_args->host_addr =
			    *(uint8_t *)in_args->dev_addr;
			break;

		case sizeof (uint16_t):
			*(uint16_t *)in_args->host_addr =
			    *(uint16_t *)in_args->dev_addr;
			break;

		case sizeof (uint32_t):
			*(uint32_t *)in_args->host_addr =
			    *(uint32_t *)in_args->dev_addr;
			break;

		case sizeof (uint64_t):
			*(uint64_t *)in_args->host_addr =
			    *(uint64_t *)in_args->dev_addr;
			break;

		default:
			err = DDI_FAILURE;
			break;
		}
	} else
		err = DDI_FAILURE;

	no_trap();
	return (err);
}


/*
 * This is called only to process peek/poke when the DIP is NULL.
 * Assume that this is for memory, as nexi take care of device safe accesses.
 */
int
peekpoke_mem(ddi_ctl_enum_t cmd, peekpoke_ctlops_t *in_args)
{
	return (cmd == DDI_CTLOPS_PEEK ? peek_mem(in_args) : poke_mem(in_args));
}

/*
 * we've just done a cautious put/get. Check if it was successful by
 * calling pci_ereport_post() on all puts and for any gets that return -1
 */
static int
pci_peekpoke_check_fma(dev_info_t *dip, void *arg, ddi_ctl_enum_t ctlop)
{
	int	rval = DDI_SUCCESS;
	peekpoke_ctlops_t *in_args = (peekpoke_ctlops_t *)arg;
	ddi_fm_error_t de;
	ddi_acc_impl_t *hp = (ddi_acc_impl_t *)in_args->handle;
	ddi_acc_hdl_t *hdlp = (ddi_acc_hdl_t *)in_args->handle;
	int check_err = 0;
	int repcount = in_args->repcount;

	if (ctlop == DDI_CTLOPS_POKE &&
	    hdlp->ah_acc.devacc_attr_access != DDI_CAUTIOUS_ACC)
		return (DDI_SUCCESS);

	if (ctlop == DDI_CTLOPS_PEEK &&
	    hdlp->ah_acc.devacc_attr_access != DDI_CAUTIOUS_ACC) {
		for (; repcount; repcount--) {
			switch (in_args->size) {
			case sizeof (uint8_t):
				if (*(uint8_t *)in_args->host_addr == 0xff)
					check_err = 1;
				break;
			case sizeof (uint16_t):
				if (*(uint16_t *)in_args->host_addr == 0xffff)
					check_err = 1;
				break;
			case sizeof (uint32_t):
				if (*(uint32_t *)in_args->host_addr ==
				    0xffffffff)
					check_err = 1;
				break;
			case sizeof (uint64_t):
				if (*(uint64_t *)in_args->host_addr ==
				    0xffffffffffffffff)
					check_err = 1;
				break;
			}
		}
		if (check_err == 0)
			return (DDI_SUCCESS);
	}
	/*
	 * for a cautious put or get or a non-cautious get that returned -1 call
	 * io framework to see if there really was an error
	 */
	bzero(&de, sizeof (ddi_fm_error_t));
	de.fme_version = DDI_FME_VERSION;
	de.fme_ena = fm_ena_generate(0, FM_ENA_FMT1);
	if (hdlp->ah_acc.devacc_attr_access == DDI_CAUTIOUS_ACC) {
		de.fme_flag = DDI_FM_ERR_EXPECTED;
		de.fme_acc_handle = in_args->handle;
	} else if (hdlp->ah_acc.devacc_attr_access == DDI_DEFAULT_ACC) {
		/*
		 * We only get here with DDI_DEFAULT_ACC for config space gets.
		 * Non-hardened drivers may be probing the hardware and
		 * expecting -1 returned. So need to treat errors on
		 * DDI_DEFAULT_ACC as DDI_FM_ERR_EXPECTED.
		 */
		de.fme_flag = DDI_FM_ERR_EXPECTED;
		de.fme_acc_handle = in_args->handle;
	} else {
		/*
		 * Hardened driver doing protected accesses shouldn't
		 * get errors unless there's a hardware problem. Treat
		 * as nonfatal if there's an error, but set UNEXPECTED
		 * so we raise ereports on any errors and potentially
		 * fault the device
		 */
		de.fme_flag = DDI_FM_ERR_UNEXPECTED;
	}
	pci_ereport_post(dip, &de, NULL);
	if (hdlp->ah_acc.devacc_attr_access != DDI_DEFAULT_ACC &&
	    de.fme_status != DDI_FM_OK) {
		ndi_err_t *errp = (ndi_err_t *)hp->ahi_err;
		rval = DDI_FAILURE;
		errp->err_ena = de.fme_ena;
		errp->err_expected = de.fme_flag;
		errp->err_status = DDI_FM_NONFATAL;
	}
	return (rval);
}

/*
 * pci_peekpoke_check_nofma() is for when an error occurs on a register access
 * during pci_ereport_post(). We can't call pci_ereport_post() again or we'd
 * recurse, so assume all puts are OK and gets have failed if they return -1
 */
static int
pci_peekpoke_check_nofma(void *arg, ddi_ctl_enum_t ctlop)
{
	int rval = DDI_SUCCESS;
	peekpoke_ctlops_t *in_args = (peekpoke_ctlops_t *)arg;
	ddi_acc_impl_t *hp = (ddi_acc_impl_t *)in_args->handle;
	ddi_acc_hdl_t *hdlp = (ddi_acc_hdl_t *)in_args->handle;
	int repcount = in_args->repcount;

	if (ctlop == DDI_CTLOPS_POKE)
		return (rval);

	for (; repcount; repcount--) {
		switch (in_args->size) {
		case sizeof (uint8_t):
			if (*(uint8_t *)in_args->host_addr == 0xff)
				rval = DDI_FAILURE;
			break;
		case sizeof (uint16_t):
			if (*(uint16_t *)in_args->host_addr == 0xffff)
				rval = DDI_FAILURE;
			break;
		case sizeof (uint32_t):
			if (*(uint32_t *)in_args->host_addr == 0xffffffff)
				rval = DDI_FAILURE;
			break;
		case sizeof (uint64_t):
			if (*(uint64_t *)in_args->host_addr ==
			    0xffffffffffffffff)
				rval = DDI_FAILURE;
			break;
		}
	}
	if (hdlp->ah_acc.devacc_attr_access != DDI_DEFAULT_ACC &&
	    rval == DDI_FAILURE) {
		ndi_err_t *errp = (ndi_err_t *)hp->ahi_err;
		errp->err_ena = fm_ena_generate(0, FM_ENA_FMT1);
		errp->err_expected = DDI_FM_ERR_UNEXPECTED;
		errp->err_status = DDI_FM_NONFATAL;
	}
	return (rval);
}

int
pci_peekpoke_check(dev_info_t *dip, dev_info_t *rdip,
	ddi_ctl_enum_t ctlop, void *arg, void *result,
	int (*handler)(dev_info_t *, dev_info_t *, ddi_ctl_enum_t, void *,
	void *), kmutex_t *err_mutexp, kmutex_t *peek_poke_mutexp)
{
	int rval;
	peekpoke_ctlops_t *in_args = (peekpoke_ctlops_t *)arg;
	ddi_acc_impl_t *hp = (ddi_acc_impl_t *)in_args->handle;

	/*
	 * this function only supports cautious accesses, not peeks/pokes
	 * which don't have a handle
	 */
	if (hp == NULL)
		return (DDI_FAILURE);

	if (hp->ahi_acc_attr & DDI_ACCATTR_CONFIG_SPACE) {
		if (!mutex_tryenter(err_mutexp)) {
			/*
			 * As this may be a recursive call from within
			 * pci_ereport_post() we can't wait for the mutexes.
			 * Fortunately we know someone is already calling
			 * pci_ereport_post() which will handle the error bits
			 * for us, and as this is a config space access we can
			 * just do the access and check return value for -1
			 * using pci_peekpoke_check_nofma().
			 */
			rval = handler(dip, rdip, ctlop, arg, result);
			if (rval == DDI_SUCCESS)
				rval = pci_peekpoke_check_nofma(arg, ctlop);
			return (rval);
		}
		/*
		 * This can't be a recursive call. Drop the err_mutex and get
		 * both mutexes in the right order. If an error hasn't already
		 * been detected by the ontrap code, use pci_peekpoke_check_fma
		 * which will call pci_ereport_post() to check error status.
		 */
		mutex_exit(err_mutexp);
	}
	mutex_enter(peek_poke_mutexp);
	rval = handler(dip, rdip, ctlop, arg, result);
	if (rval == DDI_SUCCESS) {
		mutex_enter(err_mutexp);
		rval = pci_peekpoke_check_fma(dip, arg, ctlop);
		mutex_exit(err_mutexp);
	}
	mutex_exit(peek_poke_mutexp);
	return (rval);
}

void
impl_setup_ddi(void)
{
	/* Create device nodes for built-in devices. */
	builtin_device_init();

	/* Probe xramfs devices. */
	xramdev_impl_probe(ddi_root_node());

	/*
	 * Read in the properties from the boot.
	 */
	get_boot_properties();

	/* do bus dependent probes. */
	impl_bus_initialprobe();

	/* not framebuffer should be enumerated, if present */
	get_vga_properties();
}

#ifndef	ARMPF_PLAT_BUILTIN_DEVINIT
/*
 * static void
 * builtin_device_init(void)
 *	Create device node for built-in devices.
 *	All nodes for built-in devices are linked under to directly.
 */
static void
builtin_device_init(void)
{
	int		err;
	dev_info_t	*parent = ddi_root_node();
	const builtin_dev_t	*dev, *edev;

	/* Create built-in device nodes. */
	dev = builtin_dev;
	edev = dev + builtin_ndevs;
	for (; dev < edev; dev++) {
		builtin_device_create(parent, dev);
	}

	/* Set UART ports into root node. */
	err = ndi_prop_update_int_array(DDI_DEV_T_NONE, parent,
					BUILTIN_PROPNAME_UART_PORT,
					(int *)builtin_uart_port,
					builtin_uart_nports);
	ASSERT(err == DDI_PROP_SUCCESS);
	
}
#endif	/* !ARMPF_PLAT_BUILTIN_DEVINIT */

/*
 * For int, int64, string, byte properties.
 */
#define	BUILTIN_PROP_UPDATE(dev, type, dip, etype)			\
	do {								\
		const builtin_prop_t	*__prp, *__eprp;		\
									\
		__prp = BUILTIN_PROP(dev, type);			\
		__eprp = __prp + BUILTIN_NPROPS(dev, type);		\
		for (; __prp < __eprp; __prp++) {			\
			int	__err;					\
			char	*__name = (char *)__prp->bp_name;	\
			etype	__value = (etype)__prp->bp_value;	\
			uint_t	__nelems = (uint_t)__prp->bp_nelems;	\
									\
			__err = BUILTIN_PROP_SET(type)(DDI_DEV_T_NONE,	\
						       (dip), __name,	\
						       __value,		\
						       __nelems);	\
			ASSERT(__err == DDI_PROP_SUCCESS);		\
		}							\
	} while (0)

/*
 * For boolean properties.
 */
#define	BUILTIN_BOOLEAN_PROP_UPDATE(dev, dip)				\
	do {								\
		const char	**__prp, **__eprp;			\
									\
		__prp = BUILTIN_PROP(dev, boolean);			\
		__eprp = __prp + BUILTIN_NPROPS(dev, boolean);		\
		for (; __prp < __eprp; __prp++) {			\
			int	__err;					\
			char	*__name = (char *)*__prp;		\
									\
			__err = ndi_prop_create_boolean(DDI_DEV_T_NONE,	\
							(dip), __name);	\
			ASSERT(__err == DDI_SUCCESS);			\
		}							\
	} while (0)

/*
 * Bind driver for the device node.
 */
#ifdef	DEBUG
#define	BUILTIN_BIND_DRIVER(dip)					\
	do {								\
		int	__err = ndi_devi_bind_driver(dip, 0);		\
									\
		if (__err != NDI_SUCCESS) {				\
			cmn_err(CE_WARN, "Failed to bind driver: %s",	\
				ddi_node_name(dip));			\
		}							\
	} while (0)
#else	/* !DEBUG */
#define	BUILTIN_BIND_DRIVER(dip)	(void)ndi_devi_bind_driver(dip, 0);
#endif	/* DEBUG */

/*
 * void
 * builtin_device_create(dev_info_t *parent, const builtin_dev_t *dev)
 *	Create device node according to the contents of builtin_dev_t.
 *	Created device node is linked under the given dev_info.
 */
void
builtin_device_create(dev_info_t *parent, const builtin_dev_t *dev)
{
	dev_info_t	*dip;

	/* Allocate a node under root. */
	ndi_devi_alloc_sleep(parent, (char *)dev->bd_name,
			     (pnode_t)DEVI_SID_NODEID, &dip);

	/* Set integer properties. */
	BUILTIN_PROP_UPDATE(dev, int, dip, int *);

	/* Set 64-bit integer properties. */
	BUILTIN_PROP_UPDATE(dev, int64, dip, int64_t *);

	/* Set string properties. */
	BUILTIN_PROP_UPDATE(dev, string, dip, char **);

	/* Set byte properties. */
	BUILTIN_PROP_UPDATE(dev, byte, dip, uchar_t *);

	/* Set boolean properties. */
	BUILTIN_BOOLEAN_PROP_UPDATE(dev, dip);

        /* Bind driver. */
	BUILTIN_BIND_DRIVER(dip);
}

#ifndef	USE_SWAPGENERIC
dev_t
getrootdev(void)
{
	/*
	 * Precedence given to rootdev if set in /etc/system
	 */
	if (root_is_svm) {
		return (ddi_pathname_to_dev_t(svm_bootpath));
	}

	/*
	 * Usually rootfs.bo_name is initialized by the
	 * the bootpath property from bootenv.rc, but
	 * defaults to "/ramdisk:a" otherwise.
	 */
	return (ddi_pathname_to_dev_t(rootfs.bo_name));
}
#endif	/* !USE_SWAPGENERIC */

static struct bus_probe {
	struct bus_probe *next;
	void (*probe)(int);
} *bus_probes;

void
impl_bus_add_probe(void (*func)(int))
{
	struct bus_probe *probe;

	probe = kmem_alloc(sizeof (*probe), KM_SLEEP);
	probe->next = bus_probes;
	probe->probe = func;
	bus_probes = probe;
}

/*ARGSUSED*/
void
impl_bus_delete_probe(void (*func)(int))
{
	struct bus_probe *prev = NULL;
	struct bus_probe *probe = bus_probes;

	while (probe) {
		if (probe->probe == func)
			break;
		prev = probe;
		probe = probe->next;
	}

	if (probe == NULL)
		return;

	if (prev)
		prev->next = probe->next;
	else
		bus_probes = probe->next;

	kmem_free(probe, sizeof (struct bus_probe));
}

/*
 * impl_bus_initialprobe
 *	Modload the prom simulator, then let it probe to verify existence
 *	and type of PCI support.
 */
static void
impl_bus_initialprobe(void)
{
	struct bus_probe *probe;

	/* load modules to install bus probes */
	if (modload("misc", "pci_autoconfig") < 0) {
		/* Skip PCI bus probe. */
		return;
	}

	probe = bus_probes;
	while (probe) {
		/* run the probe function */
		(*probe->probe)(0);
		probe = probe->next;
	}
}

/*
 * impl_bus_reprobe
 *	Reprogram devices not set up by firmware.
 */
static void
impl_bus_reprobe(void)
{
	struct bus_probe *probe;

	probe = bus_probes;
	while (probe) {
		/* run the probe function */
		(*probe->probe)(1);
		probe = probe->next;
	}
}
