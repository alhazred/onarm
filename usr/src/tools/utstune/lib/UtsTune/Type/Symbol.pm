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
# Copyright (c) 2008 NEC Corporation
# All rights reserved.
#

##
## Abstract type for symbol value.
##

package UtsTune::Type::Symbol;

use strict;
use vars qw(@ISA);

use UtsTune::Type;

@ISA = qw(UtsTune::Type);

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($value, $type, $symbols) = @_;

	my $me = $class->SUPER::new($value);
	$me->{_SYM_TYPE} = $type || 'symbol';
	$me->{_SYM_SYMBOLS} = $symbols;

	return bless $me;
}

sub getType
{
	my $me = shift;

	return $me->{_SYM_TYPE};
}

sub checkValue
{
	my $me = shift;
	my ($value, $type) = @_;

	my $syms = $me->{_SYM_SYMBOLS};
	if ($type != $me->SYMBOL or
	    (defined($syms) and !$syms->{$value})) {
		my $type = $me->getType();
		return "Invalid value for \"$type\": $value";
	}
	return undef;
}

sub valueFromArgument
{
	my $me = shift;
	my ($arg) = @_;

	return ($arg, $me->SYMBOL);
}

sub isTrue
{
	return 1;
}

1;
