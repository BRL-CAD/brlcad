#!/usr/local/bin/perl -w

# CONVERT FROM ONE EXTERNAL STRUCTURE TO A
# SLIGHTLY DIFFERENT INTERNAL STRUCTURE
#
# LOCALIZED RULEVARS ARE ALWAYS EASIEST WHEN CHANGING STRUCTURES
# SEE demo_restructure_painful.pl FOR ANOTHER APPROACH THAT SHOWS WHY

use strict;
use Parse::RecDescent;
use Data::Dumper;

my $grammar = q(

	      file: <rulevar: local $file>
              file:  section(s)
                { $file }

              section:  header '{' body '}'
                { $file->{$item[1]} = $item[3] }

              header: 'Domain=' /.+/

	      body: <rulevar: local $body>
              body: line(s)
                { $body }

              line: lineA | lineB

              lineA: /[^\W_]+/ '=' /.+/
                { $body->{$item[1]} = $item[3] }

              lineB: /[^\W_]+/ '_' /[^\W_]+/ '=' /.+/
                { $body->{$item[1]}{$item[3]} = $item[5] }
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
