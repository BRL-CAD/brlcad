/*                         P L Y - G . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
/** @file conv/ply/ply-g.c
 *
 * Program to convert the PLY format to the BRL-CAD format.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include "bio.h"

/* interface headers */
#include "vmath.h"
#include "bu/app.h"
#include "bu/debug.h"
#include "bu/getopt.h"
#include "bu/endian.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"
#include "rply.h"


static struct rt_wdb *out_fp=NULL;
static char *ply_file;
static char *brlcad_file;
static fastf_t scale_factor=1000.0;	/* default units are meters */
static int verbose=0;

static char *usage="Usage:\n\tply-g [-s scale_factor] [-d] [-v] input_file.ply output_file.g";

static int cur_vertex=-1;
static int cur_face=-1;

static void
log_elements(p_ply ply_fp)
{
    p_ply_property prop;
    const char *elem_name = NULL;
    const char *prop_name = NULL;
    long ninstances;
    p_ply_element elem = NULL;

    while ((elem = ply_get_next_element(ply_fp, elem))) {
	prop = NULL;
	ninstances = 0;
	if (!ply_get_element_info(elem, &elem_name, &ninstances)) {
	    bu_log("Could not get info of this element\n");
	    continue;
	}
	bu_log("There are %ld instances of the %s element:\n", ninstances, elem_name);

	/* iterate over all properties of current element */
	while ((prop = ply_get_next_property(elem, prop))) {
	    if (!ply_get_property_info(prop, &prop_name, NULL, NULL, NULL)) {
		bu_log("\t\tCould not get info of this element's property\n");
		continue;

	    }
	    bu_log("\t%s is a property\n", prop_name);
	}
    }
}

static int
vertex_cb(p_ply_argument argument)
{
    long vert_index;
    struct rt_bot_internal bot;
    struct rt_bot_internal *pbot = &bot;
    double botval = ply_get_argument_value(argument);
    if (!ply_get_argument_user_data(argument, (void **)&pbot, &vert_index)) {
	bu_bomb("Unable to import BOT");
    }
    if (vert_index == 0) {
	cur_vertex++;
	pbot->vertices[cur_vertex*3] = botval * (double) scale_factor;
    } else if (vert_index == 1) {
	pbot->vertices[cur_vertex*3+1] = botval * (double) scale_factor;
    } else if (vert_index == 2) {
	pbot->vertices[cur_vertex*3+2] = botval * (double) scale_factor;
    }
    return 1;
}

static int
color_cb(p_ply_argument argument)
{
    int *rgb;
    int (*prgb)[4] = NULL;
    long vert_index;
    double botval = ply_get_argument_value(argument);
    if (!ply_get_argument_user_data(argument, (void **)&prgb, &vert_index)) {
	bu_bomb("Unable to import BOT");
    }
    rgb = *prgb;
    if (botval < 0) {
	bu_log("ignoring invalid color\n");
	return 1;
    }
    if (rgb[vert_index] < 0) {
	rgb[vert_index] = botval;
    }
    else if (rgb[vert_index] != (int) botval) {
	rgb[3] = 1;
    }
    return 1;
}

static int
face_cb(p_ply_argument argument)
{
    long list_len, vert_index;
    struct rt_bot_internal bot;
    struct rt_bot_internal *pbot = &bot;
    int botval;
    if (!ply_get_argument_property(argument, NULL, &list_len, &vert_index)) {
	bu_bomb("Unable to import face lists");
    }
    if (list_len < 3 || list_len > 4) {
	bu_log("ignoring face with %ld vertices\n", list_len);
	return 1;
    }
    ply_get_argument_user_data(argument, (void **)&pbot, NULL);
    botval = ply_get_argument_value(argument);

    switch (vert_index) {
	case 0:
	    cur_face++;
	    pbot->faces[cur_face*3] = botval;
	    break;
	case 1:
	    pbot->faces[cur_face*3+1] = botval;
	    break;
	case 2:
	    pbot->faces[cur_face*3+2] = botval;
	    break;
	case 3:
	    /* need to break this into two BOT faces */
	    pbot->num_faces++;
	    pbot->faces = (int *)bu_realloc(pbot->faces, pbot->num_faces * 3 * sizeof(int), "bot_faces");
	    pbot->faces[cur_face*3+3] = botval;
	    pbot->faces[cur_face*3+4] = pbot->faces[cur_face*3];
	    pbot->faces[cur_face*3+5] = pbot->faces[cur_face*3+2];
	    cur_face++;
	    break;
	default:
	    /* will never execute because lists of length > 4 are not allowed */
	    break;
    }
    return 1;
}

