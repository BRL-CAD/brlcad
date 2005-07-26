/*                      P R O E - B R L . C
 * BRL-CAD
 *
 * Copyright (c) 2001-2004 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file proe-brl.c
 *
 *  This code uses the Pro/Toolkit from Pro/Engineer to add a menu item in Pro/E
 *  to convert models to the BRL-CAD ".asc" format (actually a tcl script).
 *
 *  Author -
 *	John R. Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */

#include <math.h>
#include <string.h>
#include <wchar.h>
#include <errno.h>
#include "ProToolkit.h"
#include "ProAssembly.h"
#include "ProMessage.h"
#include "ProMenuBar.h"
#include "ProMode.h"
#include "ProMdl.h"
#include "ProFaminstance.h"
#include "pd_prototype.h"
#include "ProPart.h"
#include "ProSolid.h"
#include "ProSkeleton.h"
#include "ProUIDialog.h"
#include "ProUIInputpanel.h"
#include "ProUILabel.h"
#include "ProUIPushbutton.h"
#include "ProUICheckbutton.h"
#include "ProUITextarea.h"
#include "ProAsmcomppath.h"
#include "ProHole.h"
#include "ProNotify.h"
#include "prodev_light.h"
#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "bn.h"

static wchar_t  MSGFIL[] = {'u','s','e','r','m','s','g','.','t','x','t','\0'};

static char *err_mess_form="During the conversion %d features of part %s\n\
were suppressed. After the conversion was complete, an\n\
attempt was made to unsuppress these same features.\n\
The unsuppression failed, so these features are still\n\
suppressed. Please exit Pro/E without saving any\n\
changes so that this problem will not persist.";

static double proe_to_brl_conv=25.4;	/* inches to mm */

static ProBool do_facets_only;	/* flag to indicate no CSG should be done */
static ProBool get_normals;	/* flag to indicate surface normals should be extracted from geometry */
static ProBool do_elims;	/* flag to indicate that small features are to be eliminated */
static double max_error=1.5;	/* (mm) maximimum allowable error in facetized approximation */
static double tol_dist=0.005;	/* (mm) minimum distance between two distinct vertices */
static double angle_cntrl=0.5;	/* angle control for tessellation ( 0.0 - 1.0 ) */
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
static bu_rb_tree *done_list_part=NULL;	/* list of parts already done */
static bu_rb_tree *done_list_asm=NULL;	/* list of assemblies already done */
static bu_rb_tree *brlcad_names=NULL;	/* list of BRL-CAD names in use */

/* declaration of functions passed to the feature visit routine */
static ProError assembly_comp( ProFeature *feat, ProError status, ProAppData app_data );
static ProError assembly_filter( ProFeature *feat, ProAppData *data );

/* structure to hold info about CSG operations for current part */
struct csg_ops {
	char operator;
	struct bu_vls name;
	struct bu_vls dbput;
	struct csg_ops *next;
};
struct csg_ops *csg_root;
static int hole_no=0;	/* hole counter for unique names */
static char *tgc_format="tgc V {%.25G %.25G %.25G} H {%.25G %.25G %.25G} A {%.25G %.25G %.25G} B {%.25G %.25G %.25G} C {%.25G %.25G %.25G} D {%.25G %.25G %.25G}\n";

/* structure to hold info about a member of the current asssembly
 * this structure is created during feature visit
 */
struct asm_member {
	ProCharName name;
	ProMatrix xform;
	ProMdlType type;
	struct asm_member *next;
};

/* structure to hold info about current assembly
 * members are added during feature visit
 */
struct asm_head {
	ProCharName name;
	ProMdl model;
	struct asm_member *members;
};

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

