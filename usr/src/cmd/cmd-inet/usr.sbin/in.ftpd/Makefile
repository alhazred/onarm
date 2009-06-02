#
# ident	"@(#)Makefile	1.43	07/05/14 SMI"
#
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#
# Copyright (c) 2007 NEC Corporation
#

PROG=		in.ftpd ftpcount ftpshut ftprestart privatepw
SCRIPTS=	ftpaddhost ftpconfig
MANIFEST=	ftp.xml

include		../../../Makefile.cmd

COMMON_OBJS=	COPYRIGHT.o vers.o
FTPD_OBJS=	$(COMMON_OBJS) ftpd.o ftpcmd.o glob.o logwtmp.o popen.o \
		access.o extensions.o realpath.o acl.o private.o \
		authenticate.o conversions.o rdservers.o paths.o hostacc.o \
		routevector.o restrict.o domain.o wu_fnmatch.o timeout.o \
		getpwnam.o strsep.o strcasestr.o inet.o xferlog.o gssutil.o \
		privs.o
FTPCOUNT_OBJS=	$(COMMON_OBJS) ftpcount.o rdservers.o inet.o
FTPSHUT_OBJS=	$(COMMON_OBJS) ftpshut.o rdservers.o inet.o
FTPREST_OBJS=	$(COMMON_OBJS) ftprestart.o rdservers.o inet.o
CKCONFIG_OBJS=	$(COMMON_OBJS) ckconfig.o rdservers.o inet.o
PRIVATE_OBJS=	$(COMMON_OBJS) privatepw.o
OBJS=		$(FTPD_OBJS) ftpcount.o ftpshut.o ftprestart.o ckconfig.o \
		privatepw.o
SRCS=		$(OBJS:%.o=%.c)
CONFIGFILES=	ftpaccess ftpconversions ftpgroups ftphosts ftpservers ftpusers
ETCFTPDDIR=	$(ROOTETC)/ftpd
ETCFTPDFILES=	$(CONFIGFILES:%=$(ETCFTPDDIR)/%)
$(ETCFTPDFILES):=	FILEMODE= 0644
$(ETCFTPDFILES):=	GROUP= sys
ROOTUSRSBINSCRIPTS=	$(SCRIPTS:%=$(ROOTUSRSBIN)/%)
ROOTFTPCOUNT=	$(ROOTUSRSBIN)/ftpcount
ROOTFTPWHO=	$(ROOTUSRSBIN)/ftpwho

ROOTMANIFESTDIR=	$(ROOTSVCNETWORK)

# I18n
POFILE=		in.ftpd_all.po
POFILES=	$(SCRIPTS:%=%.po)

# When building for Solaris 8 add to CPPFLAGS:
# -Ddn_skipname=__dn_skipname -DSOLARIS_NO_AUDIT_FTPD_LOGOUT
CLOBBERFILES +=	ckconfig ftpwho $(SCRIPTS)
CPPFLAGS +=	-DBSD_COMP -D_FILE_OFFSET_BITS=64 -DINET6 -DSOLARIS_BSM_AUDIT \
		-DSOLARIS_ETC_FTPUSERS -DSENDFILE -DCLOSEFROM \
		-DSOLARIS_GSS_USEROK -DSOLARIS_PRIVS \
		-DDAEMON
LDLIBS +=	-lpam -lbsm -lsocket -lnsl -lsendfile
$(__ARLIB)$(ARM_BLD)LDLIBS +=	-ltsol -lsecdb -lnsl -lmd -lcmd
YFLAGS +=	-d

in.ftpd :=	LDFLAGS += $(MAPFILE.NGB:%=-M%)

# tcov output relies on atexit(3C) registered functions being called, so stop
# _exit() from being used. Passing -l to yacc stops it generating #line
# directives which don't work with tcov.
tcov :=		CFLAGS += -xa
tcov :=		CPPFLAGS += -D_exit=exit
tcov :=		YFLAGS += -l

.KEEP_STATE:

all tcov:	$(PROG) ckconfig ftpwho $(CONFIGFILES) $(SCRIPTS)

in.ftpd:	$(FTPD_OBJS) $(MAPFILE.NGB)
		$(LINK.c) $(FTPD_OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

ftpcount:	$(FTPCOUNT_OBJS)
#		$(LINK.c) $(FTPCOUNT_OBJS) -o $@ -lsocket -lnsl
		$(LINK.c) $(FTPCOUNT_OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

ftpwho:		ftpcount
		$(RM) $@
		$(LN) ftpcount $@

ftpshut:	$(FTPSHUT_OBJS)
#		$(LINK.c) $(FTPSHUT_OBJS) -o $@ -lsocket -lnsl
		$(LINK.c) $(FTPSHUT_OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

ftprestart:	$(FTPREST_OBJS)
#		$(LINK.c) $(FTPREST_OBJS) -o $@ -lsocket -lnsl
		$(LINK.c) $(FTPREST_OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

ckconfig:	$(CKCONFIG_OBJS)
#		$(LINK.c) $(CKCONFIG_OBJS) -o $@ -lsocket -lnsl
		$(LINK.c) $(CKCONFIG_OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

privatepw:	$(PRIVATE_OBJS)
#		$(LINK.c) $(PRIVATE_OBJS) -o $@
		$(LINK.c) $(PRIVATE_OBJS) -o $@ $(LDLIBS)
		$(POST_PROCESS)

# This causes y.tab.c to be renamed to ftpcmd.c, needed by tcov.
ftpcmd.c:	ftpcmd.y

$(ETCFTPDDIR)/%: %
		$(INS.file)

$(POFILE):	$(POFILES)
		$(RM) $@
		cat $(POFILES) >$@

$(ROOTFTPWHO):	$(ROOTFTPCOUNT)
		$(RM) $@
		$(LN) $(ROOTFTPCOUNT) $@

install:	all $(ROOTUSRSBINPROG) $(ROOTFTPWHO) $(ROOTUSRSBINSCRIPTS) \
		$(ETCFTPDFILES) $(ROOTMANIFEST)

check:		$(CHKMANIFEST)

clean:
		$(RM) $(OBJS) *.d *.tcov y.tab.h y.tab.c ftpcmd.c

lint:		lint_SRCS

include		../../../Makefile.targ
