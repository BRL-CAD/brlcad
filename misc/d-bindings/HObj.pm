package HObj;

use v5.14;

use strict;
# get compile-time warnings with Class::Struct when overriding an accessor
#use warnings;

#use Text::Balanced 'extract_bracketed'; # extract balanced text (Conway)
use Data::Dumper; $Data::Dumper::Indent = 1;
use Class::Struct;

use F;
use R;

my $next_name_num = 1;
my %names = ();

our %tag
  = (
     'num'        => '$', # the input sequence number (indexed from 0)
     'converted'  => '$', # set true if D conversion was required and completed
     'c_line'     => '$', # originally extracted array flattened to one line (in C format)
     'first_line' => '$', # line number of first line
     'last_line'  => '$', # line number of last line
     'd_line'     => '$', # 'c_line' with some D type conversions

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
		'converted'  => '',
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

  my $line = $self->c_line(); # the flattened C line

  # warning: don't do anything to typedefs here--we handle them later
  # to ensure we don't try to redefine D types
  if ($line =~ /typedef/) {
    $self->d_line($line);
    return;
  }

  $line = c_types_to_d_types($line);

  # save the converted line
  $self->d_line($line);

  $self->converted(1)
    if $line ne $self->c_line;

} # c_line_to_d_line

sub c_to_d {
  my $self = shift @_;

  # process input pretty_d lines as a group, insert back into pretty_d
  # lines

  my $daref = $self->pretty_d();
  my $nd = @$daref;

  # put new lines in a tmp array which will become the new pretty_d
  my @arr = @$daref;

  # conversion notes:

die "FATAL: have to handle typedefs that try to refine D types such as 'wchar'";

  # note D form 1 is the new and preferred form
  # C: typedef int* bar;   # D form 1: alias bar = int*;   # 'bar' is the name, 'int*' is the type
  #                        # D form 2: alias int* bar;
  # C: typedef int bar[2]; # D form 1: alias bar = int[2]; # 'bar' (array of 2 ints) is the name, 'int' is the type
  #                        # D form 2: alias int[2] bar;

  # convert to D where necessary
  # hold parts from first (or only) and last lines
  my %fline = ();
  my %oline = ();
  my %lline = ();

 LINE:
  for (my $i = 0; $i < $nd; ++$i) {
    my $line = $daref->[$i];

    if ($i == 0) {
      # first line

      # check out types and subtypes for text before a first bracket
      # (if it is '{')

      # possibilities for the ENTIRE line:
      if ($line =~ m{$R::r_first_line}) {
	# save the captures first!
	my ($t, $type, $name, $curly) = (defined $1 ? F::trim_string($1) : '',
					 defined $2 ? F::trim_string($2) : '',
					 defined $3 ? F::trim_string($3) : '',
					 defined $4 ? F::trim_string($4) : '');

	if ($G::debug) {
	  F::debug_regex("first line match: '$line'",
			 ($t, $type, $name, $curly));
	}

	$fline{typedef} = $t ? 1 : 0;
	$fline{type}    = $type;
	$fline{name}    = $name;
	$fline{curly}   = $curly;

	# we don't save the line
	next LINE;
      }
      elsif ($line =~ m{$R::r_only_line}) {
	# save the captures first!
	my ($t, $toks, $semi) = (defined $1 ? F::trim_string($1) : '',
				 defined $2 ? F::trim_string($2) : '',
				 defined $3 ? F::trim_string($3) : '');
	if ($G::debug) {
	  F::debug_regex("only line match: '$line'",
			 ($t, $toks, $semi));
	}

	$oline{typedef} = $t ? 1 : 0;
        $oline{semi} = $semi;

	if ($toks) {
	  # the tokens
	  my @d = split ' ', $toks;

	  my $name = pop @d;
	  # if name has an array indicator we need to add it to the D
	  # type, e.g., 'val[3] long'
	  my $arrind = '';
	  if ($name =~ m{\A ([a-zA-Z_]+) (\[ [0-9] \]) \z}x) {
	    $name   = $1;
	    $arrind = $2;
	  }
	  $arrind = F::trim_string($arrind)
	    if $arrind;

	  if ($G::debug) {
	    say "DEBUG:  only line : '$line', name = '$name'";
	  }

          my $type = join ' ', @d;
	  $type = F::trim_string($type);
	  $type .= $arrind
	    if $arrind;

	  $name = F::trim_string($name)
	    if $name;
	  $type = F::trim_string($type)
	    if $type;

	  $oline{name} = $name;
	  $oline{type} = $type;
	}

	if ($G::debug) {
	  say "DEBUG:  only line : '$line', semi = '$semi'";
	}

	# we don't save the line
	next LINE;
      }
    }
    # D lines only ========================
    elsif ($i == $nd - 1) {
      # last line
      # possibilities for the ENTIRE line:
      if ($line =~ m{$R::r_last_line}) {
	# save the captures first!
	my ($curly, $toks, $semi) = (defined $1 ? F::trim_string($1) : '',
				     defined $2 ? F::trim_string($2) : '',
				     defined $3 ? F::trim_string($3) : '');
	if ($G::debug) {
	  F::debug_regex("last line match: '$line'",
			 ($curly, $toks, $semi));
	}

	$lline{curly} = $curly;
	$lline{semi}  = $semi;

	if ($toks) {
	  # the tokens, e.g., *t, z;
	  my @d = split ',', $2;
	  my @names = ();
	  foreach my $d (@d) {
	    my @s = split ' ', $d;
	    my $n = join '', @s;
	    push @names, $n
	  }
	  $lline{names} = [@names];
	}
	else {
	  $lline{names} = [];
	}

	# we don't save the line
	next LINE;
      }

      # substitute correct types for unchecked body lines
      if (exists $fline{typedef} && $fline{typedef}
	 ) {
      }
    }

    push @arr, $line;
  }

  # have we any D conversions?
  my $nF = scalar (keys %fline);
  my $nO = scalar (keys %oline);
  my $nL = scalar (keys %lline);
  my $nA = $nO || $nF || $nL;

  # requirements
  my %is_required
    = (
       fline => { type => 1, curly => 1 },
       oline => { typedef => 1, type => 1, name => 1, curly => 1 },
       lline => { curly => 1, semi => 1 },
    );

  if ($nO) {
    my $err = 0;
    my $h = 'oline';
    my $r = \%oline;
    my $o = 'one line only';
    my @k = qw(typedef type name);

    $err += check_lines($h, $r, $o, \@k, \%is_required)
      if $G::debug;
    die "FATAL" if $err;

    # this looks good so far
    @arr = ("alias $oline{name} = $oline{type};");

  }
  elsif ($nF && $nL) {
    my $err = 0;
    my ($h, $r, $o, @k);

    $h = 'fline';
    $r = \%fline;
    $o = 'first line';
    @k = qw(typedef type name curly);
    $err += check_lines($h, $r, $o, \@k, \%is_required)
      if $G::debug;

    $h = 'lline';
    $r = \%lline;
    $o = 'last line';
    @k = qw(curly names semi);
    $err += check_lines($h, $r, $o, \@k, \%is_required)
      if $G::debug;
    die "FATAL" if $err;

  }
  elsif ($nF || $nL) {
    die "FATAL:  Unexpected first or last line without the other."
  }

  if (0 && $G::debug) {
    say "DEBUUG:  dumping \@arr:";
    print Dumper(\@arr);
    die "DEBUG exit";
  }

  # save the work (but only if changed)
  if ($nA) {
    $self->pretty_d(\@arr);

    # update object status
    $self->converted(1);
  }

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

  if ($self->converted) {
    say $fp "// the D version (pretty-printed):";
  }
  else {
    say $fp "// no conversion necessary but a prettier C version:";
  }

  $self->print_pretty($fp, 'd');

  say $fp "//==========================================================";
  say $fp "// end C version (object number $num)";
  say $fp "//==========================================================";

} # print_final

