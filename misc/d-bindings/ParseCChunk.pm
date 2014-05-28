package ParsePPCHeader;

# parse a pre-processed C header (with gcc -E)

use strict;
use warnings;

use CGrammar; # <== an auto-generated file

sub parse_cfile {
  my $ifil = shift @_;
  open my $fp, '<', $ifil
    or die "$ifil: $!";

  local $/;
  my @ilines = <$fp>;
  my $text = join(' ', @ilines);

  $::RD_HINT = 1;
  $::RD_AUTOACTION = q {%item};
  my $parser = CGrammar->new();

  my $parse_tree = $parser->translation_unit($text);
  die "undef \$parse_tree" if !defined $parse_tree;

  use Data::Dumper;
  print Dumper(\@CGrammar::namespace000item);

  die "debug exit";

} # parse_cfile

sub parse_chunk {
  my $aref = shift @_;

  my $text = join(' ', @{$aref});

  my $parser = CGrammar->new();

  #my $parse_tree = $parser->type_specifier($text);
  #my $parse_tree = $parser->enum_specifier($text);
  my $parse_tree = $parser->translation_unit($text);

  if (!defined $parse_tree) {
    print "=== DEBUG BAD C CODE : lines to be parsed:\n";
    print "  $_\n" for @{$aref};
    print "the single line:\n";
    print "$text\n";
  }
  else {
    print "=== GOOD PARSE : lines to be parsed:\n";
    print "  $_\n" for @{$aref};
    print "the single line:\n";
    print "$text\n";
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
