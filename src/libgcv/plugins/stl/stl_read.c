/*                      S T L _ R E A D . C
 * BRL-CAD
 *
 * Copyright (c) 2002-2021 United States Government as represented by
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
#include "bu/units.h"
#include "gcv/api.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"


struct stl_read_options
{
    int binary;	        	/* flag indicating input is binary format */
    int starting_id;
    int const_id;		/* Constant ident number (assigned to all regions if non-negative) */
    int mat_code;		/* default material code */
};


struct conversion_state
{
    const struct gcv_opts *gcv_options;
    const struct stl_read_options *stl_read_options;

    const char *input_file;	/* name of the input file */
    FILE *fd_in;		/* input file */
    struct rt_wdb *fd_out;	/* Resulting BRL-CAD file */

    struct wmember all_head;
    struct bn_vert_tree *tree;
    int *bot_faces;	        /* array of ints (indices into tree->the_array array) three per face */

    int id_no;	            	/* Ident numbers */
    int bot_fsize;		/* current size of the bot_faces array */
    int bot_fcurr;		/* current bot face */
};


/* Size of blocks of faces to malloc */
#define BOT_FBLOCK 128

#define MAX_LINE_SIZE 512


HIDDEN void
Add_face(struct conversion_state *pstate, int face[3])
{
    if (!pstate->bot_faces) {
	pstate->bot_faces = (int *)bu_malloc(3 * BOT_FBLOCK * sizeof(int), "bot_faces");
	pstate->bot_fsize = BOT_FBLOCK;
	pstate->bot_fcurr = 0;
    } else if (pstate->bot_fcurr >= pstate->bot_fsize) {
	pstate->bot_fsize += BOT_FBLOCK;
	pstate->bot_faces = (int *)bu_realloc((void *)pstate->bot_faces, 3 * pstate->bot_fsize * sizeof(int), "bot_faces increase");
    }

    VMOVE(&pstate->bot_faces[3*pstate->bot_fcurr], face);
    pstate->bot_fcurr++;
}

HIDDEN void
mk_unique_brlcad_name(struct conversion_state *pstate, struct bu_vls *name)
{
    char *c;
    int count=0;
    size_t len;

    c = bu_vls_addr(name);

    while (*c != '\0') {
	if (*c == '/' || !isprint((int)*c)) {
	    *c = '_';
	}
	c++;
    }

    len = bu_vls_strlen(name);
    while (db_lookup(pstate->fd_out->dbip, bu_vls_addr(name), LOOKUP_QUIET) != RT_DIR_NULL) {
	char suff[10];

	bu_vls_trunc(name, len);
	count++;
	sprintf(suff, "_%d", count);
	bu_vls_strcat(name, suff);
    }
}

