option add *HighlightThickness		0
option add *Tile			bgTexture
option add *Button.Tile			""

image create photo bgTexture -file ./images/chalk.gif

set configOptions [subst {
    InvertXY			no
    Axis.TickFont		{ Helvetica 14 bold }
    Axis.TitleFont		{ Helvetica 12 bold }
    BorderWidth			2
    Element.Pixels		8
    Element.ScaleSymbols	true
    Element.Smooth		cubic
    Font			{ Helvetica 18 bold }
    Foreground			white
    Legend.ActiveBorderWidth	2
    Legend.ActiveRelief		raised
    Legend.Anchor		ne
    Legend.BorderWidth		0
    Legend.Font			{ Helvetica 34 }
    Legend.Foreground		orange
    #Legend.Position		plotarea
    Legend.Hide			yes
    Legend.Relief		flat
    Postscript.Preview		yes
    Relief			raised
    Shadow			{ navyblue 2 }
    Title			"Bitmap Symbols" 
    degrees.Command		[namespace current]::FormatAxisLabel
    degrees.LimitsFormat	"Deg=%g"
    degrees.Subdivisions	0 
    degrees.Title		"Degrees" 
    degrees.stepSize		90 
    temp.LimitsFormat		"Temp=%g"
    temp.Title			"Temperature"
    y.Color			purple2
    y.LimitsFormat		"Y=%g"
    y.Rotate			90 
    y.Title			"Y" 
    y.loose			no
    y2.Color			magenta3
    y2.Hide			no
    xy2.Rotate			270
    y2.Rotate			0
    y2.Title			"Y2" 
    y2.LimitsFormat		"Y2=%g"
    x2.LimitsFormat		"x2=%g"
}]

set resource [string trimleft $graph .]
foreach { option value } $configOptions {
    option add *$resource.$option $value
}

proc FormatAxisLabel {graph x} {
     format "%d%c" [expr int($x)] 0xB0
}

set max -1.0
set step 0.2

set letters { A B C D E F G H I J K L }
set count 0
for { set level 30 } { $level <= 100 } { incr level 10 } {
    set color [format "#dd0d%0.2x" [expr round($level*2.55)]]
    set pen "pen$count"
    set symbol "symbol$count"
    bitmap compose $symbol [lindex $letters $count] \
    	-font -*-helvetica-medium-r-*-*-34-*-*-*-*-*-*-*
    $graph pen create $pen \
	-color $color \
	-symbol $symbol \
	-fill "" \
	-pixels 13
    set min $max
    set max [expr $max + $step]
    lappend styles "$pen $min $max"
    incr count
}

$graph axis create temp \
    -color lightgreen \
    -title Temp  
$graph axis create degrees \
    -rotate 90
$graph xaxis use degrees

set tcl_precision 15
set pi1_2 [expr 3.14159265358979323846/180.0]

vector create w x sinX cosX radians
x seq -360.0 360.0 10.0
#x seq -360.0 -180.0 30.0
radians expr { x * $pi1_2 }
sinX expr sin(radians)
cosX expr cos(radians)
cosX dup w
vector destroy radians

vector create xh xl yh yl
set pct [expr ($cosX(max) - $cosX(min)) * 0.025]
yh expr {cosX + $pct}
yl expr {cosX - $pct}
set pct [expr ($x(max) - $x(min)) * 0.025]
xh expr {x + $pct}
xl expr {x - $pct}

$graph element create line3 \
    -color green4 \
    -fill green \
    -label "cos(x)" \
    -mapx degrees \
    -styles $styles \
    -weights w \
    -x x \
    -y cosX \
    -yhigh yh -ylow yl

$graph element create line1 \
    -color orange \
    -outline black \
    -fill orange \
    -fill yellow \
    -label "sin(x)" \
    -linewidth 3 \
    -mapx degrees \
    -pixels 6m \
    -symbol "@bitmaps/hobbes.xbm @bitmaps/hobbes_mask.xbm" \
    -x x \
    -y sinX 

Blt_ZoomStack $graph
Blt_Crosshairs $graph
#Blt_ActiveLegend $graph
Blt_ClosestPoint $graph
Blt_PrintKey $graph

