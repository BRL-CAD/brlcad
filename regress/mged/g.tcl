puts "*** Testing 'g' command ***"

if {![info exists make_primitives_list]} {  
   source regression_resources.tcl
}

in_sph comb 1
in_sph comb 2
in_sph comb 3
comb g_comb1.c u comb_sph1.s u comb_sph2.s u comb_sph3.s
g g1.c g_sph1.s g_sph2.s g_sph3.s
g g2.c g_sph1.s g_sph2.s g_comb1.c

puts "*** 'g' testing completed ***\n"
