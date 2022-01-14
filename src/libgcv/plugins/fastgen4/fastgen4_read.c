/*                 F A S T G E N 4 _ R E A D . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2022 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file fastgen4_read.c
 *
 * Plugin for importing from FASTGEN4 format.
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
#include "bu/debug.h"
#include "bu/getopt.h"
#include "rt/db4.h"
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"
#include "bv/plot3.h"
#include "gcv/api.h"


#define PUSH(ptr) bu_ptbl_ins(&pstate->stack, (long *)ptr)
#define POP(structure, ptr) { \
	if (BU_PTBL_LEN(&pstate->stack) == 0) \
	    ptr = (struct structure *)NULL; \
	else { \
	    ptr = (struct structure *)BU_PTBL_GET(&pstate->stack, BU_PTBL_LEN(&pstate->stack)-1); \
	    bu_ptbl_rm(&pstate->stack, (long *)ptr); \
	} \
    }
#define PUSH2(ptr) bu_ptbl_ins(&pstate->stack2, (long *)ptr)
#define POP2(structure, ptr) { \
	if (BU_PTBL_LEN(&pstate->stack2) == 0) \
	    ptr = (struct structure *)NULL; \
	else { \
	    ptr = (struct structure *)BU_PTBL_GET(&pstate->stack2, BU_PTBL_LEN(&pstate->stack2)-1); \
	    bu_ptbl_rm(&pstate->stack2, (long *)ptr); \
	} \
    }


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


enum section_mode {MODE_PLATE = 1, MODE_VOLUME = 2};


struct fast4_color {
    struct bu_list l;
    short low;
    short high;
    unsigned char rgb[3];
};


struct name_tree {
    uint32_t magic;
    int region_id;
    enum section_mode mode;		/* MODE_PLATE or MODE_VOLUME */
    int inner;		/* 0 => this is a base/group name for a FASTGEN element */
    int in_comp_group;	/* > 0 -> region already in a component group */
    char *name;
    struct name_tree *nleft, *nright, *rleft, *rright;
};


#define NAME_TREE_MAGIC 0x55555555


HIDDEN void
check_name_tree_magic(const struct name_tree *tree)
{
	if (!tree) { \
	    bu_log("ERROR: Null name_tree pointer, file=%s, line=%d\n", __FILE__, __LINE__);
	    bu_bomb("bad magic");
	} else if (tree->magic != NAME_TREE_MAGIC) {
	    bu_log("ERROR: bad name_tree pointer (%p), file=%s, line=%d\n", (void *)tree, __FILE__, __LINE__);
	    bu_bomb("bad magic");
	}
}


HIDDEN void
free_name_tree(struct name_tree *ptree)
{
    if (!ptree)
	return;

    check_name_tree_magic(ptree);

    free_name_tree(ptree->nleft);
    free_name_tree(ptree->nright);

    bu_free(ptree->name, "name");
    bu_free(ptree, "ptree");
}


struct compsplt {
    int ident_to_split;
    int new_ident;
    fastf_t z;
    struct compsplt *next;
};

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
};


HIDDEN const int hex_faces[12][3]={
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


struct conversion_state {
    const struct gcv_opts *gcv_options;
    struct rt_wdb *fpout;

    FILE *fpin;			        /* Input FASTGEN4 file pointer */
    FILE *fp_plot;	        	/* file for plot output */
    FILE *fp_muves;	        	/* file for MUVES data, output CHGCOMP and CBACKING data */

    int	*region_list;	                /* array of region_ids to be processed */
    point_t *grid_points;
    struct wmember *group_head;         /* Lists of regions for groups */
    struct wmember hole_head;	        /* List of regions used as holes (not solid parts of model) */
    struct bu_ptbl stack;		/* stack for traversing name_tree */
    struct bu_ptbl stack2;		/* stack for traversing name_tree */
    struct fast4_color HeadColor;
    struct compsplt *compsplt_root;
    struct name_tree *name_root;
    struct holes *hole_root;

    int	*faces;	                        /* one triplet per face indexing three grid points */
    char *facemode;	                /* mode for each face */
    fastf_t *thickness;	                /* thickness of each face */

    fastf_t min_radius;		        /* minimum radius for TGC solids */
    ssize_t group_head_cnt;
    int	grid_size;	                /* Number of points that will fit in current grid_pts array */
    int	max_grid_no;	                /* Maximum grid number used */
    enum section_mode mode;	                 	/* Plate mode (1) or volume mode (2), of current component */
    int	group_id;                 	/* Group identification number from SECTION card */
    int	comp_id;                 	/* Component identification number from SECTION card */
    int	region_id;		        /* Region id number (group id no X 1000 + component id no) */
    int	region_id_max;
    int	name_count;	                /* Count of number of times this name_name has been used */
    int	pass;		                /* pass number (0 -> only make names, 1-> do geometry) */
    int	bot;		                /* Flag: >0 -> There are bot's in current component */
    int	comp_count;	                /* Count of components in FASTGEN4 file */
    int	f4_do_skips;	                /* flag indicating that not all components will be processed */
    int	f4_do_plot;	                /* flag indicating plot file should be created */
    int	face_size;	                /* actual length of above arrays */
    int	face_count;	                /* number of faces in above arrays */
    int	region_list_len;                /* actual length of the malloc'd region_list array */

    char line[MAX_LINE_SIZE + 1];		/* Space for input line */
    char field[9];	            	/* Space for storing one field from an input line */
    char vehicle[17];	            	/* Title for BRL-CAD model from vehicle card */
};


HIDDEN void
fg4_zero_conversion_state(struct conversion_state *state)
{
    memset(state, 0, sizeof(*state));

    state->group_id = -1;
    state->comp_id = -1;

    state->grid_size = GRID_BLOCK;
    state->grid_points = (point_t *)bu_malloc(state->grid_size * sizeof(point_t),
			 "fast4-g: grid_points");
    state->min_radius = 2.0 * sqrt(SQRT_SMALL_FASTF);
    bu_ptbl_init(&state->stack, 64, " &stack");
    bu_ptbl_init(&state->stack2, 64, " &stack2 ");
    BU_LIST_INIT(&state->hole_head.l);
    BU_LIST_INIT(&state->HeadColor.l);
}


HIDDEN void
fg4_free_conversion_state(struct conversion_state *state)
{
    if (state->fpin)
	fclose(state->fpin);

    if (state->fp_plot)
	fclose(state->fp_plot);

    if (state->fp_muves)
	fclose(state->fp_muves);

    if (state->region_list)
	bu_free(state->region_list, "region_list");

    if (state->grid_points)
	bu_free(state->grid_points, "grid_points");

    if (state->group_head) {
	mk_freemembers(&state->group_head->l);
	bu_free(state->group_head, "group_head");
    }

    mk_freemembers(&state->hole_head.l);

    bu_ptbl_free(&state->stack);
    bu_ptbl_free(&state->stack2);

    bu_list_free(&state->HeadColor.l);

    if (state->compsplt_root) {
	struct compsplt *current, *next;

	for (current = state->compsplt_root; current; current = next) {
	    next = current->next;
	    bu_free(current, "current");
	}
    }

    if (state->hole_root) {
	struct holes *current_hole, *next_hole;

	for (current_hole = state->hole_root; current_hole; current_hole = next_hole) {
	    struct hole_list *current_list, *next_list;

	    for (current_list = current_hole->holes; current_list;
		 current_list = next_list) {
		next_list = current_list->next;
		bu_free(current_list, "current_list");
	    }

	    next_hole = current_hole->next;
	    bu_free(current_hole, "current_hole");
	}
    }

    free_name_tree(state->name_root);

    if (state->faces)
	bu_free(state->faces, "faces");

    if (state->facemode)
	bu_free(state->facemode, "facemode");

    if (state->thickness)
	bu_free(state->thickness, "thickness");
}


/* convenient function for building regions */
HIDDEN void
make_region(const struct conversion_state *pstate, struct rt_wdb *fp, struct wmember *headp, const char *name, int r_id, const unsigned char *rgb)
{
	if (pstate->mode == MODE_PLATE) {
	    if (pstate->gcv_options->verbosity_level)
		bu_log("Making region: %s (PLATE)\n", name);
	    mk_comb(fp, name, &((headp)->l), 'P', (char *)NULL, (char *)NULL, rgb, r_id, 0, 1, 100, 0, 0, 0);
	} else if (pstate->mode == MODE_VOLUME) {
	    if (pstate->gcv_options->verbosity_level)
		bu_log("Making region: %s (VOLUME)\n", name);
	    mk_comb(fp, name, &((headp)->l), 'V', (char *)NULL, (char *)NULL, rgb, r_id, 0, 1, 100, 0, 0, 0);
	} else {
	    bu_bomb("invalid mode");
	}
}


HIDDEN int
get_line(struct conversion_state *pstate)
{
    int len, done;
    struct bu_vls buffer = BU_VLS_INIT_ZERO;

    done = 0;
    while (!done) {
	len = bu_vls_gets(&buffer, pstate->fpin);
	if (len < 0) goto out; /* eof or error */
	if (len == 0) continue;
	bu_vls_trimspace(&buffer);
	len = (int)bu_vls_strlen(&buffer);
	if (len == 0) continue;
	done = 1;
    }

    if (len > MAX_LINE_SIZE)
	bu_log("WARNING: long line truncated\n");

    memset((void *)pstate->line, 0, MAX_LINE_SIZE);
    snprintf(pstate->line, MAX_LINE_SIZE, "%s", bu_vls_addr(&buffer));

out:
    bu_vls_free(&buffer);

    return len >= 0;
}


HIDDEN unsigned char *
get_fast4_color(const struct conversion_state *pstate, int r_id) {
    struct fast4_color *fcp;

    for (BU_LIST_FOR(fcp, fast4_color, &pstate->HeadColor.l)) {
	if (fcp->low <= r_id && r_id <= fcp->high)
	    return fcp->rgb;
    }

    return (unsigned char *)NULL;
}


HIDDEN int
is_a_hole(const struct conversion_state *pstate, int id)
{
    struct holes *hole_ptr;
    struct hole_list *ptr;

    hole_ptr = pstate->hole_root;

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


HIDDEN void
plot_tri(const struct conversion_state *pstate, int pt1, int pt2, int pt3)
{
    pdv_3move(pstate->fp_plot, pstate->grid_points[pt1]);
    pdv_3cont(pstate->fp_plot, pstate->grid_points[pt2]);
    pdv_3cont(pstate->fp_plot, pstate->grid_points[pt3]);
    pdv_3cont(pstate->fp_plot, pstate->grid_points[pt1]);
}


HIDDEN void
Check_names(struct conversion_state *pstate)
{
    struct name_tree *ptr;

    if (!pstate->name_root)
	return;

    bu_ptbl_reset(&pstate->stack);

    check_name_tree_magic(pstate->name_root);
    /* ident order */
    ptr = pstate->name_root;
    while (1) {
	while (ptr) {
	    PUSH(ptr);
	    ptr = ptr->rleft;
	}
	POP(name_tree, ptr);
	if (!ptr)
	    break;

	/* visit node */
	check_name_tree_magic(ptr);
	ptr = ptr->rright;
    }

    /* alphabetical order */
    ptr = pstate->name_root;
    while (1) {
	while (ptr) {
	    PUSH(ptr);
	    ptr = ptr->nleft;
	}
	POP(name_tree, ptr);
	if (!ptr)
	    break;

	/* visit node */
	check_name_tree_magic(ptr);
	ptr = ptr->nright;
    }
}


HIDDEN struct name_tree *
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


HIDDEN struct name_tree *
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


HIDDEN void
List_names(struct conversion_state *pstate)
{
    struct name_tree *ptr;

    bu_ptbl_reset(&pstate->stack);

    bu_log("\nNames in ident order:\n");
    ptr = pstate->name_root;
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
    ptr = pstate->name_root;
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


HIDDEN void
Insert_region_name(struct conversion_state *pstate, const char *name, int reg_id)
{
    struct name_tree *nptr_model, *rptr_model;
    struct name_tree *new_ptr;
    int foundn, foundr;

    if (pstate->gcv_options->debug_mode)
	bu_log("Insert_region_name(name=%s, reg_id=%d\n", name, reg_id);

    rptr_model = Search_ident(pstate->name_root, reg_id, &foundr);
    nptr_model = Search_names(pstate->name_root, name, &foundn);

    if (foundn && foundr)
	return;

    if (foundn != foundr) {
	bu_log("Insert_region_name: name %s ident %d\n\tfound name is %d\n\tfound ident is %d\n",
	       name, reg_id, foundn, foundr);
	List_names(pstate);
	bu_bomb("Cannot insert new node");
    }

    /* Add to tree for entire model */
    BU_ALLOC(new_ptr, struct name_tree);
    new_ptr->rleft = (struct name_tree *)NULL;
    new_ptr->rright = (struct name_tree *)NULL;
    new_ptr->nleft = (struct name_tree *)NULL;
    new_ptr->nright = (struct name_tree *)NULL;
    new_ptr->region_id = reg_id;
    new_ptr->mode = pstate->mode;
    new_ptr->inner = -1;
    new_ptr->in_comp_group = 0;
    new_ptr->name = bu_strdup(name);
    new_ptr->magic = NAME_TREE_MAGIC;

    V_MAX(pstate->region_id_max, reg_id);

    if (!pstate->name_root) {
	pstate->name_root = new_ptr;
    } else {
	int diff;

	diff = bu_strcmp(name, nptr_model->name);

	if (diff > 0) {
	    if (nptr_model->nright) {
		bu_log("Insert_region_name: nptr_model->nright not null\n");
		bu_bomb("Cannot insert new node");
	    }
	    nptr_model->nright = new_ptr;
	} else {
	    if (nptr_model->nleft) {
		bu_log("Insert_region_name: nptr_model->nleft not null\n");
		bu_bomb("Cannot insert new node");
	    }
	    nptr_model->nleft = new_ptr;
	}


	diff = reg_id - rptr_model->region_id;

	if (diff > 0) {
	    if (rptr_model->rright) {
		bu_log("Insert_region_name: rptr_model->rright not null\n");
		bu_bomb("Cannot insert new node");
	    }
	    rptr_model->rright = new_ptr;
	} else {
	    if (rptr_model->rleft) {
		bu_log("Insert_region_name: rptr_model->rleft not null\n");
		bu_bomb("Cannot insert new node");
	    }
	    rptr_model->rleft = new_ptr;
	}
    }
    Check_names(pstate);
}


HIDDEN char *
find_region_name(const struct conversion_state *pstate, int g_id, int c_id)
{
    struct name_tree *ptr;
    int reg_id;
    int found;

    reg_id = g_id * 1000 + c_id;

    if (pstate->gcv_options->debug_mode)
	bu_log("find_region_name(g_id=%d, c_id=%d), reg_id=%d\n", g_id, c_id, reg_id);

    ptr = Search_ident(pstate->name_root, reg_id, &found);

    if (found)
	return ptr->name;
    else
	return (char *)NULL;
}


static void
make_unique_name(struct bu_vls *name, const struct conversion_state *pstate)
{
    int name_cnt = 0;
    struct bu_vls vls = BU_VLS_INIT_ZERO;
    int found;

    /* If we're already unique, we're good */
    (void)Search_names(pstate->name_root, bu_vls_cstr(name), &found);
    if (!found)
	return;

    /* Not unique - make a unique name from what we got off the $NAME card */
    while (found) {
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "%s_%d", bu_vls_cstr(name), name_cnt);
	(void)Search_names(pstate->name_root, bu_vls_addr(&vls), &found);
	name_cnt++;
    }

    bu_vls_sprintf(name, "%s", bu_vls_cstr(&vls));
    bu_vls_free(&vls);
}


HIDDEN void
make_region_name(struct conversion_state *pstate, int g_id, int c_id)
{
    int r_id;
    const char *tmp_name;
    struct bu_vls name = BU_VLS_INIT_ZERO;

    r_id = g_id * 1000 + c_id;

    if (pstate->gcv_options->debug_mode)
	bu_log("make_region_name(g_id=%d, c_id=%d)\n", g_id, c_id);

    tmp_name = find_region_name(pstate, g_id, c_id);
    if (tmp_name)
	return;

    /* create a new name */
    bu_vls_sprintf(&name, "comp_%04d.r", r_id);

    make_unique_name(&name, pstate);

    Insert_region_name(pstate, bu_vls_cstr(&name), r_id);
    bu_vls_free(&name);
}


HIDDEN char *
get_solid_name(char type, int element_id, int c_id, int g_id, int inner)
{
    int reg_id;
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    reg_id = g_id * 1000 + c_id;

    bu_vls_printf(&vls, "%d.%d.%c%d", reg_id, element_id, type, inner);

    return bu_vls_strgrab(&vls);
}


HIDDEN void
Insert_name(const struct conversion_state *pstate, struct name_tree **root, char *name, int inner)
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
    new_ptr->region_id = (-pstate->region_id);
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
	    bu_bomb("Cannot insert new node");
	}
	ptr->nright = new_ptr;
    } else {
	if (ptr->nleft) {
	    bu_log("Insert_name: ptr->nleft not null\n");
	    bu_bomb("Cannot insert new node");
	}
	ptr->nleft = new_ptr;
    }
}


