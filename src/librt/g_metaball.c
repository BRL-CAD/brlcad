/*			G _ M E T A B A L L . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @addtogroup g_  */

/*@{*/
/** @file g_metaball.c
 * Intersect a ray with a metaball implicit surface.
 *
 * NOTICE: this primitive is incomplete and should be considered
 *	  experimental.
 *
 * Authors -
 *	Erik Greenwald <erikg@arl.army.mil>
 *
 * Source -
 *	ARL/SLAD/SDB Bldg 238
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
/*@}*/

#ifndef lint
static const char RCSmetaball[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stddef.h>
#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "rtgeom.h"
#include "wdb.h"
#include "./debug.h"


/*
 *  Algorithm:
 *	completely punted at the moment :D
 */

/* compute the bounding sphere for a metaball cluster. center is filled, and the
 * radius is returned. */
fastf_t rt_metaball_get_bounding_sphere(point_t *center, fastf_t threshold, struct bu_list *points)
{
	struct wdb_metaballpt *mbpt, *mbpt2;
	point_t min, max, d;
	fastf_t r = 0.0, dist, mag;
	int i;

	/* find a bounding box for the POINTS (NOT the surface) */
	VSETALL(min,+INFINITY);
	VSETALL(max,-INFINITY);
	for(BU_LIST_FOR(mbpt, wdb_metaballpt, points))
		for(i=0;i<3;i++) {
			if(mbpt->coord[i] < min[i])
				min[i] = mbpt->coord[i];
			if (mbpt->coord[i] > max[i])
				max[i] = mbpt->coord[i];
		}

	/* compute the center of the generated box, call that the center */
	VADD2(*center, min, max);
	VSCALE(*center, *center, 0.5);

	/* start looking for the radius... */
	for(BU_LIST_FOR(mbpt, wdb_metaballpt, points)){
		VSUB2(d,mbpt->coord,*center);
		/* since the surface is where threshold=fldstr/mag,
		   mag=fldstr/threshold, so make that the initial value */
		dist = MAGNITUDE(d) + mbpt->fldstr/threshold;
		/* and add all the contribution */
		for(BU_LIST_FOR(mbpt2, wdb_metaballpt, points))
			if(mbpt2 != mbpt){
				fastf_t additive;
				VSUB2(d, mbpt2->coord, mbpt->coord);
				mag = MAGNITUDE(d) + dist;
				additive = (mbpt2->fldstr*mbpt2->fldstr) / mag;
				dist += additive;
			}
		/* then see if this is a 'defining' point */
		if(dist > r)
			r = dist;
	}
	return r;
}

/**
 *  			R T _ M E T A B A L L _ P R E P
 *
 * prep and build bounding volumes... unfortunately, generating the bounding
 * sphere is too 'loose' (I think) and O(n^2).
 */
int
rt_metaball_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
	struct rt_metaball_internal *mb, *nmb;
	struct wdb_metaballpt *mbpt, *nmbpt;
	int i;

	mb = ip->idb_ptr;
	RT_METABALL_CK_MAGIC(mb);

	/* generate a copy of the metaball */
	nmb = bu_malloc(sizeof(struct rt_metaball_internal), "rt_metaball_prep: nmb");
	BU_LIST_INIT( &nmb->metaball_ctrl_head );
	nmb->threshold = mb->threshold;

	/* and copy the list of control points */
	for(BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_ctrl_head)) {
		nmbpt = (struct wdb_metaballpt *)bu_malloc(sizeof(struct wdb_metaballpt), "rt_metaball_prep: nmbpt");
		nmbpt->fldstr = mbpt->fldstr;
		VMOVE(nmbpt->coord,mbpt->coord);
		BU_LIST_INSERT( &nmb->metaball_ctrl_head, &nmbpt->l );
	}

	/* find the bounding sphere */
	stp->st_aradius = rt_metaball_get_bounding_sphere(&stp->st_center, mb->threshold, &mb->metaball_ctrl_head);
	stp->st_bradius = stp->st_aradius * 1.01;
	/* generate a bounding box around the sphere... 
	 * XXX this can be optimized greatly to reduce the BSP presense... */
	VSET(stp->st_min, 
		stp->st_center[X] - stp->st_aradius, 
		stp->st_center[Y] - stp->st_aradius, 
		stp->st_center[Z] - stp->st_aradius);
	VSET(stp->st_max, 
		stp->st_center[X] + stp->st_aradius, 
		stp->st_center[Y] + stp->st_aradius, 
		stp->st_center[Z] + stp->st_aradius);
	stp->st_specific = (void *)nmb;
	return 0;
}

