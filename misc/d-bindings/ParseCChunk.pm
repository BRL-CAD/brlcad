package ParseCChunk;

#use strict;
use warnings;

use CGrammar; # <== an auto-generated file

sub parse_chunk {
  my $aref = shift @_;

  my $text = join(' ', @{$aref});

  my $parser = CGrammar->new();
  my $parse_tree = $parser->statement_list($text);

  if (!defined $parse_tree) {
    print "=== DEBUG BAD C CODE : lines to be parsed:\n";
    print "  $_\n" for @{$aref};
    print "the single line:\n";
    print "$text\n";
  }
  else {
    print "=== GOOD PARSE\n";
  }

=pod

    or
      #{
	#print "FATAL: lines:\n";
	#print "  $_\n" for @{$aref};
	warn "bad C code";
      #}

  #use Data::Dumper;
  #warn Dumper [ $parse_tree ];

=cut

} # parse_chunk

# mandatory true return for a Perl module
1;
