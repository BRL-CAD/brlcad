puts "*** Testing 'in' command ***"

# Load regression testing common definitions if
# not already loaded
if {![info exists make_primitives_list]} {
   source regression_resources.tcl
}

# Now, run the pre-defined in routines for all primitives.
# Avoid making extrude in the list iteration, as it needs 
# a specific pre-existing sketch first
foreach x $make_primitives_list {
     if {![string match "extrude" $x] } { in_$x in }
}

# Special commands to handle the extrude primitive
in_sketch
in_extrude in
puts "*** 'in' testing completed ***\n"
