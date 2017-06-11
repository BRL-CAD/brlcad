#!/usr/local/bin/perl -w

# CONVERT FROM ONE EXTERNAL STRUCTURE TO A
# SLIGHTLY DIFFERENT INTERNAL STRUCTURE
#
# RETURNING NESTED LISTS AND THEN MUNGING THEM IS PAINFUL
# SEE demo_restructure_easy.pl FOR A BETTER TECHNIQUE

use strict;
use Parse::RecDescent;
use Data::Dumper;

my $grammar = q(

              {
               sub consolidate {
                   my $aref = shift;
                   my @a = @{$aref};

                   my %new = ();
                   foreach my $a (@a) {
                       my %h = %{$a};
                       foreach my $k (keys %h) {
                           if (ref($h{$k})) {
                              foreach my $k2 (keys %{$h{$k}}) {
                                  $new{$k}->{$k2} = $h{$k}->{$k2};
                              }
                          } else {
                              $new{$k} = $h{$k};
                          }
                       }
                   }
                   return \%new;
               }

           }

              file:  section(s)
                { $return = consolidate($item[1]) }

              section:  header '{' body '}'
                { $return = { $item[1] => $item[3] } }

              header: 'Domain=' /.+/
                { $return = $item[2] }

              body: line(s)
                { $return = consolidate($item[1])  }

              line: lineA | lineB
                { $return = $item[1] }

              lineA: /[^\W_]+/ '=' /.+/
                { $return = { $item[1] => $item[3] } }

              lineB: /[^\W_]+/ '_' /[^\W_]+/ '=' /.+/
                { $return = { $item[1] => { $item[3] => $item[5] } } }
               );

my $parser = Parse::RecDescent->new($grammar);
my $text;
my @text = <DATA>;

foreach (@text)
{
    next if /^\#/;  # Strip comments
    $text .= $_;
}

my $f = $parser->file($text);
print Dumper ($f);

__DATA__
#
#
Domain=domain1
{
P1_Name=n1
P1_Address=host1:port1

P2_Name=n2
P2_Address=host2:port2
}

Domain=domain2
{
f1=v1
f2=v2a v2b
#comment
}