/**
 *			R T _ M E T A B A L L _ P R I N T
 */
void
rt_metaball_print(register const struct soltab *stp)
{
	bu_log("rt_metaball_print called\n");
	return;
}

fastf_t
rt_metaball_point_value(point_t *p, struct bu_list *points)
{
	struct wdb_metaballpt *mbpt;
	fastf_t ret = 0.0;
	point_t v;

	for(BU_LIST_FOR(mbpt, wdb_metaballpt, points)) {
		VSUB2(v, mbpt->coord, *p);
		ret += (mbpt->fldstr*mbpt->fldstr) / MAGSQ(v);	/* f^2/r^2 */
	}
	return ret;
}

/*
 *  			R T _ M E T A B A L L _ S H O T
 */
int
rt_metaball_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
	int stat=0, retval = 0, segsleft = abs(ap->a_onehit);
	struct rt_metaball_internal *mb = (struct rt_metaball_internal *)stp->st_specific;
	struct seg *segp = NULL;
	point_t p, inc, delta;
	fastf_t initstep = stp->st_aradius / 20.0, finalstep = stp->st_aradius / 1e8, step = initstep, distleft = (rp->r_max-rp->r_min);
	
	VJOIN1(p, rp->r_pt, rp->r_min, rp->r_dir);
	VSCALE(inc, rp->r_dir, step); /* assume it's normalized and we want to creep at step */

/* we hit, but not as fine-grained as we want. So back up one step, cut the 
 * step size in half and start over... Note that once we're happily inside, we
 * do NOT change the step size back! */
#define STEPBACK { distleft += step; VSUB2(p, p, inc); step *= .5; VSCALE(inc,inc,.5); }
#define STEPIN(x) { --segsleft; ++retval; VSUB2(delta, p, rp->r_pt); segp->seg_##x.hit_dist = MAGNITUDE(delta); }
	while( distleft >= 0.0 ) {
		distleft -= step; 
		VADD2(p, p, inc);
		if(stat == 1) {
			if(rt_metaball_point_value(&p, &mb->metaball_ctrl_head) < mb->threshold ) {
				if(step<=finalstep) {
					STEPIN(out);
					stat = 0;
					if(segsleft <= 0) {
					    return retval;
					}
				} else
					STEPBACK
			}
		} else {
			if(rt_metaball_point_value(&p, &mb->metaball_ctrl_head) > mb->threshold ) {
				if(step<=finalstep) {
					RT_GET_SEG(segp, ap->a_resource);
					segp->seg_stp = stp;
					STEPIN(in);
					segp->seg_out.hit_dist = segp->seg_in.hit_dist + .1; /* cope with silliness */
					BU_LIST_INSERT( &(seghead->l), &(segp->l) );
					if(segsleft <= 0){	/* exit now if we're one-hit (like visual rendering) */
						return retval;
					}
					/* reset the ray-walk shtuff */
					stat = 1;
					VSUB2(p, p, inc); 
					VSCALE(inc, rp->r_dir, step);
					step = initstep;
				} else
					STEPBACK
			}
		}
	}
#undef STEPBACK
#undef STEPIN
	return retval;
}

/**
 *  			R T _ M E T A B A L L _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_metaball_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
	struct rt_metaball_internal *mb = stp->st_specific;
	struct wdb_metaballpt *mbpt;
	vect_t v;
	fastf_t f, a;

	VSETALL(hitp->hit_normal, 0);
	for(BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_ctrl_head)){
		VSUB2(v, hitp->hit_point, mbpt->coord);
		f = mbpt->fldstr * mbpt->fldstr;	/* f^2 */
		a = MAGNITUDE(v);
		f /= (a*a*a);				/* f^2 / r^3 */
		VUNITIZE(v);						
		VJOIN1(hitp->hit_normal, hitp->hit_normal, f, v);	
	}
	VUNITIZE(hitp->hit_normal);					
	return;
}

/**
 *			R T _ M E T A B A L L _ C U R V E
 *
 *  Return the curvature of the metaball.
 */
void
rt_metaball_curve(register struct curvature *cvp, register struct hit *hitp, struct soltab *stp)
{
	bu_log("called rt_metaball_curve!\n");
	return;
}


/**
 *  			R T _ M E T A B A L L _ U V
 *
 *  For a hit on the surface of an METABALL, return the (u,v) coordinates
 *  of the hit point, 0 <= u,v <= 1.
 *  u = azimuth
 *  v = elevation
 */
