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
# Copyright (c) 2008-2009 NEC Corporation
# All rights reserved.
#

##
## Class for "device" block in built-in device configuration file.
##

package DevConf::Block::Device;

use strict;
use vars qw(@ISA %PARAMETER);

use UtsTune::Block;
use DevConf::Constants;
use DevConf::Properties;
use DevConf::Type::CppInt;

@ISA = qw(UtsTune::Block);

# Valid parameter names.
# Note that not all device property format are defined in %PARAMETER.
%PARAMETER = (
	interrupts	=> {type => 'DevConf::Type::CppInt', list => 1},
	cpp_if		=> {type => 'UtsTune::Type::String'},
);

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($name, $devidx) = @_;

	my $me = $class->SUPER::new(undef, \%PARAMETER);
	$me->{REGS} = [];
	$me->{UART_PORTS} = [];
	$me->{NAME} = $name;
	$me->{INDEX} = $devidx;
	$me->{PROP} = DevConf::Properties->new($devidx);
	$me->{OBJREF} = {};

	return bless $me, $class;
}

sub addIntProperty
{
	my $me = shift;
	my ($parser, $name, $values) = @_;

	my $prop = $me->{PROP};
	$prop->addIntProperty($parser, $name, $values);
}

sub addInt64Property
{
	my $me = shift;
	my ($parser, $name, $values) = @_;

	my $prop = $me->{PROP};
	$prop->addInt64Property($parser, $name, $values);
}

sub addStringProperty
{
	my $me = shift;
	my ($parser, $name, $values) = @_;

	my $prop = $me->{PROP};
	$prop->addStringProperty($parser, $name, $values);
}

sub addByteProperty
{
	my $me = shift;
	my ($parser, $name, $values) = @_;

	my $prop = $me->{PROP};
	$prop->addByteProperty($parser, $name, $values);
}

sub addBoolProperty
{
	my $me = shift;
	my ($parser, $name) = @_;

	my $prop = $me->{PROP};
	$prop->addBoolProperty($parser, $name);
}

sub setRegisters
{
	my $me = shift;
	my ($parser, $regs) = @_;

	$me->{REGS} = $regs;

	# Set "regs" property.
	my (@array);
	foreach my $r (@$regs) {
		push(@array, @{$r->getIntArray()});
	}

	my $qname = $parser->stringfy(PROP_REG);
	$me->addIntProperty($parser, $qname, \@array);
}

sub addParameter
{
	my $me = shift;
	my ($parser, $pname, $params) = @_;

	$me->SUPER::addParameter($parser, $pname, $params);

	if ($pname eq 'cpp_if') {
		my $a = $params->[0];
		my ($token, $type) = (@$a);
		my $str = $parser->evalString($token);
		$parser->parseError("\"cpp_if\" must not be an empty string")
			if (isEmptyString($str));
		$me->{CPP_COND} = $str;
	}
	elsif ($pname eq PROP_INTERRUPTS) {
		# Set "interrupts" property.
		my @array;
		foreach my $a (@$params) {
			my $t = DevConf::Type::CppInt->new(@$a);
			push(@array, $t);
		}
		my $qname = $parser->stringfy(PROP_INTERRUPTS);
		$me->addIntProperty($parser, $qname, \@array);
	}
}

sub fixup
{
	my $me = shift;
	my ($parser) = @_;

	$me->SUPER::fixup($parser);
	my (@ports);
	foreach my $r (@{$me->getRegisters()}) {
		if ($r->isUartPort()) {
			my $addr = $r->getAddress();
			push(@ports, $addr->dumpValue());
		}
	}
	$me->{UART_PORTS} = \@ports;
}

sub cppCondition
{
	my $me = shift;

	return $me->{CPP_COND};
}

sub dumpForward
{
	my $me = shift;
	my ($out, $parser) = @_;

	# Dump property definitions.
	my $cond = $me->cppCondition();
	my $prop = $me->{PROP};

	return $prop->output($out, $parser, $cond);
}

sub getName
{
	my $me = shift;

	my $name = $me->{NAME};
	return $name->dumpValue();
}

sub getRegisters
{
	my $me = shift;

	my $regs = $me->{REGS};
	return (wantarray) ? @$regs : $regs;
}

sub isUart
{
	my $me = shift;

	my $ports = $me->{UART_PORTS};
	return (@$ports != 0);
}

sub getUartPorts
{
	my $me = shift;

	my $ports = $me->{UART_PORTS};
	return (wantarray) ? @$ports : $ports;
}

sub addObjectReference
{
	my $me = shift;
	my ($name) = @_;

	$me->{OBJREF}->{$name} = 1;
}

sub getAllObjectReferences
{
	my $me = shift;

	my (@ret) = keys(%{$me->{OBJREF}});

	return (wantarray) ? @ret : \@ret;
}

sub checkValueImpl;
sub checkValue;

1;
