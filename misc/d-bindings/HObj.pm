package Hobj;

use strict;
# get compile-time warnings with Class::Struct when overriding an accessor
#use warnings;

use Class::Struct;

our %tag
  = (
     'num'        => '$', # the inout sequence number (indexed from 0)
     'type'       => '$', # see 'C-BNF.txt'
     'orig_line'  => '$', # originally extracted array flattened to one line
     'first_line' => '$', # line number of first line
     'last_line'  => '$', # line number of last line

     # arrays need special accessors
     'tokens'     => '@', # original line tokenized
     'orig_array' => '@', # as extracted from the original header
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

		# arrays initially empty
		'tokens'     => [], # original line tokenized
		'orig_array' => [], # as extracted from the original header
	       );

  return $b;
} # get_new_hobj

# arrays need special accessors
sub tokens {
  my $self = shift @_;
  my $aref = shift @_;
  if (defined $aref) {
    $self->{tokens} = [@{$aref}];
  }
  else {
    return $self->{tokens};
  }
} # special accessor

sub orig_array {
  my $self = shift @_;
  my $aref = shift @_;
  if (defined $aref) {
    $self->{orig_array} = [@{$aref}];
  }
  else {
    return $self->{orig_array};
  }
} # special accessor

sub dump {
  my $self = shift @_;
  print  "Dumping an Hobj object:\n";
  printf "  num: %d\n",        $self->num();
  printf "  file: %s\n",       $self->file();
  printf "  title: %s\n",      $self->title();
  printf "  subtitle: %s\n",   $self->subtitle();
  printf "  author: %s\n",     $self->author();
  printf "  editor: %s\n",     $self->editor();
  printf "  pub: %s\n",        $self->pub();
  printf "  date: %s\n",       $self->date();
  printf "  location: %s\n",   $self->location();
  printf "  medium: %s\n",     $self->medium();
  printf "  isbn: %s\n",       $self->isbn();
  printf "  dtic_adnum: %s\n", $self->dtic_adnum();
  printf "  keywords: %s\n",   $self->keywords();

  my ($sref);

  $sref = $self->dtic_abstract();
  print "  dtic_abstract:\n";
  if (defined $sref) {
    print "    $_\n" for (@$sref);
  }

  $sref = $self->notes();
  print "  notes:\n";
  if (defined $sref) {
    print "    $_\n" for (@$sref);
  }

  $sref = $self->pubsummary();
  print "  pubsummary:\n";
  if (defined $sref) {
    print "    $_\n" for (@$sref);
  }

  $sref = $self->summary();
  print "  summary:\n";
  if (defined $sref) {
    print "    $_\n" for (@$sref);
  }

} # dump

# mandatory true return for a module
1;
