#! /usr/local/bin/perl -sw

# RECURSIVE #includes DURING A RECURSIVE DESCENT

use Parse::RecDescent;

sub loadfile($)
{
	open FILE, $_[0]  or die "Couldn't find included file: $_[0]\n";
	my $contents = <FILE>;
	close FILE;
	return $contents;
}

%macro = ();

sub demacro($)
{
	my $text = shift;
	while (($macro,$defn) = each %macro )
	{
		$text =~ s/$macro/$defn/;
	}
	return $text;
}

$grammar =
q{
	file     : line(s)

	line	 : include
		 | macrodef
		 | linedir
			{ $thisline = $item[1]; }
		 | codeline
			{ print "found: [$item[1]] at $thisline\n" }

	include	 : '#include' filename
			{
			  print "pre: [$text] at $thisline\n";
			  $text = ::loadfile($item[-1]) . $text;
			  Parse::RecDescent::LineCounter::resync $thisline;
			  print "post: [$text] at $thisline\n";
			}

	filename : '"' m#[a-z0-9_./-]+#i '"'
			{ $return = $item[-2] }
		 | '<' m#[a-z0-9_./-]+#i '>'
			{ $return = $item[-2] }

	macrodef : '#define' /[a-z]\w*/i /.*/
			{ $::macro{$item[-2]} = $item[-1] }

	linedir:   '#line' /\d+/

	codeline : /.*\n/
			{ $return = ::demacro($item[-1]); }

};

$parse = new Parse::RecDescent ($grammar);

undef $/;

$reinput = $input = <>;

$parse->file($input) or die "Bad file! No biscuit!\n";

$parse->file($reinput) or die "Bad file! No biscuit!\n";
