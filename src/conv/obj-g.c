/*                         O B J - G . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file conv/obj-g.c
 *
 * Convert wavefront obj format files to BRL-CAD .g binary format
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

static char *usage = "%s [-d] [-x rt_debug_flag] input.obj output.g\n\
			 where input.obj is a WaveFront Object file\n\
			 and output.g is the name of a BRL-CAD database file to receive the conversion.\n\
			 The -d option prints additional debugging information.\n\
			 The -x option specifies an RT debug flags (see raytrace.h).\n";
static int debug = 0;
static int verbose = 0;

struct object_s {
    char *name;
    struct rt_bot_internal *bot;
};


struct obj_data {
    size_t num_vertices;
    fastf_t *vertices;
    size_t num_faces;
    long *faces;
};


struct object_s *
new_object(const char *name)
{
    struct object_s *r = (struct object_s *)bu_calloc(1, sizeof(struct object_s), "new_object");
    r->bot = (struct rt_bot_internal *)bu_calloc(1, sizeof(struct rt_bot_internal), "new_object:rt_bot_internal");
    r->name = strdup(name);
    r->bot->magic = RT_BOT_INTERNAL_MAGIC;
    r->bot->mode = RT_BOT_SURFACE;
    r->bot->orientation = RT_BOT_UNORIENTED;
    r->bot->num_vertices = 0;
    r->bot->vertices = NULL;
    r->bot->num_faces = 0;
    r->bot->faces = NULL;
    return r;
}


int
free_object(struct object_s *r)
{
    if (!r)
	return 0;

    if (r->name)
	bu_free(r->name, "object name");
    r->name = NULL;

    if (r->bot)
	bu_free(r->bot, "rt_bot_internal");
    r->bot = NULL;

    bu_free(r, "object");

    return 0;
}


int
write_object(struct object_s *r, struct rt_wdb *out_fp)
{
    int rval = -1;
    char *regname;

    if (r->bot->num_faces == 0) {
	rval = 0;
	if (r->name && verbose) {
	    bu_log("%s has 0 faces, skipping\n", r->name);
	}
    } else {
	int faces;
	/* add the object long name to list */
	regname = bu_basename(r->name);
	faces = r->bot->num_faces;
	rval = wdb_export(out_fp, regname, (genptr_t)(r->bot), ID_BOT, 1.0);
	r->bot = NULL; /* released during export */
	if (verbose)
	    bu_log("Wrote %s (%d faces)\n", regname, faces);
	bu_free(regname, "regname free");
    }

    /* done with this object, let it go */
    free_object(r);
    r = NULL;

    return rval;
}


int
add_vertex(struct obj_data *o, char *buf)
{
    /* syntax is "v <x> <y> <z> [w]" */
    if (!o->vertices) {
	o->vertices = bu_calloc(3, sizeof(fastf_t), "vertices");
	o->num_vertices = 0;
    } else {
	o->vertices = bu_realloc(o->vertices, 3 * sizeof(fastf_t) * (o->num_vertices+1), "vertices");
    }

    sscanf(buf, "%lf %lf %lf", 
	   &(o->vertices[3*o->num_vertices + 0]),
	   &(o->vertices[3*o->num_vertices + 1]),
	   &(o->vertices[3*o->num_vertices + 2]));
    o->num_vertices++;

    return 0;
}


