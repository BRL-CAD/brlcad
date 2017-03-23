#! /usr/local/bin/perl -sw

# DAMMIT, JIM, I'M A DOCTOR, NOT A PERL PROGRAM!

use Parse::RecDescent;

$grammar =
q{
	McCoy	:	curse ',' name ", I'm a doctor, not a" profession '!'
	     	|	possessive 'dead,' name '!'
	     	|	<error>

	curse	:	'Dammit' | 'Goddammit'

	name	:	'Jim' | 'Spock' | 'Scotty'


	profession:	'magician' | 'miracle worker' | 'Perl hacker'

	possessive:	"He's" | "She's" | "It's" | "They're"
};

$parse = new Parse::RecDescent ($grammar);

print "> ";
while (<>)
{
	$parse->McCoy($_);
	print "> ";
}
