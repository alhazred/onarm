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
# Copyright (c) 2009 NEC Corporation
# All rights reserved.
#

##
## Class for "defobj" block, which defines C object, in built-in device
## configuration file.
##

package DevConf::Block::DefObj;

use strict;
use vars qw(@ISA %PARAMETER);

use UtsTune::Block;
use DevConf::Constants;
use DevConf::Properties;

@ISA = qw(UtsTune::Block);

use constant	PARAM_ARRAY	=> 'array';

# Valid parameter names.
%PARAMETER = (
	type		=> {type => 'UtsTune::Type::String', mandatory => 1},
	value		=> {type => 'UtsTune::Type::String', mandatory => 1},
	PARAM_ARRAY()	=> {type => 'UtsTune::Type::Bool'},
);

use constant	PARAM_KEY	=> {type => 'OBJTYPE', value => 'OBJVALUE'};

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($name) = @_;

	my $me = $class->SUPER::new(undef, \%PARAMETER);
	$me->{NAME} = $name;
	$me->{CPP_COND} = [];

	return bless $me, $class;
}

sub addParameter
{
	my $me = shift;
	my ($parser, $pname, $params) = @_;

	$me->SUPER::addParameter($parser, $pname, $params);

	if (my $key = PARAM_KEY->{$pname}) {
		my $a = $params->[0];
		my ($token, $type) = (@$a);
		my $str = $parser->evalString($token);
		$parser->parseError("\"$pname\" must not be an empty string")
			if (isEmptyString($str));
		$me->{$key} = $str;
	}
}

sub getCppCondition
{
	my $me = shift;

	my $ret = $me->{CPP_COND};
	return (wantarray) ? @$ret : $ret;
}

sub addCppCondition
{
	my $me = shift;
	my ($cond) = @_;

	# If no condition is specified, this object must be always defined.
	$cond = 1 unless ($cond);
	push(@{$me->{CPP_COND}}, $cond);
}

sub getName
{
	my $me = shift;

	return $me->{NAME};
}

sub getObjectType
{
	my $me = shift;

	return $me->{OBJTYPE};
}

sub getObjectValue
{
	my $me = shift;

	return $me->{OBJVALUE};
}

sub isArray
{
	my $me = shift;

	return $me->getBool(PARAM_ARRAY);
}

sub output
{
	my $me = shift;
	my ($out) = @_;

	my $cpp = $me->getCppCondition();
	my $array = $me->isArray();
	my $cond;
	if ($cpp and @$cpp) {
		$cond = '(' . join(') || (', @$cpp) . ')';
		$out->print("#if\t$cond\n");
	}

	my $type = $me->getObjectType();
	my $value = $me->getObjectValue();
	my $name = $me->getName();
	my $sym = SYM_OBJECT_PREFIX . $name;
	$out->printf("static const %s %s%s = %s;\n",
		     $type, $sym, ($array) ? '[]' : '', $value);
	$out->print("#endif\t/* $cond */\n") if ($cond);
}

sub checkValueImpl;
sub checkValue;

1;