HIDDEN void
Convert_part_ascii(struct conversion_state *pstate, char line[MAX_LINE_SIZE])
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
    struct wmember head;
    vect_t normal={0, 0, 0};
    int solid_in_region=0;

    pstate->bot_fcurr = 0;
    BU_LIST_INIT(&head.l);

    start = (-1);
    /* skip leading blanks */
    while (isspace((int)line[++start]) && line[start] != '\0');
    if (bu_strncmp(&line[start], "solid", 5) && bu_strncmp(&line[start], "SOLID", 5)) {
	bu_log("Convert_part_ascii: Called for non-part\n%s\n", line);
	return;
    }

    /* skip blanks before name */
    start += 4;
    while (isspace((int)line[++start]) && line[start] != '\0');

    if (line[start] != '\0') {
	char *ptr;

	/* get name */
	bu_vls_strcpy(&region_name, &line[start]);
	bu_vls_trimspace(&region_name);
	ptr = bu_vls_addr(&region_name);
	while (*ptr != '\0') {
	    if (isspace((int)*ptr)) {
		bu_vls_trunc(&region_name, ptr - bu_vls_addr(&region_name));
		break;
	    }
	    ptr++;
	}
    } else {
	/* build a name from the file name */

	char *ptr;
	int base_len;

	/* copy the file name into our work space (skip over path) */
	ptr = strrchr(pstate->input_file, '/');
	if (ptr) {
	    ptr++;
	    bu_vls_strcpy(&region_name, ptr);
	} else {
	    bu_vls_strcpy(&region_name, pstate->input_file);
	}

	/* eliminate a trailing ".stl" */
	ptr = strstr(bu_vls_addr(&region_name), ".stl");
	if ((base_len=(ptr - bu_vls_addr(&region_name))) > 0) {
	    bu_vls_trunc(&region_name, base_len);
	}
    }

    mk_unique_brlcad_name(pstate, &region_name);

    if (pstate->gcv_options->verbosity_level)
	bu_log("Converting Part: %s\n", bu_vls_addr(&region_name));

    bu_vls_strcpy(&solid_name, "s.");
    bu_vls_vlscat(&solid_name, &region_name);
    mk_unique_brlcad_name(pstate, &solid_name);

    if (pstate->gcv_options->verbosity_level)
	bu_log("\tUsing solid name: %s\n", bu_vls_addr(&solid_name));

    while (bu_fgets(line1, MAX_LINE_SIZE, pstate->fd_in) != NULL) {
	start = (-1);
	while (isspace((int)line1[++start]));
	if (!bu_strncmp(&line1[start], "endsolid", 8) || !bu_strncmp(&line1[start], "ENDSOLID", 8)) {
	    break;
	} else if (!bu_strncmp(&line1[start], "color", 5) || !bu_strncmp(&line1[start], "COLOR", 5)) {
	    sscanf(&line1[start+5], "%f%f%f", &colr[0], &colr[1], &colr[2]);
	    for (i=0; i<3; i++)
		color[i] = (int)(colr[i] * 255.0);
	} else if (!bu_strncmp(&line1[start], "normal", 6) || !bu_strncmp(&line1[start], "NORMAL", 6)) {
	    float x, y, z;

	    start += 6;
	    sscanf(&line1[start], "%f%f%f", &x, &y, &z);
	    VSET(normal, x, y, z);
	} else if (!bu_strncmp(&line1[start], "facet", 5) || !bu_strncmp(&line1[start], "FACET", 5)) {
	    VSET(normal, 0.0, 0.0, 0.0);

	    start += 4;
	    while (line1[++start] && isspace((int)line1[start]));

	    if (line1[start]) {
		if (!bu_strncmp(&line1[start], "normal", 6) || !bu_strncmp(&line1[start], "NORMAL", 6)) {
		    float x, y, z;

		    start += 6;
		    sscanf(&line1[start], "%f%f%f", &x, &y, &z);
		    VSET(normal, x, y, z);
		}
	    }
	} else if (!bu_strncmp(&line1[start], "outer loop", 10) || !bu_strncmp(&line1[start], "OUTER LOOP", 10)) {
	    int endloop=0;
	    int vert_no=0;
	    int tmp_face[3] = {0, 0, 0};

	    while (!endloop) {
		if (bu_fgets(line1, MAX_LINE_SIZE, pstate->fd_in) == NULL)
		    bu_exit(EXIT_FAILURE, "Unexpected EOF while reading a loop in a part!\n");

		start = (-1);
		while (isspace((int)line1[++start]));

		if (!bu_strncmp(&line1[start], "endloop", 7) || !bu_strncmp(&line1[start], "ENDLOOP", 7))
		    endloop = 1;
		else if (!bu_strncmp(&line1[start], "vertex", 6) || !bu_strncmp(&line1[start], "VERTEX", 6)) {
		    double x, y, z;

		    sscanf(&line1[start+6], "%lf%lf%lf", &x, &y, &z);

		    if (vert_no > 2) {
			int n;

			bu_log("Non-triangular loop:\n");
			for (n=0; n<3; n++)
			    bu_log("\t(%g %g %g)\n", V3ARGS(&pstate->tree->the_array[tmp_face[n]]));

			bu_log("\t(%g %g %g)\n", x, y, z);
		    }
		    x *= pstate->gcv_options->scale_factor;
		    y *= pstate->gcv_options->scale_factor;
		    z *= pstate->gcv_options->scale_factor;
		    tmp_face[vert_no++] = bn_vert_tree_add( pstate->tree,x, y, z, pstate->gcv_options->calculational_tolerance.dist_sq);
		} else {
		    bu_log("Unrecognized line: %s\n", line1);
		}
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

	    Add_face(pstate, tmp_face);
	    face_count++;
	}
    }

    /* Check if this part has any solid parts */
    if (face_count == 0) {
	bu_log("\t%s has no solid parts, ignoring\n", bu_vls_addr(&region_name));
	if (degenerate_count)
	    bu_log("\t%d faces were degenerate\n", degenerate_count);
	bu_vls_free(&region_name);
	bu_vls_free(&solid_name);

	return;
    } else {
	if (degenerate_count)
	    bu_log("\t%d faces were degenerate\n", degenerate_count);
    }

    mk_bot(pstate->fd_out, bu_vls_addr(&solid_name), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, pstate->tree->curr_vert, pstate->bot_fcurr,
	   pstate->tree->the_array, pstate->bot_faces, NULL, NULL);
    bn_vert_tree_clean(pstate->tree);

    if (face_count && !solid_in_region) {
	(void)mk_addmember(bu_vls_addr(&solid_name), &head.l, NULL, WMOP_UNION);
    }

    if (pstate->gcv_options->verbosity_level)
	bu_log("\tMaking region (%s)\n", bu_vls_addr(&region_name));

    if (pstate->stl_read_options->const_id) {
	mk_lrcomb(pstate->fd_out, bu_vls_addr(&region_name), &head, 1, (char *)NULL,
		  (char *)NULL, color, pstate->id_no, 0, pstate->stl_read_options->mat_code, 100, 0);
	if (face_count) {
	    (void)mk_addmember(bu_vls_addr(&region_name), &pstate->all_head.l, NULL, WMOP_UNION);
	}
    } else {
	mk_lrcomb(pstate->fd_out, bu_vls_addr(&region_name), &head, 1, (char *)NULL, (char *)NULL, color, pstate->id_no, 0, pstate->stl_read_options->mat_code, 100, 0);
	if (face_count)
	    (void)mk_addmember(bu_vls_addr(&region_name), &pstate->all_head.l, NULL, WMOP_UNION);
	pstate->id_no++;
    }

    bu_vls_free(&region_name);
    bu_vls_free(&solid_name);

    return;
}

