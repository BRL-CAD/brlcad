#! /usr/local/bin/perl -ws
$|++;

use Parse::RecDescent;
# $::RD_TRACE = 1;

my $start = "START";		# start symbol

(my $parser = Parse::RecDescent->new(<<'END_OF_GRAMMAR')) or die "bad!";

## return hashref
## { ident => {
##     is => [
##       [weight => item, item, item, ...],
##       [weight => item, item, item, ...], ...
##     ],
##     defined => { line-number => times }
##     used => { line-number => times }
##   }, ...
## }
## item is " literal" or ident
## ident is C-symbol or number (internal for nested rules)

{ my %grammar; my $internal = 0; }

grammar: rule(s) /\Z/ { \%grammar; }

## rule returns identifier (not used)
rule: identifier ":" defn ';' {
           push @{$grammar{$item[1]}{is}}, @{$item[3]};
           $grammar{$item[1]}{defined}{$itempos[1]{line}{to}}++;
           $item[1];
           }
    | <error>

## defn returns listref of choices
defn: <leftop: choice "|" choice>

## choice returns a listref of [weight => @items]
choice: unweightedchoice { [ 1 => @{$item[1]} ] }
    | /\d+(\.\d+)?/ /\@/ unweightedchoice { [ $item[1] => @{$item[3]} ] }

## unweightedchoice returns a listref of @items
unweightedchoice: item(s)

item: quoted_string
    | identifier ...!/:/ {
        $grammar{$item[1]}{used}{$itempos[1]{line}{to}}++;
        $item[1]; # non-leading space flags an identifier
    }
    | "(" defn ")" { # parens for recursion, gensym an internal
        ++$internal;
        push @{$grammar{$internal}{is}}, @{$item[2]};
        $internal;
    }
    | <error>

quoted_string: /"/ <skip:""> quoted_char(s?) /"/ {
        " " . join "", @{$item[3]} # leading space flags a string
    }

## this should be expanded, but it works for this grammar :)
quoted_char:
      /[^\\"]+/
    | /\\n/ { "\n" }
    | /\\"/ { "\"" }

identifier: /[A-Za-z_]\w*/

END_OF_GRAMMAR

my @data = <DATA>;
for (@data) {
  s/^\s*#.*//;
}

(my $parsed = $parser->grammar(join '', @data)) or die "bad parse";

for my $id (sort keys %$parsed) {
  next if $id =~ /^\d+$/;	# skip internals
  my $id_ref = $parsed->{$id};
  unless (exists $id_ref->{defined}) {
    print "$id used in @{[sort keys %{$id_ref->{used}}]} but not defined - FATAL\n";
  }
  unless (exists $id_ref->{used} or $id eq $start) {
    print "$id defined in @{[sort keys %{$id_ref->{defined}}]} but not used - WARNING\n";
  }
}

use Data::Dumper; print Dumper($parsed);
show($start);

sub show {
  my $defn = shift;
  die "missing defn for $defn" unless exists $parsed->{$defn};

  my @choices = @{$parsed->{$defn}{is}};
  my $weight = 0;
  my @keeper = ();
  while (@choices) {
    my ($thisweight, @thisitem) = @{pop @choices};
    $thisweight = 0 if $thisweight < 0; # no funny stuff
    $weight += $thisweight;
    @keeper = @thisitem if rand($weight) < $thisweight;
  }
  for (@keeper) {
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
START: stanza "\n---\n" stanza "\n---\n" stanza;

stanza: stanza " " exclaim " " stanza2 | stanza2;
stanza2: sentence " " comparison " " question |
         sentence " " comparison |
         comparison " " comparison " " exclaim |
         address " " question " " question " " sentence;

sentence: sentence "\n" sentence2 | sentence2;
sentence2: "The " adjectiveNotHep " " personNotHep " " verbRelating " the "
adjectiveHep " " personHep "." |
"The " personHep " " verbRelating " the " adjectiveNotHep ", " adjectiveNotHep " " personNotHep ".";

question: question " " question2  | question2;
question2: ques_start " " adjectiveHep " " personNotHep "?" |
ques_start " " adjectiveNotHep " " personHep "?";

comparison: comparison " " comparison2 | comparison2;
comparison2: "One says '" compNotHep "' while the other says '" compHep
"'." |
"One thinks '" compNotHep "' while the other thinks '" compHep "'." |
"They shout '" compNotHep "!' And we shout '" compHep "'." |
"It's " compNotHep " versus " compHep "!" ;

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

ques_start: 2 @ (5 @ "Could there ever" | 7 @ "How could there") " be a" |
  "Can you picture a" ;
address:  "Catch this:" | "Listen, cats," | "Dig it:" |
  "I lay this on you:";
exclaim: "Heavy, man."| "Heavy." | "Yow!" | "Snap 'em for me." |
  "Dig it.";


