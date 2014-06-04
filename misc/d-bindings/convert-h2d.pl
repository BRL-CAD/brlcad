#!/usr/bin/env perl

# converts a C .h file to a rudimentary .d file for linking C to D

use strict;
use warnings;

use File::Basename;
use lib('.');

use G; # globals
use D; # local convenience module
use BP;

# use the reference D compiler
my $DMD = '/usr/bin/dmd';
die "ERROR:  Reference compiler 'dmd' not found.\n"
  if ! -f $DMD;

# check for proper source dir:
die "ERROR:  Unknown include dir for BRL-CAD public headers: '$BP::IDIR' (see D.pm)\n"
  if ! -d $BP::IDIR;

# check for proper D directory for interface files
die "ERROR:  Unknown include dir for BRL-CAD public headers: '$BP::DIDIR' (see D.pm)\n"
  if ! -d $BP::DIDIR;

my $p = basename($0);
my $usage  = "Usage: $p mode [options...]\n\n";
$usage    .= "  modes:   -r | -cN | -h=X | -b | -e\n";
$usage    .= "  options: -f -d -c -n=X -h";
if (!@ARGV) {
  print <<"HERE";
$usage

Use option '-h' for details.
HERE

  exit;
}

# modes
my $report  = 0;
my $convert = 0;
my $Bgen    = 0;
my $Egen    = 0;

my $mode_selected = 0;
sub zero_modes {
  my $ms = shift @_;
  $ms = 0 if !defined $ms;
  $report  = 0;
  $convert = 0;
  $Bgen    = 0;
  $Egen    = 0;

  $mode_selected = 1 if $ms;
} # zero_modes

my @ifils = ();

foreach my $arg (@ARGV) {
  my $val = undef;
  my $idx = index $arg, '=';
  if ($idx >= 0) {
    print "DEBUG: input arg is '$arg'\n"
      if $G::debug;

    $val = substr $arg, $idx+1;
    if ((!defined $val) && !$G::debug) {
      die "ERROR:  For option '$arg' \$val is empty.'\n";
    }
    $arg = substr $arg, 0, $idx;
    if ($G::debug) {
      print "arg is now '$arg', val is '$val'\n";
      die "debug exit";
    }
  }

  if ($arg =~ m{\A -f}xms) {
    $G::force = 1;
  }
  elsif ($arg =~ m{\A -d}xms) {
    $G::debug = 1;
  }
  elsif ($arg =~ m{\A -v}xms) {
    $G::verbose = 1;
  }
  elsif ($val && $arg =~ m{\A -h}xms ) {
    my $f = $val;
    die "ERROR:  Input file '$f' not found.\n"
      if (! -e $f);
    # one more check: must be a '.h' file
    die "ERROR:  Input file '$f' must be a '$G::Hsuf' file.\n"
      if ($f !~ m{$G::Hsuf \z}xms);
    @ifils = ($f);
  }
  elsif ($arg =~ m{\A -h}xms) {
    help();
  }
  elsif ($arg =~ m{\A -C \z}xms) {
    $G::clean = 1;
  }
  elsif ($arg =~ m{\A -n}xms) {
    if ($val !~ m{\A (?: 0 | [1-9][0-9]*) \z}x) {
      print "ERROR:  X in option '-n=X' must be a positive integer,\n";
      die   "        but it's value is '$val'.\n";
    }
    $G::maxchunks = $val;
  }

  # modes
  elsif ($arg =~ m{\A -r}xms) {
    zero_modes(1);
    $report = 1;
  }
  elsif ($arg =~ m{\A -c([1]){1}}xms) {
    # note c2 is not working yet
    # note c3 is not working yet
    zero_modes(1);
    $convert = $1;
  }
  elsif ($arg =~ m{\A -b}xms) {
    zero_modes(1);
    $Bgen = 1;
  }
  elsif ($arg =~ m{\A -e}xms) {
    zero_modes(1);
    $Egen = 1;
  }
  elsif ($arg =~ m{\A -[Tt]{1}}xms) {
    # a test mode
    zero_modes(1);
    $convert = 1;

    $G::force        = 1;
    $G::verbose      = 1;
    $G::debug        = 1;
    $G::clean        = 1;
    $G::devel        = 1;
    $G::quitundef    = 0;
    $G::maxchunks    = 1;
    $G::inspect_tree = 1;

  }

  # error
  else {
    die "ERROR:  Unknown arg '$arg'.\n";
  }
}

die "ERROR:  No mode selected.\n"
  if !$mode_selected;

if ($G::devel) {
  @ifils = ("${BP::IDIR}/bu.h");
}

# collect all .h and .d files; note that some .h files are obsolete
# and are so indicated inside the following function
my (@h, @d) = ();
D::collect_files(\@h, \@d); # deletes generated files if $G::clean
my $nh   = @h;
my $nd  = @d;
my @fils = (@h, @d);
my $nf   = @fils;
my %f;
@f{@fils} = ();
my %stats = ();
# collect their statuses
get_status(\%stats, \%f);

