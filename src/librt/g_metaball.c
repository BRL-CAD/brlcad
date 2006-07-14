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
  return 0;			/* OK */
}

/**
 *			R T _ M E T A B A L L _ P R I N T
 */
void
rt_metaball_print(register const struct soltab *stp)
{
    return;
}

/*
 *  			R T _ M E T A B A L L _ S H O T
 */
int
rt_metaball_shot(struct soltab *stp, register struct xray *rp, struct application *ap, struct seg *seghead)
{
  return 1;
}

/**
 *  			R T _ M E T A B A L L _ N O R M
 *
 *  Given ONE ray distance, return the normal and entry/exit point.
 */
void
rt_metaball_norm(register struct hit *hitp, struct soltab *stp, register struct xray *rp)
{
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
    return;
}


int
rt_metaball_class(void)
{
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
	return;
}

/**
 *			R T _ M E T A B A L L _ P L O T
 */
int
rt_metaball_plot(struct bu_list *vhead, struct rt_db_internal *ip, const struct rt_tess_tol *ttol, const struct bn_tol *tol)
{
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
 *			R T _ M E T A B A L L _ I M P O R T
 *
 *  Import an metaball/sphere from the database format to
 *  the internal structure.
 *  Apply modeling transformations as wmetaball.
 */
int
rt_metaball_import(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
	return 0;		/* OK */
}

/**
 *			R T _ M E T A B A L L _ E X P O R T
 */
int
rt_metaball_export(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	return 0;
}

/**
 *			R T _ M E T A B A L L _ I M P O R T 5
 *
 *  Import an metaball/sphere from the database format to
 *  the internal structure.
 *  Apply modeling transformations as wmetaball.
 */
int
rt_metaball_import5(struct rt_db_internal *ip, const struct bu_external *ep, register const fastf_t *mat, const struct db_i *dbip)
{
	return 0;		/* OK */
}

/**
 *			R T _ M E T A B A L L _ E X P O R T 5
 *
 *  The external format is:
 *	V point
 *	A vector
 *	B vector
 *	C vector
 */
int
rt_metaball_export5(struct bu_external *ep, const struct rt_db_internal *ip, double local2mm, const struct db_i *dbip)
{
	return 0;
}

/**
 *			R T _ M E T A B A L L _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this solid.
 *  First line describes type of solid.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_metaball_describe(struct bu_vls *str, const struct rt_db_internal *ip, int verbose, double mm2local)
{
	return 0;
}

/**
 *			R T _ M E T A B A L L _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
 */
void
rt_metaball_ifree(struct rt_db_internal *ip)
{
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
