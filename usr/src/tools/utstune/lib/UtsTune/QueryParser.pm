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
# Copyright (c) 2008-2009 NEC Corporation
# All rights reserved.
#

##
## Parameter query parser.
##

package UtsTune::QueryParser;

use strict;
use vars qw(@ISA);

use UtsTune::ParameterParser;
use UtsTune::OptionLoader;
use UtsTune::Script;

@ISA = qw(UtsTune::ParameterParser);

use constant	TOKEN_CHAR	=> {
	'(' => 1, ')' => 1, '=' => 1, '<' => 1, '>' => 1, '!' => 1,
	'&' => 1, '|' => 1
};

use constant	OPERATOR_PRIORITY	=> {
	'=='	=> 3,
	'!='	=> 3,
	'<'	=> 5,
	'<='	=> 5,
	'>'	=> 5,
	'>='	=> 5,
	'&&'	=> 2,
	'||'	=> 1,
	'!'	=> 10,
};

use constant	LPAREN		=> 10;
use constant	RPAREN		=> 11;
use constant	OPERATOR	=> 12;

# Operator's priority
use constant	PRI_SENTINEL	=> 1;		# for sentinel
use constant	PRI_PAREN	=> 2;		# for parenthesis

use constant	OPERATOR_CLASS	=> {
	'&&'	=> 'UtsTune::QueryParser::AndOperator',
	'||'	=> 'UtsTune::QueryParser::OrOperator',
	'!'	=> 'UtsTune::QueryParser::NotOperator',
};

# Base class for parsed token
{
	package UtsTune::QueryParser::Token;

	use overload	'""' => 'token', 'bool' => 'boolean';

	sub new
	{
		my $this = shift;
		my $class = ref($this) || $this;
		my ($token) = @_;

		my $me = {TOKEN => $token};
		return bless $me, $class;
	}

	sub token
	{
		my $me = shift;
		return $me->{TOKEN};
	}

	sub boolean
	{
		return $_[0];
	}

	sub isOperator
	{
		return undef;
	}
}

# Base class for value
{
	package UtsTune::QueryParser::Value;

	use vars qw(@ISA);
	@ISA = qw(UtsTune::QueryParser::Token);

	sub new
	{
		my $this = shift;
		my $class = ref($this) || $this;

		my $me = $class->SUPER::new(@_);
		return bless $me, $class;
	}

	sub value
	{
		my $me = shift;
		return $me->token();
	}

	sub evalBoolean
	{
		my $me = shift;
		my ($parser) = @_;

		my $value = $me->value();
		my $script = $parser->getScript();
		my $res = $script->evaluateOne($value);

		return UtsTune::QueryParser::BoolValue->new($res);
	}

	sub isBoolean
	{
		return undef;
	}

	sub isParameter
	{
		return undef;
	}
}

# Parameter value
{
	package UtsTune::QueryParser::Parameter;

	use vars qw(@ISA);
	@ISA = qw(UtsTune::QueryParser::Value);

	sub new
	{
		my $this = shift;
		my $class = ref($this) || $this;
		my ($token, $opt) = @_;

		my $me = $class->SUPER::new($token);
		$me->{OPTION} = $opt;

		return bless $me, $class;
	}

	sub value
	{
		my $me = shift;
		return $me->{OPTION};
	}

	sub isParameter
	{
		return 1;
	}
}

# Boolean value
{
	package UtsTune::QueryParser::BoolValue;

	use vars qw(@ISA);
	@ISA = qw(UtsTune::QueryParser::Value);

	sub new
	{
		my $this = shift;
		my $class = ref($this) || $this;
		my ($value) = @_;

		my $token = ($value) ? 'true' : 'false';
		my $me = $class->SUPER::new($token);
		$me->{VALUE} = $value;

		return bless $me, $class;
	}

	sub value
	{
		my $me = shift;
		return $me->{VALUE};
	}

	sub evalBoolean
	{
		my $me = shift;

		return $me;
	}

	sub isBoolean
	{
		return 1;
	}
}

# Base class for operator
{
	package UtsTune::QueryParser::BaseOperator;

	use vars qw(@ISA);
	@ISA = qw(UtsTune::QueryParser::Token);

	sub new
	{
		my $this = shift;
		my $class = ref($this) || $this;
		my ($op, $pri) = @_;

		my $me = $class->SUPER::new($op);
		bless $me, $class;
		$me->{PRI} = $pri;

		return $me;
	}

	sub isOperator
	{
		return 1;
	}

	sub operator
	{
		my $me = shift;
		return $me->token();
	}

	sub priority
	{
		my $me = shift;
		return $me->{PRI};
	}
}

# Class for parenthesis
{
	package UtsTune::QueryParser::Parenthesis;

	use vars qw(@ISA);
	@ISA = qw(UtsTune::QueryParser::BaseOperator);

	sub new
	{
		my $this = shift;
		my $class = ref($this) || $this;
		my ($op) = @_;

		my $pri = $UtsTune::QueryParser::PRI_PAREN;
		my $me = $class->SUPER::new($op, $pri);
		bless $me, $class;

		return $me;
	}
}

