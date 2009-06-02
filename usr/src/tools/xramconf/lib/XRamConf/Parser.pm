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
## Parser for xramfs device configuration file.
##

package XRamConf::Parser;

use strict;
use vars qw(@ISA @HEADERS);

use FileHandle;
use File::stat;
use File::Basename;

use UtsTune::Parser;
use UtsTune::TuneDefParser;
use XRamConf::Block::Kernel;
use XRamConf::Block::XRamDev;
use XRamConf::Block::UserData;
use XRamConf::Constants;
use XRamConf::Parser::Passwd;
use XRamConf::Parser::Group;

@ISA = qw(UtsTune::TuneDefParser);	# Piggyback utstune parser.
@HEADERS = qw(sys/xramdev.h sys/param.h vm/hat.h);

# Parser state
use constant	S_BEGIN		=> 1;
use constant	S_XRAMDEV	=> 2;
use constant	S_XRAMDEV_END	=> 3;
use constant	S_PARAM		=> 4;
use constant	S_PARAM_COLON	=> 5;
use constant	S_PARAM_COMMA	=> 6;
use constant	S_PARAM_VALUE	=> 7;
use constant	S_PARAM_END	=> 8;
use constant	S_USERDATA	=> 9;
use constant	S_USERDATA_END	=> 10;

# use constant	OCTAL		=> (100 | UtsTune::Parser::TOKEN_VALUE);

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($file) = @_;

	my $fh = FileHandle->new($file) or die "open($file) failed: $!\n";
	$fh->input_line_number(1);

	my $sb = stat($fh) or die "stat($file) failed: $!\n";
	my $mtime = $sb->mtime();

	my $me = bless {FILE => $file, HANDLE => $fh, CHAR => [],
			DEVICES => {}, MTIME => $mtime, DEVLIST => [],
			UNPARSED => [], USERDATA => []}, $class;

	return $me;
}

sub parseError
{
	my $me = shift;

	my $line = $me->lineNumber();
	my $file = $me->{FILE};
	my $err = join("", @_);
	die "$file: $line: $err\n";
}

sub tokenize
{
	my $me = shift;
	my ($c) = @_;

	my $ptype = $me->{PARSE_TYPE};
	if ($ptype) {
		delete($me->{PARSE_TYPE});
		return $ptype->tokenize($me, $c) if ($ptype->can('tokenize'));
	}

	return $me->SUPER::tokenize($c);
}

sub getPasswd
{
	my $me = shift;

	return $me->{_PASSWD};
}

sub getGroup
{
	my $me = shift;

	return $me->{_GROUP};
}

sub parse
{
	my $me = shift;
	my $state = S_BEGIN;
	my ($name, $param, $pname);

	return if ($me->{_PARSED});
	$me->{_PARSED} = 1;

	$me->{_PASSWD} = XRamConf::Parser::Passwd->new();
	$me->{_GROUP} = XRamConf::Parser::Group->new();

	LOOP:
	while (1) {
		my $needed = ($state != S_BEGIN);
		my ($token, $type) = $me->nextToken($needed);
		last unless (defined($type));

		if ($state == S_BEGIN) {
			if ($token eq 'xramdev') {
				# Definition of xramfs device.
				$state = $me->parseDevice();
				next LOOP;
			}
			elsif ($token eq 'userdata') {
				# Definition of user data device.
				$state = $me->parseUserData();
				next LOOP;
			}
			elsif ($token eq 'kernel') {
				# Definition of properties for kernel.
				$state = $me->parseKernel();
				next LOOP;
			}
			$me->syntaxError("Unknown block name: $token");
		}
	}
}

