package HObj;

use v5.14;

use strict;
# get compile-time warnings with Class::Struct when overriding an accessor
#use warnings;

#use Text::Balanced 'extract_bracketed'; # extract balanced text (Conway)
use Data::Dumper; $Data::Dumper::Indent = 1;
use Class::Struct;

use F;

our %tag
  = (
     'num'        => '$', # the inout sequence number (indexed from 0)
     'type'       => '$', # see 'C-BNF.txt'
     'orig_line'  => '$', # originally extracted array flattened to one line (in C format)
     'first_line' => '$', # line number of first line
     'last_line'  => '$', # line number of last line

     'd_line'     => '$', # 'orig_line' with some D conversions

     # arrays need special accessors
     'pretty_c'   => '@', # 'orig_line' split by 'gen_pretty' (in C format)
     'pretty_d'   => '@', # 'd_line' split by 'gen_pretty'    (in D format)
    );

struct(HObj => \%tag);

sub get_new_hobj {

  # provide a default initialization
  my $b
    = HObj->new(
		'num'        => '',
		'type'       => '',
		'orig_line'  => '',
		'first_line' => '',
		'last_line'  => '',
		'd_line'     => '',

		# arrays initially empty
		'pretty_c'     => [],
		'pretty_d'   => [],
	       );

  return $b;
} # get_new_hobj

# arrays need special accessors
sub pretty_c {
  my $self = shift @_;
  my $aref = shift @_;
  if (defined $aref) {
    $self->{pretty_c} = [@{$aref}];
  }
  else {
    return $self->{pretty_c}; # an array ref
  }
} # special accessor

sub pretty_d {
  my $self = shift @_;
  my $aref = shift @_;
  if (defined $aref) {
    $self->{pretty_d} = [@{$aref}];
  }
  else {
    return $self->{pretty_d}; # an array ref
  }
} # special accessor

# open brackets
my %ob
  = (
     #'(' => 0,
     #'[' => 1,
     '{' => 2,
     '<' => 3,
    );
# close brackets
my %cb
  = (
     #')' => 0,
     #']' => 1,
     '}' => 2,
     '>' => 3,
     #';' => 3,
    );

sub lastchar {
  # returns last character of a string
  my $s = shift @_;
  my $len = length $s;
  return '' if !$len;

  my $c = substr $s, $len-1, 1;
  return $c;

} # lastchar

sub gen_pretty {
  # put extracted object into pretty format for printing
  my $self = shift @_;
  my $ptyp = shift @_;

  my $text;
  if ($ptyp eq 'c') {
    # this is C code
    $text = $self->orig_line(); # the flattened line
  }
  elsif ($ptyp eq 'd') {
    # this is D code
    $text = $self->d_line(); # the flattened line
  }

  # use a tmp array and a line
  my @arr = ();
  my $len = length $text;
  my $line = '';
  my $level = 0;
  for (my $i = 0; $i < $len; ++$i) {
    my $c = substr $text, $i, 1;
    if (exists $ob{$c}) {
      # an open bracket ends the line (and increases the level)
      ++$level;
      $line .= ' ' if ($line && lastchar($line) ne ' ');
      $line .= $c;
      push @arr, $line;

      # next line should start with an indent
      $line = F::get_spaces($level);
    }
    elsif (exists $cb{$c}) {
      # a close bracket ends the current line and starts a new line
      # (and decreases the level)
      --$level;

      push @arr, $line if $line;

      $line = $level ? F::get_spaces($level) : '';
      $line .= $c;
    }
    elsif ($c eq ';') {
      # a semi the current line and starts a new line (no change in
      # level)
      $line .= $c;
      push @arr, $line;
      $line = $level ? F::get_spaces($level) : '';
    }
    else {
      if ($c eq ' ') {
	$line .= $c if ($line && lastchar($line) ne ' ');
      }
      else {
	$line .= $c;
      }
    }

    # check for typedef
    if ($line =~ m{\A typedef \s* \z}x) {
      push @arr, 'typedef';
      $line = '';
    }
  }
  # remainder of any line
  push @arr, $line
    if $line;

=pod

  say "DEBUG: dumping \@arr array BEFORE cleanup:";
  print Dumper(\@arr);

=cut

  # now tidy up a bit
  my @arr2 = ();
  foreach my $line (@arr) {
    # remove spaces around square brackets
    $line =~ s{\s* \[}{\[}xg;
    $line =~ s{\[ \s*}{\[}xg;
    $line =~ s{\s* \]}{\]}xg;
    $line =~ s{\] \s*}{\]}xg;

    # remove spaces before semicolons
    $line =~ s{\s* ;}{;}xg;

    # remove spaces before open parens
    $line =~ s{\s* \(}{\(}xg;
    # remove spaces before close parens
    $line =~ s{\s* \)}{\)}xg;
    # remove spaces after open parens
    $line =~ s{\( \s*}{\(}xg;

    push @arr2, $line
      if ($line =~ /\S/);
  }

  # and save the mess
  if ($ptyp eq 'c') {
    # this is C code
    $self->pretty_c(\@arr2);
  }
  elsif ($ptyp eq 'd') {
    # this is D code
    $self->pretty_d(\@arr2);
  }

=pod

  say "DEBUG: dumping \@arr array AFTER cleanup:";
  print Dumper(\@arr);

=cut

} # gen_pretty

sub print_pretty {
  my $self = shift @_;
  my $fp   = shift @_;
  my $ptyp = shift @_;

  if ($ptyp eq 'c') {
    # this is C code

    foreach my $line (@{$self->pretty_c()}) {
      print $fp "$line\n";
    }
  }
  elsif ($ptyp eq 'd') {
    # this is D code

    foreach my $line (@{$self->pretty_d()}) {
      print $fp "$line\n";
    }
  }

} # print_pretty

sub dump {
  my $self = shift @_;
  say    "Dumping an Hobj object:";
  printf "  num: %d\n",        $self->num();
  printf "  type: %s\n",       $self->type();
  printf "  orig_line: \"%s\"\n",  $self->orig_line();
  printf "  d_line: \"%s\"\n",  $self->d_line();
  printf "  first_line: %d\n", $self->first_line();
  printf "  last_line: %d\n",  $self->last_line();

  my ($sref);

  $sref = $self->pretty_c();
  say "  pretty_c:";
  if (defined $sref) {
    say "    $_" for @$sref;
  }

  $sref = $self->pretty_d();
  say "  pretty_d:";
  if (defined $sref) {
    say "    $_" for @$sref;
  }

} # dump

# mandatory true return for a module
1;
