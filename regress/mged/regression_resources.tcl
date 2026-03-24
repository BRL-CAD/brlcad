#        R E G R E S S I O N _ R E S O U R C E S . T C L
# BRL-CAD
#
# Copyright (c) 2009-2025 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# version 2.1 as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
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

  # bb
  proc bb_all {} {
     global make_primitives_list
     foreach x $make_primitives_list {
	bb -c [format bb_%s.s $x] [format make_bb_%s.s $x]
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

  # orot all combinations (object edit rotation via orot command)
  proc orot_all_combs {cmdname coord1 coord2 coord3} {
     global make_primitives_list
     foreach x $make_primitives_list {
      # for now, in nmg isn't producing sensible results
      if {![string match nmg $x]} {
	e [format %s_%s.c $cmdname $x]
	oed / [format %s_%s.c/%s_%s.s $cmdname $x $cmdname $x]
	orot $coord1 $coord2 $coord3
	accept
	puts "orot $x combination"
	d [format %s_%s.c $cmdname $x]
       }
     }
  }

  # translate all combinations via oed translate (object edit translate)
  proc oed_translate_all_combs {cmdname coord1 coord2 coord3} {
     global make_primitives_list
     foreach x $make_primitives_list {
      # for now, in nmg isn't producing sensible results
      if {![string match nmg $x]} {
	e [format %s_%s.c $cmdname $x]
	oed / [format %s_%s.c/%s_%s.s $cmdname $x $cmdname $x]
	translate $coord1 $coord2 $coord3
	accept
	puts "oed translate $x combination"
	d [format %s_%s.c $cmdname $x]
       }
     }
  }


  puts "Regression testing definitions loaded.\n"

  # Helper proc for primitive parameter editing regression tests (prim_edit.mged)
  # Tests that sed/press/p/accept correctly sets primitive parameters via rt_edit_process()
  # idx: component index for vector attributes (-1 for scalar, 0/1/2 for x/y/z)
  proc prim_edit_check {prim menu_item param_val attr_name expected_val idx} {
      set tolerance 0.001
      e $prim
      sed $prim
      press $menu_item
      p $param_val
      press accept
      d $prim
      if {$idx >= 0} {
          set raw [db get $prim $attr_name]
          if {[llength $raw] <= $idx} {
              puts "  FAIL: \[$prim\] $menu_item -> $param_val  ($attr_name list too short: $raw)"
              return
          }
          set got [lindex $raw $idx]
      } else {
          set got [db get $prim $attr_name]
      }
      if {![string is double -strict $got]} {
          puts "  FAIL: \[$prim\] $menu_item -> $param_val  ($attr_name returned non-numeric: $got)"
          return
      }
      set diff [expr {abs($got - $expected_val)}]
      if {$diff < $tolerance} {
          puts "  PASS: \[$prim\] $menu_item -> $param_val"
      } else {
          puts "  FAIL: \[$prim\] $menu_item -> $param_val  ($attr_name expected $expected_val, got $got)"
      }
  }

  # Mouse-simulation helper for prim_edit.mged
  # Tests the graphical sedit_mouse path: M command simulates mouse Y-axis drag
  # which calls sedit_mouse() -> sedit() -> rt_edit_process() -> DM callbacks.
  # At M 1 0 2047 (max Y), mousevec[Y] = 2047*INV_BV ≈ 0.9995, so
  # es_scale = 1 + 0.25*0.9995 = ~1.2499 (scale up ~25%).
  # Checks that: (a) value changed, and (b) new value ≈ init_val * MOUSE_SCALE_UP.
  # idx: component index for vector attributes (-1 for scalar, 0/1/2 for x/y/z)
  proc prim_edit_mouse_check {prim menu_item attr_name init_val idx} {
      set tolerance    0.01
      set MOUSE_SCALE_UP 1.24987793
      e $prim
      sed $prim
      press $menu_item
      M 1 0 2047
      press accept
      d $prim
      if {$idx >= 0} {
          set raw [db get $prim $attr_name]
          if {[llength $raw] <= $idx} {
              puts "  FAIL: \[$prim\] $menu_item mouse  ($attr_name list too short: $raw)"
              return
          }
          set got [lindex $raw $idx]
      } else {
          set got [db get $prim $attr_name]
      }
      if {![string is double -strict $got]} {
          puts "  FAIL: \[$prim\] $menu_item mouse  ($attr_name non-numeric: $got)"
          return
      }
      set expected [expr {$init_val * $MOUSE_SCALE_UP}]
      set rel_diff [expr {abs($got - $expected) / $expected}]
      if {$rel_diff < $tolerance} {
          puts "  PASS: \[$prim\] $menu_item (mouse M=2047, got=[format %.4f $got])"
      } else {
          puts "  FAIL: \[$prim\] $menu_item mouse  (expected≈[format %.4f $expected], got=[format %.4f $got])"
      }
  }

  # NMG-specific edit check for prim_edit.mged
  # NMG uses topological editing (Pick Edge + Move Edge) rather than scalar parameter
  # scaling, so it requires a dedicated proc.  The vertex list returned by
  # [db get $prim V] is a multi-token Tcl result that cannot be captured with `set`
  # from the MGED command stream; it CAN be iterated with foreach inside a proc that
  # is defined in a sourced .tcl file (this file), which is why this proc lives here.
  #
  # Workflow tested:
  #   p-path:   sed -> press "Pick Edge" -> M 1 0 0 (pick nearest edge at screen
  #             centre) -> press "Move Edge" -> p move_x move_y move_z -> accept
  #             Verifies: first vertex X coordinate is within tolerance of expected_vx
  #   nav-path: sed -> press "Pick Edge" -> M 1 0 0 -> press "Next EU" -> accept
  #             Verifies: navigation completes without error (calls rt_edit_process)
  proc prim_edit_nmg_check {prim move_x move_y move_z expected_vx} {
      set tolerance 1.0
      # p-path: pick edge via mouse, move to known 3-D position
      e $prim
      sed $prim
      press "Pick Edge"
      M 1 0 0
      press "Move Edge"
      p $move_x $move_y $move_z
      press accept
      d $prim
      set found 0
      foreach v [db get $prim V] {
          set x [lindex $v 0]
          if {[string is double $x] && [expr {abs($x - $expected_vx)}] < $tolerance} {
              set found 1
          }
      }
      if {$found} {
          puts "  PASS: \[$prim\] NMG Move Edge p $move_x $move_y $move_z"
      } else {
          puts "  FAIL: \[$prim\] NMG Move Edge (no vertex with X near $expected_vx)"
      }
      # nav-path: pick edge, navigate to next edgeuse (calls rt_edit_process)
      e $prim
      sed $prim
      press "Pick Edge"
      M 1 0 0
      press "Next EU"
      press accept
      d $prim
      puts "  PASS: \[$prim\] NMG Next EU (navigation)"
  }

}
