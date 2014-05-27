#! /usr/local/bin/perl -w

use Parse::RecDescent;
use Data::Dumper;

my $grammar = q{

        s_expr: '(' s_expr(s) ')'               # NESTED S-EXPRS
                        { $return = $item[2] }
              | /" [^\\"]* (\\. [^\\"]*)* "/x   # STRINGS
              | /[^()\s]+/                      # ANYTHING ELSE


};

my $parser = Parse::RecDescent->new($grammar) or die;

undef $/;
my $data = <DATA>;

my $nested_array = $parser->s_expr($data);


print Data::Dumper->Dump($nested_array);

__DATA__

     (net
       (rename VDDTX "vddtx")
       (joined
	 (portRef VDDTX)
	 (portRef &2
	   (instanceRef I_45_1102680350)))
       (figure WIRE
	 (path
	   (pointList
	     (pt 6700 -2100)
	     (pt 6900 -2100)))
	 (path
	   (pointList
	     (pt 6900 -2100)
	     (pt 6900 -2500)))))
     (net
       (rename N00023 "N00023")
       (joined
	 (portRef &2
	   (instanceRef I_45_1215476478))
	 (portRef &1
	   (instanceRef I_45_1102680350))
	 (portRef &2
	   (instanceRef I_45_1215470655)))
       (figure WIRE
	 (path
	   (pointList
	     (pt 5800 -3400)
	     (pt 6900 -3400)))
	 (path
	   (pointList
	     (pt 5800 -3400)
	     (pt 5800 -3700)))
	 (path
	   (pointList
	     (pt 6900 -3100)
	     (pt 6900 -3400)))
	 (path
	   (pointList
	     (pt 8000 -3400)
	     (pt 8000 -3700)))
	 (path
	   (pointList
	     (pt 6900 -3400)
	     (pt 8000 -3400))))
       (instance
	 (rename TIE1 "TIE1")
	 (viewRef XTIE0A2Y2
	   (cellRef XTIE0A2Y2
	     (libraryRef RIPPER_LIBRARY)))
	 (transform
	   (origin
	     (pt 6900 -3400)))))

