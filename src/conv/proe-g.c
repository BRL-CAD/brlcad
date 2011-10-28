/*                        P R O E - G . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
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
/** @file conv/proe-g.c
 *
 * Code to convert ascii output from Pro/Engineer to BRL-CAD
 * The required output is from the Pro/Develop application proe-brl
 * that must be initiated from the "BRL-CAD" option of Pro/Engineer's
 * "EXPORT" menu.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>
#include <errno.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"


static struct wmember all_head;
static char *input_file;	/* name of the input file */
static char *brlcad_file;	/* name of output file */
static struct bu_vls ret_name;	/* unique name built by Build_unique_name() */
static char *forced_name=NULL;	/* name specified on command line */
static int stl_format=0;	/* Flag, non-zero indocates raw Stereolithography format input */
static int solid_count=0;	/* count of solids converted */
static struct bn_tol tol;	/* Tolerance structure */
static int id_no=1000;		/* Ident numbers */
static int const_id=-1;		/* Constant ident number (assigned to all regions if non-negative) */
static int mat_code=1;		/* default material code */
static int air_no=1;		/* Air numbers */
static int debug=0;		/* Debug flag */
static int cut_count=0;		/* count of assembly cut HAF solids created */
static int do_regex=0;		/* flag to indicate if 'u' option is in effect */
static regex_t reg_cmp;		/* compiled regular expression */
static FILE *fd_in;		/* input file (from Pro/E) */
static struct rt_wdb *fd_out;	/* Resulting BRL-CAD file */
static struct bu_ptbl null_parts; /* Table of NULL solids */
static float conv_factor=1.0;	/* conversion factor from model units to mm */
static int top_level=1;		/* flag to catch top level assembly or part */
static mat_t re_orient;		/* rotation matrix to put model in BRL-CAD orientation
				 * (+x towards front +z is up) */
static int do_air=0;		/* When set, all regions are BRL-CAD "air" regions */
static int do_reorient=1;	/* When set, reorient entire model to BRL-CAD style */
static unsigned int obj_count=0; /* Count of parts converted for "stl-g" conversions */
static int *bot_faces=NULL;	 /* array of ints (indices into vert_tree_root->the_array array) three per face */
static int bot_fsize=0;		/* current size of the bot_faces array */
static int bot_fcurr=0;		/* current bot face */
static struct vert_root *vert_tree_root;	/* binary search tree for vertices */

/* Size of blocks of faces to malloc */
#define BOT_FBLOCK 128

struct render_verts
{
    point_t pt;
    struct vertex *v;
};

struct name_conv_list
{
    char *brlcad_name;
    char *solid_name;
    char *name;
    unsigned int obj;
    int solid_use_no;
    int comb_use_no;
    struct name_conv_list *next;
} *name_root=(struct name_conv_list *)NULL;

struct ptc_plane
{
    double e1[3], e2[3], e3[3], origin[3];
};

struct ptc_cylinder
{
    double e1[3], e2[3], e3[3], origin[3];
    double radius;
};

union ptc_surf
{
    struct ptc_plane plane;
    struct ptc_cylinder cylinder;
};

struct ptc_surf_list
{
    struct bu_list l;
    int type;
    union ptc_surf surf;
} *surf_root=(struct ptc_surf_list *)NULL;

/* for type in struct ptc_plane and struct ptc_cylinder */
#define SURF_PLANE	1
#define SURF_CYLINDER	2

#define MAX_LINE_SIZE	512

#define UNKNOWN_TYPE	0
#define ASSEMBLY_TYPE	1
#define PART_TYPE	2
#define CUT_SOLID_TYPE	3

char *
Build_unique_name(char *name)
{
    struct name_conv_list *ptr;
    int name_len;
    int tries=0;

    name_len = strlen(name);
    bu_vls_strcpy(&ret_name, name);
    ptr = name_root;
    while (ptr) {
	if (BU_STR_EQUAL(bu_vls_addr(&ret_name), ptr->brlcad_name) ||
	    (ptr->solid_name && BU_STR_EQUAL(bu_vls_addr(&ret_name), ptr->solid_name))) {
	    /* this name already exists, build a new one */
	    ++tries;
	    bu_vls_trunc(&ret_name, name_len);
	    bu_vls_printf(&ret_name, "_%d", tries);

	    ptr = name_root;
	}

	ptr = ptr->next;
    }

    return bu_vls_addr(&ret_name);
}

