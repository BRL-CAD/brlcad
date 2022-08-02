#!/bin/sh

# set up a half-torus ring
echo " put tor tor V {0 0 0}  H {1 0 0}  r_a 400 r_h 100 ;
       put sph ell V {0 400 0}  A {100 0 0}  B {0 100 0}  C {0 0 100} ;
       put sph2 ell V {0 -400 0}  A {100 0 0}  B {0 100 0}  C {0 0 100} ;
       in cut rpp -101 101 -501 501 -501 0 ;
       r ring.r u tor - cut u sph u sph2 ;" | mged -c metaring.g

# create metaballs at all positions
for i in `seq 0 5 350` ; do c=`echo "scale=10; c($i * 4*a(1) / 180) * 400" | bc -l` ; s=`echo "scale=10; s($i * 4*a(1) / 180) * 400" | bc -l` ; echo "kill metaball`printf %03d $i` ; in metaball`printf %03d $i` metaball 0 1 3 0 400 0 100 0 -400 0 100 0 $c $s 100" ; done | mged -c metaring.g

# render image frames
for i in `seq 0 5 350` ; do echo $i ;  rm -f m`printf %03d $i`.png ; rt -o m`printf %03d $i`.png -c "viewsize 1200" -c "orientation .5 .5 .5 .5" -c "eye_pt 5000 0 0" metaring.g metaball`printf %03d $i` ring.r 2>/dev/null ; done

# convert frames to animation
convert *.png anim.gif