# Operator class
{
	package UtsTune::QueryParser::Operator;

	use vars qw(@ISA);
	@ISA = qw(UtsTune::QueryParser::BaseOperator);

	sub new
	{
		my $this = shift;
		my $class = ref($this) || $this;
		my ($op, $pri) = @_;

		$pri += $UtsTune::QueryParser::PRI_PAREN + 1;
		my $me = $class->SUPER::new($op, $pri);
		bless $me, $class;

		return $me;
	}

	sub isSingle
	{
		return undef;
	}

	sub evaluate
	{
		my $me = shift;
		my ($parser, $arg1, $arg2) = @_;

		my $script = $parser->getScript();
		my $v1 = $arg1->value();
		my $v2 = $arg2->value();
		my $op = $me->operator();
		my $res = $script->evaluate($v1, $op, $v2);

		return UtsTune::QueryParser::BoolValue->new($res);
	}
}

# Class for "NOT" operator
{
	package UtsTune::QueryParser::NotOperator;

	use vars qw(@ISA);

	@ISA = qw(UtsTune::QueryParser::Operator);

	sub new
	{
		my $this = shift;
		my $class = ref($this) || $this;

		my $me = $class->SUPER::new(@_);

		return bless $me, $class;
	}

	sub evaluate
	{
		my $me = shift;
		my ($parser, $arg) = @_;

		$arg = $arg->evalBoolean($parser) unless ($arg->isBoolean());
		my $res = !$arg->value();

		return UtsTune::QueryParser::BoolValue->new($res);
	}

	sub isSingle
	{
		return 1;
	}
}

# Class for "&&" operator
{
	package UtsTune::QueryParser::AndOperator;

	use vars qw(@ISA);

	@ISA = qw(UtsTune::QueryParser::Operator);

	sub new
	{
		my $this = shift;
		my $class = ref($this) || $this;

		my $me = $class->SUPER::new(@_);

		return bless $me, $class;
	}

	sub evaluate
	{
		my $me = shift;
		my ($parser, $arg1, $arg2) = @_;

		$arg1 = $arg1->evalBoolean($parser)
			unless ($arg1->isBoolean());
		$arg2 = $arg2->evalBoolean($parser)
			unless ($arg2->isBoolean());
		my $res = ($arg1->value() and $arg2->value());

		return UtsTune::QueryParser::BoolValue->new($res);
	}
}

# Class for "||" operator
{
	package UtsTune::QueryParser::OrOperator;

	use vars qw(@ISA);

	@ISA = qw(UtsTune::QueryParser::Operator);

	sub new
	{
		my $this = shift;
		my $class = ref($this) || $this;

		my $me = $class->SUPER::new(@_);

		return bless $me, $class;
	}

	sub evaluate
	{
		my $me = shift;
		my ($parser, $arg1, $arg2) = @_;

		$arg1 = $arg1->evalBoolean($parser)
			unless ($arg1->isBoolean());
		$arg2 = $arg2->evalBoolean($parser)
			unless ($arg2->isBoolean());
		my $res = ($arg1->value() or $arg2->value());

		return UtsTune::QueryParser::BoolValue->new($res);
	}
}

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my (%args) = @_;

	my $db = $args{DB};
	my $plat = $args{PLATFORM};
	my $objdir = $args{OBJDIR};
	my $query = $args{QUERY};
	my $debug = $args{DEBUG};

	my $loader = UtsTune::OptionLoader->new($db, $plat, $objdir);
	my $script = UtsTune::Script->new($db, $objdir, $loader);
	$script->setNoEval();

	return bless {LOADER => $loader, PLATFORM => $plat, OBJDIR => $objdir,
		      SCRIPT => $script, DEBUG => $debug}, $class;
}

sub getScript
{
	my $me = shift;

	return $me->{SCRIPT};
}

