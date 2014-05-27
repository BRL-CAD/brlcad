#! /usr/local/bin/perl -sw

use Parse::RecDescent;

$grammar = q {
		list: 	  <leftop: <matchrule:$arg{rule}> /$arg{sep}/ <matchrule:$arg{rule}> >

		function: 'func' ident '(' list[rule=>'param',sep=>';'] ')'
				{{ name=>$item{ident}, param=>$item{list} }}

		param:	  list[rule=>'ident',sep=>','] ':' typename
				{{ vars=>$item{list}, type=>$item{typename} }}

		ident:	  /\w+/

		typename: /\w+/
	     };

unless( $parser = new Parse::RecDescent( $grammar ))
{
    die "bad grammar; bailing\n";
}

while (defined($input = <DATA>))
{
	use Data::Dumper;
	print Data::Dumper->Dump([$parser->function( $input )]);
}

__DATA__
func f (a,b,c:int; d:float; e,f:string)
func g (x:int)
func h (y;x)