int
add_face(struct object_s *r, struct obj_data *o, char *buf)
{
    long vertex;

    /* checking to see if OBJ file contains texture and/or normal data.
     * we won't be using it but will need to parse around it.
     * syntax is ... messy. v1/vt1/vn1, can be:
     *
     * "f 1 2 3 ...", or "f 1//1 2//x ..." or "f 1/1/1/ ..." or "f 1/1 ..." or ...
     */

    char *contains_slash = strchr(buf, '/');

    if (!o->faces) {
	o->faces = bu_calloc(3, sizeof(long), "faces");
	o->num_faces = 0;
    } else {
	o->faces = bu_realloc(o->faces, 3 * sizeof(long) * (o->num_faces+1), "faces");
    }

    /* add to global data */
    if (contains_slash) {
	sscanf(buf, "%ld/%*s %ld/%*s %ld/%*s", 
	       &(o->faces[3*o->num_faces + 0]),
	       &(o->faces[3*o->num_faces + 1]),
	       &(o->faces[3*o->num_faces + 2]));
    } else {
	sscanf(buf, "%ld %ld %ld", 
	       &(o->faces[3*o->num_faces + 0]),
	       &(o->faces[3*o->num_faces + 1]),
	       &(o->faces[3*o->num_faces + 2]));
    }

    /* add to BoT */
    if (!r->bot->vertices) {
	r->bot->vertices = bu_calloc(9, sizeof(fastf_t), "bot vertices");
	r->bot->num_vertices = 0;
    } else {
	r->bot->vertices = bu_realloc(r->bot->vertices, 3 * sizeof(fastf_t) * (r->bot->num_vertices+3), "bot vertices");
    }

    /* face vertices are indexed from 1, not 0 */
    vertex = o->faces[3*o->num_faces+0] - 1;
    r->bot->vertices[3*r->bot->num_vertices + 0] = o->vertices[3*vertex + 0];
    r->bot->vertices[3*r->bot->num_vertices + 1] = o->vertices[3*vertex + 1];
    r->bot->vertices[3*r->bot->num_vertices + 2] = o->vertices[3*vertex + 2];
    r->bot->num_vertices++;

    vertex = o->faces[3*o->num_faces+1] - 1;
    r->bot->vertices[3*r->bot->num_vertices + 0] = o->vertices[3*vertex + 0];
    r->bot->vertices[3*r->bot->num_vertices + 1] = o->vertices[3*vertex + 1];
    r->bot->vertices[3*r->bot->num_vertices + 2] = o->vertices[3*vertex + 2];
    r->bot->num_vertices++;

    vertex = o->faces[3*o->num_faces+2] - 1;
    r->bot->vertices[3*r->bot->num_vertices + 0] = o->vertices[3*vertex + 0];
    r->bot->vertices[3*r->bot->num_vertices + 1] = o->vertices[3*vertex + 1];
    r->bot->vertices[3*r->bot->num_vertices + 2] = o->vertices[3*vertex + 2];
    r->bot->num_vertices++;

    if (!r->bot->faces)
	r->bot->faces = bu_calloc(3, sizeof(int), "bot faces");
    else
	r->bot->faces = bu_realloc(r->bot->faces, 3 * sizeof(int) * (r->bot->num_faces+1), "bot faces");
    r->bot->faces[3*r->bot->num_faces+0] = r->bot->num_vertices-3;
    r->bot->faces[3*r->bot->num_faces+1] = r->bot->num_vertices-2;
    r->bot->faces[3*r->bot->num_faces+2] = r->bot->num_vertices-1;
    r->bot->num_faces++;

    o->num_faces++;

    return 0;
}


