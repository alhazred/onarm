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
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"%Z%%M%	%I%	%E% SMI"
#
# cmd/power/Makefile
#

DAEMON_SRCS =	powerd.c sysstat.c
DAEMON_OBJS =	$(DAEMON_SRCS:%.c=%.o)
DAEMON =	powerd
PMCFG_SRCS = 	conf.c parse.c handlers.c
PMCFG_OBJS =	$(PMCFG_SRCS:%.c=%.o)
PMCFG = 	pmconfig
SRCS = 		$(DAEMON_SRCS) $(PMCFG_SRCS)
OBJS = 		$(SRCS:%.c=%.o)
SCRIPTS =	sysidpm.sh
SYSIDPM = 	sysidpm
PROG =		$(DAEMON) $(PMCFG) $(SYSIDPM)
POWERCONF=	power.conf
ETCFILES =	$(POWERCONF)
POWERPERM =	power
DEFAULTFILES =	power.dfl

MANIFEST=	power.xml
SVCMETHOD=	svc-power

include ../Makefile.cmd

ROOTMANIFESTDIR =	$(ROOTSVCSYSTEM)

TEXT_DOMAIN=	SUNW_OST_OSCMD

XGETFLAGS +=    -a -x power_all.xcl
POFILE=		power_all.po
POFILES=	$(PMCFG_SRCS:%.c=%.po)
# needed because POFILES is deleted by clobber, but it's under
# SCCS control here.
OPOFILES=	sysidpm.po

# pmconfig only needs libdevinfo on sparc
sparc_LDEVINFO=	-ldevinfo
i386_LDEVINFO=

LDEVINFO=	-ldevinfo

DAEMON_LDLIBS =	$(LDLIBS.cmd) -lkstat $(LDEVINFO)
PMCFG_LDLIBS =	$(LDLIBS.cmd) -lcmd -lsmbios -lkstat $($(MACH)_LDEVINFO)

OWNER= root
ROOTUSRSBINPMCFG= $(PMCFG:%=$(ROOTUSRSBIN)/%)
$(ROOTUSRSBINPMCFG) := GROUP= bin
$(ROOTUSRSBINPMCFG) := FILEMODE= 4555

GROUP= sys
ROOTLIBPOWER= $(ROOTLIB)/power
ROOTLIBPOWERDAEMON= $(DAEMON:%=$(ROOTLIBPOWER)/%)
$(ROOTLIBPOWER) := GROUP= sys
$(ROOTLIBPOWER) := FILEMODE= 755
$(ROOTLIBPOWERDAEMON) := GROUP= bin
$(ROOTLIBPOWERDAEMON) := FILEMODE= 555

ROOTETCFILES= $(ETCFILES:%=$(ROOTETC)/%) 
$(ROOTETCFILES) := FILEMODE= 644 

ROOTUSRSBINSYSIDPM= $(SYSIDPM:%=$(ROOTUSRSBIN)/%)
$(ROOTUSRSBINSYSIDPM) := FILEMODE= 755

#
# lint pass one enforcement
#
CFLAGS += $(CCVERBOSE)

.PARALLEL: $(OBJS)

.KEEP_STATE:

all: $(PROG) $(POWERPERM).dfl $(ETCFILES) $(SCRIPTS)

install clean:

$(POWERCONF): $(POWERCONF).$(MACH)

$(DAEMON_OBJS): $(DAEMON_SRCS)
	$(CC) $(CFLAGS) -D_REENTRANT $(CPPFLAGS) -o $@ -c $<
	$(PROCESS_COMMENT) $@

$(DAEMON): $(DAEMON_OBJS)
	$(CC) -o $@ $(DAEMON_OBJS) $(LDFLAGS) $(DAEMON_LDLIBS)
	$(POST_PROCESS)

$(PMCFG_OBJS): pmconfig.h

$(PMCFG): $(PMCFG_OBJS)
	$(LINK.c) -o $@ $(PMCFG_OBJS) $(PMCFG_LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTUSRSBINPMCFG) $(ROOTLIBPOWERDAEMON) \
	$(ROOTETCFILES) $(ROOTUSRSBINSYSIDPM) $(ROOTETCDEFAULTFILES) \
	$(ROOTMANIFEST) $(ROOTSVCMETHOD)

$(ROOTLIBPOWER):
	$(INS.dir)

$(ROOTLIBPOWER)/%:	%
	$(INS.file)

$(ROOTLIBPOWERDAEMON): $(ROOTLIBPOWER)

$(OPOFILES):
	@

$(POFILE):	$(POFILES) $(OPOFILES)
	$(RM)	$@
	cat	$(POFILES) $(OPOFILES)	> $@

check:	$(CHKMANIFEST)

clean:
	$(RM) $(OBJS) $(SYSIDPM) $(POWERCONF)
	$(RM) $(POFILE) $(POFILES)

lint := LINTFLAGS=-auxn
lint:
	$(LINT.c) $(DAEMON_SRCS)
	$(LINT.c) $(PMCFG_SRCS)

cstyle:
	$(CSTYLE) $(SRCS)

%:	%.$(MACH)
	$(RM) $@
	cat $< > $@

include ../Makefile.targ
