# rtsync.tcl
# A prototype GUI for rtsync
#  -Mike Muuss, ARL, March 97.

puts "running rtsync.tcl"
option add *background #ffffff
. configure -background #ffffff

# Create main interaction widget
frame .logo_fr
frame .title_fr
frame .sunangle_fr
frame .suncolor_fr
pack .logo_fr -side top
pack .title_fr -side top
pack .sunangle_fr -side top
pack .suncolor_fr -side top

# Title, across the top
image create photo .eagle -file "/m/cad/remrt/eagleCAD.gif"
label .logo -image .eagle
label .title -text "RTSYNC, the real-time ray-tracer"
pack .logo .title -side left -in .title_fr

label .sunangle -text .sunangle
pack .sunangle -in .sunangle_fr
label .suncolor -text .suncolor
pack .suncolor -in .suncolor_fr

puts "done rtsync.tcl"