/* Byte swaps a 4-byte val */
HIDDEN void stl_read_lswap(unsigned int *v)
{
    unsigned int r;

    r = *v;
    *v = ((r & 0xff) << 24) | ((r & 0xff00) << 8) | ((r & 0xff0000) >> 8)
	| ((r & 0xff000000) >> 24);
}

HIDDEN void
Convert_part_binary(struct conversion_state *pstate)
{
    unsigned char buf[51];
    unsigned long num_facets=0;
    float flts[12];
    vect_t normal;
    int tmp_face[3];
    struct wmember head;
    struct bu_vls solid_name = BU_VLS_INIT_ZERO;
    struct bu_vls region_name = BU_VLS_INIT_ZERO;
    int face_count=0;
    int degenerate_count=0;
    size_t ret;

    bu_vls_strcat(&solid_name, "s.stl");
    bu_vls_strcat(&region_name, "r.stl");
    bu_log("\tUsing solid name: %s\n", bu_vls_addr(&solid_name));


    ret = fread(buf, 4, 1, pstate->fd_in);
    if (ret != 1)
	perror("fread");

    /* swap bytes to convert from Little-endian to network order (big-endian) */
    stl_read_lswap((unsigned int *)buf);

    /* now use our network to native host format conversion tools */
    num_facets = ntohl(*(uint32_t *)buf);

    bu_log("\t%ld facets\n", num_facets);
    while (fread(buf, 48, 1, pstate->fd_in)) {
	int i;
	double pt[3];

	/* swap bytes to convert from Little-endian to network order (big-endian) */
	for (i=0; i<12; i++) {
	    stl_read_lswap((unsigned int *)&buf[i*4]);
	}

	/* now use our network to native host format conversion tools */
	bu_cv_ntohf((unsigned char *)flts, buf, 12);

	/* unused attribute byte count */
	ret = fread(buf, 2, 1, pstate->fd_in);
	if (ret != 1)
	    perror("fread");

	VMOVE(normal, flts);
	VSCALE(pt, &flts[3], pstate->gcv_options->scale_factor);
	tmp_face[0] = bn_vert_tree_add(pstate->tree, V3ARGS(pt), pstate->gcv_options->calculational_tolerance.dist_sq);
	VSCALE(pt, &flts[6], pstate->gcv_options->scale_factor);
	tmp_face[1] = bn_vert_tree_add(pstate->tree, V3ARGS(pt), pstate->gcv_options->calculational_tolerance.dist_sq);
	VSCALE(pt, &flts[9], pstate->gcv_options->scale_factor);
	tmp_face[2] = bn_vert_tree_add(pstate->tree, V3ARGS(pt), pstate->gcv_options->calculational_tolerance.dist_sq);

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

	Add_face(pstate, tmp_face);
	face_count++;
    }

    /* Check if this part has any solid parts */
    if (face_count == 0) {
	bu_log("\tpart has no solid parts, ignoring\n");
	if (degenerate_count)
	    bu_log("\t%d faces were degenerate\n", degenerate_count);
	return;
    } else {
	if (degenerate_count)
	    bu_log("\t%d faces were degenerate\n", degenerate_count);
    }

    mk_bot(pstate->fd_out, bu_vls_addr(&solid_name), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0,
	   pstate->tree->curr_vert, pstate->bot_fcurr, pstate->tree->the_array, pstate->bot_faces, NULL, NULL);
    bn_vert_tree_clean(pstate->tree);

    BU_LIST_INIT(&head.l);
    if (face_count) {
	(void)mk_addmember(bu_vls_addr(&solid_name), &head.l, NULL, WMOP_UNION);
    }

    if (pstate->gcv_options->verbosity_level)
	bu_log("\tMaking region (%s)\n", bu_vls_addr(&region_name));

    if (pstate->stl_read_options->const_id) {
	mk_lrcomb(pstate->fd_out, bu_vls_addr(&region_name), &head, 1, (char *)NULL,
		  (char *)NULL, NULL, pstate->id_no, 0, pstate->stl_read_options->mat_code, 100, 0);
	if (face_count) {
	    (void)mk_addmember(bu_vls_addr(&region_name), &pstate->all_head.l, NULL, WMOP_UNION);
	}
    } else {
	mk_lrcomb(pstate->fd_out, bu_vls_addr(&region_name), &head, 1, (char *)NULL, (char *)NULL, NULL, pstate->id_no, 0, pstate->stl_read_options->mat_code, 100, 0);
	if (face_count)
	    (void)mk_addmember(bu_vls_addr(&region_name), &pstate->all_head.l, NULL, WMOP_UNION);
	pstate->id_no++;
    }

    return;
}


