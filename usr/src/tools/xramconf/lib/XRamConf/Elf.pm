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
## ELF utility class.
## This class uses elfdatamod(1) command.
##

package XRamConf::Elf;

use strict;
use vars qw($ELFDATAMOD);

use FileHandle;
use POSIX;

use XRamConf::ByteOrder;
use XRamConf::ProcUtil;

use constant	EHDR_SIZE	=> 64;

use constant	EI_NIDENT	=> 16;
use constant	EI_CLASS	=> 4;
use constant	EI_DATA		=> 5;
use constant	EI_MAGIC	=> (0x7f, ord('E'), ord('L'), ord('F'));

use constant	ELFCLASS32	=> 1;
use constant	ELFCLASS64	=> 2;

use constant	ELFDATA2LSB	=> 1;
use constant	ELFDATA2MSB	=> 2;

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($file) = @_;

	my $cmd = (defined($ELFDATAMOD)) ? $ELFDATAMOD
		: find_program('elfdatamod');
	die "elfdatamod command ($cmd) is not found.\n" unless (-x $cmd);
	my $me = bless {CMD => $cmd, FILE => $file}, $class;

	my $elfclass = $me->getClass();
	my $ptrsz;
	if ($elfclass == ELFCLASS32) {
		$ptrsz = '/w';
	}
	elsif ($elfclass == ELFCLASS64) {
		$ptrsz = '/d';
	}
	else {
		die "$file: Unknown ELF class: $elfclass\n";
	}

	$me->{PTRSZ} = $ptrsz;

	return $me;
}

sub getFile
{
	my $me = shift;

	return $me->{FILE};
}

sub getClass()
{
	my $me = shift;

	my $class = $me->{CLASS};
	return $class if (defined($class));

	my $file = $me->getFile();
	my $fh = FileHandle->new($file) or die "open($file) failed: $!\n";

	my $data;
	$fh->read($data, EHDR_SIZE, 0) or die "read($file) failed: $!\n";
	my (@ident) = unpack("C*", $data);

	$me->{EHDR} = $data;
	$me->{IDENT} = \@ident;

	my $i = 0;
	foreach my $magic (EI_MAGIC) {
		die "$file: Not ELF file.\n" unless ($ident[$i] == $magic);
	}
	continue {
		$i++;
	}

	my $class = $ident[EI_CLASS];
	$me->{CLASS} = $class;

	return $class;
}

sub getPointerSize
{
	my $me = shift;

	return $me->{PTRSZ};
}

sub get
{
	my $me = shift;
	my (@args) = @_;

	my %results;
	my $handler = sub {
		my ($line) = @_;

		chomp($line);
		if (my ($key, $value) = split(/\s*:\s*/, $line, 2)) {
			$results{$key} = hex($value);
		}
	};

	my @a;
	foreach my $a (@args) {
		push(@a, '-p', $a);
	}

	$me->elfdatamod($handler, @a);

	return (wantarray) ? %results : \%results;
}

sub set
{
	my $me = shift;
	my (%args) = @_;

	my $handler = sub {return};

	my @a;
	foreach my $k (keys %args) {
		my $v = $args{$k};

		push(@a, '-m', sprintf("%s:%s", $v, $k));
	}

	$me->elfdatamod($handler, @a);
}

sub elfdatamod
{
	my $me = shift;
	my ($handler, @args) = @_;

	my $errfh = $me->quiet();
	my $cmd = $me->{CMD};
	my $file = $me->getFile();

	push(@args, $file);
	read_from_process($handler, $errfh, $cmd, @args);
}

sub quiet
{
	my $me = shift;
	my ($arg) = @_;

	my $ret = $me->{ERRFH};
	if (defined($arg)) {
		if ($arg) {
			$me->{ERRFH} = FileHandle->new("/dev/null") or
				die "open(/dev/null) failed: $!\n";
		}
		else {
			delete($me->{ERRFH});
		}
	}

	return $ret;
}

sub getEntry
{
	my $me = shift;

	my $class = $me->getClass();
	my $data = $me->{EHDR};
	my $ident = $me->{IDENT};

	my $order = get_host_byteorder();
	my $hostenc = ($order == LITTLE_ENDIAN) ? ELFDATA2LSB : ELFDATA2MSB;
	my $bswap = ($hostenc != $ident->[EI_DATA()]);
	my $entry;
	if ($class == ELFCLASS32) {
		my $tmpl = sprintf("C%dS2L5S6", EI_NIDENT);
		my (@hdr) = unpack($tmpl, $data);

		$entry = $hdr[EI_NIDENT + 3];
		$entry = bswap_32($entry) if ($bswap);
	}
	else {
		my $tmpl = sprintf("C%dS2LQ3LS6", EI_NIDENT);
		my (@hdr) = unpack($tmpl, $data);
		$entry = $hdr[EI_NIDENT + 3];
		$entry = bswap_64($entry) if ($bswap);
	}

	return $entry;
}

1;
