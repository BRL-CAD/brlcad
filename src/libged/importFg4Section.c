/*              I M P O R T F G 4 S E C T I O N . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2011 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file libged/importFg4Section.c
 *
 * Some of this code was taken from conv/fast4-g.c and libwdb/bot.c
 * and modified to behave as a method of the BRL-CAD database object
 * that imports a Fastgen4 section from a string. This section can
 * only contain GRIDs, CTRIs and CQUADs.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "bio.h"

#include "db.h"
#include "vmath.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "plot3.h"

#include "./ged_private.h"


static int grid_size;		/* Number of points that will fit in current grid_pts array */
static int max_grid_no=0;	/* Maximum grid number used */
static int mode=0;		/* Plate mode (1) or volume mode (2), of current component */
static int group_id=(-1);	/* Group identification number from SECTION card */
static int comp_id=(-1);	/* Component identification number from SECTION card */
static int region_id=0;		/* Region id number (group id no X 1000 + component id no) */
static char field[9];		/* Space for storing one field from an input line */

static int bot=0;		/* Flag: >0 -> There are BOT's in current component */
static int debug=0;		/* Debug flag */

static int *faces=NULL;		/* one triplet per face indexing three grid points */
static fastf_t *thickness;	/* thickness of each face */
static char *facemode;		/* mode for each face */
static int face_size=0;		/* actual length of above arrays */
static int face_count=0;	/* number of faces in above arrays */

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

static point_t *grid_pts;

static void do_grid(char *line);
static void do_tri(char *line);
static void do_quad(char *line);
static void make_bot_object(const char *name,
			    struct rt_wdb *wdbp);

/*************************** code from libwdb/bot.c ***************************/

static int
rt_mk_bot_w_normals(
    struct rt_wdb *fp,
    const char *name,
    unsigned char botmode,
    unsigned char orientation,
    unsigned char flags,
    int num_vertices,
    int num_faces,
    fastf_t *vertices,		/* array of floats for vertices [num_vertices*3] */
    int *botfaces,		/* array of ints for faces [num_faces*3] */
    fastf_t *platethickness,	/* array of plate mode thicknesses
				 * (corresponds to array of faces)
				 * NULL for modes RT_BOT_SURFACE and
				 * RT_BOT_SOLID.
				 */
    struct bu_bitv *face_mode,	/* a flag for each face indicating
				 * thickness is appended to hit point,
				 * otherwise thickness is centered
				 * about hit point
				 */
    int num_normals,		/* number of unit normals in normals array */
    fastf_t *normals,		/* array of floats for normals [num_normals*3] */
    int *face_normals)		/* array of ints (indices into normals
				 * array), must have 3*num_faces
				 * entries */
{
    struct rt_bot_internal *botip;
    int i;

    if ((num_normals > 0) && (db_version(fp->dbip) < 5)) {
	bu_log("You are using an old database format which does not support surface normals for BOT primitives\n");
	bu_log("You are attempting to create a BOT primitive named \"%s\" with surface normals\n", name);
	bu_log("The surface normals will not be saved\n");
	bu_log("Please upgrade to the current database format by using \"dbupgrade\"\n");
    }

    BU_GET(botip, struct rt_bot_internal);
    botip->magic = RT_BOT_INTERNAL_MAGIC;
    botip->mode = botmode;
    botip->orientation = orientation;
    botip->bot_flags = flags;
    botip->num_vertices = num_vertices;
    botip->num_faces = num_faces;
    botip->vertices = (fastf_t *)bu_calloc(num_vertices * 3, sizeof(fastf_t), "botip->vertices");
    for (i=0; i<num_vertices*3; i++)
	botip->vertices[i] = vertices[i];
    botip->faces = (int *)bu_calloc(num_faces * 3, sizeof(int), "botip->faces");
    for (i=0; i<num_faces*3; i++)
	botip->faces[i] = botfaces[i];
    if (mode == RT_BOT_PLATE) {
	botip->thickness = (fastf_t *)bu_calloc(num_faces, sizeof(fastf_t), "botip->thickness");
	for (i=0; i<num_faces; i++)
	    botip->thickness[i] = platethickness[i];
	botip->face_mode = bu_bitv_dup(face_mode);
    } else {
	botip->thickness = (fastf_t *)NULL;
	botip->face_mode = (struct bu_bitv *)NULL;
    }

    if ((num_normals > 0) && (db_version(fp->dbip) > 4)) {
	botip->num_normals = num_normals;
	botip->num_face_normals = botip->num_faces;
	botip->normals = (fastf_t *)bu_calloc(botip->num_normals * 3, sizeof(fastf_t), "BOT normals");
	botip->face_normals = (int *)bu_calloc(botip->num_faces * 3, sizeof(int), "BOT face normals");
	memcpy(botip->normals, normals, botip->num_normals * 3 * sizeof(fastf_t));
	memcpy(botip->face_normals, face_normals, botip->num_faces * 3 * sizeof(int));
    } else {
	botip->bot_flags = 0;
	botip->num_normals = 0;
	botip->num_face_normals = 0;
	botip->normals = (fastf_t *)NULL;
	botip->face_normals = (int *)NULL;
    }

    return wdb_export(fp, name, (genptr_t)botip, ID_BOT, 1.0);
}


