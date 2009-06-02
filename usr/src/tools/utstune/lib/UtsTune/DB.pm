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
# Copyright (c) 2007 NEC Corporation
# All rights reserved.
#

###
### Base class for database.
###

package UtsTune::DB;

use strict;
use Fcntl;
use Fcntl qw(:flock);
use File::Basename;
use FileHandle;
use POSIX qw(:errno_h);
use SDBM_File;
use DirHandle;

use UtsTune;

use constant	LOCK_SUFFIX	=> '.lock';

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($dbfile, $write) = @_;

	my $lkfile = $dbfile . LOCK_SUFFIX;

	my ($op, $flag);
	if ($write) {
		$op = LOCK_EX;
		$flag = O_RDWR|O_CREAT;
	}
	else {
		$op = LOCK_SH;
		$flag = O_RDONLY;
	}

	my $me = {DBFILE => $dbfile, LOCKFILE => $lkfile, LOCKOP => $op,
		  OPENFLAG => $flag};
	return bless $me, $class;
}

sub lock
{
	my $me = shift;

	return if ($me->{LOCK});

	my $lkfile = $me->{LOCKFILE};
	my $op = $me->{LOCKOP};
	my $fh = FileHandle->new($lkfile, O_CREAT|O_RDWR|O_TRUNC, 0644)
		or die "open($lkfile) failed: $!\n";
	my $ret;
	do {
		$ret = flock($fh, $op);
	} while (!$ret and $! == EINTR);
	die "flock($lkfile, $op) failed: $!\n" unless ($ret);

	$me->{LOCK} = $fh;
}

sub unlock
{
	my $me = shift;

	delete($me->{LOCK});
}

sub reset
{
	my $me = shift;

	my $h = $me->open();
	undef %$h;
}

sub open
{
	my $me = shift;

	my $h = $me->{DB};
	unless ($h) {
		$me->lock();
		my %hash;
		my $dbfile = $me->{DBFILE};
		my $flag = $me->{OPENFLAG};
		unless (tie(%hash, 'SDBM_File', $dbfile, $flag, 0644)) {
			if ($! == ENOENT) {
				my $msg = <<'OUT';
No database. Try "make tune-setup" under $SRC/uts.
OUT
				die $msg;
			}
			die "tie($dbfile) failed: $!\n";
		}
		$h = \%hash;
		$me->{DB} = $h;
	}

	return $h;
}

sub close
{
	my $me = shift;

	my $h = $me->{DB};
	if ($h) {
		untie(%$h);
		delete($me->{DB});
	}
	$me->unlock();
}

sub put
{
	my $me = shift;
	my ($key, $value) = @_;

	my $h = $me->open();
	$h->{$key} = $value;
}

sub remove
{
	my $me = shift;
	my ($key) = @_;

	my $h = $me->open();
	delete($h->{$key});
}

sub get
{
	my $me = shift;
	my ($key) = @_;

	my $h = $me->open();
	return $h->{$key};
}

sub walk
{
	my $me = shift;
	my ($func) = @_;

	my $h = $me->open();
	foreach my $key (keys %$h) {
		&$func($key, $h->{$key});
	}
}

sub clobber
{
	my $me = shift;

	my $dbfile = $me->{DBFILE};
	my ($fname, $parent) = fileparse($dbfile);
	my $dir = DirHandle->new($parent) or die "open($parent) failed: $!\n";
	while (my $dp = $dir->read()) {
		if ($dp =~ /^\Q$fname\E/) {
			my $path = $parent . '/' . $dp;
			unlink($path);
		}
	}
}

1;
