/*
 *			A R B N . C
 *
 *  libwdb support for writing an ARBN.
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

#ifdef SYSV
#define bzero(str,n)		memset( str, '\0', n )
#define bcopy(from,to,count)	memcpy( to, from, count )
#endif

/*
 *			M K _ A R B N
 *
 *  Returns -
 *	-1	error
 *	 0	OK
 */
int
mk_arbn( filep, name, neqn, eqn )
FILE	*filep;
char	*name;
int	neqn;
plane_t	*eqn;
{
	union record	rec;
	int		bytes;
	unsigned char	*buf;

	if( neqn <= 0 )  return(-1);

	bzero( (char *)&rec, sizeof(rec) );

	rec.n.n_id = DBID_ARBN;
	NAMEMOVE( name, rec.n.n_name );
	rec.n.n_neqn = neqn;

	/* The network format for a double is 8 bytes and there are 4
	 * doubles per plane equation.
	 */
	rec.n.n_grans = (neqn * 8 * 4 + sizeof(union record)-1 ) /
		sizeof(union record);
	bytes = rec.n.n_grans * sizeof(union record);
	if( (buf = (unsigned char *)malloc(bytes)) == (unsigned char *)0 )
		return(-1);

	htond( buf, (char *)eqn, neqn * 4 );

	if( fwrite( (char *)&rec, sizeof(rec), 1, filep ) != 1 ||
	    fwrite( buf, bytes, 1, filep ) != 1 )  {
		free( buf );
		return(-1);
	}
	free( buf );
	return(0);			/* OK */
}
