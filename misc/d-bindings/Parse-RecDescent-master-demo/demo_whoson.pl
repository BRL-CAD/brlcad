#! /usr/local/bin/perl -sw

# WHO'S ON FIRST?

use vars qw( @base @man @attempt );   # This is a comment

use Parse::RecDescent;

sub Parse::RecDescent::choose { $_[int rand @_]; }

$abbott = new Parse::RecDescent <<'EOABBOTT';

	Interpretation:
		ConfirmationRequest
	      | NameRequest
	      | BaseRequest

	ConfirmationRequest:
	       	Preface(s?) Name /[i']s on/ Base
			{ (lc $::man{$item[4]} eq lc $item[2])
				? "Yes"
				: "No, $::man{$item[4]}\'s on $item[4]"
			}

	      | Preface(s?) Name /[i']s the (name of the)?/ Man /('s name )?on/ Base
			{ (lc $::man{$item[6]} eq lc $item[2])
				? "Certainly"
				: "No. \u$item[2] is on " . $::base{lc $item[2]}
			}

	BaseRequest:
	        Preface(s?) Name /(is)?/
			{ "He's on " . $::base{lc $item[2]} }

	NameRequest:
	        /(What's the name of )?the/i Base "baseman"
			{ $::man{$item[2]} }

	Preface: ...!Name /\S*/

	Name:	/Who/i  | /What/i  | /I Don't Know/i

	Base:	'first' | 'second' | 'third'

	Man:	'man'   | 'guy'    | 'fellow'
EOABBOTT

$costello = new Parse::RecDescent <<'EOCOSTELLO';

	Interpretation:
		Meaning <reject:$item[1] eq $thisparser->{prev}>
			{ $thisparser->{prev} = $item[1] }
	      | { choose(@::attempt) }

	Meaning:
		Question
	      | UnclearReferent
	      | NonSequitur
	      | { choose(@::attempt) }

	Question:
	        Preface Interrogative /[i']s on/ Base
	      		{ choose ("Yes, what is the name of the guy on $item[4]?",
				  "The $item[4] baseman?",
				  "I'm asking you! $item[2]?",
				  "I don't know!")			}

	      | Interrogative
	      		{ choose ("That's right, $item[1]?",
				  "What?",
				  "I don't know!")			}

	UnclearReferent:
	        "He's on" Base
	      		{ choose ("Who's on $item[2]?",
				  "Who is?",
				  "So, what is the name of the guy on $item[2]?"
				  )					}

	NonSequitur:
		( "Yes" | 'Certainly' | /that's correct/i )
			{ choose("$item[1], who?",
				 "What?",
				 @::attempt)				}

	Interrogative: /who/i | /what/i

	Base:	'first' | 'second' | 'third'

	Preface: ...!Interrogative /\S*/

EOCOSTELLO

%man = ( first => "Who", second => "What", third => "I Don't Know" );
%base  = map { lc } reverse %man;

@attempt =
(
	"So, who's on first?",
	"I want to know who's on first?",
	"What's the name of the first baseman?",
	"Let's start again. What's the name of the guy on first?",
	"Okay, then, who's on second?",
	"Well then, who's on third?",
	"What's the name of the fellow on third?",
);

$costello->{prev} = $line = "Who's on first?";

while (1)
{
	print "<costello>  ", $line, "\n";
	$line = $abbott->Interpretation($line);
	sleep 1;
	print "<abbott>    ", $line, "\n";
	$line = $costello->Interpretation($line);
	sleep 1;
}

