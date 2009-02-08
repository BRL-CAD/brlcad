puts "*** Testing 'arb' command ***"

if {![info exists make_primitives_list]} {  
   source regression_resources.tcl
}

arb arb_test1.s 30 3
arb arb_test2.s 55 20

puts "*** 'arb' testing completed ***\n"
