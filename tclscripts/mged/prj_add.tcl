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
	set usage  {Usage prj_add [-t] [-n] [-b] shaderfile [image_file] [width] [height]\n}

	if {[llength $args] == 0} {
		puts $usage
		puts {Appends image filename + current view parameters to shader}
		return
	}

	set through "0"
	set behind "0"
	set antialias "1"

	set argc [llength $args]
	set n 0
	set opt [lindex $args $n]
	while { [string match "-*" $opt ] } {
		switch -- $opt {
			"-t" { set through "1" }
			"-b" { set behind "1" }
			"-n" { set antialias "0" }
			default {
				error "Unrecognized option ($opt)\n$usage"
			}
		}
		incr n
		if { $n >= $argc } break
		set opt [lindex $args $n]
	}

	set need [expr $n + 4]
	if { $need != $argc } {
		error $usage
	}

	set shaderfile [lindex $args $n]
	incr n
	set image [lindex $args $n]
	incr n
	set width [lindex $args $n]
	incr n
	set height [lindex $args $n]

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
