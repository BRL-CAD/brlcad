#!/usr/bin/env perl

use strict;
use warnings;

use File::Basename;
use Data::Dumper;

use XML::XPath;
use XML::XPath::XMLParser;

use BRLCAD_DOC ('print_xml_header', 'print_xhtml_header');

# global vars
my $vfil     = 'db-file-list.txt';
my $sfil     = 'find-db-files.pl';

my $prog = basename($0);
my $usage = "Usage: $prog --go | <input file> [options...]";
if (!@ARGV) {
  print <<"HERE";
$usage

Use option '--help' or '-h' for more details.
HERE
  exit;
}

my $verbose  = 0;
my $debug    = 0;
my $ifil     = 0;
my $ofil     = 'index.html';

foreach my $arg (@ARGV) {
  my $val = undef;
  my $idx = index $arg, '=';

  if ($idx >= 0) {
    $val = substr $arg, $idx+1;
    $arg = substr $arg, 0, $idx;
  }

  if ($arg =~ /^[-]{1,2}h/i) {
    help(); # exits from help()
  }
  elsif ($arg =~ /^[-]{1,2}g/i) {
    ; # ignore
  }
  elsif ($arg =~ /^[-]{1,2}v/i) {
    $verbose = 1;
  }
  elsif ($arg =~ /^[-]{1,2}d/i) {
    $debug = 1;
  }
  elsif (!$ifil) {
    $ifil = $arg;
  }
  else {
    die "ERROR:  Unknown argument '$arg'.\n"
  }
}

#print "DEBUG: args = '@ARGV'\n"; die "debug exit";

# error checks
my $errors = 0;
# check for input file
if (!$ifil) {
  if (-f $vfil) {
    $ifil = $vfil;
  }
  else {
    print "ERROR: No input file entered.\n";
    ++$errors;
  }
}
elsif (! -f $ifil) {
  print "ERROR:  Input file '$ifil' not found.\n";
  ++$errors;
}

die "ERROR exit.\n"
  if $errors;

open my $fp, '<', $ifil
  or die "$ifil: $!\n";

my %db = (); # db{$dir}{fils}{$fil} = 1
my %dir
  = (
     1 => { dir => 'articles', name => '',},
     2 => { dir => 'books', name => '',},
     3 => { dir => 'lessons', name => '',},
     4 => { dir => 'presentations', name => '',},
     5 => { dir => 'specifications', name => '',},
     6 => { dir => 'system', name => '',},
     );

# some files are not for normal viewing
my %ignore
  = (
     'tutorial_series_authors.xml' => 1,
    );

while (defined(my $line = <$fp>)) {
  # eliminate any comments
  my  $idx = index $line, '#';
  if ($idx >= 0) {
    $line = substr $line, 0, $idx;
  }

  # tokenize (may have one or more files on the line)
  my @d = split(' ', $line);
  next if !defined $d[0];
  while (@d) {
    my $f = shift @d;
    if (! -f $f) {
      print "WARNING:  DB file '$f' not found...skipping.\n"
	if $verbose;
      next;
    }

    # need the dir and basename
    my $fil  = basename($f);

    # some files should be ignored
    next if exists $ignore{$fil};

    my $dirs = dirname($f);

    # break the dir down into parts
    my @dirs = split('/', $dirs);
    die "What? \$dirs = '$dirs'!"
      if !@dirs;

    my $tdir = shift @dirs;
    die "What? \$dirs = '$dirs'!"
      if !@dirs;
    die "What? \$dirs = '$dirs'!"
      if !defined $tdir;
    # reassemble
    my $sdir = shift @dirs;
    $sdir = '' if !defined $sdir;
    while (@dirs) {
      my $d = shift @dirs;
      $sdir .= '/';
      $sdir .= $d;
    }

    $db{$tdir}{subdirs}{$sdir}{fils}{$fil} = 1;

  }
}
close $fp;

#print Dumper(\%db);
#die "debug exit";

my @dirs  = ();
my @names = ();
foreach my $k (sort { $a <=> $b } keys %dir) {
  my $dir = $dir{$k}{dir};
  print "dir = '$dir'\n" if $verbose;
  push @dirs, $dir;
  my $name = (exists $dir{$k}{name} && $dir{$k}{name})
           ? $dir{$k}{name} : ucfirst $dir;
  push @names, $name;
}


open $fp, '>', $ofil
  or die "$ofil: $!";

write_headers($fp);

print $fp "  <h1>BRL-CAD Documentation</h1>\n";
print $fp "\n";

