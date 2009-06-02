#!/bin/sh
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
# Copyright (c) 2009 NEC Corporation
# All rights reserved.
#

#
# Sample launcher script for NaviEngine emulation.
#

# emulation system
MACHINE=naviengine
MEMSIZE=320		# DRAM:256MB + NORFLASH:64MB

# kernel and root filesystem image
KERNEL=uImage
INITRD=ufs_root.img

# boot option
#
# NOTE: NE1 emulation for multiprocessor causes kernel panic.
#       Don't change the number of boot-ncpus.
#
BOOTARGS="prom_debug=1,gmt-lag=-32400,boot-ncpus=1"

# device option
FLASH=norflash.img
SERIAL0=stdio
SERIAL1=file:uartdump1
SERIAL2=file:uartdump2

./qemu-system-arm -M $MACHINE -kernel $KERNEL -m $MEMSIZE -pflash $FLASH \
        -append $BOOTARGS -initrd $INITRD -nographic \
        -serial $SERIAL0 -serial $SERIAL1 -serial $SERIAL2
