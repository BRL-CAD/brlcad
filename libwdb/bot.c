/*
 *			B O T . C
 *
 * Support for BOT solid (Bag O'Triangles)
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
static const char part_RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

int
mk_bot(
	struct rt_wdb *fp,
	const char *name,
	unsigned char	mode,
	unsigned char	orientation,
	unsigned char	error_mode,	/* may be used to indicate error handling (ignored for now) */
	int		num_vertices,
	int		num_faces,
	fastf_t		*vertices,	/* array of floats for vertices [num_vertices*3] */
	int		*faces,		/* array of ints for faces [num_faces*3] */
	fastf_t		*thickness,	/* array of plate mode thicknesses (corresponds to array of faces)
					 * NULL for modes RT_BOT_SURFACE and RT_BOT_SOLID.
					 */
	struct bu_bitv	*face_mode )	/* a flag for each face indicating thickness is appended to hit point,
					 * otherwise thickness is centered about hit point
					 */
{
	struct rt_bot_internal *bot;
	int i;

	BU_GETSTRUCT( bot, rt_bot_internal );
	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->mode = mode;
	bot->orientation = orientation;
	bot->error_mode = error_mode;
	bot->num_vertices = num_vertices;
	bot->num_faces = num_faces;
	bot->vertices = (fastf_t *)bu_calloc( num_vertices * 3, sizeof( fastf_t ), "bot->vertices" );
	for( i=0 ; i<num_vertices*3 ; i++ )
		bot->vertices[i] = vertices[i];
	bot->faces = (int *)bu_calloc( num_faces * 3, sizeof( int ), "bot->faces" );
	for( i=0 ; i<num_faces*3 ; i++ )
		bot->faces[i] = faces[i];
	if( mode == RT_BOT_PLATE )
	{
		bot->thickness = (fastf_t *)bu_calloc( num_faces, sizeof( fastf_t ), "bot->thickness" );
		for( i=0 ; i<num_faces ; i++ )
			bot->thickness[i] = thickness[i];
		bot->face_mode = bu_bitv_dup( face_mode );
	}
	else
	{
		bot->thickness = (fastf_t *)NULL;
		bot->face_mode = (struct bu_bitv *)NULL;
	}
	

	return wdb_export( fp, name, (genptr_t)bot, ID_BOT, mk_conv2mm );
}
