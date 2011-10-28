#!/usr/bin/env perl

use strict;
use warnings;

use lib ('.');
use File::Basename;
use XML::LibXML::Reader;
use Data::Dumper;

use ElNode;

my $prog  = basename($0);
my $usage = "Usage: $prog <input DB xml file> [options...][--help]";
if (!@ARGV) {
  print <<"HERE";
$usage
Use option '--help' for more information.
HERE
  exit;
}

# some globals at the BEGIN block
my %typ;

# others
my $ifil   = 0;
my $script = 0;
my $force  = 0;
my $debug  = 0;
foreach my $arg (@ARGV) {
  if ($arg =~ m{\A [-]{1,2} f}xms) {
    $force = 1;
  }
  elsif ($arg =~ m{\A [-]{1,2} d}xms) {
    $debug = 1;
  }
  elsif ($arg =~ m{\A [-]{1,2} s}xms) {
    $script = 1;
  }
  elsif ($arg =~ m{\A [-]{1,2} h}xms) {
    help();
  }
  elsif (!$ifil) {
    $ifil = $arg;
  }
  else {
    die "ERROR:  Unknown option '$arg'.\n";
  }
}

die "No input file entered.\n"
  if !-f $ifil;

my @ofils = ();
read_db_file($ifil);

if (!@ofils) {
  die "No files written";
}
if (!$script) {
  my $n = @ofils;
  my $s = $n > 1 ? 's' : '';
  print "Normal end.  See file$s:\n";
  foreach my $f (@ofils) {
    print "  $f\n";
  }
}

### SUBROUTINES ####
sub help {
  print <<"HERE";
$usage

Reads and extracts data from a DB xml file.
For a 'refentry' DB type output is to two files:

  basefilename_options.txt
  basefilename_purpose.txt

For an 'article' DB type output is to one file:

  basefilename_title.txt

Options:

  -d  Debug
HERE
  exit;
} # help

