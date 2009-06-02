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
# Copyright (c) 2007-2008 NEC Corporation
# All rights reserved.
#

##
## Class for "option" block in modtune.
##

package UtsTune::Block::Option;

use strict;
use vars qw(@ISA %PARAMETER);

use UtsTune;
use UtsTune::Block;

@ISA = qw(UtsTune::Block);

# Valid parameter names
%PARAMETER = (
	default			=> {type => undef},
	default_debug32		=> {type => undef},
	default_debug64		=> {type => undef},
	default_obj32		=> {type => undef},
	default_obj64		=> {type => undef},
	description		=> {type => 'UtsTune::Type::String'},
	value			=> {type => undef, nodef => 1},
	max			=> {type => undef},
	max_debug32		=> {type => undef},
	max_debug64		=> {type => undef},
	max_obj32		=> {type => undef},
	max_obj64		=> {type => undef},
	min			=> {type => undef},
	min_debug32		=> {type => undef},
	min_debug64		=> {type => undef},
	min_obj32		=> {type => undef},
	min_obj64		=> {type => undef},
	candidates		=> {type => undef, list => 1},
	candidates_debug32	=> {type => undef, list => 1},
	candidates_debug64	=> {type => undef, list => 1},
	candidates_obj32	=> {type => undef, list => 1},
	candidates_obj64	=> {type => undef, list => 1},
	power			=> {type => 'UtsTune::Type::Integer',
				    natural => 1},
	power_debug32		=> {type => 'UtsTune::Type::Integer',
				    natural => 1},
	power_debug64		=> {type => 'UtsTune::Type::Integer',
				    natural => 1},
	power_obj32		=> {type => 'UtsTune::Type::Integer',
				    natural => 1},
	power_obj64		=> {type => 'UtsTune::Type::Integer',
				    natural => 1},
	align			=> {type => 'UtsTune::Type::Integer',
				    natural => 1},
	align_debug32		=> {type => 'UtsTune::Type::Integer',
				    natural => 1},
	align_debug64		=> {type => 'UtsTune::Type::Integer',
				    natural => 1},
	align_obj32		=> {type => 'UtsTune::Type::Integer',
				    natural => 1},
	align_obj64		=> {type => 'UtsTune::Type::Integer',
				    natural => 1},
	rebuild			=> {type => 'UtsTune::Type::Bool'},
	'also-define'		=> {type => 'UtsTune::Type::Symbol',
				    list => 1},
);

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($name, $type) = @_;

	# Check whether option name is valid.
	die "Invalid option name: $name\n"
		if ($name =~ /^[^A-Z]|[^A-Z\d_]/o);

	my $me = $class->SUPER::new($type, \%PARAMETER);
	$me->{NAME} = $name;

	return bless $me, $class;
}

sub getBlockDecl
{
	my $me = shift;

	my $name = $me->getName();
	my $type = $me->getType();
	my $opttype = $type->getType();
	return "option $opttype $name";
}

sub getName
{
	my $me = shift;

	return $me->{NAME};
}

sub getParamKey
{
	my $me = shift;
	my ($pname, $objdir) = @_;

	return $pname . '_' . $objdir;
}

sub getParameter
{
	my $me = shift;
	my ($pname, $objdir) = @_;

	my $key = $me->getParamKey($pname, $objdir);
	my $pmap = $me->{PARAM};
	if (exists($pmap->{$key})) {
		return $pmap->{$key};
	}

	if (exists($pmap->{$pname})) {
		return $pmap->{$pname};
	}
	return undef;
}

sub setValue
{
	my $me = shift;
	my ($value) = @_;

	$me->{PARAM}->{value} = $value if (defined($value));
}

sub getValue
{
	my $me = shift;
	my ($objdir) = @_;

	my $value = $me->{PARAM}->{value};
	$value = $me->getParameter('default', $objdir)
		unless (defined($value));

	return $value;
}

sub reset
{
	my $me = shift;

	delete($me->{PARAM}->{value});
}

