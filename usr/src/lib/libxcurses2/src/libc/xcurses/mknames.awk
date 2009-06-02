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
# Copyright (c) 1995-1998 by Sun Microsystems, Inc.
# All rights reserved.
#
# ident	"@(#)mknames.awk	1.6	05/06/10 SMI"
#
# mknames.awk	
#
# XCurses Library
#
# Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved.
#
#  rcsid = $Header: /team/ps/sun_xcurses/archive/local_changes/xcurses/src/lib/libxcurses/src/libc/xcurses/rcs/mknames.awk 1.3 1998/05/29 15:58:51 cbates Exp $
#

function header(file, array) {
print "/*" > file
print " * Copyright (c) 1998 by Sun Microsystems, Inc." > file
print " * All rights reserved." > file
print " */" > file
print > file
print "#pragma ident	\"@(#)" file "\t1.6\t05/06/10 SMI\"" > file
print > file
print "/*" > file
printf " * %s\n", file > file
print " *" > file
print " * XCurses Library" > file
print " *" > file
print " * **** THIS FILE IS MACHINE GENERATED." > file
print " * **** DO NOT EDIT THIS FILE." > file
print " *" > file
print " * Copyright 1990, 1995 by Mortice Kern Systems Inc.  All rights reserved." > file
print " *" > file
print " */" > file
print > file
printf "const char *%s[] = {\n", array > file
}

function tail(file) {
	printf("\t0\n};\n") >file;
	close(file);
}

BEGIN {
	header("boolname.c", "boolnames");
	header("boolcode.c", "boolcodes");
	header("boolfnam.c", "boolfnames");
	header("numname.c", "numnames");
	header("numcode.c", "numcodes");
	header("numfnam.c", "numfnames");
	header("strname.c", "strnames");
	header("strcode.c", "strcodes");
	header("strfnam.c", "strfnames");
}

$4 == "bool" {
	printf "\t\"%s\",\n", $1 > "boolfnam.c"
	printf "\t\"%s\",\n", $2 > "boolname.c"
	printf "\t\"%s\",\n", $3 > "boolcode.c"
}

$4 == "number" {
	printf "\t\"%s\",\n", $1 > "numfnam.c"
	printf "\t\"%s\",\n", $2 > "numname.c"
	printf "\t\"%s\",\n", $3 > "numcode.c"
}

$4 == "str" {
	if ($1 ~ /^key_/)
	{
		printf "\t\"%s\",\n", toupper($1) > "strfnam.c"
	}
	else
	{
		printf "\t\"%s\",\n", $1 > "strfnam.c"
	}
	printf "\t\"%s\",\n", $2 > "strname.c"
	printf "\t\"%s\",\n", $3 > "strcode.c"
}

END {
	tail("boolname.c");
	tail("boolcode.c");
	tail("boolfnam.c");
	tail("numname.c");
	tail("numcode.c");
	tail("numfnam.c");
	tail("strname.c");
	tail("strcode.c");
	tail("strfnam.c");
}
