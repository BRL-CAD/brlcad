/*
 *                            O B J - G . C BRL-CAD
 *
 * Copyright (c) 2002-2009 United States Government as represented by the U.S.
 * Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this file; see the file named COPYING for more information.
 *
 */

/** @file obj-g.c
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

struct region_s {
    char*name;
    struct rt_bot_internal bot;
};

struct region_s *
new_region(char *name)
{
    struct region_s *r;
    r = (struct region_s *) malloc(sizeof(struct region_s *));
    if(r) {
	memset(r, 0, sizeof(r));    /* flash everything to 0, hope that NULL==0x0 */
	r->name = strdup(name);
	r->bot.magic = RT_BOT_INTERNAL_MAGIC;
	r->bot.mode = RT_BOT_SURFACE;
	r->bot.orientation = RT_BOT_CCW;
    }
    return r;
}

int
free_region(struct region_s * r)
{
    if (r && r->name)
	free(r->name);
    if (r)
	free(r);
    return 0;
}

int
write_region(struct region_s *r, struct rt_wdb *out_fp)
{
    int rval;
    const char *regname;
    /* add the region long name to list */
    /* lock here, regname is not reentrant. */
    regname = bu_basename(r->name);
    rval = wdb_export( out_fp, regname, (genptr_t)&(r->bot), ID_BOT, 1.0 );
    /* unlock here. */
    return rval;
}


int
add_vertex(struct region_s * r, char *buf)
{
    r->bot.num_vertices++;
    r->bot.vertices = realloc(r->bot.vertices, sizeof(fastf_t) * 3 * r->bot.num_vertices);
    sscanf(buf, "%lf %lf %lf", 
	    &r->bot.vertices[r->bot.num_vertices],
	    &r->bot.vertices[r->bot.num_vertices+1],
	    &r->bot.vertices[r->bot.num_vertices+2]);
    return 0;
}

int
add_face(struct region_s * r, char *buf)
{
    r->bot.num_faces++;
    r->bot.faces = realloc(r->bot.faces, sizeof(int) * 3 * r->bot.num_faces);
    sscanf(buf, "%d %d %d", 
	    &(r->bot.faces[r->bot.num_faces]),
	    &(r->bot.faces[r->bot.num_faces+1]),
	    &(r->bot.faces[r->bot.num_faces+2]));
    return 0;
}

int
main(int argc, char **argv)
{
    int             c;
    char           *prog = *argv, buf[BUFSIZ];
    FILE           *fd_in;	/* input file */
    struct rt_wdb  *fd_out;	/* Resulting BRL-CAD file */
    struct region_s *region = NULL;

    if (argc < 2)
	bu_exit(1, usage, argv[0]);

    while ((c = bu_getopt(argc, argv, "dx")) != EOF) {
	switch (c) {
	    case 'd':
		debug = 1;
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *) &rt_g.debug);
		bu_printb("librt RT_G_DEBUG", RT_G_DEBUG, DEBUG_FORMAT);
		bu_log("\n");
		break;
	    default:
		bu_exit(1, usage, argv[0]);
		break;
	}
    }
    argv += bu_optind;
    argc -= bu_optind;

    rt_init_resource(&rt_uniresource, 0, NULL);

    if ((fd_in = fopen(argv[0], "r")) == NULL) {
	bu_log("Cannot open input file (%s)\n", argv[0]);
	perror(prog);
	bu_exit(1, NULL);
    }
    if ((fd_out = wdb_fopen(argv[1])) == NULL) {
	bu_log("Cannot create new BRL-CAD file (%s)\n", argv[1]);
	perror(prog);
	bu_exit(1, NULL);
    }
    while (fgets(buf, BUFSIZ, fd_in)) {
	if (ferror(fd_in)) {
	    fprintf(stderr, "Ack! %d\nflaming death\n", ferror(fd_in));
	    return EXIT_FAILURE;
	}
	switch (*buf) {
	    case '#':	/* comment */
		continue;
	    case 'g':	/* group (region) */
		if (region)
		    write_region(region, fd_out);
		free_region(region);
		region = new_region(buf + 2);
		if (!region) {
		    perror(prog);
		    return EXIT_FAILURE;
		}
		break;
	    case 'v':	/* vertex */
		if (!region) {
		    perror(prog);
		    return EXIT_FAILURE;
		}
		add_vertex(region, buf + 2);
		break;
	    case 'f':	/* face */
		if (!region) {
		    perror(prog);
		    return EXIT_FAILURE;
		}
		add_face(region, buf + 2);
		break;
	    default:
		fprintf(stderr, "Unknown control code: %c\n", *buf);
		return EXIT_FAILURE;
	}
    }

    /* using the list generated in write regions, build the tree. */

    wdb_close(fd_out);

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
