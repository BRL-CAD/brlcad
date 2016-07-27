#! /usr/local/bin/perl -w

use Parse::RecDescent;
use Data::Dumper;

my $lexer = new Parse::RecDescent q
{
	lex:	token(s)

  	token:	/(I|you|he)\b/i	<token:PRON>
 	     |	/(is|are)\b/	<token:VERB>
	     |	'dumbest'	<token:ADJ>
	     |	'Bill-loving'	<token:ADJ>
	     |	'clearly'	<token:ADJ>
	     |	/the\b|a\b/	<token:ARTICLE>
	     |	/\w+/		<token:WORD>
	     |  /\S+/		<token:PUNCT,OTHER>
};

my $tokens = $lexer->lex(join "", <DATA>);

print  Data::Dumper->Dump($tokens);

__DATA__

You are clearly the dumbest, Bill-loving script-kiddie I have ever seen!