sub parseParameter
{
	my $me = shift;
	my ($dev) = @_;

	my $state = S_PARAM;
	my ($pname, @param);

	LOOP:
	while (1) {
		my ($token, $type) = $me->nextToken(1);

		if ($state == S_PARAM) {
			if ($type == $me->RBRACE) {
				# End of xramdev block.
				$state = S_BEGIN;
				last LOOP;
			}
			$me->syntaxError("parameter name is ",
					 "required.")
				unless ($type == $me->SYMBOL);
			$pname = $token;
			$state = S_PARAM_COLON;
			next LOOP;
		}
		if ($state == S_PARAM_COLON) {
			$me->syntaxError("':' is required.")
				unless ($type == $me->COLON);
			$state = S_PARAM_VALUE;
			if ($me->{CURSTATE} == S_USERDATA_END and
			    defined($pname)) {
				my $ptype = $dev->getParamType($pname);
				next LOOP unless (defined($ptype));
				$me->{PARSE_TYPE} = $ptype;
			}
			next LOOP;
		}
		if ($state == S_PARAM_VALUE ||
		    $state == S_PARAM_COMMA) {
			$me->syntaxError("Value must be defined.")
				unless ($type & $me->TOKEN_VALUE);
			push(@param, [$token, $type]);
			$state = S_PARAM_END;
			next LOOP;
		}
		if ($state == S_PARAM_END) {
			if ($type == $me->COMMA) {
				$state = S_PARAM_COMMA;
			}
			else {
				$me->syntaxError("Semicolon is required.")
					unless ($type == $me->SEMI);

				$dev->addParameter($me, $pname, \@param);
				$state = S_PARAM;
				undef $pname;
				undef @param;
			}
			next LOOP;
		}
	}
	delete($me->{CURPARAM});

	return $state;
}

sub parseDevice
{
	my $me = shift;

	my $state = S_XRAMDEV;
	$me->{CURSTATE} = $state;
	my ($name, $dev);
	
	LOOP:
	while (1) {
		my ($token, $type) = $me->nextToken(1);

		if ($state == S_XRAMDEV) {
			$me->syntaxError("\"xramdev\" requires node name: ",
					 $token)
				unless ($type == $me->SYMBOL);

			$me->checkNodeName($token);
			$name = $token;
			$dev = XRamConf::Block::XRamDev->new($token);
			$state = S_XRAMDEV_END;
			next LOOP;
		}
		if ($state == S_XRAMDEV_END) {
			$me->syntaxError("'{' is required.")
				unless ($type == $me->LBRACE);
			$state = $me->parseParameter($dev);
			last LOOP;
		}
	}
	continue {
		$me->{CURSTATE} = $state;
	}
	delete($me->{CURSTATE});

	# Check whether parameters are valid.
	$dev->fixup($me);

	$me->{DEVICES}->{$name} = $dev;
	my $list = $me->{DEVLIST};
	$me->parseError("Too many xramdev") if (@$list >= XRAMDEV_MAXDEVS);
	push(@$list, $dev);
	return $state;
}

sub parseUserData
{
	my $me = shift;

	my $state = S_USERDATA;
	$me->{CURSTATE} = $state;
	my ($name, $dev);

	LOOP:
	while (1) {
		my ($token, $type) = $me->nextToken(1);

		if ($state == S_USERDATA) {
			$me->syntaxError("\"userdata\" requires node name: ",
					 $token)
				unless ($type == $me->SYMBOL);

			$me->checkNodeName($token);
			$name = $token;
			$dev = XRamConf::Block::UserData->new($token);
			$state = S_USERDATA_END;
			next LOOP;
		}
		if ($state == S_USERDATA_END) {
			$me->syntaxError("'{' is required.")
				unless ($type == $me->LBRACE);
			$state = $me->parseParameter($dev);
			last LOOP;
		}
	}
	continue {
		$me->{CURSTATE} = $state;
	}
	delete($me->{CURSTATE});

	# Check whether parameters are valid.
	$dev->fixup($me);

	$me->{DEVICES}->{$name} = $dev;
	my $list = $me->{USERDATA};
	$me->parseError("Too many userdata") if (@$list >= USERDATA_MAXDEVS);
	push(@$list, $dev);
	return $state;
}

