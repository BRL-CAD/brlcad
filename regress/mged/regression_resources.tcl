# This file needs to be run only once per series of commands, but
# with command tests broken into individual files each file might
# need to run it.  To avoid the problem, the entire file contents
# will be wrapped in a conditional check - this means the file can
# be safely sourced multiple times without re-executing the file
# contents

if {![info exists make_primitives_list]} {

  # A complete list of the primitives to be tested in regression
  set make_primitives_list {arb4 arb5 arb6 arb7 arb8 arbn ars bot ehy ell ell1 epa eto extrude grip half hyp nmg part pipe rcc rec rhc rpc rpp sketch sph tec tgc tor trc}

  # A convenience routine is defined for cases where one wants to
  # make an instance of each primitive.

  proc make_all_prims {cmdname {size "-1"} {extratext1 ""} {extratext2 ""} } {
     global make_primitives_list
     foreach x $make_primitives_list {
        if {$size == -1} {
	  make [format %s_%s%s.s%s $cmdname $extratext1 $x $extratext2] $x
	} else {
	  make -s $size [format %s_%s%s.s%s $cmdname $extratext1 $x $extratext2] $x
	}
     }
  }

  # Because it will frequently be necessary to insert primitives
  # with explicit value settings (to avoid tests failing due to
  # changes in make behavior, for example) for each primitive type
  # an explicit in_<primitive> command will be defined that will
  # take an argument to be used to generate its name and (optionally)
  # a middle string to insert and a custom extension.
  proc in_arb4 {cmdname {mid_str ""} {extension ".s"}} {in [format %s_arb4%s%s $cmdname $mid_str $extension] arb4 3 -3 -3 3 0 -3 3 0 0 0 0 -3 }

  proc in_arb5 {cmdname {mid_str ""} {extension ".s"}} {in [format %s_arb5%s%s $cmdname $mid_str $extension] arb5 1 0 0 1 2 0 3 2 0 3 0 0 1.5 1.5 5 }
  proc in_arb6 {cmdname {mid_str ""} {extension ".s"}} {in [format %s_arb6%s%s $cmdname $mid_str $extension] arb6 2 -.5 -.5 2 0 -.5 2 0 0 2 -.5 0 2.5 -.3 -.5 2.5 -.3 0 }
  proc in_arb7 {cmdname {mid_str ""} {extension ".s"}} {in [format %s_arb7%s%s $cmdname $mid_str $extension] arb7 3.25 -1.25 -0.75 3.25 -0.25 -0.75 3.25 -0.25 0.25 3.25 -1.25 -0.25 2.25 -1.25 -0.75 2.25 -0.25 -0.75 2.25 -0.25 -0.25 }
  proc in_arb8 {cmdname {mid_str ""} {extension ".s"}} {in [format %s_arb8%s%s $cmdname $mid_str $extension] arb8 10 -9 -8 10 -1 -8 10 -1 0 10 -9 0 3 -9 -8 3 -1 -8 3 -1 0 3 -9 0 }
  proc in_arbn {cmdname {mid_str ""} {extension ".s"}} {in [format %s_arbn%s%s $cmdname $mid_str $extension] arbn 8 1 0 0 1000 -1 0 0 1000 0 1 0 1000 0 -1 0 1000 0 0 1 1000 0 0 -1 1000 0.57735 0.57735 0.57735 1000 -0.57735 -0.57735 -0.57735 200 }
  proc in_ars {cmdname {mid_str ""} {extension ".s"}} {in [format %s_ars%s%s $cmdname $mid_str $extension] ars 3 3 0 0 0 0 0 100 100 0 100 100 100 100 0 0 200 }
  proc in_bot {cmdname {mid_str ""} {extension ".s"}} {in [format %s_bot%s%s $cmdname $mid_str $extension] bot 4 4 2 1 0 0 0 10 10 0 -10 10 0 0 10 10 0 1 2 1 2 3 3 2 0 0 3 1 }
  proc in_ehy {cmdname {mid_str ""} {extension ".s"}} {in [format %s_ehy%s%s $cmdname $mid_str $extension] ehy 0 0 0 0 10 10 10 0 0 10 3 }
  proc in_ell {cmdname {mid_str ""} {extension ".s"}} {in [format %s_ell%s%s $cmdname $mid_str $extension] ell 10 0 0 -12 0 0 0 -3 0 0 0 5 } 
  proc in_ell1 {cmdname {mid_str ""} {extension ".s"}} {in [format %s_ell1%s%s $cmdname $mid_str $extension] ell1 3 2 8 3 -1 8 4 }
  proc in_epa {cmdname {mid_str ""} {extension ".s"}} {in [format %s_epa%s%s $cmdname $mid_str $extension] epa 0 0 0 3 0 0 0 5 0 3 } 
  proc in_eto {cmdname {mid_str ""} {extension ".s"}} {in [format %s_eto%s%s $cmdname $mid_str $extension] eto 0 0 0 1 1 1 10 0 2 2 1.5 }
  proc in_grip {cmdname {mid_str ""} {extension ".s"}} {in [format %s_grip%s%s $cmdname $mid_str $extension] grip 0 0 0 3 0 0 6 }
  proc in_half {cmdname {mid_str ""} {extension ".s"}} {in [format %s_half%s%s $cmdname $mid_str $extension] half 1 1 1 5 }
  proc in_hyp {cmdname {mid_str ""} {extension ".s"}} {in [format %s_hyp%s%s $cmdname $mid_str $extension] hyp 0 0 0 0 0 10 3 0 0 4 .3 }
  proc in_part {cmdname {mid_str ""} {extension ".s"}} {in [format %s_part%s%s $cmdname $mid_str $extension] part 0 0 0 0 0 16 4 2 }
  proc in_pipe {cmdname {mid_str ""} {extension ".s"}} {in [format %s_pipe%s%s $cmdname $mid_str $extension] pipe 4 0 0 0 3 5 6 0 0 3 3 5 7 3 4 8 2 6 10 8 8 10 0 6 8 }
  proc in_rcc {cmdname {mid_str ""} {extension ".s"}} {in [format %s_rcc%s%s $cmdname $mid_str $extension] rcc 0 0 0 3 3 30 7 }
  proc in_rec {cmdname {mid_str ""} {extension ".s"}} {in [format %s_rec%s%s $cmdname $mid_str $extension] rec 0 0 0 3 3 10 10 0 0 0 3 0 }
  proc in_rhc {cmdname {mid_str ""} {extension ".s"}} {in [format %s_rhc%s%s $cmdname $mid_str $extension] rhc 0 0 0 0 0 10 3 0 0 4 3 }
  proc in_rpc {cmdname {mid_str ""} {extension ".s"}} {in [format %s_rpc%s%s $cmdname $mid_str $extension] rpc 0 0 0 0 0 4 0 1 0 3 }
  proc in_rpp {cmdname {mid_str ""} {extension ".s"}} {in [format %s_rpp%s%s $cmdname $mid_str $extension] rpp 0 30 -3 12 -1 22 }
  proc in_sph {cmdname {mid_str ""} {extension ".s"}} {in [format %s_sph%s%s $cmdname $mid_str $extension] sph 42 42 42 42 }
  proc in_tec {cmdname {mid_str ""} {extension ".s"}} {in [format %s_tec%s%s $cmdname $mid_str $extension] tec 0 0 0 0 0 10 5 0 0 0 3 0 .6 }
  proc in_tgc {cmdname {mid_str ""} {extension ".s"}} {in [format %s_tgc%s%s $cmdname $mid_str $extension] tgc 0 0 0 0 0 10 5 0 0 0 8 0 2 9 }
  proc in_tor {cmdname {mid_str ""} {extension ".s"}} {in [format %s_tor%s%s $cmdname $mid_str $extension] tor 0 0 0 1 1 3 5 2 }
  proc in_trc {cmdname {mid_str ""} {extension ".s"}} {in [format %s_trc%s%s $cmdname $mid_str $extension] trc 0 0 0 0 0 10 4 7 }

  proc in_nmg {cmdname {mid_str ""} {extension ".s"}} {}

  proc in_sketch {{cmdname "extrude"} {mid_str ""} {extension ""}} {put [format {%s_sketch%s%s} $cmdname $mid_str $extension] sketch V {10 20 30} A {1 0 0} B {0 1 0} VL { {250 0} {500 0} {500 500} {0 500} {0 250} {250 250} {125 125} {0 125} {125 0} {200 200} } SL { { bezier D 4 P { 4 7 9 8 0 } } { line S 0 E 1 } { line S 1 E 2 } { line S 2 E 3 } { line S 3 E 4 } { carc S 6 E 5 R -1 L 0 O 0 } }}

  proc in_extrude {cmdname {mid_str ""} {extension ".s"} {sketch "extrude_sketch"}} { in [format %s_extrude%s%s $cmdname $mid_str $extension] extrude 0 0 0 0 0 1000 10 0 0 0 10 0 $sketch} 

  # A convenience routine is defined for cases where one wants to
  # use in to create an instance of each primitive.

  proc in_all_prims { cmdname } {
     global make_primitives_list
     foreach x $make_primitives_list {
	  if {![string match "extrude" $x] } {in_$x $cmdname "" .s}
     }
     
     # Extrude's in command needs more args, handle it outside
     # of the loop.
     in_extrude $cmdname "" .s [format %s_sketch.s $cmdname]

     # Remove this once nmg gets in support.  Don't do it if the
     # cmdname is 'in' since that is supposed to be testing the
     # in command, not the make command.  Otherwise, create it
     # so there is some nmg to test.
     if {![string match "in" $cmdname]} {
       make [format %s_nmg.s $cmdname] nmg
     }
  }

  proc edit_op_all_prims { cmdname args1 args2 } {
     global make_primitives_list
     foreach x $make_primitives_list {
	  Z
	  e [format %s_%s.s $cmdname $x]
	  sed [format %s_%s.s $cmdname $x]
          $cmdname $args1 [format %s_%s.s $cmdname $x] $args2
	  accept
	  Z
     }
  }



  #  Often it is desirable to insert a large number of primitives with
  #  sequential numbers - the batch_insert command is defined to handle
  #  these situations.  In these cases the number is appended as the
  #  last item in the name.

  proc batch_insert {cmdname primname extension startnum endnum increment} {
  	for {set i $startnum} {$i < [expr {$endnum + 1}]} {set i [expr {$i + $increment}]} {
	   in_$primname $cmdname $extension $i
	}
  }

  # TCL routines needed for specific commands, organized alphabetically
  # according to command

  # comb
  proc comb_all {cmdname} {
     global make_primitives_list
     foreach x $make_primitives_list {
	comb [format %s_%s.c $cmdname $x] u [format %s_%s.s $cmdname $x]
     }
  }

  # make_bb
  proc make_bb_all {} {
     global make_primitives_list
     foreach x $make_primitives_list {
	make_bb [format make_bb_box_%s.s $x] [format make_bb_%s.s $x]
     }
  }
 
  # translate all primitives 
  proc translate_all_prims {cmdname coord1 coord2 coord3} {
     global make_primitives_list
     foreach x $make_primitives_list {
      # for now, in nmg isn't producing sensible results 
      if {![string match nmg $x]} {
	e [format %s_%s.s $cmdname $x]
	sed [format %s_%s.s $cmdname $x]
	translate $coord1 $coord2 $coord3
	accept
	puts "Translated $x primitive"
	d [format %s_%s.s $cmdname $x]
       }
     }
  }

  # translate all combinations 
  proc translate_all_combs {cmdname coord1 coord2 coord3} {
     global make_primitives_list
     foreach x $make_primitives_list {
      # for now, in nmg isn't producing sensible results 
      if {![string match nmg $x]} {
        e [format %s_%s.c $cmdname $x]
        oed / [format %s_%s.c/%s_%s.s $cmdname $x $cmdname $x]
	translate $coord1 $coord2 $coord3
	accept
	puts "Translated $x combination"
	d [format %s_%s.c $cmdname $x]
      }
     }
  }

  # rotate all primitives 
  proc rot_all_prims {cmdname coord1 coord2 coord3} {
     global make_primitives_list
     foreach x $make_primitives_list {
      # for now, in nmg isn't producing sensible results 
      if {![string match nmg $x]} {
	e [format %s_%s.s $cmdname $x]
	sed [format %s_%s.s $cmdname $x]
	rot $coord1 $coord2 $coord3
	accept
	puts "rotated $x primitive"
	d [format %s_%s.s $cmdname $x]
       }
     }
  }


  puts "Regression testing definitions loaded.\n"

}
