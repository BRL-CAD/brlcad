#!/usr/bin/env perl

use strict;
use warnings;

use File::Basename;
use Data::Dumper;

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
my $ofil     = 'index.xml';

foreach my $arg (@ARGV) {
  my $arg = shift @ARGV;
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
  elsif (!$ifil) {
    $ifil = $arg;
  }
  else {
    die "ERROR:  Unknown argument '$arg'.\n"
  }
}

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

foreach my $dir (@dirs) {
  my $name = shift @names;
  my %sdir = %{$db{$dir}{subdirs}};

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
	print "Found DB xml file '$f'.\n";
      }
      else {
	print "WARNING: DB xml file '$f' not found!\n";
      }
    }
  }

}

print "Normal end.  See output file '$ofil'.\n";

#### SUBROUTINES ####
sub help {
  print <<"HERE";
$usage

Generates a DocBook xml (DB) source file ($ofil) listing the
files found in the input file organized by directory.  Input
is a list of source files to include.  Output is to file
'$ofil' which should then be processed by the document
build system in the usual manner.

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
