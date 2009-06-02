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
## modinfo parser.
##

package StaticTool::ModInfoParser;

use strict;

use FileHandle;
use StaticTool;

use constant	DEFAULT_MODINFO		=> 'modinfo';
use constant	MODMAXNAMELEN		=> 32;
use constant	MI_NS_RELOCATABLE	=> '<relocatable>';

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($file, $noparse) = @_;

	my $me = bless {FILE => $file, DEPENDS => [], DEPENDED => {}}, $class;
	$me->parse() unless ($noparse);

	return $me;
}

sub parse
{
	my $me = shift;

	my $file = $me->path();
	my $mf = FileHandle->new($file) or die "open($file) failed: $!";

	while (<$mf>) {
		next if (/^\s*\x23/);
		chomp;
		my (@token) = split(/\s+/, $_);
		my $name = shift(@token);
		next unless ($name);
		$name = lc($name);
		my $method = 'line_' . $name;
		die "Unknown parameter: $_\n" unless ($me->can($method));

		$me->$method($_, \@token);
	}

	$me->validate() if ($me->isRelocatable());
}

sub validate
{
	my $me = shift;

	return 1 if ($me->{_VALID});

	my $namespace = $me->namespace();
	die "\"namespace\" is not defined.\n"
		unless ($me->isRelocatable() or ($namespace and @$namespace));
	my $modname = $me->name();
	die "\"modname\" is not defined.\n" unless ($modname);
	foreach my $ns (@$namespace) {
		my $path = "$ns/$modname";

		$me->checkModname($path);
	}

	my $depends = $me->depends();
	die "\"depends\" can't be specified for relocatable object.\n"
		if ($me->isRelocatable() and ($depends and @$depends));
	$me->{_VALID} = 1;
	return 1;
}

sub checkModname
{
	my $me = shift;
	my ($path) = @_;

	die "Too long module path: [$path]\n"
		if (length($path) >= MODMAXNAMELEN);
}

sub path
{
	my $me = shift;

	return $me->{FILE};
}

sub namespace
{
	my $me = shift;

	return $me->{NAMESPACE};
}

sub name
{
	my $me = shift;

	return $me->{MODNAME};
}

sub depends
{
	my $me = shift;

	return $me->{DEPENDS};
}

sub isRelocatable
{
	my $me = shift;

	return $me->{RELOC};
}

sub entryPointPrefix
{
	my $me = shift;

	$me->validate();

	my $ns = $me->namespace()->[0];
	my $name = escape_symbol($me->name());
	$ns =~ s,/,_,g;
	return "moddrv_${ns}_${name}";
}

sub depended
{
	my $me = shift;
	my ($mi) = @_;

	my $map = $me->{DEPENDED};
	my $mname = $mi->name();
	$map->{$mname} = $mi;
}

sub checkDependency
{
	my $me = shift;
	my $name = shift || $me->name();
	my $parent = shift;

	my $map = $me->{DEPENDED};
	foreach my $mi (values %$map) {
		my $mname = $mi->name();
		die "Module dependency loop: $name, $parent\n"
			if ($name eq $mname);
		$mi->checkDependency($name, $me->name());
	}
}

# Create CPPFLAGS used by module build.
sub cppflags
{
	my $me = shift;
	my ($static, $objdir, $plat) = @_;

	my (@defs);

	if ($static) {
		push(@defs, "-DSTATIC_DRIVER");

		# Define entry point function name.
		my $prefix = $me->entryPointPrefix();
		foreach my $func (qw(_init _fini _info)) {
			my $macro = "MODDRV_ENTRY" . uc($func);
			my $strmacro = $macro . '_STR';
			my $value = $prefix . $func;
			push(@defs, "-D${macro}=${value}",
			     "-D${strmacro}=\\\"${value}\\\"");
		}
	}

	return (wantarray) ? @defs : \@defs;
}

# Dump additional struct definition.
# Currently NOP.
sub dumpAdditional
{
}

##
## Methods to parse each modinfo lines.
##
sub line_namespace
{
	my $me = shift;
	my ($line, $token) = @_;

	die "Invalid namespace: $line\n" unless (@$token);
	if (@$token == 1 and $token->[0] eq MI_NS_RELOCATABLE) {
		# This module is just a ELF relocatable, not loadable module.
		$me->{RELOC} = 1;
		return;
	}

	my $namespace = $me->{NAMESPACE};
	unless (defined($namespace)) {
		$namespace = [];
		$me->{NAMESPACE} = $namespace;
	}

	foreach my $ns (@$token) {
		push(@$namespace, $ns) unless (grep {$_ eq $ns} @$namespace);
	}
}

sub line_modname
{
	my $me = shift;
	my ($line, $token) = @_;

	die "Invalid modname: $line\n" unless (@$token == 1 and $token->[0]);
	die "\"modname\" duplicated: $line\n" if ($me->name());
	$me->{MODNAME} = $token->[0];
}

sub line_depends
{
	my $me = shift;
	my ($line, $token) = @_;

	my $depends = $me->{DEPENDS};
	unless (defined($depends)) {
		$depends = [];
		$me->{DEPENDS} = $depends;
	}

	foreach my $t (@$token) {
		unless (grep {$_ eq $t} @$depends) {
			$me->checkModname($t);
			push(@$depends, $t);
		}
	}
}

1;
