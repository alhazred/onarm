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
## Constructor of unified U-boot image.
##

package XRamConf::Image::arm;

use strict;
use FileHandle;
use File::stat;
use POSIX;
use POSIX qw(:errno_h);

use XRamConf::ByteOrder;
use XRamConf::Constants;
use XRamConf::Constants::arm;
use XRamConf::Kernel::arm;
use XRamConf::ProcUtil;
use XRamConf::Struct::WbInfo;
use XRamConf::Struct::WbCmd;

use constant	BUFSIZE			=> 0x10000;

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($vmunix, $parser) = @_;

	my $me = bless {PARSER => $parser, UNLINK => []}, $class;
	$me->initialize($vmunix);
	return $me;
}

sub initialize
{
	my $me = shift;
	my ($vmunix) = @_;

	unless ($vmunix) {
		my $src = $ENV{SRC} or die "\"SRC\" is not defined.\n";
		my $plat = $ENV{ARM_PLATFORM} or
			die "\"ARM_PLATFORM\" is not defined.\n";
		my ($objdir, $label);
		my $objdir = (defined($ENV{RELEASE_BUILD}))
			? 'obj32' : 'debug32';
		$vmunix = "$src/uts/$plat/unix/$objdir/vmunix";
	}

	$me->{KERN} = XRamConf::Kernel::arm->new($vmunix);
	$me->{VMUNIX} = $vmunix;
}

sub tempfile
{
	my $me = shift;
	my ($prefix) = @_;

	my $fname = $prefix . '.' . $$;
	my $fh = FileHandle->new($fname, O_WRONLY|O_CREAT|O_TRUNC, 0644) or
		die "open($fname) failed: $!\n";
	push(@{$me->{UNLINK}}, $fname);
	binmode($fh);

	return ($fname, $fh);
}