void
do_initialize()
{
	int i;

	/* initailize */
	bu_ptbl_init( &search_path_list, 8, "search_path" );

	ProStringToWstring( assem_ext, "asm" );
	ProStringToWstring( part_ext, "prt" );

	csg_root = NULL;
	for( i=0 ; i<NUM_OBJ_TYPES ; i++ ) {
		obj_type_count[i] = 0;
		obj_type[i] = NULL;
	}

	for( i=0 ; i<NUM_FEAT_TYPES ; i++ ) {
		feat_type_count[i] = 0;
		feat_type[i] = NULL;
	}

	obj_type[0] = "PRO_TYPE_UNUSED";
	obj_type[1] = "PRO_ASSEMBLY";
	obj_type[2] = "PRO_PART";
	obj_type[3] = "PRO_FEATURE";
	obj_type[4] = "PRO_DRAWING";
	obj_type[5] = "PRO_SURFACE";
	obj_type[6] = "PRO_EDGE";
	obj_type[7] = "PRO_3DSECTION";
	obj_type[8] = "PRO_DIMENSION";
	obj_type[11] = "PRO_2DSECTION";
	obj_type[12] = "PRO_PAT_MEMBER";
	obj_type[13] = "PRO_PAT_LEADER";
	obj_type[19] = "PRO_LAYOUT";
	obj_type[21] = "PRO_AXIS";
	obj_type[25] = "PRO_CSYS";
	obj_type[28] = "PRO_REF_DIMENSION";
	obj_type[32] = "PRO_GTOL";
	obj_type[33] = "PRO_DWGFORM";
	obj_type[34] = "PRO_SUB_ASSEMBLY";
	obj_type[37] = "PRO_MFG";
	obj_type[57] = "PRO_QUILT";
	obj_type[62] = "PRO_CURVE";
	obj_type[66] = "PRO_POINT";
	obj_type[68] = "PRO_NOTE";
	obj_type[69] = "PRO_IPAR_NOTE";
	obj_type[71] = "PRO_EDGE_START";
	obj_type[72] = "PRO_EDGE_END";
	obj_type[74] = "PRO_CRV_START";
	obj_type[75] = "PRO_CRV_END";
	obj_type[76] = "PRO_SYMBOL_INSTANCE";
	obj_type[77] = "PRO_DRAFT_ENTITY";
	obj_type[79] = "PRO_DRAFT_DATUM";
	obj_type[83] = "PRO_DRAFT_GROUP";
	obj_type[84] = "PRO_DRAW_TABLE";
	obj_type[92] = "PRO_VIEW";
	obj_type[96] = "PRO_CABLE";
	obj_type[105] = "PRO_REPORT";
	obj_type[116] = "PRO_MARKUP";
	obj_type[117] = "PRO_LAYER";
	obj_type[121] = "PRO_DIAGRAM";
	obj_type[133] = "PRO_SKETCH_ENTITY";
	obj_type[144] = "PRO_DATUM_TEXT";
	obj_type[145] = "PRO_ENTITY_TEXT";
	obj_type[147] = "PRO_DRAW_TABLE_CELL";
	obj_type[176] = "PRO_DATUM_PLANE";
	obj_type[180] = "PRO_COMP_CRV";
	obj_type[211] = "PRO_BND_TABLE";
	obj_type[240] = "PRO_PARAMETER";
	obj_type[305] = "PRO_DIAGRAM_OBJECT";
	obj_type[308] = "PRO_DIAGRAM_WIRE";
	obj_type[309] = "PRO_SIMP_REP";
	obj_type[371] = "PRO_WELD_PARAMS";
	obj_type[377] = "PRO_SNAP_LINE";
	obj_type[385] = "PRO_EXTOBJ";
	obj_type[500] = "PRO_EXPLD_STATE";
	obj_type[504] = "PRO_CABLE_LOCATION";
	obj_type[533] = "PRO_RELSET";
	obj_type[555] = "PRO_ANALYSIS";
	obj_type[556] = "PRO_SURF_CRV";
	obj_type[625] = "PRO_LOG_SRF";
	obj_type[622] = "PRO_SOLID_GEOMETRY";
	obj_type[626] = "PRO_LOG_EDG";
	obj_type[627] = "PRO_DESKTOP";
	obj_type[628] = "PRO_SYMBOL_DEFINITION";

	feat_type[0] = "PRO_FEAT_FIRST_FEAT";
	feat_type[911 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HOLE";
	feat_type[912 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SHAFT";
	feat_type[913 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ROUND";
	feat_type[914 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CHAMFER";
	feat_type[915 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SLOT";
	feat_type[916 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CUT";
	feat_type[917 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PROTRUSION";
	feat_type[918 - FEAT_TYPE_OFFSET] = "PRO_FEAT_NECK";
	feat_type[919 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FLANGE";
	feat_type[920 - FEAT_TYPE_OFFSET] = "PRO_FEAT_RIB";
	feat_type[921 - FEAT_TYPE_OFFSET] = "PRO_FEAT_EAR";
	feat_type[922 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DOME";
	feat_type[923 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DATUM";
	feat_type[924 - FEAT_TYPE_OFFSET] = "PRO_FEAT_LOC_PUSH";
	feat_type[925 - FEAT_TYPE_OFFSET] = "PRO_FEAT_UDF";
	feat_type[926 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DATUM_AXIS";
	feat_type[927 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRAFT";
	feat_type[928 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SHELL";
	feat_type[929 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DOME2";
	feat_type[930 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CORN_CHAMF";
	feat_type[931 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DATUM_POINT";
	feat_type[932 - FEAT_TYPE_OFFSET] = "PRO_FEAT_IMPORT";
	feat_type[932 - FEAT_TYPE_OFFSET] = "PRO_FEAT_IGES";
	feat_type[933 - FEAT_TYPE_OFFSET] = "PRO_FEAT_COSMETIC";
	feat_type[934 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ETCH";
	feat_type[935 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MERGE";
	feat_type[936 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MOLD";
	feat_type[937 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SAW";
	feat_type[938 - FEAT_TYPE_OFFSET] = "PRO_FEAT_TURN";
	feat_type[939 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MILL";
	feat_type[940 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRILL";
	feat_type[941 - FEAT_TYPE_OFFSET] = "PRO_FEAT_OFFSET";
	feat_type[942 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DATUM_SURF";
	feat_type[943 - FEAT_TYPE_OFFSET] = "PRO_FEAT_REPLACE_SURF";
	feat_type[944 - FEAT_TYPE_OFFSET] = "PRO_FEAT_GROOVE";
	feat_type[945 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE";
	feat_type[946 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DATUM_QUILT";
	feat_type[947 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ASSEM_CUT";
	feat_type[948 - FEAT_TYPE_OFFSET] = "PRO_FEAT_UDF_THREAD";
	feat_type[949 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CURVE";
	feat_type[950 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SRF_MDL";
	feat_type[952 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WALL";
	feat_type[953 - FEAT_TYPE_OFFSET] = "PRO_FEAT_BEND";
	feat_type[954 - FEAT_TYPE_OFFSET] = "PRO_FEAT_UNBEND";
	feat_type[955 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CUT_SMT";
	feat_type[956 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FORM";
	feat_type[957 - FEAT_TYPE_OFFSET] = "PRO_FEAT_THICKEN";
	feat_type[958 - FEAT_TYPE_OFFSET] = "PRO_FEAT_BEND_BACK";
	feat_type[959 - FEAT_TYPE_OFFSET] = "PRO_FEAT_UDF_NOTCH";
	feat_type[960 - FEAT_TYPE_OFFSET] = "PRO_FEAT_UDF_PUNCH";
	feat_type[961 - FEAT_TYPE_OFFSET] = "PRO_FEAT_INT_UDF";
	feat_type[962 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SPLIT_SURF";
	feat_type[963 - FEAT_TYPE_OFFSET] = "PRO_FEAT_GRAPH";
	feat_type[964 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_MFG_PUNCH";
	feat_type[965 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_MFG_CUT";
	feat_type[966 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FLATTEN";
	feat_type[967 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SET";
	feat_type[968 - FEAT_TYPE_OFFSET] = "PRO_FEAT_VDA";
	feat_type[969 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_MFG_FORM";
	feat_type[970 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_PUNCH_PNT";
	feat_type[971 - FEAT_TYPE_OFFSET] = "PRO_FEAT_LIP";
	feat_type[972 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MANUAL";
	feat_type[973 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MFG_GATHER";
	feat_type[974 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MFG_TRIM";
	feat_type[975 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MFG_USEVOL";
	feat_type[976 - FEAT_TYPE_OFFSET] = "PRO_FEAT_LOCATION";
	feat_type[977 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CABLE_SEGM";
	feat_type[978 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CABLE";
	feat_type[979 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CSYS";
	feat_type[980 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CHANNEL";
	feat_type[937 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WIRE_EDM";
	feat_type[981 - FEAT_TYPE_OFFSET] = "PRO_FEAT_AREA_NIBBLE";
	feat_type[982 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PATCH";
	feat_type[983 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PLY";
	feat_type[984 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CORE";
	feat_type[985 - FEAT_TYPE_OFFSET] = "PRO_FEAT_EXTRACT";
	feat_type[986 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MFG_REFINE";
	feat_type[987 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SILH_TRIM";
	feat_type[988 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SPLIT";
	feat_type[989 - FEAT_TYPE_OFFSET] = "PRO_FEAT_EXTEND";
	feat_type[990 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SOLIDIFY";
	feat_type[991 - FEAT_TYPE_OFFSET] = "PRO_FEAT_INTERSECT";
	feat_type[992 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ATTACH";
	feat_type[993 - FEAT_TYPE_OFFSET] = "PRO_FEAT_XSEC";
	feat_type[994 - FEAT_TYPE_OFFSET] = "PRO_FEAT_UDF_ZONE";
	feat_type[995 - FEAT_TYPE_OFFSET] = "PRO_FEAT_UDF_CLAMP";
	feat_type[996 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRL_GRP";
	feat_type[997 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ISEGM";
	feat_type[998 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CABLE_COSM";
	feat_type[999 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SPOOL";
	feat_type[1000 - FEAT_TYPE_OFFSET] = "PRO_FEAT_COMPONENT";
	feat_type[1001 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MFG_MERGE";
	feat_type[1002 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FIXSETUP";
	feat_type[1002 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SETUP";
	feat_type[1003 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FLAT_PAT";
	feat_type[1004 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CONT_MAP";
	feat_type[1005 - FEAT_TYPE_OFFSET] = "PRO_FEAT_EXP_RATIO";
	feat_type[1006 - FEAT_TYPE_OFFSET] = "PRO_FEAT_RIP";
	feat_type[1007 - FEAT_TYPE_OFFSET] = "PRO_FEAT_OPERATION";
	feat_type[1008 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WORKCELL";
	feat_type[1009 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CUT_MOTION";
	feat_type[1013 - FEAT_TYPE_OFFSET] = "PRO_FEAT_BLD_PATH";
	feat_type[1013 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CUSTOMIZE";
	feat_type[1014 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRV_TOOL_SKETCH";
	feat_type[1015 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRV_TOOL_EDGE";
	feat_type[1016 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRV_TOOL_CURVE";
	feat_type[1017 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRV_TOOL_SURF";
	feat_type[1018 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MAT_REMOVAL";
	feat_type[1019 - FEAT_TYPE_OFFSET] = "PRO_FEAT_TORUS";
	feat_type[1020 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE_SET_START";
	feat_type[1021 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE_PNT_PNT";
	feat_type[1022 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE_EXT";
	feat_type[1023 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE_TRIM";
	feat_type[1024 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE_FOLL";
	feat_type[1025 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE_JOIN";
	feat_type[1026 - FEAT_TYPE_OFFSET] = "PRO_FEAT_AUXILIARY";
	feat_type[1027 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE_LINE";
	feat_type[1028 - FEAT_TYPE_OFFSET] = "PRO_FEAT_LINE_STOCK";
	feat_type[1029 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SLD_PIPE ";
	feat_type[1030 - FEAT_TYPE_OFFSET] = "PRO_FEAT_BULK_OBJECT";
	feat_type[1031 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SHRINKAGE  ";
	feat_type[1032 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE_JOINT ";
	feat_type[1033 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE_BRANCH ";
	feat_type[1034 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRV_TOOL_TWO_CNTR";
	feat_type[1035 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SUBHARNESS";
	feat_type[1036 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_OPTIMIZE";
	feat_type[1037 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DECLARE";
	feat_type[1038 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_POPULATE";
	feat_type[1039 - FEAT_TYPE_OFFSET] = "PRO_FEAT_OPER_COMP";
	feat_type[1040 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MEASURE";
	feat_type[1041 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRAFT_LINE";
	feat_type[1042 - FEAT_TYPE_OFFSET] = "PRO_FEAT_REMOVE_SURFS";
	feat_type[1043 - FEAT_TYPE_OFFSET] = "PRO_FEAT_RIBBON_CABLE";
	feat_type[1046 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ATTACH_VOLUME";
	feat_type[1047 - FEAT_TYPE_OFFSET] = "PRO_FEAT_BLD_OPERATION";
	feat_type[1048 - FEAT_TYPE_OFFSET] = "PRO_FEAT_UDF_WRK_REG";
	feat_type[1049 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SPINAL_BEND";
	feat_type[1050 - FEAT_TYPE_OFFSET] = "PRO_FEAT_TWIST";
	feat_type[1051 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FREE_FORM";
	feat_type[1052 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ZONE";
	feat_type[1053 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WELDING_ROD";
	feat_type[1054 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WELD_FILLET";
	feat_type[1055 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WELD_GROOVE";
	feat_type[1056 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WELD_PLUG_SLOT";
	feat_type[1057 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WELD_SPOT";
	feat_type[1058 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_SHEAR";
	feat_type[1059 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PATH_SEGM";
	feat_type[1060 - FEAT_TYPE_OFFSET] = "PRO_FEAT_RIBBON_SEGM";
	feat_type[1059 - FEAT_TYPE_OFFSET] = "PRO_FEAT_RIBBON_PATH";
	feat_type[1060 - FEAT_TYPE_OFFSET] = "PRO_FEAT_RIBBON_EXTEND";
	feat_type[1061 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ASMCUT_COPY";
	feat_type[1062 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DEFORM_AREA";
	feat_type[1063 - FEAT_TYPE_OFFSET] = "PRO_FEAT_RIBBON_SOLID";
	feat_type[1064 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FLAT_RIBBON_SEGM";
	feat_type[1065 - FEAT_TYPE_OFFSET] = "PRO_FEAT_POSITION_FOLD";
	feat_type[1066 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SPRING_BACK";
	feat_type[1067 - FEAT_TYPE_OFFSET] = "PRO_FEAT_BEAM_SECTION";
	feat_type[1068 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SHRINK_DIM";
	feat_type[1070 - FEAT_TYPE_OFFSET] = "PRO_FEAT_THREAD";
	feat_type[1071 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_CONVERSION";
	feat_type[1072 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CMM_MEASSTEP";
	feat_type[1073 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CMM_CONSTR";
	feat_type[1074 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CMM_VERIFY";
	feat_type[1075 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CAV_SCAN_SET";
	feat_type[1076 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CAV_FIT";
	feat_type[1077 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CAV_DEVIATION";
	feat_type[1078 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_ZONE";
	feat_type[1079 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_CLAMP";
	feat_type[1080 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PROCESS_STEP";
	feat_type[1081 - FEAT_TYPE_OFFSET] = "PRO_FEAT_EDGE_BEND";
	feat_type[1082 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRV_TOOL_PROF";
	feat_type[1083 - FEAT_TYPE_OFFSET] = "PRO_FEAT_EXPLODE_LINE";
	feat_type[1084 - FEAT_TYPE_OFFSET] = "PRO_FEAT_GEOM_COPY";
	feat_type[1085 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ANALYSIS";
	feat_type[1086 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WATER_LINE";
	feat_type[1087 - FEAT_TYPE_OFFSET] = "PRO_FEAT_UDF_RMDT";
	feat_type[1088 - FEAT_TYPE_OFFSET] = "PRO_FEAT_VOL_SPLIT";
	feat_type[1089 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WLD_EDG_PREP";
	feat_type[1090 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMM_OFFSET";
	feat_type[1091 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMM_MATREM";
	feat_type[1092 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMM_COSMETIC";
	feat_type[1093 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMM_APPROACH";
	feat_type[1094 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMM_SLOT";
	feat_type[1095 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMM_SHAPE";
	feat_type[1096 - FEAT_TYPE_OFFSET] = "PRO_FEAT_IPM_QUILT";
	feat_type[1097 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRVD";
	feat_type[1098 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_CRN_REL";
	feat_type[1101 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SLDBEND";
	feat_type[1102 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FLATQLT ";
	feat_type[1103 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRV_TOOL_TURN ";
	feat_type[1104 - FEAT_TYPE_OFFSET] = "PRO_FEAT_GROUP_HEAD";
	feat_type[1211 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_PLATE";
	feat_type[1212 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_HOLE  ";
	feat_type[1213 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_CUTOUT";
	feat_type[1214 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_STIFFENER";
	feat_type[1215 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_BEAM";
	feat_type[1216 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_ENDCUT";
	feat_type[1217 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_WLD_FLANGE";
	feat_type[1218 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_COLLAR";
	feat_type[1219 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_DRAW";
	feat_type[1220 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_BRACKET";
	feat_type[1221 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_FOLDED_FLG";
	feat_type[1222 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_BLOCK";
	feat_type[1223 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_BLOCK_DEF";
	feat_type[1105 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FR_SYS";
	feat_type[1106 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_COMPT";
	feat_type[1107 - FEAT_TYPE_OFFSET] = "PRO_FEAT_REFERENCE";
	feat_type[1108 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SHELL_EXP";
	feat_type[1109 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FREEFORM";
	feat_type[1110 - FEAT_TYPE_OFFSET] = "PRO_FEAT_KERNEL ";
	feat_type[1111 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WELD_PROCESS";
	feat_type[1112 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_REP_TMP";
	feat_type[1113 - FEAT_TYPE_OFFSET] = "PRO_FEAT_INSULATION";
	feat_type[1114 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SLD_PIP_INSUL";
	feat_type[1115 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_EXTRACT";
	feat_type[1116 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ASSY_MERGE";
	feat_type[1117 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DS_OPTIMIZE";
	feat_type[1118 - FEAT_TYPE_OFFSET] = "PRO_FEAT_COMP_INTERFACE";
	feat_type[1119 - FEAT_TYPE_OFFSET] = "PRO_FEAT_OLE";
	feat_type[1120 - FEAT_TYPE_OFFSET] = "PRO_FEAT_TERMINATOR";
	feat_type[1121 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WLD_NOTCH";
	feat_type[1122 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ASSY_WLD_NOTCH";

}

static char *feat_status[]={
	"PRO_FEAT_ACTIVE",
	"PRO_FEAT_INACTIVE",
	"PRO_FEAT_FAMTAB_SUPPRESSED",
	"PRO_FEAT_SIMP_REP_SUPPRESSED",
	"PRO_FEAT_PROG_SUPPRESSED",
	"PRO_FEAT_SUPPRESSED",
	"PRO_FEAT_UNREGENERATED"
};

void
lower_case( char *name )
{
	unsigned char *c;

	c = (unsigned char *)name;
	while( *c ) {
		(*c) = tolower( *c );
		c++;
	}
}

void
make_legal( char *name )
{
	unsigned char *c;

	c = (unsigned char *)name;
	while( *c ) {
		if( *c <= ' ' || *c == '/' || *c == '[' || *c == ']' ) {
			*c = '_';
		} else if( *c > '~' ) {
			*c = '_';
		}
		c++;
	}
}

/* create a unique BRL-CAD object name from a possibly illegal name */
char *
create_unique_name( char *name )
{
	struct bu_vls tmp_name;
	int initial_length=0;
	int count=0;

	if( logger ) {
		fprintf( logger, "create_unique_name( %s )\n", name );
	}

	/* if we do not already have a brlcad name tree, create one here */
	if( !brlcad_names ) {
		if( logger ) {
			fprintf( logger, "\tCreating rb tree for brlcad names\n" );
		}
		brlcad_names = bu_rb_create1( "brl-cad names", strcmp );
		bu_rb_uniq_all_on( brlcad_names );
	}

	/* create a unique name */
	bu_vls_init( &tmp_name );
	bu_vls_strcpy( &tmp_name, name );
	lower_case( bu_vls_addr( &tmp_name ) );
	make_legal( bu_vls_addr( &tmp_name ) );
	initial_length = bu_vls_strlen( &tmp_name );
	while( bu_rb_insert( brlcad_names, bu_vls_addr( &tmp_name ) ) < 0 ) {
		char *data;
		if( (data=(char *)bu_rb_search1( brlcad_names, bu_vls_addr( &tmp_name ) ) ) != NULL ) {
			if( logger ) {
				fprintf( logger, "\t\tfound duplicate (%s)\n", data );
				fflush( logger );
			}
		}
		bu_vls_trunc( &tmp_name, initial_length );
		count++;
		bu_vls_printf( &tmp_name, "_%d", count );
		if( logger ) {
			fprintf( logger, "\tTrying %s\n", bu_vls_addr( &tmp_name ) );
		}
	}

	if( logger ) {
		fprintf( logger, "\tnew name for %s is %s\n", name, bu_vls_addr( &tmp_name ) );
	}
	return( bu_vls_strgrab( &tmp_name ) );
}

char *
get_brlcad_name( char *part_name )
{
	char *brlcad_name=NULL;
	struct bu_hash_entry *entry=NULL, *prev=NULL;
	int new_entry=0;
	unsigned long index=0;
	char *name_copy;

	name_copy = bu_strdup( part_name );
	lower_case( name_copy );

	if( logger ) {
		fprintf( logger, "get_brlcad_name( %s )\n", name_copy );
	}

	/* find name for this part in hash table */
	entry = bu_find_hash_entry( name_hash, (unsigned char *)name_copy, strlen( name_copy ), &prev, &index );

	if( entry ) {
		if( logger ) {
			fprintf( logger, "\treturning %s\n", (char *)bu_get_hash_value( entry ) );
		}
		bu_free( name_copy, "name_copy" );
		return( (char *)bu_get_hash_value( entry ) );
	} else {

		/* must create a new name */
		brlcad_name = create_unique_name( name_copy );
		entry = bu_hash_add_entry( name_hash, (unsigned char *)name_copy, strlen( name_copy ), &new_entry );
		bu_set_hash_value( entry, (unsigned char *)brlcad_name );
		if( logger ) {
			fprintf( logger, "\tCreating new brlcad name (%s) for part (%s)\n", brlcad_name, name_copy );
		}
		bu_free( name_copy, "name_copy" );
		return( brlcad_name );
	}
}

void
model_units( ProMdl model )
{
	int unit_subtype;
	wchar_t unit_name[PRO_NAME_SIZE];
	double proe_conv;

	/* get units, and adjust conversion factor */
	if( prodb_get_model_units( model, LENGTH_UNIT, &unit_subtype,
				   unit_name, &proe_conv ) ) {
		if( unit_subtype == UNIT_MM )
			proe_to_brl_conv = 1.0;
		else
			proe_to_brl_conv = proe_conv * 25.4;
	} else {
		ProMessageDisplay(MSGFIL, "USER_NO_UNITS" );
		ProMessageClear();
		if( logger ) {
			fprintf( logger, "No units specified, assuming inches\n" );
		}
		fprintf( stderr, "No units specified, assuming inches\n" );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		proe_to_brl_conv = 25.4;
	}

	/* adjust tolerance for Pro/E units */
	local_tol = tol_dist / proe_to_brl_conv;
	local_tol_sq = local_tol * local_tol;

	if( logger ) {
		fprintf( logger, "Units: %s, proe_to_brl_conv = %g\n",
			 ProWstringToString( astr, unit_name ),
			 proe_to_brl_conv );
	}
}

/* routine to free the list of CSG operations */
void
free_csg_ops()
{
	struct csg_ops *ptr1, *ptr2;

	ptr1 = csg_root;

	while( ptr1 ) {
		ptr2 = ptr1->next;
		bu_vls_free( &ptr1->name );
		bu_vls_free( &ptr1->dbput );
		bu_free( ptr1, "csg op" );
		ptr1 = ptr2;
	}

	csg_root = NULL;
}

void doit( char *dialog, char *compnent, ProAppData appdata );
void do_quit( char *dialog, char *compnent, ProAppData appdata );

void
add_to_done_part( wchar_t *name )
{
	wchar_t *name_copy;

	if( logger ) {
		fprintf( logger, "Added %s to list of done parts\n", ProWstringToString( astr, name ) );
	}

	if( !done_list_part ) {
		done_list_part = bu_rb_create1( "Names of Parts already Processed",
					   wcscmp );
		bu_rb_uniq_all_on( done_list_part );
	}

	name_copy = ( wchar_t *)bu_calloc( wcslen( name ) + 1, sizeof( wchar_t ),
					   "part name for done list" );
	wcscpy( name_copy, name );

	if( bu_rb_insert( done_list_part, name_copy ) < 0 ) {
		bu_free( (char *)name_copy, "part name for done list" );
	}
}

int
already_done_part( wchar_t *name )
{
	char *found=NULL;

	if( !done_list_part )
		return( 0 );

	found = bu_rb_search1( done_list_part, (void *)name );
	if( !found ) {
		return( 0 );
	} else {
		return( 1 );
	}
}


void
add_to_done_asm( wchar_t *name )
{
	wchar_t *name_copy;

	if( logger ) {
		fprintf( logger, "Added %s to list of done assemblies\n", ProWstringToString( astr, name ) );
	}

	if( !done_list_asm ) {
		done_list_asm = bu_rb_create1( "Names of Asms already Processed",
					   wcscmp );
		bu_rb_uniq_all_on( done_list_asm );
	}

	name_copy = ( wchar_t *)bu_calloc( wcslen( name ) + 1, sizeof( wchar_t ),
					   "asm name for done list" );
	wcscpy( name_copy, name );

	if( bu_rb_insert( done_list_asm, name_copy ) < 0 ) {
		bu_free( (char *)name_copy, "asm name for done list" );
	}
}

int
already_done_asm( wchar_t *name )
{
	char *found=NULL;

	if( !done_list_asm )
		return( 0 );

	found = bu_rb_search1(done_list_asm, (void *)name );
	if( !found ) {
		return( 0 );
	} else {
		return( 1 );
	}
}

void
add_to_empty_list( char *name )
{
	struct empty_parts *ptr;
	int found=0;

	if( logger ) {
		fprintf( logger, "Adding %s to list of empty parts\n", name );
	}

	if( empty_parts_root == NULL ) {
		empty_parts_root = (struct empty_parts *)bu_malloc( sizeof( struct empty_parts ),
								    "empty parts root");
		ptr = empty_parts_root;
	} else {
		ptr = empty_parts_root;
		while( !found && ptr->next ) {
			if( !strcmp( name, ptr->name ) ) {
				found = 1;
				break;
			}
			ptr = ptr->next;
		}
		if( !found ) {
			ptr->next = (struct empty_parts *)bu_malloc( sizeof( struct empty_parts ),
								     "empty parts node");
			ptr = ptr->next;
		}
	}

	ptr->next = NULL;
	ptr->name = (char *)strdup( name );
}

void
kill_empty_parts()
{
	struct empty_parts *ptr;

	if( logger ) {
		fprintf( logger, "Adding code to remove empty parts:\n" );
	}

	ptr = empty_parts_root;
	while( ptr ) {
		if( logger ) {
			fprintf( logger, "\t%s\n", ptr->name );
		}
		fprintf( outfp, "set combs [find %s]\n", ptr->name );
		fprintf( outfp, "foreach comb $combs {\n\tcatch {rm $comb %s}\n}\n", ptr->name );
		ptr = ptr->next;
	}
}

void
free_empty_parts()
{
	struct empty_parts *ptr, *prev;

	if( logger ) {
		fprintf( logger, "Free empty parts list\n" );
	}

	ptr = empty_parts_root;
	while( ptr ) {
		prev = ptr;
		ptr = ptr->next;
		bu_free( prev->name, "empty part node name" );
		bu_free( prev, "empty part node" );
	}

	empty_parts_root = NULL;

	if( logger ) {
		fprintf( logger, "Free empty parts list done\n" );
	}
}

/* routine to check for bad triangles
 * only checks for triangles with duplicate vertices
 */
int
bad_triangle( int v1, int v2, int v3 )
{
	double dist;
	double coord;
	int i;

	if( v1 == v2 || v2 == v3 || v1 == v3 )
		return( 1 );

	dist = 0;
	for( i=0 ; i<3 ; i++ ) {
		coord = vert_tree_root->the_array[v1*3+i] - vert_tree_root->the_array[v2*3+i];
		dist += coord * coord;
	}
	dist = sqrt( dist );
	if( dist < local_tol ) {
		return( 1 );
	}

	dist = 0;
	for( i=0 ; i<3 ; i++ ) {
		coord = vert_tree_root->the_array[v2*3+i] - vert_tree_root->the_array[v3*3+i];
		dist += coord * coord;
	}
	dist = sqrt( dist );
	if( dist < local_tol ) {
		return( 1 );
	}

	dist = 0;
	for( i=0 ; i<3 ; i++ ) {
		coord = vert_tree_root->the_array[v1*3+i] - vert_tree_root->the_array[v3*3+i];
		dist += coord * coord;
	}
	dist = sqrt( dist );
	if( dist < local_tol ) {
		return( 1 );
	}

	return( 0 );
}

/* routine to add a new triangle and its normals to the current part */
void
add_triangle_and_normal( int v1, int v2, int v3, int n1, int n2, int n3 )
{
	if( curr_tri >= max_tri ) {
		/* allocate more memory for triangles and normals */
		max_tri += TRI_BLOCK;
		part_tris = (ProTriangle *)bu_realloc( part_tris, sizeof( ProTriangle ) * max_tri,
						       "part triangles");
		if( !part_tris ) {
			(void)ProMessageDisplay(MSGFIL, "USER_ERROR",
						"Failed to allocate memory for part triangles" );
			fprintf( stderr, "Failed to allocate memory for part triangles\n" );
			(void)ProWindowRefresh( PRO_VALUE_UNUSED );
			exit( 1 );
		}
		part_norms = (int *)bu_realloc( part_norms, sizeof( int ) * max_tri * 3,
						"part normals");
	}

	/* fill in triangle info */
	part_tris[curr_tri][0] = v1;
	part_tris[curr_tri][1] = v2;
	part_tris[curr_tri][2] = v3;

	part_norms[curr_tri*3]     = n1;
	part_norms[curr_tri*3 + 1] = n2;
	part_norms[curr_tri*3 + 2] = n3;

	/* increment count */
	curr_tri++;
}


/* routine to add a new triangle to the current part */
void
add_triangle( int v1, int v2, int v3 )
{
	if( curr_tri >= max_tri ) {
		/* allocate more memory for triangles */
		max_tri += TRI_BLOCK;
		part_tris = (ProTriangle *)bu_realloc( part_tris, sizeof( ProTriangle ) * max_tri,
						       "part rtiangles");
		if( !part_tris ) {
			(void)ProMessageDisplay(MSGFIL, "USER_ERROR",
						"Failed to allocate memory for part triangles" );
			fprintf( stderr, "Failed to allocate memory for part triangles\n" );
			(void)ProWindowRefresh( PRO_VALUE_UNUSED );
			exit( 1 );
		}
	}

	/* fill in triangle info */
	part_tris[curr_tri][0] = v1;
	part_tris[curr_tri][1] = v2;
	part_tris[curr_tri][2] = v3;

	/* increment count */
	curr_tri++;
}

ProError
check_dimension( ProDimension *dim, ProError status, ProAppData data )
{
	ProDimensiontype dim_type;
	ProError ret;
	double tmp;

	if( (ret=ProDimensionTypeGet( dim, &dim_type ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "ProDimensionTypeGet Failed for %s\n", curr_part_name );
		return ret;
	}

	switch( dim_type ) {
		case PRODIMTYPE_RADIUS:
			if( (ret=ProDimensionValueGet( dim, &radius ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "ProDimensionValueGet Failed for %s\n", curr_part_name );
				return ret;
			}
			diameter = 2.0 * radius;
			got_diameter = 1;
			break;
		case PRODIMTYPE_DIAMETER:
			if( (ret=ProDimensionValueGet( dim, &diameter ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "ProDimensionValueGet Failed for %s\n", curr_part_name );
				return ret;
			}
			radius = diameter / 2.0;
			got_diameter = 1;
			break;
		case PRODIMTYPE_LINEAR:
			if( (ret=ProDimensionValueGet( dim, &tmp ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "ProDimensionValueGet Failed for %s\n", curr_part_name );
				return ret;
			}
			if( got_distance1 ) {
				distance2 = tmp;
			} else {
				got_distance1 = 1;
				distance1 = tmp;
			}
			break;
	}

	return PRO_TK_NO_ERROR;
}

ProError
dimension_filter( ProDimension *dim, ProAppData data ) {
	return PRO_TK_NO_ERROR;
}

void
Add_to_feature_delete_list( int id )
{
	if( feat_id_count >= feat_id_len ) {
		feat_id_len += FEAT_ID_BLOCK;
		feat_ids_to_delete = (int *)bu_realloc( (char *)feat_ids_to_delete,
						     feat_id_len * sizeof( int ),
							"fetaure ids to delete");

	}
	feat_ids_to_delete[feat_id_count++] = id;

	if( logger ) {
		fprintf( logger, "Adding feature %d to list of features to delete (list length = %d)\n",
			 id, feat_id_count );
	}
}

ProError
geomitem_visit( ProGeomitem *item, ProError status, ProAppData data )
{
	ProGeomitemdata *geom;
	ProCurvedata *crv;
	ProError ret;

	if( (ret=ProGeomitemdataGet( item, &geom )) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get geomitem for type %d\n",
			 item->type );
		return ret;
	}

	crv = PRO_CURVE_DATA( geom );
	if( (ret=ProLinedataGet( crv, end1, end2 ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get line data for axis\n" );
		return ret;
	}

	return PRO_TK_NO_ERROR;
}

ProError
geomitem_filter( ProGeomitem *item, ProAppData data )
{
	return PRO_TK_NO_ERROR;
}

ProError
hole_elem_visit( ProElement elem_tree, ProElement elem, ProElempath elem_path, ProAppData data )
{
	ProError ret;
	ProElemId elem_id;
	ProValue val_junk;
	ProValueData val;

	if( (ret=ProElementIdGet( elem, &elem_id ) ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get element id!!!\n" );
		return ret;
	}

	switch( elem_id ) {
		case PRO_E_HLE_ADD_CBORE:
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			add_cbore = val.v.i;
			break;
		case PRO_E_HLE_ADD_CSINK:
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			add_csink = val.v.i;
			break;
		case PRO_E_DIAMETER:
			/* diameter of straight hole */
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			hole_diam = val.v.d;
			break;
		case PRO_E_HLE_HOLEDIAM:
			/* diameter of main portion of standard drilled hole */
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			hole_diam = val.v.d;
			break;
		case PRO_E_HLE_CBOREDEPTH:
			/* depth of counterbore */
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			cb_depth = val.v.d;
			break;
		case PRO_E_HLE_CBOREDIAM:
			/* diameter of counterbore */
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			cb_diam = val.v.d;
			break;
		case PRO_E_HLE_CSINKANGLE:
			/* angle of countersink (degrees ) */
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			cs_angle = val.v.d;
			break;
		case PRO_E_HLE_CSINKDIAM:
			/* diameter of countersink */
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			cs_diam = val.v.d;
			break;
		case PRO_E_HLE_DRILLDEPTH:
			/* overall depth of standard drilled hole without drill tip */
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			hole_depth = val.v.d;
			break;
		case PRO_E_HLE_DRILLANGLE:
			/* drill tip angle (degrees) */
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			drill_angle = val.v.d;
			break;
		case PRO_E_HLE_DEPTH:
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			hole_depth_type = val.v.i;
			break;
		case PRO_E_HLE_TYPE_NEW:
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			hole_type = val.v.i;
			break;
		case PRO_E_HLE_STAN_TYPE:
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			break;
		case PRO_E_STD_EDGE_CHAMF_DIM1:
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			break;
		case PRO_E_STD_EDGE_CHAMF_DIM2:
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			break;
		case PRO_E_STD_EDGE_CHAMF_ANGLE:
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			break;
		case PRO_E_STD_EDGE_CHAMF_DIM:
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			break;
		case PRO_E_STD_EDGE_CHAMF_SCHEME:
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			break;
		case PRO_E_EXT_DEPTH_FROM_VALUE:
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			break;
		case PRO_E_EXT_DEPTH_TO_VALUE:
			if( (ret=ProElementValueGet( elem, &val_junk )) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value\n" );
				return ret;
			}
			if( (ret=ProValueDataGet( val_junk, &val ) ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to get value data\n" );
				return ret;
			}
			break;
	}

	return PRO_TK_NO_ERROR;
}

ProError
hole_elem_filter( ProElement elem_tree, ProElement elem, ProElempath elem_path, ProAppData data )
{
	return PRO_TK_NO_ERROR;
}

/* Subtract_hole()
 *	routine to create TGC primitives to make holes
 *
 *	return value:
 *		0 - do not delete this hole feature before tessellating
 *		1 - delete this hole feature before tessellating
 */
int
Subtract_hole()
{
	struct csg_ops *csg;
	vect_t a, b, c, d, h;

	if( do_facets_only ) {
		if( diameter < min_hole_diameter )
			return 1;
		else
			return 0;
	}

	if( logger ) {
		fprintf( logger, "Doing a CSG hole subtraction\n" );
	}

	/* make a replacement hole using CSG */
	if( hole_type == PRO_HLE_NEW_TYPE_STRAIGHT ) {
		/* plain old striaght hole */

		if( diameter < min_hole_diameter )
			return 1;
		if( !csg_root ) {
			csg_root = (struct csg_ops *)bu_malloc( sizeof( struct csg_ops ), "csg root" );
			csg = csg_root;
			csg->next = NULL;
		} else {
			csg = (struct csg_ops *)bu_malloc( sizeof( struct csg_ops ), "csg op" );
			csg->next = csg_root;
			csg_root = csg;
		}
		bu_vls_init( &csg->name );
		bu_vls_init( &csg->dbput );

		csg->operator = '-';
		hole_no++;
		bu_vls_printf( &csg->name, "hole.%d ", hole_no );
		VSUB2( h, end1, end2 );
		bn_vec_ortho( a, h );
		VCROSS( b, a, h );
		VUNITIZE( b );
		VSCALE( end2, end2, proe_to_brl_conv );
		VSCALE( a, a, radius*proe_to_brl_conv );
		VSCALE( b, b, radius*proe_to_brl_conv );
		VSCALE( h, h, proe_to_brl_conv );
		bu_vls_printf( &csg->dbput, tgc_format,
			       V3ARGS( end2 ),
			       V3ARGS( h ),
			       V3ARGS( a ),
			       V3ARGS( b ),
			       V3ARGS( a ),
			       V3ARGS( b ) );
	} else if( hole_type == PRO_HLE_NEW_TYPE_STANDARD ) {
		/* drilled hole with possible countersink and counterbore */
		point_t start;
		vect_t dir;
		double cb_radius;
		double accum_depth=0.0;
		double hole_radius=hole_diam / 2.0;

		if( hole_diam < min_hole_diameter )
			return 1;

		VSUB2( dir, end1, end2 );
		VUNITIZE( dir );

		VMOVE( start, end2 );
		VSCALE( start, start, proe_to_brl_conv );

		if( add_cbore == PRO_HLE_ADD_CBORE ) {

			if( !csg_root ) {
				csg_root = (struct csg_ops *)bu_malloc( sizeof( struct csg_ops ), "csg root" );
				csg = csg_root;
				csg->next = NULL;
			} else {
				csg = (struct csg_ops *)bu_malloc( sizeof( struct csg_ops ), "csg op" );
				csg->next = csg_root;
				csg_root = csg;
			}
			bu_vls_init( &csg->name );
			bu_vls_init( &csg->dbput );
			
			csg->operator = '-';
			hole_no++;
			bu_vls_printf( &csg->name, "hole.%d ", hole_no );
			bn_vec_ortho( a, dir );
			VCROSS( b, a, dir );
			VUNITIZE( b );
			cb_radius = cb_diam * proe_to_brl_conv / 2.0;
			VSCALE( a, a, cb_radius );
			VSCALE( b, b, cb_radius );
			VSCALE( h, dir, cb_depth * proe_to_brl_conv );
			bu_vls_printf( &csg->dbput, tgc_format,
			       V3ARGS( start ),
			       V3ARGS( h ),
			       V3ARGS( a ),
			       V3ARGS( b ),
			       V3ARGS( a ),
			       V3ARGS( b ) );
			VADD2( start, start, h );
			accum_depth += cb_depth;
			cb_diam = 0.0;
			cb_depth = 0.0;
		}
		if( add_csink == PRO_HLE_ADD_CSINK ) {
			double cs_depth;
			double cs_radius=cs_diam / 2.0;

			if( !csg_root ) {
				csg_root = (struct csg_ops *)bu_malloc( sizeof( struct csg_ops ), "csg root" );
				csg = csg_root;
				csg->next = NULL;
			} else {
				csg = (struct csg_ops *)bu_malloc( sizeof( struct csg_ops ), "csg op" );
				csg->next = csg_root;
				csg_root = csg;
			}
			bu_vls_init( &csg->name );
			bu_vls_init( &csg->dbput );
			
			csg->operator = '-';
			hole_no++;
			bu_vls_printf( &csg->name, "hole.%d ", hole_no );
			cs_depth = (cs_diam - hole_diam) / (2.0 * tan( cs_angle * M_PI / 360.0 ) );
			bn_vec_ortho( a, dir );
			VCROSS( b, a, dir );
			VUNITIZE( b );
			VMOVE( c, a );
			VMOVE( d, b );
			VSCALE( h, dir, cs_depth * proe_to_brl_conv );
			VSCALE( a, a, cs_radius * proe_to_brl_conv );
			VSCALE( b, b, cs_radius * proe_to_brl_conv );
			VSCALE( c, c, hole_diam * proe_to_brl_conv / 2.0 );
			VSCALE( d, d, hole_diam * proe_to_brl_conv / 2.0 );
			bu_vls_printf( &csg->dbput, tgc_format,
			       V3ARGS( start ),
			       V3ARGS( h ),
			       V3ARGS( a ),
			       V3ARGS( b ),
			       V3ARGS( c ),
			       V3ARGS( d ) );
			VADD2( start, start, h );
			accum_depth += cs_depth;
			cs_diam = 0.0;
			cs_angle = 0.0;
		}

		if( !csg_root ) {
			csg_root = (struct csg_ops *)bu_malloc( sizeof( struct csg_ops ), "csg root" );
			csg = csg_root;
			csg->next = NULL;
		} else {
			csg = (struct csg_ops *)bu_malloc( sizeof( struct csg_ops ), "csg op" );
			csg->next = csg_root;
			csg_root = csg;
		}
		bu_vls_init( &csg->name );
		bu_vls_init( &csg->dbput );
			
		csg->operator = '-';
		hole_no++;
		bu_vls_printf( &csg->name, "hole.%d ", hole_no );
		bn_vec_ortho( a, dir );
		VCROSS( b, a, dir );
		VUNITIZE( b );
		VMOVE( c, a );
		VMOVE( d, b );
		VSCALE( a, a, hole_radius * proe_to_brl_conv );
		VSCALE( b, b, hole_radius * proe_to_brl_conv );
		VSCALE( c, c, hole_radius * proe_to_brl_conv );
		VSCALE( d, d, hole_radius * proe_to_brl_conv );
		VSCALE( h, dir, (hole_depth - accum_depth) * proe_to_brl_conv );
		bu_vls_printf( &csg->dbput, tgc_format,
			       V3ARGS( start ),
			       V3ARGS( h ),
			       V3ARGS( a ),
			       V3ARGS( b ),
			       V3ARGS( c ),
			       V3ARGS( d ) );
		VADD2( start, start, h );
		hole_diam = 0.0;
		hole_depth = 0.0;
		if( hole_depth_type == PRO_HLE_STD_VAR_DEPTH ) {
			double tip_depth;

			if( !csg_root ) {
				csg_root = (struct csg_ops *)bu_malloc( sizeof( struct csg_ops ), "csg root" );
				csg = csg_root;
				csg->next = NULL;
			} else {
				csg = (struct csg_ops *)bu_malloc( sizeof( struct csg_ops ), "csg op" );
				csg->next = csg_root;
				csg_root = csg;
			}
			bu_vls_init( &csg->name );
			bu_vls_init( &csg->dbput );
			
			csg->operator = '-';
			hole_no++;
			bu_vls_printf( &csg->name, "hole.%d ", hole_no );
			bn_vec_ortho( a, dir );
			VCROSS( b, a, dir );
			VUNITIZE( b );
			VMOVE( c, a );
			VMOVE( d, b );
			tip_depth = (hole_radius - MIN_RADIUS) / tan( drill_angle * M_PI / 360.0 );
			VSCALE( h, dir, tip_depth * proe_to_brl_conv );
			VSCALE( a, a, hole_radius * proe_to_brl_conv );
			VSCALE( b, b, hole_radius * proe_to_brl_conv );
			VSCALE( c, c, MIN_RADIUS );
			VSCALE( d, d, MIN_RADIUS );
			bu_vls_printf( &csg->dbput, tgc_format,
			       V3ARGS( start ),
			       V3ARGS( h ),
			       V3ARGS( a ),
			       V3ARGS( b ),
			       V3ARGS( c ),
			       V3ARGS( d ) );
			drill_angle = 0.0;
		}
	} else {
		fprintf( stderr, "Unrecognized hole type\n" );
		return 0;
	}

	return 1;
}

ProError
do_feature_visit( ProFeature *feat, ProError status, ProAppData data )
{
	ProError ret;
	ProElement elem_tree;
	ProElempath elem_path=NULL;

	if( (ret=ProFeatureElemtreeCreate( feat, &elem_tree ) ) == PRO_TK_NO_ERROR ) {
		if( (ret=ProElemtreeElementVisit( elem_tree, elem_path,
						  hole_elem_filter, hole_elem_visit,
						  data ) ) != PRO_TK_NO_ERROR ) {
			fprintf( stderr, "Element visit failed for feature (%d) of %s\n",
				 feat->id, curr_part_name );
			if( ProElementFree( &elem_tree ) != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Error freeing element tree\n" );
			}
			return ret;
		}
		if( ProElementFree( &elem_tree ) != PRO_TK_NO_ERROR ) {
			fprintf( stderr, "Error freeing element tree\n" );
		}
	}

	radius = 0.0;
	diameter = 0.0;
	distance1 = 0.0;
	distance2 = 0.0;
	got_diameter = 0;
	got_distance1 = 0;

	if( (ret=ProFeatureDimensionVisit( feat, check_dimension, dimension_filter, data ) ) !=
	    PRO_TK_NO_ERROR ) {
		return( ret );
	}

	if( curr_feat_type == PRO_FEAT_HOLE ) {
		/* need more info to recreate holes */
		if( (ret=ProFeatureGeomitemVisit( feat, PRO_AXIS, geomitem_visit,
				    geomitem_filter, data ) ) != PRO_TK_NO_ERROR ) {
			return ret;
		}
	}

	switch( curr_feat_type ) {
		case PRO_FEAT_HOLE:
			if( Subtract_hole() )
				Add_to_feature_delete_list( feat->id );
			break;
		case PRO_FEAT_ROUND:
			if( got_diameter && radius < min_round_radius ) {
				Add_to_feature_delete_list( feat->id );
			}
			break;
		case PRO_FEAT_CHAMFER:
			if( got_distance1 && distance1 < min_chamfer_dim &&
			    distance2 < min_chamfer_dim ) {
				Add_to_feature_delete_list( feat->id );
			}
			break;
	}


	return ret;
}

int
feat_adds_material( ProFeattype feat_type )
{
	if( feat_type >= PRO_FEAT_UDF_THREAD ) {
		return( 1 );
	}

	switch( feat_type ) {
	case PRO_FEAT_SHAFT:
	case PRO_FEAT_PROTRUSION:
	case PRO_FEAT_NECK:
	case PRO_FEAT_FLANGE:
	case PRO_FEAT_RIB:
	case PRO_FEAT_EAR:
	case PRO_FEAT_DOME:
	case PRO_FEAT_LOC_PUSH:
	case PRO_FEAT_UDF:
	case PRO_FEAT_DRAFT:
	case PRO_FEAT_SHELL:
	case PRO_FEAT_DOME2:
	case PRO_FEAT_IMPORT:
	case PRO_FEAT_MERGE:
	case PRO_FEAT_MOLD:
	case PRO_FEAT_OFFSET:
	case PRO_FEAT_REPLACE_SURF:
	case PRO_FEAT_PIPE:
		return( 1 );
		break;
	default:
		return( 0 );
		break;
	}

	return( 0 );
}

void
remove_holes_from_id_list( ProMdl model )
{
	int i;
	ProFeature feat;
	ProError status;
	ProFeattype type;

	if( logger ) {
		fprintf( logger, "Removing any holes from CSG list and from features to delete\n" );
	}

	free_csg_ops();		/* these are only holes */
	for( i=0 ; i<feat_id_count ; i++ ) {
		status = ProFeatureInit( ProMdlToSolid(model),
					 feat_ids_to_delete[i],
					 &feat );
		if( status != PRO_TK_NO_ERROR ) {
			fprintf( stderr, "Failed to get handle for id %d\n",
				 feat_ids_to_delete[i] );
			if( logger ) {
				fprintf( logger, "Failed to get handle for id %d\n",
					 feat_ids_to_delete[i] );
			}
		}
		status = ProFeatureTypeGet( &feat, &type );
		if( status != PRO_TK_NO_ERROR ) {
			fprintf( stderr, "Failed to get feature type for id %d\n",
				 feat_ids_to_delete[i] );
			if( logger ) {
				fprintf( logger, "Failed to get feature type for id %d\n",
					 feat_ids_to_delete[i] );
			}
		}
		if( type == PRO_FEAT_HOLE ) {
			/* remove this from the list */
			int j;

			if( logger ) {
				fprintf( logger, "\tRemoving feature id %d from deltion list\n",
					 feat_ids_to_delete[i] );
			}
			feat_id_count--;
			for( j=i ; j<feat_id_count ; j++ ) {
				feat_ids_to_delete[j] = feat_ids_to_delete[j+1];
			}
			i--;
		}
	}
}


/* this routine filters out which features should be visited by the feature visit initiated in
 * the output_part() routine.
 */
ProError
feature_filter( ProFeature *feat, ProAppData data )
{
	ProError ret;
	ProMdl model = (ProMdl)data;

	if( (ret=ProFeatureTypeGet( feat, &curr_feat_type )) != PRO_TK_NO_ERROR ) {
		
		fprintf( stderr, "ProFeatureTypeGet Failed for %s!!\n", curr_part_name );
		return ret;
	}
	if( curr_feat_type > 0 ) {
		feat_type_count[curr_feat_type - FEAT_TYPE_OFFSET]++;
	} else if( curr_feat_type == 0 ) {
		feat_type_count[0]++;
	}

	/* handle holes, chamfers, and rounds only */
	if( curr_feat_type == PRO_FEAT_HOLE ||
	    curr_feat_type == PRO_FEAT_CHAMFER ||
	    curr_feat_type == PRO_FEAT_ROUND ) {
		return PRO_TK_NO_ERROR;
	}

	/* if we encounter a protrusion (or any feature that adds material) after a hole,
	 * we cannot convert previous holes to CSG
	 */
	if( feat_adds_material( curr_feat_type ) ) {
		/* any holes must be removed from the list */
		remove_holes_from_id_list( model );
	}

	/* skip everything else */
	return PRO_TK_CONTINUE;
}

void
build_tree( char *sol_name, struct bu_vls *tree )
{
	struct csg_ops *ptr;

	if( logger ) {
		fprintf( logger, "Building CSG tree for %s\n", sol_name );
	}
	ptr = csg_root;
	while( ptr ) {
		bu_vls_printf( tree, "{%c ", ptr->operator );
		ptr = ptr->next;
	}

	bu_vls_strcat( tree, "{ l {" );
	bu_vls_strcat( tree, sol_name );
	bu_vls_strcat( tree, "} }" );
	ptr = csg_root;
	while( ptr ) {
		if( logger ) {
			fprintf( logger, "Adding %c %s\n", ptr->operator, bu_vls_addr( &ptr->name ) );
		}
		bu_vls_printf( tree, " {l {%s}}}", bu_vls_addr( &ptr->name ) );
		ptr = ptr->next;
	}

	if( logger ) {
		fprintf( logger, "Final tree: %s\n", bu_vls_addr( tree ) );
	}
}

void
output_csg_prims()
{
	struct csg_ops *ptr;

	ptr = csg_root;

	while( ptr ) {
		if( logger ) {
			fprintf( logger, "Creating primitive: %s %s\n",
				 bu_vls_addr( &ptr->name ), bu_vls_addr( &ptr->dbput ) );	 
		}

		fprintf( outfp, "put {%s} %s", bu_vls_addr( &ptr->name ), bu_vls_addr( &ptr->dbput ) );
		ptr = ptr->next;
	}
}

void
kill_error_dialog( char *dialog, char *component, ProAppData appdata )
{
	(void)ProUIDialogDestroy( "proe_brl_error" );
}

void
kill_gen_error_dialog( char *dialog, char *component, ProAppData appdata )
{
	(void)ProUIDialogDestroy( "proe_brl_gen_error" );
}

/* routine to output a part as a BRL-CAD region with one BOT solid
 * The region will have the name from Pro/E.
 * The solid will have the same name with "s." prefix.
 *
 *	returns:
 *		0 - OK
 *		1 - Failure
 *		2 - empty part, nothing output
 */
int
output_part( ProMdl model )
{
	ProName part_name;
	Pro_surf_props props;
	char *brl_name=NULL;
	char *sol_name=NULL;
	ProSurfaceTessellationData *tess=NULL;
	ProError status;
	ProMdlType type;
	char str[PRO_NAME_SIZE + 1];
	int ret=0;
	int ret_status=0;
	char err_mess[512];
	wchar_t werr_mess[512];

	/* if this part has already been output, do not do it again */
	if( ProMdlNameGet( model, part_name ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get name for a part\n" );
		return( 1 );
	}
	if( already_done_part( part_name ) )
		return( 0 );

	if( logger ) {
		fprintf( logger, "Processing %s:\n", ProWstringToString( astr, part_name ) );
	}

	if( ProMdlTypeGet( model, &type ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get part type\n" );
	} else {
		obj_type_count[type]++;
	}
	/* let user know we are doing something */

	status = ProUILabelTextSet( "proe_brl", "curr_proc", part_name );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to update dialog label for currently processed part\n" );
		return( 1 );
	}
	status = ProUIDialogActivate( "proe_brl", &ret_status );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Error in proe-brl Dialog, error = %d\n",
			 status );
		fprintf( stderr, "\t dialog returned %d\n", ret_status );
	}

	if( !do_facets_only || do_elims ) {
		free_csg_ops();
		ProSolidFeatVisit( ProMdlToSolid(model), do_feature_visit,
				   feature_filter, (ProAppData)model );

		if( feat_id_count ) {
			int i;

			if( logger ) {
				fprintf( logger, "suppressing %d features of %s:\n",
					 feat_id_count, curr_part_name );
				for( i=0 ; i<feat_id_count ; i++ ) {
					fprintf( logger, "\t%d\n", feat_ids_to_delete[i] );
				}
			}
			fprintf( stderr, "suppressing %d features\n", feat_id_count );
			ret = ProFeatureSuppress( ProMdlToSolid(model),
						  feat_ids_to_delete, feat_id_count,
						  NULL, 0 );
			if( ret != PRO_TK_NO_ERROR ) {
				ProFeatureResumeOptions resume_opts[1];
				ProFeatStatus feat_stat;
				ProFeature feat;

				resume_opts[0] = PRO_FEAT_RESUME_INCLUDE_PARENTS;

				if( logger ) {
					fprintf( logger, "Failed to suppress features!!!\n" );
				}
				fprintf( stderr, "Failed to delete %d features from %s\n",
					 feat_id_count, curr_part_name );

				for( i=0 ; i<feat_id_count ; i++ ) {
					status = ProFeatureInit( ProMdlToSolid(model),
								 feat_ids_to_delete[i],
								 &feat );
					if( status != PRO_TK_NO_ERROR ) {
						fprintf( stderr, "Failed to get handle for id %d\n",
								 feat_ids_to_delete[i] );
						if( logger ) {
							fprintf( logger, "Failed to get handle for id %d\n",
								 feat_ids_to_delete[i] );
						}
					} else {
						status = ProFeatureStatusGet( &feat, &feat_stat );
						if( status != PRO_TK_NO_ERROR ) {
							fprintf( stderr,
								 "Failed to get status for feature %d\n",
								 feat_ids_to_delete[i] );
							if( logger ) {
								fprintf( logger,
									 "Failed to get status for feature %d\n",
									 feat_ids_to_delete[i] );
							}
						} else {
							if( logger ) {
								if( feat_stat < 0 ) {
									fprintf( logger,
										 "invalid feature (%d)\n",
										 feat_ids_to_delete[i] );
								} else {
									fprintf( logger, "feat %d status = %s\n",
										 feat_ids_to_delete[i],
										 feat_status[ feat_stat ] );
								}
							}
							if( feat_stat == PRO_FEAT_SUPPRESSED ) {
								/* unsuppress this one */
								if( logger ) {
									fprintf( logger,
										 "Unsuppressing feature %d\n",
										  feat_ids_to_delete[i] );
								}
								status = ProFeatureResume( ProMdlToSolid(model),
											   &feat_ids_to_delete[i],
											   1, resume_opts, 1 );
								if( logger ) {
									if( status == PRO_TK_NO_ERROR ) {
										fprintf( logger,
											 "\tfeature id %d unsuppressed\n",
											 feat_ids_to_delete[i] );
									} else if( status == PRO_TK_SUPP_PARENTS ) {
										fprintf( logger,
											 "\tsuppressed parents for feature %d not found\n",
											 feat_ids_to_delete[i] );
										
									} else {
										fprintf( logger,
											 "\tfeature id %d unsuppression failed\n",
											 feat_ids_to_delete[i] );
									}
								}
							}
						}
					}
				}

				feat_id_count = 0;
				free_csg_ops();
			} else {
				fprintf( stderr, "features suppressed!!\n" );
			}
		}
	}

	/* can get bounding box of a solid using "ProSolidOutlineGet"
	 * may want to use this to implement relative facetization talerance
	 */

	/* tessellate part */
	if( logger ) {
		fprintf( logger, "Tessellate part (%s)\n", curr_part_name );
	}

	status = ProPartTessellate( ProMdlToPart(model), max_error/proe_to_brl_conv,
			   angle_cntrl, PRO_B_TRUE, &tess  );
	if( status != PRO_TK_NO_ERROR ) {
		/* Failed!!! */

		if( logger ) {
			fprintf( logger, "Failed to tessellate %s!!!\n", curr_part_name );
		}
		sprintf( astr, "Failed to tessellate part (%s)", curr_part_name );
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR", astr );
		ProMessageClear();
		fprintf( stderr, "%s\n", astr );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		ret = 1;
	} else if( !tess ) {
		/* not a failure, just an empty part */

		if( logger ) {
			fprintf( logger, "part (%s) is empty\n", curr_part_name );
		}
		sprintf( astr, "%s has no surfaces, ignoring", curr_part_name );
		(void)ProMessageDisplay(MSGFIL, "USER_WARNING", astr );
		ProMessageClear();
		fprintf( stderr, "%s\n", astr );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		add_to_empty_list( get_brlcad_name( curr_part_name ) );
		ret = 2;
	} else {
		/* output the triangles */
		int surface_count;

		status = ProArraySizeGet( (ProArray)tess, &surface_count );
		if( status != PRO_TK_NO_ERROR ) {
			(void)ProMessageDisplay(MSGFIL, "USER_ERROR", "Failed to get array size" );
			ProMessageClear();
			fprintf( stderr, "Failed to get array size\n" );
			(void)ProWindowRefresh( PRO_VALUE_UNUSED );
			ret = 1;
		} else if( surface_count < 1 ) {
			if( logger ) {
				fprintf( logger, "part (%s) has no surfaces\n", curr_part_name );
			}
			sprintf( astr, "%s has no surfaces, ignoring", curr_part_name );
			(void)ProMessageDisplay(MSGFIL, "USER_WARNING", astr );
			ProMessageClear();
			fprintf( stderr, "%s\n", astr );
			(void)ProWindowRefresh( PRO_VALUE_UNUSED );
			add_to_empty_list( get_brlcad_name( curr_part_name ) );
			ret = 2;
		} else {
			int i;
			int v1, v2, v3, n1, n2, n3;
			int surfno;
			int vert_no;
			int stat;
			ProName material;
			ProMassProperty mass_prop;
			ProMaterialProps material_props;
			int got_density;
			struct bu_vls tree;

			curr_tri = 0;
			clean_vert_tree(vert_tree_root);
			clean_vert_tree(norm_tree_root);

			/* add all vertices and triangles to our lists */
			if( logger ) {
				fprintf( logger, "Processing surfaces of part %s\n", curr_part_name );
			}
			for( surfno=0 ; surfno<surface_count ; surfno++ ) {
				for( i=0 ; i<tess[surfno].n_facets ; i++ ) {
					/* grab the triangle */
					vert_no = tess[surfno].facets[i][0];
					v1 = Add_vert( tess[surfno].vertices[vert_no][0], tess[surfno].vertices[vert_no][1],
						       tess[surfno].vertices[vert_no][2], vert_tree_root, local_tol_sq );
					vert_no = tess[surfno].facets[i][1];
					v2 = Add_vert( tess[surfno].vertices[vert_no][0], tess[surfno].vertices[vert_no][1],
						       tess[surfno].vertices[vert_no][2], vert_tree_root, local_tol_sq );
					vert_no = tess[surfno].facets[i][2];
					v3 = Add_vert( tess[surfno].vertices[vert_no][0], tess[surfno].vertices[vert_no][1],
						       tess[surfno].vertices[vert_no][2], vert_tree_root, local_tol_sq );
					if( bad_triangle( v1, v2, v3 ) ) {
						continue;
					}

					if( !get_normals ) {
						add_triangle( v1, v2, v3 );
						continue;
					}

					/* grab the surface normals */
					vert_no = tess[surfno].facets[i][0];
					VUNITIZE( tess[surfno].normals[vert_no] );
					n1 = Add_vert( tess[surfno].normals[vert_no][0], tess[surfno].normals[vert_no][1],
						       tess[surfno].normals[vert_no][2], norm_tree_root, local_tol_sq );
					vert_no = tess[surfno].facets[i][1];
					VUNITIZE( tess[surfno].normals[vert_no] );
					n2 = Add_vert( tess[surfno].normals[vert_no][0], tess[surfno].normals[vert_no][1],
						       tess[surfno].normals[vert_no][2], norm_tree_root, local_tol_sq );
					vert_no = tess[surfno].facets[i][2];
					VUNITIZE( tess[surfno].normals[vert_no] );
					n3 = Add_vert( tess[surfno].normals[vert_no][0], tess[surfno].normals[vert_no][1],
						       tess[surfno].normals[vert_no][2], norm_tree_root, local_tol_sq );

					add_triangle_and_normal( v1, v2, v3, n1, n2, n3 );
				}
			}

			/* actually output the part */
			/* first the BOT solid with a made-up name */
			brl_name = get_brlcad_name( curr_part_name );
			sol_name = (char *)bu_malloc( strlen( brl_name ) + 3, "aol_name" );
			sprintf( sol_name, "s.%s", brl_name );
			if( logger ) {
				fprintf( logger, "Creating bot primitive (%s) for part %s\n",
					 sol_name, brl_name );
			}
			fprintf( outfp, "put {%s} bot mode volume orient no V { ", sol_name );
			for( i=0 ; i<vert_tree_root->curr_vert ; i++ ) {
				fprintf( outfp, " {%.12e %.12e %.12e}",
					 vert_tree_root->the_array[i*3] * proe_to_brl_conv,
					 vert_tree_root->the_array[i*3+1] * proe_to_brl_conv,
					 vert_tree_root->the_array[i*3+2] * proe_to_brl_conv );
			}
			fprintf( outfp, " } F {" );
			for( i=0 ; i<curr_tri ; i++ ) {
				fprintf( outfp, " {%d %d %d}", part_tris[i][0],
					 part_tris[i][1], part_tris[i][2] ); 
			}
			if( get_normals ) {
				if( logger ) {
					fprintf( logger, "Getting vertex normals for part %s\n",
						 curr_part_name );
				}
				fprintf( outfp, " } flags { has_normals use_normals } N {" );
				for( i=0 ; i<norm_tree_root->curr_vert ; i++ ) {
				fprintf( outfp, " {%.12e %.12e %.12e}",
					 norm_tree_root->the_array[i*3] * proe_to_brl_conv,
					 norm_tree_root->the_array[i*3+1] * proe_to_brl_conv,
					 norm_tree_root->the_array[i*3+2] * proe_to_brl_conv );
				}
				fprintf( outfp, " } fn {" );
				for( i=0 ; i<curr_tri ; i++ ) {
					fprintf( outfp, " {%d %d %d}", part_norms[i*3],
						 part_norms[i*3+1], part_norms[i*3+2] ); 
				}
			}
			fprintf( outfp, " }\n" );

			/* build the tree for this region */
			bu_vls_init( &tree );
			build_tree( sol_name, &tree );
			bu_free( sol_name, "sol_name" );
			output_csg_prims();

			/* get the surface properties for the part
			 * and create a region using the actual part name
			 */
			if( logger ) {
				fprintf( logger, "Creating region for part %s\n", curr_part_name );
			}
			stat = prodb_get_surface_props( model, SEL_3D_PART, -1, 0, &props );
			if( stat == PRODEV_SURF_PROPS_NOT_SET ) {
				/* no surface properties */
				fprintf( outfp,
				   "put {%s} comb region yes id %d los 100 GIFTmater 1 tree %s\n",
					 get_brlcad_name( curr_part_name ), reg_id, bu_vls_addr( &tree) );
			} else if( stat == PRODEV_SURF_PROPS_SET ) {
				/* use the colors, ... that was set in Pro/E */
				fprintf( outfp,
				    "put {%s} comb region yes id %d los 100 GIFTmater 1 rgb {%d %d %d} shader {plastic {",
					 get_brlcad_name( curr_part_name ),
					 reg_id,
					 (int)(props.color_rgb[0]*255.0),
					 (int)(props.color_rgb[1]*255.0),
					 (int)(props.color_rgb[2]*255.0) );
				if( props.transparency != 0.0 ) {
					fprintf( outfp, " tr %g", props.transparency );
				}
				if( props.shininess != 1.0 ) {
					fprintf( outfp, " sh %d", (int)(props.shininess * 18 + 2.0) );
				}
				if( props.diffuse != 0.3 ) {
					fprintf( outfp, " di %g", props.diffuse );
				}
				if( props.highlite != 0.7 ) {
					fprintf( outfp, " sp %g", props.highlite );
				}
				fprintf( outfp, "} }" );
				fprintf( outfp, " tree %s\n", bu_vls_addr( &tree ) );
			} else {
				/* something is wrong, but just ignore the missing properties */
				fprintf( stderr, "Error getting surface properties for %s\n",
					 curr_part_name );
				fprintf( outfp, "put {%s} comb region yes id %d los 100 GIFTmater 1 tree %s\n",
					 get_brlcad_name( curr_part_name ), reg_id, bu_vls_addr( &tree ) );
			}

			/* if the part has a material, add it as an attribute */
			got_density = 0;
			status = ProPartMaterialNameGet( ProMdlToPart(model), material );
			if( status == PRO_TK_NO_ERROR ) {
				fprintf( outfp, "attr set {%s} material_name {%s}\n",
					 get_brlcad_name( curr_part_name ),
					 ProWstringToString( str, material ) ); 

				/* get the density for this material */
				status = ProPartMaterialdataGet( ProMdlToPart(model), material, &material_props );
				if( status == PRO_TK_NO_ERROR ) {
					got_density = 1;
					fprintf( outfp, "attr set {%s} density %g\n",
						 get_brlcad_name( curr_part_name ),
						 material_props.mass_density );
				}
			}

			/* calculate mass properties */
			status = ProSolidMassPropertyGet( ProMdlToSolid( model ), NULL, &mass_prop );
			if( status == PRO_TK_NO_ERROR ) {
				if( !got_density ) {
					if( mass_prop.density > 0.0 ) {
						fprintf( outfp, "attr set {%s} density %g\n",
							 get_brlcad_name( curr_part_name ),
							 mass_prop.density );
					}
				}
				if( mass_prop.mass > 0.0 ) {
					fprintf( outfp, "attr set {%s} mass %g\n",
						 get_brlcad_name( curr_part_name ),
						 mass_prop.mass );
				}
				if( mass_prop.volume > 0.0 ) {
					fprintf( outfp, "attr set {%s} volume %g\n",
						 get_brlcad_name( curr_part_name ),
						 mass_prop.volume );
				}
			}

			/* increment the region id */
			reg_id++;

			/* free the tree */
			bu_vls_free( &tree );
		}
	}


	/* free the tessellation memory */
	ProPartTessellationFree( &tess );
	tess = NULL;

	/* add this part to the list of objects already output */
	add_to_done_part( part_name );

	/* unsuppress anything we suppressed */
	if( feat_id_count ) {
		if( logger ) {
			fprintf( logger, "Unsuppressing %d features\n", feat_id_count );
		}
		fprintf( stderr, "Unsuppressing %d features\n", feat_id_count );
		if( (ret=ProFeatureResume( ProMdlToSolid(model),
					     feat_ids_to_delete, feat_id_count,
					     NULL, 0 )) != PRO_TK_NO_ERROR) {

			fprintf( stderr, "Failed to unsuppress %d features from %s\n",
				 feat_id_count, curr_part_name );

			/* use UI dialog */
			status = ProUIDialogCreate( "proe_brl_error", "proe_brl_error" );
			if( status != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to create dialog box for proe-brl, error = %d\n", status );
				return( 0 );
			}
			sprintf( err_mess, err_mess_form, feat_id_count, curr_part_name );
			(void)ProStringToWstring( werr_mess, err_mess );
			status = ProUITextareaValueSet( "proe_brl_error", "the_message", werr_mess );
			if( status != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to create dialog box for proe-brl, error = %d\n", status );
				return( 0 );
			}
			(void)ProUIPushbuttonActivateActionSet( "proe_brl_error", "ok", kill_error_dialog, NULL );
			status = ProUIDialogActivate( "proe_brl_error", &ret_status );
			if( status != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Error in proe-brl error Dialog, error = %d\n",
					 status );
				fprintf( stderr, "\t dialog returned %d\n", ret_status );
			}

			feat_id_count = 0;
			return 0;
		}
		fprintf( stderr, "features unsuppressed!!\n" );
		feat_id_count = 0;
	}

	return( ret );
}

/* routine to free the memory associated with our assembly info */
void
free_assem( struct asm_head *curr_assem )
{
	struct asm_member *ptr, *tmp;

	if( logger ) {
		fprintf( logger, "Freeing assembly info\n" );
	}

	ptr = curr_assem->members;
	while( ptr ) {
		tmp = ptr;
		ptr = ptr->next;
		bu_free( (char *)tmp, "asm member" );
	}
}

/* routine to list assembly info (for debugging) */
void
list_assem( struct asm_head *curr_asm )
{
	struct asm_member *ptr;

	fprintf( stderr, "Assembly %s:\n", curr_asm->name );
	ptr = curr_asm->members;
	while( ptr ) {
		fprintf( stderr, "\t%s\n", ptr->name );
		ptr = ptr->next;
	}
}

/* routine to output an assembly as a BRL-CAD combination
 * The combination will have the Pro/E name with a ".c" suffix.
 * Cannot just use the Pro/E name, because assembly can have the same name as a part.
 */
void
output_assembly( ProMdl model )
{
	ProName asm_name;
	ProMassProperty mass_prop;
	ProError status;
	ProBoolean is_exploded;
	struct asm_head curr_assem;
	struct asm_member *member;
	int member_count=0;
	int i, j, k;
	int ret_status=0;

	if( ProMdlNameGet( model, asm_name ) != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get model name for an assembly\n" );
		return;
	}

	/* do not output this assembly more than once */
	if( already_done_asm( asm_name ) )
		return;

	if( logger ) {
		fprintf( logger, "Processing assembly %s:\n", ProWstringToString( astr, asm_name ) );
	}

	/* let the user know we are doing something */

	status = ProUILabelTextSet( "proe_brl", "curr_proc", asm_name );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to update dialog label for currently processed assembly\n" );
		return;
	}
	status = ProUIDialogActivate( "proe_brl", &ret_status );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Error in proe-brl Dialog, error = %d\n",
			 status );
		fprintf( stderr, "\t dialog returned %d\n", ret_status );
	}

	/* everything starts out in "curr_part_name", copy name to "curr_asm_name" */
	strcpy( curr_asm_name, curr_part_name );

	/* start filling in assembly info */
	strcpy( curr_assem.name, curr_part_name );
	curr_assem.members = NULL;
	curr_assem.model = model;

	/* make sure this assembly is not "exploded"!!!
	 * some careless designers leave assemblies in exploded mode
	 */

	status = ProAssemblyIsExploded( model, &is_exploded );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get explode status of %s\n", curr_assem.name );
		if( logger ) {
			fprintf( logger, "Failed to get explode status of %s\n", curr_assem.name );
		}
	}

	if( is_exploded ) {
		/* unexplode this assembly !!!! */
		status = ProAssemblyUnexplode( model );
		if( status != PRO_TK_NO_ERROR ) {
			fprintf( stderr, "Failed to un-explode assembly %s\n", curr_assem.name );
			fprintf( stderr, "\tcomponents will be incorrectly positioned\n" );
			if( logger ) {
				fprintf( logger, "Failed to un-explode assembly %s\n", curr_assem.name );
				fprintf( logger, "\tcomponents will be incorrectly positioned\n" );
			}
		}
	}

	/* use feature visit to get info about assembly members.
	 * also calls output functions for members (parts or assemblies)
	 */
	status = ProSolidFeatVisit( ProMdlToPart(model),
				    assembly_comp,
				    (ProFeatureFilterAction)assembly_filter,
				    (ProAppData)&curr_assem );

	/* output the accumulated assembly info */
	fprintf( outfp, "put {%s.c} comb region no tree ", get_brlcad_name( curr_assem.name ) );

	/* count number of members */
	member = curr_assem.members;
	while( member ) {
		member_count++;
		member = member->next;
	}

	if( logger ) {
		fprintf( logger, "Output %d members of assembly\n", member_count );
	}

	/* output the "tree" */
	for( i=1 ; i<member_count ; i++ ) {
		fprintf( outfp, "{u ");
	}

	member = curr_assem.members;
	i = 0;
	while( member ) {
		/* output the member name */
		if( member->type == PRO_MDL_ASSEMBLY ) {
			fprintf( outfp, "{l {%s.c}", get_brlcad_name( member->name ) );
		} else {
			fprintf( outfp, "{l {%s}", get_brlcad_name( member->name ) );
		}

		/* if there is an xform matrix, put it here */
		if( is_non_identity( member->xform ) ) {
			fprintf( outfp, " {" );
			for( j=0 ; j<4 ; j++ ) {
				for( k=0 ; k<4 ; k++ ) {
					if( k == 3 && j < 3 ) {
						fprintf( outfp, " %.12e",
						     member->xform[k][j] * proe_to_brl_conv );
					} else {
						fprintf( outfp, " %.12e",
							 member->xform[k][j] );
					}
				}
			}
			fprintf( outfp, "}" );
		}
		if( i ) {
			fprintf( outfp, "}} " );
		} else {
			fprintf( outfp, "} " );
		}
		member = member->next;
		i++;
	}
	fprintf( outfp, "\n" );

	/* calculate mass properties */
	if( logger ) {
		fprintf( logger, "Getting mass properties for this assmebly\n" );
	}

	status = ProSolidMassPropertyGet( ProMdlToSolid( model ), NULL, &mass_prop );
	if( status == PRO_TK_NO_ERROR ) {
		if( mass_prop.density > 0.0 ) {
			fprintf( outfp, "attr set {%s.c} density %g\n",
				 get_brlcad_name( curr_asm_name ),
				 mass_prop.density );
		}
		if( mass_prop.mass > 0.0 ) {
			fprintf( outfp, "attr set {%s.c} mass %g\n",
				 get_brlcad_name( curr_asm_name ),
				 mass_prop.mass );
		}
		if( mass_prop.volume > 0.0 ) {
			fprintf( outfp, "attr set {%s.c} volume %g\n",
				 get_brlcad_name( curr_asm_name ),
				 mass_prop.volume );
		}
	}
	/* add this assembly to the list of already output objects */
	add_to_done_asm( asm_name );

	/* free the memory associated with this assembly */
	free_assem( &curr_assem );
}

/* routine that is called by feature visit for each assembly member
 * the "app_data" is the head of the assembly info for this assembly
 */
ProError
assembly_comp( ProFeature *feat, ProError status, ProAppData app_data )
{
	ProIdTable id_table;
	ProMdl model;
	ProAsmcomppath comp_path;
	ProMatrix xform;
	ProMdlType type;
	ProName name;
	struct asm_head *curr_assem = (struct asm_head *)app_data;
	struct asm_member *member, *prev=NULL;
	int i, j;

	status = ProAsmcompMdlNameGet( feat, &type, name );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "ProAsmcompMdlNameGet() failed\n" );
		return status;
	}
	(void)ProWstringToString( curr_part_name, name );
	if( logger ) {
		fprintf( logger, "Processing assembly member %s\n", curr_part_name );
	}

	/* the next two Pro/Toolkit calls are the only way I could find to get
	 * the transformation matrix to apply to this member.
	 * this call is creating a path from the assembly to this particular member
	 * (assembly/member)
	 */
	id_table[0] = feat->id;
	status = ProAsmcomppathInit( (ProSolid)curr_assem->model, id_table, 1, &comp_path );
	if( status != PRO_TK_NO_ERROR ) {
		sprintf( astr, "Failed to get path from %s to %s (aborting)", curr_asm_name,
			 curr_part_name );
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR", astr );
		ProMessageClear();
		fprintf( stderr, "%s\n", astr );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( status );
	}

	/* this call accumulates the xform matrix along the path created above */
	status = ProAsmcomppathTrfGet( &comp_path, PRO_B_TRUE, xform );
	if( status != PRO_TK_NO_ERROR ) {
		sprintf( astr, "Failed to get transformation matrix %s/%s, error = %d, id = %d",
			 curr_asm_name, curr_part_name, status, feat->id );
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR", astr );
		ProMessageClear();
		fprintf( stderr, "%s\n", astr );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( PRO_TK_NO_ERROR );
	}

	/* add this member to our assembly info */
	prev = NULL;
	member = curr_assem->members;
	if( member ) {
		while( member->next ) {
			prev = member;
			member = member->next;
		}
		member->next = (struct asm_member *)bu_malloc( sizeof( struct asm_member ), "asm member" );
		prev = member;
		member = member->next;
	} else {
		curr_assem->members = (struct asm_member *)bu_malloc(
					 sizeof( struct asm_member ),
					 "asm member");
		member = curr_assem->members;
	}

	if( !member ) {
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR",
					"memory allocation for member failed" );
		ProMessageClear();
		fprintf( stderr, "memory allocation for member failed\n" );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( PRO_TK_GENERAL_ERROR );
	}
	member->next = NULL;

	/* capture its name */
	(void)ProWstringToString( member->name, name );

	/* copy xform matrix */
	for( i=0 ; i<4 ; i++ ) {
		for( j=0 ; j<4 ; j++ ) {
			member->xform[i][j] = xform[i][j];
		}
	}

	/* get the model for this member */
	status = ProAsmcompMdlGet( feat, &model );
	if( status != PRO_TK_NO_ERROR ) {
		sprintf( astr, "Failed to get model for component %s",
			 curr_part_name );
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR", astr );
		ProMessageClear();
		fprintf( stderr, "%s\n", astr );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( status );
	}

	/* get its type (part or assembly are the only ones that should make it here) */
	status = ProMdlTypeGet( model, &type );
	if( status != PRO_TK_NO_ERROR ) {
		sprintf( astr, "Failed to get type for component %s",
			 curr_part_name );
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR", astr );
		ProMessageClear();
		fprintf( stderr, "%s\n", astr );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( status );
	}

	/* remember the type */
	member->type = type;

	/* output this member */
	switch( type ) {
	case PRO_MDL_ASSEMBLY:
		output_assembly( model );
		break;
	case PRO_MDL_PART:
		if( output_part( model ) == 2 ) {
			/* part had no solid parts, eliminate from the assembly */
			if( prev ) {
				prev->next = NULL;
			} else {
				curr_assem->members = NULL;
			}
			bu_free( (char *)member, "asm member" );
		}
		break;
	}

	return( PRO_TK_NO_ERROR );
}

/* this routine is a filter for the feature visit routine
 * selects only "component" items (should be only parts and assemblies)
 */
ProError
assembly_filter( ProFeature *feat, ProAppData *data )
{
	ProFeattype type;
	ProFeatStatus feat_status;
	ProError status;

	status = ProFeatureTypeGet( feat, &type );
	if( status != PRO_TK_NO_ERROR ) {
		sprintf( astr, "In assembly_filter, cannot get feature type for feature %d",
			 feat->id );
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR", astr );
		ProMessageClear();
		fprintf( stderr, "%s\n", astr );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( PRO_TK_CONTINUE );
	}

	if( type != PRO_FEAT_COMPONENT ) {
		return( PRO_TK_CONTINUE );
	}

	status = ProFeatureStatusGet( feat, &feat_status );
	if( status != PRO_TK_NO_ERROR ) {
		sprintf( astr, "In assembly_filter, cannot get feature status for feature %d",
			 feat->id );
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR", astr );
		ProMessageClear();
		fprintf( stderr, "%s\n", astr );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( PRO_TK_CONTINUE );
	}

	if( feat_status != PRO_FEAT_ACTIVE ) {
		return( PRO_TK_CONTINUE );
	}

	return( PRO_TK_NO_ERROR );
}

void
set_identity( ProMatrix xform )
{
	int i,j;

	for( i=0 ; i<4 ; i++ ) {
		for( j=0 ; j<4 ; j++ ) {
			if( i == j ) {
				xform[i][j] = 1.0;
			} else {
				xform[i][j] = 0.0;
			}
		}
	}
}

/* routine to check if xform is an identity */
int
is_non_identity( ProMatrix xform )
{
	int i, j;

	for( i=0 ; i<4 ; i++ ) {
		for( j=0 ; j<4 ; j++ ) {
			if( i == j ) {
				if( xform[i][j] != 1.0 )
					return( 1 );
			} else {
				if( xform[i][j] != 0.0 )
					return( 1 );
			}
		}
	}

	return( 0 );
}

/* routine to output the top level object that is currently displayed in Pro/E */
void
output_top_level_object( ProMdl model, ProMdlType type )
{
	ProName name;
	ProCharName top_level;

	/* get its name */
	if( ProMdlNameGet( model, name ) != PRO_TK_NO_ERROR ) {
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR",
					"Could not get name for part!!" );
		ProMessageClear();
		fprintf( stderr, "Could not get name for part" );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		strcpy( curr_part_name, "noname" );
	} else {
		(void)ProWstringToString( curr_part_name, name );
	}

	/* save name */
	strcpy( top_level, curr_part_name );

	if( logger ) {
		fprintf( logger, "Output top level object (%s)\n", top_level );
	}

	/* output the object */
	if( type == PRO_MDL_PART ) {
		/* tessellate part and output triangles */
		output_part( model );
	} else if( type == PRO_MDL_ASSEMBLY ) {
		/* visit all members of assembly */
		output_assembly( model );
	} else {
		sprintf( astr, "Object %s is neither PART nor ASSEMBLY, skipping",
			 curr_part_name );
		(void)ProMessageDisplay(MSGFIL, "USER_WARNING", astr );
		ProMessageClear();
		fprintf( stderr, "%s\n", astr );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	}

	/* make a top level combination named "top", if there is not already one
	 * this combination contains the xform to rotate the model into BRL-CAD standard orientation
	 */
	fprintf( outfp, "if { [catch {get top} ret] } {\n" );
	if( type == PRO_MDL_ASSEMBLY ) {
		fprintf( outfp,
			 "\tput top comb region no tree {l %s.c {0 0 1 0 1 0 0 0 0 1 0 0 0 0 0 1}}\n",
			 get_brlcad_name( top_level ) );
	} else {
		fprintf( outfp,
			 "\tput top comb region no tree {l %s {0 0 1 0 1 0 0 0 0 1 0 0 0 0 0 1}}\n",
			 get_brlcad_name( top_level ) );
	}
	fprintf( outfp, "}\n" );
}

void
free_rb_data( void *data )
{
	bu_free( data, "rb_data" );
}

int
create_temp_directory()
{
	ProError status;
	int ret_status;

	empty_parts_root = NULL;
#if 1
	/* use UI dialog */
	status = ProUIDialogCreate( "proe_brl", "proe_brl" );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to create dialog box for proe-brl, error = %d\n", status );
		return( 0 );
	}

	status = ProUIPushbuttonActivateActionSet( "proe_brl", "doit", doit, NULL );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to set action for 'Go' button\n" );	
		ProUIDialogDestroy( "proe_brl" );
		return( 0 );
	}

	status = ProUIPushbuttonActivateActionSet( "proe_brl", "quit", do_quit, NULL );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to set action for 'Go' button\n" );	
		ProUIDialogDestroy( "proe_brl" );
		return( 0 );
	}

	status = ProUIDialogActivate( "proe_brl", &ret_status );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Error in proe-brl Dialog, error = %d\n",
			 status );
		fprintf( stderr, "\t dialog returned %d\n", ret_status );
	}

#else
	/* default output file name */
	strcpy( output_file, "proe.asc" );

	/* get the angle control */
	(void) ProMessageDisplay( MSGFIL, "USER_PROMPT_DOUBLE",
				  "Enter a value for angle control: ",
				  &angle_cntrl );
	range[0] = 0.0;
	range[1] = 1.0;
	status = ProMessageDoubleRead( range, &angle_cntrl );
	if( status == PRO_TK_MSG_USER_QUIT ) {
		return( 0 );
	}
#endif
	return( 0 );
}

void
free_hash_values( struct bu_hash_tbl *htbl )
{
	struct bu_hash_entry *entry;
	struct bu_hash_record rec;

	entry = bu_hash_tbl_first( htbl, &rec );

	while( entry ) {
		bu_free( bu_get_hash_value( entry ), "hash entry" );
		entry = bu_hash_tbl_next( &rec );
	}
}

struct bu_hash_tbl *
create_name_hash( FILE *name_fd )
{
	struct bu_hash_tbl *htbl;
	char line[MAX_LINE_LEN];
	struct bu_hash_entry *entry=NULL;
	int new_entry=0;
	long line_no=0;

	htbl = bu_create_hash_tbl( NUM_HASH_TABLE_BINS );

	if( logger ) {
		fprintf( logger, "name hash created, now filling it:\n" );
	}
	while( fgets( line, MAX_LINE_LEN, name_fd ) ) {
		char *part_no, *part_name, *ptr;
		line_no++;

		if( logger ) {
			fprintf( logger, "line %ld: %s", line_no, line );
		}

		ptr = strtok( line, " \t\n" );
		if( !ptr ) {
			bu_log( "Warning: unrecognizable line in part name file:\n\t%s\n", line );	
			bu_log( "\tIgnoring\n" );
			continue;
		}
		part_no = bu_strdup( ptr );
		lower_case( part_no );
		ptr = strtok( (char *)NULL, " \t\n" );
		if( !ptr ) {
			bu_log( "Warning: unrecognizable line in part name file:\n\t%s\n", line );	
			bu_log( "\tIgnoring\n" );
			continue;
		}
		entry = bu_hash_add_entry( htbl, (unsigned char *)part_no, strlen( part_no ), &new_entry );
		if( !new_entry ) {
			if( logger ) {
				fprintf( logger, "\t\t\tHash table entry already exists for above part\n" );
			}
			bu_free( part_no, "part_no" );
			continue;
		}
		lower_case( ptr );
		part_name = create_unique_name( ptr );

		if( logger ) {
			fprintf( logger, "\t\tpart_no = %s, part name = %s\n", part_no, part_name );
		}


		if( logger ) {
			fprintf( logger, "\t\t\tCreating new hash tabel entry for above names\n" );
		}
		bu_set_hash_value( entry, (unsigned char *)part_name );
	}

	return( htbl );
}

void
doit( char *dialog, char *compnent, ProAppData appdata )
{
	ProError status;
	ProMdl model;
	ProMdlType type;
	ProLine tmp_line;
	int unit_subtype;
	wchar_t unit_name[PRO_NAME_SIZE];
	double proe_conv;
	wchar_t *w_output_file;
	wchar_t *w_name_file;
	wchar_t *tmp_str;
	char output_file[128];
	char name_file[128];
	char log_file[128];
	int ret_status=0;

	empty_parts_root = (struct empty_parts *)NULL;
	brlcad_names = (bu_rb_tree *)NULL;

	ProStringToWstring( tmp_line, "Not processing" );
	status = ProUILabelTextSet( "proe_brl", "curr_proc", tmp_line );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to update dialog label for currently processed part\n" );
	}
	status = ProUIDialogActivate( "proe_brl", &ret_status );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Error in proe-brl Dialog, error = %d\n",
			 status );
		fprintf( stderr, "\t dialog returned %d\n", ret_status );
	}

	/* get the name of the log file */
	status = ProUIInputpanelValueGet( "proe_brl", "log_file", &tmp_str );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get log file name\n" );
		ProUIDialogDestroy( "proe_brl" );
		return;
	}
	ProWstringToString( log_file, tmp_str );
	ProWstringFree( tmp_str );

	/* get the name of the output file */
	status = ProUIInputpanelValueGet( "proe_brl", "output_file", &w_output_file );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get output file name\n" );
		ProUIDialogDestroy( "proe_brl" );
		return;
	}
	ProWstringToString( output_file, w_output_file );
	ProWstringFree( w_output_file );

	/* get the name of the part number to part name mapping file */
	status = ProUIInputpanelValueGet( "proe_brl", "name_file", &w_name_file );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get name of part number to part name mapping file\n" );
		ProUIDialogDestroy( "proe_brl" );
		return;
	}
	ProWstringToString( name_file, w_name_file );
	ProWstringFree( w_name_file );

	/* get starting ident */
	status = ProUIInputpanelValueGet( "proe_brl", "starting_ident", &tmp_str );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get starting ident\n" );
		ProUIDialogDestroy( "proe_brl" );
		return;
	}

	ProWstringToString( astr, tmp_str );
	ProWstringFree( tmp_str );
	reg_id = atoi( astr );
	if( reg_id < 1 )
		reg_id = 1;

	/* get max error */
	status = ProUIInputpanelValueGet( "proe_brl", "max_error", &tmp_str );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get max tesellation error\n" );
		ProUIDialogDestroy( "proe_brl" );
		return;
	}

	ProWstringToString( astr, tmp_str );
	ProWstringFree( tmp_str );
	max_error = atof( astr );

	/* get the angle control */
	status = ProUIInputpanelValueGet( "proe_brl", "angle_ctrl", &tmp_str );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get angle control\n" );
		ProUIDialogDestroy( "proe_brl" );
		return;
	}

	ProWstringToString( astr, tmp_str );
	ProWstringFree( tmp_str );
	angle_cntrl = atof( astr );

	/* check if user wants to do any CSG */
	status = ProUICheckbuttonGetState( "proe_brl", "facets_only", &do_facets_only );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get checkbutton setting (facetize only)\n" );
		ProUIDialogDestroy( "proe_brl" );
		return;
	}

	/* check if user wants to eliminate small features */
	status = ProUICheckbuttonGetState( "proe_brl", "elim_small", &do_elims );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get checkbutton setting (eliminate small features)\n" );
		ProUIDialogDestroy( "proe_brl" );
		return;
	}

	/* check if user wants surface normals in the BOT's */
	status = ProUICheckbuttonGetState( "proe_brl", "get_normals", &get_normals );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to get checkbutton setting (extract surface normals)\n" );
		ProUIDialogDestroy( "proe_brl" );
		return;
	}

	if( do_elims ) {

		/* get the minimum hole diameter */
		status = ProUIInputpanelValueGet( "proe_brl", "min_hole", &tmp_str );
		if( status != PRO_TK_NO_ERROR ) {
			fprintf( stderr, "Failed to get minimum hole diameter\n" );
			ProUIDialogDestroy( "proe_brl" );
			return;
		}

		ProWstringToString( astr, tmp_str );
		ProWstringFree( tmp_str );
		min_hole_diameter = atof( astr );
		if( min_hole_diameter < 0.0 )
			min_hole_diameter = 0.0;

		/* get the minimum chamfer dimension */
		status = ProUIInputpanelValueGet( "proe_brl", "min_chamfer", &tmp_str );
		if( status != PRO_TK_NO_ERROR ) {
			fprintf( stderr, "Failed to get minimum chamfer dimension\n" );
			ProUIDialogDestroy( "proe_brl" );
			return;
		}

		ProWstringToString( astr, tmp_str );
		ProWstringFree( tmp_str );
		min_chamfer_dim = atof( astr );
		if( min_chamfer_dim < 0.0 )
			min_chamfer_dim = 0.0;

		/* get the minimum round radius */
		status = ProUIInputpanelValueGet( "proe_brl", "min_round", &tmp_str );
		if( status != PRO_TK_NO_ERROR ) {
			fprintf( stderr, "Failed to get minimum round radius\n" );
			ProUIDialogDestroy( "proe_brl" );
			return;
		}

		ProWstringToString( astr, tmp_str );
		ProWstringFree( tmp_str );
		min_round_radius = atof( astr );

		if( min_round_radius < 0.0 )
			min_round_radius = 0.0;
	} else {
		min_hole_diameter = 0.0;
		min_round_radius = 0.0;
		min_chamfer_dim = 0.0;
	}

	/* open output file */
	if( (outfp=fopen( output_file, "w" ) ) == NULL ) {
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR", "Cannot open output file" );
		ProMessageClear();
		fprintf( stderr, "Cannot open output file\n" );
		perror( "\t" );
		ProUIDialogDestroy( "proe_brl" );
		return;
	}

	/* open log file, if a name was provided */
	if( strlen( log_file ) > 0 ) {
		if( strcmp( log_file, "stderr" ) == 0 ) {
			logger = stderr;
		} else if( (logger=fopen( log_file, "w" ) ) == NULL ) {
			(void)ProMessageDisplay(MSGFIL, "USER_ERROR", "Cannot open log file" );
			ProMessageClear();
			fprintf( stderr, "Cannot open log file\n" );
			perror( "\t" );
			ProUIDialogDestroy( "proe_brl" );
			return;
		}
	} else {
		logger = (FILE *)NULL;
	}

	/* open part name mapper file, if a name was provided */
	if( strlen( name_file ) > 0 ) {
		FILE *name_fd;

		if( logger ) {
			fprintf( logger, "Opening part name map file (%s)\n", name_file );
		}

		if( (name_fd=fopen( name_file, "r" ) ) == NULL ) {
			struct bu_vls error_msg;
			int dialog_return=0;
			wchar_t w_error_msg[512];

			if( logger ) {
				fprintf( logger, "Failed to open part name map file (%s)\n", name_file );
				fprintf( logger, "%s\n", strerror( errno ) );
			}

			bu_vls_init( &error_msg );
			(void)ProMessageDisplay(MSGFIL, "USER_ERROR", "Cannot open part name file" );
			ProMessageClear();
			fprintf( stderr, "Cannot open part name file\n" );
			perror( name_file );
			status = ProUIDialogCreate( "proe_brl_gen_error", "proe_brl_gen_error" );
			if( status != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to create error dialog (%d)\n", status );
			}
			(void)ProUIPushbuttonActivateActionSet( "proe_brl_gen_error",
								"ok_button",
								kill_gen_error_dialog, NULL );
			bu_vls_printf( &error_msg, "\n\tCannot open part name file (%s)\n\t",
				       name_file );
			bu_vls_strcat( &error_msg, strerror( errno ) );
			ProStringToWstring( w_error_msg, bu_vls_addr( &error_msg ) );
			status = ProUITextareaValueSet( "proe_brl_gen_error", "error_message", w_error_msg );
			if( status != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to set message for error dialog (%d)\n", status );
			}
			bu_vls_free( &error_msg );
			status = ProUIDialogActivate( "proe_brl_gen_error", &dialog_return );
			if( status != PRO_TK_NO_ERROR ) {
				fprintf( stderr, "Failed to activate error dialog (%d)\n", status );
			}
			ProUIDialogDestroy( "proe_brl" );
			return;
		}

		/* create a hash table of part numbers to part names */
		if( logger ) {
			fprintf( logger, "Creating name hash\n" );
		}

		ProStringToWstring( tmp_line, "Processing part name file" );
		status = ProUILabelTextSet( "proe_brl", "curr_proc", tmp_line );
		if( status != PRO_TK_NO_ERROR ) {
			fprintf( stderr, "Failed to update dialog label for currently processed part\n" );
		}
		status = ProUIDialogActivate( "proe_brl", &ret_status );
		if( status != PRO_TK_NO_ERROR ) {
			fprintf( stderr, "Error in proe-brl Dialog, error = %d\n",
				 status );
			fprintf( stderr, "\t dialog returned %d\n", ret_status );
		}

		name_hash = create_name_hash( name_fd );
		fclose( name_fd );
		
	} else {
		if( logger ) {
			fprintf( logger, "No name hash used\n" );
		}
		/* create an empty hash table */
		name_hash = bu_create_hash_tbl( 512 );
	}

	/* get the curently displayed model in Pro/E */
	status = ProMdlCurrentGet( &model );
	if( status == PRO_TK_BAD_CONTEXT ) {
		(void)ProMessageDisplay(MSGFIL, "USER_NO_MODEL" );
		ProMessageClear();
		fprintf( stderr, "No model is displayed!!\n" );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		ProUIDialogDestroy( "proe_brl" );
		if( name_hash ) {
			free_hash_values( name_hash );
			bu_hash_tbl_free( name_hash );
			name_hash = (struct bu_hash_tbl *)NULL;
		}
		return;
	}

	/* get its type */
	status = ProMdlTypeGet( model, &type );
	if( status == PRO_TK_BAD_INPUTS ) {
		(void)ProMessageDisplay(MSGFIL, "USER_NO_TYPE" );
		ProMessageClear();
		fprintf( stderr, "Cannot get type of current model\n" );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		ProUIDialogDestroy( "proe_brl" );
		if( name_hash ) {
			free_hash_values( name_hash );
			bu_hash_tbl_free( name_hash );
			name_hash = (struct bu_hash_tbl *)NULL;
		}
		return;
	}

	/* can only do parts and assemblies, no drawings, etc */
	if( type != PRO_MDL_ASSEMBLY && type != PRO_MDL_PART ) {
		(void)ProMessageDisplay(MSGFIL, "USER_TYPE_NOT_SOLID" );
		ProMessageClear();
		fprintf( stderr, "Current model is not a solid object\n" );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		ProUIDialogDestroy( "proe_brl" );
		if( name_hash ) {
			free_hash_values( name_hash );
			bu_hash_tbl_free( name_hash );
			name_hash = (struct bu_hash_tbl *)NULL;
		}
		return;
	}
#if 0
	/* skeleton models have no solid parts, but Pro/E will not let you recognize skeletons unless you buy the module */
	status = ProMdlIsSkeleton( model, &is_skeleton );
	if( status != PRO_TK_NO_ERROR ) {
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR", "Failed to determine if model is a skeleton" );
		ProMessageClear();
		fprintf( stderr, "Failed to determine if model is a skeleton, error = %d\n", status );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		ProUIDialogDestroy( "proe_brl" );
		return;
	}

	if( is_skeleton ) {
		(void)ProMessageDisplay(MSGFIL, "USER_SKELETON" );
		ProMessageClear();
		fprintf( stderr, "current model is a skeleton, cannot convert skeletons\n" );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		ProUIDialogDestroy( "proe_brl" );
		return;
	}
#endif
	/* get units, and adjust conversion factor */
	if( prodb_get_model_units( model, LENGTH_UNIT, &unit_subtype,
				   unit_name, &proe_conv ) ) {
		if( unit_subtype == UNIT_MM )
			proe_to_brl_conv = 1.0;
		else
			proe_to_brl_conv = proe_conv * 25.4;
	} else {
		ProMessageDisplay(MSGFIL, "USER_NO_UNITS" );
		ProMessageClear();
		fprintf( stderr, "No units specified, assuming inches\n" );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		proe_to_brl_conv = 25.4;
	}

	/* adjust tolerance for Pro/E units */
	local_tol = tol_dist / proe_to_brl_conv;
	local_tol_sq = local_tol * local_tol;

	vert_tree_root = create_vert_tree();
	norm_tree_root = create_vert_tree();

	/* output the top level object
	 * this will recurse through the entire model
	 */
	output_top_level_object( model, type );

	/* kill any references to empty parts */
	kill_empty_parts();

	/* let user know we are done */
	ProStringToWstring( tmp_line, "Conversion complete" );
	ProUILabelTextSet( "proe_brl", "curr_proc", tmp_line );

	/* free a bunch of stuff */
	if( done_list_part ) {
		bu_rb_free( done_list_part, free_rb_data );
		done_list_part = NULL;
	}

	if( done_list_asm ) {
		bu_rb_free( done_list_asm, free_rb_data );
		done_list_asm = NULL;
	}

	if( part_tris ) {
		bu_free( (char *)part_tris, "part triangles" );
		part_tris = NULL;
	}

	if( part_norms ) {
		bu_free( (char *)part_norms, "part normals" );
		part_norms = NULL;
	}

	free_vert_tree( vert_tree_root );
	vert_tree_root = NULL;
	free_vert_tree( norm_tree_root );
	norm_tree_root = NULL;

	max_tri = 0;

	free_empty_parts();

	if( logger ) {
		fprintf( logger, "Closing output file\n" );
	}

	fclose( outfp );

	if( name_hash ) {
		if( logger ) {
			fprintf( logger, "freeing name hash\n" );
		}
		free_hash_values( name_hash );
		bu_hash_tbl_free( name_hash );
		name_hash = (struct bu_hash_tbl *)NULL;
	}

	if( brlcad_names ) {
		if( logger ) {
			fprintf( logger, "freeing name rb_tree\n" );
		}
		bu_rb_free( brlcad_names, NULL );	/* data was already freed by free_hash_values() */
		brlcad_names = (bu_rb_tree *)NULL;
	}

	if( logger ) {
		fprintf( logger, "Closing logger file\n" );
		fclose( logger );
		logger = (FILE *)NULL;
	}

	return;
}

void
elim_small_activate( char *dialog_name, char *button_name, ProAppData data )
{
	ProBoolean state;
	ProError status;

	status = ProUICheckbuttonGetState( dialog_name, button_name, &state );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "checkbutton activate routine: failed to get state\n" );
		return;
	}

	if( state ) {
		status = ProUIInputpanelEditable( dialog_name, "min_hole" );
		if( status != PRO_TK_NO_ERROR ) {
			fprintf( stderr, "Failed to activate \"minimum hole diameter\"\n" );
			return;
		}
		status = ProUIInputpanelEditable( dialog_name, "min_chamfer" );
		if( status != PRO_TK_NO_ERROR ) {
			fprintf( stderr, "Failed to activate \"minimum chamfer dimension\"\n" );
			return;
		}
		status = ProUIInputpanelEditable( dialog_name, "min_round" );
		if( status != PRO_TK_NO_ERROR ) {
			fprintf( stderr, "Failed to activate \"minimum round radius\"\n" );
			return;
		}
	} else {
		status = ProUIInputpanelReadOnly( dialog_name, "min_hole" );
		if( status != PRO_TK_NO_ERROR ) {
			fprintf( stderr, "Failed to de-activate \"minimum hole diameter\"\n" );
			return;
		}
		status = ProUIInputpanelReadOnly( dialog_name, "min_chamfer" );
		if( status != PRO_TK_NO_ERROR ) {
			fprintf( stderr, "Failed to de-activate \"minimum chamfer dimension\"\n" );
			return;
		}
		status = ProUIInputpanelReadOnly( dialog_name, "min_round" );
		if( status != PRO_TK_NO_ERROR ) {
			fprintf( stderr, "Failed to de-activate \"minimum round radius\"\n" );
			return;
		}
	}
}

/* this is the driver routine for converting Pro/E to BRL-CAD */
int
proe_brl( uiCmdCmdId command, uiCmdValue *p_value, void *p_push_cmd_data )
{
	ProError status;
	int ret_status=0;

	empty_parts_root = NULL;
#if 1
	/* use UI dialog */
	status = ProUIDialogCreate( "proe_brl", "proe_brl" );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to create dialog box for proe-brl, error = %d\n", status );
		return( 0 );
	}

	status = ProUICheckbuttonActivateActionSet( "proe_brl", "elim_small", elim_small_activate, NULL );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to set action for \"eliminate small features\" checkbutton, error = %d\n", status );
		return( 0 );
	}

	status = ProUIPushbuttonActivateActionSet( "proe_brl", "doit", doit, NULL );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to set action for 'Go' button\n" );	
		ProUIDialogDestroy( "proe_brl" );
		return( 0 );
	}

	status = ProUIPushbuttonActivateActionSet( "proe_brl", "quit", do_quit, NULL );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to set action for 'Go' button\n" );	
		ProUIDialogDestroy( "proe_brl" );
		return( 0 );
	}

	status = ProUIDialogActivate( "proe_brl", &ret_status );
	if( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Error in proe-brl Dialog, error = %d\n",
			 status );
		fprintf( stderr, "\t dialog returned %d\n", ret_status );
	}

#else
	/* get the curently displayed model in Pro/E */
	status = ProMdlCurrentGet( &model );
	if( status == PRO_TK_BAD_CONTEXT ) {
		ProName dialog_label;
		ProLine w_answer;
		char answer[PRO_LINE_SIZE];
		char *ptr;

		ProStringToWstring( dialog_label, "Select an Object for Conversion" );
		ProStringToWstring( dialog_filter, "*.prt,*.asm" );
		status = ProFileOpen( dialog_label, dialog_filter, (ProPath *)NULL,
				      (ProName *)NULL, NULL,
				      NULL, file_to_open );
		if( status != PRO_TK_NO_ERROR )
			return status;

		(void)ProWstringToString( file_name, file_to_open );
	}

	/* default output file name */
	strcpy( output_file, "proe.asc" );

	/* get the output file name */
	(void)ProMessageDisplay( MSGFIL, "USER_PROMPT_STRING",
				 "Enter name of file to receive output: ",
				 output_file );
	status = ProMessageStringRead( 127, w_output_file );
	if( status == PRO_TK_NO_ERROR ) {
		(void)ProWstringToString( output_file, w_output_file );
	} else if( status == PRO_TK_MSG_USER_QUIT) {
		return( 0 );
	}

	/* get starting ident number */
	(void)ProMessageDisplay( MSGFIL, "USER_PROMPT_INT",
				 "Enter starting ident number: ",
				 &reg_id );
	status = ProMessageIntegerRead( NULL, &reg_id );
	if( status == PRO_TK_MSG_USER_QUIT ) {
		return( 0 );
	}

	/* get the maximum allowed error */
	(void)ProMessageDisplay( MSGFIL, "USER_PROMPT_DOUBLE",
				 "Enter maximum allowable error for tessellation (mm): ",
				 &max_error );
	range[0] = 0.0;
	range[1] = 500.0;
	status = ProMessageDoubleRead( range, &max_error );
	if( status == PRO_TK_MSG_USER_QUIT ) {
		return( 0 );
	}

	/* get the angle control */
	(void) ProMessageDisplay( MSGFIL, "USER_PROMPT_DOUBLE",
				  "Enter a value for angle control: ",
				  &angle_cntrl );
	range[0] = 0.0;
	range[1] = 1.0;
	status = ProMessageDoubleRead( range, &angle_cntrl );
	if( status == PRO_TK_MSG_USER_QUIT ) {
		return( 0 );
	}

	/* get the minimum hole diameter */
	(void) ProMessageDisplay( MSGFIL, "USER_PROMPT_DOUBLE",
				  "Enter the minimum allowed hole diameter (smaller holes will be deleted): ",
				  &min_hole_diameter );
	status = ProMessageDoubleRead( NULL, &min_hole_diameter );
	if( status == PRO_TK_MSG_USER_QUIT ) {
		return( 0 );
	}
	if( min_hole_diameter < 0.0 ) {
		min_hole_diameter = 0.0;
	}

	/* get the minimum round radius */
	(void) ProMessageDisplay( MSGFIL, "USER_PROMPT_DOUBLE",
				  "Enter the minimum allowed round radius (smaller rounds will be deleted): ",
				  &min_round_radius );
	status = ProMessageDoubleRead( NULL, &min_round_radius );
	if( status == PRO_TK_MSG_USER_QUIT ) {
		return( 0 );
	}
	if( min_round_radius < 0.0 ) {
		min_round_radius = 0.0;
	}

	/* get the minimum chamfer dimension */
	(void) ProMessageDisplay( MSGFIL, "USER_PROMPT_DOUBLE",
				  "Enter the minimum allowed chamfer dimension (smaller chamfers will be deleted): ",
				  &min_chamfer_dim );
	status = ProMessageDoubleRead( NULL, &min_chamfer_dim );
	if( status == PRO_TK_MSG_USER_QUIT ) {
		return( 0 );
	}
	if( min_chamfer_dim < 0.0 ) {
		min_chamfer_dim = 0.0;
	}

	/* initailize */
	do_initialize();

	/* open output file */
	mode = create_for_writing;
	if( (outfp=fopen( output_file, mode ) ) == NULL ) {
		(void)ProMessageDisplay(MSGFIL, "USER_ERROR", "Cannot open output file" );
		ProMessageClear();
		fprintf( stderr, "Cannot open output file\n" );
		perror( "\t" );
		return( PRO_TK_GENERAL_ERROR );
	}

	/* get model type */
	status = ProMdlTypeGet( model, &type );
	if( status == PRO_TK_BAD_INPUTS ) {
		(void)ProMessageDisplay(MSGFIL, "USER_NO_TYPE" );
		ProMessageClear();
		fprintf( stderr, "Cannot get type of current model\n" );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( PRO_TK_NO_ERROR );
	}

	/* can only do parts and assemblies, no drawings, etc */
	if( type != PRO_MDL_ASSEMBLY && type != PRO_MDL_PART ) {
		(void)ProMessageDisplay(MSGFIL, "USER_TYPE_NOT_SOLID" );
		ProMessageClear();
		fprintf( stderr, "Current model is not a solid object\n" );
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( PRO_TK_NO_ERROR );
	}

	/* get units, and adjust conversion factor */
	model_units( model );

	vert_tree_root = create_vert_tree();

	/* output the top level object
	 * this will recurse through the entire model
	 */
	output_top_level_object( model, type );

	free_vert_tree( vert_tree_root );

	/* kill any references to empty parts */
	kill_empty_parts();

	/* let user know we are done */
	ProMessageDisplay(MSGFIL, "USER_INFO", "Conversion complete" );

	/* let user know we are done */
	ProStringToWstring( tmp_line, "Conversion complete" );
	ProUILabelTextSet( "proe_brl", "curr_proc", tmp_line );

	/* free a bunch of stuff */
	if( done_list_part ) {
		bu_rb_free( done_list_part, free_rb_data );
		done_list_part = NULL;
	}

	if( done_list_asm ) {
		bu_rb_free( done_list_asm, free_rb_data );
		done_list_asm = NULL;
	}

	/* free a bunch of stuff */
	if( done ) {
		bu_free( (char *)done, "done" );
	}
	done = NULL;
	max_done = 0;
	curr_done = 0;

	for( i=0 ; i<BU_PTBL_LEN( &search_path_list ) ; i++ ) {
		bu_free( (char *)BU_PTBL_GET( &search_path_list, i ), "search_path entry" );
	}
	bu_ptbl_free( &search_path_list );

	if( part_tris ) {
		bu_free( (char *)part_tris, "part triangles" );
	}
	part_tris = NULL;

	free_vert_tree( vert_tree_root );

	max_tri = 0;

	free_empty_parts();

	fclose( outfp );

	/* list summary of objects and feature type seen */
	fprintf( stderr, "Object types encountered:\n" );
	for( i=0 ; i<NUM_OBJ_TYPES ; i++ ) {
		if( !obj_type_count[i] )
			continue;
		fprintf( stderr, "\t%s\t%d\n", obj_type[i], obj_type_count[i] );
	}
	fprintf( stderr, "Feature types encountered:\n" );
	for( i=0 ; i<NUM_FEAT_TYPES ; i++ ) {
		if( !feat_type_count[i] )
			continue;
		fprintf( stderr, "\t%s\t%d\n", feat_type[i], feat_type_count[i] );
	}
#endif
	return( 0 );
}

/* this routine determines whether the "proe-brl" menu item in Pro/E
 * should be displayed as available or greyed out
 */
static uiCmdAccessState
proe_brl_access( uiCmdAccessMode access_mode )
{

#if 1
	/* doing the correct checks appears to be unreliable */
	return( ACCESS_AVAILABLE );
#else
	ProMode mode;
	ProError status;

	status = ProModeCurrentGet( &mode );
	if ( status != PRO_TK_NO_ERROR ) {
		return( ACCESS_UNAVAILABLE );
	}

	/* only allow our menu item to be used when parts or assemblies are displayed */
	if( mode == PRO_MODE_ASSEMBLY || mode == PRO_MODE_PART ) {
		return( ACCESS_AVAILABLE );
	} else {
		return( ACCESS_UNAVAILABLE );
	}
#endif
}

/* routine to add our menu item */
int
user_initialize( int argc, char *argv[], char *version, char *build, wchar_t errbuf[80] )
{
	ProError status;
	int i;
	uiCmdCmdId cmd_id;

	/* Pro/E says always check the size of w_char */
	status = ProWcharSizeVerify (sizeof (wchar_t), &i);
	if ( status != PRO_TK_NO_ERROR || (i != sizeof (wchar_t)) ) {
		sprintf(astr,"ERROR wchar_t Incorrect size (%d). Should be: %d",
			sizeof(wchar_t), i );
		status = ProMessageDisplay(MSGFIL, "USER_ERROR", astr);
		printf("%s\n", astr);
		ProStringToWstring(errbuf, astr);
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return(-1);
	}

	/* add a command that calls our proe-brl routine */
	status = ProCmdActionAdd( "Proe-BRL", (uiCmdCmdActFn)proe_brl, uiProe2ndImmediate,
				  proe_brl_access, PRO_B_FALSE, PRO_B_FALSE, &cmd_id );
	if( status != PRO_TK_NO_ERROR ) {
		sprintf( astr, "Failed to add proe-brl action" );
		fprintf( stderr, "%s\n", astr);
		ProMessageDisplay(MSGFIL, "USER_ERROR", astr);
		ProStringToWstring(errbuf, astr);
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( -1 );
	}

	/* add a menu item that runs the new command */
	status = ProMenubarmenuPushbuttonAdd( "File", "Proe-BRL", "Proe-BRL", "Proe-BRL-HELP",
					      "File.psh_exit", PRO_B_FALSE, cmd_id, MSGFIL );
	if( status != PRO_TK_NO_ERROR ) {
		sprintf( astr, "Failed to add proe-brl menu button" );
		fprintf( stderr, "%s\n", astr);
		ProMessageDisplay(MSGFIL, "USER_ERROR", astr);
		ProStringToWstring(errbuf, astr);
		(void)ProWindowRefresh( PRO_VALUE_UNUSED );
		return( -1 );
	}

	/* let user know we are here */
	ProMessageDisplay( MSGFIL, "OK" );
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );

	return( 0 );
}

void
do_quit( char *dialog, char *compnent, ProAppData appdata )
{
	ProUIDialogDestroy( "proe_brl" );
}


void
user_terminate()
{
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
