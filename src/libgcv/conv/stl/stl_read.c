/*                         S T L _ R E A D . C
 * BRL-CAD
 *
 * Copyright (c) 2002-2014 United States Government as represented by
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
 * Convert Stereolithography format files to BRL-CAD .g binary format
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
#include "bnetwork.h"
#include "bio.h"

#include "bu/cv.h"
#include "bu/getopt.h"
#include "bu/units.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "../../plugin.h"

static struct vert_root *tree_root;
static struct wmember all_head;
static const char *input_file;	/* name of the input file */
static char *forced_name = NULL;	/* name specified on command line */
static int solid_count = 0;	/* count of solids converted */
static struct bn_tol tol;	/* Tolerance structure */
static int id_no = 1000;		/* Ident numbers */
static int const_id =
    -1;		/* Constant ident number (assigned to all regions if non-negative) */
static int mat_code = 1;		/* default material code */
static int debug = 0;		/* Debug flag */
static int binary = 0;		/* flag indicating input is binary format */
static FILE *fd_in;		/* input file */
static struct rt_wdb *fd_out;	/* Resulting BRL-CAD file */
static float conv_factor = 1.0;	/* conversion factor from model units to mm */
static unsigned int obj_count =
    0; /* Count of parts converted for "stl-g" conversions */
static int *bot_faces =
    NULL;	 /* array of ints (indices into tree_root->the_array array) three per face */
static int bot_fsize = 0;		/* current size of the bot_faces array */
static int bot_fcurr = 0;		/* current bot face */

/* Size of blocks of faces to malloc */
#define BOT_FBLOCK 128

#define MAX_LINE_SIZE 512


void
Add_face(int face[3])
{
    if (!bot_faces) {
	bot_faces = (int *)bu_malloc(3 * BOT_FBLOCK * sizeof(int), "bot_faces");
	bot_fsize = BOT_FBLOCK;
	bot_fcurr = 0;
    } else if (bot_fcurr >= bot_fsize) {
	bot_fsize += BOT_FBLOCK;
	bot_faces = (int *)bu_realloc((void *)bot_faces, 3 * bot_fsize * sizeof(int),
				      "bot_faces increase");
    }

    VMOVE(&bot_faces[3 * bot_fcurr], face);
    bot_fcurr++;
}

void
mk_unique_brlcad_name(struct bu_vls *name)
{
    char *c;
    int count = 0;
    int len;

    c = bu_vls_addr(name);

    while (*c != '\0') {
	if (*c == '/' || !isprint((int)*c)) {
	    *c = '_';
	}

	c++;
    }

    len = bu_vls_strlen(name);

    while (db_lookup(fd_out->dbip, bu_vls_addr(name),
		     LOOKUP_QUIET) != RT_DIR_NULL) {
	char suff[10];

	bu_vls_trunc(name, len);
	count++;
	sprintf(suff, "_%d", count);
	bu_vls_strcat(name, suff);
    }
}

