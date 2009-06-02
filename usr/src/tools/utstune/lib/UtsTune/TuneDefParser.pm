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
# Copyright (c) 2007-2009 NEC Corporation
# All rights reserved.
#

##
## Tune definition file parser.
##

package UtsTune::TuneDefParser;

use strict;
use vars qw(@ISA);

use Cwd qw(abs_path);
use FileHandle;
use File::Copy;
use POSIX;
use File::Basename;
use File::stat;

use UtsTune;
use UtsTune::ParameterParser;
use UtsTune::Block::Option;

use constant	TOKEN_CHAR	=> {
	';' => 1, '{' => 1, '}' => 1, ':' => 1, '"' => 1, '#' => 1, ',' => 1
};

@ISA = qw(UtsTune::ParameterParser);

# Parser state
use constant	S_BEGIN		=> 1;
use constant	S_OPTION	=> 2;
use constant	S_OPTTYPE	=> 3;
use constant	S_OPTNAME	=> 4;
use constant	S_OPTNAME_END	=> 5;
use constant	S_PARAM		=> 6;
use constant	S_PARAM_COLON	=> 7;
use constant	S_PARAM_COMMA	=> 8;
use constant	S_PARAM_VALUE	=> 9;
use constant	S_PARAM_END	=> 10;

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my (%args) = @_;

	my $file = $args{FILE};
	my $def = $args{ISDEF};
	my $prt = $args{PRINT};

	my ($fh, $mtime);
	if (-f $file) {
		$fh = FileHandle->new($file) or die "open($file) failed: $!\n";
		$fh->input_line_number(1);
		my $sb = stat($fh);
		$mtime = $sb->mtime();
	}

	my $path = abs_path($file);
	die "$file is not located under kernel source tree.\n"
		unless (check_utstree($path));
	my $me = {FILE => $file, MTIME => $mtime, HANDLE => $fh,
		  PATH => $path, CHAR => [], OPTIONS => {},
		  UNPARSED => [], DEF => $def, PRINT => $prt};
	$me = bless $me, $class;

	$me->parse();

	return $me;
}

sub debug
{
	my $me = shift;
	my $prt = $me->{PRINT};
	return unless ($prt);

	$prt->debug(@_);
}

sub print
{
	my $me = shift;
	my $prt = $me->{PRINT};
	return unless ($prt);

	$prt->print(@_);
}

sub isDefinition
{
	my $me = shift;

	return $me->{DEF};
}

sub getMtime
{
	my $me = shift;

	return $me->{MTIME};
}

sub getPath
{
	my $me = shift;

	return $me->{PATH};
}

sub nextToken
{
	my $me = shift;
	my ($needed) = @_;

	my $unparsed = pop(@{$me->{UNPARSED}});
	return @$unparsed if (defined($unparsed));

	my ($skip, $c);
	while (defined($c = $me->getChar())) {
		# Skip newline.
		if ($c eq "\n") {
			undef $skip;
			next;
		}

		# Skip whitespace.
		next if ($c =~ /^\s$/);

		# Skip comment.
		if ($c eq '#') {
			$skip = 1;
			next;
		}
		next if ($skip);
		last;
	}

	unless (defined($c)) {
		$me->syntaxError("Unexpected EOF.") if ($needed);
		return undef;
	}

	return $me->tokenize($c);
}

sub ungetToken
{
	my $me = shift;
	my ($token, $type) = @_;

	push(@{$me->{UNPARSED}}, [$token, $type]);
}

sub tokenize
{
	my $me = shift;
	my ($c) = @_;

	my ($token, $type);
	if ($c eq ';') {
		$type = $me->SEMI;
		$token = $c;
	}
	elsif ($c eq "{") {
		$type = $me->LBRACE;
		$token = $c;
	}
	elsif ($c eq "}") {
		$type = $me->RBRACE;
		$token = $c;
	}
	elsif ($c eq ':') {
		$type = $me->COLON;
		$token = $c;
	}
	elsif ($c eq ',') {
		$type = $me->COMMA;
		$token = $c;
	}
	elsif ($c eq '-' or $c =~ /^\d$/o) {
		($token, $type) = $me->parseInt($c);
	}
	elsif ($c eq '"') {
		$token = $me->parseString($c);
		$type = $me->STRING;
	}
	else {
		$token = $me->parseSymbol($c);
		if ($token eq 'true') {
			$type = $me->TRUE;
		}
		elsif ($token eq 'false') {
			$type = $me->FALSE;
		}
		else {
			$type = $me->SYMBOL;
		}
	}
	return ($token, $type);
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
	my $path = utspath(abs_path($file));
	my $err = join("", @_);
	die "$path: $line: $err\n";
}

sub isTokenChar
{
	my $me = shift;
	my ($c) = @_;

	return TOKEN_CHAR->{$c};
}

