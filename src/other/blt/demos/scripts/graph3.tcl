proc FormatAxisLabel {graph x} {
     return "[expr int($x)]\260"
}

set configOptions [subst {
    Axis.Hide			no
    Axis.Limits			"%g"
    Axis.TickFont		{ helvetica 12 bold }
    Axis.TitleFont		{ helvetica 12 bold }
    BorderWidth			1
    Element.Pixels		1.75m
    Element.ScaleSymbols	yes
    Font			{ helvetica 23 bold }
    Legend.ActiveBorderWidth	2
    Legend.ActiveRelief		raised
    Legend.Anchor		ne
    Legend.BorderWidth		0
    Legend.Font			{ Helvetica 24 }
    Legend.Position		plotarea
    Relief			sunken
    Title			"Sine and Cosine Functions" 
    x.Command			[namespace current]::FormatAxisLabel
    x.StepSize			90 
    x.Subdivisions		0 
    x.Title			"X" 
    y.Color			purple2
    y.Loose			no
    y.Title			"Y" 
    y.rotate			90 
    y2.color			magenta3
}]

set resName [string trimleft $graph .]
foreach { option value } $configOptions {
    option add *$resName.$option $value
}

$graph configure -leftvar changed

set tcl_precision 15
set pi1_2 [expr 3.14159265358979323846/180.0]

vector create x sinX cosX -variable ""
x seq -360 360 5
sinX expr { sin(x*$pi1_2) }
cosX expr { cos(x*$pi1_2) }

$graph element create line1 \
    -label "sin(x)" \
    -fill orange \
    -color black \
    -x x \
    -y sinX  
$graph element create line2 \
    -label "cos(x)" \
    -color yellow4 \
    -fill yellow \
    -x x \
    -y cosX 

Blt_ZoomStack $graph
Blt_Crosshairs $graph
Blt_ActiveLegend $graph
Blt_ClosestPoint $graph
#Blt_PrintKey $graph

$graph marker create bitmap \
    -name bg \
    -coords "-360 -1 360 1" \
    -bitmap @bitmaps/greenback.xbm \
    -bg darkseagreen1 \
    -fg darkseagreen3 \
    -under yes \
	-rotate 45
#    -rotate 45
$graph postscript configure \
    -maxpect yes \
    -landscape yes

