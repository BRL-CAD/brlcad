/*
 *			D B _ P A T H . C
 *
 *
 * Functions -
 *
 *
 *  Authors -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"

#include "./debug.h"

/*
 *			D B _ A D D _ N O D E _ T O _ F U L L _ P A T H
 */
void
db_add_node_to_full_path( pp, dp )
register struct db_full_path	*pp;
register struct directory	*dp;
{
	if( pp->fp_maxlen <= 0 )  {
		pp->fp_maxlen = 32;
		pp->fp_names = (struct directory **)rt_malloc(
			pp->fp_maxlen * sizeof(struct directory *),
			"db_full_path array");
	} else if( pp->fp_len >= pp->fp_maxlen )  {
		pp->fp_maxlen *= 4;
		pp->fp_names = (struct directory **)rt_realloc(
			(char *)pp->fp_names,
			pp->fp_maxlen * sizeof(struct directory *),
			"enlarged db_full_path array");
	}
	pp->fp_names[pp->fp_len++] = dp;
}

/*
 *			D B _ D U P _ F U L L _ P A T H
 */
void
db_dup_full_path( newp, oldp )
register struct db_full_path		*newp;
register CONST struct db_full_path	*oldp;
{
	RT_CK_FULL_PATH(newp);
	RT_CK_FULL_PATH(oldp);

	newp->fp_maxlen = oldp->fp_maxlen;
	newp->fp_len = oldp->fp_len;
	if( oldp->fp_len <= 0 )  {
		newp->fp_names = (struct directory **)0;
		return;
	}
	newp->fp_names = (struct directory **)rt_malloc(
		newp->fp_maxlen * sizeof(struct directory *),
		"duplicate full path array" );
	bcopy( (char *)oldp->fp_names, (char *)newp->fp_names,
		newp->fp_len * sizeof(struct directory *) );
}

/*
 *			D B _ P A T H _ T O _ S T R I N G
 *
 *  Unlike rt_path_str(), this version can be used in parallel.
 *  Caller is responsible for freeing the returned buffer.
 */
char *
db_path_to_string( pp )
register CONST struct db_full_path	*pp;
{
	register char	*cp;
	char	*buf;
	int	len;
	int	i;

	len = 3;	/* leading slash, trailing null, spare */
	for( i=pp->fp_len-1; i >= 0; i-- )  {
		if( pp->fp_names[i] )
			len += strlen( pp->fp_names[i]->d_namep ) + 1;
		else
			len += 16;
	}

	buf = rt_malloc( len, "pathname string" );
	cp = buf;

	for( i=0; i < pp->fp_len; i++ )  {
		*cp++ = '/';
		if( pp->fp_names[i] )
			strcpy( cp, pp->fp_names[i]->d_namep );
		else
			strcpy( cp, "**NULL**" );
		cp += strlen( cp );
	}
	*cp++ = '\0';
	return( buf );
}

/*
 *			D B _ F R E E _ F U L L _ P A T H
 */
void
db_free_full_path( pp )
register struct db_full_path	*pp;
{
	if( pp->fp_maxlen > 0 )  {
		rt_free( (char *)pp->fp_names, "db_full_path array" );
		pp->fp_maxlen = pp->fp_len = 0;
		pp->fp_names = (struct directory **)0;
	}
}
