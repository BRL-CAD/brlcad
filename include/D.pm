package D;

use strict;
use warnings;

use Storable;
use Digest::MD5::File qw(file_md5_hex);
use Readonly;
use Data::Dumper;

# local vars
my $storefile = '.md5tablestore';
# key md5 file hases by file name in this table;
my %md5table;

# vars for export
our $force   = 0;
our $verbose = 0;
our $debug   = 0;
our $clean   = 0;

# global vars
Readonly our $NEW  => -1;
Readonly our $SAME =>  0;
Readonly our $DIFF =>  1;

#### subroutines ####
sub remove_md5hash_store {
  unlink $storefile;
  %md5table = ();
} # remove_md5hash_store

sub convert1 {
  my $ifils_ref = shift @_; # @ifils
  my $ofils_ref = shift @_; # @ofils
  my $fref      = shift @_; # %f
  my $sref      = shift @_; # %stats

  my $incdirs  = '-I. -I../src/other/tcl/generic';

  foreach my $ifil (@{$ifils_ref}) {
    my ($process, $stat) = (0, 0);
    my ($curr_hash, $prev_hash);

    # check to see current status of input file
    $curr_hash = file_md5_hex($ifil);
    $prev_hash = retrieve_md5hash($ifil);

    if (!$prev_hash || $prev_hash ne $curr_hash) {
      $process = 1;
      store_md5hash($ifil, $curr_hash);
    }
    $process = 1
      if ($force);

    # final output file
    my $ofil = $ifil;
    $ofil =~ s{\.h \z}{\.di}xms;
    $prev_hash = retrieve_md5hash($ofil);
    if (!$prev_hash) {
      $process = 1;
    }

    # don't overwrite
    if (-e $ofil && !$force) {
      $process = 0;
    }

    if (!$process) {
      print "D::convert1(): Skipping input file '$ifil'...\n"
	if $verbose;
      next;
    }

    print "D::convert1(): Processing file '$ifil' => '$ofil'...\n"
      if $verbose;

    my $msg = qx(gcc -E -x c $incdirs -o $ofil $ifil);

    # update hash for the new file
    $curr_hash = calc_md5hash($ofil);

    if (!$prev_hash || $prev_hash ne $curr_hash) {
      if ($verbose) {
	print "D::convert1(): Output file '$ofil' hashes:\n";
	print "  prev '$prev_hash'\n";
	print "  curr '$curr_hash'\n";
      }
      store_md5hash($ofil, $curr_hash);
      push @{$ofils_ref}, $ofil;
    }
    else {
      print "D::convert1(): Output file '$ofil' has not changed.\n"
	if $verbose;
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

  if ($debug) {
    print Dumper($tabref); die "debug exit";
  }
} # store_md5hash

sub retrieve_md5hash {
  # return stored file hash or 0 if not found in hash
  my $f = shift @_;

  my $tabref = retrieve($storefile) if -e $storefile;
  $tabref = \%md5table if !defined $tabref;

  my $md5hash = 0;
  if (exists $tabref->{$f}) {
    $md5hash = $tabref->{$f};
  }

  return $md5hash;

} # retrieve_md5hash

sub collect_files {
  # deletes generated files if $D::clean
  my $href  = shift @_; # @h
  my $diref = shift @_; # @di

  my @di = glob("*.di");
  if ($clean) {
    unlink @di;
    remove_md5hash_store();
  }
  else {
    push @{$diref}, @di;
  }

  # ignored .h files
  my @ignore
    = (
       'conf.h',
       'dvec.h',
       'redblack.h',
      );
  my %ignore = ();
  @ignore{@ignore} = ();

  my @h  = glob("*.h");
  foreach my $f (@h) {
    next if exists $ignore{$f};
    push @{$href}, $f;
  }

} # collect_files

# mandatory true return from a Perl module
1;
