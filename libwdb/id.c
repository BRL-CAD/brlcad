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
 *			M K _ I D _ U N I T S
 *
 *  Make a database header (ID) record, and note the
 *  user's preferred editing units.
 *
 *  Returns -
 *	<0	error
 *	0	success
 */
int
mk_id_units( fp, title, units )
FILE		*fp;
CONST char	*title;
register CONST char	*units;
{
	union record rec;
	int	code;

	bzero( (char *)&rec, sizeof(rec) );
	rec.i.i_id = ID_IDENT;

	if( (code = db_v4_get_units_code(units)) < 0 )
		return -2;		/* ERROR */
	rec.i.i_units = code;

	strncpy( rec.i.i_version, ID_VERSION, sizeof(rec.i.i_version) );
	strncpy( rec.i.i_title, title, sizeof(rec.i.i_title) );
	if( fwrite( (char *)&rec, sizeof(rec), 1, fp ) != 1 )
		return(-1);
	return(0);
}

/* Should there be a routine which takes a local2mm arg as well? */
