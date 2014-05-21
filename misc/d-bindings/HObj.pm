package Biblio;

# based on "Biblio.pm"

use strict;
# get compile-time warnings with Class::Struct when overriding an accessor
#use warnings;

use Class::Struct;

our %tag
  = (
     'type'       => '$', # see 'C-BNF.txt'
     'orig_line'  => '$', # originally extracted array flattened to one line

     # arrays need special accessors
     'tokens'     => '@', # original line tokenized
     'orig_array' => '@', # as extracted from the original header
    );

struct(HObj => \%tag);

sub get_new_hobj {

  # provide a default initialization
  my $b
    = HObj->new(
		'type'       => '',
		'orig_line'  => '',

		# arrays initially empty
		'tokens'     => [], # original line tokenized
		'orig_array' => [], # as extracted from the original header
	       );

  return $b;
} # get_new_biblio

# arrays need special accessors
sub summary {
  my $self = shift @_;
  my $aref = shift @_;
  if (defined $aref) {
    $self->{summary} = [@{$aref}];
  }
  else {
    return $self->{summary};
  }
} # special accessor

sub pubsummary {
  my $self = shift @_;
  my $aref = shift @_;
  if (defined $aref) {
    $self->{pubsummary} = [@{$aref}];
  }
  else {
    return $self->{pubsummary};
  }
} # special accessor

sub dtic_abstract {
  my $self = shift @_;
  my $aref = shift @_;
  if (defined $aref) {
    $self->{dtic_abstract} = [@{$aref}];
  }
  else {
    return $self->{dtic_abstract};
  }
} # special accessor

sub notes {
  my $self = shift @_;
  my $aref = shift @_;
  if (defined $aref) {
    $self->{notes} = [@{$aref}];
  }
  else {
    return $self->{notes};
  }
} # special accessor

sub dump {
  my $self = shift @_;
  print  "Dumping a biblio object:\n";
  printf "  id: %s\n",         $self->id();
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
