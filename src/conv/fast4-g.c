/*                       F A S T 4 - G . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2021 United States Government as represented by
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
/** @file conv/fast4-g.c
 *
 * Program to convert the FASTGEN4 format to BRL-CAD.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "bio.h"

/* interface headers */
#include "bu/app.h"
#include "bu/debug.h"
#include "bu/getopt.h"
#include "bu/path.h"
#include "rt/db4.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"
#include "bn/plot3.h"


/* NOTE: there should be no space after comma */
#define COMMA ','


/* convenient macro for building regions */
#define MK_REGION(fp, headp, name, r_id, rgb) {\
	if (s->mode == 1) {\
	    if (!s->quiet)\
		bu_log("Making region: %s (PLATE)\n", name); \
	    mk_comb(fp, name, &((headp)->l), 'P', (char *)NULL, (char *)NULL, rgb, r_id, 0, 1, 100, 0, 0, 0); \
	} else if (s->mode == 2) {\
	    if (!s->quiet) \
		bu_log("Making region: %s (VOLUME)\n", name); \
	    mk_comb(fp, name, &((headp)->l), 'V', (char *)NULL, (char *)NULL, rgb, r_id, 0, 1, 100, 0, 0, 0); \
	} else {\
	    bu_log("Illegal mode (%d), while trying to make region (%s)\n", s->mode, name);\
	    bu_log("\tRegion not made!\n");\
	}\
    }


#define PUSH(ptr) bu_ptbl_ins(s->stack, (long *)ptr)
#define POP(structure, ptr) { \
	if (BU_PTBL_LEN(s->stack) == 0) \
	    ptr = (struct structure *)NULL; \
	else { \
	    ptr = (struct structure *)BU_PTBL_GET(s->stack, BU_PTBL_LEN(s->stack)-1); \
	    bu_ptbl_rm(s->stack, (long *)ptr); \
	} \
    }
#define PUSH2(ptr) bu_ptbl_ins(s->stack2, (long *)ptr)
#define POP2(structure, ptr) { \
	if (BU_PTBL_LEN(s->stack2) == 0) \
	    ptr = (struct structure *)NULL; \
	else { \
	    ptr = (struct structure *)BU_PTBL_GET(s->stack2, BU_PTBL_LEN(s->stack2)-1); \
	    bu_ptbl_rm(s->stack2, (long *)ptr); \
	} \
    }


#define NAME_TREE_MAGIC 0x55555555
#define CK_TREE_MAGIC(ptr) {\
	if (!ptr)\
	    bu_log("ERROR: Null name_tree pointer, file=%s, line=%d\n", __FILE__, __LINE__);\
	else if (ptr->magic != NAME_TREE_MAGIC)\
	    bu_log("ERROR: bad name_tree pointer (%p), file=%s, line=%d\n", (void *)ptr, __FILE__, __LINE__);\
    }


#define PLATE_MODE	1
#define VOLUME_MODE	2

#define POS_CENTER	1	/* face positions for facets */
#define POS_FRONT	2

#define END_OPEN	1	/* End closure codes for cones */
#define END_CLOSED	2

#define GRID_BLOCK	256	/* allocate space for grid points in blocks of 256 points */

#define CLINE		'l'
#define CHEX1		'p'
#define CHEX2		'b'
#define CTRI		't'
#define CQUAD		'q'
#define CCONE1		'c'
#define CCONE2		'd'
#define CCONE3		'e'
#define CSPHERE		's'
#define NMG		'n'
#define BOT		't'
#define COMPSPLT	'h'

#define HOLE 1
#define WALL 2
#define INT_LIST_BLOCK		256	/* Number of int_list array slots to allocate */
#define MAX_LINE_SIZE			128	/* Length of char array for input line */
#define REGION_LIST_BLOCK	256	/* initial length of array of region ids to process */


struct fast4_color {
    struct bu_list l;
    short low;
    short high;
    unsigned char rgb[3];
};


struct cline {
    int pt1, pt2;
    int element_id;
    int made;
    fastf_t thick;
    fastf_t radius;
    struct cline *next;
} *cline_root;

struct name_tree {
    uint32_t magic;
    int region_id;
    int mode;		/* PLATE_MODE or VOLUME_MODE */
    int inner;		/* 0 => this is a base/group name for a FASTGEN element */
    int in_comp_group;	/* > 0 -> region already in a component group */
    char *name;
    struct name_tree *nleft, *nright, *rleft, *rright;
} *name_root;

struct compsplt {
    int ident_to_split;
    int new_ident;
    fastf_t z;
    struct compsplt *next;
} *compsplt_root;

struct hole_list {
    int group;
    int component;
    struct hole_list *next;
};


struct holes {
    int group;
    int component;
    int type;
    struct hole_list *holes;
    struct holes *next;
} *hole_root;


static int hex_faces[12][3]={
    { 0, 1, 4 }, /* 1 */
    { 1, 5, 4 }, /* 2 */
    { 1, 2, 5 }, /* 3 */
    { 2, 6, 5 }, /* 4 */
    { 2, 3, 6 }, /* 5 */
    { 3, 7, 6 }, /* 6 */
    { 3, 0, 7 }, /* 7 */
    { 0, 4, 7 }, /* 8 */
    { 4, 6, 7 }, /* 9 */
    { 4, 5, 6 }, /* 10 */
    { 0, 1, 2 }, /* 11 */
    { 0, 2, 3 }  /* 12 */
};

struct f4g_state {
    struct fast4_color HeadColor;
    char line[MAX_LINE_SIZE+1];	/* Space for input line */
    FILE *fpin;			/* Input FASTGEN4 file pointer */
    struct rt_wdb *fpout;	/* Output BRL-CAD file pointer */
    FILE *fp_plot;		/* file for plot output */
    FILE *fp_muves;		/* file for MUVES data, output CHGCOMP and CBACKING data */
    int grid_size;		/* Number of points that will fit in current grid_pts array */
    int max_grid_no;		/* Maximum grid number used */
    int mode;			/* Plate mode (1) or volume mode (2), of current component */
    int group_id;		/* Group identification number from SECTION card */
    int comp_id;		/* Component identification number from SECTION card */
    int region_id;		/* Region id number (group id no X 1000 + component id no) */
    int region_id_max;
    char field[9];		/* Space for storing one field from an input line */
    char vehicle[17];		/* Title for BRL-CAD model from VEHICLE card */
    int name_count;		/* Count of number of times this name_name has been used */
    int pass;			/* Pass number (0 -> only make names, 1-> do geometry) */
    int bot;			/* Flag: >0 -> There are BOT's in current component */
    int warnings;		/* Flag: >0 -> Print warning messages */
    int debug;			/* Debug flag */
    int frt_debug;		/* RT_G_DEBUG */
    int quiet;			/* flag to not blather */
    int comp_count;		/* Count of components in FASTGEN4 file */
    int f4_do_skips;		/* flag indicating that not all components will be processed */
    int *region_list;		/* array of region_ids to be processed */
    int region_list_len;	/* actual length of the malloc'd region_list array */
    int f4_do_plot;		/* flag indicating plot file should be created */
    struct wmember *group_head; /* Lists of regions for groups */
    ssize_t group_head_cnt;
    struct wmember hole_head;	/* List of regions used as holes (not solid parts of model) */
    struct bu_ptbl *stack;	/* Stack for traversing name_tree */
    struct bu_ptbl *stack2;	/* Stack for traversing name_tree */
    fastf_t min_radius;		/* minimum radius for TGC solids */

    int *faces;			/* one triplet per face indexing three grid points */
    fastf_t *thickness;		/* thickness of each face */
    char *facemode;		/* mode for each face */
    int face_size;		/* actual length of above arrays */
    int face_count;		/* number of faces in above arrays */

    point_t *grid_points;
};

void f4g_init(struct f4g_state *s)
{

    BU_LIST_INIT(&s->HeadColor.l);

    for (size_t i = 0; i < MAX_LINE_SIZE+1; i++) {
	s->line[i] = '\0';
    }
    s->fpin = NULL;
    s->fpout = NULL;
    s->fp_plot = NULL;
    s->fp_muves = NULL;
    s->max_grid_no = 0;
    s->mode = 0;
    s->group_id = -1;
    s->comp_id = -1;
    s->region_id = 0;
    s->region_id_max = 0;
    for (size_t i = 0; i < 9; i++) {
	s->field[i] = '\0';
    }
    for (size_t i = 0; i < 17; i++) {
	s->vehicle[i] = '\0';
    }
    s->name_count = 0;
    s->pass = 0;
    s->bot = 0;
    s->warnings = 0;
    s->debug = 0;
    s->frt_debug = 0;
    s->quiet = 0;
    s->comp_count = 0;
    s->f4_do_skips = 0;
    s->region_list = NULL;
    s->region_list_len = 0;
    s->f4_do_plot = 0;
    s->group_head = NULL;
    s->group_head_cnt=0;

    BU_LIST_INIT(&s->hole_head.l);

    BU_GET(s->stack, struct bu_ptbl);
    bu_ptbl_init(s->stack, 64, "stack");
    BU_GET(s->stack2, struct bu_ptbl);
    bu_ptbl_init(s->stack2, 64, "stack2");

    s->min_radius = 2.0 * sqrt(SQRT_SMALL_FASTF);

    s->faces = NULL;
    s->thickness = NULL;
    s->facemode = NULL;
    s->face_size = 0;
    s->face_count = 0;

    s->grid_size = GRID_BLOCK;
    s->grid_points = (point_t *)bu_calloc(s->grid_size, sizeof(point_t), "fast4-g: grid_points");
}

static void
usage() {
    bu_log("Usage: fast4-g [-dwq] [-c component_list] [-m muves_file] [-o plot_file] [-b BU_DEBUG_FLAG] [-x RT_DEBUG_FLAG] fastgen4_bulk_data_file output.g\n");
    bu_log("	d - print debugging info\n");
    bu_log("	q - quiet mode (don't say anything except error messages)\n");
    bu_log("	w - print warnings about creating default names\n");
    bu_log("	c - process only the listed region ids, may be a list (3001, 4082, 5347) or a range (2314-3527)\n");
    bu_log("	m - create a MUVES input file containing CHGCOMP and CBACKING elements\n");
    bu_log("	o - create a 'plot_file' containing a libplot3 plot file of all CTRI and CQUAD elements processed\n");
    bu_log("	b - set LIBBU debug flag\n");
    bu_log("	x - set RT debug flag\n");

    bu_exit(1, NULL);
}


static int
get_line(struct f4g_state *s)
{
    int len, done;
    struct bu_vls buffer = BU_VLS_INIT_ZERO;

    done = 0;
    while (!done) {
	len = bu_vls_gets(&buffer, s->fpin);
	if (len < 0) goto out; /* eof or error */
	if (len == 0) continue;
	bu_vls_trimspace(&buffer);
	len = (int)bu_vls_strlen(&buffer);
	if (len == 0) continue;
	done = 1;
    }

    if (len > MAX_LINE_SIZE)
	bu_log("WARNING: long line truncated\n");

    memset((void *)s->line, 0, MAX_LINE_SIZE);
    snprintf(s->line, MAX_LINE_SIZE, "%s", bu_vls_addr(&buffer));

out:
    bu_vls_free(&buffer);

    return len >= 0;
}


static unsigned char *
get_fast4_color(struct f4g_state *s, int r_id) {
    struct fast4_color *fcp;

    for (BU_LIST_FOR(fcp, fast4_color, &s->HeadColor.l)) {
	if (fcp->low <= r_id && r_id <= fcp->high)
	    return fcp->rgb;
    }

    return (unsigned char *)NULL;
}


static int
is_a_hole(int id)
{
    struct holes *hole_ptr;
    struct hole_list *ptr;

    hole_ptr = hole_root;

    while (hole_ptr) {
	if (hole_ptr->type == HOLE) {
	    ptr = hole_ptr->holes;
	    while (ptr) {
		if ((ptr->group * 1000 + ptr->component) == id)
		    return 1;
		ptr = ptr->next;
	    }
	}
	hole_ptr = hole_ptr->next;
    }
    return 0;
}


