#! /usr/local/bin/perl -ws

use Parse::RecDescent;

my $parser = new Parse::RecDescent q
{
	components: ganglia(s)

	ganglia:    'ganglia' '{' cell(s) '}'

	cell:	    'cell' '{' synapse(s) '}'

	synapse:    'synapse' '{' WHATEVER '}'

	WHATEVER:    /\d\.\d\.\d/   # OBVIOUSLY REPLACE THIS WITH WHATEVER :-)
			<defer: print "$item[1]\n" >

} or die "Bad grammar!\n";

my $text = '';

getnextchunk($text)				# GET FIRST CHUNK OF DATA
	or die "No data!\n";

$text =~ s/\s*define\s*NetworkDetails\s*{//	# EAT OUTERMOST OPENING BRACKET
	or die "Bad data! (expected 'define...')";

do
{
	$parser->components(\$text);		# TRY TO EAT GANGLIA
}
while (getnextchunk($text));			# APPEND THE NEXT CHUNK

$text =~ m/\s*}/		# CHECK FOR THE OUTERMOST CLOSING BRACKET
	or die "Unclosed 'define' (missing '}')!";


sub getnextchunk	# APPEND SOME MORE TEXT TO THE FIRST ARG
{
	my $wasmore = 0;
	for (1..10)		# OPTIMALLY, ONE LINE BIGGER THAN THE LARGEST
				# SINGLE GANGLIA DEFINITION, BUT ANY NUMBER
				# OF LINES WILL DO
	{
		my $nextline = <DATA>;
		defined $nextline or last;
		$_[0] .= $nextline;
		$wasmore++
	}
	return $wasmore;
}

__DATA__

define NetworkDetails
{
	ganglia {
		cell {
			synapse { 1.1.1 }
			synapse { 1.1.2 }
			synapse { 1.1.3 }
			synapse { 1.1.4 }
			synapse { 1.1.5 }
		}
		cell {
			synapse { 1.2.1 }
			synapse { 1.2.2 }
			synapse { 1.2.3 }
			synapse { 1.2.4 }
			synapse { 1.2.5 }
		}
		cell {
			synapse { 1.3.1 }
			synapse { 1.3.2 }
			synapse { 1.3.3 }
			synapse { 1.3.4 }
			synapse { 1.3.5 }
		}
		cell {
			synapse { 1.4.1 }
			synapse { 1.4.2 }
			synapse { 1.4.3 }
			synapse { 1.4.4 }
			synapse { 1.4.5 }
		}
	}

	ganglia {
		cell {
			synapse { 2.1.1 }
			synapse { 2.1.2 }
			synapse { 2.1.3 }
			synapse { 2.1.4 }
			synapse { 2.1.5 }
		}
		cell {
			synapse { 2.2.1 }
			synapse { 2.2.2 }
			synapse { 2.2.3 }
			synapse { 2.2.4 }
			synapse { 2.2.5 }
		}
		cell {
			synapse { 2.3.1 }
			synapse { 2.3.2 }
			synapse { 2.3.3 }
			synapse { 2.3.4 }
			synapse { 2.3.5 }
		}
		cell {
			synapse { 2.4.1 }
			synapse { 2.4.2 }
			synapse { 2.4.3 }
			synapse { 2.4.4 }
			synapse { 2.4.5 }
		}
	}

	ganglia {
		cell {
			synapse { 3.1.1 }
			synapse { 3.1.2 }
			synapse { 3.1.3 }
			synapse { 3.1.4 }
			synapse { 3.1.5 }
		}
		cell {
			synapse { 3.2.1 }
			synapse { 3.2.2 }
			synapse { 3.2.3 }
			synapse { 3.2.4 }
			synapse { 3.2.5 }
		}
		cell {
			synapse { 3.3.1 }
			synapse { 3.3.2 }
			synapse { 3.3.3 }
			synapse { 3.3.4 }
			synapse { 3.3.5 }
		}
		cell {
			synapse { 3.4.1 }
			synapse { 3.4.2 }
			synapse { 3.4.3 }
			synapse { 3.4.4 }
			synapse { 3.4.5 }
		}
	}

	ganglia {
		cell {
			synapse { 4.1.1 }
			synapse { 4.1.2 }
			synapse { 4.1.3 }
			synapse { 4.1.4 }
			synapse { 4.1.5 }
		}
		cell {
			synapse { 4.2.1 }
			synapse { 4.2.2 }
			synapse { 4.2.3 }
			synapse { 4.2.4 }
			synapse { 4.2.5 }
		}
		cell {
			synapse { 4.3.1 }
			synapse { 4.3.2 }
			synapse { 4.3.3 }
			synapse { 4.3.4 }
			synapse { 4.3.5 }
		}
		cell {
			synapse { 4.4.1 }
			synapse { 4.4.2 }
			synapse { 4.4.3 }
			synapse { 4.4.4 }
			synapse { 4.4.5 }
		}
	}

}
