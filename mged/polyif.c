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
	int	npts;			/* number of points */
	vect_t	normal;			/* surface normal */
};
#define POLYGON_HEADER_MAGIC	0x8623bad2
struct rt_imexport  polygon_desc[] = {
	"%d",	offsetof(struct polygon_header, magic),		1,
	"%d",	offsetof(struct polygon_header, ident),		1,
	"%d",	offsetof(struct polygon_header, interior),	1,
	"%d",	offsetof(struct polygon_header, npts),		1,
	"%f",	offsetofarray(struct polygon_header, normal),	3,
	"",	0,						0
};
struct rt_imexport vertex_desc[] = {
	"%f",	0,	99,	/* im_count will be filled in at runtime */
	"",	0,	0
};

/*
 *			F _ P O L Y B I N O U T
 *
 *  Experimental interface for writing binary polygons
 *  that represent the current (evaluated) view.
 *
 *  Usage:  polybinout file
 */
void
f_polybinout( argc, argv )
int	argc;
char	**argv;
{
	register struct solid *sp;
	register struct vlist *vp;
	FILE	*fp;
	int	pno = 0;
	struct polygon_header ph;
#define MAX_VERTS	1024
	vect_t	verts[MAX_VERTS];
	int	need_normal = 0;
	int	olen;
	char	*obuf;

	if( (fp = fopen( argv[1], "w" )) == NULL )  {
		perror(argv[1]);
		return;
	}

	FOR_ALL_SOLIDS( sp )  {
		for( vp = sp->s_vlist; vp != VL_NULL; vp = vp->vl_forw )  {
			/* For each polygon, spit it out.  Ignore vectors */
			switch( vp->vl_draw )  {
			case VL_CMD_LINE_MOVE:
				/* Move, start line */
				break;
			case VL_CMD_LINE_DRAW:
				/* Draw line */
				break;
			case VL_CMD_POLY_START:
				/* Start poly marker & normal, followed by POLY_MOVE */
				ph.magic = POLYGON_HEADER_MAGIC;
				ph.ident = pno++;
				ph.interior = 0;
				ph.npts = 0;
				/* Set surface normal (vl_pnt points outward) */
				VMOVE( ph.normal, vp->vl_pnt );
				need_normal = 0;
				break;
			case VL_CMD_POLY_MOVE:
				/* Start of polygon, has first point */
				/* fall through to... */
			case VL_CMD_POLY_DRAW:
				/* Polygon Draw */
				if( ph.npts >= MAX_VERTS )  {
					printf("excess vertex skipped\n");
					break;
				}
				VMOVE( verts[ph.npts], vp->vl_pnt );
				ph.npts++;
				break;
			case VL_CMD_POLY_END:
				/*
				 *  End Polygon.  Point given is repeat of
				 *  first one, ignore it.
				 * XXX note:  if poly_markers was not set,
				 * XXX poly will end with next POLY_MOVE.
				 */
				if( ph.npts < 3 )  {
					printf("polygon with %d points discarded\n",
						ph.npts);
					break;
				}
				if( need_normal )  {
					vect_t	e1, e2;
					VSUB2( e1, verts[0], verts[1] );
					VSUB2( e2, verts[0], verts[2] );
					VCROSS( ph.normal, e1, e2 );
				}
				obuf = rt_struct_export( &olen,
					(genptr_t)&ph, polygon_desc );
				if( obuf == (char *)0 )  {
					printf("header export error\n");
					break;
				}
				if( fwrite( obuf, 1, olen, fp ) != olen )  {
					perror("fwrite");
					break;
				}
				rt_free( obuf, "polygon header" );

				/* Now export the vertices */
				vertex_desc[0].im_count = ph.npts * 3;
				obuf = rt_struct_export( &olen,
					(genptr_t)verts, vertex_desc );
				if( obuf == (char *)0 )  {
					printf("vertex export error\n");
					break;
				}
				if( fwrite( obuf, 1, olen, fp ) != olen )  {
					perror("fwrite");
					break;
				}
				rt_free( obuf, "vertex buffer" );
				printf("%d, ", ph.npts);
				ph.npts = 0;		/* sanity */
				break;
			}
		}
	}
	printf("\n");
	fclose( fp );
}
