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
# Copyright (c) 2006-2008 NEC Corporation
#

include ../../../Makefile.master

HDRS	=			\
	archsystm.h		\
	asm_linkage.h		\
	bootconf.h		\
	bootregs.h		\
	bootsvcs.h		\
	cachectl.h		\
	cache_l220.h		\
	controlregs.h		\
	cpu.h			\
	ddi_isa.h		\
	fasttrap_isa.h		\
	fp.h			\
	frame.h			\
	gic.h			\
	inline.h		\
	kd.h			\
	kdi_machimpl.h		\
	machelf.h		\
	machlock.h		\
	machsig.h		\
	machtypes.h		\
	memtest.h		\
	mii.h			\
	miipriv.h		\
	mpcore.h		\
	mutex_impl.h		\
	obpdefs.h		\
	old_procfs.h		\
	pcb.h			\
	privregs.h		\
	procfs_isa.h		\
	prom_emul.h		\
	prom_isa.h		\
	prom_plat.h		\
	promif.h		\
	promimpl.h		\
	psw.h			\
	pte.h			\
	reg.h			\
	regset.h		\
	spl.h			\
	stack.h			\
	stat_impl.h		\
	synch32.h		\
	sysarm.h		\
	sysconfig_impl.h	\
	trap.h			\
	traptrace.h		\
	ucontext.h		\
	utrap.h			\
	vmparam.h		\
	x_call.h		\
	xc_levels.h

CLOSEDHDRS =

ROOTDIR=	$(ROOT)/usr/include/sys
SCSIDIR=	$(ROOTDIR)/scsi
SCSIDIRS=	$(SCSIDIR) $(SCSIDIR)/conf $(SCSIDIR)/generic \
		$(SCSIDIR)/impl $(SCSIDIR)/targets
DKTPDIR=	$(ROOTDIR)/dktp
DKTPDIRS=	$(DKTPDIR)
ROOTFSDIR=	$(ROOTDIR)/fs
ROOTDIRS=	$(ROOTDIR) $(ROOTFSDIR)

ROOTHDRS= $(HDRS:%=$(ROOTDIR)/%) $(SUBHDRS:%=$(ROOTDIR)/%)
$(CLOSED_BUILD)ROOTHDRS += $(CLOSEDHDRS:%=$(ROOTDIR)/%)

ROOTFSHDRS= $(FSHDRS:%=$(ROOTDIR)/fs/%)

CHECKHDRS= 			\
	$(HDRS:%.h=%.check)	\
	$(SUBHDRS:%.h=%.check)	\
	$(FSHDRS:%.h=fs/%.check)

$(CLOSED_BUILD)CHECKHDRS += $(CLOSEDHDRS:%.h=$(CLOSED)/uts/arm/sys/%.check)

# install rules
$(ROOTDIR)/%: %
	$(INS.file)

$(ROOTDIR)/%:	$(CLOSED)/uts/arm/sys/%
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS) $(ROOTFSHDRS)

install_h: $(ROOTDIRS) .WAIT $(ROOTHDRS) $(ROOTFSHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
