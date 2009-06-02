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
## Device properties per one device node.
##

package DevConf::Properties;

use strict;
use vars qw(@ISA %PROP_SPEC $BOOL_VALUE);

use UtsTune::Parser;
use DevConf::Constants;
use DevConf::Type::CppInt;

%PROP_SPEC = (INT	=> {type => 'int', type_name => 'int',
			    member_prop => 'bd_prop_int',
			    member_nprops => 'bd_nprops_int',
			    member_type => 'builtin_prop_t *'},
	      INT64	=> {type => 'int64_t', type_name => 'int64',
			    member_prop => 'bd_prop_int64',
			    member_nprops => 'bd_nprops_int64',
			    member_type => 'builtin_prop_t *'},
	      STRING	=> {type => 'char *', type_name => 'string',
			    member_prop => 'bd_prop_string',
			    member_nprops => 'bd_nprops_string',
			    member_type => 'builtin_prop_t *'},
	      BYTE	=> {type => 'uchar_t', type_name => 'byte',
			    member_prop => 'bd_prop_byte',
			    member_nprops => 'bd_nprops_byte',
			    member_type => 'builtin_prop_t *'},
	      BOOLEAN	=> {type => undef,  type_name => 'boolean',
			    member_prop => 'bd_prop_boolean',
			    member_nprops => 'bd_nprops_boolean',
			    member_type => 'char **'});

$BOOL_VALUE = [DevConf::Type::CppInt->new(1, UtsTune::Parser::INT)];

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($devidx) = @_;

	my $me = bless {INDEX => $devidx, PROPERTIES => {},
			INT => [], INT64 => [], STRING => [], BYTE => [],
			BOOLEAN => [], OBJREF => {}}, $class;
	return $me;
}

sub _add
{
	my $me = shift;
	my ($parser, $key, $name, $values) = @_;

	my $spec = $PROP_SPEC{$key};
	my $tname = $spec->{type_name};
	my $list = $me->{$key};
	my $prop = $me->{PROPERTIES};
	my $str = $parser->evalString($name);
	$parser->parseError("Property name must not be an empty string")
		if (isEmptyString($str));
	$parser->parseError("Duplicated property: $name")
		if (exists($prop->{$name}));
	$parser->parseError("Too many $tname properties")
		if (scalar(@$list) >= MAX_NPROPS);
	$prop->{$name} = [@$values];

	# Check whether the value is a object reference.
	if (@$values == 1 and $values->[0]->getValueType() == OBJ) {
		$me->{OBJREF}->{$name} = 1;
	}

	push(@$list, $name);
}

sub addIntProperty
{
	my $me = shift;
	my ($parser, $name, $values) = @_;

	$me->_add($parser, 'INT', $name, $values);
}

sub addInt64Property
{
	my $me = shift;
	my ($parser, $name, $values) = @_;

	$me->_add($parser, 'INT64', $name, $values);
}

sub addStringProperty
{
	my $me = shift;
	my ($parser, $name, $values) = @_;

	$me->_add($parser, 'STRING', $name, $values);
}

sub addByteProperty
{
	my $me = shift;
	my ($parser, $name, $values) = @_;

	$me->_add($parser, 'BYTE', $name, $values);
}

sub addBoolProperty
{
	my $me = shift;
	my ($parser, $name) = @_;

	$me->_add($parser, 'BOOLEAN', $name, $BOOL_VALUE);
}

sub output
{
	my $me = shift;
	my ($out, $parser, $cond) = @_;

	my %map;

	$out->print("\n#if\t$cond\n") if ($cond);

	# Dump int, int64, string, byte properties.
	foreach my $key ('INT', 'INT64', 'STRING', 'BYTE') {
		$me->_dumpProp($out, $parser, $key, \%map);
	}

	# Dump boolean properties.
	$me->_dumpBoolProp($out, \%map);

	$out->print("#endif\t/* $cond */\n") if ($cond);

	return \%map;
}

