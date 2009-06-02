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
## Makefile option script utilities.
##

package UtsTune::MakeOption;

use strict;

use DirHandle;
use FileHandle;
use File::Basename;
use File::stat;
use POSIX;

use UtsTune;
use UtsTune::OptionLoader;
use UtsTune::Script::Makeopt;
use UtsTune::DB::Parameter;

use constant	MAKEOPT		=> 'makeopt';
use constant	MAKEOPT_SUFFIX	=> '.mk';

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;

	return bless {}, $class;
}

# Add Makefile configuration script.
sub add
{
	my $me = shift;
	my ($script, @scope) = @_;

	require UtsTune::PackageLoader;
	import UtsTune::PackageLoader;
	my $loader = UtsTune::PackageLoader->new();
	my $util = $loader->utilInstance();
	my $mtime = $util->checkTimeStamp($script);

	my $db = UtsTune::DB::Parameter->new(1);
	$db->addMakeOpt($script, $mtime, \@scope);
}

sub remove
{
	my $me = shift;
	my ($script) = @_;

	my $stamp = stat($UtsTune::INIT_STAMP);
	my $db = UtsTune::DB::Parameter->new(1);

	unless ($stamp) {
		my $msg = <<OUT;
Not yet initialized. Try "make tune-init" under \$SRC/uts.
OUT
		die $msg;
	}

	my $mtime = $db->getMakeOptMtime($script);
	if (defined($mtime)) {
		my $file = utspath($script);
		my $msg = <<OUT;
$file file has been removed. Try "make tune-init" under \$SRC/uts.
OUT
		die $msg;
	}
	$db->removeMakeOpt($script);
	$me->clean(dirname($script));
}

sub outfile
{
	my $me = shift;
	my ($dir, $objdir, $script) = @_;

	my $mkpath = "$dir/$objdir";
	return $mkpath . '/' . basename($script) . MAKEOPT_SUFFIX;
}

sub dump
{
	my $me = shift;
	my ($script, $objdir) = @_;

	my $dir = dirname($script);
	unless (grep {$objdir eq $_} (get_build_objdirs()) and -f $script) {
		# This target is never built on this environment, or
		# makeopt script has been removed.
		# Create empty file.
		my $mkfile = $me->outfile($dir, $objdir, $script);
		my $fh = FileHandle->new($mkfile, O_WRONLY|O_CREAT|O_TRUNC,
					 0644)
			or die "open($mkfile) failed: $!\n";
		return;
	}
	my $db = UtsTune::DB::Parameter->new(1);
	my $loader = UtsTune::OptionLoader->new($db);
	my $scopelist = $db->getRelatedScope($script);
	push(@$scopelist, local_scope($script));
	my $runner = UtsTune::Script::Makeopt->new($db, $loader, $scopelist);
	$me->outputImpl($runner, $script, $dir, $objdir);
}

# Clean up Makefile options.
sub clean
{
	my $me = shift;
	my ($dir) = @_;

	my $suff = MAKEOPT_SUFFIX;
	foreach my $objdir (keys %{OBJS_DIRS()}) {
		my $dpath = "$dir/$objdir";
		my $dirp = DirHandle->new($dpath) or next;
		while (my $dp = $dirp->read()) {
			next unless ($dp =~ m,\Q$suff\E$,);
			unlink("$dpath/$dp");
		}
	}
}

# Get all scripts related to the specified scope.
sub getScriptByScope
{
	my $me = shift;
	my ($db, $scopelist) = @_;

	my (@list, $scmap);
	if ($scopelist) {
		$scmap = {};
		foreach my $scope (@$scopelist) {
			if (is_local_scope($scope)) {
				$scmap->{$scope} = 1;
			}
			elsif (!$scmap->{$scope}) {
				my $p = $db->getPlatPath($scope) ||
					$db->getArchPath($scope);
				$scmap->{$scope} = 1 if ($p);
			}
		}
	}

	if (!$scmap or %$scmap) {
		push(@list, @{$db->getMakeOptByScope($scmap)});
	}

	return (wantarray) ? @list : \@list;
}

sub output
{
	my $me = shift;
	my ($dirlist, $plat, $exclude, $db, $mkscope, $out) = @_;

	my $list = $me->getScriptByScope($db, $mkscope);
	return unless ($list and @$list);

	my $loader = UtsTune::OptionLoader->new($db, $plat);
	foreach my $script (@$list) {
		# Derive imported scopes from parameter DB.
		my $scopelist = $db->getRelatedScope($script);
		next if (grep({$exclude->{$_}} @$scopelist));

		# Append local scope.
		push(@$scopelist, local_scope($script));

		# Evaluate script for each object directory.
		my $runner = UtsTune::Script::Makeopt->
			new($db, $loader, $scopelist);
		my $dir = dirname($script);
		foreach my $objdir (@$dirlist) {
			$out->debug("Apply makeopt script for $plat.$objdir: ",
				    $script);
			$me->outputImpl($runner, $script, $dir, $objdir);
		}
	}
}

sub outputImpl
{
	my $me = shift;
	my ($runner, $script, $dir, $objdir) = @_;

	$runner->setObjDir($objdir);
	$runner->run($script);

	my $mkfile = $me->outfile($dir, $objdir, $script);
	unless (-d $dir) {
		unless (mkdir($dir, 0755)) {
			my $p = utspath($dir);
			die "mkdir($p) failed: $!\n";
		}
	}

	my $opts = $runner->getOptions();
	my $fh = FileHandle->new($mkfile, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	unless ($fh) {
		my $p = utspath($mkfile);
		die "open($p) failed: $!\n";
	}

	$fh->print(<<OUT);
# Auto-generated file. DO NOT EDIT.

OUT
	foreach my $o (@$opts) {
		$fh->print($o, "\n");
	}
}

1;
