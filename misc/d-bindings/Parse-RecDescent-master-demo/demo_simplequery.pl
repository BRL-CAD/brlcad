#! /usr/local/bin/perl -ws

use Parse::RecDescent;
use Data::Dumper;

use Parse::RecDescent;

my $grammar = <<'EOGRAMMAR';

	{ my  %patterns; }

	query:	  { %patterns=() }
		  pattern(s) end_of_query
		  { $return = { %patterns } }
	     |    <error>

	end_of_query: /\Z/

	pattern: '+' string  { push @{$patterns{required}}, $item[2] }
	       | '-' string  { push @{$patterns{excluded}}, $item[2] }
	       | string      { push @{$patterns{optional}}, $item[1] }
	       | <error>

	string: quoted_string
	      | bareword

	quoted_string:
		 { my $string = extract_delimited($text,q{'"});
		   $return = substr($string,1,length($string)-2) if $string; }

	bareword: /\w+/

EOGRAMMAR

my $parser = Parse::RecDescent->new($grammar) or die;

while (<>)
{
	my $query = $parser->query($_);
	print Data::Dumper->Dump([$query],['query']);
}

