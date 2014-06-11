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
     'type'       => '$', # 'typedef'
     'name'       => '$', # name of typedef, if any
     'c_line'     => '$', # originally extracted array flattened to one line (in C format)
     'first_line' => '$', # line number of first line
     'last_line'  => '$', # line number of last line

     'd_line'     => '$', # 'c_line' with some D conversions

     # arrays need special accessors
     'pretty_c'   => '@', # 'c_line' split by 'gen_pretty' (in C format)
     'pretty_d'   => '@', # 'd_line' split by 'gen_pretty'    (in D format)
    );

struct(HObj => \%tag);

sub get_new_hobj {

  # provide a default initialization
  my $b
    = HObj->new(
		'num'        => '',
		'type'       => '',
		'c_line'     => '',
		'first_line' => '',
		'last_line'  => '',
		'd_line'     => '',

		# arrays initially empty
		'pretty_c'   => [],
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


sub c_line_to_d_line {
  # convert C types to D types
  my $self = shift @_;

  my $text = $self->c_line(); # the flattened C line

  foreach my $k (@G::d64map_keys) {
    my $val = $G::d64map{$k};

    # build the regex
    my @d = split(' ', $k);
    my $r = "\\b";
    $r .= shift @d;
    while (@d) {
      $r .= "\\s+";
      $r .= shift @d;
    }

    #say "DEBUG:  regex: '$r'";

    # apply it
    $text =~ s/$r/$val/g;
  }

  $self->d_line($text);

} # c_line_to_d_line

sub c_to_d {
  my $self = shift @_;

  # process pretty_d lines as a group, insert back into pretty_d lines
  my $daref = $self->pretty_d();

  # use a tmp array copy
  my @arr = @$daref;
  my $nl  = @arr;

  my $typ = $self->type;
  my $nam = $self->name;

  if ($typ eq 'typedef') {
    my $tok = shift @arr;
    die "FATAL:  \$tok ne '$typ', it's '$tok'"
      if $tok ne $typ;
    $nl  = @arr;

    if ($nl == 1) {
      # note D form 1 is the new amd preferred form
      # C: typedef int* bar;   # D form 1: alias bar = int*;   # 'bar' is the name, 'int*' is the type
      #                        # D form 2: alias int* bar;
      # C: typedef int bar[2]; # D form 1: alias bar = int[2]; # 'bar' (array of 2 ints) is the name, 'int' is the type
      #                        # D form 2: alias int[2] bar;
      my $line = shift @arr;
      $line =~ s{;}{ ; };
      my @d = split ' ', $line;
      my $tok = pop @d;
      die "FATAL:  \$tok ne ';', it's '$tok'"
	if $tok ne ';';

      $tok = pop @d;
      die "FATAL:  \$tok ne '$nam', it's '$tok'"
	if $tok ne $nam;

      # if name has an array indicator we need to add it to the D type
      # e.g., 'val[3] long'
      my $brexp = '';

      if ($tok =~ m{\A ([a-zA-Z_]+) (\[ [0-9] \]) \z}x) {
	$tok   = $1;
	$brexp = $2;
      }

      # we should have all we need to write the single D line
      $line = "alias $tok = ";
      $line .= join ' ', @d;
      $line .= $brexp
	if $brexp;
      $line .= ';';
      @arr = ($line);

      # save the work, we're done
      $self->pretty_d(\@arr);
      return;
    }
  }

  if (0 && $G::debug) {
    say "DEBUUG:  dumping \@arr:";
    print Dumper(\@arr);
    die "DEBUG exit";
  }

  for (my $i = 0; $i < $nl; ++$i) {
    my $line = $daref->[$i];

    # process the line
    # ...

    #push @arr, $line;
  }

  # save the work
  $self->pretty_d(\@arr);

} # c_to_d

sub print_final {
  # print the C one-liner and data for reference; then pretty-print
  # the D version
  my $self  = shift @_;
  my $fp    = shift @_;
  my $ppfil = shift @_; # the .i file

  my $slin = $self->first_line();
  my $elin = $self->last_line();
  my $num  = $self->num();

  my $cline = $self->c_line();

  print $fp "\n";

  say $fp "//==========================================================";
  say $fp "// the C version (object number $num):";
  say $fp "//==========================================================";
  say $fp "//   from C preprocessed file '$ppfil'"
    if (defined $ppfil);
  say $fp "//   line $slin to line $elin";
  say $fp "// $cline";
  say $fp "// the D version:";

  $self->print_pretty($fp, 'd');

  say $fp "//==========================================================";
  say $fp "// end C version (object number $num)";
  say $fp "//==========================================================";

} # print_final

sub do_all_conversions {
  my $self = shift @_;

  # convert C line to D line
  $self->c_line_to_d_line();

  # make the pretty D array
  $self->gen_pretty('d');

  # convert the D array to final D lines
  # final conversion
  $self->c_to_d();

} # do_all_conversions

sub gen_pretty {
  # put extracted object into pretty format for printing
  my $self = shift @_;
  my $ptyp = shift @_;

  my $text;
  if ($ptyp eq 'c') {
    # this is C code
    $text = $self->c_line(); # the flattened line
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
      $self->type('typedef');
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

  # extract the "name" of any typedef
  if ($self->type() eq 'typedef') {
    my $nl = @arr2;
    my $line = $arr2[$nl-1];
    $line =~ s{;}{ ; }g;
    my @d = split ' ', $line;
    my $end = pop @d;
    if ($end ne ';') {
      say "FATAL:  last typedef line: '$line'";
      die "   Line not ended with semicolon."
    }
    # the name should be the next to last token
    my $name = pop @d;
    $self->name($name);
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

  my $aref;
  if ($ptyp eq 'c') {
    # this is C code
    $aref = $self->pretty_c();
  }
  elsif ($ptyp eq 'd') {
    # this is D code
    $aref = $self->pretty_d();
  }

  foreach my $line (@$aref) {
    say $fp $line;
  }

} # print_pretty

sub dump {
  my $self = shift @_;
  say    "Dumping an Hobj object:";
  printf "  num: %d\n",        $self->num();
  printf "  type: %s\n",       $self->type();
  printf "  name: %s\n",       $self->name();
  printf "  c_line: \"%s\"\n",  $self->c_line();
  printf "  d_line: \"%s\"\n",  $self->d_line();
  printf "  first_line: %d\n", $self->first_line();
  printf "  last_line: %d\n",  $self->last_line();

  my ($sref);

=pod

  $sref = $self->pretty_c();
  say "  pretty_c:";
  if (defined $sref) {
    say "    $_" for @$sref;
  }

=cut

  $sref = $self->pretty_d();
  say "  pretty_d:";
  if (defined $sref) {
    say "    $_" for @$sref;
  }

} # dump

# mandatory true return for a module
1;