sub getHandle
{
	my $me = shift;

	return $me->{HANDLE};
}

sub lineNumber
{
	my $me = shift;

	my $fh = $me->getHandle();
	return 1 unless ($fh);
	return $fh->input_line_number();
}

sub getChar
{
	my $me = shift;

	my $fh = $me->getHandle();
	return undef unless ($fh);
	my $c = pop(@{$me->{CHAR}});
	$c = $fh->getc() unless (defined($c));
	if ($c eq "\n") {
		my $line = $fh->input_line_number();
		$fh->input_line_number($line + 1);
	}

	return $c;
}

sub ungetChar
{
	my $me = shift;
	my ($c) = (@_);

	if ($c eq "\n") {
		my $fh = $me->getHandle();
		return unless ($fh);
		my $line = $fh->input_line_number();
		$fh->input_line_number($line - 1);
	}

	push(@{$me->{CHAR}}, $c);
}

sub parse
{
	my $me = shift;
	my $state = S_BEGIN;
	my ($name, $param, $pname);

	LOOP:
	while (1) {
		my $needed = ($state != S_BEGIN);
		my ($token, $type) = $me->nextToken($needed);
		last unless (defined($type));

		if ($state == S_BEGIN) {
			if ($token eq 'option') {
				# Definition of option.
				$state = $me->parseOption();
				next LOOP;
			}
			if ($me->isDefinition()) {
				if ($token eq 'static') {
					# Static module configuration script.
					$state = $me->parseUniqueBlock($token);
					next LOOP;
				}
				if ($token eq 'makeopt') {
					# Makefile option configuration script.
					$state = $me->parseUniqueBlock($token);
					next LOOP;
				}
				if ($token eq 'export') {
					# Define scope.
					$state = $me->parseUniqueBlock($token);
					next LOOP;
				}
			}
			elsif ($token eq 'meta') {
				# Meta data in tune file.
				$state = $me->parseUniqueBlock($token);
				next LOOP;
			}
			$me->syntaxError("Unknown block name: $token");
		}
	}

	$me->parseError("No meta data.")
		unless ($me->isDefinition() or $me->{META});
	$me->checkAlsoDefine() if ($me->isDefinition());
}

sub checkAlsoDefine
{
	my $me = shift;

	my $omap = $me->{OPTIONS};
	foreach my $name (keys %$omap) {
		my $opt = $omap->{$name};

		my $list = $opt->getParameter('also-define');
		next unless ($list and @$list);

		my $type = $opt->getType();
		foreach my $ad (@$list) {
			my $adopt = $omap->{$ad};

			$me->parseError("$name: 'also-define' option ($ad) ",
					"must be defined in the same file.")
				unless ($adopt);
			$me->parseError("$name: nested 'also-define' ($ad) ",
					"is not allowed.")
				if ($adopt->getParameter('also-define'));

			foreach my $odir (get_build_objdirs()) {
				my $b = $opt->
					getParameter('default', $odir);
				next unless ($type->isTrue($b));

				my $b1 = $adopt->
					getParameter('default', $odir);
				
				$me->parseError("$name: Default value of ",
						"'also-define' option ($ad) ",
						"must be true.")
					unless ($type->isTrue($b1));
			}
		}
	}
}

sub checkAlsoDefineValue
{
	my $me = shift;
	my ($objdir) = @_;

	my $omap = $me->{OPTIONS};
	foreach my $name (keys %$omap) {
		my $opt = $omap->{$name};

		my $list = $opt->getParameter('also-define');
		next unless ($list and @$list);

		my $type = $opt->getType();
		foreach my $ad (@$list) {
			my $adopt = $omap->{$ad};

			my $v1 = $opt->getValue($objdir);
			my $v2 = $adopt->getValue($objdir);
			if ($type->isTrue($v1) and !$type->isTrue($v2)) {
				die <<OUT;
$ad must not be false because $name is still true.
OUT
			}
		}
	}
}

sub parseParameter
{
	my $me = shift;
	my ($opt) = @_;

	my $state = S_PARAM;
	my ($pname, @param);

	LOOP:
	while (1) {
		my ($token, $type) = $me->nextToken(1);

		if ($state == S_PARAM) {
			if ($type == $me->RBRACE) {
				# End of definition of option.
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

				$opt->addParameter($me, $pname, \@param);
				$state = S_PARAM;
				undef $pname;
				undef @param;
			}
			next LOOP;
		}
	}

	return $state;
}

