/*                      P L Y _ R E A D . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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

#include <limits.h>
#include <math.h>
#include <string.h>
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#include "bu/malloc.h"
#include "bu/path.h"
#include "gcv/api.h"
#include "wdb.h"
#include "rply.h"

#define PLY_COMPONENTS 3
#define PLY_MAX_FACE_VERTICES 4
#define PLY_TRIANGLES_PER_FACE 2
#define PLY_DEFAULT_SCALE 1000.0

struct ply_read_options {
    fastf_t scale;
    int verbose;
    int debug;
};

struct conversion_state {
    const struct ply_read_options *options;
    struct rt_bot_internal *bot;
    size_t face_count;
    size_t face_capacity;
    int face[PLY_MAX_FACE_VERTICES];
    int color[4];
};

/* Generated names must not replace geometry already in the context. */
static int
ply_name_available(struct bu_vls *name, void *data)
{
    struct db_i *database = (struct db_i *)data;
    return db_lookup(database, bu_vls_cstr(name), LOOKUP_QUIET) == RT_DIR_NULL;
}

static int
ply_unique_name(struct db_i *database, struct bu_vls *name)
{
    return ply_name_available(name, database) ||
	bu_vls_incr(name, NULL, NULL, ply_name_available, database) >= 0;
}

static int
vertex_cb(p_ply_argument argument)
{
    struct conversion_state *state = NULL;
    long coordinate, vertex;
    double value = ply_get_argument_value(argument);
    if (!ply_get_argument_user_data(argument, (void **)&state, &coordinate) || !state ||
	!ply_get_argument_element(argument, NULL, &vertex) || coordinate < 0 || coordinate >= PLY_COMPONENTS ||
	vertex < 0 || (size_t)vertex >= state->bot->num_vertices || !isfinite(value) || !isfinite(value * state->options->scale))
	return 0;
    state->bot->vertices[(size_t)vertex * PLY_COMPONENTS + coordinate] = value * state->options->scale;
    return 1;
}

static int
color_cb(p_ply_argument argument)
{
    struct conversion_state *state = NULL;
    long channel;
    double value = ply_get_argument_value(argument);
    if (!ply_get_argument_user_data(argument, (void **)&state, &channel) || !state ||
	channel < 0 || channel >= PLY_COMPONENTS || !isfinite(value) || value < 0 || value > 255)
	return 0;
    if (state->color[channel] < 0)
	state->color[channel] = (int)value;
    else if (state->color[channel] != (int)value)
	state->color[3] = 1;
    return 1;
}

static int
face_cb(p_ply_argument argument)
{
    struct conversion_state *state = NULL;
    long length, index;
    if (!ply_get_argument_user_data(argument, (void **)&state, NULL) || !state ||
	!ply_get_argument_property(argument, NULL, &length, &index))
	return 0;
    if (length < PLY_COMPONENTS || length > PLY_MAX_FACE_VERTICES) {
	bu_log("PLY: only triangular and quadrilateral faces are supported\n");
	return 0;
    }
    if (index < 0)
	return 1;
    double value = ply_get_argument_value(argument);
    /* BoT indices are discrete; a tolerance must not round a fractional index. */
    if (index >= length || !isfinite(value) || value < 0 || value >= (double)state->bot->num_vertices ||
	value > floor(value)) {
	bu_log("PLY: invalid face vertex index\n");
	return 0;
    }
    state->face[index] = (int)value;
    if (index != length - 1)
	return 1;
    size_t triangles = length == PLY_COMPONENTS ? 1 : PLY_TRIANGLES_PER_FACE;
    if (triangles > state->face_capacity - state->face_count)
	return 0;
    int *destination = state->bot->faces + state->face_count * PLY_COMPONENTS;
    memcpy(destination, state->face, PLY_COMPONENTS * sizeof(int));
    if (triangles == PLY_TRIANGLES_PER_FACE) {
	destination[3] = state->face[0];
	destination[4] = state->face[2];
	destination[5] = state->face[3];
    }
    state->face_count += triangles;
    return 1;
}

