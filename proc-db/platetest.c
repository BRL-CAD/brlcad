/*
 *			P L A T E T E S T . C
 *
 *  Program to generate test fgp mode solids.
 *
 *  Author -
 *	John Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1999 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "externs.h"
#include "vmath.h"
#include "rtlist.h"
#include "wdb.h"

main( argc, argv )
int argc;
char *argv[];
{
	point_t min, max;

	mk_id( stdout, "Plate mode test" );

	/* first an rpp */
	VSET( min, 10.0, 20.0, 30.0 );
	VSET( max, 40.0, 50.0, 60.0 );
	mk_rpp( stdout, "an_rpp", min, max  );
	mk_rpp( stdout, "another_rpp", min, max  );

	mk_fgp( stdout, "fgp.1", "an_rpp", 5.0, 1 );
	mk_fgp( stdout, "fgp.2", "another_rpp", 5.0, 2 );
}
