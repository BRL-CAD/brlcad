#!/usr/local/bin/perl -ws

# REMOVE COMMENTS FROM C++ CODE

# ORIGINAL BY Helmut Jarausch
# EXTENDED BY Damian Conway AND Helmut Jarausch AND Jeremy Braun

use strict;
use Parse::RecDescent;

use vars qw/ $Grammar /;

my $parser = new Parse::RecDescent $Grammar  or  die "invalid grammar";

undef $/;
my $text = @ARGV ? <> : <DATA>;

$parser->program($text) or die "malformed C program";

print "Comments\n========\n$parser->{comments}\n";
print "\nCode\n====\n$parser->{code}\n";
print "\nStrings\n=======\n", map(qq{\t"$_"\n}, @{$parser->{strings}});

BEGIN
{ $Grammar=<<'EOF';

program	: <skip:''> { @{$thisparser}{qw(comments code strings)} = () }
	  part(s)

part	: comment { $thisparser->{comments} .= $item[1];
        	    $thisparser->{code}     .= " "; }
        | C_code  { $thisparser->{code}     .= $item[1]; }
        | string  { $thisparser->{code}     .= qq("$item[1]");
		    push @{$thisparser->{strings}}, $item[1]; }
        | charlit  { $thisparser->{code}     .= qq('$item[1]');
		    push @{$thisparser->{strings}}, $item[1]; }

C_code  : m{(
	      [^"/]+		# one or more non-delimiters
	      (			# then (optionally)...
	       /		# a potential comment delimiter
	       [^*/]		# which is not an actual delimiter
	      )?		#
	    )+			# all repeated once or more
	   }x

string	: m{"			# a leading delimiter
	    ((			# zero or more...
	      \\.		# escaped anything
	      |			# or
	      [^"]		# anything but a delimiter
	     )*
	    )
	    "}x
		{ $return =  $1 }

charlit	: m{'			# a leading delimiter
	    ((			# zero or more...
	      \\.		# escaped anything
	      |			# or
	      [^']		# anything but a delimiter
	     )*
	    )
	    '}x
		{ $return =  $1 }

comment	: m{\s*			# optional whitespace
	    //			# comment delimiter
	    [^\n]*		# anything except a newline
	    \n			# then a newline
	   }x

	| m{\s*			# optional whitespace
	    /\*			# comment opener
	    (?:[^*]+|\*(?!/))*	# anything except */
	    \*/		        # comment closer
            ([ \t]*)?           # trailing blanks or tabs
	   }x

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





