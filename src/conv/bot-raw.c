/*                       B O T - R A W . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file bot-raw.c
 *
 */
#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "bu.h"
#include "raytrace.h"
#include "wdb.h"


void write_bot(struct rt_bot_internal *bot, FILE *fh, char *name)
{
    int num_vertices;
    fastf_t *vertices;
    int num_faces, *faces, i, n;
    char string[64];
    float v;

    num_vertices = bot->num_vertices;
    vertices = bot->vertices;
    num_faces = bot->num_faces;
    faces = bot->faces;


    printf ("name: %s, verts: %d, faces: %d\n", name, num_vertices, num_faces);

    /* Name */
    memset (string, 0, 64);
    memcpy (string, name, strlen(name));
    fwrite (string, 64, 1, fh);

    /* Vertices */
    fwrite (&num_vertices, sizeof(int), 1, fh);
    for (i = 0; i < num_vertices; i++)
      for (n = 0; n < 3; n++) {
        v = (float)vertices[3*i+n] * 0.001;
        fwrite ( &v , sizeof(float), 1, fh);
      }

    /* Faces */
    fwrite (&num_faces, sizeof(int), 1, fh);
    for (i = 0; i < num_faces; i++)
      for (n = 0; n < 3; n++)
        fwrite (&faces[3*i+n], sizeof(int), 1, fh);
}


/*
 *	M A I N
 *
 *	Call parse_args to handle command line arguments first, then
 *	process input.
 */
int main(int ac, char *av[])
{
    char idbuf[132];		/* First ID record info */
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    struct rt_i *rtip;
    struct directory *dp;


    if (ac < 2) {
	fprintf(stderr, "usage: bot-raw geom.g [file.raw]\n");
	return 1;
    }

    RT_INIT_DB_INTERNAL(&intern);

    printf("geometry file: %s\n", av[1]);



    if ((rtip=rt_dirbuild(av[1], idbuf, sizeof(idbuf)))==RTI_NULL){
	fprintf(stderr,"rtexample: rt_dirbuild failure\n");
	return 2;
    }


    {
	mat_t mat;
	int i;
	FILE *fh;


        fh = fopen(av[2], "w");

	mat_idn(mat);

	/* dump all the bots */
	FOR_ALL_DIRECTORY_START(dp, rtip->rti_dbip)

        /* we only dump BOT primitives, so skip some obvious exceptions */
        if (dp->d_major_type != DB5_MAJORTYPE_BRLCAD) continue;
	if (dp->d_flags & DIR_COMB) continue;

	/* get the internal form */
	i=rt_db_get_internal(&intern, dp, rtip->rti_dbip, mat,&rt_uniresource);

	if (i < 0) {
	    fprintf(stderr, "rt_get_internal failure %d on %s\n", i, dp->d_namep);
	    continue;
	}

	if (i != ID_BOT) {
	    continue;
	}

	bot = (struct rt_bot_internal *)intern.idb_ptr;

	write_bot(bot, fh, dp->d_namep);

	FOR_ALL_DIRECTORY_END
    }
    return 0;
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
