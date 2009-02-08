puts "*** Testing 'build_region' command ***"

if {![info exists make_primitives_list]} {  
   source regression_resources.tcl
}


for {set i 1} {$i < 11} {incr i} {
	in_epa build_region .s $i
}

build_region build_region_epa 0 15
build_region -a 42 build_region_epa 5 8


puts "*** 'build_region' testing completed ***\n"
