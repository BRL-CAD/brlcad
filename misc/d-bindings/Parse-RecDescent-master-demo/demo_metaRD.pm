# Changes! /usr/local/bin/perl -w

use Parse::RecDescent;

local $/;
my $parse = Parse::RecDescent->new(<DATA>);

my $grammar = <>;

$tree = parse->grammar($grammar) or die "Bad grammar! No biscuit!";

print Data::Dumper->Dump([$tree]);



__DATA__

<autotree>

grammar    : prerule(s?) components(s?) /\Z/

component  : rule
	   | comment

rule       : <skip:""> "\n" <skip: '[ \t]'> identifier ":"
	     <skip: $item[1]> production(s? /|/)

production : items(s)

item       : lookahead(s?) simpleitem
           | directive
           | comment

lookahead  : '...' | '...!'                   # +'ve or -'ve lookahead

simpleitem : subrule args(?) rep(?)           # match another rule
           | terminal                         # match the next input
           | bracket args(?)                  # match alternative items
           | action                           # do something

subrule    : identifier                       # the name of the rule

args       : {extract_codeblock($_[0],'[]')}  # just like a [...] array ref

rep	   : '(' repspec ')'

repspec	   : '?'                              # 0 or 1 times
           | 's?'                             # 0 or more times
           | 's'                              # 1 or more times
           | /(\d+)[.][.](/\d+)/              # $1 to $2 times
           | /[.][.](/\d*)/                   # at most $1 times
           | /(\d*)[.][.])/                   # at least $1 times

terminal   : /[/]([\][/]|[^/])*[/]/           # interpolated pattern
           | /"([\]"|[^"])*"/                 # interpolated literal
           | /'([\]'|[^'])*'/                 # uninterpolated literal

action     : <perl_codeblock>		      # embedded Perl code

bracket    : '(' production(s? /|/) ')'       # alternative subrules

directive  : '<commit>'                       # commit to production
           | '<uncommit>'                     # cancel commitment
           | '<resync>'                       # skip to newline
           | '<resync:' pattern '>'           # skip <pattern>
           | '<reject>'                       # fail this production
           | '<reject:' condition '>'         # fail if <condition>
           | '<error>'                        # report an error
           | '<error:' string '>'             # report error as "<string>"
           | '<error?>'                       # error only if committed
           | '<error?:' string '>'            #   "    "    "    "
           | '<rulevar:' /[^>]+/ '>'          # define rule-local variable
           | '<matchrule:' string '>'         # invoke rule named in string

identifier : /[a-z]\w*/i                      # must start with alpha

comment    : /#[^\n]*/                        # same as Perl

pattern    : {extract_bracketed($text,'<')}   # allow embedded "<..>"

condition  : {extract_codeblock($text,'{<')}  # full Perl expression

string     : {extract_variable($text)}        # any Perl variable
           | {extract_quotelike($text)}       #   or quotelike string
           | {extract_bracketed($text,'<')}   #   or balanced brackets
