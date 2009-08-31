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

#pragma ident	"%Z%%M%	%I%	%E% SMI"

/*
 *	Library file that has miscellaneous support for npe(7d)
 */

#include <sys/conf.h>
#include <sys/pci.h>
#include <sys/sunndi.h>
#include <sys/acpi/acpi.h>
#include <sys/acpi/acpi_pci.h>
#include <sys/acpica.h>
#include <io/pciex/pcie_nvidia.h>

/*
 * Prototype declaration
 */
void	npe_query_acpi_mcfg(dev_info_t *dip);
void	npe_ck804_fix_aer_ptr(ddi_acc_handle_t cfg_hdl);
int	npe_disable_empty_bridges_workaround(dev_info_t *child);

/*
 * Default ecfga base address
 */
int64_t npe_default_ecfga_base = 0xE0000000;

/*
 * Query the MCFG table using ACPI.  If MCFG is found, setup the
 * 'ecfga-base-address' (Enhanced Configuration Access base address)
 * property accordingly.  Otherwise, set the value of the property
 * to the default value.
 */
void
npe_query_acpi_mcfg(dev_info_t *dip)
{
	MCFG_TABLE *mcfgp;
	CFG_BASE_ADDR_ALLOC *cfg_baap;
	char *cfg_baa_endp;
	uint64_t ecfga_base;

	/* Query the MCFG table using ACPI */
	if (AcpiGetFirmwareTable(MCFG_SIG, 1, ACPI_LOGICAL_ADDRESSING,
	    (ACPI_TABLE_HEADER **)&mcfgp) == AE_OK) {

		cfg_baap = (CFG_BASE_ADDR_ALLOC *)mcfgp->CfgBaseAddrAllocList;
		cfg_baa_endp = ((char *)mcfgp) + mcfgp->Length;

		while ((char *)cfg_baap < cfg_baa_endp) {
			ecfga_base = ACPI_GET_ADDRESS(cfg_baap->base_addr);
			if (ecfga_base != (uint64_t)0) {
				/*
				 * Setup the 'ecfga-base-address' property to
				 * the base_addr found in the MCFG and return.
				 */
				(void) ndi_prop_update_int64(DDI_DEV_T_NONE,
				    dip, "ecfga-base-address", ecfga_base);
				return;
			}
			cfg_baap++;
		}
	}
	/*
	 * If MCFG is not found or ecfga_base is not found in MCFG table,
	 * set the 'ecfga-base-address' property to the default value.
	 */
	(void) ndi_prop_update_int64(DDI_DEV_T_NONE, dip,
	    "ecfga-base-address", npe_default_ecfga_base);
}


/*
 * Enable reporting of AER capability next pointer.
 * This needs to be done only for CK8-04 devices
 * by setting NV_XVR_VEND_CYA1 (offset 0xf40) bit 13
 * NOTE: BIOS is disabling this, it needs to be enabled temporarily
 */
void
npe_ck804_fix_aer_ptr(ddi_acc_handle_t cfg_hdl)
{
	ushort_t cya1;

	if ((pci_config_get16(cfg_hdl, PCI_CONF_VENID) == NVIDIA_VENDOR_ID) &&
	    (pci_config_get16(cfg_hdl, PCI_CONF_DEVID) ==
	    NVIDIA_CK804_DEVICE_ID) &&
	    (pci_config_get8(cfg_hdl, PCI_CONF_REVID) >=
	    NVIDIA_CK804_AER_VALID_REVID)) {
		cya1 =  pci_config_get16(cfg_hdl, NVIDIA_CK804_VEND_CYA1_OFF);
		if (!(cya1 & ~NVIDIA_CK804_VEND_CYA1_ERPT_MASK))
			(void) pci_config_put16(cfg_hdl,
			    NVIDIA_CK804_VEND_CYA1_OFF,
			    cya1 | NVIDIA_CK804_VEND_CYA1_ERPT_VAL);
	}
}


/*
 * If the bridge is empty, disable it
 */
int
npe_disable_empty_bridges_workaround(dev_info_t *child)
{
	/*
	 * Do not bind drivers to empty bridges.
	 * Fail above, if the bridge is found to be hotplug capable
	 */
	if (ddi_driver_major(child) == ddi_name_to_major("pcie_pci") &&
	    ddi_get_child(child) == NULL &&
	    ddi_prop_get_int(DDI_DEV_T_ANY, child, DDI_PROP_DONTPASS,
	    "pci-hotplug-type", INBAND_HPC_NONE) == INBAND_HPC_NONE)
		return (1);

	return (0);
}