static struct name_conv_list *
Add_new_name(char *name, unsigned int obj, int type)
{
    struct name_conv_list *ptr;

    if (debug)
	bu_log("Add_new_name(%s, x%x, %d)\n", name, obj, type);

    if (type != ASSEMBLY_TYPE && type != PART_TYPE && type != CUT_SOLID_TYPE) {
	bu_exit(EXIT_FAILURE, "Bad type for name (%s) in Add_new_name\n", name);
    }


    /* Add a new name */
    ptr = (struct name_conv_list *)bu_calloc(1, sizeof(struct name_conv_list), "Add_new_name: prev->next");
    ptr->next = (struct name_conv_list *)NULL;
    ptr->brlcad_name = bu_strdup(name);
    ptr->obj = obj;
    if (do_regex && type != CUT_SOLID_TYPE) {
	regmatch_t pmatch;

	if (regexec(&reg_cmp, ptr->brlcad_name, 1, &pmatch, 0 ) == 0) {
	    /* got a match */
	    bu_strlcpy(&ptr->brlcad_name[pmatch.rm_so], &ptr->brlcad_name[pmatch.rm_eo], MAX_LINE_SIZE);
	}
	if (debug)
	    bu_log("\tafter reg_ex, name is %s\n", ptr->brlcad_name);
    } else if (type == CUT_SOLID_TYPE) {
	bu_free((char *)ptr->brlcad_name, "brlcad_name");
	ptr->brlcad_name = NULL;
    }
    ptr->solid_use_no = 0;
    ptr->comb_use_no = 0;

    if (type != CUT_SOLID_TYPE) {
	/* make sure brlcad_name is unique */
	char *tmp;

	tmp = ptr->brlcad_name;
	ptr->brlcad_name = bu_strdup(Build_unique_name(ptr->brlcad_name));
	bu_free((char *)tmp, "brlcad_name");
    }

    if (type == ASSEMBLY_TYPE) {
	ptr->solid_name = NULL;
	return ptr;
    } else if (type == PART_TYPE) {
	struct bu_vls vls;

	bu_vls_init(&vls);

	bu_vls_strcpy(&vls, "s.");
	bu_vls_strcat(&vls, ptr->brlcad_name);

	ptr->solid_name = bu_vls_strgrab(&vls);
    } else {
	struct bu_vls vls;

	bu_vls_init(&vls);

	bu_vls_strcpy(&vls, "s.");
	bu_vls_strcat(&vls, ptr->brlcad_name);

	ptr->solid_name = bu_vls_strgrab(&vls);
    }

    /* make sure solid name is unique */
    ptr->solid_name = bu_strdup(Build_unique_name(ptr->solid_name));
    return ptr;
}

static char *
Get_unique_name(char *name, unsigned int obj, int type)
{
    struct name_conv_list *ptr, *prev;

    if (name_root == (struct name_conv_list *)NULL) {
	/* start new list */
	name_root = Add_new_name(name, obj, type);
	ptr = name_root;
    } else {
	int found=0;

	prev = (struct name_conv_list *)NULL;
	ptr = name_root;
	while (ptr && !found) {
	    if (obj == ptr->obj)
		found = 1;
	    else {
		prev = ptr;
		ptr = ptr->next;
	    }
	}

	if (!found) {
	    prev->next = Add_new_name(name, obj, type);
	    ptr = prev->next;
	}
    }

    return ptr->brlcad_name;
}

static char *
Get_solid_name(char *name, unsigned int obj)
{
    struct name_conv_list *ptr;

    ptr = name_root;

    while (ptr && obj != ptr->obj)
	ptr = ptr->next;

    if (!ptr)
	ptr = Add_new_name(name, 0, PART_TYPE);

    return ptr->solid_name;
}

