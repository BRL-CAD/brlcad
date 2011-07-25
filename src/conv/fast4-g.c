/*                       F A S T 4 - G . C
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
/** @file conv/fast4-g.c
 *
 * Program to convert the FASTGEN4 format to BRL-CAD.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "bio.h"

/* interface headers */
#include "db.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "plot3.h"


/* convenient macro for building regions */
#define	MK_REGION(fp, headp, name, r_id, rgb) {\
    if (mode == 1) {\
	if (!quiet)\
	    bu_log("Making region: %s (PLATE)\n", name); \
	    mk_comb(fp, name, &((headp)->l), 'P', (char *)NULL, (char *)NULL, rgb, r_id, 0, 1, 100, 0, 0, 0); \
    } else if (mode == 2) {\
	if (!quiet) \
	    bu_log("Making region: %s (VOLUME)\n", name); \
	    mk_comb(fp, name, &((headp)->l), 'V', (char *)NULL, (char *)NULL, rgb, r_id, 0, 1, 100, 0, 0, 0); \
    } else {\
	bu_log("Illegal mode (%d), while trying to make region (%s)\n", mode, name);\
	bu_log("\tRegion not made!\n");\
    }\
}

#define	PUSH(ptr)	bu_ptbl_ins(&stack, (long *)ptr)
#define POP(structure, ptr) { \
    if (BU_PTBL_END(&stack) == 0) \
	ptr = (struct structure *)NULL; \
    else { \
	ptr = (struct structure *)BU_PTBL_GET(&stack, BU_PTBL_END(&stack)-1); \
	bu_ptbl_rm(&stack, (long *)ptr); \
    } \
}
#define	PUSH2(ptr)	bu_ptbl_ins(&stack2, (long *)ptr)
#define POP2(structure, ptr)	{ \
    if (BU_PTBL_END(&stack2) == 0) \
	ptr = (struct structure *)NULL; \
    else { \
	ptr = (struct structure *)BU_PTBL_GET(&stack2, BU_PTBL_END(&stack2)-1); \
	bu_ptbl_rm(&stack2, (long *)ptr); \
    } \
}

#define	NAME_TREE_MAGIC	0x55555555
#define CK_TREE_MAGIC(ptr) {\
    if (!ptr)\
	bu_log("ERROR: Null name_tree pointer, file=%s, line=%d\n", __FILE__, __LINE__);\
    else if (ptr->magic != NAME_TREE_MAGIC)\
	bu_log("ERROR: bad name_tree pointer (%p), file=%s, line=%d\n", (void *)ptr, __FILE__, __LINE__);\
}


#define	PLATE_MODE	1
#define	VOLUME_MODE	2

#define	POS_CENTER	1	/* face positions for facets */
#define	POS_FRONT	2

#define	END_OPEN	1	/* End closure codes for cones */
#define	END_CLOSED	2

#define	GRID_BLOCK	256	/* allocate space for grid points in blocks of 256 points */

#define	CLINE		'l'
#define	CHEX1		'p'
#define	CHEX2		'b'
#define	CTRI		't'
#define	CQUAD		'q'
#define	CCONE1		'c'
#define	CCONE2		'd'
#define	CCONE3		'e'
#define	CSPHERE		's'
#define	NMG		'n'
#define	BOT		't'
#define	COMPSPLT	'h'

#define HOLE 1
#define WALL 2
#define INT_LIST_BLOCK		256	/* Number of int_list array slots to allocate */
#define	MAX_LINE_SIZE			128	/* Length of char array for input line */
#define	REGION_LIST_BLOCK	256	/* initial length of array of region ids to process */


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
    long magic;
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

static struct fast4_color HeadColor;

static char	line[MAX_LINE_SIZE+1];		/* Space for input line */
static FILE	*fpin;			/* Input FASTGEN4 file pointer */
static struct rt_wdb *fpout;		/* Output BRL-CAD file pointer */
static FILE	*fp_plot=NULL;		/* file for plot output */
static FILE	*fp_muves=NULL;		/* file for MUVES data, output CHGCOMP and CBACKING data */
static int	grid_size;		/* Number of points that will fit in current grid_pts array */
static int	max_grid_no=0;		/* Maximum grid number used */
static int	mode=0;			/* Plate mode (1) or volume mode (2), of current component */
static int	group_id=(-1);		/* Group identification number from SECTION card */
static int	comp_id=(-1);		/* Component identification number from SECTION card */
static int	region_id=0;		/* Region id number (group id no X 1000 + component id no) */
static char	field[9];		/* Space for storing one field from an input line */
static char	vehicle[17];		/* Title for BRL-CAD model from VEHICLE card */
static int	name_count;		/* Count of number of times this name_name has been used */
static int	pass;			/* Pass number (0 -> only make names, 1-> do geometry) */
static int	bot=0;			/* Flag: >0 -> There are BOT's in current component */
static int	warnings=0;		/* Flag: >0 -> Print warning messages */
static int	debug=0;		/* Debug flag */
static int	rt_debug=0;		/* RT_G_DEBUG */
static int	quiet=0;		/* flag to not blather */
static int	comp_count=0;		/* Count of components in FASTGEN4 file */
static int	f4_do_skips=0;		/* flag indicating that not all components will be processed */
static int	*region_list;		/* array of region_ids to be processed */
static int	region_list_len=0;	/* actual length of the malloc'd region_list array */
static int	f4_do_plot=0;		/* flag indicating plot file should be created */
static struct wmember  group_head[11];	/* Lists of regions for groups */
static struct wmember  hole_head;	/* List of regions used as holes (not solid parts of model) */
static struct bu_ptbl stack;		/* Stack for traversing name_tree */
static struct bu_ptbl stack2;		/* Stack for traversing name_tree */
static fastf_t	min_radius;		/* minimum radius for TGC solids */

static int	*faces=NULL;	/* one triplet per face indexing three grid points */
static fastf_t	*thickness;	/* thickness of each face */
static char	*facemode;	/* mode for each face */
static int	face_size=0;	/* actual length of above arrays */
static int	face_count=0;	/* number of faces in above arrays */

/*static int	*int_list;*/		/* Array of integers */
/*static int	int_list_count=0;*/	/* Number of ints in above array */
/*static int	int_list_length=0;*/	/* Length of int_list array */

static point_t *grid_points = NULL;

static void
usage() {
    bu_log("Usage:\n\tfast4-g [-dwq] [-c component_list] [-m muves_file] [-o plot_file] [-b BU_DEBUG_FLAG] [-x RT_DEBUG_FLAG] fastgen4_bulk_data_file output.g\n");
    bu_log("	d - print debugging info\n");
    bu_log("	q - quiet mode (don't say anyhing except error messages\n");
    bu_log("	w - print warnings about creating default names\n");
    bu_log("	c - process only the listed region ids, may be a list (3001, 4082, 5347) or a range (2314-3527)\n");
    bu_log("	m - create a MUVES input file containing CHGCOMP and CBACKING elements\n");
    bu_log("	o - create a 'plot_file' containing a libplot3 plot file of all CTRI and CQUAD elements processed\n");
    bu_log("	b - set LIBBU debug flag\n");
    bu_log("	x - set RT debug flag\n");

    bu_exit(1, NULL);
}


static int
get_line(void)
{
    int len;
    struct bu_vls buffer;

    bu_vls_init(&buffer);
    len = bu_vls_gets(&buffer, fpin);

    /* eof? */
    if (len < 0)
	return 0;

    if (len > MAX_LINE_SIZE)
	bu_log("WARNING: long line truncated\n");

    memset((void *)line, 0, MAX_LINE_SIZE);
    snprintf(line, MAX_LINE_SIZE, "%s", bu_vls_addr(&buffer));

    bu_vls_free(&buffer);

    return len >= 0;
}


