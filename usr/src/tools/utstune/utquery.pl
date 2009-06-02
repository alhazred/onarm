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

use strict;
use Config;
use File::Basename;
use FileHandle;
use POSIX;

my ($perl, @libs) = (@ARGV);
my $file = basename($0);
$file =~ s/\.pl$//;

my $out = FileHandle->new($file, O_WRONLY|O_CREAT|O_TRUNC, 0755) or
	die "Can't open ($file): $!\n";

print $out <<EOF;
#!$perl

use lib qw(@libs);

EOF

print $out <<'EOF';
#
# Copyright (c) 2009 NEC Corporation
# All rights reserved.
#

=head1 NAME

I<utquery> - Client program to query kernel build-time tunable parameters.

=head1 SYNOPSIS

B<utquery> [-T timeout] channel query

=head1 DESCRIPTION

I<utquery> command is a subset of I<utstune>.

I<utquery> command sends the given query to I<utstune> running as
query server mode. The exit status of I<utquery> represents the result
of query.

The query format is described in I<utstune> manual.

=head1 OPTIONS

I<utquery> takes the following optionss:

=over 4

=item B<-T>|B<--timeout> I<timeout>

How long (in seconds) I<utquery> can wait for query result.

=item B<-q>|B<--quit>

Tell I<utstune> running as query server to quit.

=head1 EXIT STATUS

The following exit values are returned:

=over 3

=item 0

The result was true.

=item 1

The result was false.

=item >1

An error was detected.

=back

=head1 SEE ALSO

B<bldenv>(1),
B<utstune>(1)

=cut

use strict;
use Getopt::Long;
use IO::Handle;
use POSIX;
use Time::HiRes qw(usleep);

use UtsTune::QueryProto;

use vars qw($PROGNAME);

$PROGNAME = 'utquery';

sub usage($);
sub qserv_connect($$);
sub do_query($$$);

MAIN:
{
	Getopt::Long::Configure(qw(no_ignore_case bundling require_order));

	my ($help, $manual, $timeout, $quit);
	usage(2) unless (GetOptions
			 ('help'	=> \$help,
			  'T|timeout=i'	=> \$timeout,
			  'q|quit'	=> \$quit,
			  'manual'	=> \$manual));

	usage(0) if ($help);

	if ($manual) {
		require Config;
		import Config;

		use vars qw(%Config);

		my $perldoc = $Config{installbin} . '/perldoc';
		exec($perldoc, $0);
		die "exec($perldoc, $0) failed: $!";
	}

	usage(1) unless (($quit and @ARGV == 1) or @ARGV == 2);
	my $channel = $ARGV[0];
	my $command = ($quit) ? QSERV_QUIT : $ARGV[1];
	my $status;

	eval {
		$status = do_query($channel, $command, $timeout);
	};
	if ($@) {
		my $err = "$@";
		chomp($err);
		print STDERR "*** ERROR: $err\n";
		exit 2;
	}

	exit $status;
}

sub qserv_connect($$)
{
	my ($channel, $timeout) = @_;

	my $path = qserv_channel($channel);
	my $start = time();
	while (! -S $path) {
		# Server may not be ready.
		usleep(100000);
		if ($timeout) {
			die "Connection timed out. (server is not ready)\n"
				if (time() - $start > $timeout);
		}
	}

	return qserv_client($path, $timeout);
}

sub do_query($$$)
{
	my ($channel, $command, $timeout) = @_;

	my $fh = qserv_connect($channel, $timeout);

	# Send command.
	$fh->print($command, "\n");
	$fh->flush();

	exit 0 if ($command eq QSERV_QUIT);

	my $result;
	if ($timeout) {
		eval {
			local $SIG{ALRM} = sub { die "timeout\n"; };

			alarm($timeout);
			$result = $fh->getline();
			alarm(0);
		};
		if ($@) {
			my $err = "$@";

			chomp($err);
			die "Query timeout.\n" if ($err =~ /timeout/);
			die "Read error; $err\n";
		}
	}
	else {
		$result = $fh->getline();
	}

	return 0 if ($result eq "0\n");
	return 1 if ($result eq "1\n");

	chomp($result);
	$result = "Empty response." unless ($result);
	die "$result\n";
}

sub usage($)
{
	my ($status) = @_;

	my $out = ($status) ? \*STDERR : \*STDOUT;
	print $out <<OUT;
Usage: $PROGNAME [-T timeout] channel query
Usage: $PROGNAME -q channel

OUT

	if ($status) {
		print $out <<OUT;
Try "$PROGNAME --help".
OUT
	}
	else {
		print $out <<OUT;
Options:

  -T|--timeout
      How long (in seconds) utquery can wait for query result.

  --manual
      Print online manual for utquery(1).

OUT
	}

	exit $status;
}
EOF
