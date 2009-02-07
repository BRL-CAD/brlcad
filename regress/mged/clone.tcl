puts "*** Testing 'clone' command ***"

if {![info exists make_primitives_list]} {  
   source regression_resources.tcl
}

in_eto clone _a
clone -a 10 10 10 10 clone_eto_a.s
build_region -a 1 clone_eto_a 0 1000
cp clone_eto_a.s clone_eto_b.s
clone -b 10 100 100 100 clone_eto_b.s
build_region -a 2 clone_eto_b 0 1000
cp clone_eto_b.s clone_eto_c.s
clone -i 30 -m x 10 -n 1 clone_eto_c.s 
build_region -a 3 clone_eto_c 0 30
cp clone_eto_c.s clone_eto_d.s
clone -i 5 -p 30 30 30 -r 10 10 15 -n 20 clone_eto_d.s
build_region -a 4 clone_eto_d 0 100
cp clone_eto_d.s clone_eto_e.s
clone -i 11 -t 1 1 1 -n 20 clone_eto_e.s
build_region -a 5 clone_eto_e 0 230
cp clone_eto_e.r5 clone_a.c
attr rm clone_a.c region
clone -i 2 -p 5 0 10 -r 10 20 30 -n 20 clone_a.c
for {set i 2} {$i < 22} {incr i} {
   g clone_comb.c clone_a.c$i
}

puts "*** 'clone' testing completed ***\n"