void
rt_metaball_uv(struct application *ap, struct soltab *stp, register struct hit *hitp, register struct uvcoord *uvp)
{
	bu_log("called rt_metaball_uv!\n");
	return;
}

/**
 *			R T _ M E T A B A L L _ F R E E
 */
void
rt_metaball_free(register struct soltab *stp)
{
	/* seems to be unused? */
	bu_log("rt_metaball_free called\n");
	return;
}


/* I have no clue what this function is supposed to do */
int
rt_metaball_class(void)
{
	return RT_CLASSIFY_UNIMPLEMENTED;	/* "assume the worst" */
}


void
rt_metaball_plot_sph(struct bu_list *vhead, point_t *center, fastf_t radius)
{
	fastf_t top[16*3], middle[16*3], bottom[16*3];
	point_t a, b, c;
	int i;

	VSET(a, radius, 0, 0);
	VSET(b, 0, radius, 0);
	VSET(c, 0, 0, radius);
	rt_ell_16pts( top, *center, a, b );
	rt_ell_16pts( bottom, *center, b, c );
	rt_ell_16pts( middle, *center, a, c );

	RT_ADD_VLIST( vhead, &top[15*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE );
	for( i=0; i<16; i++ ) RT_ADD_VLIST( vhead, &top[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, &bottom[15*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE );
	for( i=0; i<16; i++ ) RT_ADD_VLIST( vhead, &bottom[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW );
	RT_ADD_VLIST( vhead, &middle[15*ELEMENTS_PER_VECT], BN_VLIST_LINE_MOVE );
	for( i=0; i<16; i++ ) RT_ADD_VLIST( vhead, &middle[i*ELEMENTS_PER_VECT], BN_VLIST_LINE_DRAW );
}

/**
 *			R T _ M E T A B A L L _ P L O T
 */
int
rt_metaball_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	struct rt_metaball_internal *mb;
	struct wdb_metaballpt *mbpt;
	point_t bsc;
	fastf_t rad;

	RT_CK_DB_INTERNAL(ip);
	mb = (struct rt_metaball_internal *)ip->idb_ptr;
	RT_METABALL_CK_MAGIC(mb);
	rad = rt_metaball_get_bounding_sphere(&bsc, mb->threshold, &mb->metaball_ctrl_head);
	rt_metaball_plot_sph(vhead, &bsc, rad);
	for(BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_ctrl_head))
		rt_metaball_plot_sph(vhead, &mbpt->coord, mbpt->fldstr / mb->threshold);
	return 0;
}

/**
 *			R T _ M E T A B A L L _ T E S S
 *
 *  Tessellate a metaball.
 */
int
rt_metaball_tess(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	bu_log("rt_metaball_tess called!\n");
	return -1;
}

/**
 *			R T _ M E T A B A L L _ I M P O R T 5
 *
 *  Import an metaball/sphere from the database format to the internal 
 *  structure. Apply modeling transformations as well.
 */
int
rt_metaball_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
	struct wdb_metaballpt *mbpt;
	struct rt_metaball_internal *mb;
	fastf_t *buf;
	int metaball_count = 0, i;

	BU_CK_EXTERNAL( ep );
	metaball_count = bu_glong((unsigned char *)ep->ext_buf);
	buf = (fastf_t *)bu_malloc((metaball_count*4+2)*SIZEOF_NETWORK_DOUBLE,"rt_metaball_import5: buf");
	ntohd((unsigned char *)buf, (unsigned char *)ep->ext_buf+SIZEOF_NETWORK_LONG, metaball_count*4+2);

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_METABALL;
	ip->idb_meth = &rt_functab[ID_METABALL];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_metaball_internal), "rt_metaball_internal");
	mb = (struct rt_metaball_internal *)ip->idb_ptr;
	mb->magic = RT_METABALL_INTERNAL_MAGIC;
	mb->threshold = buf[0];
	mb->method = ((long *)buf)[1];
	BU_LIST_INIT( &mb->metaball_ctrl_head );
	for(i=2 ; i<=metaball_count*4 ; i+=4) {
			/* Apply modeling transformations */
		BU_GETSTRUCT( mbpt, wdb_metaballpt );
		mbpt->l.magic = WDB_METABALLPT_MAGIC;
		MAT4X3PNT( mbpt->coord, mat, &buf[i] );
		mbpt->fldstr = buf[i+3] / mat[15];
		BU_LIST_INSERT( &mb->metaball_ctrl_head, &mbpt->l );
	}

	bu_free((genptr_t)buf, "rt_metaball_import5: buf");
	return 0;		/* OK */
}