foreach my $dir (@dirs) {
  my $name = shift @names;
  my %sdir = %{$db{$dir}{subdirs}};

  # a section chunk (table)

  print $fp "  <h2>$name</h2>\n";
  print $fp "\n";

  print $fp "   <table width='100%' align='left'>\n";
  print $fp "\n";

  my @sdirs = (sort keys %sdir);
  foreach my $sdir (@sdirs) {
    my %fil = %{$sdir{$sdir}{fils}};
    my @fils = (sort keys %fil);
    foreach my $fil (@fils) {
      # reassemble complete file name
      my $f = $dir;
      if ($sdir) {
	$f .= "/$sdir";
      }
      $f .= "/$fil";
      if (-f $f) {
	print "Found DB xml file '$f'.\n"
	  if $verbose;
      }
      else {
	print "WARNING: DB xml file '$f' not found!\n";
	next;
      }

      # file name without extension
      my $fname = $fil;
      $fname  =~ s{\.xml \z}{}xmsi;

      # here we can try to find the title from inside the file
      my $xp = XML::XPath->new(filename => $f);

      # find all titles
      my $nodeset = $xp->find('//title'); # || $xp->find('TITLE');

      my $title = 0;
      foreach my $node ($nodeset->get_nodelist) {
	$title = XML::XPath::XMLParser::as_string($node);
	if (0 && $debug) {
	  print "DEBUG: Found first title: '$title'\n";
	  print "  file: $fil\n";
	}
	last;
      }

      # manipulate title
      if ($title) {
	if ($debug) {
	  print "==== DEBUG: Found first title: '$title'\n";
	  print "  file: $fil\n";
	}
	# strip bounding element tags
	$title =~ s{<[/]?title>}{}gi;
	if ($debug) {
	  print "  stripped: '$title'\n";
	}

	if ($title =~ m{\s* description \s*}xmsi) {
	  # a man page--use the file name
	  $title = $fname;
	  if ($debug) {
	    print "  a man page: '$title'\n";
	  }
	}
      }

      # use the file name if nothing else
      $title = $fname
	if !$title;

      # for links
      # need the html extension
      my $fhtm = $f;
      $fhtm =~ s{\.xml \z}{\.html}xmsi;
      # and pdf
      my $fpdf = $f;
      $fpdf =~ s{\.xml \z}{\.pdf}xmsi;
      $fpdf = "../pdf/${fpdf}";

      my $lang = get_lang($f);
      $lang = '' if (!defined $lang || !$lang);

      # an entry
      #print $fp "      <li>\n";
      print $fp "      <tr>\n";

      # four cells
      print $fp "      <td width='50%'>$title</td>\n";
      print $fp "      <td width='10%'><a href='$fhtm'>(html)</a></td>\n";
      print $fp "      <td width='10%'><a href='$fpdf'>(pdf)</a></td>\n";
      print $fp "      <td>$lang</td>\n";

      print $fp "      </tr>\n";
    }
  }

  # end of table
  print $fp "    </table>\n"; # a numbered list
  print $fp "\n";

}

write_enders($fp);
close $fp;

print "Normal end.  See output file '$ofil'.\n";

#### SUBROUTINES ####
sub write_headers {
  my $fp = shift @_;

  print_xhtml_header($fp, 'utf');

    print $fp <<"HERE";
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
  <title>BRL-CAD Documentation</title>

<!--
  <link rel="stylesheet" type="text/css" href="css/std_props.css" />
-->

</head>
<body>

HERE

} # write_headers

sub write_enders {
  my $fp = shift @_;

  print $fp <<"HERE";
</body>
</html>
HERE

} # write_enders

sub help {
  print <<"HERE";
$usage

Generates an html index file listing the html files found
in the input file organized by directory.  Input
is a list of source files to include.  Output is to file
'$ofil' which should be installed by the build
system in the usual manner.

Note that the \%dir hash must be edited to represent the
desired order of presentation of top-level sub-directories
and their presentation names.

Options:

  --verbose       show more info about what's going on

Notes:

File and directory names must not have spaces.

The list file may be generated with the shell script '$sfil'
The output file is named '$vfil'.  If the file name
'$vfil' is found it will be used automatically if no file
name is entered at the prompt (in that case at least on arg must be
entered so use '--go' if no other arg is needed).

In the list file all blank lines and all characters on a line at and
after the '#' symbol are ignored.

HERE
  exit;
} # help

sub get_lang {
  my $s = shift @_;

  # extract the two-letter language code
  $s =~ m{(?: \A | [/\\]{1}) ([a-z]{2}) [/\\]{1} }xmsi;

  my $lang = 0;
  if ($1) {
    my $code = lc $1;
    print "NOTE:  Lang code is '$code'.\n"
      if $verbose;
    # we have a possible language couplet
    if ($code eq 'en') {
      ; # default
    }
    elsif ($code eq 'es') {
      $lang = 'Spanish';
    }
    elsif ($code eq 'it') {
      $lang = 'Italian';
    }
    elsif ($code eq 'ru') {
      $lang = 'Russian';
    }
  }

  return $lang;

} # get_lang
