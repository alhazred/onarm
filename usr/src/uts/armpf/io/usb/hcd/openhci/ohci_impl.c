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

#include <sys/usb/hcd/openhci/ohcid.h>

#include <sys/devhalt.h>
#include <sys/ddi_impldefs.h> 

extern void *		ohci_statep;
extern int		ohci_max_instance;
static devhalt_id_t	ohci_devhalt_id = 0;

int			ohci_register_devhalt();
void			ohci_unregister_devhalt();
static boolean_t	ohci_reset_all_hc();

/*
 * ohci_register_devhalt:
 *
 * Register ohci_reset_all_hc to devhalt
 */
int
ohci_register_devhalt()
{
	/* Register ohci_reset_all_hc to devhalt. */
	if(!ohci_devhalt_id){
		ohci_devhalt_id = devhalt_add(ohci_reset_all_hc);
		if(!ohci_devhalt_id){
			return (USB_FAILURE);
		}
	}
	return (USB_SUCCESS);
}

/*
 * ohci_unregister_devhalt:
 *
 * Unregister ohci_reset_all_hc from devhalt
 */
void
ohci_unregister_devhalt()
{
	ohci_devhalt_id = devhalt_remove(ohci_devhalt_id);
}

/*
 * ohci_reset_all_hc:
 *
 * Reset Host Controller on reboot.
 *
 *  This function will be called on reboot.
 *  Host Controller is not reset on OS reboot.
 */
static boolean_t
ohci_reset_all_hc()
{
	int i;
	ohci_state_t	*ohcip;

	for(i = 0; i <= ohci_max_instance; i++){
		ohcip = ddi_get_soft_state(ohci_statep, i);
		if(ohcip){
			/* Do soft reset */
			Set_OpReg(hcr_cmd_status, HCR_STATUS_RESET);

			/* Wait 10ms for reset to complete */
			drv_usecwait(drv_usectohz(OHCI_RESET_TIMEWAIT));

			/* Do hard reset */
			Set_OpReg(hcr_control, HCR_CONTROL_RESET);
		}
	}

	return 0;
}

