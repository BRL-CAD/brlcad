#                       L I G H T S . C M A K E
# BRL-CAD
#
# Copyright (c) 2010-2022 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###

include("${CMAKE_CURRENT_SOURCE_DIR}/BRLCAD_Regress_Util.cmake")

ensearch(rt      RT      REQUIRED)
ensearch(asc2g   A2G     REQUIRED)
ensearch(pixdiff PIXDIFF REQUIRED)

execute_process(COMMAND ${CMAKE_COMMAND} -E remove -f lights.pix)

set(lights_asc "
title {Untitled BRL-CAD Database}
units mm
put {local} ell V {-4 -4 4} A {0.5 0 0} B {0 0.5 0} C {0 0 0.5}
put {infinite} ell V {-4 4 4} A {0.5 0 0} B {0 0.5 0} C {0 0 0.5}
put {plate.s} arb8 V1 {-30 -30 -1} V2 {30 -30 -1} V3 {30 30 -1} V4 {-30 30 -1} V5 {-30 -30 0} V6 {30 -30 0} V7 {30 30 0} V8 {-30 30 0}
put {pole2.s} tgc V {9 2.5 1} H {0 0 10} A {0 -0.25 0} B {0.25 0 0} C {0 -0.25 0} D {0.25 0 0}
put {pole1.s} tgc V {-11 2.5 1} H {0 0 10} A {0 -0.25 0} B {0.25 0 0} C {0 -0.25 0} D {0.25 0 0}
put {ball2.s} ell V {10 0 5} A {2 0 0} B {0 2 0} C {0 0 2}
put {ball1.s} ell V {-10 0 5} A {2 0 0} B {0 2 0} C {0 0 2}
put {shadow_objs.r} comb region yes tree {u {u {l ball1.s} {l pole1.s}} {u {l ball2.s} {l pole2.s}}}
attr set {shadow_objs.r} {region} {R} {los} {100} {material_id} {1} {region_id} {1001}
put {plate.r} comb region yes tree {l plate.s}
attr set {plate.r} {region} {R} {los} {100} {material_id} {1} {region_id} {1000}
put {infinite.r} comb region yes tree {l infinite}
attr set {infinite.r} {region} {R} {los} {100} {material_id} {1} {region_id} {1002} {oshader} {light {i 1 v 0}} {rgb} {255/255/255}
put {local.r} comb region yes tree {l local}
attr set {local.r} {region} {R} {rgb} {255/255/255} {oshader} {light {s 4  pt {-4.46848 -4.09864 4.1442} pt {-3.55149 -4.11535 4.18852} pt {-4.41469 -4.27933 4.00234} pt {-3.62755 -4.33234 4.02878} pt {-4.09301 -4.44478 3.79139} pt {-4.10851 -3.55786 3.79328} pt {-3.63361 -4.2739 3.79817} pt {-3.62019 -3.74156 3.80263} pt {-4.432 -3.88231 4.22254} pt {-3.59094 -3.85892 4.25053} pt {-3.85797 -4.45142 3.83861} pt {-3.84812 -3.55582 3.82785} pt {-4.16269 -4.30592 4.36048} pt {-3.84629 -4.30072 4.3687} pt {-4.3644 -3.65806 3.98285} pt {-3.67668 -3.61863 3.99506} pt {-4.38028 -3.92574 4.31602} pt {-3.62451 -3.93287 4.32327} pt {-4.36414 -3.6574 4.00475} pt {-3.66174 -3.63259 4.02432} pt {-4.27269 -3.6819 3.72714} pt {-3.71379 -3.68422 3.73853} pt {-4.38155 -3.67804 4.02765} pt {-3.60849 -3.68903 3.99648} pt {-4.41137 -4.15778 3.76362} pt {-3.59758 -4.17667 3.76157}}} {region_id} {1003} {material_id} {1} {los} {100}
put {all.g} comb region no tree {u {u {l infinite.r} {l local.r}} {u {l plate.r} {l shadow_objs.r}}}
")

file(WRITE lights.asc "${lights_asc}")
execute_process(COMMAND ${A2G} lights.asc lights.g)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove -f lights.asc)

message("rendering lights...")
set(vmat "
viewsize 1.600000000000000e+02;
orientation 0.000000000000000e+00 0.000000000000000e+00 0.000000000000000e+00 1.000000000000000e+00;
eye_pt 0.000000000000000e+00 0.000000000000000e+00 7.950000000000000e+01;
start 0; clean;
end;

")
file(WRITE vmat.in "${vmat}")
execute_process(COMMAND ${RT} -M -B -p30 -o lights.pix lights.g all.g INPUT_FILE vmat.in ERROR_FILE lights.log)
execute_process(COMMAND ${CMAKE_COMMAND} -E remove -f vmat.in)

execute_process(COMMAND ${PIXDIFF} lights.pix INPUT_FILE lights.pix OUTPUT_FILE lights_diff.pix ERROR_FILE lights_diff.log)

file(READ lights_diff.log llog)
file(APPEND lights.log "${llog}")
string(REGEX REPLACE ".*([0-9]+) off by many.*" "\\1" n_wrong "${llog}")
message("lights.pix ${n_wrong} off by many")
execute_process(COMMAND ${CMAKE_COMMAND} -E remove -f lights_diff.log)

if(${n_wrong} EQUAL 0)
    message("-> lights.sh succeeded")
else(${n_wrong} EQUAL 0)
    message(FATAL_ERROR "-> lights.sh FAILED")
endif(${n_wrong} EQUAL 0)



# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
