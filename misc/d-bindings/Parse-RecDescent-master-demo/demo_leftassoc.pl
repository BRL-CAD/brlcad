#! /usr/local/bin/perl -ws

# THE COMMONEST REASON FOR WANTING LEFT RECURSION

use strict;
use Parse::RecDescent; $::RD_HINT = 1;


my $parse = Parse::RecDescent->new(<<'EndGrammar');

	main: expr /\Z/ { $item[1] }
	    | <error>

	expr: left_assoc[qw{term add_op term}]
	    | term

	add_op: '+'	{ sub { $_[0] + $_[1] } }
	      | '-'	{ sub { $_[0] - $_[1] } }

	term: left_assoc[qw{factor mult_op factor}]
	    | factor

	mult_op: '*'	{ sub { $_[0] * $_[1] } }
	       | '/'	{ sub { $_[0] / $_[1] } }

	factor: number
	      | '(' expr ')' { $item[2] }

	number: /[-+]?\d+(\.\d+)?/


# THE BLACK MAGIC THAT MAKES IT WORK...

	left_assoc: left_assoc_left[@arg[0,1]](s) <matchrule:$arg[2]>
			{ my @terms = $item[1]
				? ((map { @$_ } @{$item[1]}), $item[2])
				: $item[1];
			  splice @terms, 0, 3, $terms[1]->(@terms[0,2])
				while @terms>1;
			  $terms[0];
			}

	left_assoc_left: <matchrule:$arg[0]> <matchrule:$arg[1]>
			{ [ @item[1..2] ] }


EndGrammar

while (<DATA>) {
  print $parse->main($_), "\n";
}

__DATA__
+1-1+1-1+1-1+1-1+1
7*7-6*8
121/(121/11)/121*11
1/(10-1/(1/(10-1)))
