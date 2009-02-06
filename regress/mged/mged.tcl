#                         M G E D . T C L
# BRL-CAD
#
# Copyright (c) 2004-2009 United States Government as represented by
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

###########
#
#                     M G E D . T C L
#
# This script is the master top level script used to run a comprehensive
# regression test on all MGED commands.  It runs individual scripts with
# mged for each command and is responsible for managing outputs in such
# a fashion that they can be systematically compared to a standard
#

#
#	SETUP

set MGED_CMD /Users/cyapp/brlcad-install/bin/mged

file delete mged.g mged.log

proc run_test {cmdname} {
     global MGED_CMD
     exec $MGED_CMD -c mged.g source [format %s.tcl $cmdname] >>& mged.log
}

#
#	GEOMETRIC INPUT COMMANDS
#
run_test in
run_test make
run_test 3ptarb
run_test arb
run_test comb
run_test g
run_test r
run_test make_bb
run_test cp
run_test cpi
run_test mv
run_test mvall
run_test clone
run_test build_region
run_test prefix
run_test mirror

#
#	DISPLAYING GEOMETRY - COMMANDS
#