HIDDEN char *
make_solid_name(struct conversion_state *pstate, char type, int element_id, int c_id, int g_id, int inner)
{
    char *name;

    name = get_solid_name(type, element_id, c_id, g_id, inner);

    Insert_name(pstate, &pstate->name_root, name, inner);

    return name;
}


HIDDEN void
f4_do_compsplt(struct conversion_state *pstate)
{
    int gr, co, gr1, co1;
    fastf_t z;
    struct compsplt *splt;

    bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
    gr = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[16], sizeof(pstate->field));
    co = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[24], sizeof(pstate->field));
    gr1 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[32], sizeof(pstate->field));
    co1 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[40], sizeof(pstate->field));
    z = atof(pstate->field) * 25.4;

    if (pstate->compsplt_root == NULL) {
	BU_ALLOC(pstate->compsplt_root, struct compsplt);
	splt = pstate->compsplt_root;
    } else {
	splt = pstate->compsplt_root;
	while (splt->next)
	    splt = splt->next;
	BU_ALLOC(splt->next, struct compsplt);
	splt = splt->next;
    }
    splt->next = (struct compsplt *)NULL;
    splt->ident_to_split = gr * 1000 + co;
    splt->new_ident = gr1 * 1000 + co1;
    splt->z = z;
    make_region_name(pstate, gr1, co1);
}


