package D;

use strict;
use warnings;

use Storable;
use Digest::MD5::File qw(file_md5_hex);
use Readonly;

# local vars
my $storefile = '.md5tablestore';
# key md5 file hases by file name in this table;
my %md5table;

# vars for export

# global vars
Readonly our $NEW  => -1;
Readonly our $SAME =>  0;
Readonly our $DIFF =>  1;

#### subroutines ####
sub convert1 {
  my $ifils_ref = shift @_; # @ifils
  my $ofils_ref = shift @_; # @ofils
  my $fref      = shift @_; # %f
  my $sref      = shift @_; # %stats
  my $force     = shift @_;

  my $incdirs  = '-I. -I../src/other/tcl/generic';

  foreach my $ifil (@{$ifils_ref}) {
    my ($process, $stat) = (0, 0);
    # check to see current status of input file
    $stat = $fref->{$ifil}{status};
    if ($stat == $DIFF ||$stat == $NEW) {
      $process = 1;
    }

    # final output file
    my $ofil = $ifil;
    $ofil =~ s{\.h \z}{\.di}xms;
    $stat = $fref->{$ofil}{status};
    if (defined $stat && $stat == $NEW) {
      $process = 1;
    }
    elsif (-e $ofil && !$force) {
      $process = 0;
    }

    if (!$process) {
      print "Skipping input file '$ifil'...\n";
      next;
    }

    print "Processing file '$ifil' => '$ofil'...\n";
    my $msg = qx(gcc -E -x c $incdirs -o $ofil $ifil);

    # update hash for the new file
    my $curr_hash = file_md5_hex($ofil);
    my $prev_hash = retrieve_md5hash($ofil);

    if (!$prev_hash || $prev_hash ne $curr_hash) {
      store_md5hash($ofil, $curr_hash);
      push @{$ofils_ref}, $ofil;
    }
    else {
      print "Output file '$ofil' has not changed.\n";
    }
  }

} # convert1

sub calc_md5hash {
  my $f = shift @_;
  return file_md5_hex($f);
} # calc_md5hash

sub store_md5hash {
  # store an input hash (or calculates a new one) and stores it
  my $f       = shift @_;
  my $md5hash = shift @_;

  $md5hash = calc_md5hash($f) if !defined $md5hash;

  my $tabref = retrieve($storefile) if -e $storefile;
  $tabref = \%md5table if !defined $tabref;
  $tabref->{$f} = $md5hash;
  store $tabref, $storefile;
} # store_md5hash

sub retrieve_md5hash {
  # return stored file hash or 0 if not found in hash
  my $f = shift @_;

  my $tabref = retrieve($storefile) if -e $storefile;
  $tabref = \%md5table if !defined $tabref;

  my $md5hash = 0;
  if (exists $tabref->{$f}) {
    $md5hash = $tabref->{$f} = $md5hash;
  }

  return $md5hash;

} # retrieve_md5hash

# mandatory true return from a Perl module
1;
