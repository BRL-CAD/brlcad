/*                      V I E W S C A T . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file viewscat.c
 *
 *  Ray Tracing program RTRAD bottom half.
 *
 *  This module takes the first hit from rt_shootray(), and produces
 *  a GIFT/SRIM format Radar file.  It tracks specular direction
 *  reflections.
 *
 *  Author -
 *	Phillip Dykstra
 *	Paul R. Stay		(Added parallelization and physics code)
 *	From viewpp.c and viewray.c by
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
#ifndef lint
static const char RCSppview[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtprivate.h"
#include "rad.h"

#ifndef M_PI
#define M_PI            3.14159265358979323846
#endif

#define	MAXREFLECT	16
#define	DEFAULTREFLECT	16

int		use_air = 0;		/* Handling of air in librt */
int		using_mlib = 0;		/* Material routines NOT used */

extern	FILE	*outfp;
extern	point_t	viewbase_model;		/* lower_left of viewing plane */
extern	mat_t	view2model;
extern	fastf_t	viewsize;
extern 	int 	npsw;			/* number of processors */

extern	int	width;
extern	int	height;

int	numreflect = DEFAULTREFLECT;	/* max number of reflections */

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
    "%d",	1, "maxreflect",	(int)&numreflect,	BU_STRUCTPARSE_FUNC_NULL,
    "%f",	1, "wavelength",	(int)&wavelength,	BU_STRUCTPARSE_FUNC_NULL,
    "%f",	1, "xhpol",	(int)&xhpol,		BU_STRUCTPARSE_FUNC_NULL,
    "%f",	1, "xvpol",	(int)&xvpol,		BU_STRUCTPARSE_FUNC_NULL,
    "%f",	1, "rhpol",	(int)&rhpol,		BU_STRUCTPARSE_FUNC_NULL,
    "%f",	1, "rvpol",	(int)&rvpol,		BU_STRUCTPARSE_FUNC_NULL,
    "%f",	1, "epsilon",	(int)&epsilon,		BU_STRUCTPARSE_FUNC_NULL,
    "",	0, (char *)0,	0,			BU_STRUCTPARSE_FUNC_NULL
};

void		dumpray();
void		dumpall();
static int	isvisible();

char usage[] = "\
Usage:  rtrad [options] model.g objects... >file.rad\n\
Options:\n\
 -s #		Grid size in pixels, default 512\n\
 -n #		Vertical # rays \n\
 -w #		Horizontal # rays \n\
 -a Az		Azimuth in degrees\n\
 -e Elev	Elevation in degrees\n\
 -g #		ray horizontal tube size (cell_width)\n\
 -G #		ray vertical tube size (cell_heigth)\n\
 -M		Read matrix, cmds on stdin\n\
 -o file.rad	Output file name, else stdout\n\
 -x #		Set librt debug flags\n\
";

struct rayinfo rayinfo[MAX_PSW][MAXREFLECT];
struct xray firstray[MAX_PSW];

static int radhit();
static int radmiss();

vect_t uhoriz;	/* horizontal emanation plane unit vector. */
vect_t unorml;	/* normal unit vector to emanation plane. */
vect_t cemant;	/* center vector of emanation plane. */
vect_t uvertp;	/* vertical emanation plane unit vector. */
fastf_t wavelength = 1.0;	/* Radar wavelength */
fastf_t xhpol = 0.0;	/* Transmitter vertical polarization */
fastf_t xvpol = 1.0;	/* Transmitter horizontal polarization */
fastf_t rhpol = 0.0;	/* Receiver vertical polarization */
fastf_t rvpol = 1.0;	/* Receiver horizontal polarization */
fastf_t epsilon = 1.0e-07;
fastf_t totali;
fastf_t totalq;

/*
 *  			V I E W _ I N I T
 *
 *  Called at the start of a run.
 *  Returns 1 if framebuffer should be opened, else 0.
 */
view_init( struct application *ap, char *file, char *obj, int minus_o )
{
    ap->a_hit = radhit;
    ap->a_miss = radmiss;
    ap->a_onehit = 1;

#ifdef SAR
    sar_init(ap, file, obj, minus_o);
#endif
    return(0);		/* no framebuffer needed */
}

