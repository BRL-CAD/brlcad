#! /usr/local/bin/perl -sw

# PARSE LOGICAL EXPRESSIONS

use Parse::RecDescent;

$RD_TRACE=1;

$grammar =
q{
	expr	:	disj  no_garbage

	no_garbage: /^\s*$/
		  | <error: Trailing garbage>

	disj	:	conj  ('or' conj)(s?)

	conj	:	unary ('and' unary)(s?)

	unary	:	'not' atom
		|	'(' disj ( ')' | <error> )
		|	atom

	atom	:	/<.+?>/

};

$parse = new Parse::RecDescent ($grammar);

$input = '';

print "> ";
while (<>)
{

	if (/^\.$/) { defined $parse->expr($input) or print "huh?\n"; $input = '' }
	else	    { chomp; $input .= " $_" }
	print "> ";
}