/**
 *			R T _ M E T A B A L L _ E X P O R T 5
 *
 * storage is something like
 * long		numpoints
 * fastf_t	threshold
 * long		method
 *	fastf_t	X1	(start point)
 *	fastf_t	Y1
 *	fastf_t	Z1
 *	fastf_t	fldstr1 (end point)
 *	fastf_t	X2	(start point)
 *	...
 */
int
rt_metaball_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	struct rt_metaball_internal *mb;
	struct wdb_metaballpt *mbpt;
	int metaball_count = 0, i = 2;
	fastf_t *buf;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_METABALL )  
		return(-1);
	mb = (struct rt_metaball_internal *)ip->idb_ptr;
	RT_METABALL_CK_MAGIC(mb);
	if (mb->metaball_ctrl_head.magic == 0) return -1;

	/* Count number of points */
	for(BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_ctrl_head)) metaball_count++;

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE*(1+4*metaball_count) + 2*SIZEOF_NETWORK_LONG;
	ep->ext_buf = (genptr_t)bu_malloc(ep->ext_nbytes, "metaball external");
	bu_plong((unsigned char *)ep->ext_buf, metaball_count);

	/* pack the point data */
	buf = (fastf_t *)bu_malloc((metaball_count*4+1)*SIZEOF_NETWORK_DOUBLE,"rt_metaball_export5: buf");
	buf[0] = mb->threshold;
	((long *)buf)[1] = mb->method;
	for(BU_LIST_FOR( mbpt, wdb_metaballpt, &mb->metaball_ctrl_head), i+=4){
		VSCALE(&buf[i], mbpt->coord, local2mm);
		buf[i+3] = mbpt->fldstr * local2mm;
	}
	htond((unsigned char *)ep->ext_buf + SIZEOF_NETWORK_LONG, (unsigned char *)buf, 2 + 4 * metaball_count);
	bu_free(buf,"rt_metaball_export5: buf");

	return 0;
}

/**
 *			R T _ M E T A B A L L _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid. First line 
 *  describes type of solid. Additional lines are indented one tab, and give 
 *  parameter values.
 */
int
rt_metaball_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
	int metaball_count = 0;
	char buf[BUFSIZ];
	struct rt_metaball_internal *mb;
	struct wdb_metaballpt *mbpt;

	RT_CK_DB_INTERNAL(ip);
	mb = (struct rt_metaball_internal *)ip->idb_ptr;
	RT_METABALL_CK_MAGIC(mb);
	for(BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_ctrl_head)) metaball_count++;

	snprintf(buf, BUFSIZ, "Metaball with %d points and a threshold of %g (method %d)\n", metaball_count, mb->threshold, mb->method);
	bu_vls_strcat(str,buf);
	if(!verbose)return 0;
	metaball_count=0;
	for( BU_LIST_FOR( mbpt, wdb_metaballpt, &mb->metaball_ctrl_head)){
		snprintf(buf,BUFSIZ,"\t%d: %g field strength at (%g, %g, %g)\n",
			++metaball_count, mbpt->fldstr, V3ARGS(mbpt->coord));
		bu_vls_strcat(str,buf);
	}
	return 0;
}

/**
 *			R T _ M E T A B A L L _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 *  This only effects the in-memory copy.
 */
void
rt_metaball_ifree(struct rt_db_internal *ip)
{
	register struct rt_metaball_internal	*metaball;
	register struct wdb_metaballpt	*mbpt;

	RT_CK_DB_INTERNAL(ip);
	metaball = (struct rt_metaball_internal*)ip->idb_ptr;
	RT_METABALL_CK_MAGIC(metaball);

	if (metaball->metaball_ctrl_head.magic != 0)
		while(BU_LIST_WHILE(mbpt, wdb_metaballpt, &metaball->metaball_ctrl_head)) {
			BU_LIST_DEQUEUE(&(mbpt->l));
			bu_free((char *)mbpt, "wdb_metaballpt");
		}
	bu_free( ip->idb_ptr, "metaball ifree" );
	ip->idb_ptr = GENPTR_NULL;
}

/**
 *			R T _ M E T A B A L L _ T N U R B
 */
int
rt_metaball_tnurb(struct nmgregion **r, struct model *m, struct rt_db_internal *ip, const struct bn_tol *tol)
{
	bu_log("rt_metaball_tnurb called!\n");
	return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
