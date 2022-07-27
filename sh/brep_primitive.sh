#!/usr/bin/env bash
#               B R E P P _ R I M I T I V E . S H
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
# This script creates solid primitives and converts them to brep
# format
#
###

db_name="brep_primitive.g"

# distance between two primitives
grid_size=2000
# how many primitives in each row
num_per_row=5

# 1: obj name. 2: obj type. 3: shift_x 4: shift_y
createObjWithPos(){
	mged -c $db_name <<EOF
	make $1 $2
	sed $1
	translate $3 $4 0
	accept
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

objs=("arb8" "ell" "sph" "ehy" "epa"
	  "tgc" "rcc" "rec" "rhc" "rpc" "tec"
	  "trc" "tor" "eto" "part" "pipe" "ars"
	  "bot")

# primitive name: {xpos}_{ypos}_xxx.p
# primitive name: {xpos}_{ypos}_xxx.b
for(( i=0;i<${#objs[@]};i++)) do
	element_type=${objs[i]}
	element_name=$[$i / $num_per_row]_$[$i % $num_per_row]_${objs[i]}
	echo $element_name
	createObjWithPos ${element_name}.p $element_type ${shift_x} ${shift_y}
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