/*
  static void
  add_to_holes(char *name)
  {
  if (mk_addmember(name, &hole_head.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
  bu_log("add_to_holes: mk_addmember failed for region %s\n", name);
  }
*/


static void
plot_tri(struct f4g_state *s, int pt1, int pt2, int pt3)
{
    pdv_3move(s->fp_plot, s->grid_points[pt1]);
    pdv_3cont(s->fp_plot, s->grid_points[pt2]);
    pdv_3cont(s->fp_plot, s->grid_points[pt3]);
    pdv_3cont(s->fp_plot, s->grid_points[pt1]);
}


static void
Check_names(struct f4g_state *s)
{
    struct name_tree *ptr;

    if (!name_root)
	return;

    bu_ptbl_reset(s->stack);

    CK_TREE_MAGIC(name_root);
    /* ident order */
    ptr = name_root;
    while (1) {
	while (ptr) {
	    PUSH(ptr);
	    ptr = ptr->rleft;
	}
	POP(name_tree, ptr);
	if (!ptr)
	    break;

	/* visit node */
	CK_TREE_MAGIC(ptr);
	ptr = ptr->rright;
    }

    /* alphabetical order */
    ptr = name_root;
    while (1) {
	while (ptr) {
	    PUSH(ptr);
	    ptr = ptr->nleft;
	}
	POP(name_tree, ptr);
	if (!ptr)
	    break;

	/* visit node */
	CK_TREE_MAGIC(ptr);
	ptr = ptr->nright;
    }
}


static struct name_tree *
Search_names(struct name_tree *root, const char *name, int *found)
{
    struct name_tree *ptr;

    *found = 0;

    ptr = root;
    if (!ptr)
	return (struct name_tree *)NULL;

    while (1) {
	int diff;

	diff = bu_strcmp(name, ptr->name);
	if (diff == 0) {
	    *found = 1;
	    return ptr;
	} else if (diff > 0) {
	    if (ptr->nright)
		ptr = ptr->nright;
	    else
		return ptr;
	} else if (diff < 0) {
	    if (ptr->nleft)
		ptr = ptr->nleft;
	    else
		return ptr;
	}
    }
}


static struct name_tree *
Search_ident(struct name_tree *root, int reg_id, int *found)
{
    struct name_tree *ptr;

    *found = 0;

    ptr = root;
    if (!ptr)
	return (struct name_tree *)NULL;

    while (1) {
	int diff;

	diff = reg_id - ptr->region_id;

	if (diff == 0) {
	    *found = 1;
	    return ptr;
	} else if (diff > 0) {
	    if (ptr->rright)
		ptr = ptr->rright;
	    else
		return ptr;
	} else if (diff < 0) {
	    if (ptr->rleft)
		ptr = ptr->rleft;
	    else
		return ptr;
	}
    }
}


static void
List_names(struct f4g_state *s)
{
    struct name_tree *ptr;

    bu_ptbl_reset(s->stack);

    bu_log("\nNames in ident order:\n");
    ptr = name_root;
    while (1) {
	while (ptr) {
	    PUSH(ptr);
	    ptr = ptr->rleft;
	}
	POP(name_tree, ptr);
	if (!ptr)
	    break;

	if (ptr->in_comp_group)
	    bu_log("%s %d %d (in a comp group)\n", ptr->name, ptr->region_id, ptr->inner);
	else
	    bu_log("%s %d %d (not in a comp group)\n", ptr->name, ptr->region_id, ptr->inner);
	ptr = ptr->rright;
    }

    bu_log("\tAlphabetical list of names:\n");
    ptr = name_root;
    while (1) {
	while (ptr) {
	    PUSH(ptr);
	    ptr = ptr->nleft;
	}
	POP(name_tree, ptr);
	if (!ptr)
	    break;

	bu_log("%s %d %d\n", ptr->name, ptr->region_id, ptr->inner);
	ptr = ptr->nright;
    }
}


static void
Insert_region_name(struct f4g_state *s, const char *name, int reg_id)
{
    struct name_tree *nptr_model, *rptr_model;
    struct name_tree *new_ptr;
    int foundn, foundr;

    if (s->debug)
	bu_log("Insert_region_name(name=%s, reg_id=%d\n", name, reg_id);

    rptr_model = Search_ident(name_root, reg_id, &foundr);
    nptr_model = Search_names(name_root, name, &foundn);

    if (foundn && foundr)
	return;

    if (foundn != foundr) {
	bu_log("Insert_region_name: name %s ident %d\n\tfound name is %d\n\tfound ident is %d\n",
	       name, reg_id, foundn, foundr);
	List_names(s);
	bu_exit(1, "\tCannot insert new node\n");
    }

    /* Add to tree for entire model */
    BU_ALLOC(new_ptr, struct name_tree);
    new_ptr->rleft = (struct name_tree *)NULL;
    new_ptr->rright = (struct name_tree *)NULL;
    new_ptr->nleft = (struct name_tree *)NULL;
    new_ptr->nright = (struct name_tree *)NULL;
    new_ptr->region_id = reg_id;
    new_ptr->mode = s->mode;
    new_ptr->inner = -1;
    new_ptr->in_comp_group = 0;
    new_ptr->name = bu_strdup(name);
    new_ptr->magic = NAME_TREE_MAGIC;

    V_MAX(s->region_id_max, reg_id);

    if (!name_root) {
	name_root = new_ptr;
    } else {
	int diff;

	diff = bu_strcmp(name, nptr_model->name);

	if (diff > 0) {
	    if (nptr_model->nright) {
		bu_log("Insert_region_name: nptr_model->nright not null\n");
		bu_exit(1, "\tCannot insert new node\n");
	    }
	    nptr_model->nright = new_ptr;
	} else {
	    if (nptr_model->nleft) {
		bu_log("Insert_region_name: nptr_model->nleft not null\n");
		bu_exit(1, "\tCannot insert new node\n");
	    }
	    nptr_model->nleft = new_ptr;
	}


	diff = reg_id - rptr_model->region_id;

	if (diff > 0) {
	    if (rptr_model->rright) {
		bu_log("Insert_region_name: rptr_model->rright not null\n");
		bu_exit(1, "\tCannot insert new node\n");
	    }
	    rptr_model->rright = new_ptr;
	} else {
	    if (rptr_model->rleft) {
		bu_log("Insert_region_name: rptr_model->rleft not null\n");
		bu_exit(1, "\tCannot insert new node\n");
	    }
	    rptr_model->rleft = new_ptr;
	}
    }
    Check_names(s);
}


static char *
find_region_name(struct f4g_state *s, int g_id, int c_id)
{
    struct name_tree *ptr;
    int reg_id;
    int found;

    reg_id = g_id * 1000 + c_id;

    if (s->debug)
	bu_log("find_region_name(g_id=%d, c_id=%d), reg_id=%d\n", g_id, c_id, reg_id);

    ptr = Search_ident(name_root, reg_id, &found);

    if (found)
	return ptr->name;
    else
	return (char *)NULL;
}


static void
make_unique_name(struct f4g_state *s, struct bu_vls *name)
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int found;

    /* make a unique name from what we got off the $NAME card */

    (void)Search_names(name_root, bu_vls_cstr(name), &found);
    if (!found)
	return;

    while (found) {
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "%s_%d", bu_vls_cstr(name), s->name_count);
	(void)Search_names(name_root, bu_vls_addr(&vls), &found);
	s->name_count++;
    }

    bu_vls_sprintf(name, "%s", bu_vls_cstr(&vls));
    bu_vls_free(&vls);
}


static void
make_region_name(struct f4g_state *s,int g_id, int c_id)
{
    int r_id;
    const char *tmp_name;
    struct bu_vls name = BU_VLS_INIT_ZERO;

    r_id = g_id * 1000 + c_id;

    if (s->debug)
	bu_log("make_region_name(g_id=%d, c_id=%d)\n", g_id, c_id);

    tmp_name = find_region_name(s, g_id, c_id);
    if (tmp_name)
	return;

    /* create a new name */
    bu_vls_sprintf(&name, "comp_%04d.r", r_id);

    make_unique_name(s, &name);

    Insert_region_name(s, bu_vls_cstr(&name), r_id);
    bu_vls_free(&name);
}


static char *
get_solid_name(char type, int element_id, int c_id, int g_id, int inner)
{
    int reg_id;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    reg_id = g_id * 1000 + c_id;

    bu_vls_printf(&vls, "%d.%d.%c%d", reg_id, element_id, type, inner);

    return bu_vls_strgrab(&vls);
}


static void
Insert_name(struct f4g_state *s, struct name_tree **root, char *name, int inner)
{
    struct name_tree *ptr;
    struct name_tree *new_ptr;
    int found;
    int diff;

    ptr = Search_names(*root, name, &found);

    if (found) {
	bu_log("Insert_name: %s already in name tree\n", name);
	return;
    }

    BU_ALLOC(new_ptr, struct name_tree);

    new_ptr->name = bu_strdup(name);
    new_ptr->nleft = (struct name_tree *)NULL;
    new_ptr->nright = (struct name_tree *)NULL;
    new_ptr->rleft = (struct name_tree *)NULL;
    new_ptr->rright = (struct name_tree *)NULL;
    new_ptr->region_id = (-s->region_id);
    new_ptr->in_comp_group = 0;
    new_ptr->inner = inner;
    new_ptr->magic = NAME_TREE_MAGIC;

    if (!*root) {
	*root = new_ptr;
	return;
    }

    diff = bu_strcmp(name, ptr->name);
    if (diff > 0) {
	if (ptr->nright) {
	    bu_log("Insert_name: ptr->nright not null\n");
	    bu_exit(1, "\tCannot insert new node\n");
	}
	ptr->nright = new_ptr;
    } else {
	if (ptr->nleft) {
	    bu_log("Insert_name: ptr->nleft not null\n");
	    bu_exit(1, "\tCannot insert new node\n");
	}
	ptr->nleft = new_ptr;
    }
}


static char *
make_solid_name(struct f4g_state *s, char type, int element_id, int c_id, int g_id, int inner)
{
    char *name;

    name = get_solid_name(type, element_id, c_id, g_id, inner);

    Insert_name(s, &name_root, name, inner);

    return name;
}


static void
f4_do_compsplt(struct f4g_state *s)
{
    int gr, co, gr1,  co1;
    fastf_t z;
    struct compsplt *splt;

    bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
    gr = atoi(s->field);

    bu_strlcpy(s->field, &s->line[16], sizeof(s->field));
    co = atoi(s->field);

    bu_strlcpy(s->field, &s->line[24], sizeof(s->field));
    gr1 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[32], sizeof(s->field));
    co1 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[40], sizeof(s->field));
    z = atof(s->field) * 25.4;

    if (compsplt_root == NULL) {
	BU_ALLOC(compsplt_root, struct compsplt);
	splt = compsplt_root;
    } else {
	splt = compsplt_root;
	while (splt->next)
	    splt = splt->next;
	BU_ALLOC(splt->next, struct compsplt);
	splt = splt->next;
    }
    splt->next = (struct compsplt *)NULL;
    splt->ident_to_split = gr * 1000 + co;
    splt->new_ident = gr1 * 1000 + co1;
    splt->z = z;
    make_region_name(s, gr1, co1);
}


static void
List_holes(void)
{
    struct holes *hole_ptr;
    struct hole_list *list_ptr;

    hole_ptr = hole_root;

    while (hole_ptr) {
	bu_log("Holes for Group %d, Component %d:\n", hole_ptr->group, hole_ptr->component);
	list_ptr = hole_ptr->holes;
	while (list_ptr) {
	    bu_log("\tgroup %d component %d\n", list_ptr->group, list_ptr->component);
	    list_ptr = list_ptr->next;
	}
	hole_ptr = hole_ptr->next;
    }
}


