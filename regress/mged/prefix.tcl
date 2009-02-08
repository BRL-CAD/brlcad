puts "*** Testing 'prefix' command ***"

if {![info exists make_primitives_list]} {  
   source regression_resources.tcl
}

in_sph prefix 1
in_sph prefix 2
comb prefix_comb.c u prefix_sph1.s u prefix_sph2.s
prefix has_ prefix_sph1.s
prefix has_ prefix_sph2.s
prefix has_ prefix_comb.c

puts "*** 'prefix' testing completed ***\n"
