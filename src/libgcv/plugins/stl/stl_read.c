/*                      S T L _ R E A D . C
 * BRL-CAD
 *
 * Copyright (c) 2002-2026 United States Government as represented by
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
/** @file stl_read.c
 *
 * Note that binary STL format use a little-endian byte ordering where
 * bytes at lower addresses have lower significance.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#include "bnetwork.h"
#include "bio.h"

#include "bu/cv.h"
#include "bu/getopt.h"
#include "bu/path.h"
#include "bu/units.h"
#include "bu/vls.h"
#include "gcv/api.h"
#include "../mime_probe.h"
#include "mime_stl.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"


struct stl_read_options
{
    int binary; /* flag indicating input is binary format */
    int starting_id;
    int const_id; /* Constant ident number (assigned to all regions if non-negative) */
    int mat_code; /* default material code */
};


struct conversion_state
{
    const struct gcv_opts *gcv_options;
    const struct stl_read_options *stl_read_options;

    const char *input_file; /* name of the input file */
    FILE *fd_in;	    /* input file */
    struct rt_wdb *fd_out;  /* Resulting BRL-CAD file */

    struct wmember all_head;
    struct bg_vert_tree *tree;
    int *bot_faces; /* array of ints (indices into tree->the_array array) three per face */

    int64_t id_no;   /* Ident numbers */
    int bot_fsize; /* current size of the bot_faces array */
    int bot_fcurr; /* current bot face */
    int failed;
};


/* Size of blocks of faces to malloc */
#define BOT_FBLOCK 128

#define MAX_LINE_SIZE 512

static void
stl_add_face(struct conversion_state *pstate, int face[3])
{
    const size_t maximum_faces = SIZE_MAX / (3 * sizeof(int)) < INT_MAX / 3 ?
	SIZE_MAX / (3 * sizeof(int)) : INT_MAX / 3;
    if ((size_t)pstate->bot_fcurr >= maximum_faces) {
	pstate->failed = 1;
	return;
    }
    if (!pstate->bot_faces) {
	pstate->bot_faces = (int *)bu_malloc(3 * BOT_FBLOCK * sizeof(int), "bot_faces");
	pstate->bot_fsize = BOT_FBLOCK;
	pstate->bot_fcurr = 0;
    } else if (pstate->bot_fcurr >= pstate->bot_fsize) {
	size_t capacity = (size_t)pstate->bot_fsize + BOT_FBLOCK;
	if (capacity > maximum_faces) capacity = maximum_faces;
	pstate->bot_fsize = (int)capacity;
	pstate->bot_faces = (int *)bu_realloc(pstate->bot_faces, 3 * capacity * sizeof(int), "bot_faces increase");
    }

    VMOVE(&pstate->bot_faces[3*pstate->bot_fcurr], face);
    pstate->bot_fcurr++;
}

static int
stl_name_available(struct bu_vls *n, void *data)
{
    struct db_i *dbip = (struct db_i *)data;
    if (db_lookup(dbip, bu_vls_cstr(n), LOOKUP_QUIET) == RT_DIR_NULL) return 1;
    return 0;
}

static void
stl_unique_name(struct conversion_state *pstate, struct bu_vls *name)
{
    struct db_i *dbip = pstate->fd_out->dbip;

    struct bu_vls tname = BU_VLS_INIT_ZERO;
    const char *c = bu_vls_cstr(name);
    int skip = 0;
    while (*c != '\0') {
	if (skip && *c != '>') {
	    c++;
	    continue;
	}
	if (*c == '<') {
	    skip = 1;
	    c++;
	    continue;
	}
	if (*c == '>') {
	    skip = 0;
	    c++;
	    continue;
	}
	bu_vls_putc(&tname, *c);
	c++;
    }

    const char *keep_chars = "+-=._";
    const char *dedup_chars = "._ ";
    const char *collapse_chars = "._ ";
    bu_vls_simplify(&tname, keep_chars, dedup_chars, collapse_chars);

    if (db_lookup(dbip, bu_vls_cstr(&tname), LOOKUP_QUIET) != RT_DIR_NULL)
	bu_vls_incr(&tname, NULL, NULL, &stl_name_available, (void *)dbip);

    bu_vls_sprintf(name, "%s", bu_vls_cstr(&tname));
    bu_vls_free(&tname);
}

