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
# Copyright (c) 2009 NEC Corporation
# All rights reserved.
#

##
## Setup NaviEngine system emulation environment.
##
## You have to build the kernel, commands, libraries and
## create the ramdisk image before setup.
##

include	$(SRC)/Makefile.master

#
# Command paths.
#
UTSTUNE	= $(ONBLD_TOOLS)/bin/utstune
MKFILE = /usr/sbin/mkfile

#
# Parameters for QEMU NaviEngine system emulator.
#
QEMUARM		= qemu-system-arm
SRC_QEMUARM	= ../qemu-0.9.1/arm-softmmu/$(QEMUARM)
UARTLOG		= uartdump1 uartdump2

#
# Parameters for kernel image.
#
UTSBUILD				= obj32
$(BUILD64)UTSBUILD			= obj64
$(NOT_RELEASE_BUILD)UTSBUILD		= debug32
$(BUILD64)$(NOT_RELEASE_BUILD)UTSBUILD	= debug64
UIMAGE		= uImage
SRC_UIMAGE	= $(SRC)/uts/$(ARM_PLATFORM)/unix/$(UTSBUILD)/$(UIMAGE)

#
# Parameters for filesystem image.
#
RDIMAGE		= ufs_root.img
SRC_RDIMAGE	= ../rdimage/$(RDIMAGE)

#
# Parameters for NOR flash memory image.
#
NORIMAGE	= norflash.img
NORSIZE		= 64m

.KEEP_STATE:

all:	$(QEMUARM) $(UIMAGE) $(RDIMAGE) $(NORIMAGE)

# Emulator
$(QEMUARM):
	$(SYMLINK) $(SRC_QEMUARM) $@

# Kernel uImage
$(UIMAGE):	$(SRC_UIMAGE)
	$(CP) $(SRC_UIMAGE) $@

# UFS ramdisk image
$(RDIMAGE):	$(SRC_RDIMAGE)
	$(CP) $(SRC_RDIMAGE) $@

# NOR flash memory
$(NORIMAGE):
	$(MKFILE) $(NORSIZE) $@

clean:
	$(RM) $(QEMUARM) $(UIMAGE) $(RDIMAGE) $(NORIMAGE) $(UARTLOG)

clobber:	clean

