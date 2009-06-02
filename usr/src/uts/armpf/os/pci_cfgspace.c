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
 * Copyright (c) 2007-2009 NEC Corporation
 */

/*
 * PCI configuration space access routines
 */

#include <sys/systm.h>
#include <sys/psw.h>
#include <sys/bootconf.h>
#include <sys/reboot.h>
#include <sys/pci_impl_arm.h>
#include <sys/pci_cfgspace.h>
#include <sys/pci_cfgspace_impl.h>
#include <sys/cmn_err.h>
#include <sys/pci.h>
#include <sys/platform.h>

int pci_bios_cfg_type = PCI_MECHANISM_UNKNOWN;
int pci_bios_nbus;

/* Used for search bus devices */
static unsigned int current_busno;

/*
 * These two variables can be used to force a configuration mechanism or
 * to force which function is used to probe for the presence of the PCI bus.
 */
int	PCI_CFG_TYPE = 0;
int	PCI_PROBE_TYPE = 0;

/*
 * These function pointers lead to the actual implementation routines
 * for configuration space access.  Normally they lead to either the
 * pci_mech1_* or pci_mech2_* routines, but they can also lead to
 * routines that work around chipset bugs.
 */
uint8_t (*pci_getb_func)(int bus, int dev, int func, int reg);
uint16_t (*pci_getw_func)(int bus, int dev, int func, int reg);
uint32_t (*pci_getl_func)(int bus, int dev, int func, int reg);
void (*pci_putb_func)(int bus, int dev, int func, int reg, uint8_t val);
void (*pci_putw_func)(int bus, int dev, int func, int reg, uint16_t val);
void (*pci_putl_func)(int bus, int dev, int func, int reg, uint32_t val);

/*
 * Internal routines
 */
static int pci_check(void);

static void pci_device_configure(uchar_t, uchar_t, uchar_t, uchar_t);
static void pci_cfg_pciconfig_setup(void);
static void pci_vaddr_setting( uint32_t, uint32_t);
static uint32_t pci_vaddr_setting_r( uint32_t );

/* all config-space access routines share this one... */
kmutex_t pcicfg_mutex;

/* ..except Orion and Neptune, which have to have their own */
kmutex_t pcicfg_chipset_mutex;

void
pci_cfgspace_init(void)
{
	unsigned char	header;

	mutex_init(&pcicfg_mutex, NULL, MUTEX_SPIN,
	    (ddi_iblock_cookie_t)ipltospl(15));
	mutex_init(&pcicfg_chipset_mutex, NULL, MUTEX_SPIN,
	    (ddi_iblock_cookie_t)ipltospl(15));
	if (!pci_check()) {
		mutex_destroy(&pcicfg_mutex);
		mutex_destroy(&pcicfg_chipset_mutex);
	}

	pci_cfg_pciconfig_setup();

	/* Format current bus no */
	current_busno = 0;

	/* OHCI */	
	header = (*pci_getb_func)(0, USB_DEV_NUM, 0, PCI_CONF_HEADER);
	pci_device_configure(0, USB_DEV_NUM, 0, header);

	/* EHCI */
	header = (*pci_getb_func)(0, USB_DEV_NUM, 1, PCI_CONF_HEADER);
	pci_device_configure(0, USB_DEV_NUM, 1, header);
}

/*
 * This code determines if this system supports PCI and which
 * type of configuration access method is used
 */

static int
pci_check(void)
{
	/*
	 * Only do this once.  NB:  If this is not a PCI system, and we
	 * get called twice, we can't detect it and will probably die
	 * horribly when we try to ask the BIOS whether PCI is present.
	 * This code is safe *ONLY* during system startup when the
	 * BIOS is still available.
	 */
	if (pci_bios_cfg_type != PCI_MECHANISM_UNKNOWN)
		return (TRUE);

	pci_bios_cfg_type = PCI_MECHANISM_1;

	pci_getb_func = pci_mech1_getb;
	pci_getw_func = pci_mech1_getw;
	pci_getl_func = pci_mech1_getl;
	pci_putb_func = pci_mech1_putb;
	pci_putw_func = pci_mech1_putw;
	pci_putl_func = pci_mech1_putl;

	return (TRUE);
}

static void
pci_vaddr_setting( uint32_t offset, uint32_t val )
{
	volatile uint32_t *addr_ptr = NULL;

	addr_ptr = (uint32_t *)(ARMPF_PCI_CONFIG_VADDR+offset);
	*addr_ptr = val;
}

static uint32_t
pci_vaddr_setting_r( uint32_t offset )
{
	volatile uint32_t *addr_ptr = NULL;
	uint32_t	val;

	addr_ptr = (uint32_t *)(ARMPF_PCI_CONFIG_VADDR+offset);
	val = *addr_ptr;

	return( val );
}

static void
pci_cfg_pciconfig_setup(void)
{
	volatile uint16_t *addr_ptr = NULL;
	uint16_t	ans16;
	uint32_t	ans32;

	/* 1. reset procedure */
	pci_vaddr_setting(0xec, 0x00000000);	/* PCIBAREn register */
	pci_vaddr_setting(0xe4, 0x80000000);	/* PCICTRL-H register */
	drv_usecwait(10000);
	pci_vaddr_setting(0xe4, 0x00000000);	/* PCICTRL-H register */

	/* 2. setting for PCI window size */
	pci_vaddr_setting(0xa0, 0x00000020);	/* ACRn register */

	/* 3. setting for base address */
	pci_vaddr_setting(0x10, 0x80000000);	/* PCI_BAR0 register */

	/* 4. setting for PCIBAREn register*/
	pci_vaddr_setting(0xec, 0x00000001);

	/* 5. enable CNFGDONE bit */
	ans32 = pci_vaddr_setting_r(0xe4);	/* PCICTRL-H register */
	pci_vaddr_setting(0xe4, ans32 | 0x10000000);

	/* 6. setting for command register */
	addr_ptr = (uint16_t *)(ARMPF_PCI_CONFIG_VADDR + 0x04);
	ans16 = *addr_ptr;
	ans16 |= 0x0006;
	*addr_ptr = ans16;
}

