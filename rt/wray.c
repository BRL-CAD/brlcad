/*
 *  			W R A Y . C
 *
 *  Write a VLD-standard ray on the given file pointer.
 *  VLD-standard rays are defined by /vld/include/ray.h,
 *  included here for portability.  A variety of VLD programs
 *  exist to manipulate these files, including rayvect.
 *
 *  To obtain a UNIX-plot of a ray file, the procedure is:
 *	/vld/bin/rayvect -mMM < file.ray > file.vect
 *	/vld/bin/vectplot -mMM < file.vect > file.plot
 *	tplot -Tmeg file.plot		# or equivalent
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "../h/machine.h"
#include "../h/vmath.h"
#include "../h/raytrace.h"


/* /vld/include/ray.h -- ray segment data format (D A Gwyn) */
/* binary ray segment data record; see ray(4V) */
struct vldray  {
	float	ox;			/* origin coordinates */
	float	oy;
	float	oz;
	float	rx;			/* ray vector */
	float	ry;
	float	rz;
	float	na;			/* origin surface normal */
	float	ne;
	/* the following are in 2 pieces for binary file portability: */
	short	ob_lo;			/* object code low 16 bits */
	short	ob_hi;			/* object code high 16 bits */
	short	rt_lo;			/* ray tag low 16 bits */
	short	rt_hi;			/* ray tag high 16 bits */
};

/*
 *  			W R A Y
 */
void
wray( pp, ap, fp )
register struct partition *pp;
struct application *ap;
FILE *fp;
{
	LOCAL struct vldray vldray;
	register struct hit *hitp= pp->pt_inhit;
	register int i;

	VMOVE( &(vldray.ox), hitp->hit_point );
	VSUB2( &(vldray.rx), pp->pt_outhit->hit_point,
		hitp->hit_point );

	if( pp->pt_inflip )  {
		VREVERSE( hitp->hit_normal, hitp->hit_normal );
	}
	vldray.na = atan2( hitp->hit_normal[Y], hitp->hit_normal[X] );
	vldray.ne = asin( hitp->hit_normal[Z] );

	i = pp->pt_regionp->reg_regionid;
	vldray.ob_lo = i & 0xFFFF;
	vldray.ob_hi = (i>>16) & 0xFFFF;
	vldray.rt_lo = ap->a_level;
	vldray.rt_hi = ap->a_y;
	fwrite( &vldray, sizeof(struct vldray), 1, fp );
}

/*
 *  			W R A Y D I S T
 *  
 *  Write a VLD-standard ray for a section of a ray specified
 *  by the "in" and "out" distances along the ray.  This is usually
 *  used for logging passage through "air" (ie, no solid).
 */
void
wraypts( in, out, ap, fp )
vect_t in, out;
struct application *ap;
FILE *fp;
{
	LOCAL struct vldray vldray;

	VMOVE( &(vldray.ox), in );
	VSUB2( &(vldray.rx), out, in );

	vldray.na = atan2( ap->a_ray.r_dir[Y], ap->a_ray.r_dir[X] );
	vldray.ne = asin( ap->a_ray.r_dir[Z] );

	vldray.ob_lo = 0;	/* might want to be something special */
	vldray.ob_hi = 0;

	vldray.rt_lo = ap->a_level;
	vldray.rt_hi = ap->a_y;
	fwrite( &vldray, sizeof(struct vldray), 1, fp );
}
