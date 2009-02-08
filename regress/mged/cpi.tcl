puts "*** Testing 'cpi' command ***"

if {![info exists make_primitives_list]} {  
   source regression_resources.tcl
}

in cpi-base-cyl.s rcc 1 2 3 10 10 10 3.5
cpi cpi-base-cyl.s cpi-cyl.s
accept

puts "*** 'cpi' testing completed ***\n"
