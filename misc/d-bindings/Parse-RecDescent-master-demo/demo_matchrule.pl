#! /usr/local/bin/perl -sw

use vars qw($animal);
use Parse::RecDescent;

$grammar = q {
		    object:	 thing[article=>'a'](s)
			  |	 thing[article=>'the'](s)

		    thing:	 <matchrule:$arg{article}>
			  	 <matchrule:${\scalar reverse $::animal}>

		    a:		'a'
		    the:	'the'

		    cat:	'cat' { print "meow\n"; $::animal = 'god' }
		    dog:	'dog' { print "woof\n" }
	     };

unless( $parser = new Parse::RecDescent( $grammar ))
{
    die "bad grammar; bailing\n";
}

$/ = "";
while (defined ($input = <DATA>))
{
	$::animal = reverse 'cat';

	print STDERR "parsing...\n";
	unless( defined $parser->object( $input ))
	{
	    die "error in input; bailing\n";
	}
	print STDERR "...parsed\n";
}

__DATA__
a cat
a cat
a cat
a dog

the cat
the dog
the dog
the dog
the dog
a dog
