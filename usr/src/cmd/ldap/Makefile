#
# Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#pragma ident	"%Z%%M%	%I%	%E% SMI"
#
# cmd/ldap/Makefile

include ../Makefile.cmd

SUBDIRS=	$(MACH)	
POFILE=		ldapcmd.po
NS_FILES=	ldapclient ldaplist mapping printResult ldapaddent ldapaddrbac
COMM_FILES=	ldapsearch ldapmodify ldapmodrdn ldapdelete common  \
		fileurl ldaptool-sasl
POFILES=	$(NS_FILES:%=ns_ldap/%.po) $(COMM_FILES:%=common/%.po)

all :=		TARGET= all
clean :=	TARGET= clean
clobber :=	TARGET= clobber
delete :=	TARGET= delete
install :=	TARGET= install
lint :=		TARGET= lint
catalog :=	TARGET= catalog
_msg :=		TARGET= _msg
package :=	TARGET= package
$(POFILES) :=	CFLAGS += -I ../../lib/libldap5/include/ldap \
			-I ../../lib/libsldap/common \
			-I ../../lib/libnsl/include/rpcsvc \
                	-DNO_LIBLCACHE -DLDAP_REFERRALS -DNET_SSL \
			-DLDAPSSLIO -DHAVE_SASL_OPTIONS \
			-DSOLARIS_LDAP_CMD

.KEEP_STATE:

all clobber delete install lint catalog package: $(SUBDIRS)

clean:	$(SUBDIRS)
	$(RM) $(POFILES) $(POFILE)

# install rule for install_h target

# install: $(SUBDIRS)
# 	-$(RM) $(ROOTUSRBIN)

check: $(CHECKHDRS)
	
$(MACH) :	FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

$(POFILE): $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@

FRC:

include ../Makefile.targ