static void
Add_stragglers_to_groups(struct f4g_state *s)
{
    struct name_tree *ptr;

    ptr = name_root;

    while (1) {
	while (ptr) {
	    PUSH(ptr);
	    ptr = ptr->rleft;
	}
	POP(name_tree, ptr);
	if (!ptr)
	    break;

	/* visit node */
	CK_TREE_MAGIC(ptr);

	/* FIXME: should not be manually recreating the wmember list
	 * when extending it.  just use a null-terminated list and
	 * realloc as needed...
	 */
	if (!ptr->in_comp_group && ptr->region_id > 0 && !is_a_hole(ptr->region_id)) {
	    /* add this component to a series */

	    if (!s->group_head || ptr->region_id > s->region_id_max) {
		struct wmember *new_head;
		ssize_t new_cnt, i;
		struct bu_list *list_first;

		new_cnt = lrint(ceil(s->region_id_max/1000.0));
		new_head = (struct wmember *)bu_calloc(new_cnt, sizeof(struct wmember), "group_head list");
		bu_log("ptr->region_id=%d region_id_max=%d new_cnt=%ld\n", ptr->region_id, s->region_id_max, new_cnt);

		for (i = 0 ; i < new_cnt ; i++) {
		    BU_LIST_INIT(&new_head[i].l);
		    if (i < s->group_head_cnt) {
			if (BU_LIST_NON_EMPTY(&s->group_head[i].l)) {
			    list_first = BU_LIST_FIRST(bu_list, &s->group_head[i].l);
			    BU_LIST_DEQUEUE(&s->group_head[i].l);
			    BU_LIST_INSERT(list_first, &new_head[i].l);
			}
		    }
		}
		if (s->group_head) {
		    bu_free(s->group_head, "old group_head");
		}
		s->group_head = new_head;
		s->group_head_cnt = new_cnt;
	    }

	    (void)mk_addmember(ptr->name, &s->group_head[ptr->region_id/1000].l, NULL, WMOP_UNION);
	    ptr->in_comp_group = 1;
	}

	ptr = ptr->rright;
    }
}


