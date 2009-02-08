puts "*** Testing 'make' command ***"

if {![info exists make_primitives_list]} {  
   source regression_resources.tcl
}

foreach x $make_primitives_list {
	make [format make_%s.s $x] $x
	make -s 42 [format make_sized_%s.s $x] $x
}

set make_regression_run 1

puts "*** 'make' testing completed ***\n"