int
main(int argc, char *argv[])
{
    struct rt_bot_internal *bot = NULL;
    int c, i;
    long nvertices, nfaces;
    struct bu_list head;
    struct wmember *wmp = NULL;
    p_ply ply_fp;
    char *periodpos = NULL;
    char *slashpos = NULL;
    struct bu_vls bot_name = BU_VLS_INIT_ZERO;
    struct bu_vls region_name = BU_VLS_INIT_ZERO;
    unsigned char rgb[3];
    int irgb[4] = {-1, -1, -1, 0};
    bu_setprogname(argv[0]);

    /* get command line arguments */
    while ((c = bu_getopt(argc, argv, "dvs:")) != -1) {
	switch (c) {
	    case 's':   /* scale factor */
		scale_factor = atof(bu_optarg);
		if (scale_factor < SQRT_SMALL_FASTF) {
		    bu_log("scale factor too small\n%s\n", usage);
		    goto free_mem;
		}
		break;
	    case 'd':	/* debug */
		bu_debug = BU_DEBUG_COREDUMP;
		break;
	    case 'v':	/* verbose */
		verbose = 1;
		break;
	    default:
		bu_log("%s\n", usage );
		break;
	}
    }

    if (argc - bu_optind < 2) {
	bu_log("%s\n", usage);
	goto free_mem;
    }

    ply_file = argv[bu_optind++];
    brlcad_file = argv[bu_optind];


    if ((out_fp = wdb_fopen(brlcad_file)) == NULL) {
	bu_log("ERROR: Cannot open output file (%s)\n", brlcad_file);
	goto free_mem;
    }
    /* malloc BOT storage */
    BU_ALLOC(bot, struct rt_bot_internal);
    bot->magic = RT_BOT_INTERNAL_MAGIC;
    bot->mode = RT_BOT_SURFACE;
    bot->orientation = RT_BOT_UNORIENTED;

    /* using the RPly library */
    ply_fp = ply_open(ply_file, NULL, 0, NULL);
    if (!ply_fp) {
	bu_log("ERROR: Cannot open PLY file (%s)\n", ply_file);
	goto free_mem;
    }
    if (!ply_read_header(ply_fp)) {
	bu_log("ERROR: File does not seem to be a PLY file\n");
	goto free_mem;
    }

    if (verbose) {
	log_elements(ply_fp);
    }


    nvertices = ply_set_read_cb(ply_fp, "vertex", "x", vertex_cb, bot, 0);
    ply_set_read_cb(ply_fp, "vertex", "y", vertex_cb, bot, 1);
    ply_set_read_cb(ply_fp, "vertex", "z", vertex_cb, bot, 2);

    ply_set_read_cb(ply_fp, "face", "red", color_cb, &irgb, 0);
    ply_set_read_cb(ply_fp, "face", "green", color_cb, &irgb, 1);
    ply_set_read_cb(ply_fp, "face", "blue", color_cb, &irgb, 2);
    nfaces = ply_set_read_cb(ply_fp, "face", "vertex_indices", face_cb, bot, 0);

    bot->num_vertices = nvertices;
    bot->num_faces = nfaces;

    if (bot->num_faces < 1 || bot->num_vertices < 1) {
	bu_log("This PLY file appears to contain no geometry!\n");
	goto free_bot;
    }
    bot->faces = (int *)bu_calloc(bot->num_faces * 3, sizeof(int), "bot faces");
    bot->vertices = (fastf_t *)bu_calloc(bot->num_vertices * 3, sizeof(fastf_t), "bot vertices");

    if (!ply_read(ply_fp)) {
	bu_log("ERROR: Cannot read PLY file (%s)\n", ply_file);
	goto free_bot;
    }


    ply_close(ply_fp);


    /* generate object names */
    periodpos = strrchr(ply_file, '.');
    slashpos = strrchr(ply_file, '/');
    if (slashpos) ply_file = slashpos + 1;

    bu_vls_sprintf(&bot_name, "%s", ply_file);
    if (periodpos)
	bu_vls_trunc(&bot_name, -1 * strlen(periodpos));
    bu_vls_printf(&bot_name, ".bot");
    bu_vls_sprintf(&region_name, "%s.r", bu_vls_addr(&bot_name));

    if (mk_bot(out_fp, bu_vls_addr(&bot_name), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, bot->num_vertices, bot->num_faces, bot->vertices, bot->faces, NULL, NULL)) {
	bu_log("ERROR: Failed to write BOT(%s) to database\n", bu_vls_addr(&bot_name));
	goto free_bot;
    }

    if (verbose) {
	bu_log("Wrote BOT %s\n", bu_vls_addr(&bot_name));
    }

    BU_LIST_INIT(&head);
    wmp = mk_addmember(bu_vls_addr(&bot_name), &head, NULL, WMOP_UNION);
    if (wmp == WMEMBER_NULL) {
	bu_log("ERROR: Failed to add %s to region\n", bu_vls_addr(&bot_name));
	goto free_bot;
    }

    if (irgb[0] < 0 || irgb[0] > 255) {
	if (verbose) {
	    bu_log("No color information specified\n");
	}
	if (mk_comb(out_fp, bu_vls_addr(&region_name), &head, 1, NULL, NULL, NULL, 1000, 0, 1, 100, 0, 0, 0)) {
	    bu_log("ERROR: Failed to write region(%s) to database\n", bu_vls_addr(&region_name));
	    goto free_bot;
	}
    }
    else if (irgb[3]) {
	bu_log("All triangles are not the same color. No color information written!\n");
	if (mk_comb(out_fp, bu_vls_addr(&region_name), &head, 1, NULL, NULL, NULL, 1000, 0, 1, 100, 0, 0, 0)) {
	    bu_log("ERROR: Failed to write region(%s) to database\n", bu_vls_addr(&region_name));
	    goto free_bot;
	}
    }
    else {
	for (i = 0; i < 3; i++) {
	    rgb[i] = (unsigned char) irgb[i];
	}
	if (mk_comb(out_fp, bu_vls_addr(&region_name), &head, 1, NULL, NULL, rgb, 1000, 0, 1, 100, 0, 0, 0)) {
	    bu_log("ERROR: Failed to write region(%s) to database\n", bu_vls_addr(&region_name));
	    goto free_bot;
	}
    }

    if (verbose) {
	bu_log("Wrote region %s\n", bu_vls_addr(&region_name));
    }

free_bot:
    if(bot->faces)
	bu_free(bot->faces, "bot->faces");
    if(bot->vertices)
	bu_free(bot->vertices, "bot->vertices");
free_mem:
    if(bot)
	bu_free(bot, "bot");
    if(out_fp)
	bu_free(out_fp, "out_fp");

    bu_vls_free(&bot_name);
    bu_vls_free(&region_name);

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
