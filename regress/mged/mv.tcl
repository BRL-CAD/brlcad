puts "*** Testing 'mv' command ***"

if {![info exists make_primitives_list]} {  
   source regression_resources.tcl
}

in_sph mv 1
in_sph mv 2
comb mv_comb.c u mv_sph2.s
mv mv_sph.s moved_sph.s
mv mv_comb.c moved_comb.c

puts "*** 'mv' testing completed ***\n"
