#! /usr/local/bin/perl -sw

# RPN ARITHMETIC EXPRESSIONS

use Parse::RecDescent;

sub Parse::RecDescent::rpn
{
	my @list = @_;
	for (my $i=1; $i<@list; $i+=2)
	{
		@list[$i,$i+1] = @list[$i+1,$i];
	}
	join " ", @list;
}

# $RD_TRACE=1;

$grammar =
q{
	expr	:	<leftop: conj /(\|\|)/ conj>
				{ rpn(@{$item[1]}) }

	conj	:	<leftop: addn /(&&)/ addn>
				{ rpn(@{$item[1]}) }

	addn	:	<leftop: mult /([+-])/ mult >
				{ rpn(@{$item[1]}) }

	mult	:	<leftop: expo /([*\/\%])/ expo >
				{ rpn(@{$item[1]}) }

	expo	:	<leftop: unary /(\^|\*\*)/ unary >
				{ rpn(@{$item[1]}) }

	unary	:	'(' expr ')'	{ $item[2] }
		|	value

	value	:	/\d+(\.\d+)?/
};

$parse = new Parse::RecDescent ($grammar) or die "bad grammar";

while (<>)
{
	print $parse->expr($_), "\n" if $_;
}
