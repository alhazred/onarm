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
## Base class of dynamic configuration script.
##

package UtsTune::Script;

use strict;

use FileHandle;
use Scalar::Util qw(blessed);

use UtsTune;
use UtsTune::Block::Option;
use UtsTune::PackageLoader;

use constant	OPERATOR	=> {
	'=='		=> sub { return $_[0] == 0; },
	'!='		=> sub { return $_[0] != 0; },
	'<'		=> sub { return $_[0] < 0; },
	'<='		=> sub { return $_[0] <= 0; },
	'>'		=> sub { return $_[0] > 0; },
	'>='		=> sub { return $_[0] >= 0; },
};

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($db, $objdir, $loader) = @_;

	return bless {DB => $db, OBJDIR => $objdir, LOADER => $loader}, $class;
}

sub setScope
{
	my $me = shift;
	my ($scopelist) = @_;

	if ($scopelist and @$scopelist) {
		my (%scope) = map {$_ => 1} @$scopelist;
		$me->{SCOPE} = \%scope;
	}
	else {
		delete($me->{SCOPE});
	}
}

sub setObjDir
{
	my $me = shift;
	my ($objdir) = @_;

	$me->{OBJDIR} = $objdir;

	my $loader = $me->{LOADER};
	$loader->setObjDir($objdir);
}

sub getObjDir
{
	my $me = shift;

	return $me->{OBJDIR};
}

sub setNoEval
{
	my $me = shift;

	$me->{NO_EVAL} = 1;
}

sub parameter
{
	my $me = shift;
	my ($arg) = @_;

	return undef unless (blessed($arg) and
			     $arg->isa('UtsTune::Block::Option'));
	my $objdir = $me->{OBJDIR};
	my $type = $arg->getType();
	my $value = $arg->getValue($objdir);
	return ($type, $value);
}

sub evaluate
{
	my $me = shift;
	my ($arg1, $op, $arg2) = @_;

	return $me->evaluateOne($arg1) unless ($op);

	my ($t1, $v1) = $me->parameter($arg1);
	my ($t2, $v2) = $me->parameter($arg2);

	my $opfunc = OPERATOR->{$op};
	die "Invalid operator: \"$op\"\n" unless ($opfunc);

	my $result;
	if ($t1 and $t2) {
		unless ($t1->canCompare($t2)) {
			my $n1 = $arg1->getName();
			my $n2 = $arg2->getName();
			die "Can't compare $n1 and $n2.\n";
		}
		$result = $t1->compare($v1, $v2);
	}
	elsif ($t1) {
		$v2 = $me->evalValue($t1, $arg2);
		$result = $t1->compare($v1, $v2);
	}
	elsif ($t2) {
		$v1 = $me->evalValue($t2, $arg1);
		$result = $t2->compare($v1, $v2);
	}
	else {
		die "No parameter is specified.\n";
	}

	return &$opfunc($result);
}

sub evaluateOne
{
	my $me = shift;
	my ($arg) = @_;

	my ($type, $value) = $me->parameter($arg);
	die "No parameter is specified.\n" unless ($type);

	# Tunable parameter.
	my $v = $me->evalValue($type, $value);
	return $type->isTrue($v);
}

sub evalValue
{
	my $me = shift;
	my ($type, $value) = @_;

	return $value if ($me->{NO_EVAL});
	return $type->evalValue($value);
}

sub defineFunc
{
	my $me = shift;
	my ($loader_val) = @_;

	my $db = $me->{DB};

	# Define functions for all parameters.
	my $loader = $me->{LOADER};
	my $plat = $loader->getPlatform();
	my $names = $db->getAllOptions($plat);
	my $scmap = $me->{SCOPE};
	my $funcdef = '';
	foreach my $name (@$names) {
		if ($scmap) {
			my $scope = $loader->getScope($name);
			unless ($scmap->{$scope}) {
				$funcdef .= <<OUT;
sub $name()
{
	die "Out out scope: $name\\n";
}

OUT
				next;
			}
		}
		$funcdef .= <<OUT;
sub $name()
{
	return $loader_val->loadOption("$name");
}

OUT
	}

	# Define common functions.
	$funcdef .= <<'OUT';
sub true
{
	return 'true';
}

sub false
{
	return 'false';
}

sub eval_option
{
	return $me->evaluate(@_);
}

sub build_type($)
{
	my ($dir) = @_;

	my $objdir = $me->getObjDir();
	return ($dir eq $objdir);
}

OUT

	return $funcdef;
}

sub run
{
	my $me = shift;
	my ($script) = @_;

	# Define required functions.
	my $funcdef = $me->defineFunc('$loader');
	my $loader = $me->{LOADER};

	if ($funcdef) {
		eval $funcdef;
		if ($@) {
			my $msg = "$@";
			chomp($msg);
			die "$msg\n";
		}
		undef $funcdef;
	}

	# Load dynamic configuration script.
	my $pld = UtsTune::PackageLoader->new();
	my $util = $pld->utilInstance();
	my $def = $util->preScript($script);
	my $fh = FileHandle->new($script) or die "open($script) failed: $!\n";
	while (<$fh>) {
		$def .= $_;
	}
	undef $fh;

	# Evaluate script.
	my $ret = eval $def;
	if ($@) {
		my $path = utspath($script);
		my $msg = "$@";
		chomp($msg);
		die "$path: $msg\n";
	}
	if ($ret != 1) {
		my $path = utspath($script);
		die "$path: Script does not return 1.\n";
	}
}

1;
