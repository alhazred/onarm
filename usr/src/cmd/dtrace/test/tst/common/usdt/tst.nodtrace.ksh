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
# ident	"@(#)tst.nodtrace.ksh	1.2	06/09/26 SMI"

# Fake up a scenario where _DTRACE_VERSION is not defined by having our own
# <unistd.h>. This tests that dtrace -h will produce a header file which can
# be used on a system where DTrace is not present.

if [ $# != 1 ]; then
	echo expected one argument: '<'dtrace-path'>'
	exit 2
fi

dtrace=$1
DIR=/var/tmp/dtest.$$

mkdir $DIR
cd $DIR

touch unistd.h

cat > prov.d <<EOF
provider test_prov {
	probe go();
};
EOF

$dtrace -h -s prov.d
if [ $? -ne 0 ]; then
	print -u2 "failed to generate header file"
	exit 1
fi

cat > test.c <<EOF
#include "prov.h"

int
main(int argc, char **argv)
{
	TEST_PROV_GO();

	if (TEST_PROV_GO_ENABLED()) {
		TEST_PROV_GO();
	}

	return (0);
}
EOF

cc -I. -xarch=generic -c test.c
if [ $? -ne 0 ]; then
	print -u2 "failed to compile test.c"
	exit 1
fi
cc -xarch=generic -o test test.o
if [ $? -ne 0 ]; then
	print -u2 "failed to link final executable"
	exit 1
fi

./test
status=$?

cd /
/usr/bin/rm -rf $DIR

exit $status
