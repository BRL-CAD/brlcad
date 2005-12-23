
proc FormatXTicks { w value } {

    # Determine the element name from the value

    set index [expr round($value)]
    if { $index != $value } {
	return $value 
    }
    incr index -1

    set name [lindex { A1 B1 A2 B2 C1 D1 C2 A3 E1 } $index]
    return $name
}

source scripts/patterns.tcl

image create photo bgTexture -file ./images/chalk.gif

set configOptions {
    Axis.TickFont		-*-helvetica-medium-r-*-*-12-*-*
    Axis.TitleFont		-*-helvetica-bold-r-*-*-12-*-*
    Element.Background		white
    Element.Relief		raised
    Grid.Dashes			{ 2 4 }
    Grid.Hide			no
    Grid.MapX			""
    Legend.Font			"-*-helvetica*-bold-r-*-*-12-*-*"
    Legend.ActiveBorderWidth	2 
    Legend.ActiveRelief		raised 
    Legend.Anchor		ne 
    Legend.BorderWidth		0 
    Legend.Position		right
    TextMarker.Font		*Helvetica-Bold-R*14*
    activeBar.Foreground	black
    activeBar.Stipple		pattern1
    BarMode			stacked
    Font			-*-helvetica-bold-r-*-*-14-*-*
    Tile			bgTexture
    Title			"Comparison of Simulators"
    x.Command			FormatXTicks
    x.Title			"Simulator"
    y.Title			"Time (hrs)"
}

set resource [string trimleft $graph .]
foreach { option value } $configOptions {
    option add *$resource.$option $value
}

set visual [winfo screenvisual .] 
if { $visual != "staticgray" && $visual != "grayscale" } {
    option add *print.background	yellow
    option add *quit.background		red
    option add *quit.activeBackground	red2
}

vector X Y0 Y1 Y2 Y3 Y4

X set { 1 2 3 4 5 6 7 8 9 }
Y0 set { 
    0.729111111  0.002250000  0.09108333  0.006416667  0.026509167 
    0.007027778  0.1628611    0.06405278  0.08786667  
}
Y1 set {
    0.003120278	 0.004638889  0.01113889  0.048888889  0.001814722
    0.291388889  0.0503500    0.13876389  0.04513333 
}
Y2 set {
    11.534444444 3.879722222  4.54444444  4.460277778  2.334055556 
    1.262194444  1.8009444    4.12194444  3.24527778  
}
Y3 set {
    1.015750000  0.462888889  0.49394444  0.429166667  1.053694444
    0.466111111  1.4152500    2.17538889  2.55294444 
}
Y4 set {
    0.022018611  0.516333333  0.54772222  0.177638889  0.021703889 
    0.134305556  0.5189278    0.07957222  0.41155556  
}


#
# Element attributes:  
#
#    Label	yData	Color		Stipple Pattern
set attributes { 
    "Load"	Y2	lightblue	pattern1
    "Other"	Y4	lightpink	pattern1
    "Read In"	Y0	lightgoldenrod	pattern1
    "Setup"	Y1	lightyellow	pattern2
}
set attributes { 
    "Load"	Y2	white	 	white3		""		0
    "Solve"	Y3	cyan1		cyan3		pattern2 	1
    "zOther"	Y4	lightpink1	lightpink3 	pattern1	1
    "Read In"	Y0	lightgoldenrod1	lightgoldenrod3 pattern1	1
    "Setup"	Y1	lightyellow1	lightyellow3	pattern2	1
}
     
foreach {label yData fg bg stipple bd} $attributes {
    $graph element create $yData \
	-label $label \
	-borderwidth $bd \
	-y $yData \
	-x X \
	-fg $fg \
	-bg $bg \
	-stipple $stipple
}

Blt_ZoomStack $graph
Blt_Crosshairs $graph
Blt_ActiveLegend $graph
Blt_ClosestPoint $graph

$graph marker bind all <B2-Motion> {
    set coords [%W invtransform %x %y]
    catch { %W marker configure [%W marker get current] -coords $coords }
}

$graph marker bind all <Enter> {
    set marker [%W marker get current]
    catch { %W marker configure $marker -bg green}
}

$graph marker bind all <Leave> {
    set marker [%W marker get current]
    catch { %W marker configure $marker -bg ""}
}

