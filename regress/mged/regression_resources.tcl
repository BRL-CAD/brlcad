# A complete list of the primitives to be tested in regression
set make_primitives_list {arb4 arb5 arb6 arb7 arb8 arbn ars bot ehy ell ell1 epa eto extrude grip half hyp nmg part pipe rcc rec rhc rpc rpp sketch sph tec tgc tor trc}

# Because it will frequently be necessary to insert primitives
# with explicit value settings (to avoid tests failing due to
# changes in make behavior, for example) for each primitive type
# an explicit in_<primitive> command will be defined that will
# take an argument to be used to generate its name and (optionally)
# a number to insert and a custom extension.
proc in_arb4 {cmdname {id_number ""} {extension ".s"}} {in [format %s_arb4%s%s $cmdname $id_number $extension] arb4 3 -3 -3 3 0 -3 3 0 0 0 0 -3 }

proc in_arb5 {cmdname {id_number ""} {extension ".s"}} {in [format %s_arb5%s%s $cmdname $id_number $extension] arb5 1 0 0 1 2 0 3 2 0 3 0 0 1.5 1.5 5 }
proc in_arb6 {cmdname {id_number ""} {extension ".s"}} {in [format %s_arb6%s%s $cmdname $id_number $extension] arb6 2 -.5 -.5 2 0 -.5 2 0 0 2 -.5 0 2.5 -.3 -.5 2.5 -.3 0 }
proc in_arb7 {cmdname {id_number ""} {extension ".s"}} {in [format %s_arb7%s%s $cmdname $id_number $extension] arb7 3.25 -1.25 -0.75 3.25 -0.25 -0.75 3.25 -0.25 0.25 3.25 -1.25 -0.25 2.25 -1.25 -0.75 2.25 -0.25 -0.75 2.25 -0.25 -0.25 }
proc in_arb8 {cmdname {id_number ""} {extension ".s"}} {in [format %s_arb8%s%s $cmdname $id_number $extension] arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0 }
proc in_arbn {cmdname {id_number ""} {extension ".s"}} {in [format %s_arbn%s%s $cmdname $id_number $extension] arbn 8 1 0 0 1000 -1 0 0 1000 0 1 0 1000 0 -1 0 1000 0 0 1 1000 0 0 -1 1000 0.57735 0.57735 0.57735 1000 -0.57735 -0.57735 -0.57735 200 }
proc in_ars {cmdname {id_number ""} {extension ".s"}} {in [format %s_ars%s%s $cmdname $id_number $extension] ars 3 3 0 0 0 0 0 100 100 0 100 100 100 100 0 0 200 }
proc in_bot {cmdname {id_number ""} {extension ".s"}} {in [format %s_bot%s%s $cmdname $id_number $extension] bot 4 4 2 1 0 0 0 10 10 0 -10 10 0 0 10 10 0 1 2 1 2 3 3 2 0 0 3 1 }
proc in_ehy {cmdname {id_number ""} {extension ".s"}} {in [format %s_ehy%s%s $cmdname $id_number $extension] ehy 0 0 0 0 10 10 10 0 0 10 3 }
proc in_ell {cmdname {id_number ""} {extension ".s"}} {in [format %s_ell%s%s $cmdname $id_number $extension] ell 10 0 0 -12 0 0 0 -3 0 0 0 5 } 
proc in_ell1 {cmdname {id_number ""} {extension ".s"}} {in [format %s_ell1%s%s $cmdname $id_number $extension] ell1 3 2 8 3 -1 8 4 }
proc in_epa {cmdname {id_number ""} {extension ".s"}} {in [format %s_epa%s%s $cmdname $id_number $extension] epa 0 0 0 3 0 0 0 5 0 3 } 
proc in_eto {cmdname {id_number ""} {extension ".s"}} {in [format %s_eto%s%s $cmdname $id_number $extension] eto 0 0 0 1 1 1 10 0 2 2 1.5 }
proc in_grip {cmdname {id_number ""} {extension ".s"}} {in [format %s_grip%s%s $cmdname $id_number $extension] grip 0 0 0 3 0 0 6 }
proc in_half {cmdname {id_number ""} {extension ".s"}} {in [format %s_half%s%s $cmdname $id_number $extension] half 1 1 1 5 }
proc in_hyp {cmdname {id_number ""} {extension ".s"}} {in [format %s_hyp%s%s $cmdname $id_number $extension] hyp 0 0 0 0 0 10 3 0 0 4 .3 }
proc in_part {cmdname {id_number ""} {extension ".s"}} {in [format %s_part%s%s $cmdname $id_number $extension] part 0 0 0 0 0 16 4 2 }
proc in_pipe {cmdname {id_number ""} {extension ".s"}} {in [format %s_pipe%s%s $cmdname $id_number $extension] pipe 4 0 0 0 3 5 6 0 0 3 3 5 7 3 4 8 2 6 10 8 8 10 0 6 8 }
proc in_rcc {cmdname {id_number ""} {extension ".s"}} {in [format %s_rcc%s%s $cmdname $id_number $extension] rcc 0 0 0 3 3 30 7 }
proc in_rec {cmdname {id_number ""} {extension ".s"}} {in [format %s_rec%s%s $cmdname $id_number $extension] rec 0 0 0 3 3 10 10 0 0 0 3 0 }
proc in_rhc {cmdname {id_number ""} {extension ".s"}} {in [format %s_rhc%s%s $cmdname $id_number $extension] rhc 0 0 0 0 0 10 3 0 0 4 3 }
proc in_rpc {cmdname {id_number ""} {extension ".s"}} {in [format %s_rpc%s%s $cmdname $id_number $extension] rpc 0 0 0 0 0 4 0 1 0 3 }
proc in_rpp {cmdname {id_number ""} {extension ".s"}} {in [format %s_rpp%s%s $cmdname $id_number $extension] rpp 0 30 -3 12 -1 22 }
proc in_sph {cmdname {id_number ""} {extension ".s"}} {in [format %s_sph%s%s $cmdname $id_number $extension] sph 42 42 42 42 }
proc in_tec {cmdname {id_number ""} {extension ".s"}} {in [format %s_tec%s%s $cmdname $id_number $extension] tec 0 0 0 0 0 10 5 0 0 0 3 0 .6 }
proc in_tgc {cmdname {id_number ""} {extension ".s"}} {in [format %s_tgc%s%s $cmdname $id_number $extension] tgc 0 0 0 0 0 10 5 0 0 0 8 0 2 9 }
proc in_tor {cmdname {id_number ""} {extension ".s"}} {in [format %s_tor%s%s $cmdname $id_number $extension] tor 0 0 0 1 1 3 5 2 }
proc in_trc {cmdname {id_number ""} {extension ".s"}} {in [format %s_trc%s%s $cmdname $id_number $extension] trc 0 0 0 0 0 10 4 7 }

proc in_nmg {cmdname {id_number ""} {extension ".s"}} {}

proc in_sketch {{cmdname "extrude"} {id_number ""} {extension ""}} {put [format {%s_sketch%s%s} $cmdname $id_number $extension] sketch V {10 20 30} A {1 0 0} B {0 1 0} VL { {250 0} {500 0} {500 500} {0 500} {0 250} {250 250} {125 125} {0 125} {125 0} {200 200} } SL { { bezier D 4 P { 4 7 9 8 0 } } { line S 0 E 1 } { line S 1 E 2 } { line S 2 E 3 } { line S 3 E 4 } { carc S 6 E 5 R -1 L 0 O 0 } }}

proc in_extrude {cmdname {id_number ""} {extension ".s"} {sketch "extrude_sketch"}} { in [format %s_extrude%s%s $cmdname $id_number $extension] extrude 0 0 0 0 0 1000 10 0 0 0 10 0 $sketch} 

puts "Regression testing definitions loaded.\n"
