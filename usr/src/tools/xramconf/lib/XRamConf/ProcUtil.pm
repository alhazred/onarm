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
## Process utility.
##

package XRamConf::ProcUtil;

use strict;
use vars qw(@ISA @EXPORT);

use Exporter;
use FileHandle;
use POSIX;
use POSIX qw(:sys_wait_h :errno_h);

sub check_status($$);
sub read_from_process($$@);
sub find_program($);

@ISA = qw(Exporter);
@EXPORT = qw(read_from_process find_program);

sub check_status($$)
{
	my ($cmd, $st) = @_;

	if (WIFEXITED($st)) {
		my $status = WEXITSTATUS($st);
		die "$cmd failed with status $status\n" unless ($status == 0);
	}
	elsif (WSIGNALLED($st)) {
		my $sig = WTERMSIG($st);

		# WCOREDUMP isn't a POSIX macro, do it the non-portable way.
		if ($st & 0x80) {
			die "$cmd failed with signal $sig (core dumped)\n";
		}
		else {
			die "$cmd failed with signal $sig\n";
		}
	}
	else {
		my $msg = sprintf("%s failed: status = 0x%08x", $cmd, $st);
		die "$msg\n";
	}
}

sub read_from_process($$@)
{
	my ($handler, $errfh, @argv) = @_;

	my ($rh, $wh) = FileHandle::pipe;
	my $pid = fork();
	if (!defined($pid)) {
		die "fork() failed: $!\n";
	}
	elsif ($pid == 0) {
		undef $rh;
		$wh->autoflush(1);
		POSIX::dup2($wh->fileno(), STDOUT->fileno());
		POSIX::dup2($errfh->fileno(), STDERR->fileno())
			if ($errfh);
		exec(@argv);
		die "exec(@argv) failed: $!\n";
	}

	undef $wh;
	while (<$rh>) {
		&$handler($_);
	}

	my $ret;
	do {
		$ret = waitpid($pid, 0);
	} while ($ret == -1 and $! == EINTR);
	die "waitpid() failed: $!\n" if ($ret == -1);

	return check_status($argv[0], $?);
}

sub find_program($)
{
	my ($prog) = @_;

	my $name = uc($prog);
	return $ENV{$name} if (defined($ENV{$name}));

	foreach my $dir (split(/:/, $ENV{PATH})) {
		my $p = "$dir/$prog";
		return $p if (-x $p);
	}

	return $prog;
}

1;