int
main(int argc, char *argv[])
{
    int c;
    char *prog = *argv, buf[BUFSIZ];
    FILE *fd_in; /* input file */
    struct rt_wdb *fd_out; /* Resulting BRL-CAD file */
    struct object_s *object = NULL;
    struct obj_data data = {0, NULL, 0, NULL};

    if (argc < 2)
	bu_exit(1, usage, argv[0]);

    while ((c = bu_getopt(argc, argv, "dxv")) != -1) {
	switch (c) {
	    case 'd':
		debug = 1;
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *) &rt_g.debug);
		bu_printb("librt RT_G_DEBUG", RT_G_DEBUG, DEBUG_FORMAT);
		bu_log("\n");
		break;
	    case 'v':
		verbose++;
		break;
	    default:
		bu_exit(1, usage, argv[0]);
		break;
	}
    }
    argv += bu_optind;
    argc -= bu_optind;

    rt_init_resource(&rt_uniresource, 0, NULL);

    /* open the OBJ for reading */
    if ((fd_in = fopen(argv[0], "r")) == NULL) {
	bu_log("Cannot open input file (%s)\n", argv[0]);
	perror(prog);
	bu_exit(1, NULL);
    }
    /* open the .g for writing */
    if ((fd_out = wdb_fopen(argv[1])) == NULL) {
	bu_log("Cannot create new BRL-CAD file (%s)\n", argv[1]);
	perror(prog);
	bu_exit(1, NULL);
    }

    /* prep the object, use a default name in case the OBJ has no groups */
    object = new_object("default.s");
    /* loop through the OBJ file. */
    while (bu_fgets(buf, BUFSIZ, fd_in)) {
	if (ferror(fd_in)) {
	    bu_exit(EXIT_FAILURE, "Ack! %d\nflaming death\n", ferror(fd_in));
	}
	/* ghetto chomp() */
	if (strlen(buf)>2 && buf[strlen(buf) - 1] == '\n')
	    buf[strlen(buf) - 1] = 0;

	switch (*buf) {
	    case '#':	/* comment */
	    case '\0':
	    case '\n':
	    case ' ':
		continue;
	    case 'o':
	    case 'g':	/* group (object) */
		if (object && object->bot) {
		    if (object->bot->num_faces > 0 && object->bot->num_vertices > 0) {
			bu_log("Writing [%s]\n", object->name);
			write_object(object, fd_out);
		    } else {
			if (strncmp(object->name, "default.s", 9) != 0)
			    bu_log("Skipping invalid mesh [%s] (faces=%ld, vertices=%ld)\n", object->name, (long)object->bot->num_faces, (long)object->bot->num_vertices);
		    }
		}
		object = new_object(buf + 2);
		if (!object) {
		    perror(prog);
		    return EXIT_FAILURE;
		}
		break;
	    case 'v':	/* vertex */
		switch(buf[1]) {
		    case ' ':
			if (!object) {
			    perror(prog);
			    if (data.vertices)
				bu_free(data.vertices, "vertices");
			    if (data.faces)
				bu_free(data.faces, "faces");
			    return EXIT_FAILURE;
			}
			add_vertex(&data, buf + 2);
			break;
		    case 'n':
			/* vertex normal here */
			break;
		    case 't':
			/* vertex texture coordinate here */
			break;
		}
		break;
	    case 'f':	/* face */
		if (!object) {
		    perror(prog);
		    return EXIT_FAILURE;
		}
		add_face(object, &data, buf + 2);
		break;
	    case 'l': 
		{
		    static int seen = 0;
		    if (!seen) {
			if (verbose)
			    bu_log("Saw a 'line' statement, ignoring lines.\n");
			seen++;
		    }
		}
		break;
	    case 's':
		{
		    static int seen = 0;
		    if (!seen) {
			if (verbose)
			    bu_log("Saw a 'smoothing group' statement, ignoring.\n");
			seen++;
		    }
		}
		break;
	    case 'm':
		if (!strncmp(buf, "mtllib", 6))
		    if (verbose)
			bu_log("Ignoring this mtllib for now\n");
		break;
	    case 'u':
		if (!strncmp(buf, "usemtl", 6))
		    if (verbose)
			bu_log("Ignoring this usemtl for now\n");
		break;
	    default:
		bu_log("Ignoring unknown OBJ code: %c\n", *buf);
		continue;
	}
    }
    if (object && object->bot && object->bot->num_faces > 0) {
	bu_log("Writing [%s]\n", object->name);
	write_object(object, fd_out);
    }

    /* using the list generated in write objects, build the tree. */

    wdb_close(fd_out);

    /* release face memory */
    if (data.vertices)
	bu_free(data.vertices, "vertices");
    if (data.faces)
	bu_free(data.faces, "faces");

    return 0;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
