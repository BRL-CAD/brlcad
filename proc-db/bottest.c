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
	fastf_t vertices[12];
	int faces[12];
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

	bu_free( (char *)face_mode, "bottest: face_mode" );
}