/* beginning of a frame */
void
view_2init( struct application *ap )
{
    extern fastf_t azimuth, elevation;
    fastf_t elvang, aziang;
    vect_t temp, aimpt;
    fastf_t backoff;


    if( numreflect > MAXREFLECT ) {
	bu_log("Warning: maxreflect too large (%d), using %d\n",
	       numreflect, MAXREFLECT );
	numreflect = MAXREFLECT;
    }

    elvang = elevation * M_PI / 180.0;
    aziang = azimuth * M_PI / 180.0;

    uhoriz[0] = (fastf_t) sin(aziang);
    uhoriz[1] = (fastf_t) -cos(aziang);
    uhoriz[3] = (fastf_t) 0.0;
    VUNITIZE( uhoriz );

    unorml[0] = (fastf_t) cos(elvang) * uhoriz[1];
    unorml[1] = (fastf_t) -cos(elvang) * uhoriz[0];
    unorml[2] = (fastf_t) -sin(elvang);
    VUNITIZE( unorml );

    /* this doesn't seem to be quite right emanat.f */
    uvertp[0] = uhoriz[1] * unorml[2] - unorml[1]* uhoriz[2];
    uvertp[1] = uhoriz[2] * unorml[0] - unorml[2]* uhoriz[0];
    uvertp[2] = uhoriz[0] * unorml[1] - unorml[0]* uhoriz[1];
    VUNITIZE( uvertp );
    VPRINT("uhoriz",uhoriz);
    VPRINT("unorml",unorml);
    VPRINT("uvertp",uvertp);
    totali = 0.0;
    totalq = 0.0;

    VSET(temp, 0.0, 0.0, -1.414 );
    MAT4X3PNT( aimpt,view2model, temp);
    bu_log("aim point %f %f %f\n", aimpt[0], aimpt[1], aimpt[2]);
    bu_log("viewsize %f\n", viewsize);
    backoff = 1.414 * viewsize/2.0;
    bu_log("backoff %f\n", backoff);

#ifdef SAR
    sar_2init( ap );
#endif
}

/* end of each pixel */
void	view_pixel() {}

/* end of each line */
void	view_eol() {}

void	view_setup() {}
/* Associated with "clean" command, before new tree is loaded  */
void	view_cleanup() {}

/* end of a frame */
void
view_end()
{
    fastf_t rcs;
    fastf_t iret, qret;
    LOCAL int cpu_num, cpus;

    iret = 0.0;
    qret = 0.0;
    rcs = 0.0;

    iret = totali;
    qret = totalq;

    rcs = (iret * iret) + (qret * qret);

    bu_log("Az %2.1f Ele %2.1f totali %1.4E totalq %1.4E rcs %1.4E\n",
	   azimuth, elevation, iret, qret, rcs );
}

static int
radhit( struct application *ap, struct partition *PartHeadp )
{
    register struct partition *pp;
    register struct hit *hitp;
    LOCAL struct application sub_ap;
    LOCAL struct rayinfo *rayp;
    LOCAL fastf_t	f;
    LOCAL vect_t	to_eye, work;
    LOCAL int	depth;
    LOCAL int	cpu_num;


    for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
	if( pp->pt_outhit->hit_dist >= 0.0 )  break;
    if( pp == PartHeadp )  {
	bu_log("radhit:  no hit out front?\n");
	return(0);
    }

    if(R_DEBUG&RDEBUG_HITS)  {
	rt_pr_pt( ap->a_rt_i, pp );
    }

    hitp = pp->pt_inhit;
    if( hitp->hit_dist >= INFINITY )  {
	bu_log("radhit:  entry beyond infinity\n");
	return(1);
    }
    /* Check to see if eye is "inside" the solid */
    if( hitp->hit_dist < 0 )  {
	/* XXX */
	return(0);
    }

    if(R_DEBUG&RDEBUG_HITS)  {
	rt_pr_hit( " In", hitp );
    }

    if ( ap->a_resource == RESOURCE_NULL)
	cpu_num = 0;
    else
	cpu_num = ap->a_resource->re_cpu;

    rayp = &rayinfo[cpu_num][ ap->a_level +1 ];
    rayp->x = ap->a_x;
    rayp->y = ap->a_y;
    rayp->dist = hitp->hit_dist;
    rayp->reg = pp->pt_regionp->reg_regionid;
    rayp->sol = pp->pt_inseg->seg_stp->st_id;
    rayp->surf = hitp->hit_surfno;
    RT_HIT_NORMAL( rayp->norm, hitp, pp->pt_inseg->seg_stp, &(ap->a_ray), pp->pt_inflip );
    RT_CURVATURE( &(rayp->curvature), hitp, pp->pt_inflip, pp->pt_inseg->seg_stp );
    if( VDOT( hitp->hit_normal, ap->a_ray.r_dir ) < 0 ) {
	bu_log(" debug: curvature flip\n");
	rayp->curvature.crv_c1 = - rayp->curvature.crv_c1;
	rayp->curvature.crv_c2 = - rayp->curvature.crv_c2;
    }
    VMOVE( rayp->ip, hitp->hit_point );
    VMOVE( rayp->dir, ap->a_ray.r_dir);

    /* Compute the specular direction */
    VREVERSE( to_eye, ap->a_ray.r_dir );
    f = 2 * VDOT( to_eye, rayp->norm );
    VSCALE( work, rayp->norm, f );
    /* I have been told this has unit length */
    VSUB2( rayp->spec, work, to_eye );
    VUNITIZE( rayp->spec );

    /* Save info for 1st ray */
    if( ap->a_level == 0 ) {
	firstray[cpu_num] = ap->a_ray;	/* struct copy */
	rayp->sight = 1;	/* the 1st intersect is always visible */
    } else {
	/* Check for visibility */
	rayp->sight = isvisible( ap, hitp, rayp->norm );
    }

    /*
     * Shoot another ray in the specular direction.
     */
    if( ap->a_level < numreflect-1 ) {
	sub_ap = *ap;	/* struct copy */
	sub_ap.a_level = ap->a_level+1;
	sub_ap.a_purpose = "secondary ray";
	VMOVE( sub_ap.a_ray.r_pt, hitp->hit_point );
	VMOVE( sub_ap.a_ray.r_dir, rayp->spec );
	depth = rt_shootray( &sub_ap );
    } else {
	depth = 0;
    }

    if( ap->a_level == 0 ) {
	rayinfo[cpu_num][0].x = ap->a_x;
	rayinfo[cpu_num][0].y = ap->a_y;
	rayinfo[cpu_num][0].surf = depth+1;
	rayinfo[cpu_num][0].ip[0] = ap->a_ray.r_pt[0];
	rayinfo[cpu_num][0].ip[1] = ap->a_ray.r_pt[1];
	rayinfo[cpu_num][0].ip[2] = ap->a_ray.r_pt[2];
	radar_physics( cpu_num, depth + 1 );
#ifdef SAR
	dumpall( ap, cpu_num, depth + 1);
#endif
    }

    return(depth+1);	/* report hit to main routine */
}