static int
stl_write_part(struct conversion_state *state, const char *solid, const char *region, const unsigned char *color)
{
    struct wmember members;
    BU_LIST_INIT(&members.l);
    if (state->id_no > INT_MAX ||
	mk_bot(state->fd_out, solid, RT_BOT_SOLID, RT_BOT_UNORIENTED, 0,
	    state->tree->curr_vert, state->bot_fcurr, state->tree->the_array,
	    state->bot_faces, NULL, NULL) ||
	db5_update_attribute(solid, "importer", "gcv-stl", state->fd_out->dbip))
	return 0;
    int status = mk_addmember(solid, &members.l, NULL, WMOP_UNION) != WMEMBER_NULL &&
	!mk_lrcomb(state->fd_out, region, &members, 1, NULL, NULL, color,
	    (int)state->id_no, 0, state->stl_read_options->mat_code, 100, 0) &&
	mk_addmember(region, &state->all_head.l, NULL, WMOP_UNION) != WMEMBER_NULL;
    mk_freemembers(&members.l);
    if (status && !state->stl_read_options->const_id)
	++state->id_no;
    return status;
}

static void
stl_read_ascii_part(struct conversion_state *pstate, char line[MAX_LINE_SIZE])
{
    char line1[MAX_LINE_SIZE];
    struct bu_vls solid_name = BU_VLS_INIT_ZERO;
    struct bu_vls region_name = BU_VLS_INIT_ZERO;

    int start;
    int i;
    int face_count=0;
    int degenerate_count=0;
    float colr[3]={0.5, 0.5, 0.5};
    unsigned char color[3]={ 128, 128, 128 };
    vect_t normal={0, 0, 0};
    int complete = 0;

    pstate->bot_fcurr = 0;
    bg_vert_tree_clean(pstate->tree);


    bu_vls_sprintf(&region_name, "%s", line);
    bu_vls_trimspace(&region_name);

    /* Sanity */
    if (bu_strncmp(bu_vls_cstr(&region_name), "solid", 5) && bu_strncmp(bu_vls_cstr(&region_name), "SOLID", 5)) {
	bu_log("stl_read_ascii_part: Called for non-part\n%s\n", line);
	bu_vls_free(&region_name);
	return;
    }

    bu_vls_nibble(&region_name, 5);
    bu_vls_trimspace(&region_name);

    if (!bu_vls_strlen(&region_name)) {
	/* build a name from the file name */
	bu_path_component(&region_name, pstate->input_file, BU_PATH_BASENAME_EXTLESS);
    }

    // Do the unique name check before adding the suffix to clean up the name
    // as much as possible
    stl_unique_name(pstate, &region_name);

    // Add the suffix
    bu_vls_printf(&region_name, ".r");

    // Make sure we're still unique
    stl_unique_name(pstate, &region_name);
    bu_log("Converting Part: %s\n", bu_vls_cstr(&region_name));

    bu_path_component(&solid_name, bu_vls_cstr(&region_name), BU_PATH_BASENAME_EXTLESS);
    stl_unique_name(pstate, &solid_name);
    bu_vls_printf(&solid_name, ".s");
    stl_unique_name(pstate, &solid_name);

    if (pstate->gcv_options->verbosity_level)
	bu_log("\tUsing solid name: %s\n", bu_vls_cstr(&solid_name));

    while (bu_fgets(line1, MAX_LINE_SIZE, pstate->fd_in) != NULL) {
	start = (-1);
	while (isspace((unsigned char)line1[++start]));
	if (!bu_strncmp(&line1[start], "endsolid", 8) || !bu_strncmp(&line1[start], "ENDSOLID", 8)) {
	    complete = 1;
	    break;
	} else if (!bu_strncmp(&line1[start], "color", 5) || !bu_strncmp(&line1[start], "COLOR", 5)) {
	    if (bu_sscanf(&line1[start+5], "%f%f%f", &colr[0], &colr[1], &colr[2]) != 3)
		goto invalid;
	    for (i=0; i<3; i++) {
		if (!isfinite(colr[i]) || colr[i] < 0.0 || colr[i] > 1.0)
		    goto invalid;
		color[i] = (unsigned char)(colr[i] * 255.0);
	    }
	} else if (!bu_strncmp(&line1[start], "normal", 6) || !bu_strncmp(&line1[start], "NORMAL", 6)) {
	    float x, y, z;

	    start += 6;
	    if (bu_sscanf(&line1[start], "%f%f%f", &x, &y, &z) != 3 ||
		!isfinite(x) || !isfinite(y) || !isfinite(z))
		goto invalid;
	    VSET(normal, x, y, z);
	} else if (!bu_strncmp(&line1[start], "facet", 5) || !bu_strncmp(&line1[start], "FACET", 5)) {
	    VSET(normal, 0.0, 0.0, 0.0);

	    start += 4;
	    while (line1[++start] && isspace((unsigned char)line1[start]));

	    if (line1[start]) {
		if (!bu_strncmp(&line1[start], "normal", 6) || !bu_strncmp(&line1[start], "NORMAL", 6)) {
		    float x, y, z;

		    start += 6;
		    if (bu_sscanf(&line1[start], "%f%f%f", &x, &y, &z) != 3 ||
			!isfinite(x) || !isfinite(y) || !isfinite(z))
			goto invalid;
		    VSET(normal, x, y, z);
		}
	    }
	} else if (!bu_strncmp(&line1[start], "outer loop", 10) || !bu_strncmp(&line1[start], "OUTER LOOP", 10)) {
	    int endloop=0;
	    int vert_no=0;
	    int tmp_face[3] = {0, 0, 0};

	    while (!endloop) {
		if (bu_fgets(line1, MAX_LINE_SIZE, pstate->fd_in) == NULL)
		    goto invalid;

		start = (-1);
		while (isspace((unsigned char)line1[++start]));

		if (!bu_strncmp(&line1[start], "endloop", 7) || !bu_strncmp(&line1[start], "ENDLOOP", 7))
		    endloop = 1;
		else if (!bu_strncmp(&line1[start], "vertex", 6) || !bu_strncmp(&line1[start], "VERTEX", 6)) {
		    double x, y, z;

		    if (vert_no >= 3 || bu_sscanf(&line1[start+6], "%lf%lf%lf", &x, &y, &z) != 3)
			goto invalid;
		    x *= pstate->gcv_options->scale_factor;
		    y *= pstate->gcv_options->scale_factor;
		    z *= pstate->gcv_options->scale_factor;
		    if (!isfinite(x) || !isfinite(y) || !isfinite(z))
			goto invalid;
		    size_t vertex = bg_vert_tree_add(pstate->tree, x, y, z, pstate->gcv_options->calculational_tolerance.dist_sq);
		    if (vertex > INT_MAX) goto invalid;
		    tmp_face[vert_no++] = (int)vertex;
		} else {
		    goto invalid;
		}
	    }

	    if (vert_no != 3) goto invalid;

	    /* check for degenerate faces */
	    if (tmp_face[0] == tmp_face[1]) {
		degenerate_count++;
		continue;
	    }

	    if (tmp_face[0] == tmp_face[2]) {
		degenerate_count++;
		continue;
	    }

	    if (tmp_face[1] == tmp_face[2]) {
		degenerate_count++;
		continue;
	    }

	    if (pstate->gcv_options->debug_mode) {
		int n;

		bu_log("Making Face:\n");
		for (n=0; n<3; n++)
		    bu_log("\tvertex #%d: (%g %g %g)\n", tmp_face[n], V3ARGS(&pstate->tree->the_array[3*tmp_face[n]]));
		VPRINT(" normal", normal);
	    }

	    stl_add_face(pstate, tmp_face);
	    if (pstate->failed) goto invalid;
	    face_count++;
	}
    }

    if (!complete || ferror(pstate->fd_in)) goto invalid;

    /* Check if this part has any solid parts */
    if (face_count == 0) {
	bu_log("\t%s has no solid parts, ignoring\n", bu_vls_cstr(&region_name));
	if (degenerate_count)
	    bu_log("\t%d faces were degenerate\n", degenerate_count);
	bu_vls_free(&region_name);
	bu_vls_free(&solid_name);

	return;
    } else {
	if (degenerate_count)
	    bu_log("\t%d faces were degenerate\n", degenerate_count);
    }

    if (!stl_write_part(pstate, bu_vls_cstr(&solid_name), bu_vls_cstr(&region_name), color))
	goto invalid;
    goto cleanup;
invalid:
    pstate->failed = 1;
    bu_log("STL: malformed or incomplete ASCII part\n");
cleanup:
    bu_vls_free(&region_name);
    bu_vls_free(&solid_name);
}

