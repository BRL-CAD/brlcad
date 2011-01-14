#!/bin/sh
#                P U L L _ S O L I D S U B . S H
# BRL-CAD
#
# Copyright (c) 2007-2011 United States Government as represented by
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

# This is an awk program to go through a fastgen file and pull out all the
# solid subtraction cases for the fastgen entities wedge,sphere,box, and
# cylinder. The component number and the relevant part of that component
# description are printed.


nawk '

# inside_cyl: find out whether the second of the two cylinders from
# comp[m] to comp[n] is inside the first.
# We check to see that the origin of the second cylinder is inside the
# first, that it is defined in the same direction as the first, and
# that the magnitude of the height vector of the second is less than
# that of the first. This does not check to see that the second does not
# stick out of the first by having a short vector but being defined close
# to the top of the first, nor does it check the radii of the cylinders.
# Both of these would be easy to add.
function inside_cyl(m,n)
{
	for( m += 3; m < n; m += 3 ){
		ax = substr(comp[m-3],1,8)
		ay = substr(comp[m-3],9,8)
		az = substr(comp[m-3],18,8)
		ax2 = substr(comp[m-2],1,8)
		ay2 = substr(comp[m-2],9,8)
		az2 = substr(comp[m-2],18,9)

		a_del[0] = ax2 - ax
		a_del[1] = ay2 - ay
		a_del[2] = az2 - az

		bx = substr(comp[m],1,8)
		by = substr(comp[m],9,8)
		bz = substr(comp[m],18,8)
		bx2 = substr(comp[m+1],1,8)
		by2 = substr(comp[m+1],9,8)
		bz2 = substr(comp[m+1],18,9)

		b_del[0] = bx2 - bx
		b_del[1] = by2 - by
		b_del[2] = bz2 - bz

		if( shorter( b_del, a_del ) &&
			between(bx,ax,ax2) &&
			between(by,ay,ay2) && between(bz,az,az2) )

			return 1
		else
			return 0
	}
}

# shorter: returns 1 if (a dot b)/|a||b| == 1 and mag(a) <= mag(b),
# 0 otherwise
function shorter(a,b)
{
	if( (amag=mag(a)) > (bmag=mag(b)) ) {
		return 0
	}

	adotb = a[0]*b[0] + a[1]*b[1] + a[2]*b[2]
	cosab = adotb / ( amag * bmag )

	if( cosab >= 0.9999 && cosab <= 1.0001 )
		return 1
	else
		return 0
}

# mag: returns the magnitude of vector a
function mag(a)
{
	return sqrt( a[0]*a[0] + a[1]*a[1] + a[2]*a[2] )
}

# between: returns 1 if a lies within the range from b to c inclusive of the
#		     base but not of the top of the cylinder (we want to avoid
#		     abutting cylinders),
#		   0 otherwise
# b and c do not have to be in any order ( b and c are the base and top )
function between(a,b,c)
{
	if( a == b && a == c )
		return 1
	else if( b <= c && a >= b && a < c )
		return 1
	else if( a > c && a <= b )
		return 1
	else
		return 0
}

# abs: returns the absolute value of x
function abs(x)
{
	if( x >= 0 )
		return x
	else
		return -x
}


# main loop
{
	if( NR == 1 ) {
		comp[nl=1] = $0
		prev_cc = substr($0,32,4)
	}
	else {
		comp[nl=1] = prev_line
		comp[++nl] = $0
	}

	while( getline && ((cc=substr($0,32,4)) == prev_cc ))
		comp[++nl] = $0
	prev_line = $0
	prev_cc = substr($0,32,4)

	# now we can process this component number

	prev_stype = substr( comp[1],28,1 )
	for( i=2 ; i <= nl ; i++ ) {

		if( prev_stype >= 5 && prev_stype <=8 ) {
			begini = i - 1
			stype = substr( comp[i],28,1 )
			while( i <= nl && stype == prev_stype ) {
				stype = substr(comp[i],28,1)
				i++
			}

			if( prev_stype == 5 && (i - begini) > 4 ) {
				printf "component #%s - wedge\n",substr(comp[begini],32,4)
				for( j = begini; j < i; j++ )
					printf "%s\n", comp[j]
			}
			else if( prev_stype == 6 && (i - begini) > 2 ) {
				printf "component #%s - sphere\n",substr(comp[begini],32,4)
				for( j = begini; j < i; j++ )
					printf "%s\n", comp[j]
			}
			else if( prev_stype == 7 && (i - begini) > 4 ) {
				printf "component #%s - box\n",substr(comp[begini],32,4)
				for( j = begini; j < i; j++ )
					printf "%s\n", comp[j]
			}
			else if( prev_stype == 8 && (i - begini) > 3 ) {
				if( inside_cyl( begini,i ) ) {
					printf "component #%s - cylinder\n",substr(comp[begini],32,4)
					for( j = begini; j < i; j++ )
						printf "%s\n", comp[j]
				}
			}
		}
		prev_stype = substr( comp[i],28,1 )
	}

} ' $*


# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