static void
Convert_assy(char *line)
{
    struct wmember head;
    struct wmember *wmem = NULL;
    char line1[MAX_LINE_SIZE];
    char name[MAX_LINE_SIZE];
    unsigned int obj;
    char memb_name[MAX_LINE_SIZE];
    unsigned int memb_obj;
    char *brlcad_name = NULL;
    float mat_col[4];
    float junk;
    int start;
    int i;

    if (RT_G_DEBUG & DEBUG_MEM_FULL) {
	bu_log("Barrier check at start of Convert_assy:\n");
	if (bu_mem_barriercheck())
	    bu_exit(EXIT_FAILURE,  "Barrier check failed!!!\n");
    }

    BU_LIST_INIT(&head.l);

    start = (-1);
    /* skip leading blanks */
    while (isspace(line[++start]) && line[start] != '\0');
    if (strncmp(&line[start], "assembly", 8) && strncmp(&line[start], "ASSEMBLY", 8)) {
	bu_log("PROE-G: Convert_assy called for non-assembly:\n%s\n", line);
	return;
    }

    /* skip blanks before name */
    start += 7;
    while (isspace(line[++start]) && line[start] != '\0');

    /* get name */
    i = (-1);
    start--;
    while (!isspace(line[++start]) && line[start] != '\0' && line[start] != '\n')
	name[++i] = line[start];
    name[++i] = '\0';

    /* get object pointer */
    sscanf(&line[start], "%x %f", &obj, &junk);

    bu_log("Converting Assembly: %s\n", name);

    if (debug)
	bu_log("Convert_assy: %s x%x\n", name, obj);

    while (bu_fgets(line1, MAX_LINE_SIZE, fd_in)) {
	/* skip leading blanks */
	start = (-1);
	while (isspace(line1[++start]) && line[start] != '\0');

	if (!strncmp(&line1[start], "endassembly", 11) || !strncmp(&line1[start], "ENDASSEMBLY", 11)) {

	    brlcad_name = Get_unique_name(name, obj, ASSEMBLY_TYPE);
	    if (debug) {
		struct wmember *wp;

		bu_log("\tmake assembly (%s)\n", brlcad_name);
		for (BU_LIST_FOR (wp, wmember, &head.l))
		    bu_log("\t%c %s\n", wp->wm_op, wp->wm_name);
	    } else {
		bu_log("\tUsing name: %s\n", brlcad_name);
	    }

	    mk_lcomb(fd_out, brlcad_name, &head, 0 ,
		     (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0);
	    break;
	} else if (!strncmp(&line1[start], "member", 6) || !strncmp(&line1[start], "MEMBER", 6)) {
	    start += 5;
	    while (isspace(line1[++start]) && line1[start] != '\0');
	    i = (-1);
	    start--;
	    while (!isspace(line1[++start]) && line1[start] != '\0' && line1[start] != '\n')
		memb_name[++i] = line1[start];
	    memb_name[++i] = '\0';


	    sscanf(&line1[start], "%x", &memb_obj);

	    brlcad_name = Get_unique_name(memb_name, memb_obj, PART_TYPE);
	    if (debug)
		bu_log("\tmember (%s)\n", brlcad_name);
	    wmem = mk_addmember(brlcad_name, &head.l, NULL, WMOP_UNION);
	} else if (!strncmp(&line1[start], "matrix", 6) || !strncmp(&line1[start], "MATRIX", 6)) {
	    size_t j;
	    double scale, inv_scale;

	    for (j=0; j<4; j++) {
		bu_fgets(line1, MAX_LINE_SIZE, fd_in);
		sscanf(line1, "%f %f %f %f", &mat_col[0], &mat_col[1], &mat_col[2], &mat_col[3]);
		for (i=0; i<4; i++)
		    wmem->wm_mat[4*i+j] = mat_col[i];
	    }

	    /* convert this matrix to seperate scale factor into element #15 */
/*			scale = MAGNITUDE(&wmem->wm_mat[0]); */
	    scale = pow(bn_mat_det3(wmem->wm_mat), 1.0/3.0);
	    if (debug) {
		bn_mat_print(brlcad_name, wmem->wm_mat);
		bu_log("\tscale = %g, conv_factor = %g\n", scale, conv_factor);
	    }
	    if (!ZERO(scale - 1.0)) {
		inv_scale = 1.0/scale;
		for (j=0; j<3; j++)
		    HSCALE(&wmem->wm_mat[j*4], &wmem->wm_mat[j*4], inv_scale)

			/* clamp rotation elements to fabs(1.0) */
			for (j=0; j<3; j++) {
			    for (i=0; i<3; i++) {
				if (wmem->wm_mat[j*4 + i] > 1.0)
				    wmem->wm_mat[j*4 + i] = 1.0;
				else if (wmem->wm_mat[j*4 + i] < -1.0)
				    wmem->wm_mat[j*4 + i] = -1.0;
			    }
			}

		if (top_level)
		    wmem->wm_mat[15] *= (inv_scale/conv_factor);
		else
		    wmem->wm_mat[15] *= inv_scale;
	    } else if (top_level)
		wmem->wm_mat[15] /= conv_factor;

	    if (top_level && do_reorient) {
		/* apply re_orient transformation here */
		if (debug) {
		    bu_log("Applying re-orient matrix to member %s\n", brlcad_name);
		    bn_mat_print("re-orient matrix", re_orient);
		}
		bn_mat_mul2(re_orient, wmem->wm_mat);
	    }
	    if (debug)
		bn_mat_print("final matrix", wmem->wm_mat);
	} else {
	    bu_log("Unrecognized line in assembly (%s)\n%s\n", name, line1);
	}
    }

    if (RT_G_DEBUG & DEBUG_MEM_FULL) {
	bu_log("Barrier check at end of Convet_assy:\n");
	if (bu_mem_barriercheck())
	    bu_exit(EXIT_FAILURE,  "Barrier check failed!!!\n");
    }

    top_level = 0;

}

static void
do_modifiers(char *line1, int *start, struct wmember *head, char *name, fastf_t *min, fastf_t *max)
{
    struct wmember *wmem;
    int i;

    while (strncmp(&line1[*start], "endmodifiers", 12) && strncmp(&line1[*start], "ENDMODIFIERS", 12)) {
	if (!strncmp(&line1[*start], "plane", 5) || !strncmp(&line1[*start], "PLANE", 5)) {
	    struct name_conv_list *ptr;
	    char haf_name[MAX_LINE_SIZE];
	    fastf_t dist;
	    fastf_t tmp_dist;
	    point_t origin;
	    plane_t plane;
	    vect_t e1, e2;
	    double u_min, u_max, v_min, v_max;
	    double x, y, z;
	    int orient;
	    point_t arb_pt[8];
	    point_t rpp_corner;

	    bu_fgets(line1, MAX_LINE_SIZE, fd_in);
	    sscanf(line1, "%lf %lf %lf", &x, &y, &z);
	    VSET(origin, x, y, z);
	    bu_fgets(line1, MAX_LINE_SIZE, fd_in);
	    sscanf(line1, "%lf %lf %lf", &x, &y, &z);
	    VSET(e1, x, y, z);
	    bu_fgets(line1, MAX_LINE_SIZE, fd_in);
	    sscanf(line1, "%lf %lf %lf", &x, &y, &z);
	    VSET(e2, x, y, z);
	    bu_fgets(line1, MAX_LINE_SIZE, fd_in);
	    sscanf(line1, "%lf %lf %lf", &x, &y, &z);
	    VSET(plane, x, y, z);
	    bu_fgets(line1, MAX_LINE_SIZE, fd_in);
	    sscanf(line1, "%lf %lf", &u_min, &v_min);
	    bu_fgets(line1, MAX_LINE_SIZE, fd_in);
	    sscanf(line1, "%lf %lf", &u_max, &v_max);
	    bu_fgets(line1, MAX_LINE_SIZE, fd_in);
	    sscanf(line1, "%d", &orient);

	    plane[H] = VDOT(plane, origin);

	    VJOIN2(arb_pt[0], origin, u_min, e1, v_min, e2);
	    VJOIN2(arb_pt[1], origin, u_max, e1, v_min, e2);
	    VJOIN2(arb_pt[2], origin, u_max, e1, v_max, e2);
	    VJOIN2(arb_pt[3], origin, u_min, e1, v_max, e2);

	    /* find max distance to corner of enclosing RPP */
	    dist = 0.0;
	    VSET(rpp_corner, min[X], min[Y], min[Z]);
	    tmp_dist = DIST_PT_PLANE(rpp_corner, plane) * (fastf_t)orient;
	    if (tmp_dist > dist)
		dist = tmp_dist;

	    VSET(rpp_corner, min[X], min[Y], max[Z]);
	    tmp_dist = DIST_PT_PLANE(rpp_corner, plane) * (fastf_t)orient;
	    if (tmp_dist > dist)
		dist = tmp_dist;

	    VSET(rpp_corner, min[X], max[Y], min[Z]);
	    tmp_dist = DIST_PT_PLANE(rpp_corner, plane) * (fastf_t)orient;
	    if (tmp_dist > dist)
		dist = tmp_dist;

	    VSET(rpp_corner, min[X], max[Y], max[Z]);
	    tmp_dist = DIST_PT_PLANE(rpp_corner, plane) * (fastf_t)orient;
	    if (tmp_dist > dist)
		dist = tmp_dist;

	    VSET(rpp_corner, max[X], min[Y], min[Z]);
	    tmp_dist = DIST_PT_PLANE(rpp_corner, plane) * (fastf_t)orient;
	    if (tmp_dist > dist)
		dist = tmp_dist;

	    VSET(rpp_corner, max[X], min[Y], max[Z]);
	    tmp_dist = DIST_PT_PLANE(rpp_corner, plane) * (fastf_t)orient;
	    if (tmp_dist > dist)
		dist = tmp_dist;

	    VSET(rpp_corner, max[X], max[Y], min[Z]);
	    tmp_dist = DIST_PT_PLANE(rpp_corner, plane) * (fastf_t)orient;
	    if (tmp_dist > dist)
		dist = tmp_dist;

	    VSET(rpp_corner, max[X], max[Y], max[Z]);
	    tmp_dist = DIST_PT_PLANE(rpp_corner, plane) * (fastf_t)orient;
	    if (tmp_dist > dist)
		dist = tmp_dist;

	    for (i=0; i<4; i++) {
		VJOIN1(arb_pt[i+4], arb_pt[i], dist*(fastf_t)orient, plane);
	    }

	    if (top_level) {
		for (i=0; i<8; i++) {
		    VSCALE(arb_pt[i], arb_pt[i], conv_factor);
		}
	    }

	    cut_count++;

	    snprintf(haf_name, MAX_LINE_SIZE, "cut.%d", cut_count);
	    ptr = Add_new_name(haf_name, 0, CUT_SOLID_TYPE);
	    if (mk_arb8(fd_out, ptr->solid_name, (fastf_t *)arb_pt))
		bu_log("Failed to create ARB8 solid for Assembly cut in part %s\n", name);
	    else {
		/* Add this cut to the region */
		wmem = mk_addmember(ptr->solid_name, &(head->l), NULL,
				    WMOP_SUBTRACT);

		if (top_level && do_reorient) {
		    /* apply re_orient transformation here */
		    if (debug) {
			bu_log("Applying re-orient matrix to solid %s\n", ptr->solid_name);
			bn_mat_print("re-orient matrix", re_orient);
		    }
		    bn_mat_mul2(re_orient, wmem->wm_mat);
		}

	    }
	}
	bu_fgets(line1, MAX_LINE_SIZE, fd_in);
	(*start) = (-1);
	while (isspace(line1[++(*start)]));
    }
}

void
Add_face(int *face)
{
    if (!bot_faces) {
	bot_faces = (int *)bu_malloc(3 * BOT_FBLOCK * sizeof(int), "bot_faces");
	bot_fsize = BOT_FBLOCK;
	bot_fcurr = 0;
    } else if (bot_fcurr >= bot_fsize) {
	bot_fsize += BOT_FBLOCK;
	bot_faces = (int *)bu_realloc((void *)bot_faces, 3 * bot_fsize * sizeof(int), "bot_faces increase");
    }

    VMOVE(&bot_faces[3*bot_fcurr], face);
    bot_fcurr++;
}

static void
Convert_part(char *line)
{
    char line1[MAX_LINE_SIZE];
    char name[MAX_LINE_SIZE];
    unsigned int obj=0;
    char *solid_name;
    int start;
    int i;
    int face_count=0;
    int degenerate_count=0;
    int small_count=0;
    float colr[3] = VINITALL(0.5);
    unsigned char color[3]={ 128, 128, 128 };
    char *brlcad_name;
    struct wmember head;
    struct wmember *wmem;
    vect_t normal={ 0, 0, 0 };
    int solid_in_region=0;
    point_t part_max, part_min;	/* Part RPP */

    if (RT_G_DEBUG & DEBUG_MEM_FULL)
	bu_prmem("At start of Conv_prt():\n");

    if (RT_G_DEBUG & DEBUG_MEM_FULL) {
	bu_log("Barrier check at start of Convet_part:\n");
	if (bu_mem_barriercheck())
	    bu_exit(EXIT_FAILURE,  "Barrier check failed!!!\n");
    }


    bot_fcurr = 0;
    BU_LIST_INIT(&head.l);
    VSETALL(part_min, MAX_FASTF);
    VSETALL(part_max, -MAX_FASTF);

    clean_vert_tree(vert_tree_root);

    start = (-1);
    /* skip leading blanks */
    while (isspace(line[++start]) && line[start] != '\0');
    if (strncmp(&line[start], "solid", 5) && strncmp(&line[start], "SOLID", 5)) {
	bu_log("Convert_part: Called for non-part\n%s\n", line);
	return;
    }

    /* skip blanks before name */
    start += 4;
    while (isspace(line[++start]) && line[start] != '\0');

    if (line[start] != '\0') {
	/* get name */
	i = (-1);
	start--;
	while (!isspace(line[++start]) && line[start] != '\0' && line[start] != '\n')
	    name[++i] = line[start];
	name[++i] = '\0';

	/* get object id */
	sscanf(&line[start], "%x", &obj);
    } else if (stl_format && forced_name) {
	bu_strlcpy(name, forced_name, MAX_LINE_SIZE);
    } else if (stl_format) {
	/* build a name from the file name */
	char tmp_str[512];
	char *ptr;
	int len, suff_len;

	obj_count++;
	obj = obj_count;

	/* copy the file name into our work space */
	bu_strlcpy(tmp_str, input_file, sizeof(tmp_str));

	/* eliminate a trailing ".stl" */
	len = strlen(tmp_str);
	if (len > 4) {
	    if (!strncmp(&tmp_str[len-4], ".stl", 4))
		tmp_str[len-4] = '\0';
	}

	/* skip over all characters prior to the last '/' */
	ptr = strrchr(tmp_str, '/');
	if (!ptr)
	    ptr = tmp_str;
	else
	    ptr++;

	/* now copy what is left to the name */
	bu_strlcpy(name, ptr, MAX_LINE_SIZE);

	sprintf(tmp_str, "_%d", obj_count);
	len = strlen(name);
	suff_len = strlen(tmp_str);
	if (len + suff_len < MAX_LINE_SIZE-1)
	    bu_strlcat(name, tmp_str, sizeof(name));
	else
	    snprintf(&name[MAX_LINE_SIZE-suff_len-1], MAX_LINE_SIZE, "%s", tmp_str);
	name[MAX_LINE_SIZE-1] = '\0'; /* sanity */

    } else {
	bu_strlcpy(name, "noname", MAX_LINE_SIZE);
    }

    bu_log("Converting Part: %s\n", name);

    if (debug)
	bu_log("Conv_part %s x%x\n", name, obj);

    solid_count++;
    solid_name = Get_solid_name(name, obj);

    bu_log("\tUsing solid name: %s\n", solid_name);

    if (RT_G_DEBUG & DEBUG_MEM || RT_G_DEBUG & DEBUG_MEM_FULL)
	bu_prmem("At start of Convert_part()");

    while (bu_fgets(line1, MAX_LINE_SIZE, fd_in) != NULL) {
	start = (-1);
	while (isspace(line1[++start]));
	if (!strncmp(&line1[start], "endsolid", 8) || !strncmp(&line1[start], "ENDSOLID", 8)) {
	    break;
	} else if (!strncmp(&line1[start], "color", 5) || !strncmp(&line1[start], "COLOR", 5)) {
	    sscanf(&line1[start+5], "%f%f%f", &colr[0], &colr[1], &colr[2]);
	    for (i=0; i<3; i++)
		color[i] = (int)(colr[i] * 255.0);
	} else if (!strncmp(&line1[start], "normal", 6) || !strncmp(&line1[start], "NORMAL", 6)) {
	    float x, y, z;

	    start += 6;
	    sscanf(&line1[start], "%f%f%f", &x, &y, &z);
	    VSET(normal, x, y, z);
	} else if (!strncmp(&line1[start], "facet", 5) || !strncmp(&line1[start], "FACET", 5)) {
	    VSET(normal, 0.0, 0.0, 0.0);

	    start += 4;
	    while (line1[++start] && isspace(line1[start]));

	    if (line1[start]) {
		if (!strncmp(&line1[start], "normal", 6) || !strncmp(&line1[start], "NORMAL", 6)) {
		    float x, y, z;

		    start += 6;
		    sscanf(&line1[start], "%f%f%f", &x, &y, &z);
		    VSET(normal, x, y, z);
		}
	    }
	} else if (!strncmp(&line1[start], "outer loop", 10) || !strncmp(&line1[start], "OUTER LOOP", 10)) {
	    int endloop=0;
	    int vert_no=0;
	    int tmp_face[3];

	    while (!endloop) {
		if (bu_fgets(line1, MAX_LINE_SIZE, fd_in) == NULL)
		    bu_exit(EXIT_FAILURE,  "Unexpected EOF while reading a loop in a part!!!\n");

		start = (-1);
		while (isspace(line1[++start]));

		if (!strncmp(&line1[start], "endloop", 7) || !strncmp(&line1[start], "ENDLOOP", 7)) {
		    endloop = 1;
		} else if (!strncmp(&line1[start], "vertex", 6) || !strncmp(&line1[start], "VERTEX", 6)) {
		    double x, y, z;

		    sscanf(&line1[start+6], "%lf%lf%lf", &x, &y, &z);
		    if (top_level) {
			x *= conv_factor;
			y *= conv_factor;
			z *= conv_factor;
		    }

		    if (vert_no > 2) {
			int n;

			bu_log("Non-triangular loop:\n");
			for (n=0; n<3; n++)
			    bu_log("\t(%g %g %g)\n", V3ARGS(&vert_tree_root->the_array[tmp_face[n]]));

			bu_log("\t(%g %g %g)\n", x, y, z);
		    }
		    tmp_face[vert_no++] = Add_vert(x, y, z, vert_tree_root, tol.dist_sq);
		    VMINMAX(part_min, part_max, &vert_tree_root->the_array[tmp_face[vert_no-1]*3]);
		} else
		    bu_log("Unrecognized line: %s\n", line1);
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
		for (n=0; n<3; n++)
		    bu_log("\tvertex #%d: (%g %g %g)\n", tmp_face[n], V3ARGS(&vert_tree_root->the_array[3*tmp_face[n]]));
		VPRINT(" normal", normal);
	    }

	    Add_face(tmp_face);
	    face_count++;
	} else if (!strncmp(&line1[start], "modifiers", 9) || !strncmp(&line1[start], "MODIFIERS", 9)) {
	    if (face_count) {
		wmem = mk_addmember(solid_name, &head.l, NULL, WMOP_UNION);
		if (top_level && do_reorient) {
		    /* apply re_orient transformation here */
		    if (debug) {
			bu_log("Applying re-orient matrix to solid %s\n", solid_name);
			bn_mat_print("re-orient matrix", re_orient);
		    }
		    bn_mat_mul2(re_orient, wmem->wm_mat);
		}
		solid_in_region = 1;
	    }
	    do_modifiers(line1, &start, &head, name, part_min, part_max);
	}
    }

    /* Check if this part has any solid parts */
    if (face_count == 0) {
	char *save_name;

	bu_log("\t%s has no solid parts, ignoring\n", name);
	if (degenerate_count)
	    bu_log("\t%d faces were degenerate\n", degenerate_count);
	if (small_count)
	    bu_log("\t%d faces were too small\n", small_count);
	brlcad_name = Get_unique_name(name, obj, PART_TYPE);
	save_name = bu_strdup(brlcad_name);
	bu_ptbl_ins(&null_parts, (long *)save_name);
	return;
    } else {
	if (degenerate_count)
	    bu_log("\t%d faces were degenerate\n", degenerate_count);
	if (small_count)
	    bu_log("\t%d faces were too small\n", small_count);
    }

    mk_bot(fd_out, solid_name, RT_BOT_SOLID, RT_BOT_UNORIENTED, 0, vert_tree_root->curr_vert, bot_fcurr,
	   vert_tree_root->the_array, bot_faces, NULL, NULL);

    if (face_count && !solid_in_region) {
	wmem = mk_addmember(solid_name, &head.l, NULL, WMOP_UNION);
	if (top_level && do_reorient) {
	    /* apply re_orient transformation here */
	    if (debug) {
		bu_log("Applying re-orient matrix to solid %s\n", solid_name);
		bn_mat_print("re-orient matrix", re_orient);
	    }
	    bn_mat_mul2(re_orient, wmem->wm_mat);
	}
    }
    brlcad_name = Get_unique_name(name, obj, PART_TYPE);

    if (do_air) {
	bu_log("\tMaking air region (%s)\n", brlcad_name);

	mk_lrcomb(fd_out, brlcad_name, &head, 1, (char *)NULL, (char *)NULL,
		  color, 0, air_no, 0, 100, 0);
	air_no++;
    } else {
	bu_log("\tMaking region (%s)\n", brlcad_name);

	if (const_id >= 0) {
	    mk_lrcomb(fd_out, brlcad_name, &head, 1, (char *)NULL, (char *)NULL,
		      color, const_id, 0, mat_code, 100, 0);
	    if (stl_format && face_count)
		(void)mk_addmember(brlcad_name, &all_head.l, NULL, WMOP_UNION);
	} else {
	    mk_lrcomb(fd_out, brlcad_name, &head, 1, (char *)NULL, (char *)NULL,
		      color, id_no, 0, mat_code, 100, 0);
	    if (stl_format && face_count)
		(void)mk_addmember(brlcad_name, &all_head.l, NULL, WMOP_UNION);
	    id_no++;
	}
    }

    if (RT_G_DEBUG & DEBUG_MEM_FULL) {
	bu_log("Barrier check at end of Convert_part:\n");
	if (bu_mem_barriercheck())
	    bu_exit(EXIT_FAILURE,  "Barrier check failed!!!\n");
    }

    top_level = 0;

    return;
}

static void
Convert_input(void)
{
    char line[ MAX_LINE_SIZE ];

    if (!stl_format) {
	if (!bu_fgets(line, MAX_LINE_SIZE, fd_in))
	    return;

	sscanf(line, "%f", &conv_factor);
    }

    if (!do_reorient && !stl_format)
	conv_factor = 1.0;

    while (bu_fgets(line, MAX_LINE_SIZE, fd_in) != NULL) {
	if (!strncmp(line, "assembly", 8) || !strncmp(line, "ASSEMBLY", 8))
	    Convert_assy(line);
	else if (!strncmp(line, "solid", 5) || !strncmp(line, "SOLID", 5))
	    Convert_part(line);
	else
	    bu_log("Unrecognized line:\n%s\n", line);
    }
}

static void
Rm_nulls(void)
{
    struct db_i *dbip;
    size_t i;
    struct directory *dp;

    dbip = fd_out->dbip;

    if (debug || BU_PTBL_LEN(&null_parts) ) {
	bu_log("Deleting references to the following null parts:\n");
	for (i=0; i<BU_PTBL_LEN(&null_parts); i++) {
	    char *save_name;

	    save_name = (char *)BU_PTBL_GET(&null_parts, i);
	    bu_log("\t%s\n", save_name);
	}
    }

    FOR_ALL_DIRECTORY_START(dp, dbip) {
	struct rt_tree_array *tree_list;
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;
	size_t j;
	size_t node_count;
	size_t actual_count;
	int changed=0;

	/* skip solids */
	if (dp->d_flags & RT_DIR_SOLID)
	    continue;

	/* skip non-geometry */
	if (!(dp->d_flags & (RT_DIR_SOLID | RT_DIR_COMB)))
	    continue;

	if (rt_db_get_internal(&intern, dp, dbip, (matp_t)NULL, &rt_uniresource) < 1) {
	    bu_log("Cannot get internal form of combination %s\n", dp->d_namep);
	    continue;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);
	if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
	    db_non_union_push(comb->tree, &rt_uniresource);
	    if (db_ck_v4gift_tree(comb->tree) < 0) {
		bu_log("Cannot flatten tree (%s) for editing\n", dp->d_namep);
		continue;
	    }
	}
	node_count = db_tree_nleaves(comb->tree);
	if (node_count > 0) {
	    tree_list = (struct rt_tree_array *)bu_calloc(node_count,
							  sizeof(struct rt_tree_array), "tree list");
	    actual_count = (struct rt_tree_array *)db_flatten_tree(tree_list, comb->tree, OP_UNION, 0, &rt_uniresource) - tree_list;
	    BU_ASSERT_SIZE_T(actual_count, ==, node_count);
	} else {
	    tree_list = (struct rt_tree_array *)NULL;
	    actual_count = 0;
	}

	for (j=0; j<actual_count; j++) {
	    size_t k;
	    int found=0;

	    for (k=0; k<BU_PTBL_LEN(&null_parts); k++) {
		char *save_name;

		save_name = (char *)BU_PTBL_GET(&null_parts, k);
		if (BU_STR_EQUAL(save_name, tree_list[j].tl_tree->tr_l.tl_name)) {
		    found = 1;
		    break;
		}
	    }
	    if (found) {
		/* This is a NULL part, delete the reference */
/*				if (debug) */
		bu_log("Deleting reference to null part (%s) from combination %s\n",
		       tree_list[j].tl_tree->tr_l.tl_name, dp->d_namep);

		db_free_tree(tree_list[j].tl_tree, &rt_uniresource);

		for (k=j+1; k<actual_count; k++)
		    tree_list[k-1] = tree_list[k]; /* struct copy */

		actual_count--;
		j--;
		changed = 1;
	    }
	}

	if (changed) {
	    if (actual_count)
		comb->tree = (union tree *)db_mkgift_tree(tree_list, actual_count, &rt_uniresource);
	    else
		comb->tree = (union tree *)NULL;

	    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
		const struct rt_functab *ftp = rt_get_functab_by_label("comb");
		bu_log("Unable to write modified combination '%s' to database\n", dp->d_namep);
		if (ftp && ftp->ft_ifree)
		    ftp->ft_ifree(&intern);
		continue;
	    }
	}
	bu_free((char *)tree_list, "tree_list");
    } FOR_ALL_DIRECTORY_END;
}

