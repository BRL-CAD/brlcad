/*                    G _ M E T A B A L L . C
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

/** \addtogroup g */

/*@{*/
/** @file g_metaball.c
 * Intersect a ray with a metaball implicit surface.
 *
 *      NOTICE: this primitive is incomplete and should beconsidered
 *              experimental.  this primitive will exhibit several
 *              instabilities in the existing root solver method.
 *
 *
 *  Authors -
 *      Erik Greenwald <erikg@arl.army.mil>
 *
 *  Source -
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
 *
 */


/**
 *  			R T _ M E T A B A L L _ P R E P
 */
int
rt_metaball_prep(struct soltab *stp, struct rt_db_internal *ip, struct rt_i *rtip)
{
	struct rt_metaball_internal *mb, *nmb;
	struct wdb_metaballpt *mbpt, *nmbpt;
	
	mb = ip->idb_ptr;
	RT_METABALL_CK_MAGIC(mb);
	VSET(stp->st_min, -4, -4, -4);
	VSET(stp->st_max, 4, 4, 4);
	VSET(stp->st_center, 0, 0, 0);
	stp->st_aradius = 4;
	stp->st_bradius = 4;
	nmb = bu_malloc(sizeof(struct rt_metaball_internal), "rt_metaball_prep: nmb");
	BU_LIST_INIT( &nmb->metaball_pt_head );
	nmb->threshhold = mb->threshhold;
	for(BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_pt_head)){
		nmbpt = (struct wdb_metaballpt *)bu_malloc(sizeof(struct wdb_metaballpt), "rt_metaball_prep: nmbpt");
		nmbpt->fldstr = mbpt->fldstr;
		VMOVE(nmbpt->coord,mbpt->coord);
		BU_LIST_INSERT( &nmb->metaball_pt_head, &nmbpt->l );
	}
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

inline HIDDEN fastf_t
rt_metaball_point_value(point_t *p, struct bu_list *points)
{
	struct wdb_metaballpt *mbpt;
	struct rt_metaball_internal *mb;
	fastf_t ret = 0.0;
	point_t v;
	int metaball_count = 0;

	for(BU_LIST_FOR(mbpt, wdb_metaballpt, points)) {
		VSUB2(v, mbpt->coord, *p);
		ret += mbpt->fldstr / MAGSQ(v);
	}
	return ret;
}

/*
 *  			R T _ M E T A B A L L _ S H O T
 */
int
rt_metaball_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
	struct rt_metaball_internal *mb;
	point_t p, inc;
#define STEP .05
	fastf_t i = (rp->r_max-rp->r_min)/STEP;
	int stat=0;
	struct wdb_metaballpt *mbpt;
	register struct seg *segp = NULL;

	mb = (struct rt_metaball_internal *)stp->st_specific;
	VMOVE(p, rp->r_pt);
	VSCALE(inc, rp->r_dir, STEP); /* assume it's normalized and we want to creep at STEP */

	while( (i-=STEP) > 0.0 ) {
		VADD2(p, p, inc);
		if(stat) {
			if(rt_metaball_point_value(&p, &mb->metaball_pt_head) < mb->threshhold ) {
				segp->seg_out.hit_dist = (rp->r_max-rp->r_min)/STEP - i;
				BU_LIST_INSERT( &(seghead->l), &(segp->l) );
				return 2;		/* hit */
			}
		} else {
			if(rt_metaball_point_value(&p, &mb->metaball_pt_head) > mb->threshhold ) {
				RT_GET_SEG(segp, ap->a_resource);
				segp->seg_stp = stp;
				segp->seg_in.hit_dist = (rp->r_max-rp->r_min)/STEP - i;
				++stat;
			}
		}
	}
