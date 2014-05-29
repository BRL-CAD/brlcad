package ParsePPCHeader;

# parse a pre-processed C header (with gcc -E)

use strict;
use warnings;

use CGrammar;  # <== an auto-generated file
use CGrammar2; # <== an auto-generated file

# for debugging:
my $COMP = 0;

sub parse_cfile_pure_autotree {

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

  my $parser = CGrammar->new();

  my $ptree = $parser->translation_unit($text);
  if (!defined $ptree) {
    warn "undef \$ptree";
    return;
  }

  use Data::Dumper;
  $Data::Dumper::Terse   = 1;         # don't output names where feasible (doesn't work for my tree)
  $Data::Dumper::Indent  = 1;         # mild pretty print
  $Data::Dumper::Purity  = 1;
  $Data::Dumper::Deparse = 1;

  if (0) {
    print Dumper $ptree;
  }
  elsif (1) {
    inspect_PRD_syntax_pure_autotree($ptree);
  }

  printf "DEBUG exit, file '%s', line %d\n", __FILE__, __LINE__; exit;

} # parse_cfile_pure_autotree

sub inspect_PRD_syntax_pure_autotree {
  my $tree = shift @_;
  print "DEBUG:  syntax tree:\n";

  die "what ??" if !exists $tree->{'__RULE__'};
  die "what ??" if !exists $tree->{'external_declaration(s)'};

  my @extdecl_hrefs = @{$tree->{'external_declaration(s)'}};
  foreach my $href (@extdecl_hrefs) {
    die "what ??" if !exists $href->{'__RULE__'};
    #die "what ??" if !exists $href->{'external_declaration'};
  }

  print "so far, so good\n";

=pod

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
  elsif ($COMP && $uref =~ /=/) {
    print_scalar($uref, $level);
  }
  else {
    warn_ref($r, $uref, $s, __FILE__, __LINE__);
  }

=cut

} # inspect_PRD_syntax_pure_autotree

sub get_spaces {
  my $level = shift @_;
  my $ns = $level * 3;
  my $s = sprintf "%-*.*s", $ns, $ns, ' ';
  return $s;
} # get_spaces

# mandatory true return for a Perl module
1;








########## EOF ###########
__END__
# currently unneeded routines

=pod

sub inspect_syntax_tree2 {
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
  elsif ($COMP && $uref =~ /=/) {
    print_scalar($uref, $level);
  }
  else {
    warn_ref($r, $uref, $s, __FILE__, __LINE__);
  }
} # inspect_syntax_tree2

sub warn_ref {
  my ($r, $uref, $s, $fil, $linenum) = @_;
  print "${s}WARNING:   unhandled ref type '$r'\n";
  print "${s}scalar value: '$uref'\n";
  print "${s}file '$fil', line $linenum\n";
} # warn_ref

sub print_scalar {
  my $val = shift @_;
  my $level = shift @_; # use for number of leading spaces

=pod

  # note that a scalar may be a disguised ref, e.g.,
  #   translation_unit=HASH(0x1347858)
  my $val2 = undef;
  my $idx = index $val, '=';
  if ($idx >= 0) {
    $val2 = substr $val, $idx+1;
    $val = substr $val, 0, $idx+1; # save the '=' for output
  }

=cut

  my $s = get_spaces($level);
  print "${s}scalar value: $val\n";

=pod

  if (defined $val2) {
    my $r = ref $val2;
    if (!$r) {
      print_scalar($val2, $level);
    }
    elsif ($r eq 'ARRAY') {
      print_array($val2, ++$level);
    }
    elsif ($r eq 'HASH') {
      print_hash($val2, ++$level);
    }
    elsif ($COMP && $val2 =~ /=/) {
      print_scalar($val2, $level);
    }
    else {
      warn_ref($r, $val2, $s, __FILE__, __LINE__);
    }
  }

=cut

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
    elsif ($COMP && $val =~ /=/) {
      print_scalar($val, $level);
    }
    else {
      warn_ref($r, $val, $s, __FILE__, __LINE__);
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
    elsif ($COMP && $val =~ /=/) {
      print_scalar($val, $level);
    }
    else {
      warn_ref($r, $val, $s, __FILE__, __LINE__);
    }
  }
} # print_array

=cut