static void
proe_usage(const char *argv0)
{
    bu_log("%s [-darS] [-t tolerance] [-i initial_ident] [-I constant_ident] [-m material_code] [-u reg_exp] [-x rt_debug_flag] proe_file.brl output.g\n", argv0);
    bu_log("	where proe_file.brl is the output from Pro/Engineer's BRL-CAD EXPORT option\n");
    bu_log("	and output.g is the name of a BRL-CAD database file to receive the conversion.\n");
    bu_log("	The -d option prints additional debugging information.\n");
    bu_log("	The -i option sets the initial region ident number (default is 1000).\n");
    bu_log("	The -I option sets the non-negative ident number that will be assigned to all regions (conflicts with -i).\n");
    bu_log("	The -m option sets the integer material code for all the parts. (default is 1)\n");
    bu_log("	The -u option indicates that portions of object names that match the regular expression\n");
    bu_log("		'reg_exp' should be ignored.\n");
    bu_log("	The -a option creates BRL-CAD 'air' regions from everything in the model.\n");
    bu_log("	The -r option indicates that the model should not be re-oriented or scaled, \n");
    bu_log("		but left in the same orientation as it was in Pro/E.\n");
    bu_log("		This is to allow conversion of parts to be included in\n");
    bu_log("		previously converted Pro/E assemblies.\n");
    bu_log("	The -S option indicates that the input file is raw STL (STereoLithography) format.\n");
    bu_log("	The -t option specifies the minumim distance between two distinct vertices (mm).\n");
    bu_log("	The -x option specifies an RT debug flags (see raytrace.h).\n");
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
    int c;

    tol.magic = BN_TOL_MAGIC;

    /* this value selected as a resaonable compromise between eliminating
     * needed faces and keeping degenerate faces
     */
    tol.dist = 0.0005;	/* default, same as MGED, RT, ... */
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1 - tol.perp;

    vert_tree_root = create_vert_tree();

    bu_ptbl_init(&null_parts, 64, " &null_parts");
    bu_vls_init(&ret_name);

    forced_name = NULL;

    if (argc < 2) {
	proe_usage(argv[0]);
	bu_exit(1, NULL);
    }

    /* Get command line arguments. */
    while ((c = bu_getopt(argc, argv, "St:i:I:m:rsdax:u:N:c:")) != -1) {
	double tmp;

	switch (c) {
	    case 't':	/* tolerance */
		tmp = atof(bu_optarg);
		if (tmp <= 0.0) {
		    bu_log("Tolerance must be greater then zero, using default (%g)\n",
			   tol.dist);
		    break;
		}
		tol.dist = tmp;
		tol.dist_sq = tmp * tmp;
		break;
	    case 'c':	/* convert from units */
		conv_factor = bu_units_conversion(bu_optarg);
		if (ZERO(conv_factor)) {
		    bu_log("Illegal units: (%s)\n", bu_optarg);
		    bu_exit(EXIT_FAILURE,  "Illegal units!!\n");
		} else {
		    bu_log("Converting units from %s to mm (conversion factor is %g)\n", bu_optarg, conv_factor);
		}
		break;
	    case 'N':	/* force a name on this object */
		forced_name = bu_optarg;
		break;

	    case 'S':	/* raw stl_format format */
		stl_format = 1;
		do_reorient = 0;
		break;
	    case 'i':
		id_no = atoi(bu_optarg);
		break;
	    case  'I':
		const_id = atoi(bu_optarg);
		if (const_id < 0) {
		    bu_log("Illegal value for '-I' option, must be zero or greater!!!\n");
		    proe_usage(argv[0]);
		    bu_exit(EXIT_FAILURE,  "Illegal value for option '-I'\n");
		}
		break;
	    case 'm':
		mat_code = atoi(bu_optarg);
		break;
	    case 'd':
		debug = 1;
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *)&rt_g.debug);
		bu_printb("librt RT_G_DEBUG", RT_G_DEBUG, DEBUG_FORMAT);
		bu_log("\n");
		break;
	    case 'u':
		do_regex = 1;
		if (regcomp(&reg_cmp, bu_optarg, 0)) {
		    bu_log("Bad regular expression (%s)\n", bu_optarg);
		    proe_usage(argv[0]);
		    bu_exit(1, "ERROR: Bad regular expression\n");
		}
		break;
	    case 'a':
		do_air = 1;
		break;
	    case 'r':
		do_reorient = 0;
		break;
	    default:
		proe_usage(argv[0]);
		bu_exit(1, "ERROR: unrecognized argument\n");
		break;
	}
    }

    rt_init_resource(&rt_uniresource, 0, NULL);

    input_file = argv[bu_optind];
    if ((fd_in=fopen(input_file, "rb")) == NULL) {
	bu_log("Cannot open input file (%s)\n", input_file);
	perror(argv[0]);
	bu_exit(1, NULL);
    }
    bu_optind++;
    brlcad_file = argv[bu_optind];
    if ((fd_out=wdb_fopen(brlcad_file)) == NULL) {
	bu_log("Cannot open BRL-CAD file (%s)\n", brlcad_file);
	perror(argv[0]);
	bu_exit(1, NULL);
    }

    if (stl_format)
	mk_id_units(fd_out, "Conversion from Stereolithography format", "mm");
    else
	mk_id_units(fd_out, "Conversion from Pro/Engineer", "in");

    /* Create re-orient matrix */
    bn_mat_angles(re_orient, 0.0, 90.0, 90.0);

    BU_LIST_INIT(&all_head.l);

    Convert_input();

    if (stl_format) {
	/* make a top level group */
	mk_lcomb(fd_out, "all", &all_head, 0, (char *)NULL, (char *)NULL, (unsigned char *)NULL, 0);
    }

    fclose(fd_in);

    /* Remove references to null parts */
    Rm_nulls();

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