sub do_all_conversions {
  my $self = shift @_;

  # convert C line to D line
  $self->c_line_to_d_line();

  # make the pretty D array and convert to D constructs where necessary
  # also convert the D array to final D lines
  $self->gen_pretty('d');

} # do_all_conversions

sub gen_pretty {
  # put extracted object into pretty format for printing
  # also get certain details and convert D as necessary

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

  my $curr_bracket = '';
  for (my $i = 0; $i < $len; ++$i) {
    my $c = substr $text, $i, 1;
    if (exists $ob{$c}) {
      $curr_bracket = $c;

      # an open bracket ends the line (and increases the level)
      ++$level;
      $line .= ' ' if ($line && lastchar($line) ne ' ');
      $line .= $c;
      push @arr, $line;

      # next line should start with an indent
      $line = F::get_spaces($level);
    }
    elsif (exists $cb{$c}) {
      $curr_bracket = $c;

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
    elsif ($c eq ',' && $curr_bracket eq '{') {
      # a comma ends the current line and starts a new line (no change in
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

  }

  # remainder of any line
  push @arr, $line
    if $line;

=pod

  say "DEBUG: dumping \@arr array BEFORE cleanup:";
  print Dumper(\@arr);

=cut

  # now tidy up a bit, and convert to D where necessary
  my @arr2 = ();

  my $nl = @arr;

 LINE:
  for (my $i = 0; $i < $nl; ++$i) {
    my $line = $arr[$i];

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
    # a little inefficient, but cleaner to do separately
    $self->c_to_d();
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
  printf "  converted: %s\n",  $self->converted() ? 'yes' : 'no';
  printf "  c_line: \"%s\"\n", $self->c_line();
  printf "  d_line: \"%s\"\n", $self->d_line();
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

sub reset_names {
  $next_name_num = 1;
  %names = ();
} # reset_names

sub get_next_name {
  my $n = sprintf "_Object_%04d", $next_name_num++;
  $names{$n} = 1;
  return $n;
} # get_next_name

sub check_lines {
  # a debugging function
  my ($h, $r, $o, $kref, $iref) = @_;

  my $err = 0;
  foreach my $k (@$kref) {
    my ($m, $e) = ('NOTE', 0);
    ($m, $e) = ('ERROR', 1)
      if exists $iref->{$h}{$k};
    if (!exists $r->{$k}) {
      say "$m: Object '$o' has no key '$k'";
      ++$err if $e;
    }
    elsif (!$r->{$k}) {
      say "$m: Object '$o' has no value for key '$k'";
      ++$err if $e;
    }
  }
    die "FATAL" if $err;

} # check_lines

sub c_types_to_d_types {
  my $line = shift @_;

  foreach my $k (@G::dmap_keys) {
    my $val = $G::dmap{$k};

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
    $line =~ s/$r/$val/g;
  }

=pod

  # some other one liners:
  $line =~ s/$R::r_null_2/typeof\(null\)/g;
  $line =~ s/$R::r_null_1/typeof\(null\)/g;

=cut

  return $line;

} # convert_c_types_to_d_types


# mandatory true return for a module
1;
