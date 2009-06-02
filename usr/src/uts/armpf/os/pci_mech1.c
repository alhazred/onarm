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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright (c) 2007-2009 NEC Corporation
 */

/*
 * PCI Mechanism 1 low-level routines
 */

#include <sys/types.h>
#include <sys/pci.h>
#include <sys/pci_impl_arm.h>
#include <sys/sunddi.h>
#include <sys/pci_cfgspace_impl.h>
#include <sys/cmn_err.h>

/*
 * Per PCI 2.1 section 3.7.4.1 and PCI-PCI Bridge Architecture 1.0 section
 * 5.3.1.2:  dev=31 func=7 reg=0 means a special cycle.  We don't want to
 * trigger that by accident, so we pretend that dev 31, func 7 doesn't
 * exist.  If we ever want special cycle support, we'll add explicit
 * special cycle support.
 */

uint32_t
pci_addroffset_chg( int bus, int device, int function, int reg )
{
	uint32_t	p_cfg;

	reg &= ~3;
	switch( bus ) {
	case 0 :
		device &= 0x1f;
		p_cfg = 1 << (16+device) | (device<<11) | (function<<8) | reg;
		break;
	default :
		p_cfg = bus << 16 | (device<<11) | (function<<8) | reg | 1;
		break;
	}
	return( p_cfg );
}

uint8_t
pci_mech1_getb(int bus, int device, int function, int reg)
{
	union {
		uint32_t	ans32;
		uint8_t		ans8[4];
	} valans;
	uint8_t val;
	volatile uint32_t *data_ptr = NULL;
	volatile uint32_t *addr_ptr = NULL;

	if (device == PCI_MECH1_SPEC_CYCLE_DEV &&
	    function == PCI_MECH1_SPEC_CYCLE_FUNC) {
		return (0xff);
	}
	addr_ptr = (uint32_t *)(ARMPF_PCI_CONFIG_VADDR + PCI_CONFADD);
	data_ptr = (uint32_t *)(ARMPF_PCI_CONFIG_VADDR + PCI_CONFDATA);

	val = 0xff;
	mutex_enter(&pcicfg_mutex);
	/* read Processing */
	*addr_ptr = 0x80000000 | pci_addroffset_chg(bus, device, function, reg);
	valans.ans32 = *data_ptr;
	val = valans.ans8[reg&3];
	mutex_exit(&pcicfg_mutex);

	return (val);
}

uint16_t
pci_mech1_getw(int bus, int device, int function, int reg)
{
	union {
		uint32_t	ans32;
		uint16_t	ans16[2];
	} valans;
	uint16_t val;
	volatile uint32_t *data_ptr = NULL;
	volatile uint32_t *addr_ptr = NULL;

	if (device == PCI_MECH1_SPEC_CYCLE_DEV &&
	    function == PCI_MECH1_SPEC_CYCLE_FUNC) {
		return (0xffff);
	}
	addr_ptr = (uint32_t *)(ARMPF_PCI_CONFIG_VADDR + PCI_CONFADD);
	data_ptr = (uint32_t *)(ARMPF_PCI_CONFIG_VADDR + PCI_CONFDATA);

	val = 0xffff;
	mutex_enter(&pcicfg_mutex);
	/* read Processing */
	*addr_ptr = 0x80000000 | pci_addroffset_chg(bus, device, function, reg);
	valans.ans32 = *data_ptr;
	val = valans.ans16[ ((reg & 0x2) == 0) ? 0 : 1 ];
	mutex_exit(&pcicfg_mutex);

	return (val);
}

uint32_t
pci_mech1_getl(int bus, int device, int function, int reg)
{
	uint32_t val;
	volatile uint32_t *data_ptr = NULL;
	volatile uint32_t *addr_ptr = NULL;

	if (device == PCI_MECH1_SPEC_CYCLE_DEV &&
	    function == PCI_MECH1_SPEC_CYCLE_FUNC) {
		return (0xffffffffu);
	}
	addr_ptr = (uint32_t *)(ARMPF_PCI_CONFIG_VADDR + PCI_CONFADD);
	data_ptr = (uint32_t *)(ARMPF_PCI_CONFIG_VADDR + PCI_CONFDATA);

	val = 0xffffffff;
	mutex_enter(&pcicfg_mutex);
	/* read Processing */
	*addr_ptr = 0x80000000 | pci_addroffset_chg(bus, device, function, reg);
	val = *data_ptr;
	mutex_exit(&pcicfg_mutex);

	return (val);
}

