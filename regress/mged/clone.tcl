puts "*** Testing 'clone' command ***"

if {![info exists make_primitives_list]} {  
   source regression_resources.tcl
}

in_eto clone _a
clone -a 10 10 10 10 clone_eto_a.s
g clone1.c clone_eto_a.s\*
cp clone_eto_a.s clone_eto_b.s
clone -b 10 100 100 100 clone_eto_b.s
g clone2.c clone_eto_b.s*
cp clone_eto_b.s clone_eto_c.s
clone -i 30 -m x 10 -n 1 clone_eto_c.s 
g clone3.c clone_eto_c.s*
cp clone_eto_c.s clone_eto_d.s
clone -i 5 -p 30 30 30 -r 10 10 15 -n 20 clone_eto_d.s
g clone4.c clone_eto_d.s*
cp clone_eto_d.s clone_eto_e.s
clone -i 11 -t 1 1 1 -n 20 clone_eto_e.s
g clone5.c clone_eto_e.s*
cp clone5.c clone_a.c
clone -i 2 -p 5 0 10 -r 10 20 30 -n 20 clone_a.c
g comb_clone.c clone_a.c*


puts "*** 'clone' testing completed ***\n"