static void
stl_read_binary_part(struct conversion_state *pstate)
{
    unsigned char buf[MIME_STL_TRIANGLE_SIZE];
    unsigned char network[48];
    uint32_t num_facets=0;
    float flts[12];
    vect_t normal;
    int tmp_face[3];
    struct bu_vls solid_name = BU_VLS_INIT_ZERO;
    struct bu_vls region_name = BU_VLS_INIT_ZERO;
    int face_count=0;
    int degenerate_count=0;

    bu_vls_strcat(&solid_name, "s.stl");
    bu_vls_strcat(&region_name, "r.stl");
    stl_unique_name(pstate, &solid_name);
    stl_unique_name(pstate, &region_name);
    bu_log("\tUsing solid name: %s\n", bu_vls_cstr(&solid_name));


    if (fread(buf, 1, 4, pstate->fd_in) != 4)
	goto invalid;
    num_facets = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) |
	((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    bu_log("\t%u facets\n", num_facets);
    for (uint32_t facet = 0; facet < num_facets; ++facet) {
	double pt[3];
	if (fread(buf, 1, MIME_STL_TRIANGLE_SIZE, pstate->fd_in) != MIME_STL_TRIANGLE_SIZE)
	    goto invalid;
	for (size_t i = 0; i < 12; ++i)
	    for (size_t byte = 0; byte < 4; ++byte)
		network[i * 4 + byte] = buf[i * 4 + 3 - byte];
	bu_cv_ntohf((unsigned char *)flts, network, 12);
	for (size_t i = 0; i < 12; ++i)
	    if (!isfinite(flts[i])) goto invalid;
	VMOVE(normal, flts);
	for (size_t vertex = 0; vertex < 3; ++vertex) {
	    VSCALE(pt, flts + (vertex + 1) * 3, pstate->gcv_options->scale_factor);
	    if (!isfinite(pt[0]) || !isfinite(pt[1]) || !isfinite(pt[2])) goto invalid;
	    size_t index = bg_vert_tree_add(pstate->tree, V3ARGS(pt), pstate->gcv_options->calculational_tolerance.dist_sq);
	    if (index > INT_MAX) goto invalid;
	    tmp_face[vertex] = (int)index;
	}

	/* check for degenerate faces */
	if (tmp_face[0] == tmp_face[1]) {
	    degenerate_count++;
	    continue;
	}

	if (tmp_face[0] == tmp_face[2]) {
	    degenerate_count++;
	    continue;
	}

	if (tmp_face[1] == tmp_face[2]) {
	    degenerate_count++;
	    continue;
	}

	if (pstate->gcv_options->debug_mode) {
	    int n;

	    bu_log("Making Face:\n");
	    for (n=0; n<3; n++)
		bu_log("\tvertex #%d: (%g %g %g)\n", tmp_face[n], V3ARGS(&pstate->tree->the_array[3*tmp_face[n]]));
	    VPRINT(" normal", normal);
	}

	stl_add_face(pstate, tmp_face);
	if (pstate->failed) goto invalid;
	face_count++;
    }

    if (fgetc(pstate->fd_in) != EOF || ferror(pstate->fd_in)) goto invalid;

    /* Check if this part has any solid parts */
    if (face_count == 0) {
	bu_log("\tpart has no solid parts, ignoring\n");
	if (degenerate_count)
	    bu_log("\t%d faces were degenerate\n", degenerate_count);
	goto cleanup;
    } else {
	if (degenerate_count)
	    bu_log("\t%d faces were degenerate\n", degenerate_count);
    }

    if (!stl_write_part(pstate, bu_vls_cstr(&solid_name), bu_vls_cstr(&region_name), NULL))
	goto invalid;
    goto cleanup;
invalid:
    pstate->failed = 1;
    bu_log("STL: invalid or truncated binary triangle records\n");
cleanup:
    bu_vls_free(&region_name);
    bu_vls_free(&solid_name);
}


static void
stl_read_input(struct conversion_state *pstate)
{
    char line[ MAX_LINE_SIZE ];

    if (pstate->stl_read_options->binary) {
	if (fread(line, 80, 1, pstate->fd_in) != 1) {
	    pstate->failed = 1;
	    bu_log("STL: truncated binary header\n");
	    return;
	}
	line[80] = '\0';
	bu_log("header data:\n%s\n\n", line);
	stl_read_binary_part(pstate);
    } else {
	while (bu_fgets(line, MAX_LINE_SIZE, pstate->fd_in) != NULL) {
	    int start = 0;
	    while (line[start] != '\0' && isspace((unsigned char)line[start])) {
		start++;
	    }
	    if (!line[start]) continue;
	    if (!bu_strncmp(&line[start], "solid", 5) || !bu_strncmp(&line[start], "SOLID", 5))
		stl_read_ascii_part(pstate, line);
	    else
		pstate->failed = 1;
	    if (pstate->failed) return;
	}
    }
}


static void
stl_read_create_opts(struct bu_opt_desc **options_desc, void **dest_options_data)
{
    struct stl_read_options *options_data;

    BU_ALLOC(options_data, struct stl_read_options);
    *dest_options_data = options_data;
    *options_desc = (struct bu_opt_desc *)bu_malloc(5 * sizeof(struct bu_opt_desc), "options_desc");

    options_data->binary = 0;
    options_data->starting_id = 1000;
    options_data->const_id = 0;
    options_data->mat_code = 0;

    BU_OPT((*options_desc)[0], NULL, "binary", NULL,
	    NULL, &options_data->binary,
	    "specify that the input is in binary STL format");

    BU_OPT((*options_desc)[1], NULL, "starting-ident", "number",
	    bu_opt_int, &options_data->starting_id,
	    "specify the starting ident for the regions created");

    BU_OPT((*options_desc)[2], NULL, "constant-ident", NULL,
	    NULL, &options_data->const_id,
	    "specify that the starting ident should remain constant");

    BU_OPT((*options_desc)[3], NULL, "material", "code",
	    bu_opt_int, &options_data->mat_code,
	    "specify the material code that will be assigned to created regions");

    BU_OPT_NULL((*options_desc)[4]);
}


static void
stl_read_free_opts(void *options_data)
{
    bu_free(options_data, "options_data");
}


static int
stl_read(struct gcv_context *context, const struct gcv_opts *gcv_options, const void *options_data, const char *source_path)
{
    struct conversion_state state;
    struct stl_read_options read_options = *(const struct stl_read_options *)options_data;
    const char *encoding = bu_mime_traits_get(gcv_target_traits(context), "encoding");
    if (encoding) {
	if (read_options.binary && BU_STR_EQUAL(encoding, "ascii")) {
	    bu_log("STL encoding=ascii conflicts with --binary\n");
	    return 0;
	}
	read_options.binary = BU_STR_EQUAL(encoding, "binary");
    }

    memset(&state, 0, sizeof(state));

    state.gcv_options = gcv_options;
    state.stl_read_options = &read_options;
    state.id_no = state.stl_read_options->starting_id;
    state.input_file = source_path;
    struct rt_wdb *wdbp = wdb_dbopen(context->dbip, RT_WDB_TYPE_DB_INMEM);
    state.fd_out = wdbp;

    if ((state.fd_in = fopen(source_path, "rb")) == NULL) {
	bu_log("Cannot open input file (%s)\n", source_path);
	perror("libgcv");
	return 0;
    }

    const struct bu_mime_traits *requested = gcv_target_request_traits(context);
    const struct mime_stl_profile profile = mime_stl_file_profile(state.fd_in, bu_mime_traits_get(requested, "encoding") != NULL);
    struct bu_mime_traits observed = BU_MIME_TRAITS_INIT_ZERO;
    int valid = !ferror(state.fd_in) && mime_stl_append(&profile, &observed) &&
	gcv_probe_file_traits_match(context, source_path, "model/stl", &observed, gcv_target_request_traits(context));
    bu_mime_traits_free(&observed);
    if ((profile.kind == MIME_STL_ASCII && read_options.binary) || profile.bom) valid = 0;
    if (!valid || fseek(state.fd_in, 0, SEEK_SET)) {
	fclose(state.fd_in);
	bu_log("STL input does not establish the selected layout and MIME traits\n");
	return 0;
    }
    read_options.binary = profile.kind == MIME_STL_BINARY;

    mk_id_units(state.fd_out, "Conversion from Stereolithography format", "mm");

    BU_LIST_INIT(&state.all_head.l);

    /* create a tree structure to hold the input vertices */
    state.tree = bg_vert_tree_create();

    stl_read_input(&state);

    int status = !state.failed && !ferror(state.fd_in) && !BU_LIST_IS_EMPTY(&state.all_head.l);
    if (status && mk_lcomb(wdbp, "all", &state.all_head, 0, NULL, NULL, NULL, 0))
	status = 0;
    if (fclose(state.fd_in)) status = 0;
    mk_freemembers(&state.all_head.l);
    bg_vert_tree_destroy(state.tree);
    bu_free(state.bot_faces, "STL faces");
    return status;
}

static const struct gcv_filter gcv_conv_stl_read = {
    "STL Reader", GCV_FILTER_READ, BU_MIME_MODEL_STL, NULL,
    stl_read_create_opts, stl_read_free_opts, stl_read
};

static const struct gcv_conversion gcv_conv_stl_read_execution = {
    "STL Reader", GCV_FILTER_READ, stl_read_create_opts, stl_read_free_opts, stl_read
};

extern const struct gcv_filter gcv_conv_stl_write;
extern const struct gcv_conversion gcv_conv_stl_write_execution;
static const struct gcv_filter *const filters[] = {&gcv_conv_stl_read, &gcv_conv_stl_write, NULL};
const struct gcv_plugin gcv_plugin_info_s = {filters};

COMPILER_DLLEXPORT const struct gcv_plugin *
gcv_plugin_info(void)
{
    return &gcv_plugin_info_s;
}

COMPILER_DLLEXPORT const struct gcv_plugin_v2 *
gcv_plugin_info_v2(void)
{
    static const struct bu_mime_trait ascii[] = {BU_MIME_TRAIT_STRING("encoding", "ascii"),
	BU_MIME_TRAIT_STRING("stl.serialization", "text"), BU_MIME_TRAIT_STRING("stl.utf8-bom", "false")};
    static const struct bu_mime_trait binary[] = {
	BU_MIME_TRAIT_STRING("encoding", "binary"),
	BU_MIME_TRAIT_STRING("byte-order", "little-endian"),
	BU_MIME_TRAIT_STRING("stl.serialization", "binary"),
    };
    static const struct bu_mime_trait text[] = {BU_MIME_TRAIT_STRING("stl.serialization", "text"), BU_MIME_TRAIT_STRING("stl.utf8-bom", "false")};
    static const struct bu_mime_capability types[] = {
	{"model/stl", text, sizeof(text) / sizeof(text[0]), 0, BU_MIME_DEFER_INPUT_VALIDATION},
	{"model/stl", binary, sizeof(binary) / sizeof(binary[0]), 0, BU_MIME_DEFER_INPUT_VALIDATION}};
    static const struct bu_mime_capability outputs[] = {
	BU_MIME_CAPABILITY("model/stl", text, sizeof(text) / sizeof(text[0]), 0),
	BU_MIME_CAPABILITY("model/stl", ascii, sizeof(ascii) / sizeof(ascii[0]), 0),
	BU_MIME_CAPABILITY("model/stl", binary, sizeof(binary) / sizeof(binary[0]), 0)};
    static const struct gcv_handler handlers[] = {
	{&gcv_conv_stl_read_execution, types, sizeof(types) / sizeof(types[0]), BU_MIME_DEFAULT_PRIORITY, NULL, NULL},
	{&gcv_conv_stl_write_execution, outputs, sizeof(outputs) / sizeof(outputs[0]), BU_MIME_DEFAULT_PRIORITY, NULL, NULL}};
    static const struct gcv_plugin_v2 plugin = {
	GCV_PLUGIN_ABI_VERSION, sizeof(struct gcv_plugin_v2), NULL, 0, handlers, 2};
    return &plugin;
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
