/*
 *			A R S . C
 *
 *  libwdb support for writing an ARS.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
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
#include "wdb.h"

#ifdef SYSV
#define bzero(str,n)		memset( str, '\0', n )
#define bcopy(from,to,count)	memcpy( to, from, count )
#endif

/*
 *			M K _ A R S
 *
 *  The input is an array of pointers to an array of fastf_t values.
 *  There is one pointer for each curve.
 *  It is anticipated that there will be pts_per_curve+1 elements per
 *  curve, the first point being repeated as the final point, although
 *  this is not checked here.
 *
 *  Returns -
 *	 0	OK
 *	-1	Fail
 */
int
mk_ars( filep, name, ncurves, pts_per_curve, curves )
FILE	*filep;
char	*name;
int	ncurves;
int	pts_per_curve;
fastf_t	**curves;
{
	union record	rec;
	vect_t		base_pt;
	int		per_curve_grans;
	int		cur;

	bzero( (char *)&rec, sizeof(rec) );

	rec.a.a_id = ID_ARS_A;
	rec.a.a_type = ARS;			/* obsolete? */
	NAMEMOVE( name, rec.a.a_name );
	rec.a.a_m = ncurves;
	rec.a.a_n = pts_per_curve;

	per_curve_grans = (pts_per_curve+7)/8;
	rec.a.a_curlen = per_curve_grans;
	rec.a.a_totlen = per_curve_grans * ncurves;
	if( fwrite( (char *)&rec, sizeof(rec), 1, filep ) != 1 )
		return(-1);

	VMOVE( base_pt, &curves[0][0] );
	/* The later subtraction will "undo" this, leaving just base_pt */
	VADD2( &curves[0][0], &curves[0][0], base_pt);

	for( cur=0; cur<ncurves; cur++ )  {
		register fastf_t	*fp;
		int			npts;
		int			left;

		fp = curves[cur];
		left = pts_per_curve;
		for( npts=0; npts < pts_per_curve; npts+=8, left -= 8 )  {
			register int	el;
			register int	lim;

			bzero( (char *)&rec, sizeof(rec) );
			rec.b.b_id = ID_ARS_B;
			rec.b.b_type = ARSCONT;	/* obsolete? */
			rec.b.b_n = cur+1;	/* obsolete? */
			rec.b.b_ngranule = (npts/8)+1; /* obsolete? */

			lim = (left > 8 ) ? 8 : left;
			for( el=0; el < lim; el++ )  {
				vect_t	diff;
				VSUB2SCALE( diff, fp, base_pt, mk_conv2mm );
				/* XXX converts to dbfloat_t */
				VMOVE( &(rec.b.b_values[el*3]), diff );
				fp += ELEMENTS_PER_VECT;
			}
			if( fwrite( (char *)&rec, sizeof(rec), 1, filep ) != 1 )
				return(-1);
		}
	}
	return(0);
}
