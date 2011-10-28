package ElNode;

use strict;
use warnings;

sub new {
  my $class = shift;
  my $self = {
	      NAME      => shift @_,
	      DEPTH     => shift @_,
	      ATTRHASH  => {},
	      VALUE     => undef,
	      CHILDLIST => [],
	      PATH      => undef,
	     };
  bless($self, $class);
  return $self;
}

sub name {
  my $self = shift @_;
  my $val  = shift @_;
  if (defined $val) {
    $self->{NAME} = $val;
  }
  else {
    return $self->{NAME};
  }
}

sub depth {
  my $self = shift @_;
  my $val  = shift @_;
  if (defined $val) {
    $self->{DEPTH} = $val;
  }
  else {
    return $self->{DEPTH};
  }
}

sub value {
  my $self = shift @_;
  my $val  = shift @_;
  if (defined $val) {
    $self->{VALUE} = $val;
  }
  else {
    return $self->{VALUE};
  }
}

sub path {
  my $self = shift @_;
  my $val  = shift @_;
  if (defined $val) {
    $self->{PATH} = $val;
  }
  else {
    return $self->{PATH};
  }
}

sub add_child {
  my $self  = shift @_;
  my $child = shift @_;
  push @{$self->{CHILDLIST}}, $child;
}

sub childlist {
  my $self = shift @_;
  return @{$self->{CHILDLIST}};
}

sub get_attr_value {
  my $self  = shift @_;
  my $aname = shift @_;

  return $self->{ATTRHASH}{$aname}
    if exists $self->{ATTRHASH}{$aname};

  return undef;
}

sub add_attr {
  my $self   = shift @_;
  my $aname  = shift @_;
  my $avalue = shift @_;
  $self->{ATTRHASH}{$aname} = $avalue;
}

sub attrlist {
  my $self = shift @_;
  return (sort keys %{$self->{ATTRLIST}});
}

my $space = 3;
sub dump {
  my $self = shift @_;
  my $d    = $self->depth();
  my $name = $self->name();
  my $dd = $d;
  $d *= $space;
  printf "%-*.*s depth: $dd node name: $name\n", $d, $d, " ";

  # print the path
  printf "%-*.*s    path: '%s'\n", $d, $d, " ", $self->path();

  # print attrs
  my @attrs = $self->attrlist();
  if (@attrs) {
    printf "%-*.*s    attributes:\n", $d, $d, " ";
    foreach my $aname (@attrs) {
      $self->dump_attr($aname, $self->{ATTRHASH}{$aname});
    }
  }
  my $val = $self->value();
  if (defined $val) {
    printf "%-*.*s   value: '$val'\n", $d, $d, " ";
  }

  # print children
  my @children = $self->childlist();
  if (@children) {
    printf "%-*.*s    children:\n", $d, $d, " ";
    foreach my $child (@children) {
      $child->dump();
    }
  }

}

sub dump_attr {
  my $self = shift @_;
  my $tag  = shift @_;
  my $val  = shift @_;

  my $d    = $self->depth();

  $d *= $space;
  printf "%-*.*s      '$tag' => '$val'\n", $d, $d, " ";
}

## package end
1;