void
pci_mech1_putb(int bus, int device, int function, int reg, uint8_t val)
{
	union {
		uint32_t	ans32;
		uint8_t		ans8[4];
	} valans;
	volatile uint32_t *data_ptr = NULL;
	volatile uint32_t *addr_ptr = NULL;

	if (device == PCI_MECH1_SPEC_CYCLE_DEV &&
	    function == PCI_MECH1_SPEC_CYCLE_FUNC) {
		return;
	}
	addr_ptr = (uint32_t *)(ARMPF_PCI_CONFIG_VADDR + PCI_CONFADD);
	data_ptr = (uint32_t *)(ARMPF_PCI_CONFIG_VADDR + PCI_CONFDATA);

	mutex_enter(&pcicfg_mutex);
	/* read Processing */
	valans.ans32 = 0xffffffff;
	*addr_ptr = 0x80000000 | pci_addroffset_chg(bus, device, function, reg);
	valans.ans32 = *data_ptr;
	/* write Processing */
	valans.ans8[reg&3] = val & 0xff;
	*addr_ptr = 0x80000000 | pci_addroffset_chg(bus, device, function, reg);
	*data_ptr = valans.ans32;
	mutex_exit(&pcicfg_mutex);
}

void
pci_mech1_putw(int bus, int device, int function, int reg, uint16_t val)
{
	union {
		uint32_t	ans32;
		uint16_t	ans16[2];
	} valans;
	volatile uint32_t *data_ptr = NULL;
	volatile uint32_t *addr_ptr = NULL;

	if (device == PCI_MECH1_SPEC_CYCLE_DEV &&
	    function == PCI_MECH1_SPEC_CYCLE_FUNC) {
		return;
	}
	addr_ptr = (uint32_t *)(ARMPF_PCI_CONFIG_VADDR + PCI_CONFADD);
	data_ptr = (uint32_t *)(ARMPF_PCI_CONFIG_VADDR + PCI_CONFDATA);

	mutex_enter(&pcicfg_mutex);
	/* read Processing */
	valans.ans32 = 0xffffffff;
	*addr_ptr = 0x80000000 | pci_addroffset_chg(bus, device, function, reg);
	valans.ans32 = *data_ptr;
	/* Write Processing */
	valans.ans16[((reg & 0x2) == 0) ? 0 : 1] = val & 0xffff;
	*addr_ptr = 0x80000000 | pci_addroffset_chg(bus, device, function, reg);
	*data_ptr = valans.ans32;
	mutex_exit(&pcicfg_mutex);
}

void
pci_mech1_putl(int bus, int device, int function, int reg, uint32_t val)
{
	volatile uint32_t *data_ptr = NULL;
	volatile uint32_t *addr_ptr = NULL;
	volatile uint32_t ans;

	if (device == PCI_MECH1_SPEC_CYCLE_DEV &&
	    function == PCI_MECH1_SPEC_CYCLE_FUNC) {
		return;
	}
	addr_ptr = (uint32_t *)(ARMPF_PCI_CONFIG_VADDR + PCI_CONFADD);
	data_ptr = (uint32_t *)(ARMPF_PCI_CONFIG_VADDR + PCI_CONFDATA);

	mutex_enter(&pcicfg_mutex);
	/* Read Processing */
	*addr_ptr = 0x80000000 | pci_addroffset_chg(bus, device, function, reg);
	ans = *data_ptr;
	/* Write Processing */
	*addr_ptr = 0x80000000 | pci_addroffset_chg(bus, device, function, reg);
	*data_ptr = val;
	mutex_exit(&pcicfg_mutex);
}
