#! /usr/local/bin/perl -ws

use Parse::RecDescent;

$RD_WARN = undef;
$RD_TRACE=1;

my $parse = Parse::RecDescent->new(<<'EOGRAMMAR');

line: <autoscore: @{$item[1]}>
line: seplist[sep=>',']
    | seplist[sep=>':']
    | seplist[sep=>" "]

seplist: <skip:""> <leftop: /[^$arg{sep}]*/ "$arg{sep}" /[^$arg{sep}]*/>

EOGRAMMAR

while (<DATA>)
{
    chomp;
    my $res = $parse->line($_);
    print '[', join('][', @$res), "]\n";
}

__DATA__
c,o,m,m,a,s,e,p,a,r,a,t,e,d
c:o:l:o:n:s:e:p:a:r:a:t:e:d
s p a c e s e p a r a t e d
m u:l t i,s:ep ar:a,ted
m u:l,t i,s:ep ar:a,ted
m:u:l,t i,s:ep ar:a,ted
