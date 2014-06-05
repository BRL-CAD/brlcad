package ParsePPCHeader;

# parse a pre-processed C header (with gcc -E)

use strict;
use warnings;

use CGrammar;  # <== an auto-generated file
use CGrammar2; # <== an auto-generated file

# for globals
use G;

# for debugging:
my $COMP = 0;

sub parse_cfile {
  # input can be on of four types:
  #   a file name of lines
  #   a file ptr to a file of lines
  #   an array ref of lines
  #   a string
  my $argref = shift @_;
  my $r = ref $argref;
  die "FATAL:  Input arg is NOT a HASH ref."
    if $r ne 'HASH';

  my $ityp = $argref->{ityp};
  die "FATAL:  Input arg 'ityp' not found."
    if (!defined $ityp || !exists $G::ityp{$ityp});

  my $ival = $argref->{ival};
  die "FATAL:  Input arg 'ival' not found."
    if !defined $ival;

  my $oref = $argref->{oref};
  $oref = 0 if !defined $oref;

  # The diagnostics provided by the tracing mechanism always go to
  # STDERR. If you need them to go elsewhere, localize and reopen
  # STDERR prior to the parse.
  #
  # For example:
  #
  #  {
  #      local *STDERR = IO::File->new(">$filename") or die $!;
  #
  #      my $result = $parser->startrule($text);
  #  }

  if ($oref) {
    my $errfil = 'PRD-errfile.txt';
    local *STDERR = IO::File->new(">$errfil")
      or die $!;
    push @{$oref}, $errfil
      if $oref;
  }

  my $text;

  if ($argref->{ityp} eq 'fnam') {
    local $/;
    open my $fp, '<', $ival
      or die "$ival: $!";
    my @ilines = <$fp>;
    $text = join(' ', @ilines)
  }
  elsif ($argref->{ityp} eq 'fp') {
    local $/;
    my $fp = $ival;
    my @ilines = <$fp>;
    $text = join(' ', @ilines);
  }
  elsif ($argref->{ityp} eq 'str') {
    $text = $ival;
  }
  elsif ($argref->{ityp} eq 'aref') {
    local $/;
    $text = join(' ', @{$ival});
  }

  my $parser = CGrammar->new();

  # the absolute top level
  my $ptree = $parser->translation_unit($text);

  if (!defined $ptree) {
    warn "undef \$ptree";
    my $line = $argref->{first_line};
    $line = -1 if !defined $line;
    if ($G::debug) {
      print "DEBUG: test input (line: $line)\n";
      print "  text '$text'\n";
    }
  }

  return $ptree;

} # parse_cfile

sub dump_parse_tree {
  my $fp    = shift @_;
  my $ptree = shift @_;

  use Data::Dumper;
  use Data::Structure::Util qw(unbless);
  $Data::Dumper::Terse    = 1;         # don't output names where feasible (doesn't work for my tree)
  $Data::Dumper::Indent   = 1;         # mild pretty print
  #$Data::Dumper::Indent   = 3;         # pretty print with array indices
  $Data::Dumper::Purity   = 1;
  $Data::Dumper::Deparse  = 1;
  #$Data::Dumper::Sortkeys = 1;         # sort hash keys

  print $fp Dumper(unbless($ptree));

} # dump_parse_tree

sub get_spaces {
  my $level = shift @_;
  my $ns = $level * 3;
  my $s = sprintf "%-*.*s", $ns, $ns, ' ';
  return $s;
} # get_spaces

sub print_parse_tree {
  my $fp    = shift @_;
  my $ptree = shift @_;

  print $fp "syntax tree:\n";

  print_object($fp, $ptree, 1);

} # print_parse_tree

sub print_object {
  my $fp    = shift @_;
  my $obj   = shift @_;
  my $level = shift @_; # use for number of leading spaces

  $level = 0 if !defined $level;
  my $s = $level ? get_spaces($level) : '';

  # get type of object
  my $r = ref $obj;

  if (!$r) {
    print $fp "${s}scalar value: '$obj'\n";
  }
  elsif ($r eq 'ARRAY') {
    print $fp "${s}array ref: '$obj'\n";
    foreach my $val (@{$obj}) {
      if (!defined $val) {
	print $fp "${s}value: 'undef'\n";
	next;
      }
      print_object($fp, $val, $level + 1);
    }
  }
  elsif ($r eq 'HASH') {
    print $fp "${s}hash ref: '$obj'\n";
    while (my ($key, $val) = each %{$obj}) {
      print $fp "${s}hash key: '$key'\n";
      if (!defined $val) {
	print $fp "${s}value: 'undef'\n";
	next;
      }
      print_object($fp, $val, $level + 1);
    }
  }
  else {
    warn_ref($r, $obj, $s, __FILE__, __LINE__);
  }

} # print_object

sub warn_ref {
  my ($r, $uref, $s, $fil, $linenum) = @_;
  print "${s}WARNING:   unhandled ref type '$r'\n";
  print "${s}scalar value: '$uref'\n";
  print "${s}file '$fil', line $linenum\n";
} # warn_ref

# mandatory true return for a Perl module
1;
