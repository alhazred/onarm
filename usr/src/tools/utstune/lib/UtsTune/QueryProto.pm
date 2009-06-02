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
## Define query server mode protocol.
##

package UtsTune::QueryProto;

use strict;
use vars qw(@ISA @EXPORT);

use Exporter;
use Socket;

use constant	QSERV_QUIT	=> 'CMD::QUIT';

sub qserv_channel($)
{
	my ($channel) = @_;

	my $tmpdir = $ENV{TMPDIR} || '/tmp';

	return sprintf("%s/utquery_%s", $tmpdir, $channel);
}

sub qserv_server($)
{
	my ($path) = @_;

	socket(SOCK, PF_UNIX, SOCK_STREAM, 0) or
		die "socket(PF_UNIX) failed; $!\n";

	unlink($path);
	my $sun = sockaddr_un($path);
	bind(SOCK, $sun) or die "bind($path) failed: $!\n";
	listen(SOCK, 1) or die "listen($path) failed: $!\n";

	return \*SOCK;
}

sub qserv_client($$)
{
	my ($path, $timeout) = @_;

	my $sun = sockaddr_un($path);
	socket(SOCK, PF_UNIX, SOCK_STREAM, 0) or
		die "socket(PF_UNIX) failed; $!\n";

	if ($timeout) {
		eval {
			local $SIG{ALRM} = sub {
				die "Connection timed out.\n";
			};

			alarm($timeout);
			connect(SOCK, $sun) or
				die "connect($path) failed: $!\n";
			alarm(0);
		};
		if ($@) {
			my $err = "$@";
			chomp($err);
			die "$err\n";
		}
	}
	else {
		connect(SOCK, $sun) or die "connect($path) failed: $!\n";
	}

	return \*SOCK;
}

@ISA = qw(Exporter);
@EXPORT = qw(QSERV_QUIT qserv_channel qserv_server qserv_client);

1;
