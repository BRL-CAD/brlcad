/*                          R E G 4 . C
 * BRL-CAD
 *
 * Copyright (C) 1987-2005 United States Government as represented by
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
/** @file reg4.c
 *
 *  WARNING:  Old leftover routines that only work with v4 databases.
 *
 *
 *
 *  Library for writing MGED databases from arbitrary procedures.
 *
 *  This module contains routines to create combinations, and regions.
 *
 *  It is expected that this library will grow as experience is gained.
 *
 *  Authors -
 *	Michael John Muuss
 *	Paul R. Stay
 *	Bill Mermagen Jr.
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
#include "raytrace.h"
#include "wdb.h"

/* -------------------- Begin old code, compat only -------------------- */

static float ident_mat[16] = {
	1.0, 0.0, 0.0, 0.0,
	0.0, 1.0, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.0, 0.0, 0.0, 1.0
};

/*
 *			M K _ C O M B
 *
 *  Make a combination with material properties info.
 *  Must be followed by 'len' mk_memb() calls before any other mk_* routines.
 *
 *  XXX WARNING:  This routine should not be called by new code.
 *  XXX use mk_lcomb() instead.
 */
int
mk_comb( fp, name, len, region, matname, matparm, rgb, inherit )
FILE			*fp;
const char		*name;
int			len;
int			region;
const char		*matname;
const char		*matparm;
const unsigned char	*rgb;
int			inherit;
{
	union record rec;

	BU_ASSERT_LONG( mk_version, <=, 4 );

	bzero( (char *)&rec, sizeof(rec) );
	rec.c.c_id = ID_COMB;
	/* XXX What values to pass for FASTGEN plate and volume regions? */
	if( region )
		rec.c.c_flags = DBV4_REGION;
	else
		rec.c.c_flags = DBV4_NON_REGION;
	NAMEMOVE( name, rec.c.c_name );
	rec.c.c_pad1 = len;		/* backwards compat, was c_length */
	if( matname ) {
		strncpy( rec.c.c_matname, matname, sizeof(rec.c.c_matname) );
		if( matparm )
			strncpy( rec.c.c_matparm, matparm,
				sizeof(rec.c.c_matparm) );
	}
	if( rgb )  {
		rec.c.c_override = 1;
		rec.c.c_rgb[0] = rgb[0];
		rec.c.c_rgb[1] = rgb[1];
		rec.c.c_rgb[2] = rgb[2];
	}
	rec.c.c_inherit = inherit;
	if( fwrite( (char *)&rec, sizeof(rec), 1, fp ) != 1 )
		return(-1);
	return(0);
}

/*
 *			M K _ R C O M B
 *
 *  Make a combination with material properties info.
 *  Must be followed by 'len' mk_memb() calls before any other mk_* routines.
 *  Like mk_comb except for additional region parameters.
 *
 *  XXX WARNING:  This routine should not be called by new code.
 *  XXX use mk_addmember() instead.
 */
int
mk_rcomb( fp, name, len, region, matname, matparm, rgb, id, air, material, los, inherit )
FILE		*fp;
const char	*name;
int		len;
int		region;
const char	*matname;
const char	*matparm;
const unsigned char	*rgb;
int		id;
int		air;
int		material;
int		los;
int		inherit;
{
	union record rec;

	BU_ASSERT_LONG( mk_version, <=, 4 );

	bzero( (char *)&rec, sizeof(rec) );
	rec.c.c_id = ID_COMB;
	if( region ){
		switch( region )  {
		case DBV4_NON_REGION:	/* sanity, fixes a non-bool arg */
		case DBV4_REGION:
		case DBV4_REGION_FASTGEN_PLATE:
		case DBV4_REGION_FASTGEN_VOLUME:
			rec.c.c_flags = region;
			break;
		default:
			rec.c.c_flags = DBV4_REGION;
		}
		rec.c.c_inherit = inherit;
		rec.c.c_regionid = id;
		rec.c.c_aircode = air;
		rec.c.c_material = material;
		rec.c.c_los = los;
	}
	else
		rec.c.c_flags = DBV4_NON_REGION;
	NAMEMOVE( name, rec.c.c_name );
	rec.c.c_pad1 = len;		/* backwards compat, was c_length */
	if( matname ) {
		strncpy( rec.c.c_matname, matname, sizeof(rec.c.c_matname) );
		if( matparm )
			strncpy( rec.c.c_matparm, matparm,
				sizeof(rec.c.c_matparm) );
	}
	if( rgb )  {
		rec.c.c_override = 1;
		rec.c.c_rgb[0] = rgb[0];
		rec.c.c_rgb[1] = rgb[1];
		rec.c.c_rgb[2] = rgb[2];
	}

	if( fwrite( (char *)&rec, sizeof(rec), 1, fp ) != 1 )
		return(-1);
	return(0);
}


/*
 *			M K _ F C O M B
 *
 *  Make a simple combination header ("fast" version).
 *  Must be followed by 'len' mk_memb() calls before any other mk_* routines.
 *
 *  XXX WARNING:  This routine should not be called by new code.
 */
int
mk_fcomb( fp, name, len, region )
FILE		*fp;
const char	*name;
int		len;
int		region;
{
	union record rec;

	BU_ASSERT_LONG( mk_version, <=, 4 );

	bzero( (char *)&rec, sizeof(rec) );
	rec.c.c_id = ID_COMB;
	if( region )
		rec.c.c_flags = DBV4_REGION;
	else
		rec.c.c_flags = DBV4_NON_REGION;
	NAMEMOVE( name, rec.c.c_name );
	rec.c.c_pad1 = len;		/* backwards compat, was c_length */
	if( fwrite( (char *)&rec, sizeof(rec), 1, fp ) != 1 )
		return(-1);
	return(0);
}

/*
 *			M K _ M E M B
 *
 *  Must be part of combination/member clump of records.
 *
 *  XXX WARNING:  This routine should not be called by new code.
 */
int
mk_memb( fp, name, mat, bool_op )
FILE		*fp;
const char	*name;
const mat_t	mat;
int		bool_op;
{
	union record rec;
	register int i;

	BU_ASSERT_LONG( mk_version, <=, 4 );

	bzero( (char *)&rec, sizeof(rec) );
	rec.M.m_id = ID_MEMB;
	NAMEMOVE( name, rec.M.m_instname );
	if( bool_op )
		rec.M.m_relation = bool_op;
	else
		rec.M.m_relation = UNION;
	if( mat ) {
		for( i=0; i<16; i++ )
			rec.M.m_mat[i] = mat[i];  /* double -> float */
	} else {
		for( i=0; i<16; i++ )
			rec.M.m_mat[i] = ident_mat[i];
	}
	if( fwrite( (char *)&rec, sizeof(rec), 1, fp ) != 1 )
		return(-1);
	return(0);
}

/* -------------------- End old code, compat only -------------------- */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
