#! /usr/local/bin/perl -ws

# THE COMMONEST REASON FOR WANTING LEFT RECURSION

use strict;
use Parse::RecDescent; $::RD_HINT = 1;

sub Parse::RecDescent::evalop {
	$_[0][0] = $_[0][$_+1](@{$_[0]}[0,$_+2])
		for map 2*($_-1), 1..@{$_[0]}/2;
	return $_[0][0];
}

my $parse = Parse::RecDescent->new(<<'EndGrammar');

	main: expr /\Z/ { $item[1] }
	    | <error>

	expr: <leftop:term add_op term>
			{ evalop($item[1]) }

	add_op: '+'	{ sub { $_[0] += $_[1] } }
	      | '-'	{ sub { $_[0] -= $_[1] } }

	term: <leftop:factor mult_op factor>
			{ evalop($item[1]) }

	mult_op: '*'	{ sub { $_[0] *= $_[1] } }
	       | '/'	{ sub { $_[0] /= $_[1] } }

	factor: number
	      | '(' expr ')' { $item[2] }

	number: /[-+]?\d+(\.\d+)?/

EndGrammar

while (<DATA>) {
  print "$_ = ", $parse->main($_), "\n";
}

while (print "> " and defined($_=<>)) {
  print "= ", $parse->main($_), "\n";
}

__DATA__
2+3
2*3
+1-1+1-1+1-1+1-1+1
7*7-6*8
121/(121/11)/121*11
1/(10-1/(1/(10-1)))
