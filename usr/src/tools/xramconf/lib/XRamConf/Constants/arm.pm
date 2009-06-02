#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright (c) 2008 NEC Corporation
# All rights reserved.
#

##
## ARM specific onstants.
##

package XRamConf::Constants::arm;

use strict;
use vars qw(@ISA @EXPORT);

use Exporter;
use XRamConf::Constants;

@ISA = qw(XRamConf::Constants Exporter);

##
## Kernel symbols
##

# Kernel load address in physical memory map
use constant	SYM_KERNELPHYSBASE		=> 'kernelphysbase';

# End boundary address of CTF data
use constant	SYM_CTF_END_ADDRESS		=> 'ctf_end_address';

# Start physical address of backup DRAM
use constant	SYM_BACKUP_DRAM_BASE		=> 'backup_dram_base';

# Size of backup DRAM
use constant	SYM_BACKUP_DRAM_SIZE		=> 'backup_dram_size';

# Start physical address of memory device
use constant	SYM_XRAMDEV_START_PADDR		=> 'xramdev_start_paddr';

# End physical address of memory device
use constant	SYM_XRAMDEV_END_PADDR		=> 'xramdev_end_paddr';

# Start physical address of pagable memory device
use constant	SYM_XRAMDEV_PAGESTART_PADDR	=> 'xramdev_pagestart_paddr';

# Base physical address of original data section
use constant	SYM_DATA_PADDR_BASE		=> 'data_paddr_base';

# Relocated physical address of data section
use constant	SYM_DATA_PADDR			=> 'data_paddr';

# Array of userdata device configuration.
use constant	SYM_NVDRAM_SEGS			=> 'nvdram_segs';

# Number of NVDRAM_SEGS elements
use constant	SYM_NVDRAM_SEGCNT		=> 'nvdram_segcnt';

# Pointer to memlist
use constant	SYM_ARMPF_BOOT_MEMLIST		=> 'armpf_boot_memlist';

# Root of B-tree index for memory device.
use constant	SYM_XMEMDEV_ROOT		=> 'xmemdev_root';

# Driver name of nvdram.
use constant	NVDRAM_DRVNAME			=> 'nvdram';

# nvdram device directory.
use constant	NVDRAM_DEVDIR			=> '/dev/nvdram';

##
## struct xmemdev definition.
##
use constant	XD_BASE		=> 0x0;
use constant	XD_COUNT	=> 0x4;
use constant	XD_ATTR		=> 0x8;
use constant	XD_LOWER	=> 0xc;
use constant	XD_HIGHER	=> 0x10;
use constant	SIZEOF_XMEMDEV	=> 0x14;

##
## struct xdseg definition.
##
use constant	XS_BASE		=> XD_BASE;
use constant	XS_COUNT	=> XD_COUNT;
use constant	XS_ATTR		=> XD_ATTR;
use constant	XS_LOWER	=> XD_LOWER;
use constant	XS_HIGHER	=> XD_HIGHER;
use constant	XS_FLAGS	=> (SIZEOF_XMEMDEV + 0x0);
use constant	XS_NAME		=> (SIZEOF_XMEMDEV + 0x4);
use constant	XS_VADDR	=> (SIZEOF_XMEMDEV + 0x8);
use constant	SIZEOF_XDSEG	=> (SIZEOF_XMEMDEV + 0xc);

##
## struct nvdseg definition.
##
use constant	NS_BASE		=> XD_BASE;
use constant	NS_COUNT	=> XD_COUNT;
use constant	NS_ATTR		=> XD_ATTR;
use constant	NS_LOWER	=> XD_LOWER;
use constant	NS_HIGHER	=> XD_HIGHER;
use constant	NS_MODE		=> (SIZEOF_XMEMDEV + 0x0);
use constant	NS_FLAGS	=> (SIZEOF_XMEMDEV + 0x4);
use constant	NS_UID		=> (SIZEOF_XMEMDEV + 0x8);
use constant	NS_GID		=> (SIZEOF_XMEMDEV + 0xc);
use constant	NS_NAME		=> (SIZEOF_XMEMDEV + 0x10);
use constant	NS_RPRIV	=> (SIZEOF_XMEMDEV + 0x14);
use constant	NS_WPRIV	=> (SIZEOF_XMEMDEV + 0x18);
use constant	SIZEOF_NVDSEG	=> (SIZEOF_XMEMDEV + 0x1c);

##
## struct memlist definition.
##
use constant	MEMLIST_ADDRESS	=> 0x0;
use constant	MEMLIST_SIZE	=> 0x8;
use constant	MEMLIST_NEXT	=> 0x10;
use constant	MEMLIST_PREV	=> 0x14;
use constant	SIZEOF_MEMLIST	=> 0x18;

@EXPORT = qw(XD_BASE XD_COUNT XD_ATTR XD_LOWER XD_HIGHER SIZEOF_XMEMDEV
	     XS_BASE XS_COUNT XS_ATTR XS_LOWER XS_HIGHER XS_FLAGS XS_NAME
	     XS_VADDR SIZEOF_XDSEG
	     NS_BASE NS_COUNT NS_ATTR NS_LOWER NS_HIGHER NS_MODE NS_FLAGS
	     NS_NAME NS_RPRIV NS_WPRIV SIZEOF_NVDSEG
	     MEMLIST_ADDRESS MEMLIST_SIZE MEMLIST_NEXT MEMLIST_PREV
	     SIZEOF_MEMLIST
	     SYM_KERNELPHYSBASE SYM_CTF_END_ADDRESS SYM_BACKUP_DRAM_BASE
	     SYM_BACKUP_DRAM_SIZE SYM_XRAMDEV_START_PADDR
	     SYM_XRAMDEV_END_PADDR SYM_XRAMDEV_PAGESTART_PADDR
	     SYM_DATA_PADDR_BASE SYM_DATA_PADDR
	     SYM_NVDRAM_SEGS SYM_NVDRAM_SEGCNT SYM_ARMPF_BOOT_MEMLIST
	     SYM_XMEMDEV_ROOT NVDRAM_DRVNAME NVDRAM_DEVDIR);

1;