sub fixup
{
	my $me = shift;
	my ($parser) = @_;

	my $desc = $me->getParameter('description');
	$parser->parseError("\"description\" is not defined.")
		unless ($desc);
	my $dlen = length($desc);
	my $index;
	for ($index = 0; $index < $dlen; $index++) {
		my $c = substr($desc, $index, 1);
		last if ($c =~ /^[a-z]$/o);
	}
	$parser->parseError("\"description\" doesn't contain valid ",
			    "contents.") if ($index == $dlen);

	my $type = $me->getType();
	foreach my $objdir (sort keys %{OBJS_DIRS()}) {
		next if (!$UtsTune::BUILD64 and OBJS_DIRS->{$objdir});

		# Check default value.
		my $default = $me->getParameter('default', $objdir);
		$parser->parseError("No default value is defined ",
				    "for $objdir")
			unless (defined($default));

		my $max = $me->getParameter('max', $objdir);
		my $min = $me->getParameter('min', $objdir);
		$parser->parseError("\"max\" must be larger than ",
				    "\"min\".")
			if (defined($max) and defined($min) and
			    $type->compare($min, $max) >= 0);
		eval {
			$me->checkValueImpl($default, $type, $objdir,
					    "Default value");
		};
		if ($@) {
			my $err = "$@";
			my $name = $me->getName();

			chomp($err);
			$parser->parseError("$name: $objdir: $err");
		}
	}

	my $adlist = $me->getParameter('also-define');
	if ($adlist and @$adlist) {
		unless ($type->isa('UtsTune::Type::Bool')) {
			my $name = $me->getName();
			$parser->parseError("$name: 'also-define' can be ",
					    "used only in boolean option ",
					    "block.");
		}
	}
}

sub checkValueImpl
{
	my $me = shift;
	my ($value, $type, $objdir, $label) = @_;

	my $max = $me->getParameter('max', $objdir);
	my $min = $me->getParameter('min', $objdir);
	my $power = $me->getParameter('power', $objdir);
	my $align = $me->getParameter('align', $objdir);
	my $cds = $me->getParameter('candidates', $objdir);

	if (defined($cds)) {
		# Allow this value if it's a member of candidates.
		return if ($type->checkCandidates($value, $cds));

		# Deny if no other constraint is defined.
		die "$label is not a member of candidates: $value\n"
			unless (defined($max) or defined($min) or
				defined($power) or defined($align));
	}

	die "$label is larger than max($max): $value\n"
		unless ($type->checkMax($value, $max));
	die "$label is smaller than min($min): $value\n"
		unless ($type->checkMin($value, $min));
	die "$label is not a power of $power: $value\n"
		unless ($type->checkPower($value, $power));
	die "$label is not aligned by $align: $value\n"
		unless ($type->checkAlign($value, $align));
}

sub checkValue
{
	my $me = shift;
	my ($value, $objdir) = @_;

	my $type = $me->getType();

	eval {
		$me->checkValueImpl($value, $type, $objdir, "Value");
	};
	if ($@) {
		my $err = "$@";

		chomp($err);
		my $name = $me->getName();
		die "$name: $err\n";
	}
}

sub equals
{
	my $me = shift;
	my ($opt) = @_;

	my $name1 = $me->getName();
	my $name2 = $opt->getName();
	return undef unless ($name1 eq $name2);

	my $t1 = ref($me->getType());
	my $t2 = ref($opt->getType());
	return undef unless ($t1 eq $t2);

	# Check whether property has been changed.
	my (%pmap1) = (%{$me->{PARAM}});
	my (%pmap2) = (%{$opt->{PARAM}});

	delete $pmap1{value};
	delete $pmap2{value};
	my (@k1) = (sort keys(%pmap1));
	my (@k2) = (sort keys(%pmap2));

	return undef unless (join(':', @k1) eq join(':', @k2));

	foreach my $pname (@k1) {
		my $v1 = $pmap1{$pname};
		my $v2 = $pmap2{$pname};
		my $ptype = $me->getParamType($pname);

		if (ref($v1) eq 'ARRAY') {
			return undef unless (ref($v2) eq 'ARRAY');
			return undef unless (@$v1 == @$v2);

			for (my $i = 0; $i < scalar(@$v1); $i++) {
				my $vv1 = $v1->[$i];
				my $vv2 = $v2->[$i];

				unless ($ptype->compare($vv1, $vv2)
					== 0) {
					return undef;
				}
			}
		}
		else {
			return undef if (ref($v2));
			unless ($ptype->compare($v1, $v2) == 0) {
				return undef;
			}
		}
	}

	return 1;
}

sub hasValue
{
	my $me = shift;

	return defined($me->{PARAM}->{value});
}

sub isRebuild
{
	my $me = shift;

	my $type = $me->getType();
	my $value = $me->getParameter('rebuild');

	return $type->isTrue($value);
}

1;
