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
# Copyright (c) 2007-2009 NEC Corporation
# All rights reserved.
#

##
## Utilities
##

package UtsTune::Util;

use strict;

use FileHandle;
use File::stat;
use POSIX;

use UtsTune;

use constant	DB_FORMAT_VERSION	=> 2;

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;

	return bless {}, $class;
}

sub dependency
{
	my $me = shift;
	my ($modtune, $objdir) = @_;

	my $efile = modtune_efile($modtune, $objdir);
	print "FRC\n" if (-f $efile);
}

sub getFormatVersion
{
	my $me = shift;

	return undef unless (-f $UtsTune::INIT_STAMP);

	my $fh = FileHandle->new($UtsTune::INIT_STAMP) or
		die "open($UtsTune::INIT_STAMP) failed: $!\n";
	my $ver = $fh->getline();
	chomp($ver);
	return $ver;
}

sub putFormatVersion
{
	my $me = shift;

	# Create timestamp file.
	my $fh = FileHandle->new($UtsTune::INIT_STAMP,
				 O_WRONLY|O_CREAT|O_TRUNC, 0644)
		or die "open($UtsTune::INIT_STAMP) failed: $!\n";
	$fh->print(DB_FORMAT_VERSION, "\n");
}

sub buildMode
{
	return '';
}

sub checkFormatVersion
{
	my $me = shift;
	my ($dieonerr) = @_;

	my $err;
	my $doreset;
	if (-f $UtsTune::INIT_STAMP) {
		my $fh = FileHandle->new($UtsTune::INIT_STAMP) or
			die "open($UtsTune::INIT_STAMP) failed: $!\n";
		my $ver = $fh->getline();
		chomp($ver);
		if ($ver == DB_FORMAT_VERSION) {
			my $line = $fh->getline() or '';
			chomp($line);
			my $mode = $me->buildMode();
			$err = 'Build mode mismatch' unless ($line eq $mode);
		}
		else {
			$err = 'DB format version mismatch';
			$doreset = 1;
		}
	}
	else {
		$err = 'Uninitialized';
		$doreset = 1;
	}

	if ($dieonerr and $err) {
		my $msg = <<OUT;
$err. Try "make tune-init" under \$SRC/uts.
OUT
		die $msg;
	}

	return ($err, $doreset);
}

sub checkTimeStamp
{
	my $me = shift;
	my ($file, $stamp) = @_;

	my $sb = stat($file);
	die "Can't access $file: $!\n" unless ($sb);
	$stamp = stat($UtsTune::INIT_STAMP) unless ($stamp);
	unless ($stamp) {
		my $msg = <<OUT;
Not yet initialized. Try "make tune-init" under \$SRC/uts.
OUT
		die $msg;
	}

	my $mtime = $sb->mtime();
	if ($stamp->mtime() < $mtime) {
		my $path = utspath($file);
		my $msg = <<OUT;
$path has been modified. Try "make tune-init" under \$SRC/uts.
OUT
		die $msg;
	}

	return $mtime;
}

sub preScript
{
	my $me = shift;
	my ($script) = @_;
	my $def;

	return $def;
}

1;
