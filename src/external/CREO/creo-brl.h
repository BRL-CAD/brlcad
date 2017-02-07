/*                   C R E O - B R L . H 
 * BRL-CAD
 *
 * Copyright (c) 2017 United States Government as represented by
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
 */
/** @file creo-brl.h
 *
 */

#include "common.h"

#include <set>

#ifndef _WSTUDIO_DEFINED
# define _WSTUDIO_DEFINED
#endif
extern "C" {
#include <ProToolkit.h>
#include <ProArray.h>
#include <ProAsmcomppath.h>
#include <ProAssembly.h>
#include <ProFaminstance.h>
#include <ProFeatType.h>
#include <ProHole.h>
#include <ProMdl.h>
#include <ProMdlUnits.h>
#include <ProMenuBar.h>
#include <ProMessage.h>
#include <ProMode.h>
#include <ProNotify.h>
#include <ProPart.h>
#include <ProSkeleton.h>
#include <ProSolid.h>
#include <ProSurface.h>
#include <ProUICheckbutton.h>
#include <ProUICmd.h>
#include <ProUIDialog.h>
#include <ProUIInputpanel.h>
#include <ProUILabel.h>
#include <ProUIMessage.h>
#include <ProUIPushbutton.h>
#include <ProUIRadiogroup.h>
#include <ProUITextarea.h>
#include <ProUtil.h>
#include <ProWindows.h>
#include <PtApplsUnicodeUtils.h>
#include <pd_proto.h>

#include "vmath.h"
#include "bu.h"
#include "bn.h"
}

struct StrCmp {
    bool operator()(struct bu_vls *str1, struct bu_vls *str2) const {
	return (bu_strcmp(bu_vls_addr(str1), bu_vls_addr(str2)) < 0);
    }
};


struct WStrCmp {
    bool operator()(wchar_t *str1, wchar_t *str2) const {
	return (wcscmp(str1, str2) < 0);
    }
};

