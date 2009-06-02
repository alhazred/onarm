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

#include <sys/usb/hcd/ehci/ehcid.h>

#include <sys/devhalt.h>
#include <sys/ddi_impldefs.h> 

extern void *		ehci_statep;
extern int		ehci_max_instance;
static devhalt_id_t	ehci_devhalt_id = 0;

int			ehci_register_devhalt();
void			ehci_unregister_devhalt();
static boolean_t	ehci_reset_all_hc();

/*
 * ehci_register_devhalt:
 *
 * Register ehci_reset_all_hc to devhalt
 */
int
ehci_register_devhalt()
{
	/* Register ehci_reset_all_hc to devhalt. */
	if(!ehci_devhalt_id){
		ehci_devhalt_id = devhalt_add(ehci_reset_all_hc);
		if(!ehci_devhalt_id){
			return (USB_FAILURE);
		}
	}
	return (USB_SUCCESS);
}

/*
 * ehci_unregister_devhalt:
 *
 * Unregister ehci_reset_all_hc from devhalt
 */
void
ehci_unregister_devhalt()
{
	ehci_devhalt_id = devhalt_remove(ehci_devhalt_id);
}

/*
 * ehci_reset_all_hc:
 *
 * Reset Host Controller on reboot.
 *
 *  This function will be called on reboot.
 *  Host Controller is not reset on OS reboot.
 */
static boolean_t
ehci_reset_all_hc()
{
	int i;
	ehci_state_t	*ehcip;

	for(i = 0; i <= ehci_max_instance; i++){
		ehcip = ddi_get_soft_state(ehci_statep, i);
		if(ehcip){
			/* Stop the EHCI host controller */
			Set_OpReg(ehci_command,
			    Get_OpReg(ehci_command) & ~EHCI_CMD_HOST_CTRL_RUN);

			drv_usecwait(EHCI_RESET_TIMEWAIT);

			/* Reset the EHCI host controller */
			Set_OpReg(ehci_command,
			    Get_OpReg(ehci_command) | EHCI_CMD_HOST_CTRL_RESET);
		}
	}

	return 0;
}

