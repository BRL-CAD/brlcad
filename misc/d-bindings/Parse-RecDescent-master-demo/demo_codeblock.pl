#! /usr/bin/perl -w

use Parse::RecDescent;

$RD_TRACE=1;
my $parse = Parse::RecDescent->new(do{local$/;<DATA>});


while (<>) {
	use Data::Dumper 'Dumper';
	print Dumper [
		$parse->line($_)
	];
}

__DATA__

line: block | call*

block: <perl_codeblock>

call: "foo" <perl_codeblock ()>
