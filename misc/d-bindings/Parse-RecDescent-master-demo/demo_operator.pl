#! /usr/local/bin/perl -sw

# PARSE LOGICAL EXPRESSIONS

use Parse::RecDescent;

$grammar =
q{
	expr	:	'[' <leftop: atom /,?/ atom>(?) ']' 	{ $item[2] }
		|       '[[' atomlist ']]' 	{ $item[2] }
		|	disj  no_garbage 	{ $item[1] }
		|	<rightop: atom '=' num>
		|	<leftop: atom '<<' num>

	atomlist: <leftop: atom /,?/ atom>

	no_garbage: /^\s*$/
		  | <error: Trailing garbage: $text>

	disj	:	<leftop: conj /(or)/ conj >

	conj	:	<leftop: unary 'and' unary >

	and	: 	'and'

	unary	:	'not' atom
		|	'(' expr ')'	{ $item[2] }
		|	atom

	atom	:	/(?!and|or)[a-z]/

	num	: 	/\d+/

};

$parse = new Parse::RecDescent ($grammar) or die "bad grammar";

$input = '';

use Data::Dumper;

print "> ";
while (<>)
{

	if (/^\.$/) { print STDERR Data::Dumper->Dump([$parse->expr($input)]);
		      $input = '' }
	else	    { $input .= $_ }
	print "> ";
}
