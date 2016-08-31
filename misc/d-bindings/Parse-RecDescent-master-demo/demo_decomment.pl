#!/usr/local/bin/perl -ws

# $::RD_TRACE = 1;

# REMOVE COMMENTS FROM C++ CODE

# ORIGINAL BY Helmut Jarausch
# EXTENDED BY Damian Conway AND Helmut Jarausch AND Jeremy Braun

use strict;
use Parse::RecDescent;

use vars qw/ $Grammar /;

my $parser = new Parse::RecDescent $Grammar  or  die "invalid grammar";

undef $/;
my $text = @ARGV ? <> : <DATA>;

my $parts = $parser->program($text) or die "malformed C program";

print "Comments\n========\n$parts->{comments}\n";
print "\nCode\n====\n$parts->{code}\n";
print "\nStrings\n=======\n", map(qq{\t"$_"\n}, @{$parts->{strings}});

BEGIN
{ $Grammar=<<'EOF';

program : <rulevar: local $WithinComment=0>
program : <rulevar: local $Comments = ""> /this shouldn't be here :-/
program : <reject>
program : <reject> /with prejudice/
program : <rulevar: local $Code = "">
program : <rulevar: local @Strings>

program	: <skip:''> part(s)
		{ { code=>$Code, comments=>$Comments, strings=>[@Strings]} }

part	: comment
        | C_code
        | string
        | charlit

C_code  : m{(
	      [^"/]+		# one or more non-delimiters
	      (			# then (optionally)...
	       /		# a potential comment delimiter
	       [^*/]		# which is not an actual delimiter
	      )?		#
	    )+			# all repeated once or more
	   }x
		{ $Code .= $item[1] }

string	: m{"			# a leading delimiter
	    ((			# zero or more...
	      \\.		# escaped anything
	      |			# or
	      [^"]		# anything but a delimiter
	     )*
	    )
	    "}x
		{ $Code .= $item[1]; push @Strings, $1 }

charlit	: m{'			# a leading delimiter
	    ((			# zero or more...
	      \\.		# escaped anything
	      |			# or
	      [^']		# anything but a delimiter
	     )*
	    )
	    '}x
		{ $Code .= $item[1]; push @Strings, $1 }


comment	: m{\s*			# optional whitespace
	    //			# comment delimiter
	    [^\n]*		# anything except a newline
	    \n			# then a newline
	   }x
		{ $Code .= "\n"; $Comments .= $item[1] }

	| m{\s*			# optional whitespace
	    /\*			# comment opener
	    (?:[^*]+|\*(?!/))*	# anything except */
	    \*/		        # comment closer
            ([ \t]*)?           # trailing blanks or tabs
	   }x
		{ $Code .= " "; $Comments .= $item[1] }

EOF
}
__DATA__
program test; // for decomment

// using Parse::RecDescent

int main()
{
/* this should
   be removed
*/
  char *cp1 = "";
  char *cp2 = "cp2";
  char c3   = 'c';
  int i;  // a counter
          // remove this line altogehter
  int k;
      int more_indented;  // keep indentation
      int l;  /* a loop
             variable */
      // should be completely removed

  char *str = "/* ceci n'est pas un commentaire */";
  return 0;
}





