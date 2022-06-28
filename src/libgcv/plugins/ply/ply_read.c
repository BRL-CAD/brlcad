/*                      P L Y _ R E A D . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
/** @file libgcv/plugins/ply/ply_read.c
 *
 * Program to convert the PLY format to the BRL-CAD format.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/getopt.h"
#include "gcv/api.h"
#include "bu/malloc.h"
#include "wdb.h"
#include "rply.h"

double scale_factor;                    // without refactor to callbacks theres no easy way to avoid this global
struct ply_read_options
{
    int verbose;                        /* verbose output flag */
    int debug;                          /* debug output flag */
};

struct conversion_state
{
    const struct gcv_opts *gcv_options;
    const struct ply_read_options *ply_read_options;

    const char *input_file;	        /* name of the input file */
    FILE *fd_in;		        /* input file */
    struct rt_wdb *fd_out;	        /* Resulting BRL-CAD file */

    struct rt_bot_internal* bot;        /* converted bot */    
    struct wmember wm;                  /* handle for in-memory combinations */
};

/* log_elements
 *
 * helper function which prints properties and instances to console when converting
 */
HIDDEN void
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

HIDDEN int
vertex_cb(p_ply_argument argument)
{
    static int cur_vertex = -1;
    long vert_index;
    struct rt_bot_internal bot;
    struct rt_bot_internal *pbot = &bot;
    double botval = ply_get_argument_value(argument);
    if (!ply_get_argument_user_data(argument, (void **)&pbot, &vert_index)) {
	bu_bomb("Unable to import BOT");
    }
    if (vert_index == 0) {
	cur_vertex++;
	pbot->vertices[cur_vertex*3] = botval * scale_factor;
    } else if (vert_index == 1) {
	pbot->vertices[cur_vertex*3+1] = botval * scale_factor;
    } else if (vert_index == 2) {
	pbot->vertices[cur_vertex*3+2] = botval * scale_factor;
    }
    return 1;
}

HIDDEN int
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