sub output
{
	my $me = shift;
	my (%args) = @_;

	$me->{KEEP} = $args{keep};
	$me->{QUIET} = $args{quiet};

	my $kern = $me->{KERN};
	my $parser = $me->{PARSER};
	my $config = $kern->fetchConfig();
	die "No xramfs device is configured in the kernel.\n"
		unless (defined($config));

	my $kernel = $args{kernel};
	my $infofile = $args{infofile};
	my $unified = $args{unified};

	# Import symbols from kernel.
	my $elf = $kern->getElf();
	my $stextoffkey = '%__text_start__';
	my $sbsskey = '^.bss';
	my $results = $elf->get(SYM_XRAMDEV_CONFTIME, SYM_KERNELBASE,
				$stextoffkey, SYM_KERNELPHYSBASE,
				SYM_PAGESHIFT, SYM_PAGEMASK,
				SYM_CTF_END_ADDRESS, $sbsskey,
				SYM_DATA_PADDR_BASE, SYM_DATA_PADDR);
	my $stamp = $results->{SYM_XRAMDEV_CONFTIME()};
	my $pstamp = $parser->getMtime();

	# Check timestamp of configuration file.
	if ($stamp != $pstamp) {
		my $fdate = scalar(localtime($pstamp));
		my $kdate = scalar(localtime($stamp));
		die <<OUT;
Timestamp of configuration file does not match.
    The kernel image must be rebuilt.
      Configuration file: $fdate
      Kernel            : $kdate
OUT
	}

	my $mkimage = $args{mkimage} || find_program('mkimage');
	die "mkimage command ($mkimage) is not found.\n"
		unless (-x $mkimage);

	# Create temporary file to create unified U-boot image.
	my ($tmpfile, $tmp) = $me->tempfile($unified . '_tmp')
		if ($unified);

	# Setup warm boot information image.
	my $wbinfo = XRamConf::Struct::WbInfo->new();

	# Dump kernel image into temporary file.
	$me->message("\n*** unix: %s\n", $me->{VMUNIX});
	$me->message("*** Kernel boot image : %s\n", $kernel);
	$me->message("*** Unified U-boot image: %s\n\n", $unified);

	my $baseoff = $results->{$stextoffkey};
	my $physbase = $results->{SYM_KERNELPHYSBASE()};
	my $basepaddr = $physbase + $baseoff;
	my $data_end = $results->{SYM_CTF_END_ADDRESS()};
	my $bss_start = $physbase + $results->{$sbsskey};
	$data_end = $bss_start unless ($data_end);
	{
		my $vmunix = $elf->getFile();
		my $vfh = FileHandle->new($vmunix) or
			die "open($vmunix) failed: $!\n";
		binmode($vfh);
		my $size = $data_end - $basepaddr;
		my $k = $parser->getKernel();

		my $digest = $k->getDigestInstance() if ($k);
		my $kfh = FileHandle->new($kernel, O_WRONLY|O_CREAT|O_TRUNC,
					  0644)
			or die "open($kernel) failed: $!\n";
		$me->message("*** Dumping kernel boot image into \"%s\"...\n",
			     $kernel);
		$me->addImage(in => $vfh, out => $kfh, size => $size,
			      inoff => $baseoff, outoff => 0,
			      label => "kernel", basepaddr => $basepaddr,
			      digest => $digest);
		$wbinfo->addFile($kernel, $basepaddr, $size);
		$wbinfo->addDigest($digest->getDigestType(), 0, $basepaddr,
				   $size, $digest->getValue()) if ($digest);

		if ($unified) {
			$me->addImage(in => $vfh, out => $tmp, size => $size,
				      inoff => $baseoff, outoff => 0,
				      basepaddr => $basepaddr);
		}
	}

	my $index = 0;
	my $pageshift = $results->{SYM_PAGESHIFT()};
	foreach my $dev (@$config) {
		my $cf = $parser->getDevice($index);
		my $file = $cf->getImageFile();
		next unless (defined($file));

		my $name = $dev->{name};
		my $fh = FileHandle->new($file);
		die "Can't open image file for \"$name\": $file: $!\n"
			unless ($fh);
		binmode($fh);

		my $devsize = $dev->{count} << $pageshift;
		my $sb = stat($fh);
		my $sz = $sb->size;
		die "Too large image ($sz) for device \"$name\" ($devsize)\n"
			if ($sz > $devsize);

		my $devaddr = $dev->{base} << $pageshift;
		my $off = $devaddr - $basepaddr;
		my $digest = $cf->getDigestInstance();
		$me->addImage(in => $fh, out => $tmp, size => $sz, inoff => 0,
			      outoff => $off, label => "xramdev ($name)",
			      basepaddr => $basepaddr,
			      digest => $digest);
		my $paddr = $basepaddr + $off;
		$wbinfo->addFile($file, $paddr, $sz);
		if ($digest) {
			$wbinfo->addDigest($digest->getDigestType(), 0, 
					   $paddr, $sz, $digest->getValue());
		}
	}
	continue {
		$index++;
	}

	undef $tmp;

	my $kernelbase = $results->{SYM_KERNELBASE()};
	my $entry = $elf->getEntry() - $kernelbase + $physbase;
	$wbinfo->set('wb_entry', $entry);

	if ($tmpfile) {
		# Dump unified U-boot image.
		$me->message("\n*** Dumping unified U-boot image into " .
			     "\"%s\"...\n", $unified);
		$me->mkimage($mkimage, $tmpfile, $unified, $basepaddr, $entry,
			     \%args);
	}

	# Create copy command instance that indicates copy data section,
	# and append it into warm boot information.
	# Note that we don't need to copy CTF data. It will be initialized
	# by kernel.
	my $pagemask = $results->{SYM_PAGEMASK()};
	my $data_paddr = $results->{SYM_DATA_PADDR()} & $pagemask;
	my $data_paddr_base = $results->{SYM_DATA_PADDR_BASE()} & $pagemask;
	my $dcopy = XRamConf::Struct::WbCmd->new(WBCT_COPY);
	$dcopy->set('wbcc_from', $data_paddr_base);
	$dcopy->set('wbcc_to', $data_paddr);
	$dcopy->set('wbcc_size', $bss_start - $data_paddr_base);

	# Data section copy needs to be done on every warm boot.
	$dcopy->set('wbc_flags', WBCF_PRE_ENTRY);

	$wbinfo->addCommand($dcopy);

	# Dump warm boot information file.
	my $fh = FileHandle->new($infofile, O_WRONLY|O_CREAT|O_TRUNC, 0644) or
		die "open($infofile) failed: $!\n";
	binmode($fh);

	$me->message("\n*** Dumping warm boot information into \"%s\"...\n",
		     $infofile);
	my $info = $wbinfo->encode(LITTLE_ENDIAN);
	my $infolen = length($info);
	die "Too large warm boot information: $infolen\n"
		if ($infolen > MAX_WBINFO_SIZE);
	$me->writeFile($fh, $info, $infolen);
}

