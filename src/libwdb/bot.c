/*                           B O T . C
 * BRL-CAD
 *
 * Copyright (c) 1999-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file bot.c
 *
 * Support for BOT solid (Bag O'Triangles)
 *
 *  Author -
 *	John Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static const char part_RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"


#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#include "machine.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"


int
mk_bot_w_normals(
	struct rt_wdb *fp,
	const char *name,
	unsigned char	mode,
	unsigned char	orientation,
	unsigned char	flags,
	int		num_vertices,
	int		num_faces,
	fastf_t		*vertices,	/* array of floats for vertices [num_vertices*3] */
	int		*faces,		/* array of ints for faces [num_faces*3] */
	fastf_t		*thickness,	/* array of plate mode thicknesses (corresponds to array of faces)
					 * NULL for modes RT_BOT_SURFACE and RT_BOT_SOLID.
					 */
	struct bu_bitv	*face_mode,	/* a flag for each face indicating thickness is appended to hit point,
					 * otherwise thickness is centered about hit point
					 */
	int		num_normals,	/* number of unit normals in normals array */
	fastf_t		*normals,	/* array of floats for normals [num_normals*3] */
	int		*face_normals )	/* array of ints (indices into normals array), must have 3*num_faces entries */
{
	struct rt_bot_internal *bot;
	int i;

	if( (num_normals > 0) && (fp->dbip->dbi_version < 5 ) ) {
		bu_log( "You are using an old database format which does not support surface normals for BOT primitives\n" );
		bu_log( "You are attempting to create a BOT primitive named \"%s\" with surface normals\n" );
		bu_log( "The surface normals will not be saved\n" );
		bu_log( "Please upgrade to the current database format by using \"dbupgrade\"\n" );
	}

	BU_GETSTRUCT( bot, rt_bot_internal );
	bot->magic = RT_BOT_INTERNAL_MAGIC;
	bot->mode = mode;
	bot->orientation = orientation;
	bot->bot_flags = flags;
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

	if( (num_normals > 0) && (fp->dbip->dbi_version >= 5 ) ) {
		bot->num_normals = num_normals;
		bot->num_face_normals = bot->num_faces;
		bot->normals = (fastf_t *)bu_calloc( bot->num_normals * 3, sizeof( fastf_t ), "BOT normals" );
		bot->face_normals = (int *)bu_calloc( bot->num_faces * 3, sizeof( int ), "BOT face normals" );
		memcpy( bot->normals, normals, bot->num_normals * 3 * sizeof( fastf_t ) );
		memcpy( bot->face_normals, face_normals, bot->num_faces * 3 * sizeof( int ) );
	} else {
		bot->bot_flags = 0;
		bot->num_normals = 0;
		bot->num_face_normals = 0;
		bot->normals = (fastf_t *)NULL;
		bot->face_normals = (int *)NULL;
	}

	return wdb_export( fp, name, (genptr_t)bot, ID_BOT, mk_conv2mm );
}

int
mk_bot(
	struct rt_wdb *fp,
	const char *name,
	unsigned char	mode,
	unsigned char	orientation,
	unsigned char	flags,
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
	return( mk_bot_w_normals( fp, name, mode, orientation, flags, num_vertices, num_faces, vertices,
				  faces, thickness, face_mode, 0, NULL, NULL ) );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
