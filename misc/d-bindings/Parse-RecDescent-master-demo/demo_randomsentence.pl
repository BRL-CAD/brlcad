#!/usr/bin/perl
$|++;

use Parse::RecDescent;

my $parser = Parse::RecDescent->new(<<'END_OF_GRAMMAR');

grammar: rule(s) /\Z/ { [ map { $_->[0], $_->[1] } @{$item[1]} ] }

rule: identifier ":" defn ";" { [ $item[1], $item[3] ] }
	| <error>

defn: choice (barchoice)(s?) { [ $item[1], @{$item[2]} ] }

barchoice: "|" choice

choice: item(s)

item: quoted_string | identifier | <error>

quoted_string: /"(.*?)"/s { " ".substr($item[1],1,-1) }

identifier: /[A-Za-z_]\w*/

END_OF_GRAMMAR

(my $parsed = $parser->grammar(join '', <DATA>)) or die "bad parse";

my $top = $parsed->[0];
my %defns = @$parsed;

for (1..5) {
  show($top);
  print "\n----\n";
}

sub show {
  my $defn = shift;
  die "missing defn for $defn" unless exists $defns{$defn};

  my @choices = @{$defns{$defn}};
  for (@{$choices[rand @choices]}) {
	## should be a list of ids or defns
	die "huh $_ in $defn" if ref $defn;
	if (/^ (.*)/s) {
	  print $1;
	} elsif (/^(\w+)$/) {
	  show($1);
	} else {
	  die "Can't show $_ in $defn\n";
	}
  }
}

__END__
stanza: stanza exclaim stanza2 | stanza2;
stanza2: sentence comparison question | sentence comparison | comparison
comparison exclaim | address question question sentence;

sentence: sentence sentence2 | sentence2;
sentence2: "The " adjectiveNotHep personNotHep verbRelating "the "
adjectiveHep personHep ". " | "The " personHep verbRelating "the "
adjectiveNotHep ", " adjectiveNotHep personNotHep ". ";

question: question question2  | question2;
question2: ques_start adjectiveHep personNotHep "? " | ques_start
adjectiveNotHep personHep "? ";

comparison: comparison comparison2 | comparison2;
comparison2: "One says '" compNotHep "' while the other says '" compHep
"'. "|
"One thinks '" compNotHep "' while the other thinks '" compHep "'. "|
"They shout '" compNotHep "!' And we shout'" compHep "'. "|
"It's " compNotHep " versus " compHep "! ";

personNotHep:  "capitalist" | "silk purse man" | "square" | "banker" |
"Merchant King" | "pinstripe suit" ;

personHep: "cat" | "beat soul" | "wordsmith" | "hep cat" | "free man" |
"street poet" | "skin beater" | "reed man" ;

adjectiveNotHep: "soul-sucking" | "commercial" | "cash-counting" |
"bloody-handed" | "four-cornered" | "uncool" | "love-snuffing";

adjectiveHep: "love-drunk" | "cool, cool" | "happening" | "tuned-in" |
"street wise" | "wise and learned";

verbRelating: "begrudges" | "fears" | "distresses" | "dodges" |
"dislikes" | "evades" | "curses" | "belittles" | "avoids" | "battles";

compNotHep: "recreation" | "isolation" | "tranportation" | "sacred nation"
  | "complication" | "subordination";
compHep: "fornication" | "instigation" | "interpretation" | "elevation"
| "animation" | "inebriation" | "true relation";

ques_start: "Could there ever be a "|"How could there be a "|
  "Can you picture a ";
address:  "Catch this: " | "Listen, cats, " | "Dig it: " |
  "I lay this on you: ";
exclaim: "Heavy, man. "|"Heavy. " | "Yow! " | "Snap 'em for me. " |
  "Dig it. ";
