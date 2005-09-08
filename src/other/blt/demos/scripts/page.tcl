#!/usr/local/bin/tclsh

array set page "
    rows	2
    columns	2
    padx	0.5
    pady	0.5
    width	8.5
    height	11
    gutter	0.25
"

proc Pica { dist } {
    expr $dist * 72.0
}

# ------------------------------------------------------------------
#
# TileFiles
#
#	Tiles graph postscript files together in a pre-defined
#	grid.
#
# Arguments:
#	outFile	-- Resulting tiled PostScript output file.
#	args    -- Names of input graph PostScript files.
#
# ------------------------------------------------------------------

proc TileFiles { outFile args } {
    global page

    set row 0 
    set column 0


    set padx [Pica $page(padx)]
    set pady [Pica $page(padx)]
    set width [Pica $page(width)]
    set height [Pica $page(height)]
    set gutter [Pica $page(gutter)]

    set totalGutters [expr $gutter * ($page(columns) - 1)]
    set w [expr $width - (2 * $padx) - $totalGutters] 
    set totalGutters [expr $gutter * ($page(rows) - 1)]
    set h [expr $height - (2 * $pady) - $totalGutters]

    set cellWidth  [expr double($w) / $page(columns)]
    set cellHeight [expr double($h) / $page(rows)]
    
    set out [open $outFile "w"]

    puts $out "%!PS-Adobe-3.0 EPSF-3.0"
    puts $out "%%Pages: 1"
    puts $out "%%Title: (Graph tiler)"
    puts $out "%%DocumentNeededResources: font Helvetica Courier"
    puts $out "%%CreationDate: [clock format [clock seconds]]"
    puts $out "%%EndComments"
    
    puts $out "/showsheet { showpage } bind def"
    puts $out "/showpage { } def"
    puts $out "$padx $pady translate"

    set first {}
    foreach inFile $args {
	set in [open $inFile "r"]

	# Warning, this is assuming that the BoundingBox is in the first
	# twenty lines of the graph's PostScript.  

	for { set count 0 } { $count < 20 } { incr count } {
	    gets $in line
	    if { [string match "%%BoundingBox:*" $line] } {
		set bbox $line
		break;
	    }
	    append first "$line\n"
	    if { [eof $in] } {
		break
	    }
	}
	if { ![info exists bbox] } {
	    error "can't find \"%%BoundingBox:\" line"
	}
	set n [scan $bbox "%%%%BoundingBox: %d %d %d %d" x1 y1 x2 y2]
	if { $n != 4} {
	    error "Bad bounding box line \"$bbox\""
	}

	set rest [read $in]
	close $in

	set x [expr ($cellWidth + $gutter) * $column]
	set y [expr ($cellHeight + $gutter) * $row]

	set w [expr abs($x2 - $x1)]
	set h [expr abs($y2 - $y1)]
	
	set scaleX [expr $cellWidth / $w]
	set scaleY [expr $cellHeight / $h]
	if { $scaleX > $scaleY } {
	    set scale $scaleY
	} else {
	    set scale $scaleX
	}
	puts $out "% "
	puts $out "% Tiling \"$inFile\" at ($row,$column)"
	puts $out "% "
	puts $out "gsave"
	puts $out "$x $y translate"
	puts $out "$scale $scale scale"
	puts $out "-$x1 -$y1 translate"
	puts $out $first
	puts $out $rest
	puts $out "grestore"
	incr column
	if { $column >= $page(columns) } {
	    set column 0
	    incr row
	}
    }
    puts $out "showsheet"
    close $out
}

eval TileFiles $argv