static void
f4_do_groups(struct f4g_state *s)
{
    int group_no;
    struct wmember head_all;

    if (s->debug)
	bu_log("f4_do_groups\n");

    BU_LIST_INIT(&head_all.l);

    Add_stragglers_to_groups(s);

    for (group_no=0; group_no < s->group_head_cnt; group_no++) {
	char name[MAX_LINE_SIZE] = {0};

	if (BU_LIST_IS_EMPTY(&s->group_head[group_no].l))
	    continue;

	snprintf(name, MAX_LINE_SIZE, "%dxxx_series", group_no);
	mk_lfcomb(s->fpout, name, &s->group_head[group_no], 0);

	if (mk_addmember(name, &head_all.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
	    bu_log("f4_do_groups: mk_addmember failed to add %s to group all\n", name);
    }

    if (BU_LIST_NON_EMPTY(&head_all.l))
	mk_lfcomb(s->fpout, "all", &head_all, 0);

    if (BU_LIST_NON_EMPTY(&s->hole_head.l))
	mk_lfcomb(s->fpout, "holes", &s->hole_head, 0);
}


static void
f4_do_name(struct f4g_state *s)
{
    int foundr = 0;
    int g_id;
    int c_id;
    struct bu_vls comp_name = BU_VLS_INIT_ZERO;

    if (s->pass)
	return;

    if (s->debug)
	bu_log("f4_do_name: %s\n", s->line);

    bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
    g_id = atoi(s->field);

    if (g_id != s->group_id) {
	bu_log("$NAME card for group %d in section for group %d ignored\n", g_id, s->group_id);
	bu_log("%s\n", s->line);
	return;
    }

    bu_strlcpy(s->field, &s->line[16], sizeof(s->field));
    c_id = atoi(s->field);

    if (c_id != s->comp_id) {
	bu_log("$NAME card for component %d in section for component %d ignored\n", c_id, s->comp_id);
	bu_log("%s\n", s->line);
	return;
    }

    /* Eliminate leading and trailing blanks */
    bu_vls_sprintf(&comp_name, "%s", &s->line[56]);
    bu_vls_trimspace(&comp_name);
    if (!bu_vls_strlen(&comp_name)) {
	bu_vls_free(&comp_name);
	return;
    }

    /* Simplify */
    bu_vls_simplify(&comp_name, NULL, NULL, NULL);

    /* reserve this name for group name */
    Search_ident(name_root, s->region_id, &foundr);
    if (!foundr) {
	make_unique_name(s, &comp_name);
    } else {
	bu_log("Already encountered region id %d - reusing name\n", s->region_id);
    }

    Insert_region_name(s, bu_vls_cstr(&comp_name), s->region_id);

    s->name_count = 0;

    bu_vls_free(&comp_name);
}


static void
f4_do_grid(struct f4g_state *s)
{
    int grid_no;
    fastf_t x, y, z;

    if (!s->pass)	/* not doing geometry yet */
	return;

    bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
    grid_no = atoi(s->field);

    if (grid_no < 1) {
	bu_exit(1, "ERROR: bad grid id number = %d\n", grid_no);
    }

    bu_strlcpy(s->field, &s->line[24], sizeof(s->field));
    x = atof(s->field);

    bu_strlcpy(s->field, &s->line[32], sizeof(s->field));
    y = atof(s->field);

    bu_strlcpy(s->field, &s->line[40], sizeof(s->field));
    z = atof(s->field);

    while (grid_no > s->grid_size - 1) {
	s->grid_size += GRID_BLOCK;
	s->grid_points = (point_t *)bu_realloc((char *)s->grid_points, s->grid_size * sizeof(point_t), "fast4-g: grid_points");
    }

    VSET(s->grid_points[grid_no], x*25.4, y*25.4, z*25.4);
    V_MAX(s->max_grid_no, grid_no);
}


static void
f4_do_sphere(struct f4g_state *s)
{
    int element_id;
    int center_pt;
    fastf_t thick;
    fastf_t radius;
    fastf_t inner_radius;
    char *name = (char *)NULL;
    struct wmember sphere_group;

    if (!s->pass) {
	make_region_name(s, s->group_id, s->comp_id);
	return;
    }

    bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
    element_id = atoi(s->field);

    bu_strlcpy(s->field, &s->line[24], sizeof(s->field));
    center_pt = atoi(s->field);

    bu_strlcpy(s->field, &s->line[56], sizeof(s->field));
    thick = atof(s->field) * 25.4;

    bu_strlcpy(s->field, &s->line[64], sizeof(s->field));
    radius = atof(s->field) * 25.4;

    if (radius <= 0.0) {
	bu_log("f4_do_sphere: illegal radius (%f), skipping sphere\n", radius);
	bu_log("\telement %d, component %d, group %d\n", element_id, s->comp_id, s->group_id);
	return;
    }

    if (center_pt < 1 || center_pt > s->max_grid_no) {
	bu_log("f4_do_sphere: illegal grid number for center point %d, skipping sphere\n", center_pt);
	bu_log("\telement %d, component %d, group %d\n", element_id, s->comp_id, s->group_id);
	return;
    }

    BU_LIST_INIT(&sphere_group.l);

    if (s->mode == VOLUME_MODE) {
	name = make_solid_name(s, CSPHERE, element_id, s->comp_id, s->group_id, 0);
	mk_sph(s->fpout, name, s->grid_points[center_pt], radius);
	bu_free(name, "solid_name");
    } else if (s->mode == PLATE_MODE) {
	name = make_solid_name(s, CSPHERE, element_id, s->comp_id, s->group_id, 1);
	mk_sph(s->fpout, name, s->grid_points[center_pt], radius);

	BU_LIST_INIT(&sphere_group.l);

	if (mk_addmember(name, &sphere_group.l, NULL, WMOP_UNION) == (struct wmember *)NULL) {
	    bu_exit(1, "f4_do_sphere: Error in adding %s to sphere group\n", name);
	}
	bu_free(name, "solid_name");

	inner_radius = radius - thick;
	if (thick > 0.0 && inner_radius <= 0.0) {
	    bu_log("f4_do_sphere: illegal thickness (%f), skipping inner sphere\n", thick);
	    bu_log("\telement %d, component %d, group %d\n", element_id, s->comp_id, s->group_id);
	    return;
	}

	name = make_solid_name(s, CSPHERE, element_id, s->comp_id, s->group_id, 2);
	mk_sph(s->fpout, name, s->grid_points[center_pt], inner_radius);

	if (mk_addmember(name, &sphere_group.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL) {
	    bu_exit(1, "f4_do_sphere: Error in subtracting %s from sphere region\n", name);
	}
	bu_free(name, "solid_name");

	name = make_solid_name(s, CSPHERE, element_id, s->comp_id, s->group_id, 0);
	mk_comb(s->fpout, name, &sphere_group.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 1, 1);
	bu_free(name, "solid_name");
    }
}


static void
f4_do_vehicle(struct f4g_state *s)
{
    if (s->pass)
	return;

    bu_strlcpy(s->vehicle, &s->line[8], sizeof(s->vehicle));
    mk_id_units(s->fpout, s->vehicle, "in");
}


static void
f4_do_cline(struct f4g_state *s)
{
    int element_id;
    int pt1, pt2;
    fastf_t thick;
    fastf_t radius;
    vect_t height;
    char *name;

    if (s->debug)
	bu_log("f4_do_cline: %s\n", s->line);

    if (!s->pass) {
	make_region_name(s, s->group_id, s->comp_id);
	return;
    }

    bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
    element_id = atoi(s->field);

    bu_strlcpy(s->field, &s->line[24], sizeof(s->field));
    pt1 = atoi(s->field);

    if (s->pass && (pt1 < 1 || pt1 > s->max_grid_no)) {
	bu_log("Illegal grid point (%d) in CLINE, skipping\n", pt1);
	bu_log("\telement %d, component %d, group %d\n", element_id, s->comp_id, s->group_id);
	return;
    }

    bu_strlcpy(s->field, &s->line[32], sizeof(s->field));
    pt2 = atoi(s->field);

    if (s->pass && (pt2 < 1 || pt2 > s->max_grid_no)) {
	bu_log("Illegal grid point in CLINE (%d), skipping\n", pt2);
	bu_log("\telement %d, component %d, group %d\n", element_id, s->comp_id, s->group_id);
	return;
    }

    if (pt1 == pt2) {
	bu_log("Illegal grid points in CLINE (%d and %d), skipping\n", pt1, pt2);
	bu_log("\telement %d, component %d, group %d\n", element_id, s->comp_id, s->group_id);
	return;
    }

    bu_strlcpy(s->field, &s->line[56], sizeof(s->field));
    thick = atof(s->field) * 25.4;

    bu_strlcpy(s->field, &s->line[64], sizeof(s->field));
    radius = atof(s->field) * 25.4;

    VSUB2(height, s->grid_points[pt2], s->grid_points[pt1]);

    name = make_solid_name(s, CLINE, element_id, s->comp_id, s->group_id, 0);
    mk_cline(s->fpout, name, s->grid_points[pt1], height, radius, thick);
    bu_free(name, "solid_name");
}


static void
f4_do_ccone1(struct f4g_state *s)
{
    int element_id;
    int pt1, pt2;
    fastf_t thick;
    int c1, c2;
    int end1, end2;
    vect_t height;
    fastf_t r1, r2;
    char *outer_name;
    char *inner_name;
    char *name = (char *)NULL;
    struct wmember r_head;

    bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
    element_id = atoi(s->field);

    if (!s->pass) {
	make_region_name(s, s->group_id, s->comp_id);
	if (!get_line(s)) {
	    bu_log("Unexpected EOF while reading continuation card for CCONE1\n");
	    bu_log("\ts->group_id = %d, comp_id = %d, element_id = %d\n",
		   s->group_id, s->comp_id, element_id);
	    bu_exit(1, "ERROR: unexpected end-of-file");
	}
	return;
    }

    bu_strlcpy(s->field, &s->line[24], sizeof(s->field));
    pt1 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[32], sizeof(s->field));
    pt2 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[56], sizeof(s->field));
    thick = atof(s->field) * 25.4;

    bu_strlcpy(s->field, &s->line[64], sizeof(s->field));
    r1 = atof(s->field) * 25.4;

    bu_strlcpy(s->field, &s->line[72], sizeof(s->field));
    c1 = atoi(s->field);

    if (!get_line(s)) {
	bu_log("Unexpected EOF while reading continuation card for CCONE1\n");
	bu_log("\ts->group_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
	       s->group_id, s->comp_id, element_id, c1);
	bu_exit(1, "ERROR: unexpected end-of-file");
    }

    bu_strlcpy(s->field, s->line, sizeof(s->field));
    c2 = atoi(s->field);

    if (c1 != c2) {
	bu_log("WARNING: CCONE1 continuation flags disagree, %d vs %d\n", c1, c2);
	bu_log("\ts->group_id = %d, s->comp_id = %d, element_id = %d\n",
	       s->group_id, s->comp_id, element_id);
    }

    bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
    r2 = atof(s->field) * 25.4;

    bu_strlcpy(s->field, &s->line[16], sizeof(s->field));
    end1 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[24], sizeof(s->field));
    end2 = atoi(s->field);

    if (r1 < 0.0 || r2 < 0.0) {
	bu_log("ERROR: CCONE1 has illegal radii, %f and %f\n", r1/25.4, r2/25.4);
	bu_log("\ts->group_id = %d, s->comp_id = %d, element_id = %d\n",
	       s->group_id, s->comp_id, element_id);
	bu_log("\tCCONE1 solid ignored\n");
	return;
    }

    if (s->mode == PLATE_MODE) {
	if (thick <= 0.0) {
	    bu_log("WARNING: Plate mode CCONE1 has illegal thickness (%f)\n", thick/25.4);
	    bu_log("\ts->group_id = %d, s->comp_id = %d, element_id = %d\n",
		   s->group_id, s->comp_id, element_id);
	    bu_log("\tCCONE1 solid plate mode overridden, now being treated as volume mode\n");
	    s->mode = VOLUME_MODE;
	}
    }

    if (s->mode == PLATE_MODE) {
	if (r1-thick < s->min_radius && r2-thick < s->min_radius) {
	    bu_log("ERROR: Plate s->mode CCONE1 has too large thickness (%f)\n", thick/25.4);
	    bu_log("\ts->group_id = %d, s->comp_id = %d, element_id = %d\n",
		   s->group_id, s->comp_id, element_id);
	    bu_log("\tCCONE1 solid ignored\n");
	    return;
	}
    }

    if (pt1 < 1 || pt1 > s->max_grid_no || pt2 < 1 || pt2 > s->max_grid_no || pt1 == pt2) {
	bu_log("ERROR: CCONE1 has illegal grid points (%d and %d)\n", pt1, pt2);
	bu_log("\ts->group_id = %d, s->comp_id = %d, element_id = %d\n",
	       s->group_id, s->comp_id, element_id);
	bu_log("\tCCONE1 solid ignored\n");
	return;
    }

    /* BRL_CAD doesn't allow zero radius, so use a very small radius */
    V_MAX(r1, s->min_radius);
    V_MAX(r2, s->min_radius);

    VSUB2(height, s->grid_points[pt2], s->grid_points[pt1]);

    if (s->mode == VOLUME_MODE) {
	outer_name = make_solid_name(s, CCONE1, element_id, s->comp_id, s->group_id, 0);
	mk_trc_h(s->fpout, outer_name, s->grid_points[pt1], height, r1, r2);
	bu_free(outer_name, "solid_name");
    } else if (s->mode == PLATE_MODE) {
	/* make inside TGC */

	point_t base;
	point_t top;
	vect_t inner_height;
	fastf_t inner_r1, inner_r2;
	fastf_t length;
	fastf_t sin_ang;
	fastf_t slant_len;
	vect_t height_dir;

	/* make outside TGC */
	outer_name = make_solid_name(s, CCONE1, element_id, s->comp_id, s->group_id, 1);
	mk_trc_h(s->fpout, outer_name, s->grid_points[pt1], height, r1, r2);

	BU_LIST_INIT(&r_head.l);
	if (mk_addmember(outer_name, &r_head.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
	    bu_exit(1, "CCONE1: mk_addmember failed\n");
	bu_free(outer_name, "solid_name");

	length = MAGNITUDE(height);
	VSCALE(height_dir, height, 1.0/length);
	slant_len = sqrt(length*length + (r2 - r1)*(r2 - r1));

	sin_ang = length/slant_len;

	if (end1 == END_OPEN) {
	    inner_r1 = r1 - thick/sin_ang;
	    VMOVE(base, s->grid_points[pt1]);
	} else {
	    fastf_t r1a = r1 + (r2 - r1)*thick/length;
	    inner_r1 = r1a - thick/sin_ang;
	    VJOIN1(base, s->grid_points[pt1], thick, height_dir);
	}

	if (inner_r1 < 0.0) {
	    fastf_t dist_to_new_base;

	    dist_to_new_base = inner_r1 * length/(r1 - r2);
	    inner_r1 = s->min_radius;
	    VJOIN1(base, base, dist_to_new_base, height_dir);
	} else {
	    V_MAX(inner_r1, s->min_radius);
	}

	if (end2 == END_OPEN) {
	    inner_r2 = r2 - thick/sin_ang;
	    VMOVE(top, s->grid_points[pt2]);
	} else {
	    fastf_t r2a = r2 + (r1 - r2)*thick/length;
	    inner_r2 = r2a - thick/sin_ang;
	    VJOIN1(top, s->grid_points[pt2], -thick, height_dir);
	}

	if (inner_r2 < 0.0) {
	    fastf_t dist_to_new_top;

	    dist_to_new_top = inner_r2 * length/(r2 - r1);
	    inner_r2 = s->min_radius;
	    VJOIN1(top, top, -dist_to_new_top, height_dir);
	} else {
	    V_MAX(inner_r2, s->min_radius);
	}

	VSUB2(inner_height, top, base);
	if (VDOT(inner_height, height) <= 0.0) {
	    bu_log("ERROR: CCONE1 height (%f) too small for thickness (%f)\n", length/25.4, thick/25.4);
	    bu_log("\ts->group_id = %d, s->comp_id = %d, element_id = %d\n",
		   s->group_id, s->comp_id, element_id);
	    bu_log("\tCCONE1 inner solid ignored\n");
	} else {
	    /* make inner tgc */

	    inner_name = make_solid_name(s, CCONE1, element_id, s->comp_id, s->group_id, 2);
	    mk_trc_h(s->fpout, inner_name, base, inner_height, inner_r1, inner_r2);

	    if (mk_addmember(inner_name, &r_head.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
		bu_exit(1, "CCONE1: mk_addmember failed\n");
	    bu_free(inner_name, "solid_name");
	}

	name = make_solid_name(s, CCONE1, element_id, s->comp_id, s->group_id, 0);
	mk_comb(s->fpout, name, &r_head.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 1, 1);
	bu_free(name, "solid_name");
    }

}


static void
f4_do_ccone2(struct f4g_state *s)
{
    int element_id;
    int pt1, pt2;
    int c1, c2;
    fastf_t ro1, ro2, ri1, ri2;
    vect_t height;
    char *name = (char *)NULL;
    struct wmember r_head;

    bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
    element_id = atoi(s->field);

    if (!s->pass) {
	make_region_name(s, s->group_id, s->comp_id);
	if (!get_line(s)) {
	    bu_log("Unexpected EOF while reading continuation card for CCONE2\n");
	    bu_log("\ts->group_id = %d, s->comp_id = %d, element_id = %d\n",
		   s->group_id, s->comp_id, element_id);
	    bu_exit(1, "ERROR: unexpected end-of-file encountered\n");
	}
	return;
    }

    bu_strlcpy(s->field, &s->line[24], sizeof(s->field));
    pt1 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[32], sizeof(s->field));
    pt2 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[64], sizeof(s->field));
    ro1 = atof(s->field) * 25.4;

    bu_strlcpy(s->field, &s->line[72], sizeof(s->field));
    c1 = atoi(s->field);

    if (!get_line(s)) {
	bu_log("Unexpected EOF while reading continuation card for CCONE2\n");
	bu_log("\ts->group_id = %d, s->comp_id = %d, element_id = %d, c1 = %d\n",
	       s->group_id, s->comp_id, element_id, c1);
	bu_exit(1, "ERROR: unexpected end-of-file encountered\n");
    }

    bu_strlcpy(s->field, s->line, sizeof(s->field));
    c2 = atoi(s->field);

    if (c1 != c2) {
	bu_log("WARNING: CCONE2 continuation flags disagree, %d vs %d\n", c1, c2);
	bu_log("\ts->group_id = %d, s->comp_id = %d, element_id = %d\n",
	       s->group_id, s->comp_id, element_id);
    }

    bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
    ro2 = atof(s->field) * 25.4;

    bu_strlcpy(s->field, &s->line[16], sizeof(s->field));
    ri1 = atof(s->field) * 25.4;

    bu_strlcpy(s->field, &s->line[24], sizeof(s->field));
    ri2 = atof(s->field) * 25.4;

    if (pt1 == pt2) {
	bu_log("ERROR: CCONE2 has same endpoints %d and %d\n", pt1, pt2);
	bu_log("\ts->group_id = %d, s->comp_id = %d, element_id = %d\n",
	       s->group_id, s->comp_id, element_id);
	return;
    }

    if (ro1 < 0.0 || ro2 < 0.0 || ri1 < 0.0 || ri2 < 0.0) {
	bu_log("ERROR: CCONE2 has illegal radii %f %f %f %f\n", ro1, ro2, ri1, ri2);
	bu_log("\ts->group_id = %d, s->comp_id = %d, element_id = %d\n",
	       s->group_id, s->comp_id, element_id);
	return;
    }

    V_MAX(ro1, s->min_radius);
    V_MAX(ro2, s->min_radius);

    BU_LIST_INIT(&r_head.l);

    VSUB2(height, s->grid_points[pt2], s->grid_points[pt1]);

    if (ri1 <= 0.0 && ri2 <= 0.0) {
	name = make_solid_name(s, CCONE2, element_id, s->comp_id, s->group_id, 0);
	mk_trc_h(s->fpout, name, s->grid_points[pt1], height, ro1, ro2);
	bu_free(name, "solid_name");
    } else {
	name = make_solid_name(s, CCONE2, element_id, s->comp_id, s->group_id, 1);
	mk_trc_h(s->fpout, name, s->grid_points[pt1], height, ro1, ro2);

	if (mk_addmember(name, &r_head.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
	    bu_exit(1, "mk_addmember failed!\n");
	bu_free(name, "solid_name");

	V_MAX(ri1, s->min_radius);
	V_MAX(ri2, s->min_radius);

	name = make_solid_name(s, CCONE2, element_id, s->comp_id, s->group_id, 2);
	mk_trc_h(s->fpout, name, s->grid_points[pt1], height, ri1, ri2);

	if (mk_addmember(name, &r_head.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
	    bu_exit(1, "mk_addmember failed!\n");
	bu_free(name, "solid_name");

	name = make_solid_name(s, CCONE2, element_id, s->comp_id, s->group_id, 0);
	mk_comb(s->fpout, name, &r_head.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 1, 1);
	bu_free(name, "solid_name");
    }
}


static void
f4_do_ccone3(struct f4g_state *s)
{
    int element_id;
    int pt1, pt2, pt3, pt4, i;
    char *name;
    fastf_t ro[4], ri[4], len03, len01, len12, len23;
    vect_t diff, diff2, diff3, diff4;
    struct wmember r_head;

    bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
    element_id = atoi(s->field);

    if (!s->pass) {
	make_region_name(s, s->group_id, s->comp_id);
	if (!get_line(s)) {
	    bu_log("Unexpected EOF while reading continuation card for CCONE3\n");
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   s->group_id, s->comp_id, element_id);
	    bu_exit(1, "ERROR: unexpected end-of-file encountered\n");
	}
	return;
    }

    bu_strlcpy(s->field, &s->line[24], sizeof(s->field));
    pt1 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[32], sizeof(s->field));
    pt2 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[40], sizeof(s->field));
    pt3 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[48], sizeof(s->field));
    pt4 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[72], sizeof(s->field));

    if (!get_line(s)) {
	bu_log("Unexpected EOF while reading continuation card for CCONE3\n");
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %8.8s\n",
	       s->group_id, s->comp_id, element_id, s->field);
	bu_exit(1, "ERROR: unexpected end-of-file encountered\n");
    }

    if (bu_strncmp(s->field, s->line, 8)) {
	bu_log("WARNING: CCONE3 continuation flags disagree, %8.8s vs %8.8s\n", s->field, s->line);
	bu_log("\ts->group_id = %d, comp_id = %d, element_id = %d\n",
	       s->group_id, s->comp_id, element_id);
    }

    for (i=0; i<4; i++) {
	bu_strlcpy(s->field, &s->line[8*(i+1)], sizeof(s->field));
	ro[i] = atof(s->field) * 25.4;
	if (ro[i] < 0.0) {
	    bu_log("ERROR: CCONE3 has illegal radius %f\n", ro[i]);
	    bu_log("\ts->group_id = %d, comp_id = %d, element_id = %d\n",
		   s->group_id, s->comp_id, element_id);
	    return;
	}
	if (BU_STR_EQUAL(s->field, "        "))
	    ro[i] = -1.0;
    }

    for (i=0; i<4; i++) {
	bu_strlcpy(s->field, &s->line[32 + 8*(i+1)], sizeof(s->field));
	ri[i] = atof(s->field) * 25.4;
	if (ri[i] < 0.0) {
	    bu_log("ERROR: CCONE3 has illegal radius %f\n", ri[i]);
	    bu_log("\ts->group_id = %d, comp_id = %d, element_id = %d\n",
		   s->group_id, s->comp_id, element_id);
	    return;
	}
	if (BU_STR_EQUAL(s->field, "        "))
	    ri[i] = -1.0;
    }

    VSUB2(diff4, s->grid_points[pt4], s->grid_points[pt1]);
    VSUB2(diff3, s->grid_points[pt3], s->grid_points[pt1]);
    VSUB2(diff2, s->grid_points[pt2], s->grid_points[pt1]);

    len03 = MAGNITUDE(diff4);
    len01 = MAGNITUDE(diff2);
    len12 = MAGNITUDE(diff3) - len01;
    len23 = len03 - len01 - len12;

    for (i=0; i<4; i+=3) {
	if (EQUAL(ro[i], -1.0)) {
	    if (EQUAL(ri[i], -1.0)) {
		bu_log("ERROR: both inner and outer radii at g%d of a CCONE3 are undefined\n", i+1);
		bu_log("\ts->group_id = %d, comp_id = %d, element_id = %d\n",
		       s->group_id, s->comp_id, element_id);
		return;
	    } else {
		ro[i] = ri[i];
	    }

	} else if (EQUAL(ri[i], -1.0)) {
	    ri[i] = ro[i];
	}
    }

    if (EQUAL(ro[1], -1.0)) {
	if (!EQUAL(ro[2], -1.0))
	    ro[1] = ro[0] + (ro[2] - ro[0]) * len01 / (len01 + len12);
	else
	    ro[1] = ro[0] + (ro[3] - ro[0]) * len01 / len03;
    }
    if (EQUAL(ro[2], -1.0)) {
	if (!EQUAL(ro[1], -1.0))
	    ro[2] = ro[1] + (ro[3] - ro[1]) * len12 / (len12 + len23);
	else
	    ro[2] = ro[0] + (ro[3] - ro[0]) * (len01 + len12) / len03;
    }
    if (EQUAL(ri[1], -1.0)) {
	if (!EQUAL(ri[2], -1.0))
	    ri[1] = ri[0] + (ri[2] - ri[0]) * len01 / (len01 + len12);
	else
	    ri[1] = ri[0] + (ri[3] - ri[0]) * len01 / len03;
    }
    if (EQUAL(ri[2], -1.0)) {
	if (!EQUAL(ri[1], -1.0))
	    ri[2] = ri[1] + (ri[3] - ri[1]) * len12 / (len12 + len23);
	else
	    ri[2] = ri[0] + (ri[3] - ri[0]) * (len01 + len12) / len03;
    }

    for (i=0; i<4; i++) {
	V_MAX(ro[i], s->min_radius);
	V_MAX(ri[i], s->min_radius);
    }

    BU_LIST_INIT(&r_head.l);

    if (pt1 != pt2) {
	VSUB2(diff, s->grid_points[pt2], s->grid_points[pt1]);

	/* make first cone */
	if (!EQUAL(ro[0], s->min_radius) || !EQUAL(ro[1], s->min_radius)) {
	    name = make_solid_name(s, CCONE3, element_id, s->comp_id, s->group_id, 1);
	    mk_trc_h(s->fpout, name, s->grid_points[pt1], diff, ro[0], ro[1]);
	    if (mk_addmember(name, &r_head.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
		bu_exit(1, "mk_addmember failed!\n");
	    bu_free(name, "solid_name");

	    /* and the inner cone */
	    if (!EQUAL(ri[0], s->min_radius) || !EQUAL(ri[1], s->min_radius)) {
		name = make_solid_name(s, CCONE3, element_id, s->comp_id, s->group_id, 11);
		mk_trc_h(s->fpout, name, s->grid_points[pt1], diff, ri[0], ri[1]);
		if (mk_addmember(name, &r_head.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
		    bu_exit(1, "mk_addmember failed!\n");
		bu_free(name, "solid_name");
	    }
	}
    }

    if (pt2 != pt3) {
	VSUB2(diff, s->grid_points[pt3], s->grid_points[pt2]);

	/* make second cone */
	if (!EQUAL(ro[1], s->min_radius) || !EQUAL(ro[2], s->min_radius)) {
	    name = make_solid_name(s, CCONE3, element_id, s->comp_id, s->group_id, 2);
	    mk_trc_h(s->fpout, name, s->grid_points[pt2], diff, ro[1], ro[2]);
	    if (mk_addmember(name, &r_head.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
		bu_exit(1, "mk_addmember failed!\n");
	    bu_free(name, "solid_name");

	    /* and the inner cone */
	    if (!EQUAL(ri[1], s->min_radius) || !EQUAL(ri[2], s->min_radius)) {
		name = make_solid_name(s, CCONE3, element_id, s->comp_id, s->group_id, 22);
		mk_trc_h(s->fpout, name, s->grid_points[pt2], diff, ri[1], ri[2]);
		if (mk_addmember(name, &r_head.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
		    bu_exit(1, "mk_addmember failed!\n");
		bu_free(name, "solid_name");
	    }
	}
    }

    if (pt3 != pt4) {
	VSUB2(diff, s->grid_points[pt4], s->grid_points[pt3]);

	/* make third cone */
	if (!EQUAL(ro[2], s->min_radius) || !EQUAL(ro[3], s->min_radius)) {
	    name = make_solid_name(s, CCONE3, element_id, s->comp_id, s->group_id, 3);
	    mk_trc_h(s->fpout, name, s->grid_points[pt3], diff, ro[2], ro[3]);
	    if (mk_addmember(name, &r_head.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
		bu_exit(1, "mk_addmember failed!\n");
	    bu_free(name, "solid_name");

	    /* and the inner cone */
	    if (!EQUAL(ri[2], s->min_radius) || !EQUAL(ri[3], s->min_radius)) {
		name = make_solid_name(s, CCONE3, element_id, s->comp_id, s->group_id, 33);
		mk_trc_h(s->fpout, name, s->grid_points[pt3], diff, ri[2], ri[3]);
		if (mk_addmember(name, &r_head.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
		    bu_exit(1, "mk_addmember failed!\n");
		bu_free(name, "solid_name");
	    }
	}
    }

    name = make_solid_name(s, CCONE3, element_id, s->comp_id, s->group_id, 0);
    mk_comb(s->fpout, name, &r_head.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 1, 1);
    bu_free(name, "solid_name");
}


static int
skip_region(struct f4g_state *s, int id)
{
    int i;

    if (!s->f4_do_skips)
	return 0;

    for (i=0; i<s->f4_do_skips; i++) {
	if (id == s->region_list[i])
	    return 0;
    }

    return 1;
}


static void
Add_holes(struct f4g_state *s, int type, int gr, int comp, struct hole_list *ptr)
{
    struct holes *hole_ptr = (struct holes *)NULL;
    struct holes *prev = (struct holes *)NULL;
    struct hole_list *hptr= (struct hole_list *)NULL;

    if (s->debug) {
	bu_log("Adding holes for group %d, component %d:\n", gr, comp);
	hptr = ptr;
	while (hptr) {
	    bu_log("\t%d %d\n", hptr->group, hptr->component);
	    hptr = hptr->next;
	}
    }

    if (s->f4_do_skips) {
	if (!skip_region(s, gr*1000 + comp)) {
	    /* add holes for this region to the list of regions to process */
	    hptr = ptr;
	    while (hptr) {
		if (s->f4_do_skips == s->region_list_len) {
		    s->region_list_len += REGION_LIST_BLOCK;
		    s->region_list = (int *)bu_realloc((char *)s->region_list, s->region_list_len*sizeof(int), "region_list");
		}
		s->region_list[s->f4_do_skips++] = 1000*hptr->group + hptr->component;
		hptr = hptr->next;
	    }
	}
    }

    if (!hole_root) {
	BU_ALLOC(hole_root, struct holes);
	hole_root->group = gr;
	hole_root->component = comp;
	hole_root->type = type;
	hole_root->holes = ptr;
	hole_root->next = (struct holes *)NULL;
	return;
    }

    hole_ptr = hole_root;
    prev = hole_root;
    while (hole_ptr) {
	if (hole_ptr->group == gr &&
	    hole_ptr->component == comp &&
	    hole_ptr->type == type)
	    break;
	prev = hole_ptr;
	hole_ptr = hole_ptr->next;
    }

    if (hole_ptr && hole_ptr->group == gr && hole_ptr->component == comp && hole_ptr->type == type) {
	struct hole_list *list;

	if (!hole_ptr->holes) {
	    hole_ptr->holes = ptr;
	} else {
	    list = hole_ptr->holes;
	    while (list->next)
		list = list->next;
	    list->next = ptr;
	}
    } else {
	BU_ALLOC(prev->next, struct holes);
	hole_ptr = prev->next;
	hole_ptr->group = gr;
	hole_ptr->component = comp;
	hole_ptr->type = type;
	hole_ptr->holes = ptr;
	hole_ptr->next = (struct holes *)NULL;
    }
}


static void
f4_do_hole_wall(struct f4g_state *s, int type)
{
    struct hole_list *list_ptr;
    struct hole_list *list_start;
    int group, comp;
    int igrp, icmp;
    size_t s_len;
    size_t col;

    if (s->debug)
	bu_log("f4_do_hole_wall: %s\n", s->line);

    if (s->pass)
	return;

    if (type != HOLE && type != WALL) {
	bu_exit(1, "f4_do_hole_wall: unrecognized type (%d)\n", type);
    }

    /* eliminate trailing blanks */
    s_len = strlen(s->line);
    while (isspace((int)s->line[--s_len]))
	s->line[s_len] = '\0';

    s_len = strlen(s->line);
    V_MIN(s_len, 80);

    bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
    group = atoi(s->field);

    bu_strlcpy(s->field, &s->line[16], sizeof(s->field));
    comp = atoi(s->field);

    list_start = (struct hole_list *)NULL;
    list_ptr = (struct hole_list *)NULL;
    col = 24;

    while (col < s_len) {
	bu_strlcpy(s->field, &s->line[col], sizeof(s->field));
	igrp = atoi(s->field);

	col += 8;
	if (col >= s_len)
	    break;

	bu_strlcpy(s->field, &s->line[col], sizeof(s->field));
	icmp = atoi(s->field);

	if (igrp >= 0 && icmp > 0) {
	    if (igrp == group && comp == icmp) {
		bu_log("Hole or wall card references itself (ignoring): (%s)\n", s->line);
	    } else {
		if (list_ptr) {
		    BU_ALLOC(list_ptr->next, struct hole_list);
		    list_ptr = list_ptr->next;
		} else {
		    BU_ALLOC(list_ptr, struct hole_list);
		    list_start = list_ptr;
		}

		list_ptr->group = igrp;
		list_ptr->component = icmp;
		list_ptr->next = (struct hole_list *)NULL;
	    }
	}

	col += 8;
    }

    Add_holes(s, type, group, comp, list_start);
}


static void
f4_Add_bot_face(struct f4g_state *s, int pt1, int pt2, int pt3, fastf_t thick, int pos)
{

    if (pt1 == pt2 || pt2 == pt3 || pt1 == pt3) {
	bu_log("f4_Add_bot_face: ignoring degenerate triangle in group %d component %d\n", s->group_id, s->comp_id);
	return;
    }

    if (pos == 0)	/* use default */
	pos = POS_FRONT;

    if (s->mode == PLATE_MODE) {
	if (pos != POS_CENTER && pos != POS_FRONT) {
	    bu_log("f4_Add_bot_face: illegal position parameter (%d), must be one or two (ignoring face for group %d component %d)\n", pos, s->group_id, s->comp_id);
	    return;
	}
    }

    if (s->face_count >= s->face_size) {
	s->face_size += GRID_BLOCK;
	s->faces = (int *)bu_realloc((void *)s->faces,  s->face_size*3*sizeof(int), "faces");
	s->thickness = (fastf_t *)bu_realloc((void *)s->thickness, s->face_size*sizeof(fastf_t), "thickness");
	s->facemode = (char *)bu_realloc((void *)s->facemode, s->face_size*sizeof(char), "facemode");
    }
    if (!s->faces || !s->thickness || !s->facemode)
	bu_exit(BRLCAD_ERROR, "fast4-g memory allocation error, fast4-g.c:%d\n", __LINE__);

    s->faces[s->face_count*3] = pt1;
    s->faces[s->face_count*3+1] = pt2;
    s->faces[s->face_count*3+2] = pt3;

    if (s->mode == PLATE_MODE) {
	s->thickness[s->face_count] = thick;
	s->facemode[s->face_count] = pos;
    } else {
	s->thickness[s->face_count] = 0.0;
	s->facemode[s->face_count] = 0;
    }

    s->face_count++;
}


static void
f4_do_tri(struct f4g_state *s)
{
    int element_id;
    int pt1, pt2, pt3;
    fastf_t thick;
    int pos;

    if (s->debug)
	bu_log("f4_do_tri: %s\n", s->line);

    bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
    element_id = atoi(s->field);

    if (!s->bot)
	s->bot = element_id;

    if (!s->pass)
	return;

    if (s->faces == NULL) {
	s->faces = (int *)bu_calloc(GRID_BLOCK*3, sizeof(int), "faces");
	s->thickness = (fastf_t *)bu_calloc(GRID_BLOCK, sizeof(fastf_t), "thickness");
	s->facemode = (char *)bu_calloc(GRID_BLOCK, sizeof(char), "facemode");
	s->face_size = GRID_BLOCK;
	s->face_count = 0;
    }

    bu_strlcpy(s->field, &s->line[24], sizeof(s->field));
    pt1 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[32], sizeof(s->field));
    pt2 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[40], sizeof(s->field));
    pt3 = atoi(s->field);

    thick = 0.0;
    pos = 0;

    if (s->mode == PLATE_MODE) {
	bu_strlcpy(s->field, &s->line[56], sizeof(s->field));
	thick = atof(s->field) * 25.4;

	bu_strlcpy(s->field, &s->line[64], sizeof(s->field));
	pos = atoi(s->field);
	if (pos == 0)
	    pos = POS_FRONT;

	if (s->debug)
	    bu_log("\tplate mode: thickness = %f\n", thick);

    }

    if (s->f4_do_plot)
	plot_tri(s, pt1, pt2, pt3);

    f4_Add_bot_face(s, pt1, pt2, pt3, thick, pos);
}


static void
f4_do_quad(struct f4g_state *s)
{
    int element_id;
    int pt1, pt2, pt3, pt4;
    fastf_t thick = 0.0;
    int pos = 0;

    bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
    element_id = atoi(s->field);

    if (s->debug)
	bu_log("f4_do_quad: %s\n", s->line);

    if (!s->bot)
	s->bot = element_id;

    if (!s->pass)
	return;

    if (s->faces == NULL) {
	s->faces = (int *)bu_calloc(GRID_BLOCK*3,sizeof(int), "faces");
	s->thickness = (fastf_t *)bu_calloc(GRID_BLOCK,sizeof(fastf_t), "thickness");
	s->facemode = (char *)bu_calloc(GRID_BLOCK,sizeof(char), "facemode");
	s->face_size = GRID_BLOCK;
	s->face_count = 0;
    }

    bu_strlcpy(s->field, &s->line[24], sizeof(s->field));
    pt1 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[32], sizeof(s->field));
    pt2 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[40], sizeof(s->field));
    pt3 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[48], sizeof(s->field));
    pt4 = atoi(s->field);

    if (s->mode == PLATE_MODE) {
	bu_strlcpy(s->field, &s->line[56], sizeof(s->field));
	thick = atof(s->field) * 25.4;

	bu_strlcpy(s->field, &s->line[64], sizeof(s->field));
	pos = atoi(s->field);

	if (pos == 0)	/* use default */
	    pos = POS_FRONT;

	if (pos != POS_CENTER && pos != POS_FRONT) {
	    bu_log("f4_do_quad: illegal position parameter (%d), must be one or two\n", pos);
	    bu_log("\telement %d, component %d, group %d\n", element_id, s->comp_id, s->group_id);
	    return;
	}
    }

    f4_Add_bot_face(s, pt1, pt2, pt3, thick, pos);
    f4_Add_bot_face(s, pt1, pt3, pt4, thick, pos);
}


static void
make_bot_object(struct f4g_state *s)
{
    int i;
    int max_pt = 0;
    int min_pt = 999999;
    int num_vertices;
    struct bu_bitv *bv = (struct bu_bitv *)NULL;
    int bot_mode;
    char *name = (char *)NULL;
    int element_id = s->bot;
    int count;
    struct rt_bot_internal bot_ip;

    if (!s->pass) {
	make_region_name(s, s->group_id, s->comp_id);
	return;
    }

    bot_ip.magic = RT_BOT_INTERNAL_MAGIC;
    for (i=0; i<s->face_count; i++) {
	V_MIN(min_pt, s->faces[i*3]);
	V_MAX(max_pt, s->faces[i*3]);
	V_MIN(min_pt, s->faces[i*3+1]);
	V_MAX(max_pt, s->faces[i*3+1]);
	V_MIN(min_pt, s->faces[i*3+2]);
	V_MAX(max_pt, s->faces[i*3+2]);
    }

    num_vertices = max_pt - min_pt + 1;
    bot_ip.num_vertices = num_vertices;
    bot_ip.vertices = (fastf_t *)bu_calloc(num_vertices*3, sizeof(fastf_t), "BOT vertices");
    for (i=0; i<num_vertices; i++)
	VMOVE(&bot_ip.vertices[i*3], s->grid_points[min_pt+i]);

    for (i=0; i<s->face_count*3; i++)
	s->faces[i] -= min_pt;
    bot_ip.num_faces = s->face_count;
    bot_ip.faces = (int *)bu_calloc(s->face_count*3, sizeof(int), "BOT faces");
    for (i=0; i<s->face_count*3; i++)
	bot_ip.faces[i] = s->faces[i];

    bot_ip.face_mode = (struct bu_bitv *)NULL;
    bot_ip.thickness = (fastf_t *)NULL;
    if (s->mode == PLATE_MODE) {
	bot_mode = RT_BOT_PLATE;
	bv = bu_bitv_new(s->face_count);
	for (i=0; i<s->face_count; i++) {
	    if (s->facemode[i] == POS_FRONT)
		BU_BITSET(bv, i);
	}
	bot_ip.face_mode = bv;
	bot_ip.thickness = (fastf_t *)bu_calloc(s->face_count, sizeof(fastf_t), "BOT thickness");
	for (i=0; i<s->face_count; i++)
	    bot_ip.thickness[i] = s->thickness[i];
    } else {
	bot_mode = RT_BOT_SOLID;
    }

    bot_ip.mode = bot_mode;
    bot_ip.orientation = RT_BOT_UNORIENTED;
    bot_ip.bot_flags = 0;

    count = rt_bot_vertex_fuse(&bot_ip, &s->fpout->wdb_tol);
    if (count)
	bu_log("WARNING: %d duplicate vertices eliminated from group %d component %d\n", count, s->group_id, s->comp_id);

    count = rt_bot_face_fuse(&bot_ip);
    if (count)
	bu_log("WARNING: %d duplicate faces eliminated from group %d component %d\n", count, s->group_id, s->comp_id);

    name = make_solid_name(s, BOT, element_id, s->comp_id, s->group_id, 0);
    mk_bot(s->fpout, name, bot_mode, RT_BOT_UNORIENTED, 0, bot_ip.num_vertices, bot_ip.num_faces, bot_ip.vertices,
	   bot_ip.faces, bot_ip.thickness, bot_ip.face_mode);
    bu_free(name, "solid_name");

    if (s->mode == PLATE_MODE) {
	bu_free((char *)bot_ip.thickness, "BOT thickness");
	bu_free((char *)bot_ip.face_mode, "BOT face_mode");
    }
    bu_free((char *)bot_ip.vertices, "BOT vertices");
    bu_free((char *)bot_ip.faces, "BOT faces");

}


static void
skip_section(struct f4g_state *s)
{
    b_off_t section_start;

    /* skip to start of next section */
    section_start = bu_ftell(s->fpin);
    if (section_start < 0) {
	bu_exit(1, "Error: couldn't get input file's current file position.\n");
    }

    if (get_line(s)) {
	while (s->line[0] && bu_strncmp(s->line, "SECTION", 7) &&
	       bu_strncmp(s->line, "HOLE", 4) &&
	       bu_strncmp(s->line, "WALL", 4) &&
	       bu_strncmp(s->line, "VEHICLE", 7))
	{
	    section_start = bu_ftell(s->fpin);
	    if (section_start < 0) {
		bu_exit(1, "Error: couldn't get input file's current file position.\n");
	    }
	    if (!get_line(s))
		break;
	}
    }
    /* seek to start of the section */
    bu_fseek(s->fpin, section_start, SEEK_SET);
}


/* cleanup from previous component and start a new one.
 * This is called with final == 1 when ENDDATA is found
 */
static void
f4_do_section(struct f4g_state *s, int final)
{
    int found;
    struct name_tree *nm_ptr;

    if (s->debug)
	bu_log("f4_do_section(%d): %s\n", final, s->line);

    if (s->pass)	/* doing geometry */ {
	if (s->region_id && !skip_region(s, s->region_id)) {
	    s->comp_count++;

	    if (s->bot)
		make_bot_object(s);
	}
	if (final && s->debug) /* The ENDATA card has been found */
	    List_names(s);
    } else if (s->bot) {
	make_region_name(s, s->group_id, s->comp_id);
    }

    if (!final) {
	bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
	s->group_id = atoi(s->field);

	bu_strlcpy(s->field, &s->line[16], sizeof(s->field));
	s->comp_id = atoi(s->field);

	s->region_id = s->group_id * 1000 + s->comp_id;

	if (skip_region(s, s->region_id)) /* do not process this component */ {
	    skip_section(s);
	    return;
	}

	if (s->comp_id > 999) {
	    bu_log("Illegal component id number %d, changed to 999\n", s->comp_id);
	    s->comp_id = 999;
	}

	bu_strlcpy(s->field, &s->line[24], sizeof(s->field));
	s->mode = atoi(s->field);
	if (s->mode != 1 && s->mode != 2) {
	    bu_log("Illegal mode (%d) for group %d component %d, using volume mode\n",
		   s->mode, s->group_id, s->comp_id);
	    s->mode = 2;
	}

	if (!s->pass) {
	    nm_ptr = Search_ident(name_root, s->region_id, &found);
	    if (found && nm_ptr->mode != s->mode) {
		bu_log("ERROR: second SECTION card found with different mode for component (group=%d, component=%d), conversion of this component will be incorrect!\n",
		       s->group_id, s->comp_id);
	    }
	}
    }

    s->bot = 0;
    s->face_count = 0;
}


static void
f4_do_hex1(struct f4g_state *s)
{
    fastf_t thick=0.0;
    int pos;
    int pts[8];
    int element_id;
    int i;
    int cont1, cont2;

    bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
    element_id = atoi(s->field);

    if (!s->bot)
	s->bot = element_id;

    if (!s->pass) {
	if (!get_line(s)) {
	    bu_log("Unexpected EOF while reading continuation card for CHEX1\n");
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   s->group_id, s->comp_id, element_id);
	    bu_exit(1, "ERROR: unexpected end-of-file encountered\n");
	}
	return;
    }

    if (s->faces == NULL) {
	s->faces = (int *)bu_calloc(GRID_BLOCK*3, sizeof(int), "faces");
	s->thickness = (fastf_t *)bu_calloc(GRID_BLOCK, sizeof(fastf_t), "thickness");
	s->facemode = (char *)bu_calloc(GRID_BLOCK, sizeof(char), "facemode");
	s->face_size = GRID_BLOCK;
	s->face_count = 0;
    }

    for (i=0; i<6; i++) {
	bu_strlcpy(s->field, &s->line[24 + i*8], sizeof(s->field));
	pts[i] = atoi(s->field);
    }

    bu_strlcpy(s->field, &s->line[72], sizeof(s->field));
    cont1 = atoi(s->field);

    if (!get_line(s)) {
	bu_log("Unexpected EOF while reading continuation card for CHEX1\n");
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
	       s->group_id, s->comp_id, element_id, cont1);
	bu_exit(1, "ERROR: unexpected end-of-file encountered\n");
    }

    bu_strlcpy(s->field, s->line, sizeof(s->field));
    cont2 = atoi(s->field);

    if (cont1 != cont2) {
	bu_log("Continuation card numbers do not match for CHEX1 element (%d vs %d)\n", cont1, cont2);
	bu_log("\tskipping CHEX1 element: group_id = %d, comp_id = %d, element_id = %d\n",
	       s->group_id, s->comp_id, element_id);
	return;
    }

    bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
    pts[6] = atoi(s->field);

    bu_strlcpy(s->field, &s->line[16], sizeof(s->field));
    pts[7] = atoi(s->field);

    if (s->mode == PLATE_MODE) {
	bu_strlcpy(s->field, &s->line[56], sizeof(s->field));
	thick = atof(s->field) * 25.4;
	if (thick <= 0.0) {
	    bu_log("f4_do_hex1: illegal thickness (%f), skipping CHEX1 element\n", thick);
	    bu_log("\telement %d, component %d, group %d\n", element_id, s->comp_id, s->group_id);
	    return;
	}

	bu_strlcpy(s->field, &s->line[64], sizeof(s->field));
	pos = atoi(s->field);

	if (pos == 0)	/* use default */
	    pos = POS_FRONT;

	if (pos != POS_CENTER && pos != POS_FRONT) {
	    bu_log("f4_do_hex1: illegal position parameter (%d), must be 1 or 2, skipping CHEX1 element\n", pos);
	    bu_log("\telement %d, component %d, group %d\n", element_id, s->comp_id, s->group_id);
	    return;
	}
    } else {
	pos =  POS_FRONT;
	thick = 0.0;
    }

    for (i=0; i<12; i++)
	f4_Add_bot_face(s, pts[hex_faces[i][0]], pts[hex_faces[i][1]], pts[hex_faces[i][2]], thick, pos);
}


static void
f4_do_hex2(struct f4g_state *s)
{
    int pts[8];
    int element_id;
    int i;
    int cont1, cont2;
    point_t points[8];
    char *name = (char *)NULL;

    bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
    element_id = atoi(s->field);

    if (!s->pass) {
	make_region_name(s, s->group_id, s->comp_id);
	if (!get_line(s)) {
	    bu_log("Unexpected EOF while reading continuation card for CHEX2\n");
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   s->group_id, s->comp_id, element_id);
	    bu_exit(1, "ERROR: unexpected end-of-file encountered\n");
	}
	return;
    }

    for (i=0; i<6; i++) {
	bu_strlcpy(s->field, &s->line[24 + i*8], sizeof(s->field));
	pts[i] = atoi(s->field);
    }

    bu_strlcpy(s->field, &s->line[72], sizeof(s->field));
    cont1 = atoi(s->field);

    if (!get_line(s)) {
	bu_log("Unexpected EOF while reading continuation card for CHEX2\n");
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
	       s->group_id, s->comp_id, element_id, cont1);
	bu_exit(1, "ERROR: unexpected end-of-file encountered\n");
    }

    bu_strlcpy(s->field, s->line, sizeof(s->field));
    cont2 = atoi(s->field);

    if (cont1 != cont2) {
	bu_log("Continuation card numbers do not match for CHEX2 element (%d vs %d)\n", cont1, cont2);
	bu_log("\tskipping CHEX2 element: group_id = %d, comp_id = %d, element_id = %d\n",
	       s->group_id, s->comp_id, element_id);
	return;
    }

    bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
    pts[6] = atoi(s->field);

    bu_strlcpy(s->field, &s->line[16], sizeof(s->field));
    pts[7] = atoi(s->field);

    for (i=0; i<8; i++)
	VMOVE(points[i], s->grid_points[pts[i]]);

    name = make_solid_name(s, CHEX2, element_id, s->comp_id, s->group_id, 0);
    mk_arb8(s->fpout, name, &points[0][X]);
    bu_free(name, "solid_name");

}


static void
Process_hole_wall(struct f4g_state *s)
{
    if (s->debug)
	bu_log("Process_hole_wall\n");

    rewind(s->fpin);
    while (1) {
	if (!bu_strncmp(s->line, "HOLE", 4)) {
	    f4_do_hole_wall(s, HOLE);
	} else if (!bu_strncmp(s->line, "WALL", 4)) {
	    f4_do_hole_wall(s, WALL);
	} else if (!bu_strncmp(s->line, "COMPSPLT", 8)) {
	    f4_do_compsplt(s);
	} else if (!bu_strncmp(s->line, "SECTION", 7)) {
	    bu_strlcpy(s->field, &s->line[24], sizeof(s->field));
	    s->mode = atoi(s->field);
	    if (s->mode != 1 && s->mode != 2) {
		bu_log("Illegal mode (%d) for group %d component %d, using volume mode\n",
		       s->mode, s->group_id, s->comp_id);
		s->mode = 2;
	    }
	} else if (!bu_strncmp(s->line, "ENDDATA", 7)) {
	    break;
	}

	if (!get_line(s) || !s->line[0])
	    break;
    }

    if (s->debug) {
	bu_log("At end of Process_hole_wall:\n");
	List_holes();
    }
}


static void
f4_do_chgcomp(struct f4g_state *s)
{

    if (!s->pass)
	return;

    if (!s->fp_muves)
	return;

    fprintf(s->fp_muves, "%s", s->line);
}


static void
f4_do_cbacking(struct f4g_state *s)
{
    int gr1, co1, gr2, co2, material;
    fastf_t inthickness, probability;

    if (!s->pass)
	return;

    if (!s->fp_muves)
	return;

    bu_strlcpy(s->field, &s->line[8], sizeof(s->field));
    gr1 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[16], sizeof(s->field));
    co1 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[24], sizeof(s->field));
    gr2 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[32], sizeof(s->field));
    co2 = atoi(s->field);

    bu_strlcpy(s->field, &s->line[40], sizeof(s->field));
    inthickness = atof(s->field) * 25.4;

    bu_strlcpy(s->field, &s->line[48], sizeof(s->field));
    probability = atof(s->field);

    bu_strlcpy(s->field, &s->line[56], sizeof(s->field));
    material = atoi(s->field);

    fprintf(s->fp_muves, "CBACKING %d %d %g %g %d\n", gr1*1000+co1, gr2*1000+co2, inthickness, probability, material);
}


