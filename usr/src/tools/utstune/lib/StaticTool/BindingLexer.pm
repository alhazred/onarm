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

##
## Base class for binding file parser.
##

package StaticTool::BindingLexer;

use strict;
use FileHandle;
use File::Basename;

use constant	SEMI	=> 1;
use constant	EQUAL	=> 2;
use constant	COMMA	=> 3;
use constant	STRING	=> 4;
use constant	INT	=> 5;
use constant	SYMBOL	=> 6;
use constant	COLON	=> 7;
use constant	NEWLINE	=> 8;

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($file) = @_;

	my $fh = FileHandle->new($file) or die "open($file) failed: $!\n";
	my $me = {FILE => $file, HANDLE => $fh, LINE => undef};
	return bless $me, $class;
}

sub file
{
	my $me = shift;

	return $me->{FILE};
}

sub nextToken
{
	my $me = shift;

	my $unparsed = $me->{UNPARSED};
	if ($unparsed) {
		delete($me->{UNPARSED});
		return @$unparsed;
	}

	my $line = $me->{LINE};
	my ($type, $next);

	if (length($line)) {
		if ($line =~ /^\s*\x23/) {
			# Skip comment.
			undef $line;
		}
		else {
			# Chop off whitespaces at the top and end.
			$line =~ s/^[ \t]*//;
			$line =~ s/[ \t]*$//m;
			undef $line unless (length($line));
		}
	}
	$line = $me->getLine() unless (length($line));
	unless (defined($line)) {
		$me->verbose("end.\n");
		return undef;
	}

	my $len = length($line);
	my $token;
	($token, $type, $line) = $me->tokenize($line, $len);
	$me->{LINE} = $line;
	$me->verbose("token[$token] type[$type]\n");
	return ($token, $type);
}

sub tokenize
{
	my $me = shift;
	my ($line, $len) = @_;

	my $c = substr($line, 0, 1);
	my ($token, $type);
	if ($c eq ';') {
		$type = SEMI;
		$line = substr($line, 1);
		$token = $c;
	}
	elsif ($c eq "\n") {
		$type = NEWLINE;
		$line = substr($line, 1);
		$token = $c;
	}
	elsif ($c eq ':') {
		$type = COLON;
		$line = substr($line, 1);
		$token = $c;
	}
	elsif ($c eq '=') {
		$type = EQUAL;
		$line = substr($line, 1);
		$token = $c;
	}
	elsif ($c eq ',') {
		$type = COMMA;
		$line = substr($line, 1);
		$token = $c;
	}
	elsif ($c eq '"') {
		($token, $line) = $me->parseString($line, $len);
		$type = STRING;
	}
	elsif ($c =~ /^\d$/o) {
		($token, $line) = $me->parseInteger($line, $len);
		$type = INT;
	}
	else {
		($token, $line) = $me->parseSymbol($line, $len);
		$type = SYMBOL;
	}
	return ($token, $type, $line);
}

sub verbose
{
	my $me = shift;

	my $verbose = $me->{_VERBOSE};
	unless (defined($verbose)) {
		$verbose = eval {return $main::VERBOSE;};
		$verbose = 0 unless (defined($verbose));
		$me->{_VERBOSE} = $verbose;
	}

	if ($verbose) {
		my $file = basename($me->{FILE});
		print STDERR "Parser[$file]: ", @_;
	}
}

sub syntaxError
{
	my $me = shift;

	$me->parseError("Syntax Error: ", @_);
}

sub parseError
{
	my $me = shift;

	my $line = $me->lineNumber();
	my $file = $me->{FILE};
	my $err = join("", @_);
	die "$file: $line: $err\n";
}

sub parseString
{
	my $me = shift;
	my ($line, $len) = @_;

	my $i = 1;
	while ($i < $len) {
		my $c = substr($line, $i, 1);
		if ($c eq '\\') {
			# Currently, backslash
			if ($i + 1 >= $len) {
				$me->parseError("Invalid backslash ",
						"escape in string.");
			}
			$i += 2;
		}
		last if ($c eq '"');
		$i++;
	}
	$me->parseError("Unterminated string.") if ($i >= $len);
	my $index = $i + 1;
	my $token = substr($line, 0, $index);
	if ($i == $len - 1) {
		undef $line;
	}
	else {
		$line = substr($line, $index);
	}
	return ($token, $line);
}

sub parseInteger
{
	my $me = shift;
	my ($line, $len) = @_;

	my $token;
	if ($line =~ /^(0x[0-9a-f]+)(.*)$/ios) {
		$token = hex($1);
		$line = $2;
	}
	elsif ($line =~ /^(\d+)(.*)$/s) {
		$token = int($1);
		$line = $2;
	}
	else {
		$me->parseError("Invalid integer.");
	}
	
	return ($token, $line);
}

sub parseSymbol
{
	my $me = shift;
	my ($line, $len) = @_;

	my $symEnd;
	for (my $i = 0; $i < $len; $i++) {
		my $c = substr($line, $i, 1);
		if ($c eq ';' or $c eq '=' or $c eq ',' or $c eq ':' or
		    $c eq "\x23" or $c =~ /^\s$/) {
			$symEnd = $i;
			last;
		}
		if (ord($c) < 0x21 or $c eq '/' or $c eq '\\'  or $c eq ':' or
		    $c eq '[' or $c eq ']' or $c eq '@') {
			$me->parseError("Invalid string in SYMBOL: ",
					"[$line]: [$c]");
		}
	}
	my $token;
	if (defined($symEnd)) {
		$token = substr($line, 0, $symEnd);
		$line = substr($line, $symEnd);
	}
	else {
		$token = $line;
		undef $line;
	}
	return ($token, $line);
}

sub getLine
{
	my $me = shift;

	my $fh = $me->{HANDLE};
	my $line;
	while (1) {
		$line = $fh->getline();
		last unless (defined($line));
		next if ($line =~ /^\s*\x23/);	# Skip comment line.

		# Chop off whitespaces.
		$line =~ s/^[ \t]*//o;
		$line =~ s/[ \t]*$//o;
		last if ($line);
	}

	return $line;
}

sub lineNumber
{
	my $me = shift;

	my $fh = $me->{HANDLE};
	return $fh->input_line_number();
}

1;