sub read_db_file {

  my $f = shift; # input xml file

  my $reader = new XML::LibXML::Reader(location => $f, no_blanks => 1)
    or die "cannot read file '$f'.\n";
  my $r = $reader;

  my @elnodes = ();

  my $currel  = 0;
  my $currdep = 0;

  while ($r->read) {

    # skip comments, etc;
    # skip unwanted nodes
    if (
	$r->nodeType == XML_READER_TYPE_PROCESSING_INSTRUCTION
	|| $r->nodeType == XML_READER_TYPE_COMMENT
	|| $r->nodeType == XML_READER_TYPE_XML_DECLARATION
       ) {
      next;
    }

    my $d = my $depth = $r->depth();
    my $s = sprintf("%-*.*s", $d*3, $d*3, " ");
    my $s2 = $s . '  ';

    # paths are unusable with namespaces
    #my $path =$r->nodePath();

    my $n = $r->name;

    if (0 && $r->nodeType == XML_READER_TYPE_END_ELEMENT) {
      print "${s}-----------------\n";
      print "${s}Element end ". $r->name,"\n";
      print "${s}  Depth ". $d,"\n";
    }

    if ($r->nodeType() == XML_READER_TYPE_ELEMENT) {
      my $el = new ElNode($n, $d);
      $currel  = $el;
      $currdep = $el->depth();

      # may have value AND children, how to handle?
      push @elnodes, $el;


      # paths are unusable with namespaces
      #$el->path($path);

      if ($r->hasValue) {
	my $val = $r->value;
        # trim leading and trailing
	$val =~ s{\A \s+}{}xms;
	$val =~ s{\s+ \z}{}xms;
	$el->value($val);
      }

      # get all attributes, if any
      if ($r->moveToFirstAttribute) {
	do {
	  $el->add_attr($r->name(), $r->value());
	  #print "Attribute ",$r->name(), " => ",$r->value,"\n";
	} while ($r->moveToNextAttribute);
	$r->moveToElement;
      }
      # back at the element
    }
    elsif ($r->nodeType == XML_READER_TYPE_TEXT) {
      if ($r->hasValue) {
	my $val = $r->value;
        # trim leading and trailing
	$val =~ s{\A \s+}{}xmsg;
	$val =~ s{\s+ \z}{}xmsg;
	$currel->value($val);
      }
    }
  }
  print "\n";

  # now we've collected all the desired nodes
  if (0) {
    foreach my $el (@elnodes) {
      $el->dump();
    }
  }

  # general reader for any xml file up to here
  # now customize as desired

  # from Cliff:

=pod

For the man pages, let's take these subsets of 3ptarb.xml as an example:

<refnamediv xml:id="name">
  <refname>3ptarb</refname>
  <refpurpose>
    Build an ARB8 shape by extruding a quadrilateral through a given thickness.
  </refpurpose>
</refnamediv>

I'd like to extract from the above the contents:

"Build an ARB8 shape by extruding a quadrilateral through a given thickness."

to a file 3ptarb_purpose.txt

and the other part of that file:

<!-- body begins here -->
<refsynopsisdiv xml:id="synopsis">
  <cmdsynopsis sepchar=" ">
    <command>3ptarb</command>
    <arg choice="opt" rep="norepeat">arb_name</arg>
    <arg choice="opt" rep="norepeat">x1</arg>
    <arg choice="opt" rep="norepeat">y1</arg>
    <arg choice="opt" rep="norepeat">z1</arg>
    <arg choice="opt" rep="norepeat">x2</arg>
    <arg choice="opt" rep="norepeat">y2</arg>
    <arg choice="opt" rep="norepeat">z2</arg>
    <arg choice="opt" rep="norepeat">x3</arg>
    <arg choice="opt" rep="norepeat">y3</arg>
    <arg choice="opt" rep="norepeat">z3</arg>
    <arg choice="opt" rep="norepeat">x/y/z</arg>
    <arg choice="opt" rep="norepeat">coord1</arg>
    <arg choice="opt" rep="norepeat">coord2</arg>
    <arg choice="opt" rep="norepeat">thickness</arg>
  </cmdsynopsis>
</refsynopsisdiv>

I'd like to extract to a text file 3ptarb_options.txt as:

"Usage: 3ptarb [arb_name] [x1] [y1] [z1] [x2] [y2] [z2] [x3] [y3] [z3] [x/y/z] [coord1] [coord2] [thickness]"

For articles, I'd like to take something like build_pattern.xml:

<article xmlns="http://docbook.org/ns/docbook" version="5.0"
  xmlns:xi="http://www.w3.org/2001/XInclude"
>
  <info><title>Using the Build Pattern Tool</title>


    <xi:include href="../../books/en/tutorial_series_authors.xml" xpointer="Intro_MGED_Tutorial_Series_III_authors"/>

    <legalnotice>
       <para>Approved for public release; distribution is unlimited</para>
   </legalnotice>
  </info>

And extract:

"Using the Build Pattern Tool"

to build_pattern_title.txt


=cut

  my %doctyp
    = (
       'refentry' => 1,
       'article'  => 1,
      );

  my $doctyp = ''; # refentry, article [others as desired...\

  my $docnode = $elnodes[0];
  $doctyp = $docnode->name();
  die "Unknown doc node type '$doctyp'"
    if !exists $doctyp{$doctyp};

  # for refentry ===============
  my @args = ();
  my $purpose = '';
  my $command = '';

  # for article  ===============
  my $title = '';

 EL:
  foreach my $el (@elnodes) {
    my $tagname = $el->name();

    # paths are unusable with namespaces
    #my $path = $el->path();

=pod

    if ($debug) {
      print "DEBUG: path = '$path'\n";
    }

=cut

    if ($doctyp eq 'refentry') {
      if ($tagname eq 'refpurpose') {
        $purpose = $el->value();
      }
      elsif ($tagname eq 'command' && !$command) {
        $command = $el->value();
      }
      elsif ($tagname eq 'arg') {
        my $arg = $el->value();
        push @args, $arg;
      }
    }
    elsif ($doctyp eq 'article') {
      if ($tagname eq 'title') {
        $title = $el->value();
        last EL;
      }
    }
  }

  # finished with extracting desired data
  # use the file name base
  my $fname = basename($f, ('.xml'));

  if ($doctyp eq 'refentry') {
    die "ERROR: No purpose" if !$purpose;
    die "ERROR: No args" if !@args;
    die "ERROR: No command" if !$command;

    # two output files
    my ($ofil, $fp);
    # purpose
    $ofil = $fname . '_purpose.txt';
    push @ofils, $ofil;
    open $fp, '>', $ofil
      or die "$ofil: $!";
    print $fp "$purpose";
    close $fp;

    # args
    $ofil = $fname . '_options.txt';
    open $fp, '>', $ofil
      or die "$ofil: $!";
    push @ofils, $ofil;
    print $fp "Usage: $command";
    foreach my $arg (@args) {
      print $fp " [$arg]";
    }
    # do NOT close the line
    #print $fp "\n";
    close $fp;

  }
  elsif ($doctyp eq 'article') {
    die "ERROR: No title" if !$title;
    # one output file
    my $ofil = $fname . '_title.txt';
    push @ofils, $ofil;
    open my $fp, '>', $ofil
      or die "$ofil: $!";
    print $fp "$title\n";
    close $fp;
  }
} # read_db_file