static int
Process_input(struct f4g_state *s, int pass_number)
{

    if (s->debug)
	bu_log("\n\nProcess_input(pass = %d)\n", pass_number);

    if (pass_number != 0 && pass_number != 1) {
	bu_exit(1, "Process_input: illegal pass number %d\n", pass_number);
    }

    s->region_id = 0;
    s->pass = pass_number;
    if (!get_line(s) || !s->line[0])
	bu_strlcpy(s->line, "ENDDATA", sizeof(s->line));
    while (1) {
	if (!bu_strncmp(s->line, "VEHICLE", 7))
	    f4_do_vehicle(s);
	else if (!bu_strncmp(s->line, "HOLE", 4))
	    ;
	else if (!bu_strncmp(s->line, "WALL", 4))
	    ;
	else if (!bu_strncmp(s->line, "COMPSPLT", 8))
	    ;
	else if (!bu_strncmp(s->line, "CBACKING", 8))
	    f4_do_cbacking(s);
	else if (!bu_strncmp(s->line, "CHGCOMP", 7))
	    f4_do_chgcomp(s);
	else if (!bu_strncmp(s->line, "SECTION", 7))
	    f4_do_section(s, 0);
	else if (!bu_strncmp(s->line, "$NAME", 5))
	    f4_do_name(s);
	else if (!bu_strncmp(s->line, "$COMMENT", 8))
	    ;
	else if (!bu_strncmp(s->line, "GRID", 4))
	    f4_do_grid(s);
	else if (!bu_strncmp(s->line, "CLINE", 5))
	    f4_do_cline(s);
	else if (!bu_strncmp(s->line, "CHEX1", 5))
	    f4_do_hex1(s);
	else if (!bu_strncmp(s->line, "CHEX2", 5))
	    f4_do_hex2(s);
	else if (!bu_strncmp(s->line, "CTRI", 4))
	    f4_do_tri(s);
	else if (!bu_strncmp(s->line, "CQUAD", 5))
	    f4_do_quad(s);
	else if (!bu_strncmp(s->line, "CCONE1", 6))
	    f4_do_ccone1(s);
	else if (!bu_strncmp(s->line, "CCONE2", 6))
	    f4_do_ccone2(s);
	else if (!bu_strncmp(s->line, "CCONE3", 6))
	    f4_do_ccone3(s);
	else if (!bu_strncmp(s->line, "CSPHERE", 7))
	    f4_do_sphere(s);
	else if (!bu_strncmp(s->line, "ENDDATA", 7)) {
	    f4_do_section(s, 1);
	    break;
	} else {
	    bu_log("ERROR: skipping unrecognized data type\n%s\n", s->line);
	}

	if (!get_line(s) || !s->line[0])
	    bu_strlcpy(s->line, "ENDDATA", sizeof(s->line));
    }

    if (s->debug) {
	bu_log("At pass %d:\n", s->pass);
	List_names(s);
    }

    return 0;
}


