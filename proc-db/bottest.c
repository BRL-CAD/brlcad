/*
 *			B O T T E S T . C
 *
 *  Program to generate test BOT solids
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
#include "rtgeom.h"

main( argc, argv )
int argc;
char *argv[];
{
	fastf_t vertices[36];
	int faces[15];
	struct bu_bitv *face_mode;
	fastf_t thickness[4];

	mk_id( stdout, "BOT test" );

	VSET( vertices, 0.0, 0.0, 0.0 )
	VSET( &vertices[3], 0.0, 100.0, 0.0 )
	VSET( &vertices[6], 0.0, 100.0, 50.0 )
	VSET( &vertices[9], 200.0, 0.0, 0.0 )

	/* face #1 */
	faces[0] = 0;
	faces[1] = 1;
	faces[2] = 2;

	/* face #2 */
	faces[3] = 0;
	faces[4] = 2;
	faces[5] = 3;

	/* face #3 */
	faces[6] = 0;
	faces[7] = 1;
	faces[8] = 3;

	/* face #4 */
	faces[9] = 1;
	faces[10] = 2;
	faces[11] = 3;

	mk_bot( stdout, "bot_u_surf", RT_BOT_SURFACE, RT_BOT_UNORIENTED, 0, 4, 4, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL );

	/* face #1 */
	faces[0] = 0;
	faces[1] = 2;
	faces[2] = 1;

	/* face #2 */
	faces[3] = 0;
	faces[4] = 3;
	faces[5] = 2;

	/* face #3 */
	faces[6] = 0;
	faces[7] = 1;
	faces[8] = 3;

	/* face #4 */
	faces[9] = 1;
	faces[10] = 2;
	faces[11] = 3;

	mk_bot( stdout, "bot_ccw_surf", RT_BOT_SURFACE, RT_BOT_CCW, 0, 4, 4, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL );


	/* face #1 */
	faces[0] = 1;
	faces[1] = 2;
	faces[2] = 0;

	/* face #2 */
	faces[3] = 2;
	faces[4] = 3;
	faces[5] = 0;

	/* face #3 */
	faces[6] = 3;
	faces[7] = 1;
	faces[8] = 0;

	/* face #4 */
	faces[9] = 3;
	faces[10] = 2;
	faces[11] = 1;

	mk_bot( stdout, "bot_cw_surf", RT_BOT_SURFACE, RT_BOT_CW, 0, 4, 4, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL );


	/* face #1 */
	faces[0] = 0;
	faces[1] = 1;
	faces[2] = 2;

	/* face #2 */
	faces[3] = 0;
	faces[4] = 2;
	faces[5] = 3;

	/* face #3 */
	faces[6] = 0;
	faces[7] = 1;
	faces[8] = 3;

	/* face #4 */
	faces[9] = 1;
	faces[10] = 2;
	faces[11] = 3;

	mk_bot( stdout, "bot_u_solid", RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, 4, 4, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL );

	/* face #1 */
	faces[0] = 0;
	faces[1] = 2;
	faces[2] = 1;

	/* face #2 */
	faces[3] = 0;
	faces[4] = 3;
	faces[5] = 2;

	/* face #3 */
	faces[6] = 0;
	faces[7] = 1;
	faces[8] = 3;

	/* face #4 */
	faces[9] = 1;
	faces[10] = 2;
	faces[11] = 3;

	mk_bot( stdout, "bot_ccw_solid", RT_BOT_SOLID, RT_BOT_CCW, 0, 4, 4, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL );


	/* face #1 */
	faces[0] = 1;
	faces[1] = 2;
	faces[2] = 0;

	/* face #2 */
	faces[3] = 2;
	faces[4] = 3;
	faces[5] = 0;

	/* face #3 */
	faces[6] = 3;
	faces[7] = 1;
	faces[8] = 0;

	/* face #4 */
	faces[9] = 3;
	faces[10] = 2;
	faces[11] = 1;

	mk_bot( stdout, "bot_cw_solid", RT_BOT_SOLID, RT_BOT_CW, 0, 4, 4, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL );

	face_mode = bu_bitv_new( 4 );
	bu_bitv_clear( face_mode );
	BU_BITSET( face_mode, 1);

	thickness[0] = 2.1;
	thickness[1] = 2.2;
	thickness[2] = 2.3;
	thickness[3] = 2.4;

	/* face #1 */
	faces[0] = 0;
	faces[1] = 1;
	faces[2] = 2;

	/* face #2 */
	faces[3] = 0;
	faces[4] = 2;
	faces[5] = 3;

	/* face #3 */
	faces[6] = 0;
	faces[7] = 1;
	faces[8] = 3;

	/* face #4 */
	faces[9] = 1;
	faces[10] = 2;
	faces[11] = 3;

	mk_bot( stdout, "bot_u_plate", RT_BOT_PLATE, RT_BOT_UNORIENTED, 0, 4, 4, vertices, faces, thickness, face_mode );

	/* face #1 */
	faces[0] = 0;
	faces[1] = 2;
	faces[2] = 1;

	/* face #2 */
	faces[3] = 0;
	faces[4] = 3;
	faces[5] = 2;

	/* face #3 */
	faces[6] = 0;
	faces[7] = 1;
	faces[8] = 3;

	/* face #4 */
	faces[9] = 1;
	faces[10] = 2;
	faces[11] = 3;

	mk_bot( stdout, "bot_ccw_plate", RT_BOT_PLATE, RT_BOT_CCW, 0, 4, 4, vertices, faces, thickness, face_mode );


	/* face #1 */
	faces[0] = 1;
	faces[1] = 2;
	faces[2] = 0;

	/* face #2 */
	faces[3] = 2;
	faces[4] = 3;
	faces[5] = 0;

	/* face #3 */
	faces[6] = 3;
	faces[7] = 1;
	faces[8] = 0;

	/* face #4 */
	faces[9] = 3;
	faces[10] = 2;
	faces[11] = 1;

	mk_bot( stdout, "bot_cw_plate", RT_BOT_PLATE, RT_BOT_CW, 0, 4, 4, vertices, faces, thickness, face_mode );

	/* Make a bot with duplicate vertices to test the "fuse" and "condense" code */
	VSET( vertices, 0.0, 0.0, 0.0 )
	VSET( &vertices[3], 0.0, 100.0, 0.0 )
	VSET( &vertices[6], 0.0, 100.0, 50.0 )
	VMOVE( &vertices[9], &vertices[0] )
	VMOVE( &vertices[12], &vertices[6] )
	VSET( &vertices[15], 200.0, 0.0, 0.0 )
	VMOVE( &vertices[18], &vertices[0] )
	VMOVE( &vertices[21], &vertices[3] )
	VMOVE( &vertices[24], &vertices[15] )
	VMOVE( &vertices[27], &vertices[3] )
	VMOVE( &vertices[30], &vertices[6] )
	VMOVE( &vertices[33], &vertices[15] )

	/* face #1 */
	faces[0] = 0;
	faces[1] = 1;
	faces[2] = 2;

	/* face #2 */
	faces[3] = 3;
	faces[4] = 4;
	faces[5] = 5;

	/* face #3 */
	faces[6] = 6;
	faces[7] = 7;
	faces[8] = 8;

	/* face #4 */

	faces[9] = 9;
	faces[10] = 10;
	faces[11] = 11;

	mk_bot( stdout, "bot_solid_dup_vs", RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, 12, 4, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL );

	faces[12] = 9;
	faces[13] = 10;
	faces[14] = 11;

	mk_bot( stdout, "bot_solid_dup_fs", RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, 12, 5, vertices, faces, (fastf_t *)NULL, (struct bu_bitv *)NULL );

	bu_free( (char *)face_mode, "bottest: face_mode" );
}