HIDDEN int
face_cb(p_ply_argument argument)
{
    static int cur_face = -1;
    
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

HIDDEN void
convert_input(struct conversion_state* pstate)
{
    p_ply ply_fp;
    unsigned char rgb[3];
    int irgb[4] = {-1, -1, -1, 0};
    struct bu_vls bot_name = BU_VLS_INIT_ZERO;
    struct bu_vls region_name = BU_VLS_INIT_ZERO;
    char* periodpos = NULL;
    char* slashpos = NULL;

    char* striped_input = (char*)pstate->input_file;

    /* using the RPly library */
    /* if / when a 'can_read' function is necessary, this error checking
     * should be migrated
     */
    ply_fp = ply_open(pstate->input_file, NULL, 0, NULL);
    if (!ply_fp) {
        bu_log("ERROR: Cannot open PLY file (%s)\n", pstate->input_file);
        goto free_mem;
    }
    if (!ply_read_header(ply_fp)) {
        bu_log("ERROR: File does not seem to be a PLY file\n");
        goto free_mem;
    }

    if (pstate->ply_read_options->verbose || pstate->gcv_options->verbosity_level) {
        log_elements(ply_fp);
    }

    // actual conversion starts here
    pstate->bot->num_vertices = ply_set_read_cb(ply_fp, "vertex", "x", vertex_cb, pstate->bot, 0);
    ply_set_read_cb(ply_fp, "vertex", "y", vertex_cb, pstate->bot, 1);
    ply_set_read_cb(ply_fp, "vertex", "z", vertex_cb, pstate->bot, 2);

    ply_set_read_cb(ply_fp, "face", "red", color_cb, &irgb, 0);
    ply_set_read_cb(ply_fp, "face", "green", color_cb, &irgb, 1);
    ply_set_read_cb(ply_fp, "face", "blue", color_cb, &irgb, 2);
    pstate->bot->num_faces = ply_set_read_cb(ply_fp, "face", "vertex_indices", face_cb, pstate->bot, 0);

    if (pstate->bot->num_faces < 1 || pstate->bot->num_vertices < 1) {
	bu_log("This PLY file appears to contain no geometry!\n");
	goto free_bot;
    }
    pstate->bot->faces = (int *)bu_calloc(pstate->bot->num_faces * 3, sizeof(int), "bot faces");
    pstate->bot->vertices = (fastf_t *)bu_calloc(pstate->bot->num_vertices * 3, sizeof(fastf_t), "bot vertices");

    if (!ply_read(ply_fp)) {
	bu_log("ERROR: Cannot read PLY file (%s)\n", pstate->input_file);
	goto free_bot;
    }

    ply_close(ply_fp);

    /* convert to .g
     * generate object name by striping input file of slashes and .ply */
    periodpos = strrchr(striped_input, '.');
    slashpos = strrchr(striped_input, '/');
    if (slashpos) striped_input = slashpos + 1;

    bu_vls_sprintf(&bot_name, "%s", striped_input);
    if (periodpos)
	bu_vls_trunc(&bot_name, -1 * strlen(periodpos));
    bu_vls_printf(&bot_name, ".bot");
    bu_vls_sprintf(&region_name, "%s.r", bu_vls_addr(&bot_name));

    if (mk_bot(pstate->fd_out, bu_vls_addr(&bot_name), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, pstate->bot->num_vertices, pstate->bot->num_faces, pstate->bot->vertices, pstate->bot->faces, NULL, NULL)) {
	bu_log("ERROR: Failed to write BOT(%s) to database\n", bu_vls_addr(&bot_name));
	goto free_bot;
    }

    if (pstate->ply_read_options->verbose || pstate->gcv_options->verbosity_level) {
	bu_log("Wrote BOT %s\n", bu_vls_addr(&bot_name));
    }

    if (mk_addmember(bu_vls_addr(&bot_name), &pstate->wm.l, NULL, WMOP_UNION) == WMEMBER_NULL) {
	bu_log("ERROR: Failed to add %s to region\n", bu_vls_addr(&bot_name));
	goto free_bot;
    }

    if (irgb[0] < 0 || irgb[0] > 255) {
        if (pstate->ply_read_options->verbose || pstate->gcv_options->verbosity_level) {
            bu_log("No color information specified\n");
        }
        if (mk_comb(pstate->fd_out, bu_vls_addr(&region_name), &pstate->wm.l, 1, NULL, NULL, NULL, 1000, 0, 1, 100, 0, 0, 0)) {
            bu_log("ERROR: Failed to write region(%s) to database\n", bu_vls_addr(&region_name));
            goto free_bot;
        }
    } else if (irgb[3]) {
        bu_log("All triangles are not the same color. No color information written!\n");
        if (mk_comb(pstate->fd_out, bu_vls_addr(&region_name), &pstate->wm.l, 1, NULL, NULL, NULL, 1000, 0, 1, 100, 0, 0, 0)) {
            bu_log("ERROR: Failed to write region(%s) to database\n", bu_vls_addr(&region_name));
            goto free_bot;
        }
    } else {
        for (int i = 0; i < 3; i++) {
            rgb[i] = (unsigned char) irgb[i];
        }
        if (mk_comb(pstate->fd_out, bu_vls_addr(&region_name), &pstate->wm.l, 1, NULL, NULL, rgb, 1000, 0, 1, 100, 0, 0, 0)) {
            bu_log("ERROR: Failed to write region(%s) to database\n", bu_vls_addr(&region_name));
            goto free_bot;
        }
    }

    if (pstate->ply_read_options->verbose || pstate->gcv_options->verbosity_level) {
	bu_log("Wrote region %s\n", bu_vls_addr(&region_name));
    }

free_bot:
    if(pstate->bot->faces)
	bu_free(pstate->bot->faces, "pstate->bot->faces");
    if(pstate->bot->vertices)
	bu_free(pstate->bot->vertices, "pstate->bot->vertices");
free_mem:
    if(pstate->bot)
	bu_free(pstate->bot, "pstate->bot");

    bu_vls_free(&bot_name);
    bu_vls_free(&region_name);
}

HIDDEN int
ply_can_read(const char* data)
{
    // TODO - currently the 'can_read' is not being used by gcv
    if (!data) return 0;
    return 1;
}

HIDDEN void
ply_read_create_opts(struct bu_opt_desc** options_desc, void** dest_options_data)
{
    struct ply_read_options *options_data;

    BU_ALLOC(options_data, struct ply_read_options);
    *dest_options_data = options_data;
    *options_desc = (struct bu_opt_desc *)bu_malloc(4 * sizeof(struct bu_opt_desc), "options_desc");

    scale_factor = 1000.0;                      /* default units are meters */
    options_data->verbose = 0;                  /* default flag = off */
    options_data->debug = 0;                    /* default flag = off */

    BU_OPT((*options_desc)[0], "s", "scale_factor", "float", bu_opt_fastf_t, &scale_factor, "specify the scale factor");
    BU_OPT((*options_desc)[1], "v", "verbose",      "",      NULL,           &options_data->verbose,      "specify to run with verbose output");
    BU_OPT((*options_desc)[2], "d", "debug",        "",      NULL,           &options_data->debug,        "specify specify to run with debug output");
    BU_OPT_NULL((*options_desc)[3]);
}

HIDDEN void
ply_read_free_opts(void* options_data)
{
    bu_free(options_data, "options_data");
}

HIDDEN int
ply_read_gcv(struct gcv_context* context, const struct gcv_opts* gcv_options, const void* options_data, const char* source_path)
{
    struct conversion_state state;

    state.gcv_options = gcv_options;
    state.ply_read_options = (struct ply_read_options*)options_data;
    state.input_file = source_path;
    state.fd_out = context->dbip->dbi_wdbp;

    if ((state.fd_in = fopen(source_path, "rb")) == NULL) {
	bu_log("Cannot open input file (%s)\n", source_path);
	perror("libgcv");
	bu_exit(1, NULL);
    }

    mk_id_units(state.fd_out, "Conversion from PLY format", "mm");

    /* initialize necessary components */
    BU_LIST_INIT(&state.wm.l);
    
    BU_ALLOC(state.bot, struct rt_bot_internal);
    state.bot->magic = RT_BOT_INTERNAL_MAGIC;
    state.bot->mode = RT_BOT_SURFACE;
    state.bot->orientation = RT_BOT_UNORIENTED;

    convert_input(&state);

    fclose(state.fd_in);

    return 1;
}


/* filter setup */
static const struct gcv_filter gcv_conv_ply_read = {
    "PLY Reader", GCV_FILTER_READ, BU_MIME_MODEL_PLY, ply_can_read,
    ply_read_create_opts, ply_read_free_opts, ply_read_gcv
};

extern const struct gcv_filter gcv_conv_ply_write;
static const struct gcv_filter* const filters[] = {&gcv_conv_ply_read, &gcv_conv_ply_write, NULL};

const struct gcv_plugin gcv_plugin_info_s = { filters };

COMPILER_DLLEXPORT const struct gcv_plugin *
gcv_plugin_info(){ return &gcv_plugin_info_s; }

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
