#! /usr/local/bin/perl -w

use Parse::RecDescent;

my $parse = Parse::RecDescent->new(<<'EndGrammar');

	perl: <perl_quotelike>
		{ print 'quotelike: [', join("|", @{$item[1]}), "]\n" }
	    | <skip: '-*'> <perl_variable>
		{ print "variable: $item[-1]\n" }
	    | <perl_codeblock>
		{ print "codeblock: $item[1]\n" }
	    | /.*/
		{ print "unknown: $item[1]\n" }

EndGrammar

print "> ";
while (<DATA>) {	# FOR DEMO CHANGE TO: while (<DATA>)
  $parse->perl($_);
}

__DATA__
{$a=1};
$a;
{ $a = $b; \n $a =~ /$b/; \n @a = map /\s/ @b };
$_;
$a[1];
$_[1];
$a{cat};
$_{cat};
$a->[1];
$a->{"cat"}[1];
@$listref;
@{$listref};
$obj->nextval;
@{$obj->nextval};
@{$obj->nextval($cat,$dog)->{new}};
@{$obj->nextval($cat?$dog:$fish)->{new}};
@{$obj->nextval(cat()?$dog:$fish)->{new}};
$ a {'cat'};
$a::b::c{d}->{$e->()};
$#_;
$#array;
$#{array};
$var[$#var];
'a';
"b";
`c`;
'a\'';
'a\\';
'\\a';
"a\\";
"\\a";
"b\'\"\'";
`c '\`abc\`'`;
q{a};
qq{a};
qx{a};
s{a}/b/;
tr!a!b!;
