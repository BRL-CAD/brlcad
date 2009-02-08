puts "*** Testing 'make_bb' command ***"

if {![info exists make_primitives_list]} {  
   source regression_resources.tcl
}

foreach x $make_primitives_list {
      if {![string match "extrude" $x] } { in_$x make_bb "" .s}
}

in_sketch make_bb_2 "" .s
in_extrude make_bb "" .s make_bb_2_sketch.s

# Remove this once nmg gets in support
make make_bb_nmg.s nmg

foreach x $make_primitives_list {
	make_bb [format make_bb_box_%s.s $x] [format make_bb_%s.s $x]
}

make_bb make_bb_2_box_sketch.s make_bb_2_sketch.s

comb bb_prim1.c u make_bb_arb4.s u make_bb_arb5.s u make_bb_arb6.s u make_bb_arb7.s u make_bb_arb8.s u make_bb_arbn.s
make_bb comb_bb1.s bb_prim1.c
comb bb_prim2.c u make_bb_ars.s u make_bb_bot.s u make_bb_ehy.s u make_bb_ell.s u make_bb_ell1.s u make_bb_epa.s
make_bb comb_bb2.s bb_prim2.c
comb bb_prim3.c u make_bb_eto.s u make_bb_extrude.s u make_bb_grip.s u make_bb_nmg.s u make_bb_part.s u make_bb_pipe.s
make_bb comb_bb3.s bb_prim3.c
comb bb_prim4.c u make_bb_rcc.s u make_bb_rec.s u make_bb_rhc.s u make_bb_rpc.s u make_bb_rpp.s
make_bb comb_bb4.s bb_prim4.c
comb bb_prim5.c u make_bb_2_sketch.s u make_bb_sph.s u make_bb_tec.s u make_bb_tgc.s u make_bb_tor.s u make_bb_trc.s
make_bb comb_bb5.s bb_prim5.c 
comb bb_all.c u bb_prim1.c u bb_prim2.c u bb_prim3.c u bb_prim4.c u bb_prim5.c
make_bb bb_all.s bb_all.c

puts "*** 'make_bb' testing completed ***\n"
