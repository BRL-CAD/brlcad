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
#include "./mged_solid.h"
#include "./mged_dm.h"

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

struct bu_structparse polygon_desc[] = {
	{"%d", 1, "magic", offsetof(struct polygon_header, magic), BU_STRUCTPARSE_FUNC_NULL },
	{"%d", 1, "ident", offsetof(struct polygon_header, ident), BU_STRUCTPARSE_FUNC_NULL },
	{"%d", 1, "interior", offsetof(struct polygon_header, interior), BU_STRUCTPARSE_FUNC_NULL },
	{"%f", 3, "normal", bu_offsetofarray(struct polygon_header, normal), BU_STRUCTPARSE_FUNC_NULL },
	{"%c", 3, "color", bu_offsetofarray(struct polygon_header, color), BU_STRUCTPARSE_FUNC_NULL },
	{"%d", 1, "npts", offsetof(struct polygon_header, npts), BU_STRUCTPARSE_FUNC_NULL },
	{"",   0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL }
};
struct bu_structparse vertex_desc[] = {
	{"%f", 3, "vertex", 0, BU_STRUCTPARSE_FUNC_NULL },
	{"",   0, (char *)0, 0, BU_STRUCTPARSE_FUNC_NULL }
};

#if 0
struct rt_imexport  polygon_desc[] = {
	{"%d",	offsetof(struct polygon_header, magic),		1 },
	{"%d",	offsetof(struct polygon_header, ident),		1 },
	{"%d",	offsetof(struct polygon_header, interior),	1 },
	{"%f",	bu_offsetofarray(struct polygon_header, normal),	3 },
	{"%c",	bu_offsetofarray(struct polygon_header, color),	3 },
	{"%d",	offsetof(struct polygon_header, npts),		1 },
	{"",	0,						0 }
};
struct rt_imexport vertex_desc[] = {
	{"%f",	0,	99 },	/* im_count will be filled in at runtime */
	{"",	0,	0 }
};
#endif
/*
 *			F _ P O L Y B I N O U T
 *
 *  Experimental interface for writing binary polygons
 *  that represent the current (evaluated) view.
 *
 *  Usage:  polybinout file
 */
int
f_polybinout(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
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
	struct	bu_external	obuf;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( (fp = fopen( argv[1], "w" )) == NULL )  {
	  perror(argv[1]);
	  return TCL_ERROR;
	}

	FOR_ALL_SOLIDS(sp, &HeadSolid.l)  {
		for( BU_LIST_FOR( vp, rt_vlist, &(sp->s_vlist) ) )  {
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
					  Tcl_AppendResult(interp, "excess vertex skipped\n",
							   (char *)NULL);
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
				    struct bu_vls tmp_vls;

				    bu_vls_init(&tmp_vls);
				    bu_vls_printf(&tmp_vls, "polygon with %d points discarded\n",
						  ph.npts);
				    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
				    bu_vls_free(&tmp_vls);
				    break;
				  }
					if( need_normal )  {
						vect_t	e1, e2;
						VSUB2( e1, verts[0], verts[1] );
						VSUB2( e2, verts[0], verts[2] );
						VCROSS( ph.normal, e1, e2 );
					}
					if( bu_struct_export( &obuf, (genptr_t)&ph, polygon_desc ) < 0 )  {
					  Tcl_AppendResult(interp, "header export error\n", (char *)NULL);
					  break;
					}
					if (bu_struct_put(fp, &obuf) != obuf.ext_nbytes) {
						perror("bu_struct_put");
						break;
					}
					db_free_external( &obuf );
					/* Now export the vertices */
					vertex_desc[0].sp_count = ph.npts * 3;
					if( bu_struct_export( &obuf, (genptr_t)verts, vertex_desc ) < 0 )  {
					  Tcl_AppendResult(interp, "vertex export error\n", (char *)NULL);
					  break;
					}
					if( bu_struct_put( fp, &obuf ) != obuf.ext_nbytes )  {
						perror("bu_struct_wrap_buf");
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
	return TCL_OK;
}
