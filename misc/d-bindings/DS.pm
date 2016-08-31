package DS;

# for the storage and manipulation of file data

use strict;
use warnings;

use Readonly;
use Storable;
use Digest::MD5::File qw(file_md5_hex);

# local vars ===========================
my $storefile = '.path_md5hash_tablestore';

# keep path md5 hash data in this table
my %path_md5hash_table;
# key: path
# val: md5 hash of path

my $debug = 0;

#### subroutines ####
sub calc_md5hash {
  my $pth = shift @_;
  return 0 if !-f $pth;
  return file_md5_hex($pth);
} # calc_md5hash

sub store_md5hash {
  # store an input hash (or calculates a new one) and stores it
  my $pth     = shift @_;
  my $md5hash = shift @_;

  $md5hash = calc_md5hash($pth)
    if !defined $md5hash;

  my $tabref = retrieve($storefile)
    if -e $storefile;
  $tabref = \%path_md5hash_table
    if !defined $tabref;
  $tabref->{$pth} = $md5hash;
  store $tabref, $storefile;

  if (0 && $debug) {
    print Dumper($tabref); die "debug exit";
  }
} # store_md5hash

sub retrieve_md5hash {
  # return stored path hash or 0 if not found in hash
  my $pth = shift @_;

  #die "ERROR:  input path '$pth' not defined" if !defined $pth;
  #die "ERROR:  input path '$pth' is zero" if !$pth || $pth eq '0';

  my $tabref = retrieve($storefile)
    if -e $storefile;
  $tabref = \%path_md5hash_table
    if !defined $tabref;

  my $md5hash = 0;
  $md5hash = $tabref->{$pth}
    if (exists $tabref->{$pth});

  return $md5hash;

} # retrieve_md5hash

sub remove_md5hash_store {
  unlink $storefile;
  %path_md5hash_table = ();
} # remove_md5hash_store

# mandatory true return from a Perl module
1;

