#                       S O L I D . T C L
# BRL-CAD
#
# Copyright (C) 2004-2005 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###
##
#				S O L I D . T C L
#
# Authors -
#	Bob Parker
#
# Source -
#	The U. S. Army Research Laboratory
#	Aberdeen Proving Ground, Maryland  21005
#
#
# Contributors -
#
#
# Description -
#	solid_data_vv_pairs - an array that holds the default variable/value pairs.
#	solid_data_init - initializes the solid_data variables.
#

set solid_data_vv_pairs {
    { types {arb8 sph ell tor tgc rec half rpc rhc epa ehy eto part dsp ebm hf vol} }
    { hoc,arb8 "This is an arbitrary convex polyhedron
with 8 vertices." }
    { hoc,sph "This is a sphere. The sphere
has the following input fields:

     V     vector to center of sphere
     A     radius vector A
     B     radius vector B
     C     radius vector C" }
    { hoc,ell "This is an ellipsoid. The ell
has the following input fields:

     V     vector to center of ellipsoid
     A     radius vector A
     B     radius vector B
     C     radius vector C" }
    { hoc,tor "This is a torus. The tor
has the following input fields:

     V     vector to center of torus
     H     radius vector, normal to plane of torus
     r_a   radius in A direction
     r_h   radius in H direction" }
    { hoc,tgc "This is a truncated general cone. The tgc
has the following input fields:

     V     vector to center of tgc
     H     height vector
     A     radius of A
     B     radius of B
     C     radius of C
     D     radius of D" }
    { hoc,rec "This is a right elliptical cylinder. The rec
has the following input fields:

     V     vector to center of rec
     H     height vector
     A     radius of A
     B     radius of B
     C     radius of C
     D     radius of D" }
    { hoc,half "This is a half space. The half space
has the following input fields:

     N     outward pointing normal vector
     d     distance from the origin to the plane" }
    { hoc,rpc "This is a right parabolic cylinder. The rpc
has the following input fields:

     V     vector to center of rpc
     H     height vector
     B     breadth vector
     r     scalar half-width of rectangular face" }
    { hoc,rhc "This is a right hyperbolic cylinder. The rhc
has the following input fields:

     V     vector to center of rhc
     H     height vector
     B     breadth vector
     r     scalar half-width of rectangular face
     c     dist from hyperbola to vertex of asymptotes" }
    { hoc,epa "This is an elliptical paraboloid. The epa
has the following input fields:

     V     vector to center of epa
     H     height vector
     A     vector along semi-major axis
     r_1   scalar semi-major axis length
     r_2   scalar semi-minor axis length" }
    { hoc,ehy "This is a right hyperbolic cylinder. The ehy
has the following input fields:

     V     vector to center of ehy
     H     height vector
     A     vector along semi-major axis
     r_1   scalar semi-major axis length
     r_2   scalar semi-minor axis length
     c     dist from hyperbola to vertex of asymptotes" }
    { hoc,eto "This is an elliptical torus. The eto
has the following input fields:

     V     vector from origin to center of eto
     N     unit normal to plane of eto
     C     semi-major axis of ellipse
     r     radius of revolution
     r_d   semi-minor axis of ellipse" }
    { hoc,part "This is a particle solid. The particle
solid has the following input fields:

     V     vector from origin to center of part
     H     height vector
     r_v   radius of v
     r_h   radius of h" }
    { hoc,dsp "This is a displacement map. The displacement
map has the following input fields:

     file    data file
     sm      Boolean: surface normal interpretation
     w       width of data file
     n       height of data file
     stom    matrix for transforming solid to model space" }
    { hoc,ebm "This is an extruded bitmap. The extruded
bitmap has the following input fields:

      file    bitmap
      w       width of file
      n       height of file
      d       depth of file
      mat     transform local coords to model space" }
    { hoc,hf "This is a height field. The height field
has the following input fields:

      cfile      control file
      dfile      data file
      fmt        CV style file format descriptor
      w          width of data file
      n          height of data file
      shorts     Boolean: use shorts instead of double
      file2mm    scale file elevation units to mm
      v          origin of height field in model space
      x          X direction vector
      y          Y direction vector
      xlen       magnitude of HF in X direction
      ylen       magnitude of HF in Y direction
      zscale     scale of data in ''up'' dir (after file2mm is applied)" }
    { hoc,vol "This is a volume solid. The volume solid
has the following input fields:

      file    data file
      w      width of data file
      n       height of data file
      d       depth of data file
      lo      Low threshold
      hi      High threshold
      size   ideal coords: size of each cell
      mat   transform local coords to model space" }
    { label,arb8 "Arbitrary 8-vertex polyhedron" }
    { label,sph  "Sphere" }
    { label,ell  "Ellipsoid" }
    { label,ellg "General Ellipsoid" }
    { label,tor  "Torus" }
    { label,tgc  "Truncated General Cone" }
    { label,rec  "Right Elliptical Cylinder" }
    { label,half "Halfspace"		  }
    { label,rpc  "Right Parabolic Cylinder" }
    { label,rhc  "Right Hyperbolic Cylinder" }
    { label,epa  "Elliptical Paraboloid" }
    { label,ehy  "Right Hyperbolic Cylinder" }
    { label,eto  "Elliptical Torus" }
    { label,part "Particle" }
    { label,dsp "Displacement Map" }
    { label,hf "Height Field" }
    { label,ebm "Extruded Bitmap" }
    { label,vol "Volume" }
    { label,V "Vertex" }
    { label,A "A vector" }
    { label,B "B vector" }
    { label,C "C vector" }
    { label,D "D vector" }
    { label,H "H vector" }
    { label,N "Normal vector" }
    { label,c "c value" }
    { label,d "d value" }
    { label,r "Radius" }
    { label,r_a "Radius a" }
    { label,r_d "Radius d" }
    { label,r_h "Radius h" }
    { label,r_v "Radius v" }
    { label,r_1 "Radius 1" }
    { label,r_2 "Radius 2" }
    { attr,arb8 {V1 {1 -1 -1}  V2 {1 1 -1}  V3 {1 1 1}  V4 {1 -1 1} \
	    V5 {-1 -1 -1} V6 {-1 1 -1} V7 {-1 1 1} V8 {-1 -1 1}} }
    { attr,sph  {V {0 0 0} A {1 0 0} B {0 1 0} C {0 0 1}} }
    { attr,ell  {V {0 0 0} A {2 0 0} B {0 1 0} C {0 0 1}} }
    { attr,tor  {V {0 0 0} H {1 0 0} r_h 2 r_a 1} }
    { attr,tgc  {V {0 0 0} H {0 0 4} A {1 0 0} B {0 .5 0} C {.5 0 0} D {0 1 0}} }
    { attr,rec  {V {0 0 0} H {0 0 4} A {1 0 0} B {0 .5 0} C {1 0 0} D {0 .5 0}} }
    { attr,half {N {0 0 1} d -1} }
    { attr,rpc  {V {0 0 0} H {0 0 1} B {0 .5 0} r .25} }
    { attr,rhc  {V {0 0 0} H {0 0 1} B {0 .5 0} r .25 c .1} }
    { attr,epa  {V {0 0 0} H {0 0 1} A {0 1 0} r_1 .5 r_2 .25} }
    { attr,ehy  {V {0 0 0} H {0 0 1} A {0 1 0} r_1 .5 r_2 .25 c .25} }
    { attr,eto  {V {0 0 0} N {0 0 1} C {.1 0 .1} r .5 r_d .05} }
    { attr,part {V {0 0 0} H {0 0 1} r_v 0.5 r_h 0.25} }
    { attr,dsp  {file "" sm 1 w 1 n 1 stom {1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1}} }
    { attr,hf   {cfile "" dfile "" fmt "" w 1 n 1 shorts 1 file2mm 1.0 v {0 0 0} \
	    x {0 0 0} y {0 0 0} xlen 1 ylen 1 zscale 1} }
    { attr,ebm  {file "" w 1 n 1 d 1 mat {1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1}} }
    { attr,vol  {file "" w 1 n 1 d 1 lo 0 hi 1 size {1 1 1} mat {1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1}} }
    { name,indexvar index }
    { name,op "incr index" }
    { name,default "myPrimitive" }
    { entry,incr_op "\$val + 1.0" }
    { entry,decr_op "\$val - 1.0" }
    { entry,fmt "%.4f" }
    { type,default arb8 }
}

## - solid_data_init
#
# Set the solid_data variables if not already set.
# This allows the user to override these values elsewhere.
#
proc solid_data_init {} {
    global solid_data_vv_pairs
    global solid_data

    foreach vv_pair $solid_data_vv_pairs {
	set index [lindex $vv_pair 0]

	if ![info exists solid_data($index)] {
	    set solid_data($index) [lindex $vv_pair 1]
	}
    }
}

# Local Variables:
# mode: Tcl
# tab-width: 8
# c-basic-offset: 4
# tcl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
