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
# Copyright (c) 2007 NEC Corporation
# All rights reserved.
#

##
## Base class for parser classes.
##

package UtsTune::Parser;

use strict;

use constant	TOKEN_VALUE	=> 0x8000;
use constant	SEMI		=> 1;
use constant	LBRACE		=> 2;
use constant	RBRACE		=> 3;
use constant	COLON		=> 4;
use constant	COMMA		=> 5;
use constant	STRING		=> (6 | TOKEN_VALUE);
use constant	SYMBOL		=> (7 | TOKEN_VALUE);
use constant	INT		=> (8 | TOKEN_VALUE);
use constant	NEGATIVE	=> (9 | TOKEN_VALUE);
use constant	TRUE		=> (10 | TOKEN_VALUE);
use constant	FALSE		=> (11 | TOKEN_VALUE);

sub evalString
{
	my $me = shift;
	my ($str) = @_;

	my $s = eval "return $str";
	if ($@) {
		my $err = "$@";
		chomp($err);
		die "Invalid string token: $str\n";
	}

	return $s;
}

sub stringfy
{
	my $me = shift;
	my ($str) = @_;

	$str =~ s/\x22/\\\x22/go;
	return '"' . $str . '"';
}

1;