static int
ply_read_gcv(struct gcv_context *context, const struct gcv_opts *gcv_options,
    const void *options_data, const char *source_path)
{
    const struct ply_read_options *options = (const struct ply_read_options *)options_data;
    struct conversion_state state;
    struct rt_bot_internal bot;
    struct wmember members;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct bu_vls region = BU_VLS_INIT_ZERO;
    p_ply input = NULL;
    struct rt_wdb *database = NULL;
    long vertices, faces;
    int status = 0;
    unsigned char rgb[PLY_COMPONENTS];
    const unsigned char *color = NULL;
    memset(&state, 0, sizeof(state));
    memset(&bot, 0, sizeof(bot));
    state.options = options;
    state.bot = &bot;
    state.color[0] = state.color[1] = state.color[2] = -1;
    BU_LIST_INIT(&members.l);
    if (!options || !isfinite(options->scale) || ZERO(options->scale))
	goto cleanup;
    input = ply_open(source_path, NULL, 0, NULL);
    if (!input || !ply_read_header(input))
	goto cleanup;
    vertices = ply_set_read_cb(input, "vertex", "x", vertex_cb, &state, 0);
    if (vertices <= 0 || vertices > INT_MAX ||
	ply_set_read_cb(input, "vertex", "y", vertex_cb, &state, 1) != vertices ||
	ply_set_read_cb(input, "vertex", "z", vertex_cb, &state, 2) != vertices)
	goto cleanup;
    faces = ply_set_read_cb(input, "face", "vertex_indices", face_cb, &state, 0);
    if (faces <= 0 || (size_t)faces > SIZE_MAX / PLY_TRIANGLES_PER_FACE / PLY_COMPONENTS / sizeof(int) ||
	(size_t)vertices > SIZE_MAX / PLY_COMPONENTS / sizeof(fastf_t))
	goto cleanup;
#if defined(HAVE_SYS_STAT_H) && defined(S_ISREG)
    {
	struct stat info;
	if (!stat(source_path, &info) && S_ISREG(info.st_mode) && info.st_size >= 0 &&
	    ((uintmax_t)vertices > (uintmax_t)info.st_size / PLY_COMPONENTS ||
	     (uintmax_t)faces > (uintmax_t)info.st_size / (PLY_COMPONENTS + 1)))
	    goto cleanup;
    }
#endif
    ply_set_read_cb(input, "face", "red", color_cb, &state, 0);
    ply_set_read_cb(input, "face", "green", color_cb, &state, 1);
    ply_set_read_cb(input, "face", "blue", color_cb, &state, 2);
    bot.num_vertices = (size_t)vertices;
    state.face_capacity = (size_t)faces * PLY_TRIANGLES_PER_FACE;
    bot.vertices = (fastf_t *)bu_calloc(bot.num_vertices * PLY_COMPONENTS, sizeof(fastf_t), "PLY vertices");
    bot.faces = (int *)bu_calloc(state.face_capacity * PLY_COMPONENTS, sizeof(int), "PLY faces");
    if (!ply_read(input) || !state.face_count)
	goto cleanup;
    if (!ply_close(input)) {
	input = NULL;
	goto cleanup;
    }
    input = NULL;
    bot.num_faces = state.face_count;
    database = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);
    if (!database)
	goto cleanup;
    bu_path_component(&name, source_path, BU_PATH_BASENAME);
    {
	char *extension = strrchr(bu_vls_addr(&name), '.');
	if (extension)
	    bu_vls_trunc(&name, extension - bu_vls_addr(&name));
    }
    bu_vls_strcat(&name, ".bot");
    if (!ply_unique_name(context->dbip, &name))
	goto cleanup;
    bu_vls_printf(&region, "%s.r", bu_vls_cstr(&name));
    if (!ply_unique_name(context->dbip, &region) ||
	mk_id_units(database, "Conversion from PLY format", "mm") < 0)
	goto cleanup;
    if (mk_bot(database, bu_vls_cstr(&name), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0,
	bot.num_vertices, bot.num_faces, bot.vertices, bot.faces, NULL, NULL) ||
	mk_addmember(bu_vls_cstr(&name), &members.l, NULL, WMOP_UNION) == WMEMBER_NULL)
	goto cleanup;
    if (state.color[0] >= 0 && state.color[1] >= 0 && state.color[2] >= 0 && !state.color[3]) {
	for (size_t i = 0; i < PLY_COMPONENTS; ++i)
	    rgb[i] = (unsigned char)state.color[i];
	color = rgb;
    } else if (state.color[3]) {
	bu_log("PLY: varying face colors cannot be represented by one region color\n");
    }
    if (mk_comb(database, bu_vls_cstr(&region), &members.l, 1, NULL, NULL, color, 1000, 0, 0, 100, 0, 0, 0))
	goto cleanup;
    status = 1;
    if (options->verbose || options->debug || gcv_options->verbosity_level)
	bu_log("PLY: imported %zu vertices and %zu triangles\n", bot.num_vertices, bot.num_faces);
cleanup:
    if (input)
	ply_close(input);
    mk_freemembers(&members.l);
    bu_free(bot.vertices, "PLY vertices");
    bu_free(bot.faces, "PLY faces");
    bu_vls_free(&name);
    bu_vls_free(&region);
    if (!status)
	bu_log("Cannot import PLY input %s\n", source_path);
    return status;
}

static int
ply_can_read(const char *filename)
{
    if (!filename)
	return 0;
    FILE *input = fopen(filename, "rb");
    if (!input)
	return 0;
    char signature[5];
    size_t count = fread(signature, 1, sizeof(signature), input);
    int matched = !ferror(input) && ((count >= 4 && !memcmp(signature, "ply\n", 4)) ||
	(count >= 5 && !memcmp(signature, "ply\r\n", 5)));
    if (fclose(input))
	return 0;
    return matched;
}

static void
ply_read_create_opts(struct bu_opt_desc **descriptions, void **options_data)
{
    struct ply_read_options *options;
    BU_ALLOC(options, struct ply_read_options);
    options->scale = PLY_DEFAULT_SCALE;
    *options_data = options;
    *descriptions = (struct bu_opt_desc *)bu_calloc(4, sizeof(struct bu_opt_desc), "PLY options");
    BU_OPT((*descriptions)[0], "s", "scale_factor", "float", bu_opt_fastf_t, &options->scale, "Coordinate scale (default meters to millimeters)");
    BU_OPT((*descriptions)[1], "v", "verbose", "", NULL, &options->verbose, "Verbose conversion output");
    BU_OPT((*descriptions)[2], "d", "debug", "", NULL, &options->debug, "Debug conversion output");
    BU_OPT_NULL((*descriptions)[3]);
}

static void
ply_read_free_opts(void *options_data)
{
    bu_free(options_data, "PLY options");
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
gcv_plugin_info(void){ return &gcv_plugin_info_s; }

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
