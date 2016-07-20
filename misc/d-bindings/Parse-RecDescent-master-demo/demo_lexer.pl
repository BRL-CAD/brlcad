#! /usr/local/bin/perl -sw

# IT'S A PARSER *AND* A LEXER...

use Parse::RecDescent;
use Data::Dumper;

$lexer = new Parse::RecDescent q
{
	lex: token(s)

	token: 'I\b'		<token:POSS>
	     | 'see\b'		<token:VERB>
	     | 'on\b'		<token:PREP>
	     | 'by\b'		<token:PREP>
	     | /the\b|a\b/i	<token:ARTICLE>
	     | /\w+/		<token:WORD,PUNCT,OTHER>
};

$data = join '', <DATA>;

print_tokens($lexer->lex(\$data));

print "left: [$data]\n";

sub print_tokens
{
	foreach $token ( @{$_[0]} )
	{
		print Dumper($token), "\n";
	}
}

__DATA__

I see a cat on the windowsill by the door!!!!!