HIDDEN void
Convert_input(struct conversion_state *pstate)
{
    char line[ MAX_LINE_SIZE ];

    if (pstate->stl_read_options->binary) {
	if (fread(line, 80, 1, pstate->fd_in) < 1) {
	    if (feof(pstate->fd_in)) {
		bu_exit(EXIT_FAILURE, "Unexpected EOF in input file!\n");
	    } else {
		bu_log("Error reading input file\n");
		perror("stl-g");
		bu_exit(EXIT_FAILURE, "Error reading input file\n");
	    }
	}
	line[80] = '\0';
	bu_log("header data:\n%s\n\n", line);
	Convert_part_binary(pstate);
    } else {
	while (bu_fgets(line, MAX_LINE_SIZE, pstate->fd_in) != NULL) {
	    int start = 0;
	    while (line[start] != '\0' && isspace((int)line[start])) {
		start++;
	    }
	    if (!bu_strncmp(&line[start], "solid", 5) || !bu_strncmp(&line[start], "SOLID", 5))
		Convert_part_ascii(pstate, line);
	    else
		bu_log("Unrecognized line:\n%s\n", line);
	}
    }
}


HIDDEN void
stl_read_create_opts(struct bu_opt_desc **options_desc, void **dest_options_data)
{
    struct stl_read_options *options_data;

    BU_ALLOC(options_data, struct stl_read_options);
    *dest_options_data = options_data;
    *options_desc = (struct bu_opt_desc *)bu_malloc(5 * sizeof(struct bu_opt_desc), "options_desc");

    options_data->binary = 0;
    options_data->starting_id = 1000;
    options_data->const_id = 0;
    options_data->mat_code = 1;

    BU_OPT((*options_desc)[0], NULL, "binary", NULL,
	    NULL, &options_data->binary,
	    "specify that the input is in binary STL format");

    BU_OPT((*options_desc)[1], NULL, "starting-ident", "number",
	    bu_opt_int, options_data,
	    "specify the starting ident for the regions created");

    BU_OPT((*options_desc)[2], NULL, "constant-ident", NULL,
	    NULL, &options_data->const_id,
	    "specify that the starting ident should remain constant");

    BU_OPT((*options_desc)[3], NULL, "material", "code",
	    bu_opt_int, &options_data->mat_code,
	    "specify the material code that will be assigned to created regions");

    BU_OPT_NULL((*options_desc)[4]);
}


