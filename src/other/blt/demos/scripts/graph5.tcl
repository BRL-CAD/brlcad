
set configOptions {
    Element.LineWidth		0
    Element.Pixels		0.7c
    Element.ScaleSymbols	true
    Font			{ Courier 18 bold}
    Height			4i
    Legend.ActiveRelief		raised
    Legend.Font			{ Courier 14 } 
    Legend.padY			0
    Title			"Element Symbol Types"
    Width			5i
} 
set resName [string trimleft $graph .]
foreach { option value } $configOptions {
    option add *$resName.$option $value
}

vector xValues
xValues set { 
    0.0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 
}

for { set i 0 } { $i < 10 } { incr i } {
    set vecName "y${i}"
    vector ${vecName}(10)
    $vecName variable x
    set x(:) [expr $i*50.0+10.0]
}

set attributes {
    none	"None"		red	red4		y0
    circle	"Circle"	yellow	yellow4		y2
    cross	"Cross"		cyan	cyan4		y6
    diamond	"Diamond"	green	green4		y3
    plus	"Plus"		magenta	magenta4	y9
    splus	"Splus"		Purple	purple4		y7
    scross	"Scross"	red	red4		y8
    square	"Square"	orange	orange4		y1
    triangle	"Triangle"	blue	blue4		y4
    "@bitmaps/hobbes.xbm @bitmaps/hobbes_mask.xbm"
		"Bitmap"	yellow	black		y5
}

set count 0
foreach { symbol label fill color yVec } $attributes {
    $graph element create line${count} \
	-label $label \
	-symbol $symbol \
	-color $color \
	-fill $fill \
	-x xValues \
	-y $yVec 
    incr count
}
$graph element configure line0 \
    -dashes  { 2 4 2 } \
    -linewidth 2

Blt_ZoomStack $graph
Blt_Crosshairs $graph
Blt_ActiveLegend $graph
Blt_ClosestPoint $graph
Blt_PrintKey $graph