static int
Convert_part_ascii(char line[MAX_LINE_SIZE])
{
    char line1[MAX_LINE_SIZE];
    struct bu_vls solid_name = BU_VLS_INIT_ZERO;
    struct bu_vls region_name = BU_VLS_INIT_ZERO;

    int start;
    int i;
    int face_count = 0;
    int degenerate_count = 0;
    float colr[3] = {0.5, 0.5, 0.5};
    unsigned char color[3] = { 128, 128, 128 };
    struct wmember head;
    vect_t normal = {0, 0, 0};
    int solid_in_region = 0;

    if (RT_G_DEBUG & DEBUG_MEM_FULL)
	bu_prmem("At start of Convert_part_ascii():\n");

    if (RT_G_DEBUG & DEBUG_MEM_FULL) {
	bu_log("Barrier check at start of Convert_part_ascii:\n");

	if (bu_mem_barriercheck())
	    bu_bomb("Barrier check failed!\n");
    }


    bot_fcurr = 0;
    BU_LIST_INIT(&head.l);

    start = (-1);

    /* skip leading blanks */
    while (isspace((int)line[++start]) && line[start] != '\0');

    if (bu_strncmp(&line[start], "solid", 5)
	&& bu_strncmp(&line[start], "SOLID", 5)) {
	bu_log("Convert_part_ascii: Called for non-part\n%s\n", line);
	return 1;
    }

    /* skip blanks before name */
    start += 4;

    while (isspace((int)line[++start]) && line[start] != '\0');

    if (forced_name) {
	bu_vls_strcpy(&region_name, forced_name);
    } else if (line[start] != '\0') {
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

	obj_count++;

	/* copy the file name into our work space (skip over path) */
	ptr = strrchr(input_file, '/');

	if (ptr) {
	    ptr++;
	    bu_vls_strcpy(&region_name, ptr);
	} else {
	    bu_vls_strcpy(&region_name, input_file);
	}

	/* eliminate a trailing ".stl" */
	ptr = strstr(bu_vls_addr(&region_name), ".stl");

	if ((base_len = (ptr - bu_vls_addr(&region_name))) > 0) {
	    bu_vls_trunc(&region_name, base_len);
	}
    }

    mk_unique_brlcad_name(&region_name);
    bu_log("Converting Part: %s\n", bu_vls_addr(&region_name));

    solid_count++;
    bu_vls_strcpy(&solid_name, "s.");
    bu_vls_vlscat(&solid_name, &region_name);
    mk_unique_brlcad_name(&solid_name);

    bu_log("\tUsing solid name: %s\n", bu_vls_addr(&solid_name));

    if (RT_G_DEBUG & DEBUG_MEM || RT_G_DEBUG & DEBUG_MEM_FULL)
	bu_prmem("At start of Convert_part_ascii()");

    while (bu_fgets(line1, MAX_LINE_SIZE, fd_in) != NULL) {
	start = (-1);

	while (isspace((int)line1[++start]));

	if (!bu_strncmp(&line1[start], "endsolid", 8)
	    || !bu_strncmp(&line1[start], "ENDSOLID", 8)) {
	    break;
	} else if (!bu_strncmp(&line1[start], "color", 5)
		   || !bu_strncmp(&line1[start], "COLOR", 5)) {
	    sscanf(&line1[start + 5], "%f%f%f", &colr[0], &colr[1], &colr[2]);

	    for (i = 0; i < 3; i++)
		color[i] = (int)(colr[i] * 255.0);
	} else if (!bu_strncmp(&line1[start], "normal", 6)
		   || !bu_strncmp(&line1[start], "NORMAL", 6)) {
	    float x, y, z;

	    start += 6;
	    sscanf(&line1[start], "%f%f%f", &x, &y, &z);
	    VSET(normal, x, y, z);
	} else if (!bu_strncmp(&line1[start], "facet", 5)
		   || !bu_strncmp(&line1[start], "FACET", 5)) {
	    VSET(normal, 0.0, 0.0, 0.0);

	    start += 4;

	    while (line1[++start] && isspace((int)line1[start]));

	    if (line1[start]) {
		if (!bu_strncmp(&line1[start], "normal", 6)
		    || !bu_strncmp(&line1[start], "NORMAL", 6)) {
		    float x, y, z;

		    start += 6;
		    sscanf(&line1[start], "%f%f%f", &x, &y, &z);
		    VSET(normal, x, y, z);
		}
	    }
	} else if (!bu_strncmp(&line1[start], "outer loop", 10)
		   || !bu_strncmp(&line1[start], "OUTER LOOP", 10)) {
	    int endloop = 0;
	    int vert_no = 0;
	    int tmp_face[3] = {0, 0, 0};

	    while (!endloop) {
		if (bu_fgets(line1, MAX_LINE_SIZE, fd_in) == NULL) {
		    bu_log("Unexpected EOF while reading a loop in a part!\n");
		    bu_vls_free(&region_name);
		    bu_vls_free(&solid_name);
		    return 0;
		}

		start = (-1);

		while (isspace((int)line1[++start]));

		if (!bu_strncmp(&line1[start], "endloop", 7)
		    || !bu_strncmp(&line1[start], "ENDLOOP", 7))
		    endloop = 1;
		else if (!bu_strncmp(&line1[start], "vertex", 6)
			 || !bu_strncmp(&line1[start], "VERTEX", 6)) {
		    double x, y, z;

		    sscanf(&line1[start + 6], "%lf%lf%lf", &x, &y, &z);

		    if (vert_no > 2) {
			int n;

			bu_log("Non-triangular loop:\n");

			for (n = 0; n < 3; n++)
			    bu_log("\t(%g %g %g)\n", V3ARGS(&tree_root->the_array[tmp_face[n]]));

			bu_log("\t(%g %g %g)\n", x, y, z);
		    }

		    x *= conv_factor;
		    y *= conv_factor;
		    z *= conv_factor;
		    tmp_face[vert_no++] = Add_vert(x, y, z, tree_root, tol.dist_sq);
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

	    if (debug) {
		int n;

		bu_log("Making Face:\n");

		for (n = 0; n < 3; n++)
		    bu_log("\tvertex #%d: (%g %g %g)\n", tmp_face[n],
			   V3ARGS(&tree_root->the_array[3 * tmp_face[n]]));

		VPRINT(" normal", normal);
	    }

	    Add_face(tmp_face);
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

	return 1;
    } else {
	if (degenerate_count)
	    bu_log("\t%d faces were degenerate\n", degenerate_count);
    }

    mk_bot(fd_out, bu_vls_addr(&solid_name), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0,
	   tree_root->curr_vert, bot_fcurr,
	   tree_root->the_array, bot_faces, NULL, NULL);
    clean_vert_tree(tree_root);

    if (face_count && !solid_in_region) {
	(void)mk_addmember(bu_vls_addr(&solid_name), &head.l, NULL, WMOP_UNION);
    }

    bu_log("\tMaking region (%s)\n", bu_vls_addr(&region_name));

    if (const_id >= 0) {
	mk_lrcomb(fd_out, bu_vls_addr(&region_name), &head, 1, (char *)NULL,
		  (char *)NULL, color, const_id, 0, mat_code, 100, 0);

	if (face_count) {
	    (void)mk_addmember(bu_vls_addr(&region_name), &all_head.l, NULL, WMOP_UNION);
	}
    } else {
	mk_lrcomb(fd_out, bu_vls_addr(&region_name), &head, 1, (char *)NULL,
		  (char *)NULL, color, id_no, 0, mat_code, 100, 0);

	if (face_count)
	    (void)mk_addmember(bu_vls_addr(&region_name), &all_head.l, NULL, WMOP_UNION);

	id_no++;
    }

    if (RT_G_DEBUG & DEBUG_MEM_FULL) {
	bu_log("Barrier check at end of Convert_part_ascii:\n");

	if (bu_mem_barriercheck())
	    bu_bomb("Barrier check failed!\n");
    }

    bu_vls_free(&region_name);
    bu_vls_free(&solid_name);

    return 1;
}

/* Byte swaps a 4-byte val */
void lswap(unsigned int *v)
{
    unsigned int r;

    r = *v;
    *v = ((r & 0xff) << 24) | ((r & 0xff00) << 8) | ((r & 0xff0000) >> 8)
	 | ((r & 0xff000000) >> 24);
}

static void
Convert_part_binary()
{
    unsigned char buf[51];
    unsigned long num_facets = 0;
    float flts[12];
    vect_t normal;
    int tmp_face[3];
    struct wmember head;
    struct bu_vls solid_name = BU_VLS_INIT_ZERO;
    struct bu_vls region_name = BU_VLS_INIT_ZERO;
    int face_count = 0;
    int degenerate_count = 0;
    size_t ret;

    solid_count++;

    if (forced_name) {
	bu_vls_strcpy(&solid_name, "s.");
	bu_vls_strcat(&solid_name, forced_name);
	bu_vls_strcpy(&region_name, forced_name);
    } else {
	bu_vls_strcat(&solid_name, "s.stl");
	bu_vls_strcat(&region_name, "r.stl");
    }

    bu_log("\tUsing solid name: %s\n", bu_vls_addr(&solid_name));


    ret = fread(buf, 4, 1, fd_in);

    if (ret != 1)
	perror("fread");

    /* swap bytes to convert from Little-endian to network order (big-endian) */
    lswap((unsigned int *)buf);

    /* now use our network to native host format conversion tools */
    num_facets = ntohl(*(uint32_t *)buf);

    bu_log("\t%ld facets\n", num_facets);

    while (fread(buf, 48, 1, fd_in)) {
	int i;
	double pt[3];

	/* swap bytes to convert from Little-endian to network order (big-endian) */
	for (i = 0; i < 12; i++) {
	    lswap((unsigned int *)&buf[i * 4]);
	}

	/* now use our network to native host format conversion tools */
	bu_cv_ntohf((unsigned char *)flts, buf, 12);

	/* unused attribute byte count */
	ret = fread(buf, 2, 1, fd_in);

	if (ret != 1)
	    perror("fread");

	VMOVE(normal, flts);
	VSCALE(pt, &flts[3], conv_factor);
	tmp_face[0] = Add_vert(V3ARGS(pt), tree_root, tol.dist_sq);
	VSCALE(pt, &flts[6], conv_factor);
	tmp_face[1] = Add_vert(V3ARGS(pt), tree_root, tol.dist_sq);
	VSCALE(pt, &flts[9], conv_factor);
	tmp_face[2] = Add_vert(V3ARGS(pt), tree_root, tol.dist_sq);

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

	if (debug) {
	    int n;

	    bu_log("Making Face:\n");

	    for (n = 0; n < 3; n++)
		bu_log("\tvertex #%d: (%g %g %g)\n", tmp_face[n],
		       V3ARGS(&tree_root->the_array[3 * tmp_face[n]]));

	    VPRINT(" normal", normal);
	}

	Add_face(tmp_face);
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

    mk_bot(fd_out, bu_vls_addr(&solid_name), RT_BOT_SOLID, RT_BOT_UNORIENTED, 0,
	   tree_root->curr_vert, bot_fcurr, tree_root->the_array, bot_faces, NULL, NULL);
    clean_vert_tree(tree_root);

    BU_LIST_INIT(&head.l);

    if (face_count) {
	(void)mk_addmember(bu_vls_addr(&solid_name), &head.l, NULL, WMOP_UNION);
    }

    bu_log("\tMaking region (%s)\n", bu_vls_addr(&region_name));

    if (const_id >= 0) {
	mk_lrcomb(fd_out, bu_vls_addr(&region_name), &head, 1, (char *)NULL,
		  (char *)NULL, NULL, const_id, 0, mat_code, 100, 0);

	if (face_count) {
	    (void)mk_addmember(bu_vls_addr(&region_name), &all_head.l, NULL, WMOP_UNION);
	}
    } else {
	mk_lrcomb(fd_out, bu_vls_addr(&region_name), &head, 1, (char *)NULL,
		  (char *)NULL, NULL, id_no, 0, mat_code, 100, 0);

	if (face_count)
	    (void)mk_addmember(bu_vls_addr(&region_name), &all_head.l, NULL, WMOP_UNION);

	id_no++;
    }

    if (RT_G_DEBUG & DEBUG_MEM_FULL) {
	bu_log("Barrier check at end of Convert_part_ascii:\n");

	if (bu_mem_barriercheck())
	    bu_bomb("Barrier check failed!\n");
    }

    return;
}


static int
Convert_input()
{
    char line[ MAX_LINE_SIZE ];

    if (binary) {
	if (fread(line, 80, 1, fd_in) < 1) {
	    if (feof(fd_in)) {
		bu_log("Unexpected EOF in input file!\n");
		return 0;
	    } else {
		bu_log("Error reading input file\n");
		perror("stl-g");
		return 0;
	    }
	}

	line[80] = '\0';
	bu_log("header data:\n%s\n\n", line);
	Convert_part_binary();
    } else {
	while (bu_fgets(line, MAX_LINE_SIZE, fd_in) != NULL) {
	    int start = 0;

	    while (line[start] != '\0' && isspace((int)line[start])) {
		start++;
	    }

	    if (!bu_strncmp(&line[start], "solid", 5)
		|| !bu_strncmp(&line[start], "SOLID", 5))
		return Convert_part_ascii(line);
	    else
		bu_log("Unrecognized line:\n%s\n", line);
	}
    }

    return 1;
}


HIDDEN int
gcv_stl_read(const char *path, struct rt_wdb *wdbp,
	     const struct gcv_opts *UNUSED(options))
{
    tol.magic = BN_TOL_MAGIC;

    /* this value selected as a reasonable compromise between eliminating
     * needed faces and keeping degenerate faces
     */
    tol.dist = 0.0005;	/* default, same as MGED, RT, ... */
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    forced_name = NULL;

    conv_factor = 1.0;	/* default */

    input_file = path;

    if ((fd_in = fopen(input_file, "rb")) == NULL) {
	bu_log("Cannot open input file (%s)\n", input_file);
	perror("stl-g");
	return 0;
    }

    fd_out = wdbp;

    /* FIXME: update for libgcv
     * mk_id_units(fd_out, "Conversion from Stereolithography format", "mm");
     */

    BU_LIST_INIT(&all_head.l);

    /* create a tree structure to hold the input vertices */
    tree_root = create_vert_tree();

    if (!Convert_input()) {
	fclose(fd_in);
	return 0;
    }

    /* make a top level group */
    mk_lcomb(fd_out, "all", &all_head, 0, (char *)NULL, (char *)NULL,
	     (unsigned char *)NULL, 0);

    fclose(fd_in);


    return 1;
}


static const struct gcv_converter converters[] = {
    {"stl", gcv_stl_read, NULL},
    {NULL, NULL, NULL}
};

const struct gcv_plugin_info gcv_plugin_conv_stl_read = {converters};


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
