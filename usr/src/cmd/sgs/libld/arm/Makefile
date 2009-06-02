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
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

#
# Copyright (c) 2007-2008 NEC Corporation
#

# ident	"@(#)Makefile	1.27	06/03/07 SMI"
#

E_TOOLOBJS =	leb128.o
L_MACHOBJS32 =	machrel.arm32.o	machsym.arm32.o attr.arm32.o misc.arm32.o
L_MACHOBJS64 =	

include		../Makefile.com

.KEEP_STATE:

SGSMSGCHK =	../common/libld.chk.msg
SGSMSGTARG +=	$(SGSMSGARM)

COMOBJS64 =
$(__GNULD)ZDEFS =

MAPFILES	= mapfile-vers
VERS_SCRIPT	= vers-script

#
# For cross-compilation, it is necessary to trigger the correct include files
# (see sys/elf.h).
#
ELFTARGET64 =	
ELFTARGET32 =	-DELF_TARGET_ARM

all:		$(DYNLIB) $(LIBLINKS)

install \
package:	all $(ROOTFS_DYNLIB)

include		../Makefile.targ

# Associate ELF32 and ELF64 objects to the appropriate headers.

pics/%32.o :=	CPPFLAGS += -I$(SRCBASE)/uts/$(VAR_PLAT_arm)/krtld
pics/%64.o :=	CPPFLAGS += -I$(SRCBASE)/uts/$(VAR_PLAT_arm)/krtld

# Associate the various lint targets with the appropriate headers/files.

$(LINTOUT32) :=	CPPFLAGS += -I$(SRCBASE)/uts/$(VAR_PLAT_arm)/krtld \
		    $(ELFTARGET32)
$(LINTOUT64) :=	CPPFLAGS += -I$(SRCBASE)/uts/$(VAR_PLAT_arm)/krtld \
		    $(ELFTARGET64) -D_ELF64
$(LINTLIB32) :=	CPPFLAGS += -I$(SRCBASE)/uts/$(VAR_PLAT_arm)/krtld \
		    $(ELFTARGET32)
$(LINTLIB64) :=	CPPFLAGS += -I$(SRCBASE)/uts/$(VAR_PLAT_arm)/krtld \
		    $(ELFTARGET64) -D_ELF64

LINTSRCS32 +=	$(G_MACHOBJS32:%32.o=$(SRCBASE)/uts/$(VAR_PLAT_arm)/krtld/%.c)
LINTSRCS64 +=	$(G_MACHOBJS64:%64.o=$(SRCBASE)/uts/$(VAR_PLAT_arm)/krtld/%.c)

# Compensate chkmsg with the doreloc family.

CHKSRCS +=	$(G_MACHOBJS32:%32.o=$(SRCBASE)/uts/$(VAR_PLAT_arm)/krtld/%.c)
CHKSRCS +=	$(G_MACHOBJS64:%64.o=$(SRCBASE)/uts/$(VAR_PLAT_arm)/krtld/%.c)

pics/%32.o: \
		$(SRCBASE)/uts/$(VAR_PLAT_arm)/krtld/%.c
		$(COMPILE.c) -o $@ $(ELFTARGET32) $<
		$(POST_PROCESS_O)

pics/%64.o: \
		$(SRCBASE)/uts/$(VAR_PLAT_arm)/krtld/%.c
		$(COMPILE.c) -o $@ $(ELFTARGET64) -D_ELF64 $<
		$(POST_PROCESS_O)