#undef STEP
	return 0; /* miss */
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
	fastf_t f;

	for(BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_pt_head)){
		VSUB2(v, mbpt->coord, hitp->hit_point);
		f = mbpt->fldstr / MAGSQ(v);
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


int
rt_metaball_class(void)
{
	bu_log("rt_metaball_class called\n");
	return 0;
}


/**
 *			R T _ M E T A B A L L _ 1 6 P T S
 *
 * Also used by the TGC code
 */
#define METABALLOUT(n)	ov+(n-1)*3
void
rt_metaball_16pts(register fastf_t *ov, register fastf_t *V, fastf_t *A, fastf_t *B)
{
	bu_log("rt_metaball_16pts called\n");
	return;
}

/**
 *			R T _ M E T A B A L L _ P L O T
 */
int
rt_metaball_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
	bu_log("rt_metaball_plot called\n");
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
	fastf_t *buf, threshhold;
	int metaball_count = 0, i;

	BU_CK_EXTERNAL( ep );
	metaball_count = bu_glong((unsigned char *)ep->ext_buf);
	buf = (fastf_t *)bu_malloc((metaball_count*4+1)*SIZEOF_NETWORK_DOUBLE,"rt_metaball_import5: buf");
	ntohd((unsigned char *)buf, (unsigned char *)ep->ext_buf+SIZEOF_NETWORK_LONG, metaball_count*4+1);

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BRLCAD;
	ip->idb_type = ID_METABALL;
	ip->idb_meth = &rt_functab[ID_METABALL];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_metaball_internal), "rt_metaball_internal");
	mb = (struct rt_metaball_internal *)ip->idb_ptr;
	mb->magic = RT_METABALL_INTERNAL_MAGIC;
	mb->threshhold = buf[0];
	BU_LIST_INIT( &mb->metaball_pt_head );
	for(i=1 ; i<=metaball_count*4 ; i+=4) {
			/* Apply modeling transformations */
		BU_GETSTRUCT( mbpt, wdb_metaballpt );
		mbpt->l.magic = WDB_METABALLPT_MAGIC;
		MAT4X3PNT( mbpt->coord, mat, &buf[i] );
		mbpt->fldstr = buf[i+3] / mat[15];
		BU_LIST_INSERT( &mb->metaball_pt_head, &mbpt->l );
	}

	bu_free((genptr_t)buf, "rt_metaball_import5: buf");
	return 0;		/* OK */
}

/**
 *			R T _ M E T A B A L L _ E X P O R T 5
 *
 * storage is something like
 * long		numpoints
 * fastf_t	threshhold
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
	int metaball_count = 0, i = 1;
	fastf_t *buf;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != ID_METABALL )  
	    return(-1);
	mb = (struct rt_metaball_internal *)ip->idb_ptr;
	RT_METABALL_CK_MAGIC(mb);
	if (mb->metaball_pt_head.magic == 0) return -1;

	/* Count number of points */
	for(BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_pt_head)) metaball_count++;

	BU_CK_EXTERNAL(ep);
	ep->ext_nbytes = SIZEOF_NETWORK_DOUBLE*(1+4*metaball_count) + SIZEOF_NETWORK_LONG;
	ep->ext_buf = (genptr_t)bu_malloc(ep->ext_nbytes, "metaball external");
	bu_plong((unsigned char *)ep->ext_buf, metaball_count);

	/* pack the point data */
	buf = (fastf_t *)bu_malloc((metaball_count*4+1)*SIZEOF_NETWORK_DOUBLE,"rt_metaball_export5: buf");
	buf[0] = mb->threshhold;
	for(BU_LIST_FOR( mbpt, wdb_metaballpt, &mb->metaball_pt_head), i+=4){
		VSCALE(&buf[i], mbpt->coord, local2mm);
		buf[i+3] = mbpt->fldstr * local2mm;
	}
	htond((unsigned char *)ep->ext_buf + SIZEOF_NETWORK_LONG, (unsigned char *)buf, 1 + 4 * metaball_count);
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
	for(BU_LIST_FOR(mbpt, wdb_metaballpt, &mb->metaball_pt_head)) metaball_count++;

	snprintf(buf, BUFSIZ, "Metaball with %d points and a threshhold of %g\n", metaball_count, mb->threshhold);
	bu_vls_strcat(str,buf);
	if(!verbose)return 0;
	metaball_count=0;
	for( BU_LIST_FOR( mbpt, wdb_metaballpt, &mb->metaball_pt_head)){
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

	if (metaball->metaball_pt_head.magic != 0)
	    while(BU_LIST_WHILE(mbpt, wdb_metaballpt, &metaball->metaball_pt_head))  {
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