sub _dumpProp
{
	my $me = shift;
	my ($out, $parser, $key, $map) = @_;

	my $spec = $PROP_SPEC{$key};
	my $list = $me->{$key};
	my $type = $spec->{type};
	my $tname = $spec->{type_name};

	my $idx = 0;
	foreach my $name (@$list) {
		# Referred object should be defined by parser.
		next if ($me->{OBJREF}->{$name});

		# Dump property values.
		my $values = $me->{PROPERTIES}->{$name};
		my $sym = $me->_valueSymbol($tname, $idx);
		$out->print(<<OUT);

static const $type $sym\[\] = {
OUT
		my (@member);
		foreach my $value (@$values) {
			my $v = $value->dumpValue();
			push(@member, "\t($type)$v");
		}
		$out->print(join(",\n", @member));
		$out->print(<<OUT);

};
OUT
	}
	continue {
		$idx++;
	}

	my $member_prop = $spec->{member_prop};
	my $member_nprops = $spec->{member_nprops};
	if ($idx == 0) {
		# No property of this type is defined.
		$map->{$member_prop} = C_NULL;
		$map->{$member_nprops} = 0;
		return;
	}

	# Dump property definition.
	$idx = 0;
	my $symProp = $me->_propSymbol($tname);
	$out->print(<<OUT);

static const builtin_prop_t	$symProp\[\] = {
OUT
	foreach my $name (@$list) {
		my ($sym, $nelems);

		if ($me->{OBJREF}->{$name}) {
			# Create reference to the given object symbol.
			my $values = $me->{PROPERTIES}->{$name};
			my $refsym = $values->[0]->dumpValue();
			my $obj = $parser->getDefinedObject($refsym);
			my $array = $obj->isArray();
			my $symname = SYM_OBJECT_PREFIX . $refsym;
			$sym = sprintf("(%s *)%s%s", $type,
				       ($array) ? '' : '&', $symname);
			$nelems = sprintf("sizeof(%s) / sizeof(%s)",
					  $symname, $type);
		}
		else {
			$sym = $me->_valueSymbol($tname, $idx);
			$nelems = scalar(@{$me->{PROPERTIES}->{$name}});
		}
		$out->print(<<OUT);
	{$name, $sym, $nelems},
OUT
	}
	continue {
		$idx++;
	}
	$out->print(<<OUT);
};
OUT

	$map->{$member_prop} = $symProp;
	$map->{$member_nprops} = scalar(@$list);
}

sub _dumpBoolProp
{
	my $me = shift;
	my ($out, $map) = @_;

	my $spec = $PROP_SPEC{BOOLEAN};
	my $list = $me->{BOOLEAN};
	my $tname = $spec->{type_name};
	my $member_prop = $spec->{member_prop};
	my $member_nprops = $spec->{member_nprops};
	my $nprops = scalar(@$list);

	if ($nprops == 0) {
		# No property of this type is defined.
		$map->{$member_prop} = C_NULL;
		$map->{$member_nprops} = 0;
		return;
	}

	# Dump array of boolean property names.
	my $idx = 0;
	my $symProp = $me->_propSymbol($tname);
	$out->print(<<OUT);

static const char	*$symProp\[\] = {
OUT
	foreach my $name (@$list) {
		$out->print(<<OUT);
	$name,
OUT
	}
	$out->print(<<OUT);
};
OUT

	$map->{$member_prop} = $symProp;
	$map->{$member_nprops} = $nprops;
}

sub _valueSymbol
{
	my $me = shift;
	my ($tname, $idx) = @_;

	my $devidx = $me->{INDEX};

	return SYM_INTERNAL_PREFIX . "${tname}_value_${devidx}_${idx}";
}

sub _propSymbol
{
	my $me = shift;
	my ($tname) = @_;

	my $devidx = $me->{INDEX};

	return SYM_INTERNAL_PREFIX . "${tname}_prop_${devidx}";
}

1;