static int
radmiss()  {
    return(0);
}

/*********** Eye Visibility Routines ************/
/*
 *  True if the intersection distance is >= distance back to the
 *  origin of the first ray in a chain.
 *  Called via isvisible on a hit.
 */
static int
hiteye( struct application *ap, struct partition *PartHeadp )
{
    register struct partition *pp;
    register struct hit *hitp;
    LOCAL vect_t work;
    LOCAL int cpu_num;

    for( pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw )
	if( pp->pt_outhit->hit_dist > 0 )  break;
    if( pp == PartHeadp )  {
	bu_log("hiteye:  no hit out front?\n");
	return(1);
    }
    hitp = pp->pt_inhit;
    if( hitp->hit_dist >= INFINITY )  {
	bu_log("hiteye:  entry beyond infinity\n");
	return(1);
    }
    /* The current ray segment exits "in front" of me,
     * find out where it went in.
     * Check to see if eye is "inside" of the solid.
     */
    if( hitp->hit_dist < -1.0e-10 )  {
	return(0);
    }

    if ( ap->a_resource == RESOURCE_NULL)
	cpu_num = 0;
    else
	cpu_num = ap->a_resource->re_cpu;

    VSUB2( work, firstray[cpu_num].r_pt, ap->a_ray.r_pt );
    if( hitp->hit_dist * hitp->hit_dist > MAGSQ(work) )
	return(1);
    else
	return(0);
}

/*
 *  Always true
 *  Called via isvisible on a miss.
 */
static int
hittrue( struct application *ap, struct partition *PartHeadp )
{
    return(1);
}

/*
 *  Determine whether the current hitpoint along a series of
 *  reflections is visible from the origin of the ray.
 *  (which is the location of our "point" eye for now)
 *
 *  Strategy: we shoot back toward the origin of the ray
 *   If we don't hit anything (i.e. miss) we made it.
 *   If we hit something we made it if that distance is greater
 *   than the distance back to the eye.
 */
static int
isvisible( struct application *ap, struct hit *hitp, const vect_t norm )
{
    LOCAL int cpu_num;
    LOCAL struct application sub_ap;
    LOCAL vect_t	rdir;

    if ( ap->a_resource == RESOURCE_NULL)
	cpu_num = 0;
    else
	cpu_num = ap->a_resource->re_cpu;

    /* compute the ray direction */
    VSUB2( rdir, firstray[cpu_num].r_pt, hitp->hit_point );
    VUNITIZE( rdir );
    if( VDOT(rdir, norm) < 0 )
	return( 0 );	/* backfacing */

    sub_ap = *ap;	/* struct copy */
    sub_ap.a_level = ap->a_level+1;
    sub_ap.a_onehit = 1;
    sub_ap.a_purpose = "sight";
    sub_ap.a_hit = hiteye;
    sub_ap.a_miss = hittrue;
    /*
     * New origin is one unit in the ray direction in
     * order to get away from the surface we intersected.
     */
    VADD2( sub_ap.a_ray.r_pt, hitp->hit_point, rdir );
    VMOVE( sub_ap.a_ray.r_dir, rdir );

    return( rt_shootray( &sub_ap ) );
}

void application_init () {}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
