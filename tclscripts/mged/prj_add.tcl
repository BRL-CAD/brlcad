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
#
# Usage: prj_add shaderfile image_filename image_width image_height
#
proc prj_add {args} {
	set prompts { {shaderfile: } {image_file: } {width: } {height: }}

	if {[llength $args] == 0} {
		puts {Usage prj_add shaderfile [image_file] [width] [height]}
		puts {Appends image filename + current view parameters to shader}
		return
	}

	set n [llength $args]
	while {$n < 4} {
		puts -nonewline "[lindex $prompts $n]"
		flush stdout
		gets stdin foo
		lappend args $foo
		set n [expr $n + 1]
	}

	set shaderfile [lindex $args 0]
	set image [lindex $args 1]
	set width [lindex $args 2]
	set height [lindex $args 3]

	if ![file exists $image] {
		puts "Image file $image does not exist"
		return
	}

	if [file exists $shaderfile] {
		if [catch {open $shaderfile a} fd] {
			puts "error appending to $shaderfile: $fd"
			return
		}
	} else {
		if [catch {open $shaderfile w} fd] {
			puts "error opening $shaderfile: $fd"
			return
		}
	}

	puts $fd "image=\"$image\""
	puts $fd "w=$width"
	puts $fd "n=$height"
	puts $fd "viewsize=[viewget size]"
	regsub -all { } [viewget eye] "," eye_pt
	puts $fd "eye_pt=$eye_pt"

	regsub -all { } [viewget quat] "," orientation
	puts $fd "orientation=$orientation"

	close $fd
}