my @ofils = ();
if ($report) {
  print "Mode is '-r' (report)...\n\n";
  my $n = $nh ? $nh : 'zero';
  print "There are $n C header files in the '$D::IDIR' directory:\n";
  print "  new:       $stats{h}{new}\n";
  print "  unchanged: $stats{h}{sam}\n";
  print "  changed:   $stats{h}{dif}\n";

  if ($G::verbose) {
    print "  Files:\n";
    print "    $_\n" for (@h);
  }

  $n = $nd ? $nd : 'zero';
  print "There are $n D interface files in the '$BP::DIDIR' directory:\n";
  print "  new:       $stats{di}{new}\n";
  print "  unchanged: $stats{di}{sam}\n";
  print "  changed:   $stats{di}{dif}\n";

  if ($G::verbose) {
    print "  Files:\n";
    print "    $_\n" for (@d);
  }
}
elsif ($convert) {
  @ifils = @h
    if !@ifils;
  print "Mode is '-c$convert' (convert method $convert)...\n\n";
  die "debug exit"
    if (0 && $G::debug);

  D::convert(\@ifils, \@ofils, \%f, \%stats, $convert);
}
elsif ($Bgen) {
  BP::get_brlcad_inc_data(\@ofils, $G::debug);
}
elsif ($Egen) {
  # requires BH.pm
  my $f = 'BH.pm';
  die "ERROR:  Need auto-generated file '$f'.\n"
    if ! -f $f;
  BP::get_brlcad_ext_inc_data(\@ofils, $G::debug);
}

print "Normal end.\n";
if (@ofils) {
  my $s = (1 < @ofils) ? 's' : '';
  print "See output file$s:\n";
  print "  $_\n" for @ofils;
}

#### subroutines ####
sub get_status {
  my $sref = shift @_; # \%stats
  my $fref = shift @_; # \%f

  $sref->{h}{new} = 0;
  $sref->{h}{sam} = 0;
  $sref->{h}{dif} = 0;

  $sref->{di}{new} = 0;
  $sref->{di}{sam} = 0;
  $sref->{di}{dif} = 0;

  foreach my $f (keys %{$fref}) {
    my $s = file_status($f);
    my $typ = $f =~ m{$G::Hsuf \z}xms ? 'h' : 'd';
    die "ERROR:  Uknown file type for file '$f'!"
      if ($typ eq 'd' && $f !~ m{$G::Dsuf \z}xms);

    $fref->{$f}{status} = $s;
    $fref->{$f}{typ}    = $typ;

    if ($s == $G::NEW) {
      ++$sref->{$typ}{new};
    }
    elsif ($s == $G::SAME) {
      ++$sref->{$typ}{sam};
    }
    elsif ($s == $G::DIFF) {
      ++$sref->{$typ}{dif};
    }
    else {
      die "ERROR:  Unknown status for '$f'!" if ! defined $s;
    }
  }
} # get_status

sub file_status {
  # uses an md5 hash to determine if a file has changed (or is new)
  my $f = shift @_;

  # current hash:
  my $curr_hash = DS::calc_md5hash($f);
  # previous, if any
  my $prev_hash = DS::retrieve_md5hash($f);

  my $status = undef;
  if (!$prev_hash) {
    $status = $G::NEW;
    #print "Note that '$f' is new or unprocessed.\n";
  }
  elsif ($prev_hash eq $curr_hash) {
    $status = $G::SAME;
    #print "Note that '$f' has not changed.\n";
  }
  else {
    DS::store_md5hash($f, $curr_hash);
    $status = $G::DIFF;
    #print "Note that '$f' has changed.\n";
  }

  die "ERROR:  Unknown status for '$f'!" if ! defined $status;

  return $status;

} # file_status

sub help {
  print <<"HERE";
$usage

modes:

  -r    report status of .h and .d files in the directory

  -c1   use method 1 to convert .h files to .d files
          method 1 uses a C compiler to convert the header to an
          intermediate file for further manipulation
        this program uses GNU gcc:
          \$ gcc -E -o outfile -I incdir .. -I incdirN infile

  -c2   use method 2 to convert .h files to .d files
          method 2 uses 'dstep' to convert the header to an
          intermediate file for further manipulation
        this program uses dstep:
          \$ dstep infile -o outfile

  -c3   NOT YET WORKING!!!
        use method 3 to convert .h files to .d files
          method 1 uses a C compiler to convert the header to an
          intermediate file for further manipulation
        this program uses GNU gcc:
          \$ gcc -fdump-translation-unit -o outfile -I incdir .. -I incdirN infile

  -h=X  restrict conversion attempt to just file X

options:

  -f    force overwriting files
  -d    debug
  -h    help
  -C    cleans out all generated files and the stored file hashes
  -n=X  parse after reading X chunks instead of the entire file

Notes:

  1.  The convert options will normall convert all known .h files to
      .d files only if they have no existing .d file.

  2.  Excptions to note 1:

      + Use of the '-h=X' option restricts the set of .h files to be
        considered to the single file X.

      + Use of the '-f' option will allow overwriting an existing .d
        file.

  3.  See conversion information at:

      + <http://wiki.dlang.org/Converting_C_.h_Files_to_D_Modules>

      + <http://stackoverflow.com/questions/994732/how-can-i-parse-a-c-header-file-with-perl>

HERE

  exit;
} # help