static unsigned char *
get_fast4_color(int r_id) {
    struct fast4_color *fcp;

    for (BU_LIST_FOR(fcp, fast4_color, &HeadColor.l)) {
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
plot_tri(int pt1, int pt2, int pt3)
{
    pdv_3move(fp_plot, grid_points[pt1]);
    pdv_3cont(fp_plot, grid_points[pt2]);
    pdv_3cont(fp_plot, grid_points[pt3]);
    pdv_3cont(fp_plot, grid_points[pt1]);
}

static void
Check_names(void)
{
    struct name_tree *ptr;

    if (!name_root)
	return;

    bu_ptbl_reset(&stack);

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

    /* alpabetical order */
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
Search_names(struct name_tree *root, char *name, int *found)
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

	diff = reg_id -  ptr->region_id;

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
List_names(void)
{
    struct name_tree *ptr;

    bu_ptbl_reset(&stack);

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
Insert_region_name(char *name, int reg_id)
{
    struct name_tree *nptr_model, *rptr_model;
    struct name_tree *new_ptr;
    int foundn, foundr;
    int diff;

    if (debug)
	bu_log("Insert_region_name(name=%s, reg_id=%d\n", name, reg_id);

    rptr_model = Search_ident(name_root, reg_id, &foundr);
    nptr_model = Search_names(name_root, name, &foundn);

    if (foundn && foundr)
	return;

    if (foundn != foundr) {
	bu_log("Insert_region_name: name %s ident %d\n\tfound name is %d\n\tfound ident is %d\n",
	       name, reg_id, foundn, foundr);
	List_names();
	bu_exit(1, "\tCannot insert new node\n");
    }

    /* Add to tree for entire model */
    new_ptr = (struct name_tree *)bu_malloc(sizeof(struct name_tree), "Insert_region_name: new_ptr");
    new_ptr->rleft = (struct name_tree *)NULL;
    new_ptr->rright = (struct name_tree *)NULL;
    new_ptr->nleft = (struct name_tree *)NULL;
    new_ptr->nright = (struct name_tree *)NULL;
    new_ptr->region_id = reg_id;
    new_ptr->mode = mode;
    new_ptr->inner = -1;
    new_ptr->in_comp_group = 0;
    new_ptr->name = bu_strdup(name);
    new_ptr->magic = NAME_TREE_MAGIC;

    if (!name_root) {
	name_root = new_ptr;
    } else {
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
    Check_names();
    if (RT_G_DEBUG&DEBUG_MEM_FULL &&  bu_mem_barriercheck())
	bu_log("ERROR: bu_mem_barriercheck failed in Insert_region_name\n");
}


static char *
find_region_name(int g_id, int c_id)
{
    struct name_tree *ptr;
    int reg_id;
    int found;

    reg_id = g_id * 1000 + c_id;

    if (debug)
	bu_log("find_region_name(g_id=%d, c_id=%d), reg_id=%d\n", g_id, c_id, reg_id);

    ptr = Search_ident(name_root, reg_id, &found);

    if (found)
	return ptr->name;
    else
	return (char *)NULL;
}


static char *
make_unique_name(char *name)
{
    struct bu_vls vls;
    int found;

    /* make a unique name from what we got off the $NAME card */

    (void)Search_names(name_root, name, &found);
    if (!found)
	return bu_strdup(name);

    bu_vls_init(&vls);

    while (found) {
	bu_vls_trunc(&vls, 0);
	bu_vls_printf(&vls, "%s_%d", name, name_count);
	(void)Search_names(name_root, bu_vls_addr(&vls), &found);
    }
    if (RT_G_DEBUG&DEBUG_MEM_FULL &&  bu_mem_barriercheck())
	bu_log("ERROR: bu_mem_barriercheck failed in make_unique_name\n");

    return bu_vls_strgrab(&vls);
}


static void
make_region_name(int g_id, int c_id)
{
    int r_id;
    const char *tmp_name;
    char *name;

    r_id = g_id * 1000 + c_id;

    if (debug)
	bu_log("make_region_name(g_id=%d, c_id=%d)\n", g_id, c_id);

    tmp_name = find_region_name(g_id, c_id);
    if (tmp_name)
	return;

    /* create a new name */
    name = (char *)bu_malloc(MAX_LINE_SIZE, "make_region_name");
    snprintf(name, MAX_LINE_SIZE, "comp_%04d.r", r_id);

    make_unique_name(name);

    Insert_region_name(name, r_id);
}

static char *
get_solid_name(char type, int element_id, int c_id, int g_id, int inner)
{
    int reg_id;
    struct bu_vls vls;

    reg_id = g_id * 1000 + c_id;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "%d.%d.%c%d", reg_id, element_id, type, inner);

    return bu_vls_strgrab(&vls);
}


static void
Insert_name(struct name_tree **root, char *name, int inner)
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

    new_ptr = (struct name_tree *)bu_malloc(sizeof(struct name_tree), "Insert_name: new_ptr");

    new_ptr->name = bu_strdup(name);
    new_ptr->nleft = (struct name_tree *)NULL;
    new_ptr->nright = (struct name_tree *)NULL;
    new_ptr->rleft = (struct name_tree *)NULL;
    new_ptr->rright = (struct name_tree *)NULL;
    new_ptr->region_id = (-region_id);
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
    if (RT_G_DEBUG&DEBUG_MEM_FULL &&  bu_mem_barriercheck())
	bu_log("ERROR: bu_mem_barriercheck failed in Insert_name\n");
}


static char *
make_solid_name(char type, int element_id, int c_id, int g_id, int inner)
{
    char *name;

    name = get_solid_name(type, element_id, c_id, g_id, inner);

    Insert_name(&name_root, name, inner);

    return name;
}


/*
  static void
  insert_int(int in)
  {
  int i;

  for (i=0; i<int_list_count; i++) {
  if (int_list[i] == in)
  return;
  }

  if (int_list_count == int_list_length) {
  if (int_list_length == 0)
  int_list = (int *)bu_malloc(INT_LIST_BLOCK*sizeof(int), "insert_id: int_list");
  else
  int_list = (int *)bu_realloc((char *)int_list, (int_list_length + INT_LIST_BLOCK)*sizeof(int), "insert_id: int_list");
  int_list_length += INT_LIST_BLOCK;
  }

  int_list[int_list_count] = in;
  int_list_count++;

  if (RT_G_DEBUG&DEBUG_MEM_FULL &&  bu_mem_barriercheck())
  bu_log("ERROR: bu_mem_barriercheck failed in insert_int\n");
  }
*/


/*
  static void
  Subtract_holes(struct wmember *head, int comp_id, int group_id)
  {
  struct holes *hole_ptr;
  struct hole_list *list_ptr;

  if (debug)
  bu_log("Subtract_holes(comp_id=%d, group_id=%d)\n", comp_id, group_id);

  hole_ptr = hole_root;
  while (hole_ptr) {
  if (hole_ptr->group == group_id && hole_ptr->component == comp_id) {
  list_ptr = hole_ptr->holes;
  while (list_ptr) {
  struct name_tree *ptr;
  int reg_id;

  reg_id = list_ptr->group * 1000 + list_ptr->component;
  ptr = name_root;
  while (ptr && ptr->region_id != reg_id) {
  int diff;

  diff = reg_id - ptr->region_id;
  if (diff > 0)
  ptr = ptr->rright;
  else if (diff < 0)
  ptr = ptr->rleft;
  }

  bu_ptbl_reset(&stack);

  while (ptr && ptr->region_id == reg_id) {

  while (ptr && ptr->region_id == reg_id) {
  PUSH(ptr);
  ptr = ptr->rleft;
  }
  POP(name_tree, ptr);
  if (!ptr ||  ptr->region_id != reg_id)
  break;

  if (debug)
  bu_log("\tSubtracting %s\n", ptr->name);

  if (mk_addmember(ptr->name, &(head->l), NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
  bu_exit(1, "Subtract_holes: mk_addmember failed\n");

  ptr = ptr->rright;
  }

  list_ptr = list_ptr->next;
  }
  break;
  }
  hole_ptr = hole_ptr->next;
  }
  if (RT_G_DEBUG&DEBUG_MEM_FULL &&  bu_mem_barriercheck())
  bu_log("ERROR: bu_mem_barriercheck failed in subtract_holes\n");
  }
*/


static void
f4_do_compsplt(void)
{
    int gr, co, gr1,  co1;
    fastf_t z;
    struct compsplt *splt;

    bu_strlcpy(field, &line[8], sizeof(field));
    gr = atoi(field);

    bu_strlcpy(field, &line[16], sizeof(field));
    co = atoi(field);

    bu_strlcpy(field, &line[24], sizeof(field));
    gr1 = atoi(field);

    bu_strlcpy(field, &line[32], sizeof(field));
    co1 = atoi(field);

    bu_strlcpy(field, &line[40], sizeof(field));
    z = atof(field) * 25.4;

    if (compsplt_root == NULL) {
	compsplt_root = (struct compsplt *)bu_calloc(1, sizeof(struct compsplt), "compsplt_root");
	splt = compsplt_root;
    } else {
	splt = compsplt_root;
	while (splt->next)
	    splt = splt->next;
	splt->next = (struct compsplt *)bu_calloc(1, sizeof(struct compsplt), "compsplt_root");
	splt = splt->next;
    }
    splt->next = (struct compsplt *)NULL;
    splt->ident_to_split = gr * 1000 + co;
    splt->new_ident = gr1 * 1000 + co1;
    splt->z = z;
    make_region_name(gr1, co1);
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
Add_stragglers_to_groups(void)
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

	if (!ptr->in_comp_group && ptr->region_id > 0 && !is_a_hole(ptr->region_id)) {
	    /* add this component to a series */
	    (void)mk_addmember(ptr->name, &group_head[ptr->region_id/1000].l, NULL, WMOP_UNION);
	    ptr->in_comp_group = 1;
	}

	ptr = ptr->rright;
    }
}


static void
f4_do_groups(void)
{
    int group_no;
    struct wmember head_all;

    if (debug)
	bu_log("f4_do_groups\n");

    BU_LIST_INIT(&head_all.l);

    Add_stragglers_to_groups();

    for (group_no=0; group_no < 11; group_no++) {
	char name[MAX_LINE_SIZE] = {0};

	if (BU_LIST_IS_EMPTY(&group_head[group_no].l))
	    continue;

	snprintf(name, MAX_LINE_SIZE, "%dxxx_series", group_no);
	mk_lfcomb(fpout, name, &group_head[group_no], 0);

	if (mk_addmember(name, &head_all.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
	    bu_log("f4_do_groups: mk_addmember failed to add %s to group all\n", name);
    }

    if (BU_LIST_NON_EMPTY(&head_all.l))
	mk_lfcomb(fpout, "all", &head_all, 0);

    if (BU_LIST_NON_EMPTY(&hole_head.l))
	mk_lfcomb(fpout, "holes", &hole_head, 0);
}


static void
f4_do_name(void)
{
    int i, j;
    int g_id;
    int c_id;
    char comp_name[MAX_LINE_SIZE] = {0}; /* should only use 25 chars */
    char tmp_name[MAX_LINE_SIZE] = {0}; /* should only use 25 chars */

    if (pass)
	return;

    if (debug)
	bu_log("f4_do_name: %s\n", line);

    bu_strlcpy(field, &line[8], sizeof(field));
    g_id = atoi(field);

    if (g_id != group_id) {
	bu_log("$NAME card for group %d in section for group %d ignored\n", g_id, group_id);
	bu_log("%s\n", line);
	return;
    }

    bu_strlcpy(field, &line[16], sizeof(field));
    c_id = atoi(field);

    if (c_id != comp_id) {
	bu_log("$NAME card for component %d in section for component %d ignored\n", c_id, comp_id);
	bu_log("%s\n", line);
	return;
    }

    /* skip leading blanks */
    i = 56;
    while ((size_t)i < sizeof(comp_name) && isspace(line[i]))
	i++;

    if (i == sizeof(comp_name))
	return;

    bu_strlcpy(comp_name, &line[i], sizeof(comp_name) - i);

    /* eliminate trailing blanks */
    i = sizeof(comp_name) - i;
    while ( --i >= 0 && isspace(comp_name[i]))
	comp_name[i] = '\0';

    /* copy comp_name to tmp_name while replacing white space with "_" */
    i = (-1);
    j = (-1);

    /* copy */
    while (comp_name[++i] != '\0') {
	if (isspace(comp_name[i]) || comp_name[i] == '/') {
	    if (j == (-1) || tmp_name[j] != '_')
		tmp_name[++j] = '_';
	} else {
	    tmp_name[++j] = comp_name[i];
	}
    }
    tmp_name[++j] = '\0';

    /* reserve this name for group name */
    make_unique_name(tmp_name);
    Insert_region_name(tmp_name, region_id);

    name_count = 0;
    if (RT_G_DEBUG&DEBUG_MEM_FULL &&  bu_mem_barriercheck())
	bu_log("ERROR: bu_mem_barriercheck failed in f4_do_name\n");
}


static void
f4_do_grid(void)
{
    int grid_no;
    fastf_t x, y, z;

    if (!pass)	/* not doing geometry yet */
	return;

    if (RT_G_DEBUG&DEBUG_MEM_FULL &&  bu_mem_barriercheck())
	bu_log("ERROR: bu_mem_barriercheck failed at start of f4_do_grid\n");

    bu_strlcpy(field, &line[8], sizeof(field));
    grid_no = atoi(field);

    if (grid_no < 1) {
	bu_exit(1, "ERROR: bad grid id number = %d\n", grid_no);
    }

    bu_strlcpy(field, &line[24], sizeof(field));
    x = atof(field);

    bu_strlcpy(field, &line[32], sizeof(field));
    y = atof(field);

    bu_strlcpy(field, &line[40], sizeof(field));
    z = atof(field);

    while (grid_no > grid_size - 1) {
	grid_size += GRID_BLOCK;
	grid_points = (point_t *)bu_realloc((char *)grid_points, grid_size * sizeof(point_t), "fast4-g: grid_points");
    }

    VSET(grid_points[grid_no], x*25.4, y*25.4, z*25.4);

    if (grid_no > max_grid_no)
	max_grid_no = grid_no;
    if (RT_G_DEBUG&DEBUG_MEM_FULL &&  bu_mem_barriercheck())
	bu_log("ERROR: bu_mem_barriercheck failed at end of f4_do_grid\n");
}


static void
f4_do_sphere(void)
{
    int element_id;
    int center_pt;
    fastf_t thick;
    fastf_t radius;
    fastf_t inner_radius;
    char *name = (char *)NULL;
    struct wmember sphere_group;

    if (!pass) {
	make_region_name(group_id, comp_id);
	return;
    }

    bu_strlcpy(field, &line[8], sizeof(field));
    element_id = atoi(field);

    bu_strlcpy(field, &line[24], sizeof(field));
    center_pt = atoi(field);

    bu_strlcpy(field, &line[56], sizeof(field));
    thick = atof(field) * 25.4;

    bu_strlcpy(field, &line[64], sizeof(field));
    radius = atof(field) * 25.4;

    if (radius <= 0.0) {
	bu_log("f4_do_sphere: illegal radius (%f), skipping sphere\n", radius);
	bu_log("\telement %d, component %d, group %d\n", element_id, comp_id, group_id);
	return;
    }

    if (center_pt < 1 || center_pt > max_grid_no) {
	bu_log("f4_do_sphere: illegal grid number for center point %d, skipping sphere\n", center_pt);
	bu_log("\telement %d, component %d, group %d\n", element_id, comp_id, group_id);
	return;
    }

    BU_LIST_INIT(&sphere_group.l);

    if (mode == VOLUME_MODE) {
	name = make_solid_name(CSPHERE, element_id, comp_id, group_id, 0);
	mk_sph(fpout, name, grid_points[center_pt], radius);
	bu_free(name, "solid_name");
    } else if (mode == PLATE_MODE) {
	name = make_solid_name(CSPHERE, element_id, comp_id, group_id, 1);
	mk_sph(fpout, name, grid_points[center_pt], radius);

	BU_LIST_INIT(&sphere_group.l);

	if (mk_addmember(name, &sphere_group.l, NULL, WMOP_UNION) == (struct wmember *)NULL) {
	    bu_exit(1, "f4_do_sphere: Error in adding %s to sphere group\n", name);
	}
	bu_free(name, "solid_name");

	inner_radius = radius - thick;
	if (thick > 0.0 && inner_radius <= 0.0) {
	    bu_log("f4_do_sphere: illegal thickness (%f), skipping inner sphere\n", thick);
	    bu_log("\telement %d, component %d, group %d\n", element_id, comp_id, group_id);
	    return;
	}

	name = make_solid_name(CSPHERE, element_id, comp_id, group_id, 2);
	mk_sph(fpout, name, grid_points[center_pt], inner_radius);

	if (mk_addmember(name, &sphere_group.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL) {
	    bu_exit(1, "f4_do_sphere: Error in subtracting %s from sphere region\n", name);
	}
	bu_free(name, "solid_name");

	name = make_solid_name(CSPHERE, element_id, comp_id, group_id, 0);
	mk_comb(fpout, name, &sphere_group.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 1, 1);
	bu_free(name, "solid_name");
    }
}


static void
f4_do_vehicle(void)
{
    if (pass)
	return;

    bu_strlcpy(vehicle, &line[8], sizeof(vehicle));
    mk_id_units(fpout, vehicle, "in");
}


static void
f4_do_cline(void)
{
    int element_id;
    int pt1, pt2;
    fastf_t thick;
    fastf_t radius;
    vect_t height;
    char *name;

    if (debug)
	bu_log("f4_do_cline: %s\n", line);

    if (!pass) {
	make_region_name(group_id, comp_id);
	return;
    }

    bu_strlcpy(field, &line[8], sizeof(field));
    element_id = atoi(field);

    bu_strlcpy(field, &line[24], sizeof(field));
    pt1 = atoi(field);

    if (pass && (pt1 < 1 || pt1 > max_grid_no)) {
	bu_log("Illegal grid point (%d) in CLINE, skipping\n", pt1);
	bu_log("\telement %d, component %d, group %d\n", element_id, comp_id, group_id);
	return;
    }

    bu_strlcpy(field, &line[32], sizeof(field));
    pt2 = atoi(field);

    if (pass && (pt2 < 1 || pt2 > max_grid_no)) {
	bu_log("Illegal grid point in CLINE (%d), skipping\n", pt2);
	bu_log("\telement %d, component %d, group %d\n", element_id, comp_id, group_id);
	return;
    }

    if (pt1 == pt2) {
	bu_log("Ilegal grid points in CLINE (%d and %d), skipping\n", pt1, pt2);
	bu_log("\telement %d, component %d, group %d\n", element_id, comp_id, group_id);
	return;
    }

    bu_strlcpy(field, &line[56], sizeof(field));
    thick = atof(field) * 25.4;

    bu_strlcpy(field, &line[64], sizeof(field));
    radius = atof(field) * 25.4;

    VSUB2(height, grid_points[pt2], grid_points[pt1]);

    name = make_solid_name(CLINE, element_id, comp_id, group_id, 0);
    mk_cline(fpout, name, grid_points[pt1], height, radius, thick);
    bu_free(name, "solid_name");
}


static void
f4_do_ccone1(void)
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

    bu_strlcpy(field, &line[8], sizeof(field));
    element_id = atoi(field);

    if (!pass) {
	make_region_name(group_id, comp_id);
	if (!get_line()) {
	    bu_log("Unexpected EOF while reading continuation card for CCONE1\n");
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   group_id, comp_id, element_id);
	    bu_exit(1, "ERROR: unexpected end-of-file");
	}
	return;
    }

    bu_strlcpy(field, &line[24], sizeof(field));
    pt1 = atoi(field);

    bu_strlcpy(field, &line[32], sizeof(field));
    pt2 = atoi(field);

    bu_strlcpy(field, &line[56], sizeof(field));
    thick = atof(field) * 25.4;

    bu_strlcpy(field, &line[64], sizeof(field));
    r1 = atof(field) * 25.4;

    bu_strlcpy(field, &line[72], sizeof(field));
    c1 = atoi(field);

    if (!get_line()) {
	bu_log("Unexpected EOF while reading continuation card for CCONE1\n");
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
	       group_id, comp_id, element_id, c1);
	bu_exit(1, "ERROR: unexpected end-of-file");
    }

    bu_strlcpy(field, line, sizeof(field));
    c2 = atoi(field);

    if (c1 != c2) {
	bu_log("WARNING: CCONE1 continuation flags disagree, %d vs %d\n", c1, c2);
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
	       group_id, comp_id, element_id);
    }

    bu_strlcpy(field, &line[8], sizeof(field));
    r2 = atof(field) * 25.4;

    bu_strlcpy(field, &line[16], sizeof(field));
    end1 = atoi(field);

    bu_strlcpy(field, &line[24], sizeof(field));
    end2 = atoi(field);

    if (r1 < 0.0 || r2 < 0.0) {
	bu_log("ERROR: CCONE1 has illegal radii, %f and %f\n", r1/25.4, r2/25.4);
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
	       group_id, comp_id, element_id);
	bu_log("\tCCONE1 solid ignored\n");
	return;
    }

    if (mode == PLATE_MODE) {
	if (thick <= 0.0) {
	    bu_log("ERROR: Plate mode CCONE1 has illegal thickness (%f)\n", thick/25.4);
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   group_id, comp_id, element_id);
	    bu_log("\tCCONE1 solid ignored\n");
	    return;
	}

	if (r1-thick < min_radius && r2-thick < min_radius) {
	    bu_log("ERROR: Plate mode CCONE1 has too large thickness (%f)\n", thick/25.4);
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   group_id, comp_id, element_id);
	    bu_log("\tCCONE1 solid ignored\n");
	    return;
	}
    }

    if (pt1 < 1 || pt1 > max_grid_no || pt2 < 1 || pt2 > max_grid_no || pt1 == pt2) {
	bu_log("ERROR: CCONE1 has illegal grid points (%d and %d)\n", pt1, pt2);
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
	       group_id, comp_id, element_id);
	bu_log("\tCCONE1 solid ignored\n");
	return;
    }

    /* BRL_CAD doesn't allow zero radius, so use a very small radius */
    if (r1 < min_radius)
	r1 = min_radius;
    if (r2 < min_radius)
	r2 = min_radius;

    VSUB2(height, grid_points[pt2], grid_points[pt1]);

    if (mode == VOLUME_MODE) {
	outer_name = make_solid_name(CCONE1, element_id, comp_id, group_id, 0);
	mk_trc_h(fpout, outer_name, grid_points[pt1], height, r1, r2);
	bu_free(outer_name, "solid_name");
    } else if (mode == PLATE_MODE) {
	/* make inside TGC */

	point_t base;
	point_t top;
	vect_t inner_height;
	fastf_t inner_r1, inner_r2;
	fastf_t length;
	fastf_t sin_ang;
	fastf_t slant_len;
	fastf_t r1a, r2a;
	vect_t height_dir;

	/* make outside TGC */
	outer_name = make_solid_name(CCONE1, element_id, comp_id, group_id, 1);
	mk_trc_h(fpout, outer_name, grid_points[pt1], height, r1, r2);

	BU_LIST_INIT(&r_head.l);
	if (mk_addmember(outer_name, &r_head.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
	    bu_exit(1, "CCONE1: mk_addmember failed\n");
	bu_free(outer_name, "solid_name");

	length = MAGNITUDE(height);
	VSCALE(height_dir, height, 1.0/length);
	slant_len = sqrt(length*length + (r2 - r1)*(r2 - r1));

	sin_ang = length/slant_len;

	if (end1 == END_OPEN) {
	    r1a = r1;
	    inner_r1 = r1 - thick/sin_ang;
	    VMOVE(base, grid_points[pt1]);
	} else {
	    r1a = r1 + (r2 - r1)*thick/length;
	    inner_r1 = r1a - thick/sin_ang;
	    VJOIN1(base, grid_points[pt1], thick, height_dir);
	}

	if (inner_r1 < 0.0) {
	    fastf_t dist_to_new_base;

	    dist_to_new_base = inner_r1 * length/(r1 - r2);
	    inner_r1 = min_radius;
	    VJOIN1(base, base, dist_to_new_base, height_dir);
	} else if (inner_r1 < min_radius) {
	    inner_r1 = min_radius;
	}

	if (end2 == END_OPEN) {
	    r2a = r2;
	    inner_r2 = r2 - thick/sin_ang;
	    VMOVE(top, grid_points[pt2]);
	} else {
	    r2a = r2 + (r1 - r2)*thick/length;
	    inner_r2 = r2a - thick/sin_ang;
	    VJOIN1(top, grid_points[pt2], -thick, height_dir);
	}

	if (inner_r2 < 0.0) {
	    fastf_t dist_to_new_top;

	    dist_to_new_top = inner_r2 * length/(r2 - r1);
	    inner_r2 = min_radius;
	    VJOIN1(top, top, -dist_to_new_top, height_dir);
	} else if (inner_r2 < min_radius) {
	    inner_r2 = min_radius;
	}

	VSUB2(inner_height, top, base);
	if (VDOT(inner_height, height) <= 0.0) {
	    bu_log("ERROR: CCONE1 height (%f) too small for thickness (%f)\n", length/25.4, thick/25.4);
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   group_id, comp_id, element_id);
	    bu_log("\tCCONE1 inner solid ignored\n");
	} else {
	    /* make inner tgc */

	    inner_name = make_solid_name(CCONE1, element_id, comp_id, group_id, 2);
	    mk_trc_h(fpout, inner_name, base, inner_height, inner_r1, inner_r2);

	    if (mk_addmember(inner_name, &r_head.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
		bu_exit(1, "CCONE1: mk_addmember failed\n");
	    bu_free(inner_name, "solid_name");
	}

	name = make_solid_name(CCONE1, element_id, comp_id, group_id, 0);
	mk_comb(fpout, name, &r_head.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 1, 1);
	bu_free(name, "solid_name");
    }

}


static void
f4_do_ccone2(void)
{
    int element_id;
    int pt1, pt2;
    int c1, c2;
    fastf_t ro1, ro2, ri1, ri2;
    vect_t height;
    char *name = (char *)NULL;
    struct wmember r_head;

    bu_strlcpy(field, &line[8], sizeof(field));
    element_id = atoi(field);

    if (!pass) {
	make_region_name(group_id, comp_id);
	if (!get_line()) {
	    bu_log("Unexpected EOF while reading continuation card for CCONE2\n");
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   group_id, comp_id, element_id);
	    bu_exit(1, "ERROR: unexpected end-of-file encountered\n");
	}
	return;
    }

    bu_strlcpy(field, &line[24], sizeof(field));
    pt1 = atoi(field);

    bu_strlcpy(field, &line[32], sizeof(field));
    pt2 = atoi(field);

    bu_strlcpy(field, &line[64], sizeof(field));
    ro1 = atof(field) * 25.4;

    bu_strlcpy(field, &line[72], sizeof(field));
    c1 = atoi(field);

    if (!get_line()) {
	bu_log("Unexpected EOF while reading continuation card for CCONE2\n");
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
	       group_id, comp_id, element_id, c1);
	bu_exit(1, "ERROR: unexpected end-of-file encountered\n");
    }

    bu_strlcpy(field, line, sizeof(field));
    c2 = atoi(field);

    if (c1 != c2) {
	bu_log("WARNING: CCONE2 continuation flags disagree, %d vs %d\n", c1, c2);
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
	       group_id, comp_id, element_id);
    }

    bu_strlcpy(field, &line[8], sizeof(field));
    ro2 = atof(field) * 25.4;

    bu_strlcpy(field, &line[16], sizeof(field));
    ri1 = atof(field) * 25.4;

    bu_strlcpy(field, &line[24], sizeof(field));
    ri2 = atof(field) * 25.4;

    if (pt1 == pt2) {
	bu_log("ERROR: CCONE2 has same endpoints %d and %d\n", pt1, pt2);
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
	       group_id, comp_id, element_id);
	return;
    }

    if (ro1 < 0.0 || ro2 < 0.0 || ri1 < 0.0 || ri2 < 0.0) {
	bu_log("ERROR: CCONE2 has illegal radii %f %f %f %f\n", ro1, ro2, ri1, ri2);
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
	       group_id, comp_id, element_id);
	return;
    }

    if (ro1 < min_radius)
	ro1 = min_radius;

    if (ro2 < min_radius)
	ro2 = min_radius;

    BU_LIST_INIT(&r_head.l);

    VSUB2(height, grid_points[pt2], grid_points[pt1]);

    if (ri1 <= 0.0 && ri2 <= 0.0) {
	name = make_solid_name(CCONE2, element_id, comp_id, group_id, 0);
	mk_trc_h(fpout, name, grid_points[pt1], height, ro1, ro2);
	bu_free(name, "solid_name");
    } else {
	name = make_solid_name(CCONE2, element_id, comp_id, group_id, 1);
	mk_trc_h(fpout, name, grid_points[pt1], height, ro1, ro2);

	if (mk_addmember(name, &r_head.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
	    bu_exit(1, "mk_addmember failed!\n");
	bu_free(name, "solid_name");

	if (ri1 < min_radius)
	    ri1 = min_radius;

	if (ri2 < min_radius)
	    ri2 = min_radius;

	name = make_solid_name(CCONE2, element_id, comp_id, group_id, 2);
	mk_trc_h(fpout, name, grid_points[pt1], height, ri1, ri2);

	if (mk_addmember(name, &r_head.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
	    bu_exit(1, "mk_addmember failed!\n");
	bu_free(name, "solid_name");

	name = make_solid_name(CCONE2, element_id, comp_id, group_id, 0);
	mk_comb(fpout, name, &r_head.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 1, 1);
	bu_free(name, "solid_name");
    }
}


static void
f4_do_ccone3(void)
{
    int element_id;
    int pt1, pt2, pt3, pt4, i;
    char *name;
    fastf_t ro[4], ri[4], len03, len01, len12, len23;
    vect_t diff, diff2, diff3, diff4;
    struct wmember r_head;

    bu_strlcpy(field, &line[8], sizeof(field));
    element_id = atoi(field);

    if (!pass) {
	make_region_name(group_id, comp_id);
	if (!get_line()) {
	    bu_log("Unexpected EOF while reading continuation card for CCONE3\n");
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   group_id, comp_id, element_id);
	    bu_exit(1, "ERROR: unexpected end-of-file encountered\n");
	}
	return;
    }

    bu_strlcpy(field, &line[24], sizeof(field));
    pt1 = atoi(field);

    bu_strlcpy(field, &line[32], sizeof(field));
    pt2 = atoi(field);

    bu_strlcpy(field, &line[40], sizeof(field));
    pt3 = atoi(field);

    bu_strlcpy(field, &line[48], sizeof(field));
    pt4 = atoi(field);

    bu_strlcpy(field, &line[72], sizeof(field));

    if (!get_line()) {
	bu_log("Unexpected EOF while reading continuation card for CCONE3\n");
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %8.8s\n",
	       group_id, comp_id, element_id, field);
	bu_exit(1, "ERROR: unexpected end-of-file encountered\n");
    }

    if (strncmp(field, line, 8)) {
	bu_log("WARNING: CCONE3 continuation flags disagree, %8.8s vs %8.8s\n", field, line);
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
	       group_id, comp_id, element_id);
    }

    for (i=0; i<4; i++) {
	bu_strlcpy(field, &line[8*(i+1)], sizeof(field));
	ro[i] = atof(field) * 25.4;
	if (ro[i] < 0.0) {
	    bu_log("ERROR: CCONE3 has illegal radius %f\n", ro[i]);
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   group_id, comp_id, element_id);
	    return;
	}
	if (BU_STR_EQUAL(field, "        "))
	    ro[i] = -1.0;
    }

    for (i=0; i<4; i++) {
	bu_strlcpy(field, &line[32 + 8*(i+1)], sizeof(field));
	ri[i] = atof(field) * 25.4;
	if (ri[i] < 0.0) {
	    bu_log("ERROR: CCONE3 has illegal radius %f\n", ri[i]);
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   group_id, comp_id, element_id);
	    return;
	}
	if (BU_STR_EQUAL(field, "        "))
	    ri[i] = -1.0;
    }

    VSUB2(diff4, grid_points[pt4], grid_points[pt1]);
    VSUB2(diff3, grid_points[pt3], grid_points[pt1]);
    VSUB2(diff2, grid_points[pt2], grid_points[pt1]);

    len03 = MAGNITUDE(diff4);
    len01 = MAGNITUDE(diff2);
    len12 = MAGNITUDE(diff3) - len01;
    len23 = len03 - len01 - len12;

    for (i=0; i<4; i+=3) {
	if (EQUAL(ro[i], -1.0)) {
	    if (EQUAL(ri[i], -1.0)) {
		bu_log("ERROR: both inner and outer radii at g%d of a CCONE3 are undefined\n", i+1);
		bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		       group_id, comp_id, element_id);
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
	if (ro[i] < min_radius)
	    ro[i] = min_radius;
	if (ri[i] < min_radius)
	    ri[i] = min_radius;
    }

    BU_LIST_INIT(&r_head.l);

    if (pt1 != pt2) {
	VSUB2(diff, grid_points[pt2], grid_points[pt1]);

	/* make first cone */
	if (!EQUAL(ro[0], min_radius) || !EQUAL(ro[1], min_radius)) {
	    name = make_solid_name(CCONE3, element_id, comp_id, group_id, 1);
	    mk_trc_h(fpout, name, grid_points[pt1], diff, ro[0], ro[1]);
	    if (mk_addmember(name, &r_head.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
		bu_exit(1, "mk_addmember failed!\n");
	    bu_free(name, "solid_name");

	    /* and the inner cone */
	    if (!EQUAL(ri[0], min_radius) || !EQUAL(ri[1], min_radius)) {
		name = make_solid_name(CCONE3, element_id, comp_id, group_id, 11);
		mk_trc_h(fpout, name, grid_points[pt1], diff, ri[0], ri[1]);
		if (mk_addmember(name, &r_head.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
		    bu_exit(1, "mk_addmember failed!\n");
		bu_free(name, "solid_name");
	    }
	}
    }

    if (pt2 != pt3) {
	VSUB2(diff, grid_points[pt3], grid_points[pt2]);

	/* make second cone */
	if (!EQUAL(ro[1], min_radius) || !EQUAL(ro[2], min_radius)) {
	    name = make_solid_name(CCONE3, element_id, comp_id, group_id, 2);
	    mk_trc_h(fpout, name, grid_points[pt2], diff, ro[1], ro[2]);
	    if (mk_addmember(name, &r_head.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
		bu_exit(1, "mk_addmember failed!\n");
	    bu_free(name, "solid_name");

	    /* and the inner cone */
	    if (!EQUAL(ri[1], min_radius) || !EQUAL(ri[2], min_radius)) {
		name = make_solid_name(CCONE3, element_id, comp_id, group_id, 22);
		mk_trc_h(fpout, name, grid_points[pt2], diff, ri[1], ri[2]);
		if (mk_addmember(name, &r_head.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
		    bu_exit(1, "mk_addmember failed!\n");
		bu_free(name, "solid_name");
	    }
	}
    }

    if (pt3 != pt4) {
	VSUB2(diff, grid_points[pt4], grid_points[pt3]);

	/* make third cone */
	if (!EQUAL(ro[2], min_radius) || !EQUAL(ro[3], min_radius)) {
	    name = make_solid_name(CCONE3, element_id, comp_id, group_id, 3);
	    mk_trc_h(fpout, name, grid_points[pt3], diff, ro[2], ro[3]);
	    if (mk_addmember(name, &r_head.l, NULL, WMOP_UNION) == (struct wmember *)NULL)
		bu_exit(1, "mk_addmember failed!\n");
	    bu_free(name, "solid_name");

	    /* and the inner cone */
	    if (!EQUAL(ri[2], min_radius) || !EQUAL(ri[3], min_radius)) {
		name = make_solid_name(CCONE3, element_id, comp_id, group_id, 33);
		mk_trc_h(fpout, name, grid_points[pt3], diff, ri[2], ri[3]);
		if (mk_addmember(name, &r_head.l, NULL, WMOP_SUBTRACT) == (struct wmember *)NULL)
		    bu_exit(1, "mk_addmember failed!\n");
		bu_free(name, "solid_name");
	    }
	}
    }

    name = make_solid_name(CCONE3, element_id, comp_id, group_id, 0);
    mk_comb(fpout, name, &r_head.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 1, 1);
    bu_free(name, "solid_name");
}


static int
skip_region(int id)
{
    int i;

    if (!f4_do_skips)
	return 0;

    for (i=0; i<f4_do_skips; i++) {
	if (id == region_list[i])
	    return 0;
    }

    return 1;
}


static void
Add_holes(int type, int gr, int comp, struct hole_list *ptr)
{
    struct holes *hole_ptr = (struct holes *)NULL;
    struct holes *prev = (struct holes *)NULL;
    struct hole_list *hptr= (struct hole_list *)NULL;

    if (debug) {
	bu_log("Adding holes for group %d, component %d:\n", gr, comp);
	hptr = ptr;
	while (hptr) {
	    bu_log("\t%d %d\n", hptr->group, hptr->component);
	    hptr = hptr->next;
	}
    }

    if (f4_do_skips) {
	if (!skip_region(gr*1000 + comp)) {
	    /* add holes for this region to the list of regions to process */
	    hptr = ptr;
	    if (RT_G_DEBUG&DEBUG_MEM_FULL &&  bu_mem_barriercheck())
		bu_log("ERROR: bu_mem_barriercheck failed in Add_hole\n");
	    while (hptr) {
		if (f4_do_skips == region_list_len) {
		    region_list_len += REGION_LIST_BLOCK;
		    region_list = (int *)bu_realloc((char *)region_list, region_list_len*sizeof(int), "region_list");
		    if (RT_G_DEBUG&DEBUG_MEM_FULL &&  bu_mem_barriercheck())
			bu_log("ERROR: bu_mem_barriercheck failed in Add_hole (after realloc)\n");
		}
		region_list[f4_do_skips++] = 1000*hptr->group + hptr->component;
		if (RT_G_DEBUG&DEBUG_MEM_FULL &&  bu_mem_barriercheck())
		    bu_log("ERROR: bu_mem_barriercheck failed in Add_hole (after adding %d\n)\n", 1000*hptr->group + hptr->component);
		hptr = hptr->next;
	    }
	}
    }

    if (!hole_root) {
	hole_root = (struct holes *)bu_malloc(sizeof(struct holes), "Add_holes: hole_root");
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
	prev->next = (struct holes *)bu_malloc(sizeof(struct holes), "Add_holes: hole_ptr->next");
	hole_ptr = prev->next;
	hole_ptr->group = gr;
	hole_ptr->component = comp;
	hole_ptr->type = type;
	hole_ptr->holes = ptr;
	hole_ptr->next = (struct holes *)NULL;
    }
}


static void
f4_do_hole_wall(int type)
{
    struct hole_list *list_ptr;
    struct hole_list *list_start;
    int group, comp;
    int igrp, icmp;
    size_t s_len;
    size_t col;

    if (debug)
	bu_log("f4_do_hole_wall: %s\n", line);

    if (pass)
	return;

    if (type != HOLE && type != WALL) {
	bu_exit(1, "f4_do_hole_wall: unrecognized type (%d)\n", type);
    }

    /* eliminate trailing blanks */
    s_len = strlen(line);
    while (isspace(line[--s_len]))
	line[s_len] = '\0';

    s_len = strlen(line);
    if (s_len > 80)
	s_len = 80;

    bu_strlcpy(field, &line[8], sizeof(field));
    group = atoi(field);

    bu_strlcpy(field, &line[16], sizeof(field));
    comp = atoi(field);

    list_start = (struct hole_list *)NULL;
    list_ptr = (struct hole_list *)NULL;
    col = 24;

    while (col < s_len) {
	bu_strlcpy(field, &line[col], sizeof(field));
	igrp = atoi(field);

	col += 8;
	if (col >= s_len)
	    break;

	bu_strlcpy(field, &line[col], sizeof(field));
	icmp = atoi(field);

	if (igrp >= 0 && icmp > 0) {
	    if (igrp == group && comp == icmp) {
		bu_log("Hole or wall card references itself (ignoring): (%s)\n", line);
	    } else {
		if (list_ptr) {
		    list_ptr->next = (struct hole_list *)bu_malloc(sizeof(struct hole_list), "f4_do_hole_wall: list_ptr");
		    list_ptr = list_ptr->next;
		} else {
		    list_ptr = (struct hole_list *)bu_malloc(sizeof(struct hole_list), "f4_do_hole_wall: list_ptr");
		    list_start = list_ptr;
		}

		list_ptr->group = igrp;
		list_ptr->component = icmp;
		list_ptr->next = (struct hole_list *)NULL;
	    }
	}

	col += 8;
    }

    Add_holes(type, group, comp, list_start);
}


static void
f4_Add_bot_face(int pt1, int pt2, int pt3, fastf_t thick, int pos)
{

    if (pt1 == pt2 || pt2 == pt3 || pt1 == pt3) {
	bu_log("f4_Add_bot_face: ignoring degenerate triangle in group %d component %d\n", group_id, comp_id);
	return;
    }

    if (pos == 0)	/* use default */
	pos = POS_FRONT;

    if (mode == PLATE_MODE) {
	if (pos != POS_CENTER && pos != POS_FRONT) {
	    bu_log("f4_Add_bot_face: illegal postion parameter (%d), must be one or two (ignoring face for group %d component %d)\n", pos, group_id, comp_id);
	    return;
	}
    }

    if (face_count >= face_size) {
	face_size += GRID_BLOCK;
	if (bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck())
	    bu_log("memory corrupted before realloc of faces, thickness, and facemode\n");
	faces = (int *)bu_realloc((void *)faces,  face_size*3*sizeof(int), "faces");
	thickness = (fastf_t *)bu_realloc((void *)thickness, face_size*sizeof(fastf_t), "thickness");
	facemode = (char *)bu_realloc((void *)facemode, face_size*sizeof(char), "facemode");
	if (bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck())
	    bu_log("memory corrupted after realloc of faces, thickness, and facemode\n");
    }

    faces[face_count*3] = pt1;
    faces[face_count*3+1] = pt2;
    faces[face_count*3+2] = pt3;

    if (mode == PLATE_MODE) {
	thickness[face_count] = thick;
	facemode[face_count] = pos;
    } else {
	thickness[face_count] = 0, 0;
	facemode[face_count] = 0;
    }

    face_count++;

    if (bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck())
	bu_log("memory corrupted at end of f4_Add_bot_face()\n");
}


static void
f4_do_tri(void)
{
    int element_id;
    int pt1, pt2, pt3;
    fastf_t thick;
    int pos;

    if (debug)
	bu_log("f4_do_tri: %s\n", line);

    bu_strlcpy(field, &line[8], sizeof(field));
    element_id = atoi(field);

    if (!bot)
	bot = element_id;

    if (!pass)
	return;

    if (faces == NULL) {
	if (bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck())
	    bu_log("memory corrupted before malloc of faces\n");
	faces = (int *)bu_malloc(GRID_BLOCK*3*sizeof(int), "faces");
	thickness = (fastf_t *)bu_malloc(GRID_BLOCK*sizeof(fastf_t), "thickness");
	facemode = (char *)bu_malloc(GRID_BLOCK*sizeof(char), "facemode");
	face_size = GRID_BLOCK;
	face_count = 0;
	if (bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck())
	    bu_log("memory corrupted after malloc of faces, thickness, and facemode\n");
    }

    bu_strlcpy(field, &line[24], sizeof(field));
    pt1 = atoi(field);

    bu_strlcpy(field, &line[32], sizeof(field));
    pt2 = atoi(field);

    bu_strlcpy(field, &line[40], sizeof(field));
    pt3 = atoi(field);

    thick = 0.0;
    pos = 0;

    if (mode == PLATE_MODE) {
	bu_strlcpy(field, &line[56], sizeof(field));
	thick = atof(field) * 25.4;

	bu_strlcpy(field, &line[64], sizeof(field));
	pos = atoi(field);
	if (pos == 0)
	    pos = POS_FRONT;

	if (debug)
	    bu_log("\tplate mode: thickness = %f\n", thick);

    }

    if (f4_do_plot)
	plot_tri(pt1, pt2, pt3);

    if (bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck())
	bu_log("memory corrupted before call to f4_Add_bot_face()\n");

    f4_Add_bot_face(pt1, pt2, pt3, thick, pos);

    if (bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck())
	bu_log("memory corrupted after call to f4_Add_bot_face()\n");
}


static void
f4_do_quad(void)
{
    int element_id;
    int pt1, pt2, pt3, pt4;
    fastf_t thick = 0.0;
    int pos = 0;

    bu_strlcpy(field, &line[8], sizeof(field));
    element_id = atoi(field);

    if (debug)
	bu_log("f4_do_quad: %s\n", line);

    if (!bot)
	bot = element_id;

    if (!pass)
	return;

    if (faces == NULL) {
	faces = (int *)bu_malloc(GRID_BLOCK*3*sizeof(int), "faces");
	thickness = (fastf_t *)bu_malloc(GRID_BLOCK*sizeof(fastf_t), "thickness");
	facemode = (char *)bu_malloc(GRID_BLOCK*sizeof(char), "facemode");
	face_size = GRID_BLOCK;
	face_count = 0;
    }

    bu_strlcpy(field, &line[24], sizeof(field));
    pt1 = atoi(field);

    bu_strlcpy(field, &line[32], sizeof(field));
    pt2 = atoi(field);

    bu_strlcpy(field, &line[40], sizeof(field));
    pt3 = atoi(field);

    bu_strlcpy(field, &line[48], sizeof(field));
    pt4 = atoi(field);

    if (mode == PLATE_MODE) {
	bu_strlcpy(field, &line[56], sizeof(field));
	thick = atof(field) * 25.4;

	bu_strlcpy(field, &line[64], sizeof(field));
	pos = atoi(field);

	if (pos == 0)	/* use default */
	    pos = POS_FRONT;

	if (pos != POS_CENTER && pos != POS_FRONT) {
	    bu_log("f4_do_quad: illegal postion parameter (%d), must be one or two\n", pos);
	    bu_log("\telement %d, component %d, group %d\n", element_id, comp_id, group_id);
	    return;
	}
    }

    f4_Add_bot_face(pt1, pt2, pt3, thick, pos);
    f4_Add_bot_face(pt1, pt3, pt4, thick, pos);
}


static void
make_bot_object(void)
{
    int i;
    int max_pt = 0;
    int min_pt = 999999;
    int num_vertices;
    struct bu_bitv *bv = (struct bu_bitv *)NULL;
    int bot_mode;
    char *name = (char *)NULL;
    int element_id = bot;
    int count;
    struct rt_bot_internal bot_ip;

    if (!pass) {
	make_region_name(group_id, comp_id);
	return;
    }

    bot_ip.magic = RT_BOT_INTERNAL_MAGIC;
    for (i=0; i<face_count; i++) {
	V_MIN(min_pt, faces[i*3]);
	V_MAX(max_pt, faces[i*3]);
	V_MIN(min_pt, faces[i*3+1]);
	V_MAX(max_pt, faces[i*3+1]);
	V_MIN(min_pt, faces[i*3+2]);
	V_MAX(max_pt, faces[i*3+2]);
    }

    num_vertices = max_pt - min_pt + 1;
    bot_ip.num_vertices = num_vertices;
    bot_ip.vertices = (fastf_t *)bu_calloc(num_vertices*3, sizeof(fastf_t), "BOT vertices");
    for (i=0; i<num_vertices; i++)
	VMOVE(&bot_ip.vertices[i*3], grid_points[min_pt+i])

	    for (i=0; i<face_count*3; i++)
		faces[i] -= min_pt;
    bot_ip.num_faces = face_count;
    bot_ip.faces = bu_calloc(face_count*3, sizeof(int), "BOT faces");
    for (i=0; i<face_count*3; i++)
	bot_ip.faces[i] = faces[i];

    bot_ip.face_mode = (struct bu_bitv *)NULL;
    bot_ip.thickness = (fastf_t *)NULL;
    if (mode == PLATE_MODE) {
	bot_mode = RT_BOT_PLATE;
	bv = bu_bitv_new(face_count);
	for (i=0; i<face_count; i++) {
	    if (facemode[i] == POS_FRONT)
		BU_BITSET(bv, i);
	}
	bot_ip.face_mode = bv;
	bot_ip.thickness = (fastf_t *)bu_calloc(face_count, sizeof(fastf_t), "BOT thickness");
	for (i=0; i<face_count; i++)
	    bot_ip.thickness[i] = thickness[i];
    } else {
	bot_mode = RT_BOT_SOLID;
    }

    bot_ip.mode = bot_mode;
    bot_ip.orientation = RT_BOT_UNORIENTED;
    bot_ip.bot_flags = 0;

    count = rt_bot_vertex_fuse(&bot_ip);
    count = rt_bot_face_fuse(&bot_ip);
    if (count)
	bu_log("WARNING: %d duplicate faces eliminated from group %d component %d\n", count, group_id, comp_id);

    name = make_solid_name(BOT, element_id, comp_id, group_id, 0);
    mk_bot(fpout, name, bot_mode, RT_BOT_UNORIENTED, 0, bot_ip.num_vertices, bot_ip.num_faces, bot_ip.vertices,
	   bot_ip.faces, bot_ip.thickness, bot_ip.face_mode);
    bu_free(name, "solid_name");

    if (mode == PLATE_MODE) {
	bu_free((char *)bot_ip.thickness, "BOT thickness");
	bu_free((char *)bot_ip.face_mode, "BOT face_mode");
    }
    bu_free((char *)bot_ip.vertices, "BOT vertices");
    bu_free((char *)bot_ip.faces, "BOT faces");

}


static void
skip_section(void)
{
    long section_start;

    /* skip to start of next section */
    section_start = ftell(fpin);
    if (get_line()) {
	while (line[0] && strncmp(line, "SECTION", 7) &&
	       strncmp(line, "HOLE", 4) &&
	       strncmp(line, "WALL", 4) &&
	       strncmp(line, "VEHICLE", 7)) {
	    section_start = ftell(fpin);
	    if (!get_line())
		break;
	}
    }
    /* seek to start of the section */
    fseek(fpin, section_start, SEEK_SET);
}


/*	cleanup from previous component and start a new one.
 *	This is called with final == 1 when ENDDATA is found
 */
static void
f4_do_section(int final)
{
    int found;
    struct name_tree *nm_ptr;

    if (debug)
	bu_log("f4_do_section(%d): %s\n", final, line);

    if (pass)	/* doing geometry */ {
	if (region_id && !skip_region(region_id)) {
	    comp_count++;

	    if (bot)
		make_bot_object();
	}
	if (final && debug) /* The ENDATA card has been found */
	    List_names();
    } else if (bot) {
	make_region_name(group_id, comp_id);
    }

    if (!final) {
	bu_strlcpy(field, &line[8], sizeof(field));
	group_id = atoi(field);

	bu_strlcpy(field, &line[16], sizeof(field));
	comp_id = atoi(field);

	region_id = group_id * 1000 + comp_id;

	if (skip_region(region_id)) /* do not process this component */ {
	    skip_section();
	    return;
	}

	if (comp_id > 999) {
	    bu_log("Illegal component id number %d, changed to 999\n", comp_id);
	    comp_id = 999;
	}

	bu_strlcpy(field, &line[24], sizeof(field));
	mode = atoi(field);
	if (mode != 1 && mode != 2) {
	    bu_log("Illegal mode (%d) for group %d component %d, using volume mode\n",
		   mode, group_id, comp_id);
	    mode = 2;
	}

	if (!pass) {
	    nm_ptr = Search_ident(name_root, region_id, &found);
	    if (found && nm_ptr->mode != mode) {
		bu_log("ERROR: second SECTION card found with different mode for component (group=%d, component=%d), conversion of this component will be incorrect!\n",
		       group_id, comp_id);
	    }
	}
    }

    bot = 0;
    face_count = 0;
}


static void
f4_do_hex1(void)
{
    fastf_t thick=0.0;
    int pos;
    int pts[8];
    int element_id;
    int i;
    int cont1, cont2;

    bu_strlcpy(field, &line[8], sizeof(field));
    element_id = atoi(field);

    if (!bot)
	bot = element_id;

    if (!pass) {
	if (!get_line()) {
	    bu_log("Unexpected EOF while reading continuation card for CHEX1\n");
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   group_id, comp_id, element_id);
	    bu_exit(1, "ERROR: unexpected end-of-file encountered\n");
	}
	return;
    }

    if (faces == NULL) {
	faces = (int *)bu_malloc(GRID_BLOCK*3*sizeof(int), "faces");
	thickness = (fastf_t *)bu_malloc(GRID_BLOCK*sizeof(fastf_t), "thickness");
	facemode = (char *)bu_malloc(GRID_BLOCK*sizeof(char), "facemode");
	face_size = GRID_BLOCK;
	face_count = 0;
    }

    for (i=0; i<6; i++) {
	bu_strlcpy(field, &line[24 + i*8], sizeof(field));
	pts[i] = atoi(field);
    }

    bu_strlcpy(field, &line[72], sizeof(field));
    cont1 = atoi(field);

    if (!get_line()) {
	bu_log("Unexpected EOF while reading continuation card for CHEX1\n");
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
	       group_id, comp_id, element_id, cont1);
	bu_exit(1, "ERROR: unexpected end-of-file encountered\n");
    }

    bu_strlcpy(field, line, sizeof(field));
    cont2 = atoi(field);

    if (cont1 != cont2) {
	bu_log("Continuation card numbers do not match for CHEX1 element (%d vs %d)\n", cont1, cont2);
	bu_log("\tskipping CHEX1 element: group_id = %d, comp_id = %d, element_id = %d\n",
	       group_id, comp_id, element_id);
	return;
    }

    bu_strlcpy(field, &line[8], sizeof(field));
    pts[6] = atoi(field);

    bu_strlcpy(field, &line[16], sizeof(field));
    pts[7] = atoi(field);

    if (mode == PLATE_MODE) {
	bu_strlcpy(field, &line[56], sizeof(field));
	thick = atof(field) * 25.4;
	if (thick <= 0.0) {
	    bu_log("f4_do_hex1: illegal thickness (%f), skipping CHEX1 element\n", thick);
	    bu_log("\telement %d, component %d, group %d\n", element_id, comp_id, group_id);
	    return;
	}

	bu_strlcpy(field, &line[64], sizeof(field));
	pos = atoi(field);

	if (pos == 0)	/* use default */
	    pos = POS_FRONT;

	if (pos != POS_CENTER && pos != POS_FRONT) {
	    bu_log("f4_do_hex1: illegal postion parameter (%d), must be 1 or 2, skipping CHEX1 element\n", pos);
	    bu_log("\telement %d, component %d, group %d\n", element_id, comp_id, group_id);
	    return;
	}
    } else {
	pos =  POS_FRONT;
	thick = 0.0;
    }

    for (i=0; i<12; i++)
	f4_Add_bot_face(pts[hex_faces[i][0]], pts[hex_faces[i][1]], pts[hex_faces[i][2]], thick, pos);
}


static void
f4_do_hex2(void)
{
    int pts[8];
    int element_id;
    int i;
    int cont1, cont2;
    point_t points[8];
    char *name = (char *)NULL;

    bu_strlcpy(field, &line[8], sizeof(field));
    element_id = atoi(field);

    if (!pass) {
	make_region_name(group_id, comp_id);
	if (!get_line()) {
	    bu_log("Unexpected EOF while reading continuation card for CHEX2\n");
	    bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d\n",
		   group_id, comp_id, element_id);
	    bu_exit(1, "ERROR: unexpected end-of-file encountered\n");
	}
	return;
    }

    for (i=0; i<6; i++) {
	bu_strlcpy(field, &line[24 + i*8], sizeof(field));
	pts[i] = atoi(field);
    }

    bu_strlcpy(field, &line[72], sizeof(field));
    cont1 = atoi(field);

    if (!get_line()) {
	bu_log("Unexpected EOF while reading continuation card for CHEX2\n");
	bu_log("\tgroup_id = %d, comp_id = %d, element_id = %d, c1 = %d\n",
	       group_id, comp_id, element_id, cont1);
	bu_exit(1, "ERROR: unexpected end-of-file encountered\n");
    }

    bu_strlcpy(field, line, sizeof(field));
    cont2 = atoi(field);

    if (cont1 != cont2) {
	bu_log("Continuation card numbers do not match for CHEX2 element (%d vs %d)\n", cont1, cont2);
	bu_log("\tskipping CHEX2 element: group_id = %d, comp_id = %d, element_id = %d\n",
	       group_id, comp_id, element_id);
	return;
    }

    bu_strlcpy(field, &line[8], sizeof(field));
    pts[6] = atoi(field);

    bu_strlcpy(field, &line[16], sizeof(field));
    pts[7] = atoi(field);

    for (i=0; i<8; i++)
	VMOVE(points[i], grid_points[pts[i]]);

    name = make_solid_name(CHEX2, element_id, comp_id, group_id, 0);
    mk_arb8(fpout, name, &points[0][X]);
    bu_free(name, "solid_name");

}


static void
Process_hole_wall(void)
{
    if (debug)
	bu_log("Process_hole_wall\n");
    if (bu_debug & BU_DEBUG_MEM_CHECK)
	bu_prmem("At start of Process_hole_wall:");

    rewind(fpin);
    while (1) {
	if (!strncmp(line, "HOLE", 4)) {
	    f4_do_hole_wall(HOLE);
	} else if (!strncmp(line, "WALL", 4)) {
	    f4_do_hole_wall(WALL);
	} else if (!strncmp(line, "COMPSPLT", 8)) {
	    f4_do_compsplt();
	} else if (!strncmp(line, "SECTION", 7)) {
	    bu_strlcpy(field, &line[24], sizeof(field));
	    mode = atoi(field);
	    if (mode != 1 && mode != 2) {
		bu_log("Illegal mode (%d) for group %d component %d, using volume mode\n",
		       mode, group_id, comp_id);
		mode = 2;
	    }
	} else if (!strncmp(line, "ENDDATA", 7)) {
	    break;
	}

	if (!get_line() || !line[0])
	    break;
    }

    if (debug) {
	bu_log("At end of Process_hole_wall:\n");
	List_holes();
    }
}


static void
f4_do_chgcomp(void)
{

    if (!pass)
	return;

    if (!fp_muves)
	return;

    fprintf(fp_muves, "%s", line);
}


static void
f4_do_cbacking(void)
{
    int gr1, co1, gr2, co2, material;
    fastf_t inthickness, probability;

    if (!pass)
	return;

    if (!fp_muves)
	return;

    bu_strlcpy(field, &line[8], sizeof(field));
    gr1 = atoi(field);

    bu_strlcpy(field, &line[16], sizeof(field));
    co1 = atoi(field);

    bu_strlcpy(field, &line[24], sizeof(field));
    gr2 = atoi(field);

    bu_strlcpy(field, &line[32], sizeof(field));
    co2 = atoi(field);

    bu_strlcpy(field, &line[40], sizeof(field));
    inthickness = atof(field) * 25.4;

    bu_strlcpy(field, &line[48], sizeof(field));
    probability = atof(field);

    bu_strlcpy(field, &line[56], sizeof(field));
    material = atoi(field);

    fprintf(fp_muves, "CBACKING %d %d %g %g %d\n", gr1*1000+co1, gr2*1000+co2, inthickness, probability, material);
}


static int
Process_input(int pass_number)
{

    if (debug)
	bu_log("\n\nProcess_input(pass = %d)\n", pass_number);
    if (bu_debug & BU_DEBUG_MEM_CHECK)
	bu_prmem("At start of Process_input:");

    if (pass_number != 0 && pass_number != 1) {
	bu_exit(1, "Process_input: illegal pass number %d\n", pass_number);
    }

    region_id = 0;
    pass = pass_number;
    if (!get_line() || !line[0])
	bu_strlcpy(line, "ENDDATA", sizeof(line));
    while (1) {
	if (!strncmp(line, "VEHICLE", 7))
	    f4_do_vehicle();
	else if (!strncmp(line, "HOLE", 4))
	    ;
	else if (!strncmp(line, "WALL", 4))
	    ;
	else if (!strncmp(line, "COMPSPLT", 8))
	    ;
	else if (!strncmp(line, "CBACKING", 8))
	    f4_do_cbacking();
	else if (!strncmp(line, "CHGCOMP", 7))
	    f4_do_chgcomp();
	else if (!strncmp(line, "SECTION", 7))
	    f4_do_section(0);
	else if (!strncmp(line, "$NAME", 5))
	    f4_do_name();
	else if (!strncmp(line, "$COMMENT", 8))
	    ;
	else if (!strncmp(line, "GRID", 4))
	    f4_do_grid();
	else if (!strncmp(line, "CLINE", 5))
	    f4_do_cline();
	else if (!strncmp(line, "CHEX1", 5))
	    f4_do_hex1();
	else if (!strncmp(line, "CHEX2", 5))
	    f4_do_hex2();
	else if (!strncmp(line, "CTRI", 4))
	    f4_do_tri();
	else if (!strncmp(line, "CQUAD", 5))
	    f4_do_quad();
	else if (!strncmp(line, "CCONE1", 6))
	    f4_do_ccone1();
	else if (!strncmp(line, "CCONE2", 6))
	    f4_do_ccone2();
	else if (!strncmp(line, "CCONE3", 6))
	    f4_do_ccone3();
	else if (!strncmp(line, "CSPHERE", 7))
	    f4_do_sphere();
	else if (!strncmp(line, "ENDDATA", 7)) {
	    f4_do_section(1);
	    break;
	} else {
	    bu_log("ERROR: skipping unrecognized data type\n%s\n", line);
	}

	if (!get_line() || !line[0])
	    bu_strlcpy(line, "ENDDATA", sizeof(line));
    }

    if (debug) {
	bu_log("At pass %d:\n", pass);
	List_names();
    }

    return 0;
}


static void
make_region_list(char *str)
{
    char *ptr, *ptr2;

    region_list = (int *)bu_calloc(REGION_LIST_BLOCK, sizeof(int), "region_list");
    region_list_len = REGION_LIST_BLOCK;
    f4_do_skips = 0;

    ptr = strtok(str, ",");
    while (ptr) {
	if ((ptr2=strchr(ptr, '-'))) {
	    int i, start, stop;

	    *ptr2 = '\0';
	    ptr2++;
	    start = atoi(ptr);
	    stop = atoi(ptr2);
	    if (bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck())
		bu_log("ERROR: bu_mem_barriercheck failed in make_region_list\n");
	    for (i=start; i<=stop; i++) {
		if (f4_do_skips == region_list_len) {
		    region_list_len += REGION_LIST_BLOCK;
		    region_list = (int *)bu_realloc((char *)region_list, region_list_len*sizeof(int), "region_list");
		    if (bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck())
			bu_log("ERROR: bu_mem_barriercheck failed in make_region_list (after realloc)\n");
		}
		region_list[f4_do_skips++] = i;
		if (bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck())
		    bu_log("ERROR: bu_mem_barriercheck failed in make_region_list (after adding %d)\n", i);
	    }
	} else {
	    if (f4_do_skips == region_list_len) {
		region_list_len += REGION_LIST_BLOCK;
		region_list = (int *)bu_realloc((char *)region_list, region_list_len*sizeof(int), "region_list");
	    }
	    region_list[f4_do_skips++] = atoi(ptr);
	}
	ptr = strtok((char *)NULL, ",");
    }
}


static void
make_regions(void)
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
    bu_ptbl_reset(&stack);
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
	if (skip_region(ptr1->region_id) && !is_a_hole(ptr1->region_id))
	    goto cont1;

	/* place all the solids for this ident in a "solids" combination */
	BU_LIST_INIT(&solids.l);
	bu_ptbl_reset(&stack2);
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
	if (mk_comb(fpout, solids_name, &solids.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 1, 1))
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
	if (hptr)
	    lptr = hptr->holes;
	else
	    lptr = (struct hole_list *)NULL;

	splt = compsplt_root;
	while (splt && splt->ident_to_split != ptr1->region_id)
	    splt = splt->next;

	mode = ptr1->mode;
	if (debug)
	    bu_log("Build region for %s %d, mode = %d\n", ptr1->name, ptr1->region_id, mode);

	if (splt) {
	    vect_t norm;
	    int found;

	    /* make a halfspace */
	    VSET(norm, 0.0, 0.0, 1.0);
	    snprintf(splt_name, MAX_LINE_SIZE, "splt_%d.s", ptr1->region_id);
	    mk_half(fpout, splt_name, norm, splt->z);

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
	    MK_REGION(fpout, &region, ptr1->name, ptr1->region_id, get_fast4_color(ptr1->region_id));

	    /* create new region by subtracting halfspace */
	    BU_LIST_INIT(&region.l);
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
		MK_REGION(fpout, &region, ptr2->name, splt->new_ident, get_fast4_color(splt->new_ident));
	    } else {
		sprintf(reg_name, "comp_%d.r", splt->new_ident);
		MK_REGION(fpout, &region, reg_name, splt->new_ident, get_fast4_color(splt->new_ident));
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
	    MK_REGION(fpout, &region, ptr1->name, ptr1->region_id, get_fast4_color(ptr1->region_id));
	}
    cont1:
	ptr1 = ptr1->rright;
    }

    if (BU_LIST_NON_EMPTY(&holes.l)) {
	/* build a "holes" group */
	if (mk_comb(fpout, "holes", &holes.l, 0, NULL, NULL, NULL, 0, 0, 0, 0, 0, 1, 1))
	    bu_log("Failed to make holes group!\n");
    }
}


#define COLOR_LINE_LEN 256

static void
read_fast4_colors(char *color_file)
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

	BU_GETSTRUCT(color, fast4_color);
	color->low = low;
	color->high = high;
	color->rgb[0] = r;
	color->rgb[1] = g;
	color->rgb[2] = b;
	BU_LIST_APPEND(&HeadColor.l, &color->l);
    }
}


int
main(int argc, char **argv)
{
    int i;
    int c;
    char *plot_file=NULL;
    char *color_file=NULL;

    while ((c=bu_getopt(argc, argv, "qm:o:c:dwx:b:X:C:")) != -1) {
	switch (c) {
	    case 'q':	/* quiet mode */
		quiet = 1;
		break;
	    case 'm':
		if ((fp_muves=fopen(bu_optarg, "wb")) == (FILE *)NULL) {
		    bu_log("Unable to open MUVES file (%s)\n\tno MUVES file created\n",
			   bu_optarg);
		}
		break;
	    case 'o':	/* output a plotfile of original FASTGEN4 elements */
		f4_do_plot = 1;
		plot_file = bu_optarg;
		break;
	    case 'c':	/* convert only the specified components */
		make_region_list(bu_optarg);
		break;
	    case 'd':	/* debug option */
		debug = 1;
		break;
	    case 'w':	/* print warnings */
		warnings = 1;
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *)&rt_debug);
		bu_debug = rt_debug;
		break;
	    case 'b':
		sscanf(bu_optarg, "%x", (unsigned int *)&bu_debug);
		break;
	    case 'C':
		color_file = bu_optarg;
		break;
	    default:
		bu_log("Unrecognzed option (%c)\n", c);
		usage();
		break;
	}
    }

    if (bu_debug & BU_DEBUG_MEM_CHECK)
	bu_log("doing memory checking\n");

    if (argc-bu_optind != 2) {
	usage();
    }

    rt_init_resource(&rt_uniresource, 0, NULL);

    if ((fpin=fopen(argv[bu_optind], "rb")) == (FILE *)NULL) {
	perror("fast4-g");
	bu_exit(1, "Cannot open FASTGEN4 file (%s)\n", argv[bu_optind]);
    }

    if ((fpout=wdb_fopen(argv[bu_optind+1])) == NULL) {
	perror("fast4-g");
	bu_exit(1, "Cannot open file for output (%s)\n", argv[bu_optind+1]);
    }

    if (plot_file) {
	if ((fp_plot=fopen(plot_file, "wb")) == NULL) {
	    bu_log("Cannot open plot file (%s)\n", plot_file);
	    usage();
	}
    }

    if (bu_debug) {
	bu_printb("librtbu_debug", bu_debug, DEBUG_FORMAT);
	bu_log("\n");
    }
    if (rt_g.NMG_debug) {
	bu_printb("librt rt_g.NMG_debug", rt_g.NMG_debug, NMG_DEBUG_FORMAT);
	bu_log("\n");
    }

    BU_LIST_INIT(&HeadColor.l);
    if (color_file)
	read_fast4_colors(color_file);

    grid_size = GRID_BLOCK;
    grid_points = (point_t *)bu_malloc(grid_size * sizeof(point_t), "fast4-g: grid_points");

    cline_root = (struct cline *)NULL;

    name_root = (struct name_tree *)NULL;

    hole_root = (struct holes *)NULL;

    compsplt_root = (struct compsplt *)NULL;

    min_radius = 2.0 * sqrt(SQRT_SMALL_FASTF);

    name_count = 0;

    vehicle[0] = '\0';

    bu_ptbl_init(&stack, 64, " &stack ");
    bu_ptbl_init(&stack2, 64, " &stack2 ");

    for (i=0; i<11; i++)
	BU_LIST_INIT(&group_head[i].l);

    BU_LIST_INIT(&hole_head.l);

    if (!quiet)
	bu_log("Scanning for HOLE, WALL, and COMPLSPLT cards...\n");

    Process_hole_wall();

    rewind(fpin);

    if (!quiet)
	bu_log("Building component names....\n");

    Process_input(0);

    rewind(fpin);

    /* Make an ID record if no vehicle card was found */
    if (!vehicle[0])
	mk_id_units(fpout, argv[bu_optind], "in");

    if (!quiet)
	bu_log("Building components....\n");

    while (Process_input(1));

    if (!quiet)
	bu_log("Building regions and groups....\n");

    /* make regions */
    make_regions();

    /* make groups */
    f4_do_groups();

    if (debug)
	List_holes();

    wdb_close(fpout);

    if (!quiet)
	bu_log("%d components converted\n", comp_count);

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
