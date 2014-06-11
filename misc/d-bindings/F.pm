package F;

# shared functions

use v5.14;

sub get_spaces {
  my $level = shift @_;
  my $ns = $level * 3;
  my $s = sprintf "%-*.*s", $ns, $ns, ' ';
  return $s;
} # get_spaces

sub debug_regex {
  my $msg  = shift @_;
  my @arr  = @_;

  say "DEBUG: $msg";

  my $nl = @arr;
  for (my $i = 0; $i < $nl; ++$i) {
    my $c = $arr[$i];
    my $n = $i + 1;
    if (defined $c) {
      say "  \$$n = '$c'";
    }
    else {
      say "  \$$n = 'undef'";
    }
  }

} # debug_regex

# mandatory true return for a Perl module
1;