static int
rt_mk_bot(
    struct rt_wdb *fp,
    const char *name,
    unsigned char botmode,
    unsigned char orientation,
    unsigned char flags,
    int num_vertices,
    int num_faces,
    fastf_t *vertices,		/* array of floats for vertices [num_vertices*3] */
    int *botfaces,		/* array of ints for faces [num_faces*3] */
    fastf_t *platethickness,	/* array of plate mode thicknesses
				 * (corresponds to array of faces)
				 * NULL for modes RT_BOT_SURFACE and
				 * RT_BOT_SOLID.
				 */
    struct bu_bitv *face_mode)	/* a flag for each face indicating
				 * thickness is appended to hit point,
				 * otherwise thickness is centered
				 * about hit point
				 */
{
    return(rt_mk_bot_w_normals(fp, name, botmode, orientation, flags, num_vertices, num_faces, vertices,
			       botfaces, platethickness, face_mode, 0, NULL, NULL));
}


/*************************** code from conv/fast4-g.c ***************************/

static void
do_grid(char *line)
{
    int grid_no;
    fastf_t x, y, z;

    if (RT_G_DEBUG&DEBUG_MEM_FULL &&  bu_mem_barriercheck())
	bu_log("ERROR: bu_mem_barriercheck failed at start of do_grid\n");

    bu_strlcpy(field,  &line[8], sizeof(field));
    grid_no = atoi(field);

    if (grid_no < 1) {
	bu_log("ERROR: grid id number = %d\n", grid_no);
	bu_bomb("BAD GRID ID NUMBER\n");
    }

    bu_strlcpy(field,  &line[24], sizeof(field));
    x = atof(field);

    bu_strlcpy(field,  &line[32], sizeof(field));
    y = atof(field);

    bu_strlcpy(field,  &line[40], sizeof(field));
    z = atof(field);

    while (grid_no > grid_size - 1) {
	grid_size += GRID_BLOCK;
	grid_pts = (point_t *)bu_realloc((char *)grid_pts, grid_size * sizeof(point_t), "fast4-g: grid_pts");
    }

    VSET(grid_pts[grid_no], x*25.4, y*25.4, z*25.4);

    if (grid_no > max_grid_no)
	max_grid_no = grid_no;
    if (RT_G_DEBUG&DEBUG_MEM_FULL &&  bu_mem_barriercheck())
	bu_log("ERROR: bu_mem_barriercheck failed at end of do_grid\n");
}


