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
## Boolean type
##

package UtsTune::Type::Bool;

use strict;
use vars qw(@ISA);

use UtsTune::Type;

@ISA = qw(UtsTune::Type);

sub getType
{
	return 'boolean';
}

sub checkValue
{
	my $me = shift;
	my ($value, $type) = @_;

	return "Invalid value for boolean: $value"
		unless ($type == $me->TRUE or $type == $me->FALSE);

	return undef;
}

sub checkCandidates
{
	my $me = shift;
	my ($value, $cds) = @_;

	return 1 unless (defined($cds));

	my $type = $me->getType();
	die "\"candidates\" is not supported for $type type.\n";
}

sub compare
{
	my $me = shift;
	my ($v1, $v2) = @_;

	my $val1 = ($v1 eq 'true') ? 1 : 0;
	my $val2 = ($v2 eq 'true') ? 1 : 0;

	return $val1 <=> $val2;
}

sub dumpMacro
{
	my $me = shift;
	my ($fh, $name, $value) = @_;

	if ($value eq 'true') {
		$fh->print(<<OUT);
#define	$name	1
OUT
	}
	else {
		$fh->print(<<OUT);
#undef	$name
OUT
	}
}

sub valueFromArgument
{
	my $me = shift;
	my ($arg) = @_;

	my $type = ($arg eq 'true') ? $me->TRUE
		: ($arg eq 'false') ? $me->FALSE
		: $me->SYMBOL;

	return ($arg, $type);
}

sub isTrue
{
	my $me = shift;
	my ($value) = @_;

	return ($value eq 'true') ? 1 : undef;
}

1;
