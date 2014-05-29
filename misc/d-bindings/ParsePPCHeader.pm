package ParsePPCHeader;

# parse a pre-processed C header (with gcc -E)

use strict;
use warnings;

use CGrammar; # <== an auto-generated file

sub parse_cfile {

=pod

  # IMPORTANT: namespaces as defined below are critical for use in the
  # CGrammar module!!
  package Parse::Recdescent::CGrammar;

=cut

  my $ifil = shift @_;
  my $oref = shift @_;

  $oref = 0 if !defined $oref;

  open my $fp, '<', $ifil
    or die "$ifil: $!";

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

  my $errfil = 'PRD-errfile.txt';
  local *STDERR = IO::File->new(">$errfil")
    or die $!;
  push @{$oref}, $errfil
    if $oref;

  local $/;
  my @ilines = <$fp>;
  my $text = join(' ', @ilines);

=pod

  $::RD_AUTOACTION = q {
    { $#item==1 ? $item[1] : "$item[0]_node"->new(@item[1..$#item]) };
  $::RD_HINT = 1;

  $::opt_FUNCTIONS    = '';
  $::opt_DECLARATIONS = '';
  $::opt_STRUCTS      = '';

  $::functions_output    = '';
  $::declarations_output = '';
  $::structs_output      = '';

  $::debug = 0;
  %::item  = (); # feed %items to it

=cut

  my $parser = CGrammar->new();

  my $ptree = $parser->translation_unit($text);
  if (!defined $ptree) {
    warn "undef \$ptree";
    return;
  }

=pod

  if (0) {
    print "\nDefined Functions:\n\n$::functions_output\n\n"
      if defined $::functions_output
	and $::opt_FUNCTIONS;
    print "\nDeclarations:\n\n$::declarations_output\n\n"
      if defined $::declarations_output
	and $::opt_DECLARATIONS;
    print "\nStructures:\n\n$::structs_output\n\n"
      if defined $::structs_output
	and $::opt_STRUCTS;
  }

=cut

  use Data::Dumper;
  $Data::Dumper::Terse  = 1;         # don't output names where feasible (doesn't work for my tree)
  $Data::Dumper::Indent = 1;         # mild pretty print
  $Data::Dumper::Purity = 1;

  if (0) {
    no strict 'subs';
    #print Dumper(\%::item);
    my $bar = eval($ptree);
    print ($@) if $@;
  }
  elsif (0) {
    print Dumper $ptree;
  }
  elsif (1) {
    inspect_syntax_tree($ptree);
  }

  printf "DEBUG exit, file '%s', line %d\n", __FILE__, __LINE__; exit;

} # parse_cfile

sub inspect_syntax_tree {
  my $uref = shift @_;

  print "DEBUG:  syntax tree:\n";

  my $level = 0;
  my $s = get_spaces($level);
  my $r = ref $uref;
  if (!$r) {
    print_scalar($uref, $level);
  }
  elsif ($r eq 'ARRAY') {
    print_array($uref, ++$level);
  }
  elsif ($r eq 'HASH') {
    print_hash($uref, ++$level);
  }
  else {
    print "    WARNING:   unhandled ref type '$r'\n";
    print "    scalar value: '$uref'\n";
  }
} # handle_ref

sub print_scalar {
  my $val   = shift @_;
  my $level = shift @_; # use for number of leading spaces
  my $s = get_spaces($level);
  print "${s}scalar value: $val\n";

} # print_scalar

sub print_array {
  my $aref  = shift @_;
  my $level = shift @_; # use for number of leading spaces
  my $s = get_spaces($level);
  foreach my $val (@{$aref}) {
    my $r = ref $val;
    if (!$r) {
      print_scalar($val, $level);
    }
    elsif ($r eq 'ARRAY') {
      print_array($val, ++$level);
    }
    elsif ($r eq 'HASH') {
      print_hash($val, ++$level);
    }
    else {
      print "${s}WARNING:   unhandled ref type '$r'\n";
      print "${s}scalar value: '$val'\n";
    }
  }
} # print_array

sub print_hash {
  my $href  = shift @_;
  my $level = shift @_; # use for number of leading spaces
  my $s = get_spaces($level);
  foreach my $k (keys %{$href}) {
    my $val = $href->{$k};
    my $r = ref $val;
    if (!$r) {
      print_scalar($val, $level);
    }
    elsif ($r eq 'ARRAY') {
      print_array($val, ++$level);
    }
    elsif ($r eq 'HASH') {
      print_hash($val, ++$level);
    }
    else {
      print "${s}WARNING:   unhandled ref type '$r'\n";
      print "${s}scalar value: '$val'\n";
    }
  }
} # print_array

sub get_spaces {
  my $level = shift @_;
  my $ns = $level * 3;
  my $s = sprintf "%-*.*s", $ns, $ns, ' ';
  return $s;
} # get_spaces

# mandatory true return for a Perl module
1;
