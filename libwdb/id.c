/*
 *			I D . C
 *
 *  An ID record must be the first granule in the database.
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1987 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "db.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

/*
 *			M K _ I D
 *
 *  Make a database header (ID) record.
 */
int
mk_id( fp, title )
FILE		*fp;
CONST char	*title;
{
	return mk_id_units( fp, title, "mm" );
}

/*
 *			M K _ I D
 *
 *  Make a database header (ID) record, and note the
 *  user's preferred editing units.
 *  Alas, the current database format does not have many choices.
 */
int
mk_id_units( fp, title, units )
FILE		*fp;
CONST char	*title;
register CONST char	*units;
{
	union record rec;

	bzero( (char *)&rec, sizeof(rec) );
	rec.i.i_id = ID_IDENT;

	if( strcmp( units, "none" ) == 0 )  {
		rec.i.i_units = ID_NO_UNIT;
	} else if( strcmp( units, "mm" ) == 0 )  {
		rec.i.i_units = ID_MM_UNIT;
	} else if( strcmp( units, "cm" ) == 0 )  {
		rec.i.i_units = ID_CM_UNIT;
	} else if( strcmp( units, "m" ) == 0 )  {
		rec.i.i_units = ID_M_UNIT;
	} else if( strcmp( units, "in" ) == 0 )  {
		rec.i.i_units = ID_IN_UNIT;
	} else if( strcmp( units, "ft" ) == 0 )  {
		rec.i.i_units = ID_FT_UNIT;
	} else {
		return -2;
	}

	strncpy( rec.i.i_version, ID_VERSION, sizeof(rec.i.i_version) );
	strncpy( rec.i.i_title, title, sizeof(rec.i.i_title) );
	if( fwrite( (char *)&rec, sizeof(rec), 1, fp ) != 1 )
		return(-1);
	return(0);
}
