#
#			P R J _ A D D . T C L
#
# Author -
#	Lee A. Butler
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Description -
#	helper proc to build input files for the "prj" shader
#
# Modifications -
#	Bob Parker:
#	     Added code to prompt user for input using the
#		   "more arguments needed::" hack.
#	     Replaced combination of "puts" and return with "error"
#		   so that a calling script will know an error occured.
#
# Usage: prj_add shaderfile image_filename image_width image_height
#
proc prj_add {args} {
	set prompts {{shaderfile: } {image_file: } {width: } {height: }}
	set usage "Usage:
\tprj_add \[-t\] \[-n\] \[-b\] shaderfile \[image_file\] \[width\] \[height\]
\tAppends image filename + current view parameters to shader"

	set argc [llength $args]
	if {7 < $argc} {
	    error $usage
	}

	set through 0
	set behind 0
	set antialias 1

	set n 0
	foreach opt $args {
	    switch -- $opt {
		"-t" {
		    set through 1
		}
		"-b" {
		    set behind 1
		}
		"-n" {
		    set antialias 0
		}
		default {
		    break
		}
	    }

	    incr n
	}

	set i [expr {$argc - $n}]
	switch -- $i {
	    0 {
		error "more arguments needed::[lindex $prompts $i]"
	    }
	    1 {
		error "more arguments needed::[lindex $prompts $i]"
	    }
	    2 {
		error "more arguments needed::[lindex $prompts $i]"
	    }
	    3 {
		error "more arguments needed::[lindex $prompts $i]"
	    }
	}

	set shaderfile [lindex $args $n]
	set image [lindex $args [expr {$n + 1}]]
	set width [lindex $args [expr {$n + 2}]]
	set height [lindex $args [expr {$n + 3}]]

	if ![file exists $image] {
		error "Image file $image does not exist"
	}

	if [file exists $shaderfile] {
		if [catch {open $shaderfile a} fd] {
		    error "error appending to $shaderfile: $fd"
		}
	} else {
		if [catch {open $shaderfile w} fd] {
		    error "error opening $shaderfile: $fd"
		}
	}

	puts $fd "image=\"$image\""
	puts $fd "w=$width"
	puts $fd "n=$height"
	puts $fd "through=$through"
	puts $fd "antialias=$antialias"
	puts $fd "behind=$behind"
	puts $fd "viewsize=[viewget size]"
	regsub -all { } [viewget eye] "," eye_pt
	puts $fd "eye_pt=$eye_pt"

	regsub -all { } [viewget quat] "," orientation
	puts $fd "orientation=$orientation"

	close $fd
}
