#!/usr/bin/env bash
#               B R E P P _ R H C _ T E S T . S H
# BRL-CAD
#
# Copyright (c) 2004-2022 United States Government as represented by
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
#
# This script creates rhc primitives with different parameters, then
# convert them to brep format.
#
###

db_name="brep_rhc_test.g"

# distance between two primitives
grid_size=2
# how many primitives in each row
num_per_row=3

# 1: obj name. 2: shift_x 3: shift_y
# 4 5 6: X, Y, Z, of vector H
# 7 8 9: X, Y, Z, of vector B
# 10: ectangular half-width, r
# 11: apex-to-asymptotes distance, c
createRhcWithPos(){
	mged -c $db_name <<EOF
	in $1 rhc $2 $3 0 $4 $5 $6 $7 $8 $9 ${10} ${11}
	Z
EOF
}

# 1: obj name. 2: brep name.
convertBrep(){
	mged -c $db_name <<EOF
	brep $1 brep $2
EOF
}

# create database
rm -f $db_name
mged -c $db_name opendb $db_name

shift_x=0
shift_y=0

case_num=6

# parameters:
# 0-2: X, Y, Z, of vector H
# 3-5: X, Y, Z, of vector B
# 6: ectangular half-width, r
# 7: apex-to-asymptotes distance, c
para_c0=(0 0 1 1 1 0 1 1)
para_c1=(1 0 0 0 0.5 0.5 0.2 0.1)
para_c2=(0 0 1 0.5 0.5 0 0.6 0.5)
para_c3=(0 0 1 -1 1 0 1.4 0.6)
para_c4=(0 1 0 -1 0 1 0.2 0.2)
para_c5=(1 0 0 0 -1 1 0.7 1.6)

for(( i=0;i<${case_num};i++)) do
	element_name=rhc_${i}
	eval para_name=\${para_c${i}[*]}

	createRhcWithPos ${element_name}.p ${shift_x} ${shift_y} ${para_name}
	
	convertBrep ${element_name}.p ${element_name}.b
	let shift_x+=$grid_size
	if [ $shift_x -gt $[ $grid_size * $num_per_row - $grid_size] ]; then
		shift_x=0
		let shift_y+=$grid_size
	fi
done;

# region primitives and breps respectively
mged -c $db_name "g p_union *.p" 
mged -c $db_name "g brep_union *.b" 

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