sub test_processNode {
  my $reader = shift @_;

  my $typ    = $reader->nodeType;
  my $typnam = $typ{$typ};
  if ($typnam =~ /ATTRIBUTE/) {
    die "found an attribute";
  }

  if (1) {
    printf "%d %d %s %d [%s]\n", ($reader->depth,
				  $typ,
				  $reader->name,
				  $reader->isEmptyElement,
				  $typnam
				 );
    my $empty  = $reader->isEmptyElement();
    my $hasval = $reader->hasValue();
    if ($empty) {
      print  "  node is empty...\n";
    }
    else {
      print  "  node is NOT empty...\n";
    }
    if ($hasval) {
      my  $val = $reader->value();
      print  "  node value is '$val'\n";
    }
    #print  "  node type name is '$typnam'\n";
  }

  my $inner = $reader->readInnerXML();

} # test_processNode

BEGIN {
  # node types
  %typ
    = (
       '0'  => 'XML_READER_TYPE_NONE'                    , #=> 0
       '1'  => 'XML_READER_TYPE_ELEMENT'                 , #=> 1
       '2'  => 'XML_READER_TYPE_ATTRIBUTE'               , #=> 2
       '3'  => 'XML_READER_TYPE_TEXT'                    , #=> 3
       '4'  => 'XML_READER_TYPE_CDATA'                   , #=> 4
       '5'  => 'XML_READER_TYPE_ENTITY_REFERENCE'        , #=> 5
       '6'  => 'XML_READER_TYPE_ENTITY'                  , #=> 6
       '7'  => 'XML_READER_TYPE_PROCESSING_INSTRUCTION'  , #=> 7
       '8'  => 'XML_READER_TYPE_COMMENT'                 , #=> 8
       '9'  => 'XML_READER_TYPE_DOCUMENT'                , #=> 9
       '10' => 'XML_READER_TYPE_DOCUMENT_TYPE'           , #=> 10
       '11' => 'XML_READER_TYPE_DOCUMENT_FRAGMENT'       , #=> 11
       '12' => 'XML_READER_TYPE_NOTATION'                , #=> 12
       '13' => 'XML_READER_TYPE_WHITESPACE'              , #=> 13
       '14' => 'XML_READER_TYPE_SIGNIFICANT_WHITESPACE'  , #=> 14
       '15' => 'XML_READER_TYPE_END_ELEMENT'             , #=> 15
       '16' => 'XML_READER_TYPE_END_ENTITY'              , #=> 16
       '17' => 'XML_READER_TYPE_XML_DECLARATION'         , #=> 17
      );

} # BEGIN
