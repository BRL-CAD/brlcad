package D;

use strict;
use warnings;

use Storable;
use Digest::MD5::File qw(file_md5_hex);

my $storefile = '.md5tablestore';
# key md5 file hases by file name in this table;
my %md5table;

sub store_md5hash {
  my $f = shift @_;
  my $md5hash = file_md5_hex($f);
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
