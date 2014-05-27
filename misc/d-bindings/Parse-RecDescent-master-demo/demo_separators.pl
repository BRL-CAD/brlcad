#! /usr/local/bin/perl -ws

use Parse::RecDescent;

undef $::RD_WARN;

my $parse = Parse::RecDescent->new(<<'EOGRAMMAR');

{use Tie::Hash; }

line: <rulevar: local %max; tie %max, Tie::StdHash'; %max = (count=>0) >

line: seplist[sep=>',']
    | seplist[sep=>':']
    | seplist[sep=>" "]
    | { $max{item} }

seplist: <skip:"">
	 <leftop: /[^$arg{sep}]*/ "$arg{sep}" /[^$arg{sep}]*/>
          { $max{count} = @{$max{item} = $item[2]}
                if @{$item[2]} > $max{count};
          }
	 <reject>

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
