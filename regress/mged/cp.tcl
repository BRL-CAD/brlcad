puts "*** Testing 'cp' command ***"

if {![info exists make_primitives_list]} {  
   source regression_resources.tcl
}

in_sph cp 
comb cp_comb.c u cp_sph.s
cp cp_sph.s cp_copy_sph.s
cp cp_comb.c cp_copy_comb.c

puts "*** 'cp' testing completed ***\n"