sub evaluate
{
	my $me = shift;
	my ($query) = @_;

	my (@char) = split('', $query);
	die "Query is empty.\n" unless (@char);

	$me->{QUERY} = \@char;
	$me->{UNPARSED} = [];

	# At first, convert query into RPN.
	my (@stack, @rpn);

	# Add sentinel to the bottom of stack.
	my $sentinel = UtsTune::QueryParser::BaseOperator->
		new(undef, PRI_SENTINEL);
	push(@stack, $sentinel);

	while (1) {
		my ($token, $type) = $me->nextToken();
		last unless (defined($token));

		if ($type == LPAREN) {
			# Push left parenthesis.
			my $op = UtsTune::QueryParser::Parenthesis->
				new($token);
			push(@stack, $op);
			next;
		}
		if ($type == RPAREN) {
			# Pop stacked tokens.
			while (@stack) {
				my $op = $stack[$#stack];
				my $tk = $op->operator();
				$me->parseError("Mismatched parenthesis: ')'")
					unless ($tk);
				last if ($tk eq '(');
				pop(@stack);
				push(@rpn, $op);
			}
			pop(@stack);
			next;
		}

		if ($type == OPERATOR) {
			# Found operator.
			my $pri = OPERATOR_PRIORITY->{$token} || 0;
			my $class = OPERATOR_CLASS->{$token} ||
				'UtsTune::QueryParser::Operator';
			my $op = $class->new($token, $pri);
			$pri = $op->priority();
			while (@stack) {
				# Pop operators that has higher priority.
				my $s = $stack[$#stack];
				my $spri = $s->priority();
				if ($spri < $pri or
				    ($op->isSingle() and $spri == $pri)) {
					last;
				}
				pop(@stack);
				push(@rpn, $s);
			}
			push(@stack, $op);
		}
		elsif ($type == $me->SYMBOL) {
			# This must be a parameter.
			my $loader = $me->{LOADER};
			my $opt = $loader->loadOption($token) or
				die "Unknown parameter: $token\n";
			my $v = UtsTune::QueryParser::Parameter->
				new($token, $opt);
			push(@rpn, $v);
		}
		else {
			my $v = UtsTune::QueryParser::Value->new($token);
			push(@rpn, $v);
		}
	}

	while (my $s = pop(@stack)) {
		my $op = $s->operator();
		last unless (defined($op));
		$me->parseError("Mismatched parenthesis: '('") if ($op eq '(');
		push(@rpn, $s);
	}

	$me->{DEBUG}->debug('RPN: ', join(' ', @rpn)) if ($me->{DEBUG});

	# Evaluate RPN.
	my (@exstack);
	while (@rpn) {
		my $op = shift(@rpn);
		unless ($op->isOperator()) {
			push(@exstack, $op);
			next;
		}

		# Evaluate operator.
		my $arg1 = pop(@exstack);
		$me->parseError("Syntax error near \"$op\"") unless ($arg1);

		my $res;
		if ($op->isSingle()) {
			# This operator takes one argument.
			$res = $op->evaluate($me, $arg1);
			$me->{DEBUG}->debug("EVAL: $op $arg1 => $res")
				if ($me->{DEBUG});
		}
		else {
			# This operator takes two arguments.
			my $arg2 = pop(@exstack);
			$me->parseError("Syntax error near \"$op\"")
				unless ($arg2);
			$res = $op->evaluate($me, $arg2, $arg1);
			$me->{DEBUG}->debug("EVAL: $arg2 $op $arg1 => $res")
				if ($me->{DEBUG});
		}

		# Push result into execution stack.
		push(@exstack, $res);
	}

	my $res = pop(@exstack);
	$me->parseError('Syntax error')
		unless (defined($res) and @exstack == 0);
	if ($res->isParameter()) {
		my $r = $res->evalBoolean($me);
		$me->{DEBUG}->debug("EVAL: $res  => $r")
			if ($me->{DEBUG});
		$res = $r;
	}
	elsif (!$res->isBoolean()) {
		$me->parseError('No parameter is specified.');
	}

	return ($res->value()) ? 0 : 1;
}

sub getChar
{
	my $me = shift;

	return shift(@{$me->{QUERY}});
}

sub ungetChar
{
	my $me = shift;
	my ($c) = @_;

	unshift(@{$me->{QUERY}}, $c);
}

sub isTokenChar
{
	my $me = shift;
	my ($c) = @_;

	return TOKEN_CHAR->{$c};
}

sub nextToken
{
	my $me = shift;

	my $unparsed = pop(@{$me->{UNPARSED}});
	return @$unparsed if (defined($unparsed));

	my $c;
	while (defined($c = $me->getChar())) {
		next if ($c eq "\n" or $c eq "\r");
		next if ($c =~ /^\s$/o);
		last;
	}

	return undef unless (defined($c));

	my ($token, $type);
	if ($c eq '(') {
		$token = $c;
		$type = LPAREN;
	}
	elsif ($c eq ')') {
		$token = $c;
		$type = RPAREN;
	}
	elsif ($c eq '-' or $c =~ /^\d$/o) {
		($token, $type) = $me->parseInt($c);
	}
	elsif ($c eq '"') {
		$token = $me->parseString($c);
		$type = $me->STRING;
	}
	elsif ($c eq '<' or $c eq '>' or $c eq '!') {
		my $nc = $me->getChar();
		$token = $c;
		if ($nc and $nc eq '=') {
			$token .= $nc;
		}
		else {
			$me->ungetChar($nc);
		}
		$type = OPERATOR;
	}
	elsif ($c eq '=' or $c eq '&' or $c eq '|') {
		my $nc = $me->getChar();
		if ($nc and $nc eq $c) {
			$token = $c . $nc;
			$type = OPERATOR;
		}
		else {
			$me->parseError("Syntax error near \"$c\".");
		}
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

sub ungetToken
{
	my $me = shift;
	my ($token, $type) = @_;

	push(@{$me->{UNPARSED}}, [$token, $type]);
}

sub parseError
{
	my $me = shift;
	my $err = join("", @_);

	die "$err\n";
}

1;
