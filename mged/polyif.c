/*
 *			P O L Y I F . C
 *
 *  Author -
 *	Michael John Muuss
 *	Christopher T. Johnson
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
static char RCSpolyif[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "mater.h"
#include "raytrace.h"
#include "externs.h"
#include "./ged.h"
#include "./solid.h"
#include "./dm.h"

/* XXX When finalized, this stuff belongs in a header file of it's own */
struct polygon_header  {
	int	magic;			/* magic number */
	int	ident;			/* identification number */
	int	interior;		/* >0 => interior loop, gives ident # of exterior loop */
	vect_t	normal;			/* surface normal */
	unsigned char	color[3];	/* Color from containing region */
	int	npts;			/* number of points */
};
#define POLYGON_HEADER_MAGIC	0x8623bad2
struct rt_imexport  polygon_desc[] = {
	{"%d",	offsetof(struct polygon_header, magic),		1 },
	{"%d",	offsetof(struct polygon_header, ident),		1 },
	{"%d",	offsetof(struct polygon_header, interior),	1 },
	{"%f",	offsetofarray(struct polygon_header, normal),	3 },
	{"%c",	offsetofarray(struct polygon_header, color),	3 },
	{"%d",	offsetof(struct polygon_header, npts),		1 },
	{"",	0,						0 }
};
struct rt_imexport vertex_desc[] = {
	{"%f",	0,	99 },	/* im_count will be filled in at runtime */
	{"",	0,	0 }
};

/*
 *			F _ P O L Y B I N O U T
 *
 *  Experimental interface for writing binary polygons
 *  that represent the current (evaluated) view.
 *
 *  Usage:  polybinout file
 */
int
f_polybinout( argc, argv )
int	argc;
char	**argv;
{
	register struct solid		*sp;
	register struct rt_vlist	*vp;
	FILE	*fp;
	int	pno = 1;
	struct polygon_header ph;
#define MAX_VERTS	1024
	vect_t	verts[MAX_VERTS];
	int	need_normal = 0;
	struct	rt_external	obuf;

	if( (fp = fopen( argv[1], "w" )) == NULL )  {
		perror(argv[1]);
		return CMD_BAD;
	}

	FOR_ALL_SOLIDS( sp )  {
		for( RT_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
			register int	i;
			register int	nused = vp->nused;
			register int	*cmd = vp->cmd;
			register point_t *pt = vp->pt;
			for( i = 0; i < nused; i++,cmd++,pt++ )  {
				/* For each polygon, spit it out.  Ignore vectors */
				switch( *cmd )  {
				case RT_VLIST_LINE_MOVE:
					/* Move, start line */
					break;
				case RT_VLIST_LINE_DRAW:
					/* Draw line */
					break;
				case RT_VLIST_POLY_VERTNORM:
					/* Ignore per-vertex normal */
					break;
				case RT_VLIST_POLY_START:
					/* Start poly marker & normal, followed by POLY_MOVE */
					ph.magic = POLYGON_HEADER_MAGIC;
					ph.ident = pno++;
					ph.interior = 0;
					bcopy(sp->s_basecolor, ph.color, 3);
					ph.npts = 0;
					/* Set surface normal (vl_pnt points outward) */
					VMOVE( ph.normal, *pt );
					need_normal = 0;
					break;
				case RT_VLIST_POLY_MOVE:
					/* Start of polygon, has first point */
					/* fall through to... */
				case RT_VLIST_POLY_DRAW:
					/* Polygon Draw */
					if( ph.npts >= MAX_VERTS )  {
						rt_log("excess vertex skipped\n");
						break;
					}
					VMOVE( verts[ph.npts], *pt );
					ph.npts++;
					break;
				case RT_VLIST_POLY_END:
					/*
					 *  End Polygon.  Point given is repeat of
					 *  first one, ignore it.
					 * XXX note:  if poly_markers was not set,
					 * XXX poly will end with next POLY_MOVE.
					 */
					if( ph.npts < 3 )  {
						rt_log("polygon with %d points discarded\n",
							ph.npts);
						break;
					}
					if( need_normal )  {
						vect_t	e1, e2;
						VSUB2( e1, verts[0], verts[1] );
						VSUB2( e2, verts[0], verts[2] );
						VCROSS( ph.normal, e1, e2 );
					}
					if( rt_struct_export( &obuf, (genptr_t)&ph, polygon_desc ) < 0 )  {
						rt_log("header export error\n");
						break;
					}
					if (rt_struct_put(fp, &obuf) != obuf.ext_nbytes) {
						perror("rt_struct_put");
						break;
					}
					db_free_external( &obuf );
					/* Now export the vertices */
					vertex_desc[0].im_count = ph.npts * 3;
					if( rt_struct_export( &obuf, (genptr_t)verts, vertex_desc ) < 0 )  {
						rt_log("vertex export error\n");
						break;
					}
					if( rt_struct_put( fp, &obuf ) != obuf.ext_nbytes )  {
						perror("rt_struct_buf");
						break;
					}
					db_free_external( &obuf );
					ph.npts = 0;		/* sanity */
					break;
				}
			}
		}
	}
	fclose( fp );
	return CMD_OK;
}