sub parseOption
{
	my $me = shift;

	my $state = S_OPTION;
	my ($opt, $name, $opttype);

	
	LOOP:
	while (1) {
		my ($token, $type) = $me->nextToken(1);

		if ($state == S_OPTION) {
			$me->syntaxError("\"option\" requires ",
					 "parameter type: $token")
				unless ($type == $me->SYMBOL);

			my $class = TYPE_CLASS()->{$token};
			$me->syntaxError("Unknown type: : $token")
				unless ($class);
			my $load = "require $class; import $class;";
			eval $load;
			$opttype = $class->new();
			$state = S_OPTNAME;
			next LOOP;
		}
		if ($state == S_OPTNAME) {
			$me->syntaxError("\"option\" requires name: ",
					 $token)
				unless ($type == $me->SYMBOL);
			$me->parseError("Duplicated option name: ",
					$token)
				if ($me->{OPTIONS}->{$token});
			$name = $token;
			$opt = UtsTune::Block::Option->new($token, $opttype);
			$state = S_OPTNAME_END;
			next LOOP;
		}
		if ($state == S_OPTNAME_END) {
			$me->syntaxError("'{' is required.")
				unless ($type == $me->LBRACE);
			$state = $me->parseParameter($opt);
			last LOOP;
		}
	}

	# Check whether parameters are valid.
	$opt->fixup($me);

	$me->{OPTIONS}->{$name} = $opt;
	return $state;
}

sub parseUniqueBlock
{
	my $me = shift;
	my ($block) = @_;

	my $key = uc($block);
	$me->parseError("Duplicated \"$block\" block.") if ($me->{$key});
	my $class = 'UtsTune::Block::' . ucfirst($block);
	my $load = "require $class; import $class;";
	eval $load;
	my $obj = eval { return $class->new(); };
	if ($@) {
		my $err = "$@";
		chomp($err);
		$me->parseError($err);
	}
	my ($token, $type) = $me->nextToken(1);
	$me->syntaxError("'{' is required.")
		unless ($type == $me->LBRACE);

	my $state = $me->parseParameter($obj);

	# Check whether parameters are valid.
	$obj->fixup($me);

	$me->{$key} = $obj;

	return $state;
}

sub output
{
	my $me = shift;
	my ($output, $plat, $objdir, $db) = @_;

	my $bkup;
	if (-f $output) {
		# Backup current configuration.
		$db->lock();
		$bkup = $output . '.bkup';
		move($output, $bkup) or
			die "move($output, $bkup) failed: $!\n";
		$me->{BACKUP} = [$output, $bkup];
	}
	my $fh = FileHandle->new($output, O_WRONLY|O_CREAT|O_TRUNC,
				 0644)
		or die "open($output) failed: $!\n";
	my $path;
	my $opath = abs_path($output);
	my $meta = $me->{META};
	if ($meta) {
		$path = $me->getDefPath();
	}
	else {
		# Create meta data.
		require UtsTune::Block::Meta;
		import UtsTune::Block::Meta;

		$meta = UtsTune::Block::Meta->new();
		$meta->setObjDir($objdir);
		$path = $me->getPath();
		my $p = $path;
		$p = "" unless ($me->getMtime());
		$meta->setFile($p);

		my $export = $me->getExport();
		if ($export) {
			my $func = sub {
				my ($funcname, $label, $value) = @_;

				eval {
					$db->$funcname($value, $path);
				};
				if ($@) {
					my $oldpath = "$@";

					chomp($oldpath);
					die <<OUT;
"export" block for $label is already defined in $oldpath.
OUT
				}
			};
			my $arch = $export->getArchitectureScope();
			if ($arch) {
				&$func('addArchSpec', 'architecture', $arch);
			}
			else {
				my $pl = $export->getPlatformScope();
				&$func('addPlatSpec', 'platform', $pl);
				$db->addPlatformObjDir($pl, $objdir);
				$meta->setPlatform($pl);
			}
		}
	}

	$fh->print(<<OUT);
##
## WARNING: NEVER EDIT THIS FILE!!!
##

OUT
	$meta->output($fh);

	# Dump options.
	# If no value is defined, use default value.
	my $omap = $me->{OPTIONS};
	foreach my $name (sort keys %$omap) {
		my $opt = $omap->{$name};

		$db->addOption($name, $plat, $path, $objdir, $opath);

		$fh->print("\n");
		$opt->output($fh);
	}
	$me->updateEfile($opath);

	unlink($bkup);
	delete($me->{BACKUP});
}

sub getEfile
{
	my $me = shift;
	my ($output) = @_;

	my $dir = dirname($output);
	return $dir . '/' . TUNE_EFILE;
}

sub updateEfile
{
	my $me = shift;
	my ($output) = @_;

	if ($me->isDefinition()) {
		my $efile = $me->getEfile($output);
		unlink($efile);
	}
}