extern "C" {
static wchar_t  MSGFIL[] = {'c', 'r', 'e', 'o', '-', 'b', 'r', 'l', '-', 'm', 's', 'g', '.', 't', 'x', 't', '\0'};

static double creo_to_brl_conv=25.4;	/* inches to mm */

static ProBool do_facets_only;	/* flag to indicate no CSG should be done */
static ProBool get_normals;	/* flag to indicate surface normals should be extracted from geometry */
static ProBool do_elims;	/* flag to indicate that small features are to be eliminated */
static double max_error=1.5;	/* (mm) maximum allowable error in facetized approximation */
static double min_error=1.5;	/* (mm) maximum allowable error in facetized approximation */
static double tol_dist=0.0005;	/* (mm) minimum distance between two distinct vertices */
static double max_angle_cntrl=0.5;	/* max angle control for tessellation ( 0.0 - 1.0 ) */
static double min_angle_cntrl=0.5;	/* min angle control for tessellation ( 0.0 - 1.0 ) */
static int max_to_min_steps = 1;	/* number of steps between max and min */
static double error_increment=0.0;
static double angle_increment=0.0;
static double local_tol=0.0;	/* tolerance in Pro/E units */
static double local_tol_sq=0.0;	/* tolerance squared */
static double min_hole_diameter=0.0; /* if > 0.0, all holes features smaller than this will be deleted */
static double min_chamfer_dim=0.0;   /* if > 0.0, all chamfers with both dimensions less
				      * than this value will be deleted */
static double min_round_radius=0.0;  /* if > 0.0, all rounds with radius less than this
				      * value will be deleted */
static int *feat_ids_to_delete=NULL; /* list of hole features to delete */
static int feat_id_len=0;		/* number of available slots in the above array */
static int feat_id_count=0;		/* number of hole features actually in the above list */
#define FEAT_ID_BLOCK	64		/* number of slots to allocate in above list */

static struct bu_hash_tbl *name_hash;
#define NUM_HASH_TABLE_BINS	4096	/* number of bins for part number to part name hash table */

static int reg_id = 1000;	/* region ident number (incremented with each part) */

static struct vert_root *vert_tree_root;	/* structure for storing and searching on vertices */
static struct vert_root *norm_tree_root;	/* structure for storing and searching on normals */

static ProTriangle *part_tris=NULL;	/* list of triangles for current part */
static int max_tri=0;			/* number of triangles currently malloced */
static int curr_tri=0;			/* number of triangles currently being used */

#define TRI_BLOCK 512			/* number of triangles to malloc per call */

static int *part_norms=NULL;		/* list of indices into normals (matches part_tris) */

static FILE *outfp=NULL;		/* output file */

static FILE *logger=NULL;			/* log file */

#define LOGGER_TYPE_NONE -1
#define LOGGER_TYPE_FAILURE 0
#define LOGGER_TYPE_SUCCESS 1
#define LOGGER_TYPE_FAILURE_OR_SUCCESS 2
#define LOGGER_TYPE_ALL 3

static int logger_type = LOGGER_TYPE_NONE;

static ProCharName curr_part_name;	/* current part name */
static ProCharName curr_asm_name;	/* current assembly name */
static ProFeattype curr_feat_type;	/* current feature type */

static struct bu_ptbl search_path_list;	/* parsed list of search path directories */
static ProName assem_ext;		/* "asm" */
static ProName part_ext;		/* "prt" */

static ProCharLine astr;		/* buffer for Pro/E output messages */

#define DONE_BLOCK 512			/* number of slots to malloc when above array gets full */

#define COPY_BUFFER_SIZE	1024

/* global variables for dimension visits */
static double radius=0.0, diameter=0.0, distance1=0.0, distance2=0.0;
static int got_diameter=0, got_distance1=0;
static int hole_type;
static int add_cbore;
static int add_csink;
static int hole_depth_type;

static double cb_depth=0.0;		/* counter-bore depth */
static double cb_diam=0.0;		/* counter-bore diam */
static double cs_diam=0.0;		/* counter-sink diam */
static double cs_angle=0.0;		/* counter-sink angle */
static double hole_diam=0.0;		/* drilled hle diameter */
static double hole_depth=0.0;		/* drilled hole depth */
static double drill_angle=0.0;		/* drill tip angle */
#define MIN_RADIUS	1.0e-7		/* BRL-CAD does not allow tgc's with zero radius */
static Pro3dPnt end1, end2;		/* axis endpoints for holes */
std::set<wchar_t *, WStrCmp> done_list_part;	/* list of parts already done */
std::set<wchar_t *, WStrCmp> done_list_asm;	/* list of assemblies already done */
std::set<struct bu_vls *, StrCmp> brlcad_names;	/* BRL-CAD names in use */

/* structure to hold info about CSG operations for current part */
struct csg_ops {
    char op;
    struct bu_vls name;
    struct bu_vls dbput;
    struct csg_ops *next;
};

struct csg_ops *csg_root;

static int hole_no=0;   /* hole counter for unique names */

struct empty_parts {
    char *name;
    struct empty_parts *next;
};

static struct empty_parts *empty_parts_root=NULL;

#define NUM_OBJ_TYPES 629
int obj_type_count[NUM_OBJ_TYPES];
char *obj_type[NUM_OBJ_TYPES];

#define NUM_FEAT_TYPES 314
#define FEAT_TYPE_OFFSET 910
int feat_type_count[NUM_FEAT_TYPES];
char *feat_type[NUM_FEAT_TYPES];

#define MAX_LINE_LEN		256	/* maximum allowed line length for part number to name map file */
#define CREO_NAME_ATTR "CREO_Name"

static char *feat_status[]={
    "PRO_FEAT_ACTIVE",
    "PRO_FEAT_INACTIVE",
    "PRO_FEAT_FAMTAB_SUPPRESSED",
    "PRO_FEAT_SIMP_REP_SUPPRESSED",
    "PRO_FEAT_PROG_SUPPRESSED",
    "PRO_FEAT_SUPPRESSED",
    "PRO_FEAT_UNREGENERATED"
};

}


extern "C" void kill_error_dialog(char *dialog, char *component, ProAppData appdata);
extern "C" void kill_gen_error_dialog(char *dialog, char *component, ProAppData appdata);

extern "C" void do_quit(char *dialog, char *compnent, ProAppData appdata);

extern "C" ProError do_feature_visit(ProFeature *feat, ProError status, ProAppData data);
extern "C" char *get_brlcad_name(char *part_name);
extern "C" struct bu_hash_tbl *create_name_hash(FILE *name_fd);
extern "C" void output_assembly(ProMdl model);
extern "C" int output_part(ProMdl model);

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