sub addImage
{
	my $me = shift;
	my (%args) = @_;

	my $in = $args{in};
	my $inoff = $args{inoff};
	my $out = $args{out};
	my $outoff = $args{outoff};
	my $size = $args{size};
	my $label = $args{label};
	my $basepaddr = $args{basepaddr};
	my $digest = $args{digest};

	my $dgvalue;

	sysseek($in, $inoff, SEEK_SET) or
		die "addImage: input: lseek($inoff) failed: $!\n";
	if ($out) {
		sysseek($out, $outoff, SEEK_SET) or
			die "addImage: output: lseek($outoff) failed: $!\n";
	}

	my $osize = $size;
	while ($size > 0) {
		my $bufsize = ($size > BUFSIZE) ? BUFSIZE : $size;
		my $data;
		my $rsz = sysread($in, $data, $bufsize);
		unless (defined($rsz)) {
			next if ($! == EINTR);
			die "addImage: input: read() failed: $!\n"
		}
		last if ($rsz == 0);

		if ($digest) {
			$digest->add($data);
		}
		unless ($out) {
			$size -= $rsz;
			next;
		}

		$size -= $rsz;
		$data = $me->writeFile($out, $data, $rsz);
	}

	die "addImage: unexpected EOF: rest=$size\n" if ($size);
	$me->message("    %-20s: offset=0x%08x, paddr=0x%08x, size=0x%08x\n",
		     $label, $outoff, $outoff + $basepaddr, $osize)
		if ($label);
	if ($digest) {
		my $hex = $digest->hexdigest();
		$me->message("    %-20s  digest=0x%s\n", '', $hex)
			if ($label);
	}
}

sub writeFile
{
	my $me = shift;
	my ($out, $data, $size) = @_;

	while (1) {
		my $wsz = syswrite($out, $data, $size);
		unless (defined($wsz)) {
			next if ($! == EINTR);
			die "writeFile: write() failed: $!\n";
		}
		$size -= $wsz;
		last unless ($size > 0);
		$data = substr($data, $wsz);
	};

	return $data;
}

sub mkimage
{
	my $me = shift;
	my ($mkimage, $input, $output, $load, $entry, $args) = @_;

	my ($handler, $errfh);
	if ($args->{quiet}) {
		$errfh = FileHandle->new("/dev/null") or
			die "open(/dev/null) failed: $!\n";
		$handler = sub {};
	}
	else {
		$handler = sub { print; };
	}
	read_from_process($handler, $errfh, $mkimage, @{$args->{image_args}},
			  '-d', $input, '-e', sprintf("0x%x", $entry),
			  '-a', sprintf("0x%x", $load), $output);
}

sub message
{
	my $me = shift;

	printf(@_) unless ($me->{QUIET});
}

sub DESTROY
{
	my $me = shift;

	unless  ($me->{KEEP}) {
		# Remove temporary files.
		foreach my $f (@{$me->{UNLINK}}) {
			unlink($f);
		}
	}
}

1;
