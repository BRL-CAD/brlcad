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
# Distribution Notice -
#	Re-distribution of this software is restricted, as described in
#       your "Statement of Terms and Conditions for the Release of
#       The BRL-CAD Package" agreement.
#
# Contributors -
#
#
# Description -
#	solid_data_vv_pairs - an array that holds the default variable/value pairs.
#	solid_data_init - initializes the solid_data variables.
#

set solid_data_vv_pairs {
    { types {arb8 sph ell tor tgc rec half rpc rhc epa ehy eto part} }
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
    { label,part "Particle Solid" }
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
    { attr,rpc  {V {-1 -1 -1.5} H {0 0 1} B {0 .5 0} r .25} }
    { attr,rhc  {V {-1 -1 -1.5} H {0 0 1} B {0 .5 0} r .25 c .1} }
    { attr,epa  {V {-1 -1 -1.5} H {0 0 1} A {0 1 0} r_1 .5 r_2 .25} }
    { attr,ehy  {V {-1 -1 -1.5} H {0 0 1} A {0 1 0} r_1 .5 r_2 .25 c .25} }
    { attr,eto  {V {-1 -1 -1} N {0 0 1} C {.1 0 .1} r .5 r_d .05} }
    { attr,part {V {-1 -1 -.5} H {0 0 1} r_v 0.5 r_h 0.25} }
    { name,indexvar index }
    { name,op "incr index" }
    { name,default "mySolid" }
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