static void
make_region_list(struct f4g_state *s, char *str)
{
    char *ptr, *ptr2;

    s->region_list = (int *)bu_calloc(REGION_LIST_BLOCK, sizeof(int), "region_list");
    s->region_list_len = REGION_LIST_BLOCK;
    s->f4_do_skips = 0;

    ptr = strtok(str, CPP_XSTR(COMMA));
    while (ptr) {
	if ((ptr2=strchr(ptr, '-'))) {
	    int i, start, stop;

	    *ptr2 = '\0';
	    ptr2++;
	    start = atoi(ptr);
	    stop = atoi(ptr2);
	    for (i=start; i<=stop; i++) {
		if (s->f4_do_skips == s->region_list_len) {
		    s->region_list_len += REGION_LIST_BLOCK;
		    s->region_list = (int *)bu_realloc((char *)s->region_list, s->region_list_len*sizeof(int), "region_list");
		}
		s->region_list[s->f4_do_skips++] = i;
	    }
	} else {
	    if (s->f4_do_skips == s->region_list_len) {
		s->region_list_len += REGION_LIST_BLOCK;
		s->region_list = (int *)bu_realloc((char *)s->region_list, s->region_list_len*sizeof(int), "region_list");
	    }
	    s->region_list[s->f4_do_skips++] = atoi(ptr);
	}
	ptr = strtok((char *)NULL, ", ");
    }
}


