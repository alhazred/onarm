#!/usr/bin/perl

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

=head1 NAME

I<subdirfix> - Eliminate sub directory

=head1 SYNOPSIS

B<subdirfix> -f I<path> directory name ....

=head1 DESCRIPTION

I<subdirfix> is used to eliminate specified directories from from SUBDIR
list in Makefile.

I<subdirfix> takes list of directories as argument,
and prints directories that is not specified in the file specified by
B<-f> option.

=head1 OPTIONS

The following options are supported:

=over 4

=item B<-f>|B<--file> I<path>

Specify path to file that contains directory name to be eliminated.
Each directory name must be written in one line.
From '#' character until end of line will be treated as comment.

=back

=head1 EXAMPLES

Let's assume that the "file" contains the following lines.

  subdir3
  subdir5

The command:

  subdirfix -f file subdir1 subdir2 subdir3 subdir4 subdir5

produces the output:

  subdir1
  subdir2
  subdir4

=cut

use strict;
use Getopt::Long;
use File::Basename;
use FileHandle;

use vars qw($PROGNAME);

$PROGNAME = basename($0);

sub usage($);

sub build_skipdirs($);

sub find_program($);
sub check_status($$);
sub run(@);
sub read_from_process($@);
sub parse_symbol_table($);
sub generate_header($$);
sub generate_source($);

MAIN:
{
	Getopt::Long::Configure(qw(no_ignore_case bundling require_order));

	my ($help, $file);
	usage(2) unless (GetOptions
			 ('f|file=s'	=> \$file,
			  'h|help'	=> \$help));
	usage(0) if ($help);

	my $skip = build_skipdirs($file);

	foreach my $dir (@ARGV) {
		print "$dir\n" unless ($skip->{$dir});
	}
}

sub usage($)
{
	my ($status) = @_;

	my $out = ($status) ? \*STDERR : \*STDOUT;
	print $out <<OUT;
Usage: $PROGNAME [options] compiler [CFLAGS]
OUT

	unless ($status) {
		print $out <<OUT;

Options:

  -f|--file <path>
      Specify path to file that contains directory names to be eliminated.

OUT
	}
	exit $status;
}

sub build_skipdirs($)
{
	my ($file) = @_;

	# Read directory names to be eliminated from SUBDIR.
	my $fh = FileHandle->new($file) or die "open($file) failed: $!\n";
	my %hash;
	while (<$fh>) {
		next if (/^\s*\x23/);
		chomp;

		# Eliminate comments.
		s/\x23.*$//g;

		# Eliminate preceding and trailing whitespaces.
		s/^\s*//g;
		s/\s*$//g;

		$hash{$_} = 1;
	}

	return \%hash;
}
