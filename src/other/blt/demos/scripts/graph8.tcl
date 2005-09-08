
set X { 
    2.00000e-01 4.00000e-01 6.00000e-01 8.00000e-01 1.00000e+00 
    1.20000e+00 1.40000e+00 1.60000e+00 1.80000e+00 2.00000e+00 
    2.20000e+00 2.40000e+00 2.60000e+00 2.80000e+00 3.00000e+00 
    3.20000e+00 3.40000e+00 3.60000e+00 3.80000e+00 4.00000e+00 
    4.20000e+00 4.40000e+00 4.60000e+00 4.80000e+00 5.00000e+00 
} 

set Y1 { 
    1.14471e+01 2.09373e+01 2.84608e+01 3.40080e+01 3.75691e+01 
    3.91345e+01 3.92706e+01 3.93474e+01 3.94242e+01 3.95010e+01 
    3.95778e+01 3.96545e+01 3.97313e+01 3.98081e+01 3.98849e+01 
    3.99617e+01 4.00384e+01 4.01152e+01 4.01920e+01 4.02688e+01 
    4.03455e+01 4.04223e+01 4.04990e+01 4.05758e+01 4.06526e+01 
}

set Y2 { 
    2.61825e+01 5.04696e+01 7.28517e+01 9.33192e+01 1.11863e+02 
    1.28473e+02 1.43140e+02 1.55854e+02 1.66606e+02 1.75386e+02 
    1.82185e+02 1.86994e+02 1.89802e+02 1.90683e+02 1.91047e+02 
    1.91411e+02 1.91775e+02 1.92139e+02 1.92503e+02 1.92867e+02 
    1.93231e+02 1.93595e+02 1.93958e+02 1.94322e+02 1.94686e+02 
}

set Y3 { 
    4.07008e+01 7.95658e+01 1.16585e+02 1.51750e+02 1.85051e+02 
    2.16479e+02 2.46024e+02 2.73676e+02 2.99427e+02 3.23267e+02 
    3.45187e+02 3.65177e+02 3.83228e+02 3.99331e+02 4.13476e+02 
    4.25655e+02 4.35856e+02 4.44073e+02 4.50294e+02 4.54512e+02 
    4.56716e+02 4.57596e+02 4.58448e+02 4.59299e+02 4.60151e+02 
}


proc FormatLabel { w value } {
    return $value
}

#option add *Graph.aspect		1.25
option add *Graph.title			"A Simple X-Y Graph"
option add *Graph.x.loose		yes
option add *Graph.x.title		"X Axis Label"
option add *Graph.y.title		"Y Axis Label" 
option add *Graph.y.rotate		90
option add *Graph.y.logScale		yes
option add *Graph.y.loose		no
option add *Graph.Axis.titleFont        {Times 18 bold}

option add *Legend.activeRelief		sunken
option add *Legend.background		""
option add *Legend.activeBackground	khaki2
option add *Graph.background		brown
option add *Element.xData		$X 
option add *activeLine.Color		yellow4
option add *activeLine.Fill		yellow
option add *Element.smooth		natural
option add *Element.pixels		6
option add *Element.scaleSymbols	yes

option add *Graph.line1.symbol		circle
option add *Graph.line1.color		red4
option add *Graph.line1.fill		red1

option add *Graph.line2.symbol		square
option add *Graph.line2.color		purple4
option add *Graph.line2.fill		purple1

option add *Graph.line3.symbol		triangle
option add *Graph.line3.color		green4
option add *Graph.line3.fill		green1

$graph configure \
    -width 4i \
    -height 5i
$graph element create line1 \
    -ydata $Y2 
$graph element create line2 \
    -ydata $Y3 
$graph element create line3 \
    -ydata $Y1 

Blt_ZoomStack $graph
Blt_Crosshairs $graph
Blt_ActiveLegend $graph
Blt_ClosestPoint $graph