static void
make_regions(struct f4g_state *s)
{
    struct name_tree *ptr1, *ptr2;
    struct holes *hptr;
    struct hole_list *lptr;
    struct compsplt *splt;
    struct wmember region;
    struct wmember solids;
    struct wmember holes;
    char reg_name[MAX_LINE_SIZE] = {0};
    char solids_name[MAX_LINE_SIZE] = {0};
    char hole_name[MAX_LINE_SIZE] = {0};
    char splt_name[MAX_LINE_SIZE] = {0};

    BU_LIST_INIT(&holes.l);

    /* loop through the list of region names (by ident) */
    bu_ptbl_reset(s->stack);
    ptr1 = name_root;
    while (1) {
	while (ptr1) {
	    PUSH(ptr1);
	    ptr1 = ptr1->rleft;
	}
	POP(name_tree, ptr1);
	if (!ptr1)
	    break;

	/* check if we are skipping some regions (but we might need all the holes) */
	if (skip_region(s, ptr1->region_id) && !is_a_hole(ptr1->region_id))
	    goto cont1;

	/* place all the solids for this ident in a "solids" combination */
	BU_LIST_INIT(&solids.l);
	bu_ptbl_reset(s->stack2);
	ptr2 = name_root;
	while (1) {
	    while (ptr2) {
		PUSH2(ptr2);
		ptr2 = ptr2->nleft;
	    }
	    POP2(name_tree, ptr2);
	    if (!ptr2)
		break;

	    if (ptr2->region_id == -ptr1->region_id && ptr2->inner == 0) {
		if (mk_addmember(ptr2->name, &solids.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
		    bu_log("make_regions: mk_addmember failed to add %s to %s\n", ptr1->name, ptr2->name);
	    }

	    ptr2 = ptr2->nright;
	}

	if (BU_LIST_IS_EMPTY(&solids.l))
	    goto cont1;

	snprintf(solids_name, MAX_LINE_SIZE, "solids_%d.s", ptr1->region_id);
	if (mk_comb(s->fpout, solids_name, &solids.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 1, 1))
	    bu_log("Failed to make combination of solids (%s)!\n\tRegion %s is in ERROR!\n",
		   solids_name, ptr1->name);

	/* hole components do not get made into regions */
	if (is_a_hole(ptr1->region_id)) {
	    /* just add it to the "holes" group */
	    if (mk_addmember(solids_name, &holes.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
		bu_log("make_regions: mk_addmember failed to add %s to holes group\n", ptr1->name);
	    goto cont1;
	}

	hptr = hole_root;
	while (hptr && hptr->group * 1000 + hptr->component != ptr1->region_id)
	    hptr = hptr->next;

	lptr = NULL;
	if (hptr != NULL) {
	    lptr = hptr->holes;
	}

	splt = compsplt_root;
	while (splt && splt->ident_to_split != ptr1->region_id)
	    splt = splt->next;

	s->mode = ptr1->mode;
	if (s->debug)
	    bu_log("Build region for %s %d, mode = %d\n", ptr1->name, ptr1->region_id, s->mode);

	if (splt) {
	    vect_t norm;
	    int found;

	    /* make a halfspace */
	    VSET(norm, 0.0, 0.0, 1.0);
	    snprintf(splt_name, MAX_LINE_SIZE, "splt_%d.s", ptr1->region_id);
	    mk_half(s->fpout, splt_name, norm, splt->z);

	    /* intersect halfspace with current region */
	    BU_LIST_INIT(&region.l);
	    if (mk_addmember(solids_name, &region.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
		bu_log("make_regions: mk_addmember failed to add %s to %s\n", solids_name, ptr1->name);

	    if (mk_addmember(splt_name, &region.l, NULL, WMOP_INTERSECT) == (struct wmember *)NULL)
		bu_log("make_regions: mk_addmember failed to add %s to %s\n", splt_name, ptr1->name);

	    while (lptr) {
		snprintf(hole_name, MAX_LINE_SIZE, "solids_%d.s", (lptr->group * 1000 + lptr->component));
		if (mk_addmember(hole_name, &region.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
		    bu_log("make_regions: mk_addmember failed to add %s to %s\n", hole_name, ptr1->name);
		lptr = lptr->next;
	    }
	    MK_REGION(s->fpout, &region, ptr1->name, ptr1->region_id, get_fast4_color(s, ptr1->region_id));

	    /* create new region by subtracting halfspace */
	    BU_LIST_INIT(&region.l);
	    lptr = NULL;
	    if (hptr != NULL) {
		lptr = hptr->holes;
	    }

	    if (mk_addmember(solids_name, &region.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
		bu_log("make_regions: mk_addmember failed to add %s to %s\n", solids_name, ptr1->name);

	    if (mk_addmember(splt_name, &region.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
		bu_log("make_regions: mk_addmember failed to add %s to %s\n", splt_name, ptr1->name);

	    while (lptr) {
		snprintf(hole_name, MAX_LINE_SIZE, "solids_%d.s", (lptr->group * 1000 + lptr->component));
		if (mk_addmember(hole_name, &region.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
		    bu_log("make_regions: mk_addmember failed to add %s to %s\n", hole_name, ptr1->name);
		lptr = lptr->next;
	    }
	    ptr2 = Search_ident(name_root, splt->new_ident, &found);
	    if (found) {
		MK_REGION(s->fpout, &region, ptr2->name, splt->new_ident, get_fast4_color(s, splt->new_ident));
	    } else {
		sprintf(reg_name, "comp_%d.r", splt->new_ident);
		MK_REGION(s->fpout, &region, reg_name, splt->new_ident, get_fast4_color(s, splt->new_ident));
	    }
	} else {
	    BU_LIST_INIT(&region.l);
	    if (mk_addmember(solids_name, &region.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
		bu_log("make_regions: mk_addmember failed to add %s to %s\n", solids_name, ptr1->name);

	    while (lptr) {
		snprintf(hole_name, MAX_LINE_SIZE, "solids_%d.s", (lptr->group * 1000 + lptr->component));
		if (mk_addmember(hole_name, &region.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
		    bu_log("make_regions: mk_addmember failed to add %s to %s\n", hole_name, ptr1->name);
		lptr = lptr->next;
	    }
	    MK_REGION(s->fpout, &region, ptr1->name, ptr1->region_id, get_fast4_color(s, ptr1->region_id));
	}
    cont1:
	ptr1 = ptr1->rright;
    }

    if (BU_LIST_NON_EMPTY(&holes.l)) {
	/* build a "holes" group */
	if (mk_comb(s->fpout, "holes", &holes.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 1, 1))
	    bu_log("Failed to make holes group!\n");
    }
}


#define COLOR_LINE_LEN 256

static void
read_fast4_colors(struct f4g_state *s, char *color_file)
{
    FILE *fp;
    char colorline[COLOR_LINE_LEN] = {0};
    int low, high;
    int r, g, b;
    struct fast4_color *color;

    if ((fp = fopen(color_file, "rb")) == (FILE *)NULL) {
	bu_log("Cannot open color file (%s)\n", color_file);
	return;
    }

    while (bu_fgets(colorline, COLOR_LINE_LEN, fp) != NULL) {
	if (sscanf(colorline, "%d %d %d %d %d", &low, &high, &r, &g, &b) != 5)
	    continue;

	/* skip invalid colors */
	if (r < 0 || 255 < r ||
	    g < 0 || 255 < g ||
	    b < 0 || 255 < b)
	    continue;

	/* skip bad region id ranges */
	if (high < low)
	    continue;

	BU_ALLOC(color, struct fast4_color);
	color->low = low;
	color->high = high;
	color->rgb[0] = r;
	color->rgb[1] = g;
	color->rgb[2] = b;
	BU_LIST_APPEND(&s->HeadColor.l, &color->l);
    }
    fclose(fp);
}


int
main(int argc, char **argv)
{
    int c;
    char *plot_file = NULL;
    char *color_file = NULL;
    struct f4g_state s;

    bu_setprogname(argv[0]);

    f4g_init(&s);

    while ((c=bu_getopt(argc, argv, "qm:o:c:dwx:b:X:C:h?")) != -1) {
	switch (c) {
	    case 'q':	/* quiet mode */
		s.quiet = 1;
		break;
	    case 'm':
		if ((s.fp_muves=fopen(bu_optarg, "wb")) == (FILE *)NULL) {
		    bu_log("Unable to open MUVES file (%s)\n\tno MUVES file created\n",
			   bu_optarg);
		}
		break;
	    case 'o':	/* output a plotfile of original FASTGEN4 elements */
		s.f4_do_plot = 1;
		plot_file = bu_optarg;
		break;
	    case 'c':	/* convert only the specified components */
		make_region_list(&s, bu_optarg);
		break;
	    case 'd':	/* debug option */
		s.debug = 1;
		break;
	    case 'w':	/* print warnings */
		s.warnings = 1;
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *)&s.frt_debug);
		bu_debug = s.frt_debug;
		break;
	    case 'b':
		sscanf(bu_optarg, "%x", (unsigned int *)&bu_debug);
		break;
	    case 'C':
		color_file = bu_optarg;
		break;
	    default:
		usage();
		break;
	}
    }

    if (argc-bu_optind != 2) {
	usage();
    }

    if ((s.fpin=fopen(argv[bu_optind], "rb")) == (FILE *)NULL) {
	perror("fast4-g");
	bu_exit(1, "Cannot open FASTGEN4 file (%s)\n", argv[bu_optind]);
    }

    if ((s.fpout=wdb_fopen(argv[bu_optind+1])) == NULL) {
	perror("fast4-g");
	bu_exit(1, "Cannot open file for output (%s)\n", argv[bu_optind+1]);
    }

    if (plot_file) {
	if ((s.fp_plot=fopen(plot_file, "wb")) == NULL) {
	    bu_log("Cannot open plot file (%s)\n", plot_file);
	    usage();
	}
    }

    if (bu_debug) {
	bu_printb("librtbu_debug", bu_debug, RT_DEBUG_FORMAT);
	bu_log("\n");
    }
    if (nmg_debug) {
	bu_printb("librt nmg_debug", nmg_debug, NMG_DEBUG_FORMAT);
	bu_log("\n");
    }

    if (color_file)
	read_fast4_colors(&s, color_file);


    cline_root = (struct cline *)NULL;

    name_root = (struct name_tree *)NULL;

    hole_root = (struct holes *)NULL;

    compsplt_root = (struct compsplt *)NULL;

    if (!s.quiet)
	bu_log("Scanning for HOLE, WALL, and COMPSPLT cards...\n");

    Process_hole_wall(&s);

    rewind(s.fpin);

    if (!s.quiet)
	bu_log("Building component names....\n");

    Process_input(&s, 0);

    rewind(s.fpin);

    /* Make an ID record if no vehicle card was found */
    if (!s.vehicle[0]) {
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	bu_path_component(&fname, argv[bu_optind], BU_PATH_BASENAME);
	mk_id_units(s.fpout, bu_vls_cstr(&fname), "in");
	bu_vls_free(&fname);
    }

    if (!s.quiet)
	bu_log("Building components....\n");

    while (Process_input(&s, 1));

    if (!s.quiet)
	bu_log("Building regions and groups....\n");

    /* make regions */
    make_regions(&s);

    /* make groups */
    f4_do_groups(&s);

    if (s.debug)
	List_holes();

    wdb_close(s.fpout);

    if (!s.quiet)
	bu_log("%d components converted\n", s.comp_count);

    bu_free(s.group_head, "group_head");
    bu_ptbl_free(s.stack2);
    BU_PUT(s.stack2, struct bu_ptbl);
    bu_ptbl_free(s.stack);
    BU_PUT(s.stack, struct bu_ptbl);
    bu_free(s.grid_points, "grid_points");

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
