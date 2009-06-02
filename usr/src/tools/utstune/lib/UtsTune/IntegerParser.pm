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
## Parse string representation of integer.
##

package UtsTune::IntegerParser;

use strict;
use vars qw(@ISA);

use UtsTune::AbstractIntegerParser;

@ISA = qw(UtsTune::AbstractIntegerParser);

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($str) = @_;

	my $len = length($str);
	die "Integer can't be a empty string.\n" if ($len == 0);
	my $me = {STR => $str, INDEX => 0, LEN => $len};
	return bless $me, $class;
}

sub getChar
{
	my $me = shift;

	my $index = $me->{INDEX};
	my $len = $me->{LEN};
	return undef if ($index >= $len);

	my $c = substr($me->{STR}, $index, 1);
	$me->{INDEX} = $index + 1;
	return $c;
}

sub ungetChar
{
	my $me = shift;

	my $index = $me->{INDEX};
	$me->{INDEX} = $index - 1 if ($index);
}

sub parseError
{
	my $me = shift;

	my $msg = join('', @_);
	die "$msg\n";
}

sub parse
{
	my $me = shift;

	my $c = $me->getChar();
	return $me->parseInt($c);
}

1;
