divert(-1)
#
# Copyright (c) 1998, 1999, 2001-2002 Sendmail, Inc. and its suppliers.
#	All rights reserved.
# Copyright (c) 1983 Eric P. Allman.  All rights reserved.
# Copyright (c) 1988, 1993
#	The Regents of the University of California.  All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
# ident	"@(#)genericstable.m4	1.6	04/06/21 SMI"
#

divert(0)
VERSIONID(`$Id: genericstable.m4,v 8.23 2002/06/27 23:23:57 gshapiro Exp $')
divert(-1)

define(`_GENERICS_TABLE_', `')

LOCAL_CONFIG
# Generics table (mapping outgoing addresses)
Kgenerics ifelse(defn(`_ARG_'), `', DATABASE_MAP_TYPE MAIL_SETTINGS_DIR`genericstable',
		 defn(`_ARG_'), `LDAP', `ldap -1 -v sendmailMTAMapValue,sendmailMTAMapSearch:FILTER:sendmailMTAMapObject,sendmailMTAMapURL:URL:sendmailMTAMapObject -k (&(objectClass=sendmailMTAMapObject)(|(sendmailMTACluster=${sendmailMTACluster})(sendmailMTAHost=$j))(sendmailMTAMapName=generics)(sendmailMTAKey=%0))',
		 `_ARG_')