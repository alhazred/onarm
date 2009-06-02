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
## Create ramdisk image for QEMU. (ARM architecture specific)
##
## You have to build the kernel, commands, libraries before you create
## image files. In addition, the kernel must be built with UFS/Ramdisk
## configuration.
##

include	$(SRC)/Makefile.master

QEMUDIR	= $(SRC)/qemu

#
# Command paths.
#
PAWK	= /usr/xpg4/bin/awk
UTSTUNE	= $(ONBLD_TOOLS)/bin/utstune
MKUFS	= ./mkufs
MKROOT	= ./mkroot

#
# Parameters for filesystem image.
#
FS_ROOT		= $(QEMUDIR)/rdimage/ufs_root.img
FS_MNTPOINT	= $(QEMUDIR)/rdimage/mnt
MKROOT_ERR	= mkroot.err
MKROOT_ERRPAT_PERM	= 'not owner|permission denied|cannot access'
MKROOT_PERMCHECK	=	\
	 $(EGREP) -i $(MKROOT_ERRPAT_PERM) $(MKROOT_ERR) > /dev/null 2>&1

.KEEP_STATE:

# Default target forces to rebuild filesystem image.
all:	clobber .WAIT $(FS_ROOT)

clobber:	clean

clean:
	$(MKUFS) -d -f $(FS_ROOT)
	$(RM) -r $(FS_MNTPOINT)
	$(RM) $(FS_ROOT) $(MKROOT_ERR)

# Root filesystem
$(FS_ROOT):
	rdsize=`$(UTSTUNE) -v RAMDISK_ROOT_SIZE |			\
		$(PAWK) -F'[\(\)]' '/CURRENT VALUE/ {print $$2;}'`;	\
	$(MKUFS) -c -s "$$rdsize" -f $@ -p $(FS_MNTPOINT)
	$(MKROOT) $(FS_MNTPOINT) 2> $(MKROOT_ERR)
	@if $(MKROOT_PERMCHECK); then					\
	    cmd=`basename $(MKROOT)`;					\
	    echo "\n*** $$cmd failed.";					\
	    echo "*** See $(MKROOT_ERR) for details.\n";		\
	    exit 1;							\
	fi
	$(MKUFS) -d -f $@
