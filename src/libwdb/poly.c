/*                          P O L Y . C
 * BRL-CAD
 *
 * Copyright (c) 1987-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file poly.c
 *
 *  The interface to these polygon routines is likely to evaporate.
 *  Consider using NMGs instead.
 *
 *  Return codes of 0 are OK, -1 signal an error.
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
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"


#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "bu.h"
#include "db.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

/*
 *			M K _ P O L Y S O L I D
 *
 *  Make the header record for a polygon solid.
 *  Must be followed by 1 or more mk_poly() or mk_fpoly() calls
 *  before any other mk_* routines.
 */
int
mk_polysolid( fp, name )
FILE	*fp;
char	*name;
{
	union record rec;

	/* In v5, the caller should be using BoT solids */
	BU_ASSERT_LONG( mk_version, <=, 4 );

	bzero( (char *)&rec, sizeof(rec) );
	rec.p.p_id = ID_P_HEAD;
	NAMEMOVE( name, rec.p.p_name );
	if( fwrite( (char *)&rec, sizeof(rec), 1, fp ) != 1 )
		return(-1);
	return(0);
}

/*
 *			M K _ P O L Y
 *
 *  Must follow a call to mk_polysolid(), mk_poly(), or mk_fpoly().
 */
int
mk_poly( fp, npts, verts, norms )
FILE	*fp;
int	npts;
fastf_t	verts[][3];
fastf_t	norms[][3];
{
	union record rec;
	register int i,j;

	if( npts < 3 || npts > 5 )  {
		fprintf(stderr,"mk_poly:  npts=%d is bad\n", npts);
		return(-1);
	}

	/* In v5, the caller should be using BoT solids */
	BU_ASSERT_LONG( mk_version, <=, 4 );

	bzero( (char *)&rec, sizeof(rec) );
	rec.q.q_id = ID_P_DATA;
	rec.q.q_count = npts;
	for( i=0; i<npts; i++ )  {
		for( j=0; j<3; j++ )  {
			rec.q.q_verts[i][j] = verts[i][j] * mk_conv2mm;
			rec.q.q_norms[i][j] = norms[i][j] * mk_conv2mm;
		}
	}
	if( fwrite( (char *)&rec, sizeof(rec), 1, fp ) != 1)
		return(-1);
	return(0);
}

/*
 *			M K _ F P O L Y
 *
 *  "Fast" version -- will construct surface normal from vertices.
 *  Vertices must be listed in counter-clockwise (CCW) order,
 *  to generate a proper outward-pointing normal.
 *
 *  Must follow a call to mk_polysolid(), mk_poly(), or mk_fpoly().
 */
int
mk_fpoly( fp, npts, verts )
FILE	*fp;
int	npts;
fastf_t	verts[][3];
{
	int	i;
	vect_t	v1, v2, norms[5];

	if( npts < 3 || npts > 5 )  {
		fprintf(stderr,"mk_poly:  npts=%d is bad\n", npts);
		return(-1);
	}

	/* In v5, the caller should be using BoT solids */
	BU_ASSERT_LONG( mk_version, <=, 4 );

	VSUB2( v1, verts[1], verts[0] );
	VSUB2( v2, verts[npts-1], verts[0] );
	VCROSS( norms[0], v1, v2 );
	VUNITIZE( norms[0] );
	for( i = 1; i < npts; i++ ) {
		VMOVE( norms[i], norms[0] );
	}
	return( mk_poly(fp, npts, verts, norms) );
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
