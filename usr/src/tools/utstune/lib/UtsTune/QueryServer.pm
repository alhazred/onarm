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
## Parameter query server.
##

package UtsTune::QueryServer;

use strict;

use Socket;
use IO::Handle;
use UtsTune::QueryParser;
use UtsTune::QueryProto;

use constant	CHAN_MODE	=> 0600;

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my (%args) = @_;

	my $timeout = $args{TIMEOUT};
	delete($args{TIMEOUT});

	my $channel = $args{CHANNEL};
	delete($args{CHANNEL});
	die "Channel must be specified.\n" unless ($channel);

	my $debug = $args{DEBUG};

	# Setup UNIX domain socket.
	my $path = qserv_channel($channel);
	my $fh = qserv_server($path);

	my $parser = UtsTune::QueryParser->new(%args);

	return bless {PARSER => $parser, TIMEOUT => $timeout, DEBUG => $debug,
		      HANDLE => $fh, PATH => $path}, $class;
}

sub run
{
	my $me = shift;

	my $timeout = $me->{TIMEOUT};
	my $handle = $me->{HANDLE};
	my $debug = $me->{DEBUG};
	my $parser = $me->{PARSER};
	my %cache;

	while (1) {
		my $line;

		# Read command from client.
		if ($timeout) {
			eval {
				local $SIG{ALRM} = sub { die "timeout\n"; };

				alarm($timeout);
				accept(CLIENT, $handle) or last;
				$line = <CLIENT>;
				alarm(0);
			};
			if ($@) {
				my $err = "$@";

				chomp($err);
				if ($err =~ /timeout/) {
					$debug->debug("Query server timeout.")
						if ($debug);
					exit 2;
				}
				die "$err\n";
			}
		}
		else {
			accept(CLIENT, $handle) or last;
			$line = <CLIENT>;
		}
		last unless (defined($line));
		chomp($line);

		CLIENT->autoflush(1);
		$debug->debug("COMMAND[$line]") if ($debug);

		last if ($line eq QSERV_QUIT);
		next unless ($line);

		# Evaluate query.
		# At first, try the query cache.
		my $result = $cache{$line};
		if (defined($result)) {
			$debug->debug("Result -> $result (cached)\n")
				if ($debug);
		}
		else {
			eval {
				$result = $parser->evaluate($line);
			};
			if ($@) {
				my $err = "$@";
				chomp($err);
				$err =~ s,\n,\\n,g;
				$result = $err;
			}
			$debug->debug("Result -> $result\n") if ($debug);
			$cache{$line} = $result;
		}
		CLIENT->print($result, "\n");
	}
	continue {
		close(CLIENT);
	}
}

sub DESTROY
{
	my $me = shift;

	delete($me->{HANDLE});

	my $path = $me->{PATH};
	unlink($path) if ($path);
}

1;