HIDDEN void
stl_read_free_opts(void *options_data)
{
    bu_free(options_data, "options_data");
}


HIDDEN int
stl_read(struct gcv_context *context, const struct gcv_opts *gcv_options, const void *options_data, const char *source_path)
{
    struct conversion_state state;

    memset(&state, 0, sizeof(state));

    state.gcv_options = gcv_options;
    state.stl_read_options = (struct stl_read_options *)options_data;
    state.id_no = state.stl_read_options->starting_id;
    state.input_file = source_path;
    state.fd_out = context->dbip->dbi_wdbp;

    if ((state.fd_in = fopen(source_path, "rb")) == NULL) {
	bu_log("Cannot open input file (%s)\n", source_path);
	perror("libgcv");
	bu_exit(1, NULL);
    }

    mk_id_units(state.fd_out, "Conversion from Stereolithography format", "mm");

    BU_LIST_INIT(&state.all_head.l);

    /* create a tree structure to hold the input vertices */
    state.tree = bn_vert_tree_create();

    Convert_input(&state);

    /* make a top level group */
    mk_lcomb(context->dbip->dbi_wdbp, "all", &state.all_head, 0, (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0);

    fclose(state.fd_in);

    return 1;
}

/* Try to guide the flow of this logic according to this discussion:
 *
 * https://stackoverflow.com/questions/26171521/verifying-that-an-stl-file-is-ascii-or-binary
 *
 * The STL format is so simple this is actually a bit tricky to do reliably...
 */
HIDDEN int
stl_can_read(const char *data)
{
    struct stat stat_buf;
    char ascii_header[6];
    FILE *fp;
    size_t fsize = 3*sizeof(float) + 3*3*sizeof(float) + sizeof(uint16_t);
    if(!data) return 0;
    if (stat(data, &stat_buf)) return 0;

    /* If it's too small, don't bother */
    if (stat_buf.st_size < 15) return 0;

    /* First, try an ASCII file */
    fp = fopen(data, "r");
    if (!fp) return 0;
    if (bu_fgets(ascii_header, 5, fp) != NULL && BU_STR_EQUAL(ascii_header, "solid")) {
	/* We've got solid at the beginning, so look for "endsolid" later in the file.
	 * If we find it, this is probably an ASCII file. */
	int found_endsolid = 0;
	struct bu_vls ves = BU_VLS_INIT_ZERO;
	struct bu_vls nline = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&ves, "endsolid");
	while (bu_vls_gets(&nline, fp) && !found_endsolid) {
	    bu_vls_trimspace(&nline);
	    if (bu_vls_strncmp(&nline, &ves, bu_vls_strlen(&ves)) == 0) found_endsolid = 1;
	}
	bu_vls_free(&ves);
	bu_vls_free(&nline);
	if (found_endsolid) {
	    fclose(fp);
	    return 1;
	}
    }
    fclose(fp);

    /* If that didn't work, see if it looks like a binary */

    /* If it's too small, don't bother */
    if (stat_buf.st_size < 84) return 0;

    /* Open as a binary file this time */
    fp = fopen(data, "rb");
    if (!fp) return 0;
    {
	uint32_t tri_cnt;
	unsigned char buffer[1000];
	if (!fread(buffer, 80, 1, fp)) return 0;
	/* Note: we're assuming little-endian... */
	if (!fread(((void *)&tri_cnt), 4, 1, fp)) return 0;
	if ((size_t)stat_buf.st_size == (84 + (tri_cnt * fsize))) {
	    fclose(fp);
	    return 1;
	}
    }
    fclose(fp);

    return 0;
}


static const struct gcv_filter gcv_conv_stl_read = {
    "STL Reader", GCV_FILTER_READ, (int)BU_MIME_MODEL_STL, BU_MIME_MODEL, stl_can_read,
    stl_read_create_opts, stl_read_free_opts, stl_read
};


extern const struct gcv_filter gcv_conv_stl_write;
static const struct gcv_filter * const filters[] = {&gcv_conv_stl_read, &gcv_conv_stl_write, NULL};

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