sub parseKernel
{
	my $me = shift;

	$me->syntaxError("Duplicated \"kernel\" block.") if ($me->{KERNEL});
	my ($kern);

	my ($token, $type) = $me->nextToken(1);
	$me->syntaxError("'{' is required.") unless ($type == $me->LBRACE);
	$kern = XRamConf::Block::Kernel->new();
	my $state = $me->parseParameter($kern);

	# Check whether parameters are valid.
	$kern->fixup($me);

	$me->{KERNEL} = $kern;

	return $state;
}

sub rootFs
{
	my $me = shift;
	my ($dev) = @_;

	my $root = $me->{ROOTFS};
	$me->{ROOTFS} = $dev if (defined($dev));

	return $root;
}

sub getHeaders
{
	return (wantarray) ? @HEADERS : \@HEADERS;
}

sub output
{
	my $me = shift;
	my ($fh) = @_;

	$me->parse();

	my $header = join("\n", map {"#include <$_>"} $me->getHeaders());
	my $mtime = $me->getMtime();
	my $comma;
	my ($stampsym, $segsym, $countsym) =
		(SYM_XRAMDEV_CONFTIME, SYM_XRAMDEV_SEGS, SYM_XRAMDEV_SEGCNT);

	$fh->print(<<OUT);
/*
 * WARNING: Auto-generated file. NEVER EDIT THIS FILE!!!
 */

$header

/* Timestamp of configuration file. */
const time_t	$stampsym = $mtime;

/* xramfs device segments. */
xdseg_t	$segsym\[\] = {
OUT
	# Dump device configurations.
	my (@devlist) = map {$_->declare($me)} (@{$me->{DEVLIST}});
	$fh->print(join(",\n", @devlist));
	$fh->print(<<OUT);

};

/* Number of xramfs devices. */
const uint_t	$countsym = sizeof($segsym) / sizeof(xdseg_t);

OUT

	$me->outputImpl($fh);
}

sub outputImpl
{
}

sub devlinkImpl
{
}

sub getMapAttr
{
	die "getMapAttr: Must be overridden";
}

sub checkNodeName
{
	my $me = shift;
	my ($name) = @_;

	$me->syntaxError("Duplicated node name: $name")
		if ($me->{DEVICES}->{$name});

	$me->checkSymbol($name, 'node name', XRAMDEV_MAX_NAMELEN);
}

sub checkSymbol
{
	my $me = shift;
	my ($name, $label, $maxlen) = @_;

	$me->syntaxError("Too long $label: $name")
		if (length($name) >= $maxlen);

	my $ll = ucfirst($label);
	$me->syntaxError("$ll must start with alphabet: $name")
		unless ($name =~ /^[a-zA-Z]/o);

	$me->syntaxError("$ll must end with alphabet or digit: $name")
		unless ($name =~ /[a-zA-Z\d]$/o);

	$me->syntaxError("Illegal character in $label: $name")
		if ($name =~ /[^a-zA-Z\d_]/o);
}

sub getDevice
{
	my $me = shift;
	my ($index) = @_;

	my $devs = $me->{DEVLIST};
	return $devs->[$index] if (defined($index));

	return (wantarray) ? @$devs : $devs;
}

sub getDeviceCount
{
	my $me = shift;

	return scalar(@{$me->{DEVLIST}});
}

sub getUserData
{
	my $me = shift;
	my ($index) = @_;

	my $ud = $me->{USERDATA};
	return $ud->[$index] if (defined($index));

	return (wantarray) ? @$ud : $ud;
}

sub getUserDataCount
{
	my $me = shift;

	return scalar(@{$me->{USERDATA}});
}

sub getKernel
{
	my $me = shift;

	return $me->{KERNEL};
}

# Unused TuneDefParser methods.
sub debug;
sub print;
sub isDefinition;
sub getPath;
sub parseUniqueBlock;
sub parseEnhanced;
sub getEfile;
sub updateEfile;
sub merge;
sub header;
sub getDefPath;
sub hasValue;
sub getOption;
sub getStatic;
sub getMakeOpt;
sub updateCheck;

1;