HIDDEN void
List_holes(const struct conversion_state *pstate)
{
    struct holes *hole_ptr;
    struct hole_list *list_ptr;

    hole_ptr = pstate->hole_root;

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


HIDDEN void
Add_stragglers_to_groups(struct conversion_state *pstate)
{
    struct name_tree *ptr;

    ptr = pstate->name_root;

    while (1) {
	while (ptr) {
	    PUSH(ptr);
	    ptr = ptr->rleft;
	}
	POP(name_tree, ptr);
	if (!ptr)
	    break;

	/* visit node */
	check_name_tree_magic(ptr);

	/* FIXME: should not be manually recreating the wmember list
	 * when extending it.  just use a null-terminated list and
	 * realloc as needed...
	 */
	if (!ptr->in_comp_group && ptr->region_id > 0 && !is_a_hole(pstate, ptr->region_id)) {
	    /* add this component to a series */

	    if (!pstate->group_head || ptr->region_id > pstate->region_id_max) {
		struct wmember *new_head;
		ssize_t new_cnt, i;
		struct bu_list *list_first;

		new_cnt = lrint(ceil(pstate->region_id_max/1000.0));
		new_head = (struct wmember *)bu_calloc(new_cnt, sizeof(struct wmember), "group_head list");
		bu_log("ptr->region_id=%d region_id_max=%d new_cnt=%ld\n", ptr->region_id, pstate->region_id_max, new_cnt);

		for (i = 0 ; i < new_cnt ; i++) {
		    BU_LIST_INIT(&new_head[i].l);
		    if (pstate->group_head && i < pstate->group_head_cnt) {
			if (BU_LIST_NON_EMPTY(&pstate->group_head[i].l)) {
			    list_first = BU_LIST_FIRST(bu_list, &pstate->group_head[i].l);
			    BU_LIST_DEQUEUE(&pstate->group_head[i].l);
			    BU_LIST_INSERT(list_first, &new_head[i].l);
			}
		    }
		}
		if (pstate->group_head) {
		    bu_free(pstate->group_head, "old group_head");
		}
		pstate->group_head = new_head;
		pstate->group_head_cnt = new_cnt;
	    }

	    (void)mk_addmember(ptr->name, &pstate->group_head[ptr->region_id/1000].l, NULL, WMOP_UNION);
	    ptr->in_comp_group = 1;
	}

	ptr = ptr->rright;
    }
}


HIDDEN void
f4_do_groups(struct conversion_state *pstate)
{
    int group_no;
    struct wmember head_all;

    if (pstate->gcv_options->debug_mode)
	bu_log("f4_do_groups\n");

    BU_LIST_INIT(&head_all.l);

    Add_stragglers_to_groups(pstate);

    for (group_no=0; group_no < pstate->group_head_cnt; group_no++) {
	char name[MAX_LINE_SIZE] = {0};

	if (BU_LIST_IS_EMPTY(&pstate->group_head[group_no].l))
	    continue;

	snprintf(name, MAX_LINE_SIZE, "%dxxx_series", group_no);
	mk_lfcomb(pstate->fpout, name, &pstate->group_head[group_no], 0);

	if (mk_addmember(name, &head_all.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
	    bu_log("f4_do_groups: mk_addmember failed to add %s to group all\n", name);
    }

    if (BU_LIST_NON_EMPTY(&head_all.l))
	mk_lfcomb(pstate->fpout, "all", &head_all, 0);

    if (BU_LIST_NON_EMPTY(&pstate->hole_head.l))
	mk_lfcomb(pstate->fpout, "holes", &pstate->hole_head, 0);
}


HIDDEN void
f4_do_name(struct conversion_state *pstate)
{
    int foundr = 0;
    int g_id;
    int c_id;
    struct bu_vls comp_name = BU_VLS_INIT_ZERO;

    if (pstate->pass)
	return;

    if (pstate->gcv_options->debug_mode)
	bu_log("f4_do_name: %s\n", pstate->line);

    bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
    g_id = atoi(pstate->field);

    if (g_id != pstate->group_id) {
	bu_log("$NAME card for group %d in section for group %d ignored\n", g_id, pstate->group_id);
	bu_log("%s\n", pstate->line);
	return;
    }

    bu_strlcpy(pstate->field, &pstate->line[16], sizeof(pstate->field));
    c_id = atoi(pstate->field);

    if (c_id != pstate->comp_id) {
	bu_log("$NAME card for component %d in section for component %d ignored\n", c_id, pstate->comp_id);
	bu_log("%s\n", pstate->line);
	return;
    }

    /* Eliminate leading and trailing blanks.  Note - the 56 offset is based on the
     * fastgen file format definition - the name will be after that point. */
    bu_vls_sprintf(&comp_name, "%s", &pstate->line[56]);
    bu_vls_trimspace(&comp_name);
    if (!bu_vls_strlen(&comp_name)) {
	bu_vls_free(&comp_name);
	return;
    }

    /* Simplify */
    bu_vls_simplify(&comp_name, NULL, NULL, NULL);

    /* Reserve this name for group name. If we've already seen this region_id before,
     * go with the existing name rather than making a new unique name. */
    Search_ident(pstate->name_root, pstate->region_id, &foundr);
    if (!foundr) {
	make_unique_name(&comp_name, pstate);
    } else {
	bu_log("Already encountered region id %d - reusing name\n", pstate->region_id);
    }
    Insert_region_name(pstate, bu_vls_cstr(&comp_name), pstate->region_id);

    pstate->name_count = 0;

    bu_vls_free(&comp_name);
}


HIDDEN int
f4_do_grid(struct conversion_state *pstate)
{
    int grid_no;
    fastf_t x, y, z;

    if (!pstate->pass)	/* not doing geometry yet */
	return 1;

    bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
    grid_no = atoi(pstate->field);

    if (grid_no < 1) {
	bu_log("ERROR: bad grid id number = %d\n", grid_no);
	return 0;
    }

    bu_strlcpy(pstate->field, &pstate->line[24], sizeof(pstate->field));
    x = atof(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[32], sizeof(pstate->field));
    y = atof(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[40], sizeof(pstate->field));
    z = atof(pstate->field);

    while (grid_no > pstate->grid_size - 1) {
	pstate->grid_size += GRID_BLOCK;
	pstate->grid_points = (point_t *)bu_realloc((char *)pstate->grid_points, pstate->grid_size * sizeof(point_t), "fast4-g: grid_points");
    }

    VSET(pstate->grid_points[grid_no], x*25.4, y*25.4, z*25.4);

    V_MAX(pstate->max_grid_no, grid_no);

    return 1;
}


HIDDEN void
f4_do_sphere(struct conversion_state *pstate)
{
    int element_id;
    int center_pt;
    fastf_t thick;
    fastf_t radius;
    fastf_t inner_radius;
    char *name = (char *)NULL;
    struct wmember sphere_group;

    if (!pstate->pass) {
	make_region_name(pstate, pstate->group_id, pstate->comp_id);
	return;
    }

    bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
    element_id = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[24], sizeof(pstate->field));
    center_pt = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[56], sizeof(pstate->field));
    thick = atof(pstate->field) * 25.4;

    bu_strlcpy(pstate->field, &pstate->line[64], sizeof(pstate->field));
    radius = atof(pstate->field) * 25.4;

    if (radius <= 0.0) {
	bu_log("f4_do_sphere: illegal radius (%f), skipping sphere\n", radius);
	bu_log("\telement %d, component %d, group %d\n", element_id, pstate->comp_id, pstate->group_id);
	return;
    }

    if (center_pt < 1 || center_pt > pstate->max_grid_no) {
	bu_log("f4_do_sphere: illegal grid number for center point %d, skipping sphere\n", center_pt);
	bu_log("\telement %d, component %d, group %d\n", element_id, pstate->comp_id, pstate->group_id);
	return;
    }

    BU_LIST_INIT(&sphere_group.l);

    if (pstate->mode == MODE_VOLUME) {
	name = make_solid_name(pstate, CSPHERE, element_id, pstate->comp_id, pstate->group_id, 0);
	mk_sph(pstate->fpout, name, pstate->grid_points[center_pt], radius);
	bu_free(name, "solid_name");
    } else if (pstate->mode == MODE_PLATE) {
	name = make_solid_name(pstate, CSPHERE, element_id, pstate->comp_id, pstate->group_id, 1);
	mk_sph(pstate->fpout, name, pstate->grid_points[center_pt], radius);

	BU_LIST_INIT(&sphere_group.l);

	if (mk_addmember(name, &sphere_group.l, NULL, WMOP_UNION) == (struct wmember *)NULL) {
	    bu_bomb("f4_do_sphere: mk_addmember() failed");
	}
	bu_free(name, "solid_name");

	inner_radius = radius - thick;
	if (thick > 0.0 && inner_radius <= 0.0) {
	    bu_log("f4_do_sphere: illegal thickness (%f), skipping inner sphere\n", thick);
	    bu_log("\telement %d, component %d, group %d\n", element_id, pstate->comp_id, pstate->group_id);
	    return;
	}

	name = make_solid_name(pstate, CSPHERE, element_id, pstate->comp_id, pstate->group_id, 2);
	mk_sph(pstate->fpout, name, pstate->grid_points[center_pt], inner_radius);

	if (mk_addmember(name, &sphere_group.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL) {
	    bu_bomb("f4_do_sphere: mk_addmember() failed");
	}
	bu_free(name, "solid_name");

	name = make_solid_name(pstate, CSPHERE, element_id, pstate->comp_id, pstate->group_id, 0);
	mk_comb(pstate->fpout, name, &sphere_group.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 1, 1);
	bu_free(name, "solid_name");
    } else {
	bu_bomb("invalid mode");
    }
}


HIDDEN void
f4_do_vehicle(struct conversion_state *pstate)
{
    if (pstate->pass)
	return;

    bu_strlcpy(pstate->vehicle, &pstate->line[8], sizeof(pstate->vehicle));
    mk_id_units(pstate->fpout, pstate->vehicle, "in");
}


HIDDEN void
f4_do_cline(struct conversion_state *pstate)
{
    int element_id;
    int pt1, pt2;
    fastf_t thick;
    fastf_t radius;
    vect_t height;
    char *name;

    if (pstate->gcv_options->debug_mode)
	bu_log("f4_do_cline: %s\n", pstate->line);

    if (!pstate->pass) {
	make_region_name(pstate, pstate->group_id, pstate->comp_id);
	return;
    }

    bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
    element_id = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[24], sizeof(pstate->field));
    pt1 = atoi(pstate->field);

    if (pstate->pass && (pt1 < 1 || pt1 > pstate->max_grid_no)) {
	bu_log("Illegal grid point (%d) in CLINE, skipping\n", pt1);
	bu_log("\telement %d, component %d, group %d\n", element_id, pstate->comp_id, pstate->group_id);
	return;
    }

    bu_strlcpy(pstate->field, &pstate->line[32], sizeof(pstate->field));
    pt2 = atoi(pstate->field);

    if (pstate->pass && (pt2 < 1 || pt2 > pstate->max_grid_no)) {
	bu_log("Illegal grid point in CLINE (%d), skipping\n", pt2);
	bu_log("\telement %d, component %d, group %d\n", element_id, pstate->comp_id, pstate->group_id);
	return;
    }

    if (pt1 == pt2) {
	bu_log("Illegal grid points in CLINE (%d and %d), skipping\n", pt1, pt2);
	bu_log("\telement %d, component %d, group %d\n", element_id, pstate->comp_id, pstate->group_id);
	return;
    }

    bu_strlcpy(pstate->field, &pstate->line[56], sizeof(pstate->field));
    thick = atof(pstate->field) * 25.4;

    bu_strlcpy(pstate->field, &pstate->line[64], sizeof(pstate->field));
    radius = atof(pstate->field) * 25.4;

    VSUB2(height, pstate->grid_points[pt2], pstate->grid_points[pt1]);

    name = make_solid_name(pstate, CLINE, element_id, pstate->comp_id, pstate->group_id, 0);
    mk_cline(pstate->fpout, name, pstate->grid_points[pt1], height, radius, thick);
    bu_free(name, "solid_name");
}


HIDDEN int
f4_do_ccone1(struct conversion_state *pstate)
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

    bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
    element_id = atoi(pstate->field);

    if (!pstate->pass) {
	make_region_name(pstate, pstate->group_id, pstate->comp_id);
	if (!get_line(pstate)) {
	    bu_log("Unexpected EOF while reading continuation card for CCONE1\n");
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   pstate->group_id, pstate->comp_id, element_id);
	    return 0;
	}
	return 1;
    }

    bu_strlcpy(pstate->field, &pstate->line[24], sizeof(pstate->field));
    pt1 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[32], sizeof(pstate->field));
    pt2 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[56], sizeof(pstate->field));
    thick = atof(pstate->field) * 25.4;

    bu_strlcpy(pstate->field, &pstate->line[64], sizeof(pstate->field));
    r1 = atof(pstate->field) * 25.4;

    bu_strlcpy(pstate->field, &pstate->line[72], sizeof(pstate->field));
    c1 = atoi(pstate->field);

    if (!get_line(pstate)) {
	bu_log("Unexpected EOF while reading continuation card for CCONE1\n");
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
	       pstate->group_id, pstate->comp_id, element_id, c1);
	return 0;
    }

    bu_strlcpy(pstate->field, pstate->line, sizeof(pstate->field));
    c2 = atoi(pstate->field);

    if (c1 != c2) {
	bu_log("WARNING: CCONE1 continuation flags disagree, %d vs %d\n", c1, c2);
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
	       pstate->group_id, pstate->comp_id, element_id);
    }

    bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
    r2 = atof(pstate->field) * 25.4;

    bu_strlcpy(pstate->field, &pstate->line[16], sizeof(pstate->field));
    end1 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[24], sizeof(pstate->field));
    end2 = atoi(pstate->field);

    if (r1 < 0.0 || r2 < 0.0) {
	bu_log("ERROR: CCONE1 has illegal radii, %f and %f\n", r1/25.4, r2/25.4);
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
	       pstate->group_id, pstate->comp_id, element_id);
	bu_log("\tCCONE1 solid ignored\n");
	return 1;
    }

    if (pstate->mode == MODE_PLATE) {
	if (thick <= 0.0) {
	    bu_log("WARNING: Plate mode CCONE1 has illegal thickness (%f)\n", thick/25.4);
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   pstate->group_id, pstate->comp_id, element_id);
	    bu_log("\tCCONE1 solid plate mode overridden, now being treated as volume mode\n");
	    pstate->mode = MODE_VOLUME;
	}

	if (r1-thick < pstate->min_radius && r2-thick < pstate->min_radius) {
	    bu_log("ERROR: Plate mode CCONE1 has too large thickness (%f)\n", thick/25.4);
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   pstate->group_id, pstate->comp_id, element_id);
	    bu_log("\tCCONE1 solid ignored\n");
	    return 1;
	}
    }

    if (pt1 < 1 || pt1 > pstate->max_grid_no || pt2 < 1 || pt2 > pstate->max_grid_no || pt1 == pt2) {
	bu_log("ERROR: CCONE1 has illegal grid points (%d and %d)\n", pt1, pt2);
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
	       pstate->group_id, pstate->comp_id, element_id);
	bu_log("\tCCONE1 solid ignored\n");
	return 1;
    }

    /* BRL_CAD doesn't allow zero radius, so use a very small radius */
    V_MAX(r1, pstate->min_radius);
    V_MAX(r2, pstate->min_radius);

    VSUB2(height, pstate->grid_points[pt2], pstate->grid_points[pt1]);

    if (pstate->mode == MODE_VOLUME) {
	outer_name = make_solid_name(pstate, CCONE1, element_id, pstate->comp_id, pstate->group_id, 0);
	mk_trc_h(pstate->fpout, outer_name, pstate->grid_points[pt1], height, r1, r2);
	bu_free(outer_name, "solid_name");
    } else if (pstate->mode == MODE_PLATE) {
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
	outer_name = make_solid_name(pstate, CCONE1, element_id, pstate->comp_id, pstate->group_id, 1);
	mk_trc_h(pstate->fpout, outer_name, pstate->grid_points[pt1], height, r1, r2);

	BU_LIST_INIT(&r_head.l);
	if (mk_addmember(outer_name, &r_head.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
	    bu_bomb("CCONE1: mk_addmember failed");
	bu_free(outer_name, "solid_name");

	length = MAGNITUDE(height);
	VSCALE(height_dir, height, 1.0/length);
	slant_len = sqrt(length*length + (r2 - r1)*(r2 - r1));

	sin_ang = length/slant_len;

	if (end1 == END_OPEN) {
	    inner_r1 = r1 - thick/sin_ang;
	    VMOVE(base, pstate->grid_points[pt1]);
	} else {
	    fastf_t r1a = r1 + (r2 - r1)*thick/length;
	    inner_r1 = r1a - thick/sin_ang;
	    VJOIN1(base, pstate->grid_points[pt1], thick, height_dir);
	}

	if (inner_r1 < 0.0) {
	    fastf_t dist_to_new_base;

	    dist_to_new_base = inner_r1 * length/(r1 - r2);
	    inner_r1 = pstate->min_radius;
	    VJOIN1(base, base, dist_to_new_base, height_dir);
	} else {
	    V_MAX(inner_r1, pstate->min_radius);
	}

	if (end2 == END_OPEN) {
	    inner_r2 = r2 - thick/sin_ang;
	    VMOVE(top, pstate->grid_points[pt2]);
	} else {
	    fastf_t r2a = r2 + (r1 - r2)*thick/length;
	    inner_r2 = r2a - thick/sin_ang;
	    VJOIN1(top, pstate->grid_points[pt2], -thick, height_dir);
	}

	if (inner_r2 < 0.0) {
	    fastf_t dist_to_new_top;

	    dist_to_new_top = inner_r2 * length/(r2 - r1);
	    inner_r2 = pstate->min_radius;
	    VJOIN1(top, top, -dist_to_new_top, height_dir);
	} else {
	    V_MAX(inner_r2, pstate->min_radius);
	}

	VSUB2(inner_height, top, base);
	if (VDOT(inner_height, height) <= 0.0) {
	    bu_log("ERROR: CCONE1 height (%f) too small for thickness (%f)\n", length/25.4, thick/25.4);
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   pstate->group_id, pstate->comp_id, element_id);
	    bu_log("\tCCONE1 inner solid ignored\n");
	} else {
	    /* make inner tgc */

	    inner_name = make_solid_name(pstate, CCONE1, element_id, pstate->comp_id, pstate->group_id, 2);
	    mk_trc_h(pstate->fpout, inner_name, base, inner_height, inner_r1, inner_r2);

	    if (mk_addmember(inner_name, &r_head.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
		bu_bomb("CCONE1: mk_addmember failed");
	    bu_free(inner_name, "solid_name");
	}

	name = make_solid_name(pstate, CCONE1, element_id, pstate->comp_id, pstate->group_id, 0);
	mk_comb(pstate->fpout, name, &r_head.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 1, 1);
	bu_free(name, "solid_name");
    } else {
	bu_bomb("invalid mode");
    }

    return 1;
}


HIDDEN int
f4_do_ccone2(struct conversion_state *pstate)
{
    int element_id;
    int pt1, pt2;
    int c1, c2;
    fastf_t ro1, ro2, ri1, ri2;
    vect_t height;
    char *name = (char *)NULL;
    struct wmember r_head;

    bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
    element_id = atoi(pstate->field);

    if (!pstate->pass) {
	make_region_name(pstate, pstate->group_id, pstate->comp_id);
	if (!get_line(pstate)) {
	    bu_log("Unexpected EOF while reading continuation card for CCONE2\n");
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   pstate->group_id, pstate->comp_id, element_id);
	    return 0;
	}
	return 1;
    }

    bu_strlcpy(pstate->field, &pstate->line[24], sizeof(pstate->field));
    pt1 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[32], sizeof(pstate->field));
    pt2 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[64], sizeof(pstate->field));
    ro1 = atof(pstate->field) * 25.4;

    bu_strlcpy(pstate->field, &pstate->line[72], sizeof(pstate->field));
    c1 = atoi(pstate->field);

    if (!get_line(pstate)) {
	bu_log("Unexpected EOF while reading continuation card for CCONE2\n");
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
	       pstate->group_id, pstate->comp_id, element_id, c1);
	return 0;
    }

    bu_strlcpy(pstate->field, pstate->line, sizeof(pstate->field));
    c2 = atoi(pstate->field);

    if (c1 != c2) {
	bu_log("WARNING: CCONE2 continuation flags disagree, %d vs %d\n", c1, c2);
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
	       pstate->group_id, pstate->comp_id, element_id);
    }

    bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
    ro2 = atof(pstate->field) * 25.4;

    bu_strlcpy(pstate->field, &pstate->line[16], sizeof(pstate->field));
    ri1 = atof(pstate->field) * 25.4;

    bu_strlcpy(pstate->field, &pstate->line[24], sizeof(pstate->field));
    ri2 = atof(pstate->field) * 25.4;

    if (pt1 == pt2) {
	bu_log("ERROR: CCONE2 has same endpoints %d and %d\n", pt1, pt2);
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
	       pstate->group_id, pstate->comp_id, element_id);
	return 1;
    }

    if (ro1 < 0.0 || ro2 < 0.0 || ri1 < 0.0 || ri2 < 0.0) {
	bu_log("ERROR: CCONE2 has illegal radii %f %f %f %f\n", ro1, ro2, ri1, ri2);
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
	       pstate->group_id, pstate->comp_id, element_id);
	return 1;
    }

    V_MAX(ro1, pstate->min_radius);
    V_MAX(ro2, pstate->min_radius);

    BU_LIST_INIT(&r_head.l);

    VSUB2(height, pstate->grid_points[pt2], pstate->grid_points[pt1]);

    if (ri1 <= 0.0 && ri2 <= 0.0) {
	name = make_solid_name(pstate, CCONE2, element_id, pstate->comp_id, pstate->group_id, 0);
	mk_trc_h(pstate->fpout, name, pstate->grid_points[pt1], height, ro1, ro2);
	bu_free(name, "solid_name");
    } else {
	name = make_solid_name(pstate, CCONE2, element_id, pstate->comp_id, pstate->group_id, 1);
	mk_trc_h(pstate->fpout, name, pstate->grid_points[pt1], height, ro1, ro2);

	if (mk_addmember(name, &r_head.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
	    bu_bomb("mk_addmember failed!");
	bu_free(name, "solid_name");

	V_MAX(ri1, pstate->min_radius);
	V_MAX(ri2, pstate->min_radius);

	name = make_solid_name(pstate, CCONE2, element_id, pstate->comp_id, pstate->group_id, 2);
	mk_trc_h(pstate->fpout, name, pstate->grid_points[pt1], height, ri1, ri2);

	if (mk_addmember(name, &r_head.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
	    bu_bomb("mk_addmember failed!");
	bu_free(name, "solid_name");

	name = make_solid_name(pstate, CCONE2, element_id, pstate->comp_id, pstate->group_id, 0);
	mk_comb(pstate->fpout, name, &r_head.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 1, 1);
	bu_free(name, "solid_name");
    }

    return 1;
}


HIDDEN int
f4_do_ccone3(struct conversion_state *pstate)
{
    int element_id;
    int pt1, pt2, pt3, pt4, i;
    char *name;
    fastf_t ro[4], ri[4], len03, len01, len12, len23;
    vect_t diff, diff2, diff3, diff4;
    struct wmember r_head;

    bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
    element_id = atoi(pstate->field);

    if (!pstate->pass) {
	make_region_name(pstate, pstate->group_id, pstate->comp_id);
	if (!get_line(pstate)) {
	    bu_log("Unexpected EOF while reading continuation card for CCONE3\n");
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   pstate->group_id, pstate->comp_id, element_id);
	    return 0;
	}
	return 1;
    }

    bu_strlcpy(pstate->field, &pstate->line[24], sizeof(pstate->field));
    pt1 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[32], sizeof(pstate->field));
    pt2 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[40], sizeof(pstate->field));
    pt3 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[48], sizeof(pstate->field));
    pt4 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[72], sizeof(pstate->field));

    if (!get_line(pstate)) {
	bu_log("Unexpected EOF while reading continuation card for CCONE3\n");
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %8.8s\n",
	       pstate->group_id, pstate->comp_id, element_id, pstate->field);
	return 0;
    }

    if (bu_strncmp(pstate->field, pstate->line, 8)) {
	bu_log("WARNING: CCONE3 continuation flags disagree, %8.8s vs %8.8s\n", pstate->field, pstate->line);
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
	       pstate->group_id, pstate->comp_id, element_id);
    }

    for (i=0; i<4; i++) {
	bu_strlcpy(pstate->field, &pstate->line[8*(i+1)], sizeof(pstate->field));
	ro[i] = atof(pstate->field) * 25.4;
	if (ro[i] < 0.0) {
	    bu_log("ERROR: CCONE3 has illegal radius %f\n", ro[i]);
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   pstate->group_id, pstate->comp_id, element_id);
	    return 1;
	}
	if (BU_STR_EQUAL(pstate->field, "        "))
	    ro[i] = -1.0;
    }

    for (i=0; i<4; i++) {
	bu_strlcpy(pstate->field, &pstate->line[32 + 8*(i+1)], sizeof(pstate->field));
	ri[i] = atof(pstate->field) * 25.4;
	if (ri[i] < 0.0) {
	    bu_log("ERROR: CCONE3 has illegal radius %f\n", ri[i]);
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   pstate->group_id, pstate->comp_id, element_id);
	    return 1;
	}
	if (BU_STR_EQUAL(pstate->field, "        "))
	    ri[i] = -1.0;
    }

    VSUB2(diff4, pstate->grid_points[pt4], pstate->grid_points[pt1]);
    VSUB2(diff3, pstate->grid_points[pt3], pstate->grid_points[pt1]);
    VSUB2(diff2, pstate->grid_points[pt2], pstate->grid_points[pt1]);

    len03 = MAGNITUDE(diff4);
    len01 = MAGNITUDE(diff2);
    len12 = MAGNITUDE(diff3) - len01;
    len23 = len03 - len01 - len12;

    for (i=0; i<4; i+=3) {
	if (EQUAL(ro[i], -1.0)) {
	    if (EQUAL(ri[i], -1.0)) {
		bu_log("ERROR: both inner and outer radii at g%d of a CCONE3 are undefined\n", i+1);
		bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		       pstate->group_id, pstate->comp_id, element_id);
		return 1;
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
	V_MAX(ro[i], pstate->min_radius);
	V_MAX(ri[i], pstate->min_radius);
    }

    BU_LIST_INIT(&r_head.l);

    if (pt1 != pt2) {
	VSUB2(diff, pstate->grid_points[pt2], pstate->grid_points[pt1]);

	/* make first cone */
	if (!EQUAL(ro[0], pstate->min_radius) || !EQUAL(ro[1], pstate->min_radius)) {
	    name = make_solid_name(pstate, CCONE3, element_id, pstate->comp_id, pstate->group_id, 1);
	    mk_trc_h(pstate->fpout, name, pstate->grid_points[pt1], diff, ro[0], ro[1]);
	    if (mk_addmember(name, &r_head.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
		bu_bomb("mk_addmember failed!");
	    bu_free(name, "solid_name");

	    /* and the inner cone */
	    if (!EQUAL(ri[0], pstate->min_radius) || !EQUAL(ri[1], pstate->min_radius)) {
		name = make_solid_name(pstate, CCONE3, element_id, pstate->comp_id, pstate->group_id, 11);
		mk_trc_h(pstate->fpout, name, pstate->grid_points[pt1], diff, ri[0], ri[1]);
		if (mk_addmember(name, &r_head.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
		    bu_bomb("mk_addmember failed!");
		bu_free(name, "solid_name");
	    }
	}
    }

    if (pt2 != pt3) {
	VSUB2(diff, pstate->grid_points[pt3], pstate->grid_points[pt2]);

	/* make second cone */
	if (!EQUAL(ro[1], pstate->min_radius) || !EQUAL(ro[2], pstate->min_radius)) {
	    name = make_solid_name(pstate, CCONE3, element_id, pstate->comp_id, pstate->group_id, 2);
	    mk_trc_h(pstate->fpout, name, pstate->grid_points[pt2], diff, ro[1], ro[2]);
	    if (mk_addmember(name, &r_head.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
		bu_bomb("mk_addmember failed!");
	    bu_free(name, "solid_name");

	    /* and the inner cone */
	    if (!EQUAL(ri[1], pstate->min_radius) || !EQUAL(ri[2], pstate->min_radius)) {
		name = make_solid_name(pstate, CCONE3, element_id, pstate->comp_id, pstate->group_id, 22);
		mk_trc_h(pstate->fpout, name, pstate->grid_points[pt2], diff, ri[1], ri[2]);
		if (mk_addmember(name, &r_head.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
		    bu_bomb("mk_addmember failed!");
		bu_free(name, "solid_name");
	    }
	}
    }

    if (pt3 != pt4) {
	VSUB2(diff, pstate->grid_points[pt4], pstate->grid_points[pt3]);

	/* make third cone */
	if (!EQUAL(ro[2], pstate->min_radius) || !EQUAL(ro[3], pstate->min_radius)) {
	    name = make_solid_name(pstate, CCONE3, element_id, pstate->comp_id, pstate->group_id, 3);
	    mk_trc_h(pstate->fpout, name, pstate->grid_points[pt3], diff, ro[2], ro[3]);
	    if (mk_addmember(name, &r_head.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
		bu_bomb("mk_addmember failed!");
	    bu_free(name, "solid_name");

	    /* and the inner cone */
	    if (!EQUAL(ri[2], pstate->min_radius) || !EQUAL(ri[3], pstate->min_radius)) {
		name = make_solid_name(pstate, CCONE3, element_id, pstate->comp_id, pstate->group_id, 33);
		mk_trc_h(pstate->fpout, name, pstate->grid_points[pt3], diff, ri[2], ri[3]);
		if (mk_addmember(name, &r_head.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
		    bu_bomb("mk_addmember failed!");
		bu_free(name, "solid_name");
	    }
	}
    }

    name = make_solid_name(pstate, CCONE3, element_id, pstate->comp_id, pstate->group_id, 0);
    mk_comb(pstate->fpout, name, &r_head.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 1, 1);
    bu_free(name, "solid_name");

    return 1;
}


HIDDEN int
skip_region(const struct conversion_state *pstate, int id)
{
    int i;

    if (!pstate->f4_do_skips)
	return 0;

    for (i=0; i<pstate->f4_do_skips; i++) {
	if (id == pstate->region_list[i])
	    return 0;
    }

    return 1;
}


HIDDEN void
Add_holes(struct conversion_state *pstate, int type, int gr, int comp, struct hole_list *ptr)
{
    struct holes *hole_ptr = (struct holes *)NULL;
    struct holes *prev = (struct holes *)NULL;
    struct hole_list *hptr= (struct hole_list *)NULL;

    if (pstate->gcv_options->debug_mode) {
	bu_log("Adding holes for group %d, component %d:\n", gr, comp);
	hptr = ptr;
	while (hptr) {
	    bu_log("\t%d %d\n", hptr->group, hptr->component);
	    hptr = hptr->next;
	}
    }

    if (pstate->f4_do_skips) {
	if (!skip_region(pstate, gr*1000 + comp)) {
	    /* add holes for this region to the list of regions to process */
	    hptr = ptr;
	    while (hptr) {
		if (pstate->f4_do_skips == pstate->region_list_len) {
		    pstate->region_list_len += REGION_LIST_BLOCK;
		    pstate->region_list = (int *)bu_realloc((char *)pstate->region_list, pstate->region_list_len*sizeof(int), "region_list");
		}
		pstate->region_list[pstate->f4_do_skips++] = 1000*hptr->group + hptr->component;
		hptr = hptr->next;
	    }
	}
    }

    if (!pstate->hole_root) {
	BU_ALLOC(pstate->hole_root, struct holes);
	pstate->hole_root->group = gr;
	pstate->hole_root->component = comp;
	pstate->hole_root->type = type;
	pstate->hole_root->holes = ptr;
	pstate->hole_root->next = (struct holes *)NULL;
	return;
    }

    hole_ptr = pstate->hole_root;
    prev = pstate->hole_root;
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


HIDDEN void
f4_do_hole_wall(struct conversion_state *pstate, int type)
{
    struct hole_list *list_ptr;
    struct hole_list *list_start;
    int group, comp;
    int igrp, icmp;
    size_t s_len;
    size_t col;

    if (pstate->gcv_options->debug_mode)
	bu_log("f4_do_hole_wall: %s\n", pstate->line);

    if (pstate->pass)
	return;

    if (type != HOLE && type != WALL) {
	bu_bomb("f4_do_hole_wall: unrecognized type");
    }

    /* eliminate trailing blanks */
    s_len = strlen(pstate->line);
    while (isspace((int)pstate->line[--s_len]))
	pstate->line[s_len] = '\0';

    s_len = strlen(pstate->line);
    V_MIN(s_len, 80);

    bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
    group = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[16], sizeof(pstate->field));
    comp = atoi(pstate->field);

    list_start = (struct hole_list *)NULL;
    list_ptr = (struct hole_list *)NULL;
    col = 24;

    while (col < s_len) {
	bu_strlcpy(pstate->field, &pstate->line[col], sizeof(pstate->field));
	igrp = atoi(pstate->field);

	col += 8;
	if (col >= s_len)
	    break;

	bu_strlcpy(pstate->field, &pstate->line[col], sizeof(pstate->field));
	icmp = atoi(pstate->field);

	if (igrp >= 0 && icmp > 0) {
	    if (igrp == group && comp == icmp) {
		bu_log("Hole or wall card references itself (ignoring): (%s)\n", pstate->line);
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

    Add_holes(pstate, type, group, comp, list_start);
}


HIDDEN void
f4_Add_bot_face(struct conversion_state *pstate, int pt1, int pt2, int pt3, fastf_t thick, int pos)
{

    if (pt1 == pt2 || pt2 == pt3 || pt1 == pt3) {
	bu_log("f4_Add_bot_face: ignoring degenerate triangle in group %d component %d\n", pstate->group_id, pstate->comp_id);
	return;
    }

    if (pos == 0)	/* use default */
	pos = POS_FRONT;

    if (pstate->mode == MODE_PLATE) {
	if (pos != POS_CENTER && pos != POS_FRONT) {
	    bu_log("f4_Add_bot_face: illegal position parameter (%d), must be one or two (ignoring face for group %d component %d)\n", pos, pstate->group_id, pstate->comp_id);
	    return;
	}
    }

    if (pstate->face_count >= pstate->face_size) {
	pstate->face_size += GRID_BLOCK;
	pstate->faces = (int *)bu_realloc((void *)pstate->faces, pstate->face_size*3*sizeof(int), "faces");
	pstate->thickness = (fastf_t *)bu_realloc((void *)pstate->thickness, pstate->face_size*sizeof(fastf_t), "thickness");
	pstate->facemode = (char *)bu_realloc((void *)pstate->facemode, pstate->face_size*sizeof(char), "facemode");
    }

    pstate->faces[pstate->face_count*3] = pt1;
    pstate->faces[pstate->face_count*3+1] = pt2;
    pstate->faces[pstate->face_count*3+2] = pt3;

    if (pstate->mode == MODE_PLATE) {
	pstate->thickness[pstate->face_count] = thick;
	pstate->facemode[pstate->face_count] = pos;
    } else if (pstate->mode == MODE_VOLUME) {
	pstate->thickness[pstate->face_count] = 0.0;
	pstate->facemode[pstate->face_count] = 0;
    } else {
	bu_bomb("invalid mode");
    }

    pstate->face_count++;
}


HIDDEN void
f4_do_tri(struct conversion_state *pstate)
{
    int element_id;
    int pt1, pt2, pt3;
    fastf_t thick;
    int pos;

    if (pstate->gcv_options->debug_mode)
	bu_log("f4_do_tri: %s\n", pstate->line);

    bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
    element_id = atoi(pstate->field);

    if (!pstate->bot)
	pstate->bot = element_id;

    if (!pstate->pass)
	return;

    if (pstate->faces == NULL) {
	pstate->faces = (int *)bu_malloc(GRID_BLOCK*3*sizeof(int), "faces");
	pstate->thickness = (fastf_t *)bu_malloc(GRID_BLOCK*sizeof(fastf_t), "thickness");
	pstate->facemode = (char *)bu_malloc(GRID_BLOCK*sizeof(char), "facemode");
	pstate->face_size = GRID_BLOCK;
	pstate->face_count = 0;
    }

    bu_strlcpy(pstate->field, &pstate->line[24], sizeof(pstate->field));
    pt1 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[32], sizeof(pstate->field));
    pt2 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[40], sizeof(pstate->field));
    pt3 = atoi(pstate->field);

    thick = 0.0;
    pos = 0;

    if (pstate->mode == MODE_PLATE) {
	bu_strlcpy(pstate->field, &pstate->line[56], sizeof(pstate->field));
	thick = atof(pstate->field) * 25.4;

	bu_strlcpy(pstate->field, &pstate->line[64], sizeof(pstate->field));
	pos = atoi(pstate->field);
	if (pos == 0)
	    pos = POS_FRONT;

	if (pstate->gcv_options->debug_mode)
	    bu_log("\tplate mode: thickness = %f\n", thick);

    }

    if (pstate->f4_do_plot)
	plot_tri(pstate, pt1, pt2, pt3);

    f4_Add_bot_face(pstate, pt1, pt2, pt3, thick, pos);
}


HIDDEN void
f4_do_quad(struct conversion_state *pstate)
{
    int element_id;
    int pt1, pt2, pt3, pt4;
    fastf_t thick = 0.0;
    int pos = 0;

    bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
    element_id = atoi(pstate->field);

    if (pstate->gcv_options->debug_mode)
	bu_log("f4_do_quad: %s\n", pstate->line);

    if (!pstate->bot)
	pstate->bot = element_id;

    if (!pstate->pass)
	return;

    if (pstate->faces == NULL) {
	pstate->faces = (int *)bu_malloc(GRID_BLOCK*3*sizeof(int), "faces");
	pstate->thickness = (fastf_t *)bu_malloc(GRID_BLOCK*sizeof(fastf_t), "thickness");
	pstate->facemode = (char *)bu_malloc(GRID_BLOCK*sizeof(char), "facemode");
	pstate->face_size = GRID_BLOCK;
	pstate->face_count = 0;
    }

    bu_strlcpy(pstate->field, &pstate->line[24], sizeof(pstate->field));
    pt1 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[32], sizeof(pstate->field));
    pt2 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[40], sizeof(pstate->field));
    pt3 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[48], sizeof(pstate->field));
    pt4 = atoi(pstate->field);

    if (pstate->mode == MODE_PLATE) {
	bu_strlcpy(pstate->field, &pstate->line[56], sizeof(pstate->field));
	thick = atof(pstate->field) * 25.4;

	bu_strlcpy(pstate->field, &pstate->line[64], sizeof(pstate->field));
	pos = atoi(pstate->field);

	if (pos == 0)	/* use default */
	    pos = POS_FRONT;

	if (pos != POS_CENTER && pos != POS_FRONT) {
	    bu_log("f4_do_quad: illegal position parameter (%d), must be one or two\n", pos);
	    bu_log("\telement %d, component %d, group %d\n", element_id, pstate->comp_id, pstate->group_id);
	    return;
	}
    }

    f4_Add_bot_face(pstate, pt1, pt2, pt3, thick, pos);
    f4_Add_bot_face(pstate, pt1, pt3, pt4, thick, pos);
}


HIDDEN void
make_bot_object(struct conversion_state *pstate)
{
    int i;
    int max_pt = 0;
    int min_pt = 999999;
    int num_vertices;
    struct bu_bitv *bv = (struct bu_bitv *)NULL;
    int bot_mode;
    char *name = (char *)NULL;
    int element_id = pstate->bot;
    int count;
    struct rt_bot_internal bot_ip;

    if (!pstate->pass) {
	make_region_name(pstate, pstate->group_id, pstate->comp_id);
	return;
    }

    bot_ip.magic = RT_BOT_INTERNAL_MAGIC;
    for (i=0; i<pstate->face_count; i++) {
	V_MIN(min_pt, pstate->faces[i*3]);
	V_MAX(max_pt, pstate->faces[i*3]);
	V_MIN(min_pt, pstate->faces[i*3+1]);
	V_MAX(max_pt, pstate->faces[i*3+1]);
	V_MIN(min_pt, pstate->faces[i*3+2]);
	V_MAX(max_pt, pstate->faces[i*3+2]);
    }

    num_vertices = max_pt - min_pt + 1;
    bot_ip.num_vertices = num_vertices;
    bot_ip.vertices = (fastf_t *)bu_calloc(num_vertices*3, sizeof(fastf_t), "bot vertices");
    for (i=0; i<num_vertices; i++)
	VMOVE(&bot_ip.vertices[i*3], pstate->grid_points[min_pt+i]);

    for (i=0; i<pstate->face_count*3; i++)
	pstate->faces[i] -= min_pt;
    bot_ip.num_faces = pstate->face_count;
    bot_ip.faces = (int *)bu_calloc(pstate->face_count*3, sizeof(int), "bot faces");
    for (i=0; i<pstate->face_count*3; i++)
	bot_ip.faces[i] = pstate->faces[i];

    bot_ip.face_mode = (struct bu_bitv *)NULL;
    bot_ip.thickness = (fastf_t *)NULL;
    if (pstate->mode == MODE_PLATE) {
	bot_mode = RT_BOT_PLATE;
	bv = bu_bitv_new(pstate->face_count);
	for (i=0; i<pstate->face_count; i++) {
	    if (pstate->facemode[i] == POS_FRONT)
		BU_BITSET(bv, i);
	}
	bot_ip.face_mode = bv;
	bot_ip.thickness = (fastf_t *)bu_calloc(pstate->face_count, sizeof(fastf_t), "bot thickness");
	for (i=0; i<pstate->face_count; i++)
	    bot_ip.thickness[i] = pstate->thickness[i];
    } else if (pstate->mode == MODE_VOLUME) {
	bot_mode = RT_BOT_SOLID;
    } else {
	bu_bomb("invalid mode");
	bot_mode = RT_BOT_PLATE; /* silence warning */
    }

    bot_ip.mode = bot_mode;
    bot_ip.orientation = RT_BOT_UNORIENTED;
    bot_ip.bot_flags = 0;

    count = rt_bot_vertex_fuse(&bot_ip, &pstate->fpout->wdb_tol);
    if (count)
	bu_log("WARNING: %d duplicate vertices eliminated from group %d component %d\n", count, pstate->group_id, pstate->comp_id);

    count = rt_bot_face_fuse(&bot_ip);
    if (count)
	bu_log("WARNING: %d duplicate faces eliminated from group %d component %d\n", count, pstate->group_id, pstate->comp_id);

    name = make_solid_name(pstate, BOT, element_id, pstate->comp_id, pstate->group_id, 0);
    mk_bot(pstate->fpout, name, bot_mode, RT_BOT_UNORIENTED, 0, bot_ip.num_vertices, bot_ip.num_faces, bot_ip.vertices,
	   bot_ip.faces, bot_ip.thickness, bot_ip.face_mode);
    bu_free(name, "solid_name");

    if (pstate->mode == MODE_PLATE) {
	bu_free((char *)bot_ip.thickness, "bot pstate->thickness");
	bu_free((char *)bot_ip.face_mode, "bot face_mode");
    }
    bu_free((char *)bot_ip.vertices, "bot vertices");
    bu_free((char *)bot_ip.faces, "bot faces");

}


HIDDEN void
skip_section(struct conversion_state *pstate)
{
    b_off_t section_start;

    /* skip to start of next section */
    section_start = bu_ftell(pstate->fpin);
    if (section_start < 0) {
	bu_bomb("Error: couldn't get input file's current file position.");
    }

    if (get_line(pstate)) {
	while (pstate->line[0] && bu_strncmp(pstate->line, "SECTION", 7) &&
	       bu_strncmp(pstate->line, "HOLE", 4) &&
	       bu_strncmp(pstate->line, "WALL", 4) &&
	       bu_strncmp(pstate->line, "vehicle", 7))
	{
	    section_start = bu_ftell(pstate->fpin);
	    if (section_start < 0) {
		bu_bomb("Error: couldn't get input file's current file position.");
	    }
	    if (!get_line(pstate))
		break;
	}
    }
    /* seek to start of the section */
    bu_fseek(pstate->fpin, section_start, SEEK_SET);
}


/* cleanup from previous component and start a new one.
 * This is called with final == 1 when ENDDATA is found
 */
HIDDEN void
f4_do_section(struct conversion_state *pstate, int final)
{
    int found;
    struct name_tree *nm_ptr;

    if (pstate->gcv_options->debug_mode)
	bu_log("f4_do_section(%d): %s\n", final, pstate->line);

    if (pstate->pass)	/* doing geometry */ {
	if (pstate->region_id && !skip_region(pstate, pstate->region_id)) {
	    pstate->comp_count++;

	    if (pstate->bot)
		make_bot_object(pstate);
	}
	if (final && pstate->gcv_options->debug_mode) /* The ENDATA card has been found */
	    List_names(pstate);
    } else if (pstate->bot) {
	make_region_name(pstate, pstate->group_id, pstate->comp_id);
    }

    if (!final) {
	bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
	pstate->group_id = atoi(pstate->field);

	bu_strlcpy(pstate->field, &pstate->line[16], sizeof(pstate->field));
	pstate->comp_id = atoi(pstate->field);

	pstate->region_id = pstate->group_id * 1000 + pstate->comp_id;

	if (skip_region(pstate, pstate->region_id)) /* do not process this component */ {
	    skip_section(pstate);
	    return;
	}

	if (pstate->comp_id > 999) {
	    bu_log("Illegal component id number %d, changed to 999\n", pstate->comp_id);
	    pstate->comp_id = 999;
	}

	bu_strlcpy(pstate->field, &pstate->line[24], sizeof(pstate->field));

	switch (atoi(pstate->field)) {
	    case 1:
		pstate->mode = MODE_PLATE;
		break;

	    case 2:
		pstate->mode = MODE_VOLUME;
		break;

	    default:
		bu_log("Illegal mode (%d) for group %d component %d, using volume mode\n",
			atoi(pstate->field), pstate->group_id, pstate->comp_id);
		pstate->mode = MODE_VOLUME;
	}

	if (!pstate->pass) {
	    nm_ptr = Search_ident(pstate->name_root, pstate->region_id, &found);
	    if (found && nm_ptr->mode != pstate->mode) {
		bu_log("ERROR: second SECTION card found with different mode for component (group=%d, component=%d), conversion of this component will be incorrect!\n",
		       pstate->group_id, pstate->comp_id);
	    }
	}
    }

    pstate->bot = 0;
    pstate->face_count = 0;
}


HIDDEN int
f4_do_hex1(struct conversion_state *pstate)
{
    fastf_t thick=0.0;
    int pos;
    int pts[8];
    int element_id;
    int i;
    int cont1, cont2;

    bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
    element_id = atoi(pstate->field);

    if (!pstate->bot)
	pstate->bot = element_id;

    if (!pstate->pass) {
	if (!get_line(pstate)) {
	    bu_log("Unexpected EOF while reading continuation card for CHEX1\n");
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   pstate->group_id, pstate->comp_id, element_id);
	    return 0;
	}
	return 1;
    }

    if (pstate->faces == NULL) {
	pstate->faces = (int *)bu_malloc(GRID_BLOCK*3*sizeof(int), "faces");
	pstate->thickness = (fastf_t *)bu_malloc(GRID_BLOCK*sizeof(fastf_t), "thickness");
	pstate->facemode = (char *)bu_malloc(GRID_BLOCK*sizeof(char), "facemode");
	pstate->face_size = GRID_BLOCK;
	pstate->face_count = 0;
    }

    for (i=0; i<6; i++) {
	bu_strlcpy(pstate->field, &pstate->line[24 + i*8], sizeof(pstate->field));
	pts[i] = atoi(pstate->field);
    }

    bu_strlcpy(pstate->field, &pstate->line[72], sizeof(pstate->field));
    cont1 = atoi(pstate->field);

    if (!get_line(pstate)) {
	bu_log("Unexpected EOF while reading continuation card for CHEX1\n");
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
	       pstate->group_id, pstate->comp_id, element_id, cont1);
	return 0;
    }

    bu_strlcpy(pstate->field, pstate->line, sizeof(pstate->field));
    cont2 = atoi(pstate->field);

    if (cont1 != cont2) {
	bu_log("Continuation card numbers do not match for CHEX1 element (%d vs %d)\n", cont1, cont2);
	bu_log("\tskipping CHEX1 element: group_id = %d, comp_id = %d, element_id = %d\n",
	       pstate->group_id, pstate->comp_id, element_id);
	return 1;
    }

    bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
    pts[6] = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[16], sizeof(pstate->field));
    pts[7] = atoi(pstate->field);

    if (pstate->mode == MODE_PLATE) {
	bu_strlcpy(pstate->field, &pstate->line[56], sizeof(pstate->field));
	thick = atof(pstate->field) * 25.4;
	if (thick <= 0.0) {
	    bu_log("f4_do_hex1: illegal thickness (%f), skipping CHEX1 element\n", thick);
	    bu_log("\telement %d, component %d, group %d\n", element_id, pstate->comp_id, pstate->group_id);
	    return 1;
	}

	bu_strlcpy(pstate->field, &pstate->line[64], sizeof(pstate->field));
	pos = atoi(pstate->field);

	if (pos == 0)	/* use default */
	    pos = POS_FRONT;

	if (pos != POS_CENTER && pos != POS_FRONT) {
	    bu_log("f4_do_hex1: illegal position parameter (%d), must be 1 or 2, skipping CHEX1 element\n", pos);
	    bu_log("\telement %d, component %d, group %d\n", element_id, pstate->comp_id, pstate->group_id);
	    return 1;
	}
    } else if (pstate->mode == MODE_VOLUME) {
	pos =  POS_FRONT;
	thick = 0.0;
    } else {
	bu_bomb("invalid mode");
	pos = POS_FRONT; /* silence warning */
    }

    for (i=0; i<12; i++)
	f4_Add_bot_face(pstate, pts[hex_faces[i][0]], pts[hex_faces[i][1]], pts[hex_faces[i][2]], thick, pos);

    return 1;
}


HIDDEN int
f4_do_hex2(struct conversion_state *pstate)
{
    int pts[8];
    int element_id;
    int i;
    int cont1, cont2;
    point_t points[8];
    char *name = (char *)NULL;

    bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
    element_id = atoi(pstate->field);

    if (!pstate->pass) {
	make_region_name(pstate, pstate->group_id, pstate->comp_id);
	if (!get_line(pstate)) {
	    bu_log("Unexpected EOF while reading continuation card for CHEX2\n");
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   pstate->group_id, pstate->comp_id, element_id);
	    return 0;
	}
	return 1;
    }

    for (i=0; i<6; i++) {
	bu_strlcpy(pstate->field, &pstate->line[24 + i*8], sizeof(pstate->field));
	pts[i] = atoi(pstate->field);
    }

    bu_strlcpy(pstate->field, &pstate->line[72], sizeof(pstate->field));
    cont1 = atoi(pstate->field);

    if (!get_line(pstate)) {
	bu_log("Unexpected EOF while reading continuation card for CHEX2\n");
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
	       pstate->group_id, pstate->comp_id, element_id, cont1);
	return 0;
    }

    bu_strlcpy(pstate->field, pstate->line, sizeof(pstate->field));
    cont2 = atoi(pstate->field);

    if (cont1 != cont2) {
	bu_log("Continuation card numbers do not match for CHEX2 element (%d vs %d)\n", cont1, cont2);
	bu_log("\tskipping CHEX2 element: group_id = %d, comp_id = %d, element_id = %d\n",
	       pstate->group_id, pstate->comp_id, element_id);
	return 1;
    }

    bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
    pts[6] = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[16], sizeof(pstate->field));
    pts[7] = atoi(pstate->field);

    for (i=0; i<8; i++)
	VMOVE(points[i], pstate->grid_points[pts[i]]);

    name = make_solid_name(pstate, CHEX2, element_id, pstate->comp_id, pstate->group_id, 0);
    mk_arb8(pstate->fpout, name, &points[0][X]);
    bu_free(name, "solid_name");

    return 1;
}


HIDDEN void
Process_hole_wall(struct conversion_state *pstate)
{
    if (pstate->gcv_options->debug_mode)
	bu_log("Process_hole_wall\n");

    rewind(pstate->fpin);
    while (1) {
	if (!bu_strncmp(pstate->line, "HOLE", 4)) {
	    f4_do_hole_wall(pstate, HOLE);
	} else if (!bu_strncmp(pstate->line, "WALL", 4)) {
	    f4_do_hole_wall(pstate, WALL);
	} else if (!bu_strncmp(pstate->line, "COMPSPLT", 8)) {
	    f4_do_compsplt(pstate);
	} else if (!bu_strncmp(pstate->line, "SECTION", 7)) {
	    bu_strlcpy(pstate->field, &pstate->line[24], sizeof(pstate->field));

	    switch (atoi(pstate->field)) {
		case 1:
		    pstate->mode = MODE_PLATE;
		    break;

		case 2:
		    pstate->mode = MODE_VOLUME;
		    break;

		default:
		bu_log("Illegal mode (%d) for group %d component %d, using volume mode\n",
		       atoi(pstate->field), pstate->group_id, pstate->comp_id);
		pstate->mode = MODE_VOLUME;
	    }
	} else if (!bu_strncmp(pstate->line, "ENDDATA", 7)) {
	    break;
	}

	if (!get_line(pstate) || !pstate->line[0])
	    break;
    }

    if (pstate->gcv_options->debug_mode) {
	bu_log("At end of Process_hole_wall:\n");
	List_holes(pstate);
    }
}


HIDDEN void
f4_do_chgcomp(const struct conversion_state *pstate)
{

    if (!pstate->pass)
	return;

    if (!pstate->fp_muves)
	return;

    fprintf(pstate->fp_muves, "%s", pstate->line);
}


HIDDEN void
f4_do_cbacking(struct conversion_state *pstate)
{
    int gr1, co1, gr2, co2, material;
    fastf_t inthickness, probability;

    if (!pstate->pass)
	return;

    if (!pstate->fp_muves)
	return;

    bu_strlcpy(pstate->field, &pstate->line[8], sizeof(pstate->field));
    gr1 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[16], sizeof(pstate->field));
    co1 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[24], sizeof(pstate->field));
    gr2 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[32], sizeof(pstate->field));
    co2 = atoi(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[40], sizeof(pstate->field));
    inthickness = atof(pstate->field) * 25.4;

    bu_strlcpy(pstate->field, &pstate->line[48], sizeof(pstate->field));
    probability = atof(pstate->field);

    bu_strlcpy(pstate->field, &pstate->line[56], sizeof(pstate->field));
    material = atoi(pstate->field);

    fprintf(pstate->fp_muves, "CBACKING %d %d %g %g %d\n", gr1*1000+co1, gr2*1000+co2, inthickness, probability, material);
}


HIDDEN int
Process_input(struct conversion_state *pstate, int pass_number)
{
    if (pstate->gcv_options->debug_mode)
	bu_log("\n\nProcess_input(pass = %d)\n", pass_number);

    if (pass_number != 0 && pass_number != 1) {
	bu_bomb("Process_input: illegal pass_number");
    }

    pstate->region_id = 0;
    pstate->pass = pass_number;
    if (!get_line(pstate) || !pstate->line[0])
	bu_strlcpy(pstate->line, "ENDDATA", sizeof(pstate->line));
    while (1) {
	int ret = 1;

	if (!bu_strncmp(pstate->line, "vehicle", 7))
	    f4_do_vehicle(pstate);
	else if (!bu_strncmp(pstate->line, "HOLE", 4))
	    ;
	else if (!bu_strncmp(pstate->line, "WALL", 4))
	    ;
	else if (!bu_strncmp(pstate->line, "COMPSPLT", 8))
	    ;
	else if (!bu_strncmp(pstate->line, "CBACKING", 8))
	    f4_do_cbacking(pstate);
	else if (!bu_strncmp(pstate->line, "CHGCOMP", 7))
	    f4_do_chgcomp(pstate);
	else if (!bu_strncmp(pstate->line, "SECTION", 7))
	    f4_do_section(pstate, 0);
	else if (!bu_strncmp(pstate->line, "$NAME", 5))
	    f4_do_name(pstate);
	else if (!bu_strncmp(pstate->line, "$COMMENT", 8))
	    ;
	else if (!bu_strncmp(pstate->line, "GRID", 4))
	    ret = f4_do_grid(pstate);
	else if (!bu_strncmp(pstate->line, "CLINE", 5))
	    f4_do_cline(pstate);
	else if (!bu_strncmp(pstate->line, "CHEX1", 5))
	    ret = f4_do_hex1(pstate);
	else if (!bu_strncmp(pstate->line, "CHEX2", 5))
	    ret = f4_do_hex2(pstate);
	else if (!bu_strncmp(pstate->line, "CTRI", 4))
	    f4_do_tri(pstate);
	else if (!bu_strncmp(pstate->line, "CQUAD", 5))
	    f4_do_quad(pstate);
	else if (!bu_strncmp(pstate->line, "CCONE1", 6))
	    ret = f4_do_ccone1(pstate);
	else if (!bu_strncmp(pstate->line, "CCONE2", 6))
	    ret = f4_do_ccone2(pstate);
	else if (!bu_strncmp(pstate->line, "CCONE3", 6))
	    ret = f4_do_ccone3(pstate);
	else if (!bu_strncmp(pstate->line, "CSPHERE", 7))
	    f4_do_sphere(pstate);
	else if (!bu_strncmp(pstate->line, "ENDDATA", 7)) {
	    f4_do_section(pstate, 1);
	    break;
	} else {
	    bu_log("ERROR: skipping unrecognized data type\n%s\n", pstate->line);
	}

	if (!ret)
	    return 0;

	if (!get_line(pstate) || !pstate->line[0])
	    bu_strlcpy(pstate->line, "ENDDATA", sizeof(pstate->line));
    }

    if (pstate->gcv_options->debug_mode) {
	bu_log("At pass %d:\n", pstate->pass);
	List_names(pstate);
    }

    return 1;
}


HIDDEN void
make_region_list(struct conversion_state *pstate, const char *in_str)
{
    char *str = bu_strdup(in_str);
    char *ptr, *ptr2;

    pstate->region_list = (int *)bu_calloc(REGION_LIST_BLOCK, sizeof(int), "region_list");
    pstate->region_list_len = REGION_LIST_BLOCK;
    pstate->f4_do_skips = 0;

    ptr = strtok(str, ", ");
    while (ptr) {
	if ((ptr2=strchr(ptr, '-'))) {
	    int i, start, stop;

	    *ptr2 = '\0';
	    ptr2++;
	    start = atoi(ptr);
	    stop = atoi(ptr2);
	    for (i=start; i<=stop; i++) {
		if (pstate->f4_do_skips == pstate->region_list_len) {
		    pstate->region_list_len += REGION_LIST_BLOCK;
		    pstate->region_list = (int *)bu_realloc((char *)pstate->region_list, pstate->region_list_len*sizeof(int), "region_list");
		}
		pstate->region_list[pstate->f4_do_skips++] = i;
	    }
	} else {
	    if (pstate->f4_do_skips == pstate->region_list_len) {
		pstate->region_list_len += REGION_LIST_BLOCK;
		pstate->region_list = (int *)bu_realloc((char *)pstate->region_list, pstate->region_list_len*sizeof(int), "region_list");
	    }
	    pstate->region_list[pstate->f4_do_skips++] = atoi(ptr);
	}
	ptr = strtok((char *)NULL, ", ");
    }

    bu_free(str, "str");
}


HIDDEN void
make_regions(struct conversion_state *pstate)
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
    bu_ptbl_reset(&pstate->stack);
    ptr1 = pstate->name_root;
    while (1) {
	while (ptr1) {
	    PUSH(ptr1);
	    ptr1 = ptr1->rleft;
	}
	POP(name_tree, ptr1);
	if (!ptr1)
	    break;

	/* check if we are skipping some regions (but we might need all the holes) */
	if (skip_region(pstate, ptr1->region_id) && !is_a_hole(pstate, ptr1->region_id))
	    goto cont1;

	/* place all the solids for this ident in a "solids" combination */
	BU_LIST_INIT(&solids.l);
	bu_ptbl_reset(&pstate->stack2);
	ptr2 = pstate->name_root;
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
	if (mk_comb(pstate->fpout, solids_name, &solids.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 1, 1))
	    bu_log("Failed to make combination of solids (%s)!\n\tRegion %s is in ERROR!\n",
		   solids_name, ptr1->name);

	/* hole components do not get made into regions */
	if (is_a_hole(pstate, ptr1->region_id)) {
	    /* just add it to the "holes" group */
	    if (mk_addmember(solids_name, &holes.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
		bu_log("make_regions: mk_addmember failed to add %s to holes group\n", ptr1->name);
	    goto cont1;
	}

	hptr = pstate->hole_root;
	while (hptr && hptr->group * 1000 + hptr->component != ptr1->region_id)
	    hptr = hptr->next;

	lptr = NULL;
	if (hptr != NULL) {
	    lptr = hptr->holes;
	}

	splt = pstate->compsplt_root;
	while (splt && splt->ident_to_split != ptr1->region_id)
	    splt = splt->next;

	pstate->mode = ptr1->mode;
	if (pstate->gcv_options->debug_mode)
	    bu_log("Build region for %s %d, mode = %d\n", ptr1->name, ptr1->region_id, pstate->mode);

	if (splt) {
	    vect_t norm;
	    int found;

	    /* make a halfspace */
	    VSET(norm, 0.0, 0.0, 1.0);
	    snprintf(splt_name, MAX_LINE_SIZE, "splt_%d.s", ptr1->region_id);
	    mk_half(pstate->fpout, splt_name, norm, splt->z);

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
	    make_region(pstate, pstate->fpout, &region, ptr1->name, ptr1->region_id, get_fast4_color(pstate, ptr1->region_id));

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
	    ptr2 = Search_ident(pstate->name_root, splt->new_ident, &found);
	    if (found) {
		make_region(pstate, pstate->fpout, &region, ptr2->name, splt->new_ident, get_fast4_color(pstate, splt->new_ident));
	    } else {
		sprintf(reg_name, "comp_%d.r", splt->new_ident);
		make_region(pstate, pstate->fpout, &region, reg_name, splt->new_ident, get_fast4_color(pstate, splt->new_ident));
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
	    make_region(pstate, pstate->fpout, &region, ptr1->name, ptr1->region_id, get_fast4_color(pstate, ptr1->region_id));
	}
    cont1:
	ptr1 = ptr1->rright;
    }

    if (BU_LIST_NON_EMPTY(&holes.l)) {
	/* build a "holes" group */
	if (mk_comb(pstate->fpout, "holes", &holes.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 1, 1))
	    bu_log("Failed to make holes group!\n");
    }
}


#define COLOR_LINE_LEN 256

HIDDEN void
read_fast4_colors(struct conversion_state *pstate, const char *color_file)
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
	BU_LIST_APPEND(&pstate->HeadColor.l, &color->l);
    }
    fclose(fp);
}


struct
fastgen4_read_options
{
    const char *colors_path;
    const char *muves_path;
    const char *plot_path;
    const char *section_list_str;
};


HIDDEN void
fastgen4_create_opts(struct bu_opt_desc **options_desc, void **dest_options_data)
{
    struct fastgen4_read_options *options_data;

    BU_ALLOC(options_data, struct fastgen4_read_options);
    *dest_options_data = options_data;
    *options_desc = (struct bu_opt_desc *)bu_malloc(5 * sizeof(struct bu_opt_desc), "options_desc");

    BU_OPT((*options_desc)[0], NULL, "colors", "path", bu_opt_str, &options_data->colors_path,
	    "path to file specifying component colors");
    BU_OPT((*options_desc)[1], NULL, "muves", "path", bu_opt_str, &options_data->muves_path,
	    "create a MUVES input file containing any CHGCOMP and CBACKING components");
    BU_OPT((*options_desc)[2], NULL, "plot", "path", bu_opt_str, &options_data->plot_path,
	    "create a libplot3 file of all CTRI and CQUAD elements processed");
    BU_OPT((*options_desc)[3], NULL, "sections", "list", bu_opt_str, &options_data->section_list_str,
	    "process only a list (3001, 4082, 5347) or a range (2315-3527) of SECTION IDs");
    BU_OPT_NULL((*options_desc)[4]);
}


HIDDEN void
fastgen4_free_opts(void *options_data)
{
    bu_free(options_data, "options_data");
}


HIDDEN int
fastgen4_read(struct gcv_context *context, const struct gcv_opts *gcv_options, const void *options_data, const char *source_path)
{
    const struct fastgen4_read_options * const fg4_read_options =
	(struct fastgen4_read_options *)options_data;

    struct conversion_state state;

    fg4_zero_conversion_state(&state);
    state.gcv_options = gcv_options;

    state.fpout = context->dbip->dbi_wdbp;

    if (fg4_read_options->muves_path)
	if ((state.fp_muves=fopen(fg4_read_options->muves_path, "wb")) == (FILE *)NULL) {
	    bu_log("Unable to open MUVES file (%s)\n\tno MUVES file created\n", fg4_read_options->muves_path);
	    fg4_free_conversion_state(&state);
	    return 0;
	}

    if ((state.fpin=fopen(source_path, "rb")) == (FILE *)NULL) {
	bu_log("Cannot open FASTGEN4 file (%s)\n", source_path);
	fg4_free_conversion_state(&state);
	return 0;
    }

    if (fg4_read_options->plot_path) {
	if ((state.fp_plot=fopen(fg4_read_options->plot_path, "wb")) == NULL) {
	    bu_log("Cannot open plot file (%s)\n", fg4_read_options->plot_path);
	    fg4_free_conversion_state(&state);
	    return 0;
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

    if (fg4_read_options->colors_path)
	read_fast4_colors(&state, fg4_read_options->colors_path);

    if (fg4_read_options->section_list_str) {
	make_region_list(&state, fg4_read_options->section_list_str);
    }

    if (state.gcv_options->verbosity_level)
	bu_log("Scanning for HOLE, WALL, and COMPSPLT cards...\n");

    Process_hole_wall(&state);

    rewind(state.fpin);

    if (state.gcv_options->verbosity_level)
	bu_log("Building component names....\n");

    if (Process_input(&state, 0)) {
	rewind(state.fpin);

	/* Make an ID record if no vehicle card was found */
	if (!state.vehicle[0])
	    mk_id_units(state.fpout, source_path, "in");

	if (state.gcv_options->verbosity_level)
	    bu_log("Building components....\n");

	if (Process_input(&state, 1)) {
	    if (state.gcv_options->verbosity_level)
		bu_log("Building regions and groups....\n");

	    /* make regions */
	    make_regions(&state);

	    /* make groups */
	    f4_do_groups(&state);

	    if (state.gcv_options->debug_mode)
		List_holes(&state);

	    if (state.gcv_options->verbosity_level)
		bu_log("%d components converted\n", state.comp_count);
	}
    }

    fg4_free_conversion_state(&state);
    return 1;
}


static const struct gcv_filter gcv_conv_fastgen4_read = {
    "FASTGEN4 Reader", GCV_FILTER_READ, BU_MIME_MODEL_VND_FASTGEN, NULL,
    fastgen4_create_opts, fastgen4_free_opts, fastgen4_read
};


extern const struct gcv_filter gcv_conv_fastgen4_write;
static const struct gcv_filter * const filters[] = {&gcv_conv_fastgen4_read, &gcv_conv_fastgen4_write, NULL};

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