#define BUS_SCAN(d) (((d) >> 16) & 0xff)
#define DEV_SCAN(d) (((d) >> 11) & 0x1f)
#define FUNC_SCAN(d) (((d) >> 8) & 0x7)
#define BDF_PCISCAN(b,d,f) ( ((b) << 16) | ((d) << 11) | ((f) << 8) )
#define BRISCAN_MAX(a,b) ( ((a)>(b)) ? (a) : (b) )

#define PCI_CLASS_BRIDGE_PCI     (PCI_CLASS_BRIDGE << 8) | PCI_BRIDGE_PCI
#define PCI_CLASS_STORAGE_IDE    (PCI_CLASS_MASS << 8) | PCI_MASS_IDE
#define PCI_CLASS_BRIDGE_CARDBUS (PCI_CLASS_BRIDGE << 8) | PCI_BRIDGE_CARDBUS

/*
 * FUNCTION : We don't use the configuration that configured by BIOS.
 *            So we configure the configuration registers for any device.
 * IN       : bus - bus number
 *            dev - device number
 *            func - function number
 *            header - header type(normal PCI device or PCI-PCI bridge)
 */
static void
pci_device_configure(uchar_t bus, uchar_t dev, uchar_t func, uchar_t header)
{
	uint16_t cmd_reg = 0;
	uint8_t latency = 0, iline = 0xff;
	uint32_t classcode;
	uint8_t baseclass,subclass,progclass;

	switch (header & PCI_HEADER_TYPE_M) {
		case PCI_HEADER_ZERO:
			/* normal PCI device */
			/* set command register */
			cmd_reg = PCI_COMM_ME | PCI_COMM_PARITY_DETECT
				| PCI_COMM_SERR_ENABLE | PCI_COMM_IO
				| PCI_COMM_MAE;

			/* get class code */
			classcode =
			(*pci_getl_func)((unsigned int)bus, (unsigned int)dev,
					(unsigned int)func,
					PCI_CONF_REVID) >> 8;
			baseclass = classcode >> 16;
			subclass = classcode >> 8 & 0xff;

			/* set latency timer */
			if (baseclass == PCI_CLASS_DISPLAY ||
				 classcode == 0x000100) {
				/* display */
				latency = 0x40;
			} else if (baseclass == PCI_CLASS_SERIALBUS &&
				subclass == PCI_SERIAL_USB) {

				/* figure out program class */
				progclass = classcode & 0xff;
				/* USB host controller */
				if (progclass == PCI_SERIAL_USB_IF_OHCI) {
					latency = 0x08;
				} else if (progclass==PCI_SERIAL_USB_IF_EHCI) {
					latency = 0x44;
				} else {
				/*
				 * In this case, progclass may be
				 * PCI_SERIAL_USB_IF_UHCI.
				 * But there is not such a device in H/W spec.
				 * therefore do nothing in this case.
				 */
				}
			} else {
				/*
				 * do nothing. because latency time of
				 * other device is zero.
				 */
			}

			if (bus == 0 && dev == USB_DEV_NUM && func == 0) {
				pci_putl_func(bus, dev, func,
					PCI_CONF_BASE0, OHCI_CONF_BASE);
			} else if ( bus == 0 && dev == USB_DEV_NUM &&
				func == 1 ) {
				pci_putl_func(bus, dev, func,
					PCI_CONF_BASE0, EHCI_CONF_BASE);
			} else {
				/* Do nothing */
			}	
			break;

		case PCI_HEADER_PPB:
			/* PCI-PCI bridge */
			break;

		case PCI_HEADER_CARDBUS:
			/* PCI Cardbus but not supported */
			break;

		default:
			/* not reached */
			return;
	}

	/* write command register */
	if ((*pci_getw_func)((unsigned int)bus, (unsigned int)dev,
			(unsigned int)func, PCI_CONF_COMM) == 0) {
		(*pci_putw_func)((unsigned int)bus, (unsigned int)dev,
				 (unsigned int)func, PCI_CONF_COMM, cmd_reg);
	}

	/* write cacheline size register */
	/* In ARM platform, cacheline size is 8word by MPCore spec. */
	(*pci_putb_func)((unsigned int)bus, (unsigned int)dev,
		 (unsigned int)func, PCI_CONF_CACHE_LINESZ, 0x08);

	/* write latency timer register */
	if ((*pci_getb_func)((unsigned int)bus, (unsigned int)dev,
			 (unsigned int)func, PCI_CONF_LATENCY_TIMER) == 0) {
		(*pci_putb_func)((unsigned int)bus, (unsigned int)dev,
			 (unsigned int)func, PCI_CONF_LATENCY_TIMER, latency);
	}

	/* set interrupt line register */
	pci_putb_func(bus, dev, func, PCI_CONF_ILINE, iline);

	return;
}
