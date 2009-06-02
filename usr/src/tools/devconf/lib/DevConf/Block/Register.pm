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
## Class for "register" block in built-in device configuration file.
## This class also represents one "struct regspec" instance.
##

package DevConf::Block::Register;

use strict;
use vars qw(@ISA %PARAMETER);

use UtsTune;
use UtsTune::Block;
use UtsTune::Parser;
use DevConf::Type::CppInt;

@ISA = qw(UtsTune::Block);

# Valid parameter names.
%PARAMETER = (
	address		=> {mandatory => 1},
	size		=> {mandatory => 1},
	bus_type	=> {},
	uart_port	=> {type => 'UtsTune::Type::Bool'},
);

# Default bus type value.
use constant	DEFAULT_BUSTYPE		=> 1;

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my $param = shift || \%PARAMETER;

	my $type = DevConf::Type::CppInt->new();
	my $me = $class->SUPER::new($type, $param);
	$me->{PROPS} = {};

	return bless $me, $class;
}

sub addParameter
{
	my $me = shift;
	my ($parser, $pname, $params) = @_;

	$me->SUPER::addParameter($parser, $pname, $params);
	unless ($pname eq 'uart_port') {
		my $type = $me->getType();
		my $a  = $params->[0];
		my $t = $type->new(@$a);
		$me->{PROPS}->{$pname} = $t;
	}
}

sub getAddress
{
	my $me = shift;

	return $me->{PROPS}->{address};
}

sub getSize
{
	my $me = shift;

	return $me->{PROPS}->{size};
}

sub getBusType
{
	my $me = shift;

	my $bt = $me->{PROPS}->{bus_type};
	$bt = DevConf::Type::CppInt->new(DEFAULT_BUSTYPE,
					 UtsTune::Parser::INT)
		unless (defined($bt));

	return $bt;
}

sub isUartPort
{
	my $me = shift;

	return $me->getBool('uart_port');
}

sub getIntArray
{
	my $me = shift;

	# This code assumes that struct regspec is defined as follows:
	#
	# struct regspec {
	#         uint_t regspec_bustype;
	#         uint_t regspec_addr;
	#         uint_t regspec_size;
	# };
	my (@array) = ($me->getBusType(), $me->getAddress(), $me->getSize());

	return (wantarray) ? @array : \@array;
}

sub checkValueImpl;
sub checkValue;

1;
