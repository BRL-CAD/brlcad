/*
 *			S P L . C
 *
 *  Library for writing spline objects into
 *  MGED databases from arbitrary procedures.
 *
 *  It is expected that this library will grow as experience is gained.
 *
 *  Authors -
 *	Michael John Muuss
 *	Paul R. Stay
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "db.h"
#include "vmath.h"

#include "nurb.h"
#define B_SPLINE_DEFINED 1		/* currently wdb.h needs this */

#include "wdb.h"

#ifdef SYSV
#define bzero(str,n)		memset( str, '\0', n )
#define bcopy(from,to,count)	memcpy( to, from, count )
#endif

/*
 *			M K _ B S O L I D
 *
 *  Write the header record for a B-spline solid, which is
 *  composed of several B-spline surfaces.
 *  The call to this routine must be immediately followed by
 *  the appropriate number of calls to mk_bsurf().
 */
int
mk_bsolid( fp, name, nsurf, res )
FILE	*fp;
char	*name;
int	nsurf;
double	res;
{
	union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.d.d_id = ID_BSOLID;
	NAMEMOVE( name, rec.B.B_name );
	rec.B.B_nsurf = nsurf;
	rec.B.B_resolution = res;

	if( fwrite( (char *)&rec, sizeof(rec), 1, fp ) != 1 )
		return(-1);
	return(0);
}

/*
 *			M K _ B S U R F
 *
 *  Write the external MGED description of a single B-spline surface
 *  given it's internal form from b_spline.h
 */

int
mk_bsurf( filep, srf )
FILE	* filep;
struct snurb * srf;
{
	union record rec;
	dbfloat_t	*kp;		/* knot vector area */
	dbfloat_t	*mp;		/* mesh area */
	register dbfloat_t	*dbp;
	register fastf_t	*fp;
	register int	i;
	int		n;

	if( srf->u_knots->k_size != srf->mesh->s_size[COL] + srf->order[0] ||
	    srf->v_knots->k_size != srf->mesh->s_size[ROW] + srf->order[1]) {
	    	fprintf(stderr,"mk_bsurf:  mis-matched knot/mesh/order\n");
	    	return(-1);
	}

	bzero( (char *)&rec, sizeof(rec) );
	rec.d.d_id = ID_BSURF;

	n = srf->u_knots->k_size + srf->v_knots->k_size;
	n = ((n * sizeof(dbfloat_t)) + sizeof(rec)-1) / sizeof(rec);
	kp = (dbfloat_t *)malloc(n*sizeof(rec));
	bzero( (char *)kp, n*sizeof(rec) );
	rec.d.d_nknots = n;
	rec.d.d_order[ROW] = srf->order[ROW];	/* [0] */
	rec.d.d_order[COL] = srf->order[COL];	/* [1] */
	rec.d.d_kv_size[ROW] = srf->u_knots->k_size;
	rec.d.d_kv_size[COL] = srf->v_knots->k_size;

	n = srf->mesh->s_size[ROW] * srf->mesh->s_size[COL] *
	    EXTRACT_COORDS(srf->mesh->pt_type);
	n = ((n * sizeof(dbfloat_t)) + sizeof(rec)-1) / sizeof(rec);
	mp = (dbfloat_t *)malloc(n*sizeof(rec));
	bzero( (char *)mp, n*sizeof(rec) );
	rec.d.d_nctls = n;
	rec.d.d_geom_type = EXTRACT_COORDS(srf->mesh->pt_type);
	rec.d.d_ctl_size[ROW] = srf->mesh->s_size[ROW];
	rec.d.d_ctl_size[COL] = srf->mesh->s_size[COL];

	/* Reformat the knot vectors */
	dbp = kp;
	for( i=0; i<srf->u_knots->k_size; i++ )
		*dbp++ = srf->u_knots->knots[i];
	for( i=0; i<srf->v_knots->k_size; i++ )
		*dbp++ = srf->v_knots->knots[i];

	/* Reformat the mesh */
	dbp = mp;
	fp = srf->mesh->ctl_points;
	i = srf->mesh->s_size[ROW] * srf->mesh->s_size[COL] *
	    EXTRACT_COORDS(srf->mesh->pt_type);	/* # floats/point */
	for( ; i>0; i-- )
		*dbp++ = *fp++ * mk_conv2mm;

	if( fwrite( (char *)&rec, sizeof(rec), 1, filep ) != 1 ||
	    fwrite( (char *)kp, sizeof(rec), rec.d.d_nknots, filep ) != rec.d.d_nknots ||
	    fwrite( (char *)mp, sizeof(rec), rec.d.d_nctls, filep ) != rec.d.d_nctls )  {
		free( (char *)kp );
		free( (char *)mp );
	    	return(-1);
	}

	free( (char *)kp );
	free( (char *)mp );
	return(0);
}
