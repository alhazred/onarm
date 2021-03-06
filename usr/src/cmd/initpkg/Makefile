#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License, Version 1.0 only
# (the "License").  You may not use this file except in compliance
# with the License.
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
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

#
# Copyright (c) 2007-2008 NEC Corporation
#

#ident	"%Z%%M%	%I%	%E% SMI"
#

SHFILES=	dfstab vfstab
CPFILES=	rcS rc0 rc1 rc2 rc3 mountall shutdown swapadd umountall
ALL=		$(SHFILES) $(CPFILES)
TXTS=		rcS.d/mk.rcS.d.sh rc0.d/mk.rc0.d.sh \
		rc1.d/mk.rc1.d.sh rc2.d/mk.rc2.d.sh rc3.d/mk.rc3.d.sh
CLOBBERFILES=	$(ALL)
SUBDIRS=	rcS.d rc0.d rc1.d rc2.d rc3.d

include ../Makefile.cmd

$(ARM_BLD)SHFILES	+= vfstab_ufs_ata vfstab_ufs_sata

ETCINITD=	$(ROOTETC)/init.d
ETCDFSD=	$(ROOTETC)/dfs
DIRS=		$(ETCINITD) $(ETCDFSD) $(ROOTETC)/security

SBINF=		rcS mountall rc0 rc1 rc2 rc3 swapadd umountall
$(ARM_BLD)SBINF=	mountall umountall
SBINL=		rc5 rc6
USRSBINF=	mountall shutdown umountall

sparc_ETCTABS=
i386_ETCTABS=	bootrc
ETCTABS=	vfstab inittab nscd.conf security/crypt.conf $($(MACH)_ETCTABS)
$(ARM_BLD)ETCTABS=	vfstab security/crypt.conf vfstab_ufs_ata vfstab_ufs_sata

DFSTAB=		dfstab
SBINETC=	rcS mountall rc0 rc1 rc2 rc3 rc5 rc6 swapadd umountall
USRSBINETC=	shutdown

OWNER=		root
GROUP=		sys
FILEMODE=	0744

ROOTSBINF=	$(SBINF:%=$(ROOTSBIN)/%)
ROOTSBINL=	$(SBINL:%=$(ROOTSBIN)/%)
ROOTUSRSBINF=	$(USRSBINF:%=$(ROOTUSRSBIN)/%)
ROOTETCTABS=	$(ETCTABS:%=$(ROOTETC)/%)
ROOTDFSTAB=	$(DFSTAB:%=$(ETCDFSD)/%)
SYMSBINF=	$(SBINETC:%=$(ROOTETC)/%)
SYMUSRSBINF=	$(USRSBINETC:%=$(ROOTETC)/%)

$(ROOTETC)/inittab		:= FILEMODE =	0644
$(ROOTETC)/vfstab		:= FILEMODE =	0644
$(ROOTETC)/vfstab_ufs_ata	:= FILEMODE =	0644
$(ROOTETC)/vfstab_ufs_sata	:= FILEMODE =	0644
$(ROOTETC)/nscd.conf		:= FILEMODE =	0644
$(ROOTETC)/security/crypt.conf	:= FILEMODE =	0644
$(ROOTETC)/bootrc		:= FILEMODE =	0755
$(ROOTDFSTAB)			:= FILEMODE =	0644
$(ROOTSBIN)/mountall		:= FILEMODE =	0555
$(ROOTUSRSBIN)/mountall		:= FILEMODE =	0555
$(ROOTSBIN)/umountall		:= FILEMODE =	0555
$(ROOTUSRSBIN)/umountall	:= FILEMODE =	0555
$(ROOTUSRSBIN)/shutdown		:= FILEMODE =	0755

$(ETCDFSD)/% : %
	$(INS.file)

.KEEP_STATE:

all: $(ALL) all_init.d $(TXTS)

$(SYMSBINF):
	$(RM) $@; $(SYMLINK) ../sbin/$(@F) $@

$(SYMUSRSBINF):
	$(RM) $@; $(SYMLINK) ../usr/sbin/$(@F) $@

$(ROOTSBINL):	$(ROOTSBIN)/rc0
	$(RM) $@; $(LN) $(ROOTSBIN)/rc0 $@

all_init.d: FRC
	@cd init.d; pwd; $(MAKE) $(MFLAGS) all

ins_init.d: FRC
	@cd init.d; pwd; $(MAKE) $(MFLAGS) install

$(SHFILES):
	sh $@.sh $(ROOT)

INSTALL_TARGETS			= $(ALL) ins_all ins_init.d $(SUBDIRS)
$(ARM_BLD)INSTALL_TARGETS	= $(ALL) ins_all_arm $(MACH)

install:	$(INSTALL_TARGETS)

ins_all : $(ROOTSBINF) $(ROOTSBINL) $(ROOTUSRSBINF) $(ROOTETCTABS) \
	$(ROOTDFSTAB)  $(SYMSBINF) $(SYMUSRSBINF)

ins_all_arm:	$(ROOTSBINF) $(ROOTUSRSBINF) $(ROOTETCTABS) $(ROOTDFSTAB)

# Don't re-install directories already installed by Targetdirs
#$(DIRS):
#	$(INS.dir)

$(SUBDIRS):	FRC
	@cd $@; pwd; ROOT=$(ROOT) CH=$(CH) sh mk.$@.sh

$(ARM_BLD)$(MACH):	FRC
$(ARM_BLD)	@cd $@; pwd; $(MAKE) $(MFLAGS) install

FRC:

clean lint:

include	../Makefile.targ
