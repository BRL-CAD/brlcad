/*
 *			N M G . C
 *
 *  libwdb support for writing an NMG to disk.
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

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "nmg.h"
#include "raytrace.h"
#include "wdb.h"

/*
 *			M K _ N M G
 *
 *  Caller is responsible for freeing the NMG, if desired.
 *
 *  Returns -
 *	<0	error
 *	 0	OK
 */
int
mk_nmg( filep, name, m )
FILE		*filep;
char		*name;
struct model	*m;
{
	NMG_CK_MODEL( m );

	return mk_export_fwrite( filep, name, (genptr_t)m, ID_NMG );
}

/*	W R I T E _ S H E L L _ A S _ P O L Y S O L I D
 *
 *	This routine take an NMG shell and writes it out to the file
 *	out_fp as a polysolid with the indicated name.  Obviously,
 *	the shell should be a 3-manifold (winged edge).
 *	since polysolids may only have up to 5 vertices per face,
 *	any face with a loop of more than 5 vertices is triangulated
 *	using "nmg_triangulate_fu" prior to output.
 *
 *	XXX Since the nmg_triangulate_fu needs a tolerance structure, we
 *		have to invent one for the moment.  This is bogus.
 */
void
write_shell_as_polysolid( out_fp, name, s)
FILE *out_fp;
char *name;
struct shell *s;
{
	struct faceuse *fu;
	struct loopuse *lu;
	struct edgeuse *eu;
	point_t verts[5];
	int count_npts;
	int max_count;
	int i;
	struct rt_tol tol;

	NMG_CK_SHELL( s );

	/* XXX Yet another tol structure is "faked" */
	tol.magic = RT_TOL_MAGIC;
	tol.dist = 0.005;
	tol.dist_sq = tol.dist * tol.dist;
	tol.perp = 1e-6;
	tol.para = 1 - tol.perp;

	mk_polysolid( out_fp , name );

	for( RT_LIST_FOR( fu , faceuse , &s->fu_hd ) )
	{
		NMG_CK_FACEUSE( fu );

		/* only do OT_SAME faces */
		if( fu->orientation != OT_SAME )
			continue;

		/* count vertices in loops */
		max_count = 0;
		for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			NMG_CK_LOOPUSE( lu );
			if( RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
				continue;

			count_npts = 0;
			for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
				count_npts++;

			if( count_npts > max_count )
				max_count = count_npts;

			if( max_count < 5 && !nmg_lu_is_convex( lu, &tol ) )
			{
				max_count = 6;
				break;
			}
		}

		/* if any loop has more than 5 vertices, triangulate the face */
		if( max_count > 5 ) {
			if( rt_g.NMG_debug & DEBUG_BASIC )
				rt_log( "write_shell_as_polysolid: triangulating fu x%x\n", fu );
			nmg_triangulate_fu( fu, (CONST struct rt_tol *)&tol );
		}
		for( RT_LIST_FOR( lu , loopuse , &fu->lu_hd ) )
		{
			NMG_CK_LOOPUSE( lu );
			if( RT_LIST_FIRST_MAGIC(&lu->down_hd) != NMG_EDGEUSE_MAGIC )
				continue;

			count_npts = 0;
			for( RT_LIST_FOR( eu , edgeuse , &lu->down_hd ) )
			{
				for( i=0 ; i<3 ; i++ )
					verts[count_npts][i] = eu->vu_p->v_p->vg_p->coord[i];
				count_npts++;
			}

			if( mk_fpoly( out_fp , count_npts , verts ) )
				rt_log( "write_shell_as_polysolid: mk_fpoly failed for object %s\n" , name );
		}
	}
}