sub merge
{
	my $me = shift;
	my ($parser, $plat, $objdir, $db) = @_;

	# Eliminate removed options.
	my $curmap = $parser->{OPTIONS};
	my $omap = $me->{OPTIONS};
	my (@list);
	foreach my $name (keys %$curmap) {
		push(@list, $name) unless ($omap->{$name});
	}
	foreach my $name (@list) {
		$me->debug("Option removed: $name");
		delete($curmap->{$name});
		$db->removeOption($name, $plat);
	}

	foreach my $name (keys %$omap) {
		my $curopt = $curmap->{$name};
		my $opt = $omap->{$name};
		if ($curopt) {
			# Check whether the current value is still
			# valid for new option.
			my $value = $curopt->getParameter('value');
			next if ($opt->equals($curopt));
			eval {
				$opt->checkValue($value, $objdir)
					if (defined($value));
				my $t1 = ref($opt->getType());
				my $t2 = ref($curopt->getType());
				die "Type changed\n"
					unless ($t1 eq $t2);
			};
			if ($@) {
				$me->print(<<OUT);

NOTICE: $name: Reset to default value.

OUT
			}
			else {
				$opt->setValue($value);
			}
			$curmap->{$name} = $opt;
		}
		else {
			# Insert new option.
			$me->debug("Merge new option: $name");
			$curmap->{$name} = $opt;
		}
	}

	$me->{OPTIONS} = $curmap;
}

sub header
{
	my $me = shift;
	my ($output) = @_;

	my $fh = FileHandle->new($output, O_WRONLY|O_CREAT|O_TRUNC,
				 0644)
		or die "open($output) failed: $!\n";

	my $meta = $me->{META};
	my $deffile = $meta->getFile();
	my $objdir = $meta->getParameter('objdir');
	$objdir = $me->evalString($objdir);

	my $guard = abs_path($output);
	my $uts = $UtsTune::SRC . '/uts';
	$guard =~ s/^\Q$uts\E//o;
	$guard =~ s,^/*,,go;
	$guard = uc($guard);
	$guard =~ s,[/.],_,g;
	$guard = '_UTSTUNE_' . $guard;

	return unless ($deffile);

	$deffile =~ s/^\Q$uts\E//o;
	$deffile =~ s,^/*,,go;

	my $macro = CONCAT();

	$fh->print(<<OUT);
/*
 * Configurations derived from $deffile.
 */

#ifndef	$guard
#define	$guard

#include <sys/int_const.h>

#ifdef	__cplusplus
extern "C" {
#endif	/* __cplusplus */
OUT

	# Dump options.
	# If no value is defined, use default value.
	my $omap = $me->{OPTIONS};
	foreach my $name (sort keys %$omap) {
		my $opt = $omap->{$name};

		$fh->print("\n");
		my $v = $opt->getValue($objdir);
		$opt->checkValue($v, $objdir);
		my $otype = $opt->getType();
		$otype->dumpMacro($fh, $opt->getName(), $v);
	}

	$fh->print(<<OUT);

#ifdef	__cplusplus
}
#endif	/* __cplusplus */

#endif	/* $guard */
OUT
}

sub getDefPath
{
	my $me = shift;

	my $meta = $me->{META};
	my $path = $meta->getFile() if ($meta);

	return $path;
}

sub hasValue
{
	my $me = shift;
	my ($name) = @_;

	my $opt = $me->{OPTIONS}->{$name};
	return $opt->hasValue();
}

sub getOption
{
	my $me = shift;
	my ($name) = @_;

	return $me->{OPTIONS} unless (defined($name));
	return $me->{OPTIONS}->{$name};
}

sub getStatic
{
	my $me = shift;

	return $me->{STATIC};
}

sub getMakeOpt
{
	my $me = shift;

	return $me->{MAKEOPT};
}

sub getExport
{
	my $me = shift;

	return $me->{EXPORT};
}

sub getPlatformScope
{
	my $me = shift;

	my $meta = $me->{META};
	return $meta->getPlatform() if ($meta);

	my $export = $me->getExport() or return undef;
	return $export->getPlatformScope();
}

# Check whether input file has been modified after "tune-init".
sub updateCheck
{
	my $me = shift;
	my $objdir = shift;

	my $inmtime = $me->getMtime();
	my $modtune = $me->getPath();
	my $stamp = stat($UtsTune::INIT_STAMP);
	if (!$stamp or $stamp->mtime() < $inmtime) {
		my $file = utspath($modtune);
		my $msg = <<OUT;
$file has been modified. Try "make tune-init" under \$SRC/uts.
OUT
		die $msg;
	}
}

sub DESTROY
{
	my $me = shift;

	my $bk = $me->{BACKUP};
	return unless ($bk);

	# Restore configuration from backup.
	my ($output, $bkup) = (@$bk);
	move($bkup, $output);

	my $efile = $me->getEfile(abs_path($output));
	unlink($efile);
}

1;
