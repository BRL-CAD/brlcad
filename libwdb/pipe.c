/*
 *			P I P E . C
 *
 *  Support for particles and pipes.
 *  Library for writing geometry databases from arbitrary procedures.
 *
 *  Note that routines which are passed point_t or vect_t or mat_t
 *  parameters (which are call-by-address) must be VERY careful to
 *  leave those parameters unmodified (eg, by scaling), so that the
 *  calling routine is not surprised.
 *
 *  Return codes of 0 are OK, -1 signal an error.
 *
 *  Authors -
 *	Michael John Muuss
 *	Susanne L. Muuss
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
static char part_RCSid[] = "@(#)$Header$ (BRL)";
#endif


#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "db.h"
#include "vmath.h"
#include "rtlist.h"
#include "wdb.h"

/*
 *			M K _ P A R T I C L E
 *
 *
 *  Returns -
 *	0	OK
 *	<0	failure
 */
int
mk_particle( fp, name, vertex, height, vradius, hradius )
FILE	*fp;
char	*name;
point_t	vertex;
vect_t	height;
double	vradius;
double	hradius;
{
	union record	rec;
	point_t		vert;
	vect_t		hi;
	fastf_t		vrad;
	fastf_t		hrad;

	/* Convert from user units to mm */
	VSCALE( vert, vertex, mk_conv2mm );
	VSCALE( hi, height, mk_conv2mm );
	vrad = vradius * mk_conv2mm;
	hrad = hradius * mk_conv2mm;

	bzero( (char *)&rec, sizeof(rec) );
	rec.part.p_id = DBID_PARTICLE;
	NAMEMOVE(name,rec.part.p_name);

	htond( rec.part.p_v, vert, 3 );
	htond( rec.part.p_h, hi, 3 );
	htond( rec.part.p_vrad, &vrad, 1 );
	htond( rec.part.p_hrad, &hrad, 1 );

	if( fwrite( (char *) &rec, sizeof(rec), 1, fp) != 1 )
		return(-1);
	return(0);
}


/*
 *			M K _ P I P E
 *
 *  Note that the linked list of pipe segments headed by 'headp'
 *  must be freed by the caller.  mk_pipe_free() can be used.
 *
 *  Returns -
 *	0	OK
 *	<0	failure
 */
int
mk_pipe( fp, name, headp )
FILE			*fp;
char			*name;
struct wdb_pipeseg	*headp;
{
	register struct exported_pipeseg *ep;
	register struct wdb_pipeseg	*psp;
	struct wdb_pipeseg		tmp;
	int		count;
	int		ngran;
	int		nbytes;
	union record	*rec;

	/* Count number of segments, verify that last seg is an END seg */
	count = 0;
	for( RT_LIST( psp, wdb_pipeseg, &headp->l ) )  {
		count++;
		switch( psp->ps_type )  {
		case WDB_PIPESEG_TYPE_END:
			if( RT_LIST_MORE( psp, wdb_pipeseg, &headp->l ) )
				return(-1);	/* Inconsistency in list */
			break;
		case WDB_PIPESEG_TYPE_LINEAR:
		case WDB_PIPESEG_TYPE_BEND:
			if( !RT_LIST_MORE( psp, wdb_pipeseg, &headp->l ) )
				return(-2);	/* List ends w/o TYPE_END */
			break;
		default:
			return(-3);		/* unknown segment type */
		}
	}
	if( count <= 1 )
		return(-4);			/* Not enough for 1 pipe! */

	/* Determine how many whole granules will be required */
	nbytes = sizeof(struct pipe_wire_rec) -
		sizeof(struct exported_pipeseg) +
		count * sizeof(struct exported_pipeseg);
	ngran = (nbytes + sizeof(union record) - 1) / sizeof(union record);
	nbytes = ngran * sizeof(union record);
	if( (rec = (union record *)malloc( nbytes )) == (union record *)0 )
		return(-5);
	bzero( (char *)rec, nbytes );

	rec->pw.pw_id = DBID_PIPE;
	NAMEMOVE( name, rec->pw.pw_name );
	rec->pw.pw_count = ngran;

	/* Convert the pipe segments to external form */
	ep = &rec->pw.pw_data[0];
	for( RT_LIST( psp, wdb_pipeseg, &headp->l ), ep++ )  {
		/* Avoid need for htonl() here */
		ep->eps_type[0] = (char)psp->ps_type;
		/* Convert from user units to mm */
		VSCALE( tmp.ps_start, psp->ps_start, mk_conv2mm );
		VSCALE( tmp.ps_bendcenter, psp->ps_bendcenter, mk_conv2mm );
		tmp.ps_id = psp->ps_id * mk_conv2mm;
		tmp.ps_od = psp->ps_od * mk_conv2mm;
		htond( ep->eps_start, tmp.ps_start, 3 );
		htond( ep->eps_bendcenter, tmp.ps_bendcenter, 3 );
		htond( ep->eps_id, &tmp.ps_id, 1 );
		htond( ep->eps_od, &tmp.ps_od, 1 );
	}

	if( fwrite( (char *) rec, nbytes, 1, fp) != 1 )  {
		free( (char *)rec );
		return(-10);
	}
	free( (char *)rec );
	return(0);
}

/*
 *			M K _ P I P E _ F R E E
 *
 *  Release the storage from a list of pipe segments.
 *  The head is left in initialized state (ie, forward & back point to head).
 */
void
mk_pipe_free( headp )
register struct wdb_pipeseg	*headp;
{
	register struct wdb_pipeseg	*wp;

	while( RT_LIST_LOOP( wp, wdb_pipeseg, &headp->l ) )  {
		RT_LIST_DEQUEUE( &wp->l );
		free( (char *)wp );
	}
}