static void
Add_bot_face(int pt1, int pt2, int pt3, fastf_t thick, int pos)
{

    if (pt1 == pt2 || pt2 == pt3 || pt1 == pt3) {
	bu_log("Add_bot_face: ignoring degenerate triangle in group %d component %d\n", group_id, comp_id);
	return;
    }

    if (pos == 0)	/* use default */
	pos = POS_FRONT;

    if (mode == PLATE_MODE) {
	if (pos != POS_CENTER && pos != POS_FRONT) {
	    bu_log("Add_bot_face: illegal postion parameter (%d), must be one or two (ignoring face for group %d component %d)\n", pos, group_id, comp_id);
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
	bu_log("memory corrupted at end of Add_bot_face()\n");
}


static void
do_tri(char *line)
{
    int element_id;
    int pt1, pt2, pt3;
    fastf_t thick;
    int pos;

    if (debug)
	bu_log("do_tri: %s\n", line);

    bu_strlcpy(field,  &line[8], sizeof(field));
    element_id = atoi(field);

    if (!bot)
	bot = element_id;

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

    bu_strlcpy(field,  &line[24], sizeof(field));
    pt1 = atoi(field);

    bu_strlcpy(field,  &line[32], sizeof(field));
    pt2 = atoi(field);

    bu_strlcpy(field,  &line[40], sizeof(field));
    pt3 = atoi(field);

    thick = 0.0;
    pos = 0;

    if (mode == PLATE_MODE) {
	bu_strlcpy(field,  &line[56], sizeof(field));
	thick = atof(field) * 25.4;

	bu_strlcpy(field,  &line[64], sizeof(field));
	pos = atoi(field);
	if (pos == 0)
	    pos = POS_FRONT;

	if (debug)
	    bu_log("\tplate mode: thickness = %f\n", thick);

    }

    if (bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck())
	bu_log("memory corrupted before call to Add_bot_face()\n");

    Add_bot_face(pt1, pt2, pt3, thick, pos);

    if (bu_debug&BU_DEBUG_MEM_CHECK &&  bu_mem_barriercheck())
	bu_log("memory corrupted after call to Add_bot_face()\n");
}


static void
do_quad(char *line)
{
    int element_id;
    int pt1, pt2, pt3, pt4;
    fastf_t thick = 0.0;
    int pos = 0;

    bu_strlcpy(field,  &line[8], sizeof(field));
    element_id = atoi(field);

    if (debug)
	bu_log("do_quad: %s\n", line);

    if (!bot)
	bot = element_id;

    if (faces == NULL) {
	faces = (int *)bu_malloc(GRID_BLOCK*3*sizeof(int), "faces");
	thickness = (fastf_t *)bu_malloc(GRID_BLOCK*sizeof(fastf_t), "thickness");
	facemode = (char *)bu_malloc(GRID_BLOCK*sizeof(char), "facemode");
	face_size = GRID_BLOCK;
	face_count = 0;
    }

    bu_strlcpy(field,  &line[24], sizeof(field));
    pt1 = atoi(field);

    bu_strlcpy(field,  &line[32], sizeof(field));
    pt2 = atoi(field);

    bu_strlcpy(field,  &line[40], sizeof(field));
    pt3 = atoi(field);

    bu_strlcpy(field,  &line[48], sizeof(field));
    pt4 = atoi(field);

    if (mode == PLATE_MODE) {
	bu_strlcpy(field,  &line[56], sizeof(field));
	thick = atof(field) * 25.4;

	bu_strlcpy(field,  &line[64], sizeof(field));
	pos = atoi(field);

	if (pos == 0)	/* use default */
	    pos = POS_FRONT;

	if (pos != POS_CENTER && pos != POS_FRONT) {
	    bu_log("do_quad: illegal postion parameter (%d), must be one or two\n", pos);
	    bu_log("\telement %d, component %d, group %d\n", element_id, comp_id, group_id);
	    return;
	}
    }

    Add_bot_face(pt1, pt2, pt3, thick, pos);
    Add_bot_face(pt1, pt3, pt4, thick, pos);
}


static void
make_bot_object(const char *name,
		struct rt_wdb *wdbp)
{
    int i;
    int max_pt=0, min_pt=999999;
    int num_vertices;
    struct bu_bitv *bv=NULL;
    int bot_mode;
    int count;
    struct rt_bot_internal bot_ip;

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
	VMOVE(&bot_ip.vertices[i*3], grid_pts[min_pt+i])

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
    } else
	bot_mode = RT_BOT_SOLID;

    bot_ip.mode = bot_mode;
    bot_ip.orientation = RT_BOT_UNORIENTED;
    bot_ip.bot_flags = 0;

    count = rt_bot_vertex_fuse(&bot_ip);
    count = rt_bot_face_fuse(&bot_ip);
    if (count)
	bu_log("WARNING: %d duplicate faces eliminated from group %d component %d\n", count, group_id, comp_id);

    rt_mk_bot(wdbp, name, bot_mode, RT_BOT_UNORIENTED, 0,
	      bot_ip.num_vertices, bot_ip.num_faces, bot_ip.vertices,
	      bot_ip.faces, bot_ip.thickness, bot_ip.face_mode);

    if (mode == PLATE_MODE) {
	bu_free((char *)bot_ip.thickness, "BOT thickness");
	bu_free((char *)bot_ip.face_mode, "BOT face_mode");
    }
    bu_free((char *)bot_ip.vertices, "BOT vertices");
    bu_free((char *)bot_ip.faces, "BOT faces");
}


/*************************** Start of new code. ***************************/

#define FIND_NEWLINE(_cp, _eosFlag) \
    while (*(_cp) != '\0' && \
	   *(_cp) != '\n') \
	++(_cp); \
    \
    if (*(_cp) == '\0') \
	_eosFlag = 1; \
    else \
	*(_cp) = '\0';

int
ged_importFg4Section(struct ged *gedp, int argc, const char *argv[])
{
    char *cp;
    char *line;
    char *lines;
    int eosFlag = 0;
    static const char *usage = "obj section";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    grid_size = GRID_BLOCK;
    grid_pts = (point_t *)bu_malloc(grid_size * sizeof(point_t) ,
				    "importFg4Section: grid_pts");

    lines = strdup(argv[2]);
    cp = line = lines;

    FIND_NEWLINE(cp, eosFlag);

    bu_strlcpy(field, line+8, sizeof(field));
    group_id = atoi(field);

    bu_strlcpy(field, line+16, sizeof(field));
    comp_id = atoi(field);

    region_id = group_id * 1000 + comp_id;

    if (comp_id > 999) {
	bu_log("Illegal component id number %d, changed to 999\n", comp_id);
	comp_id = 999;
    }

    bu_strlcpy(field, line+24, sizeof(field));
    mode = atoi(field);
    if (mode != 1 && mode != 2) {
	bu_log("Illegal mode (%d) for group %d component %d, using volume mode\n",
	       mode, group_id, comp_id);
	mode = 2;
    }

    while (!eosFlag) {
	++cp;
	line = cp;
	FIND_NEWLINE(cp, eosFlag);

	if (!strncmp(line, "GRID", 4))
	    do_grid(line);
	else if (!strncmp(line, "CTRI", 4))
	    do_tri(line);
	else if (!strncmp(line, "CQUAD", 4))
	    do_quad(line);
    }

    make_bot_object(argv[1], gedp->ged_wdbp);
    free((void *)lines);
    bu_free((void *)grid_pts, "importFg4Section: grid_pts");

    /* free memory associated with globals */
    bu_free((void *)faces, "importFg4Section: faces");
    bu_free((void *)thickness, "importFg4Section: thickness");
    bu_free((void *)facemode, "importFg4Section: facemode");

    faces = NULL;
    thickness = NULL;
    facemode = NULL;

    return TCL_OK;
}
/** @} */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
