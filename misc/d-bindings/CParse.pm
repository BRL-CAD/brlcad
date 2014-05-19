package CParse;

use strict;
use warnings;

our @keys
  = (
     'typedef',
     'struct',
     'enum',
     'union',
     'extern',
     'void',
     '__extension__',
     '__attribute__((deprecated))',
     'int',
     'long',
    );
our %key;
@key{@keys} = ();

our @keys2
  = (
     'struct',
     'enum',
     'long',
     'int',
     'signed',
     'short',
     'unsigned',
     'void',
     '__off64_t',
     '__quad_t',
     'char',
     '__u_char',
     '__u_short',
     '__u_int',
     '__u_long',
     '__u_quad_t',
     '__fsid_t',
     '__loff_t',
     '__ino_t',
     '__dev_t',
     '__gid_t',
    );
our %key2;
@key2{@keys2} = ();

sub extract_unknown {

  my $lines_aref = shift @_; # \@lines
  my $i          = shift @_; # $i - current @lines index
  my $fp         = shift @_; # ouput file pointer

  my $nl = scalar @{$lines_aref};

  # we're at the first line of the unknown object
  # get all lines til the last
  my @olines = ();

  # track '{}', '()', '[]' levels
  my $clevel = 0; # curly braces
  my $plevel = 0; # parentheses
  my $slevel = 0; # square brackets

  my $first_index = $i;
  my $last_index;

 LINE:
  for (; $i < $nl; ++$i) {
    my $lnum = $i + 1;
    my $line = $lines_aref->[$i];
    my $len = length $line;
    for (my $j = 0; $j < $len; ++$j) {
      my $c = substr $line, $j, 1;
      if ($c eq '{') {
	++$clevel;
      }
      elsif ($c eq '}') {
	--$clevel;
      }

      elsif ($c eq '(') {
	++$plevel;
      }
      elsif ($c eq ')') {
	--$plevel;
      }

      elsif ($c eq '[') {
	++$slevel;
      }
      elsif ($c eq ']') {
	--$slevel;
      }

      elsif ($c eq ';'
	     && $clevel == 0
	     && $plevel == 0
	     && $slevel == 0
	    ) {
	push @olines, $line;
	$last_index = $i;
	last LINE;
      }
    }
    push @olines, $line;
  }

  # print good lines to output file
  print  $fp "\n";
  printf $fp "=== starting extracted code at input line %d:\n", $first_index + 1;
  print  $fp "$_" for @olines;
  printf $fp "=== ending extracted code at input line %d:\n", $last_index + 1;
  print  $fp "\n";

  return $last_index;

} # extract_unknown

=pod

sub extract_enum {
  # example:
  #   typedef enum {
  #       BU_LITTLE_ENDIAN = 1234,
  #       BU_BIG_ENDIAN = 4321,
  #       BU_PDP_ENDIAN = 3412
  #   } bu_endian_t;

  my $lines_aref = shift @_; # \@lines
  my $i          = shift @_; # $i - current @lines index
  my $fp         = shift @_; # ouput file pointer

  my $nl = scalar @{$lines_aref};

  # we're at the first line of the enum
  # get all lines til the last
  my @olines = ();
  # track '{}' levels
  my $level = 0;
  my $first_index = $i;
  my $last_index;

 LINE:
  for (; $i < $nl; ++$i) {
    my $lnum = $i + 1;
    my $line = $lines_aref->[$i];
    my $len = length $line;
    for (my $j = 0; $j < $len; ++$j) {
      my $c = substr $line, $j, 1;
      if ($c eq '{') {
	++$level;
      }
      elsif ($c eq '}') {
	--$level;
      }
      elsif ($c eq ';' && $level == 0) {
	push @olines, $line;
	$last_index = $i;
	last LINE;
      }
    }
    push @olines, $line;
  }

  # print good lines to output file
  printf $fp "=== starting extracted code at input line %d:\n", $first_index + 1;
  print  $fp "$_" for @olines;
  printf $fp "=== ending extracted code at input line %d:\n", $last_index + 1;

  return $last_index;

} # extract_enum

sub extract_struct {
  # example:
  #   typedef struct (OR struct)
  #     {
  #       unsigned long int __val[(1024 / (8 * sizeof (unsigned long int)))];
  #     } __sigset_t;

  my $lines_aref = shift @_; # \@lines
  my $i          = shift @_; # $i - current @lines index
  my $fp         = shift @_; # ouput file pointer

  my $nl = scalar @{$lines_aref};

  # we're at the first line of the struct
  # get all lines til the last
  my @olines = ();
  # track '{}' levels
  my $level = 0;
  my $first_index = $i;
  my $last_index;

 LINE:
  for (; $i < $nl; ++$i) {
    my $lnum = $i + 1;
    my $line = $lines_aref->[$i];
    my $len = length $line;
    for (my $j = 0; $j < $len; ++$j) {
      my $c = substr $line, $j, 1;
      if ($c eq '{') {
	++$level;
      }
      elsif ($c eq '}') {
	--$level;
      }
      elsif ($c eq ';' && $level == 0) {
	push @olines, $line;
	$last_index = $i;
	last LINE;
      }
    }
    push @olines, $line;
  }

  # print good lines to output file
  printf $fp "=== starting extracted code at input line %d:\n", $first_index + 1;
  print  $fp "$_" for @olines;
  printf $fp "=== ending extracted code at input line %d:\n", $last_index + 1;

  return $last_index;

} # extract_struct

=cut

# mandatory true return for a Perl module
1;
