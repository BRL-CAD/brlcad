
set X { 
    2.00000e-01 4.00000e-01 6.00000e-01 8.00000e-01 1.00000e+00 
    1.20000e+00 1.40000e+00 1.60000e+00 1.80000e+00 2.00000e+00 
    2.20000e+00 2.40000e+00 2.60000e+00 2.80000e+00 3.00000e+00 
    3.20000e+00 3.40000e+00 3.60000e+00 3.80000e+00 4.00000e+00 
    4.20000e+00 4.40000e+00 4.60000e+00 4.80000e+00 5.00000e+00 
} 

set Y1 { 
    4.07008e+01 7.95658e+01 1.16585e+02 1.51750e+02 1.85051e+02 
    2.16479e+02 2.46024e+02 2.73676e+02 2.99427e+02 3.23267e+02 
    3.45187e+02 3.65177e+02 3.83228e+02 3.99331e+02 4.13476e+02 
    4.25655e+02 4.35856e+02 4.44073e+02 4.50294e+02 4.54512e+02 
    4.56716e+02 4.57596e+02 4.58448e+02 4.59299e+02 4.60151e+02 
}

set Y2 { 
    5.14471e-00 2.09373e+01 2.84608e+01 3.40080e+01 3.75691e+01
    3.91345e+01 3.92706e+01 3.93474e+01 3.94242e+01 3.95010e+01 
    3.95778e+01 3.96545e+01 3.97313e+01 3.98081e+01 3.98849e+01 
    3.99617e+01 4.00384e+01 4.01152e+01 4.01920e+01 4.02688e+01 
    4.03455e+01 4.04223e+01 4.04990e+01 4.05758e+01 4.06526e+01 
}

set Y3 { 
    2.61825e+01 5.04696e+01 7.28517e+01 9.33192e+01 1.11863e+02 
    1.28473e+02 1.43140e+02 1.55854e+02 1.66606e+02 1.75386e+02 
    1.82185e+02 1.86994e+02 1.89802e+02 1.90683e+02 1.91047e+02 
    1.91411e+02 1.91775e+02 1.92139e+02 1.92503e+02 1.92867e+02 
    1.93231e+02 1.93595e+02 1.93958e+02 1.94322e+02 1.94686e+02 
}

set configOptions {
    Axis.TitleFont		{Times 18 bold}
    Element.Pixels		6
    Element.Smooth		catrom
    Legend.ActiveBackground	khaki2
    Legend.ActiveRelief		sunken
    Legend.Background		""
    Title			"A Simple X-Y Graph"
    activeLine.Color		yellow4
    activeLine.Fill		yellow
    background			khaki3
    line1.Color			red4
    line1.Fill			red1
    line1.Symbol		circle
    line2.Color			purple4
    line2.Fill			purple1
    line2.Symbol		arrow
    line3.Color			green4
    line3.Fill			green1
    line3.Symbol		triangle
    x.Descending		no
    x.Loose			no
    x.Title			"X Axis Label"
    y.Rotate			90
    y.Title			"Y Axis Label" 
}

set resource [string trimleft $graph .]
foreach { option value } $configOptions {
    option add *$resource.$option $value
}
$graph element create line1 -x $X -y $Y2 
$graph element create line2 -x $X -y $Y3 
$graph element create line3 -x $X -y $Y1 

Blt_ZoomStack $graph
Blt_Crosshairs $graph
Blt_ActiveLegend $graph
Blt_ClosestPoint $graph
