/*                    M A I N . C P P
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
/** @file main.cpp
 *
 */

#include "common.h"
#include "creo-brl.h"

extern "C" void
creo_conv_info_init(struct creo_conv_info *cinfo)
{
    int i;

    /* Region ID */
    cinfo->reg_id = 1000;

    /* File settings */
    cinfo->outfp=NULL;
    cinfo->logger=NULL;
    cinfo->logger_type=LOGGER_TYPE_NONE;

    /* units - model */
    cinfo->creo_to_brl_conv = 25.4; /* inches to mm */
    cinfo->local_tol=0.0;
    cinfo->local_tol_sq=0.0;

    /* facetization settings */
    cinfo->max_error=1.5;
    cinfo->min_error=1.5;
    cinfo->tol_dist=0.0005;
    cinfo->max_angle_cntrl=0.5;
    cinfo->min_angle_cntrl=0.5;
    cinfo->max_to_min_steps = 1;
    cinfo->error_increment=0.0;
    cinfo->angle_increment=0.0;

    /* csg settings */
    cinfo->min_hole_diameter=0.0;
    cinfo->min_chamfer_dim=0.0;
    cinfo->min_round_radius=0.0;

    /* features */
    cinfo->feat_ids_to_delete=NULL;
    cinfo->feat_id_len=0;
    cinfo->feat_id_count=0;

    /* current part triangles */
    cinfo->part_tris=NULL;
    cinfo->max_tri=0;
    cinfo->curr_tri=0;
    cinfo->part_norms=NULL;

    cinfo->hole_no=0;
    cinfo->csg_root=NULL;
    cinfo->empty_parts_root=NULL;


    for ( i=0; i<NUM_OBJ_TYPES; i++ ) {
	cinfo->obj_type_count[i] = 0;
	cinfo->obj_type[i] = NULL;
    }

    for ( i=0; i<NUM_FEAT_TYPES; i++ ) {
	cinfo->feat_type_count[i] = 0;
	cinfo->feat_type[i] = NULL;
    }

    cinfo->obj_type[0] = "PRO_TYPE_UNUSED";
    cinfo->obj_type[1] = "PRO_ASSEMBLY";
    cinfo->obj_type[2] = "PRO_PART";
    cinfo->obj_type[3] = "PRO_FEATURE";
    cinfo->obj_type[4] = "PRO_DRAWING";
    cinfo->obj_type[5] = "PRO_SURFACE";
    cinfo->obj_type[6] = "PRO_EDGE";
    cinfo->obj_type[7] = "PRO_3DSECTION";
    cinfo->obj_type[8] = "PRO_DIMENSION";
    cinfo->obj_type[11] = "PRO_2DSECTION";
    cinfo->obj_type[12] = "PRO_PAT_MEMBER";
    cinfo->obj_type[13] = "PRO_PAT_LEADER";
    cinfo->obj_type[19] = "PRO_LAYOUT";
    cinfo->obj_type[21] = "PRO_AXIS";
    cinfo->obj_type[25] = "PRO_CSYS";
    cinfo->obj_type[28] = "PRO_REF_DIMENSION";
    cinfo->obj_type[32] = "PRO_GTOL";
    cinfo->obj_type[33] = "PRO_DWGFORM";
    cinfo->obj_type[34] = "PRO_SUB_ASSEMBLY";
    cinfo->obj_type[37] = "PRO_MFG";
    cinfo->obj_type[57] = "PRO_QUILT";
    cinfo->obj_type[62] = "PRO_CURVE";
    cinfo->obj_type[66] = "PRO_POINT";
    cinfo->obj_type[68] = "PRO_NOTE";
    cinfo->obj_type[69] = "PRO_IPAR_NOTE";
    cinfo->obj_type[71] = "PRO_EDGE_START";
    cinfo->obj_type[72] = "PRO_EDGE_END";
    cinfo->obj_type[74] = "PRO_CRV_START";
    cinfo->obj_type[75] = "PRO_CRV_END";
    cinfo->obj_type[76] = "PRO_SYMBOL_INSTANCE";
    cinfo->obj_type[77] = "PRO_DRAFT_ENTITY";
    cinfo->obj_type[79] = "PRO_DRAFT_DATUM";
    cinfo->obj_type[83] = "PRO_DRAFT_GROUP";
    cinfo->obj_type[84] = "PRO_DRAW_TABLE";
    cinfo->obj_type[92] = "PRO_VIEW";
    cinfo->obj_type[96] = "PRO_CABLE";
    cinfo->obj_type[105] = "PRO_REPORT";
    cinfo->obj_type[116] = "PRO_MARKUP";
    cinfo->obj_type[117] = "PRO_LAYER";
    cinfo->obj_type[121] = "PRO_DIAGRAM";
    cinfo->obj_type[133] = "PRO_SKETCH_ENTITY";
    cinfo->obj_type[144] = "PRO_DATUM_TEXT";
    cinfo->obj_type[145] = "PRO_ENTITY_TEXT";
    cinfo->obj_type[147] = "PRO_DRAW_TABLE_CELL";
    cinfo->obj_type[176] = "PRO_DATUM_PLANE";
    cinfo->obj_type[180] = "PRO_COMP_CRV";
    cinfo->obj_type[211] = "PRO_BND_TABLE";
    cinfo->obj_type[240] = "PRO_PARAMETER";
    cinfo->obj_type[305] = "PRO_DIAGRAM_OBJECT";
    cinfo->obj_type[308] = "PRO_DIAGRAM_WIRE";
    cinfo->obj_type[309] = "PRO_SIMP_REP";
    cinfo->obj_type[371] = "PRO_WELD_PARAMS";
    cinfo->obj_type[377] = "PRO_SNAP_LINE";
    cinfo->obj_type[385] = "PRO_EXTOBJ";
    cinfo->obj_type[500] = "PRO_EXPLD_STATE";
    cinfo->obj_type[504] = "PRO_CABLE_LOCATION";
    cinfo->obj_type[533] = "PRO_RELSET";
    cinfo->obj_type[555] = "PRO_ANALYSIS";
    cinfo->obj_type[556] = "PRO_SURF_CRV";
    cinfo->obj_type[625] = "PRO_LOG_SRF";
    cinfo->obj_type[622] = "PRO_SOLID_GEOMETRY";
    cinfo->obj_type[626] = "PRO_LOG_EDG";
    cinfo->obj_type[627] = "PRO_DESKTOP";
    cinfo->obj_type[628] = "PRO_SYMBOL_DEFINITION";

    cinfo->feat_type[0] = "PRO_FEAT_FIRST_FEAT";
    cinfo->feat_type[911 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HOLE";
    cinfo->feat_type[912 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SHAFT";
    cinfo->feat_type[913 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ROUND";
    cinfo->feat_type[914 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CHAMFER";
    cinfo->feat_type[915 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SLOT";
    cinfo->feat_type[916 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CUT";
    cinfo->feat_type[917 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PROTRUSION";
    cinfo->feat_type[918 - FEAT_TYPE_OFFSET] = "PRO_FEAT_NECK";
    cinfo->feat_type[919 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FLANGE";
    cinfo->feat_type[920 - FEAT_TYPE_OFFSET] = "PRO_FEAT_RIB";
    cinfo->feat_type[921 - FEAT_TYPE_OFFSET] = "PRO_FEAT_EAR";
    cinfo->feat_type[922 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DOME";
    cinfo->feat_type[923 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DATUM";
    cinfo->feat_type[924 - FEAT_TYPE_OFFSET] = "PRO_FEAT_LOC_PUSH";
    cinfo->feat_type[925 - FEAT_TYPE_OFFSET] = "PRO_FEAT_UDF";
    cinfo->feat_type[926 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DATUM_AXIS";
    cinfo->feat_type[927 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRAFT";
    cinfo->feat_type[928 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SHELL";
    cinfo->feat_type[929 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DOME2";
    cinfo->feat_type[930 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CORN_CHAMF";
    cinfo->feat_type[931 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DATUM_POINT";
    cinfo->feat_type[932 - FEAT_TYPE_OFFSET] = "PRO_FEAT_IMPORT";
    cinfo->feat_type[932 - FEAT_TYPE_OFFSET] = "PRO_FEAT_IGES";
    cinfo->feat_type[933 - FEAT_TYPE_OFFSET] = "PRO_FEAT_COSMETIC";
    cinfo->feat_type[934 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ETCH";
    cinfo->feat_type[935 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MERGE";
    cinfo->feat_type[936 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MOLD";
    cinfo->feat_type[937 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SAW";
    cinfo->feat_type[938 - FEAT_TYPE_OFFSET] = "PRO_FEAT_TURN";
    cinfo->feat_type[939 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MILL";
    cinfo->feat_type[940 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRILL";
    cinfo->feat_type[941 - FEAT_TYPE_OFFSET] = "PRO_FEAT_OFFSET";
    cinfo->feat_type[942 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DATUM_SURF";
    cinfo->feat_type[943 - FEAT_TYPE_OFFSET] = "PRO_FEAT_REPLACE_SURF";
    cinfo->feat_type[944 - FEAT_TYPE_OFFSET] = "PRO_FEAT_GROOVE";
    cinfo->feat_type[945 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE";
    cinfo->feat_type[946 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DATUM_QUILT";
    cinfo->feat_type[947 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ASSEM_CUT";
    cinfo->feat_type[948 - FEAT_TYPE_OFFSET] = "PRO_FEAT_UDF_THREAD";
    cinfo->feat_type[949 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CURVE";
    cinfo->feat_type[950 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SRF_MDL";
    cinfo->feat_type[952 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WALL";
    cinfo->feat_type[953 - FEAT_TYPE_OFFSET] = "PRO_FEAT_BEND";
    cinfo->feat_type[954 - FEAT_TYPE_OFFSET] = "PRO_FEAT_UNBEND";
    cinfo->feat_type[955 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CUT_SMT";
    cinfo->feat_type[956 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FORM";
    cinfo->feat_type[957 - FEAT_TYPE_OFFSET] = "PRO_FEAT_THICKEN";
    cinfo->feat_type[958 - FEAT_TYPE_OFFSET] = "PRO_FEAT_BEND_BACK";
    cinfo->feat_type[959 - FEAT_TYPE_OFFSET] = "PRO_FEAT_UDF_NOTCH";
    cinfo->feat_type[960 - FEAT_TYPE_OFFSET] = "PRO_FEAT_UDF_PUNCH";
    cinfo->feat_type[961 - FEAT_TYPE_OFFSET] = "PRO_FEAT_INT_UDF";
    cinfo->feat_type[962 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SPLIT_SURF";
    cinfo->feat_type[963 - FEAT_TYPE_OFFSET] = "PRO_FEAT_GRAPH";
    cinfo->feat_type[964 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_MFG_PUNCH";
    cinfo->feat_type[965 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_MFG_CUT";
    cinfo->feat_type[966 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FLATTEN";
    cinfo->feat_type[967 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SET";
    cinfo->feat_type[968 - FEAT_TYPE_OFFSET] = "PRO_FEAT_VDA";
    cinfo->feat_type[969 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_MFG_FORM";
    cinfo->feat_type[970 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_PUNCH_PNT";
    cinfo->feat_type[971 - FEAT_TYPE_OFFSET] = "PRO_FEAT_LIP";
    cinfo->feat_type[972 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MANUAL";
    cinfo->feat_type[973 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MFG_GATHER";
    cinfo->feat_type[974 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MFG_TRIM";
    cinfo->feat_type[975 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MFG_USEVOL";
    cinfo->feat_type[976 - FEAT_TYPE_OFFSET] = "PRO_FEAT_LOCATION";
    cinfo->feat_type[977 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CABLE_SEGM";
    cinfo->feat_type[978 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CABLE";
    cinfo->feat_type[979 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CSYS";
    cinfo->feat_type[980 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CHANNEL";
    cinfo->feat_type[937 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WIRE_EDM";
    cinfo->feat_type[981 - FEAT_TYPE_OFFSET] = "PRO_FEAT_AREA_NIBBLE";
    cinfo->feat_type[982 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PATCH";
    cinfo->feat_type[983 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PLY";
    cinfo->feat_type[984 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CORE";
    cinfo->feat_type[985 - FEAT_TYPE_OFFSET] = "PRO_FEAT_EXTRACT";
    cinfo->feat_type[986 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MFG_REFINE";
    cinfo->feat_type[987 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SILH_TRIM";
    cinfo->feat_type[988 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SPLIT";
    cinfo->feat_type[989 - FEAT_TYPE_OFFSET] = "PRO_FEAT_EXTEND";
    cinfo->feat_type[990 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SOLIDIFY";
    cinfo->feat_type[991 - FEAT_TYPE_OFFSET] = "PRO_FEAT_INTERSECT";
    cinfo->feat_type[992 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ATTACH";
    cinfo->feat_type[993 - FEAT_TYPE_OFFSET] = "PRO_FEAT_XSEC";
    cinfo->feat_type[994 - FEAT_TYPE_OFFSET] = "PRO_FEAT_UDF_ZONE";
    cinfo->feat_type[995 - FEAT_TYPE_OFFSET] = "PRO_FEAT_UDF_CLAMP";
    cinfo->feat_type[996 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRL_GRP";
    cinfo->feat_type[997 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ISEGM";
    cinfo->feat_type[998 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CABLE_COSM";
    cinfo->feat_type[999 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SPOOL";
    cinfo->feat_type[1000 - FEAT_TYPE_OFFSET] = "PRO_FEAT_COMPONENT";
    cinfo->feat_type[1001 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MFG_MERGE";
    cinfo->feat_type[1002 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FIXSETUP";
    cinfo->feat_type[1002 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SETUP";
    cinfo->feat_type[1003 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FLAT_PAT";
    cinfo->feat_type[1004 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CONT_MAP";
    cinfo->feat_type[1005 - FEAT_TYPE_OFFSET] = "PRO_FEAT_EXP_RATIO";
    cinfo->feat_type[1006 - FEAT_TYPE_OFFSET] = "PRO_FEAT_RIP";
    cinfo->feat_type[1007 - FEAT_TYPE_OFFSET] = "PRO_FEAT_OPERATION";
    cinfo->feat_type[1008 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WORKCELL";
    cinfo->feat_type[1009 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CUT_MOTION";
    cinfo->feat_type[1013 - FEAT_TYPE_OFFSET] = "PRO_FEAT_BLD_PATH";
    cinfo->feat_type[1013 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CUSTOMIZE";
    cinfo->feat_type[1014 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRV_TOOL_SKETCH";
    cinfo->feat_type[1015 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRV_TOOL_EDGE";
    cinfo->feat_type[1016 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRV_TOOL_CURVE";
    cinfo->feat_type[1017 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRV_TOOL_SURF";
    cinfo->feat_type[1018 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MAT_REMOVAL";
    cinfo->feat_type[1019 - FEAT_TYPE_OFFSET] = "PRO_FEAT_TORUS";
    cinfo->feat_type[1020 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE_SET_START";
    cinfo->feat_type[1021 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE_PNT_PNT";
    cinfo->feat_type[1022 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE_EXT";
    cinfo->feat_type[1023 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE_TRIM";
    cinfo->feat_type[1024 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE_FOLL";
    cinfo->feat_type[1025 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE_JOIN";
    cinfo->feat_type[1026 - FEAT_TYPE_OFFSET] = "PRO_FEAT_AUXILIARY";
    cinfo->feat_type[1027 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE_LINE";
    cinfo->feat_type[1028 - FEAT_TYPE_OFFSET] = "PRO_FEAT_LINE_STOCK";
    cinfo->feat_type[1029 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SLD_PIPE ";
    cinfo->feat_type[1030 - FEAT_TYPE_OFFSET] = "PRO_FEAT_BULK_OBJECT";
    cinfo->feat_type[1031 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SHRINKAGE  ";
    cinfo->feat_type[1032 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE_JOINT ";
    cinfo->feat_type[1033 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PIPE_BRANCH ";
    cinfo->feat_type[1034 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRV_TOOL_TWO_CNTR";
    cinfo->feat_type[1035 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SUBHARNESS";
    cinfo->feat_type[1036 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_OPTIMIZE";
    cinfo->feat_type[1037 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DECLARE";
    cinfo->feat_type[1038 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_POPULATE";
    cinfo->feat_type[1039 - FEAT_TYPE_OFFSET] = "PRO_FEAT_OPER_COMP";
    cinfo->feat_type[1040 - FEAT_TYPE_OFFSET] = "PRO_FEAT_MEASURE";
    cinfo->feat_type[1041 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRAFT_LINE";
    cinfo->feat_type[1042 - FEAT_TYPE_OFFSET] = "PRO_FEAT_REMOVE_SURFS";
    cinfo->feat_type[1043 - FEAT_TYPE_OFFSET] = "PRO_FEAT_RIBBON_CABLE";
    cinfo->feat_type[1046 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ATTACH_VOLUME";
    cinfo->feat_type[1047 - FEAT_TYPE_OFFSET] = "PRO_FEAT_BLD_OPERATION";
    cinfo->feat_type[1048 - FEAT_TYPE_OFFSET] = "PRO_FEAT_UDF_WRK_REG";
    cinfo->feat_type[1049 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SPINAL_BEND";
    cinfo->feat_type[1050 - FEAT_TYPE_OFFSET] = "PRO_FEAT_TWIST";
    cinfo->feat_type[1051 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FREE_FORM";
    cinfo->feat_type[1052 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ZONE";
    cinfo->feat_type[1053 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WELDING_ROD";
    cinfo->feat_type[1054 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WELD_FILLET";
    cinfo->feat_type[1055 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WELD_GROOVE";
    cinfo->feat_type[1056 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WELD_PLUG_SLOT";
    cinfo->feat_type[1057 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WELD_SPOT";
    cinfo->feat_type[1058 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_SHEAR";
    cinfo->feat_type[1059 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PATH_SEGM";
    cinfo->feat_type[1060 - FEAT_TYPE_OFFSET] = "PRO_FEAT_RIBBON_SEGM";
    cinfo->feat_type[1059 - FEAT_TYPE_OFFSET] = "PRO_FEAT_RIBBON_PATH";
    cinfo->feat_type[1060 - FEAT_TYPE_OFFSET] = "PRO_FEAT_RIBBON_EXTEND";
    cinfo->feat_type[1061 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ASMCUT_COPY";
    cinfo->feat_type[1062 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DEFORM_AREA";
    cinfo->feat_type[1063 - FEAT_TYPE_OFFSET] = "PRO_FEAT_RIBBON_SOLID";
    cinfo->feat_type[1064 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FLAT_RIBBON_SEGM";
    cinfo->feat_type[1065 - FEAT_TYPE_OFFSET] = "PRO_FEAT_POSITION_FOLD";
    cinfo->feat_type[1066 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SPRING_BACK";
    cinfo->feat_type[1067 - FEAT_TYPE_OFFSET] = "PRO_FEAT_BEAM_SECTION";
    cinfo->feat_type[1068 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SHRINK_DIM";
    cinfo->feat_type[1070 - FEAT_TYPE_OFFSET] = "PRO_FEAT_THREAD";
    cinfo->feat_type[1071 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_CONVERSION";
    cinfo->feat_type[1072 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CMM_MEASSTEP";
    cinfo->feat_type[1073 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CMM_CONSTR";
    cinfo->feat_type[1074 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CMM_VERIFY";
    cinfo->feat_type[1075 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CAV_SCAN_SET";
    cinfo->feat_type[1076 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CAV_FIT";
    cinfo->feat_type[1077 - FEAT_TYPE_OFFSET] = "PRO_FEAT_CAV_DEVIATION";
    cinfo->feat_type[1078 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_ZONE";
    cinfo->feat_type[1079 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_CLAMP";
    cinfo->feat_type[1080 - FEAT_TYPE_OFFSET] = "PRO_FEAT_PROCESS_STEP";
    cinfo->feat_type[1081 - FEAT_TYPE_OFFSET] = "PRO_FEAT_EDGE_BEND";
    cinfo->feat_type[1082 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRV_TOOL_PROF";
    cinfo->feat_type[1083 - FEAT_TYPE_OFFSET] = "PRO_FEAT_EXPLODE_LINE";
    cinfo->feat_type[1084 - FEAT_TYPE_OFFSET] = "PRO_FEAT_GEOM_COPY";
    cinfo->feat_type[1085 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ANALYSIS";
    cinfo->feat_type[1086 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WATER_LINE";
    cinfo->feat_type[1087 - FEAT_TYPE_OFFSET] = "PRO_FEAT_UDF_RMDT";
    cinfo->feat_type[1088 - FEAT_TYPE_OFFSET] = "PRO_FEAT_VOL_SPLIT";
    cinfo->feat_type[1089 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WLD_EDG_PREP";
    cinfo->feat_type[1090 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMM_OFFSET";
    cinfo->feat_type[1091 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMM_MATREM";
    cinfo->feat_type[1092 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMM_COSMETIC";
    cinfo->feat_type[1093 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMM_APPROACH";
    cinfo->feat_type[1094 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMM_SLOT";
    cinfo->feat_type[1095 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMM_SHAPE";
    cinfo->feat_type[1096 - FEAT_TYPE_OFFSET] = "PRO_FEAT_IPM_QUILT";
    cinfo->feat_type[1097 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRVD";
    cinfo->feat_type[1098 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_CRN_REL";
    cinfo->feat_type[1101 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SLDBEND";
    cinfo->feat_type[1102 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FLATQLT ";
    cinfo->feat_type[1103 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DRV_TOOL_TURN ";
    cinfo->feat_type[1104 - FEAT_TYPE_OFFSET] = "PRO_FEAT_GROUP_HEAD";
    cinfo->feat_type[1211 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_PLATE";
    cinfo->feat_type[1212 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_HOLE  ";
    cinfo->feat_type[1213 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_CUTOUT";
    cinfo->feat_type[1214 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_STIFFENER";
    cinfo->feat_type[1215 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_BEAM";
    cinfo->feat_type[1216 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_ENDCUT";
    cinfo->feat_type[1217 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_WLD_FLANGE";
    cinfo->feat_type[1218 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_COLLAR";
    cinfo->feat_type[1219 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_DRAW";
    cinfo->feat_type[1220 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_BRACKET";
    cinfo->feat_type[1221 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_FOLDED_FLG";
    cinfo->feat_type[1222 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_BLOCK";
    cinfo->feat_type[1223 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_BLOCK_DEF";
    cinfo->feat_type[1105 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FR_SYS";
    cinfo->feat_type[1106 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_COMPT";
    cinfo->feat_type[1107 - FEAT_TYPE_OFFSET] = "PRO_FEAT_REFERENCE";
    cinfo->feat_type[1108 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SHELL_EXP";
    cinfo->feat_type[1109 - FEAT_TYPE_OFFSET] = "PRO_FEAT_FREEFORM";
    cinfo->feat_type[1110 - FEAT_TYPE_OFFSET] = "PRO_FEAT_KERNEL ";
    cinfo->feat_type[1111 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WELD_PROCESS";
    cinfo->feat_type[1112 - FEAT_TYPE_OFFSET] = "PRO_FEAT_HULL_REP_TMP";
    cinfo->feat_type[1113 - FEAT_TYPE_OFFSET] = "PRO_FEAT_INSULATION";
    cinfo->feat_type[1114 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SLD_PIP_INSUL";
    cinfo->feat_type[1115 - FEAT_TYPE_OFFSET] = "PRO_FEAT_SMT_EXTRACT";
    cinfo->feat_type[1116 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ASSY_MERGE";
    cinfo->feat_type[1117 - FEAT_TYPE_OFFSET] = "PRO_FEAT_DS_OPTIMIZE";
    cinfo->feat_type[1118 - FEAT_TYPE_OFFSET] = "PRO_FEAT_COMP_INTERFACE";
    cinfo->feat_type[1119 - FEAT_TYPE_OFFSET] = "PRO_FEAT_OLE";
    cinfo->feat_type[1120 - FEAT_TYPE_OFFSET] = "PRO_FEAT_TERMINATOR";
    cinfo->feat_type[1121 - FEAT_TYPE_OFFSET] = "PRO_FEAT_WLD_NOTCH";
    cinfo->feat_type[1122 - FEAT_TYPE_OFFSET] = "PRO_FEAT_ASSY_WLD_NOTCH";
}

extern "C" void
kill_empty_parts()
{
    struct empty_parts *ptr;

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "Adding code to remove empty parts:\n" );
    }

    ptr = empty_parts_root;
    while ( ptr ) {
	if ( logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( logger, "\t%s\n", ptr->name );
	}
	fprintf( outfp, "set combs [dbfind %s]\n", ptr->name );
	fprintf( outfp, "foreach comb $combs {\n\tcatch {rm $comb %s}\n}\n", ptr->name );
	ptr = ptr->next;
    }
}


extern "C" void
free_empty_parts()
{
    struct empty_parts *ptr, *prev;

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "Free empty parts list\n" );
    }

    ptr = empty_parts_root;
    while ( ptr ) {
	prev = ptr;
	ptr = ptr->next;
	bu_free( prev->name, "empty part node name" );
	bu_free( prev, "empty part node" );
    }

    empty_parts_root = NULL;

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "Free empty parts list done\n" );
    }
}

extern "C" void
free_hash_values( struct bu_hash_tbl *htbl )
{
    struct bu_hash_entry *entry;
    struct bu_hash_record rec;

    entry = bu_hash_tbl_first( htbl, &rec );

    while ( entry ) {
	bu_free( bu_get_hash_value( entry ), "hash entry" );
	entry = bu_hash_tbl_next( &rec );
    }
}


/* routine to output the top level object that is currently displayed in Pro/E */
extern "C" void
output_top_level_object( ProMdl model, ProMdlType type )
{
    ProName name;
    ProFileName msgfil;
    ProCharName top_level;
    ProCharLine astr;
    char buffer[1024] = {0};

    ProStringToWstring(msgfil, CREO_BRL_MSG_FILE);

    /* get its name */
    if ( ProMdlNameGet( model, name ) != PRO_TK_NO_ERROR ) {
	(void)ProMessageDisplay(msgfil, "USER_ERROR",
		"Could not get name for part!!" );
	ProMessageClear();
	fprintf( stderr, "Could not get name for part" );
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	bu_strlcpy( curr_part_name, "noname", PRO_NAME_SIZE );
    } else {
	(void)ProWstringToString( curr_part_name, name );
    }

    /* save name */
    bu_strlcpy( top_level, curr_part_name, sizeof(top_level) );

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "Output top level object (%s)\n", top_level );
    }

    /* output the object */
    if ( type == PRO_MDL_PART ) {
	/* tessellate part and output triangles */
	output_part( model );
    } else if ( type == PRO_MDL_ASSEMBLY ) {
	/* visit all members of assembly */
	output_assembly( model );
    } else {
	snprintf( astr, sizeof(astr), "Object %s is neither PART nor ASSEMBLY, skipping",
		curr_part_name );
	(void)ProMessageDisplay(msgfil, "USER_WARNING", astr );
	ProMessageClear();
	fprintf( stderr, "%s\n", astr );
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
    }

    if ( type == PRO_MDL_ASSEMBLY ) {
	snprintf(buffer, 1024, "put $topname comb region no tree {l %s.c {0 0 1 0 1 0 0 0 0 1 0 0 0 0 0 1}}", get_brlcad_name(top_level) );
    } else {
	snprintf(buffer, 1024, "put $topname comb region no tree {l %s {0 0 1 0 1 0 0 0 0 1 0 0 0 0 0 1}}", get_brlcad_name(top_level) );
    }

    /* make a top level combination named "top", if there is not
     * already one.  if one does already exist, try "top.#" where
     * "#" is the first available number.  this combination
     * contains the xform to rotate the model into BRL-CAD
     * standard orientation.
     */
    fprintf(outfp,
	    "set topname \"top\"\n"
	    "if { ! [catch {get $topname} ret] } {\n"
	    "  set num 0\n"
	    "  while { $num < 1000 } {\n"
	    "    set topname \"top.$num\"\n"
	    "    if { [catch {get $name} ret ] } {\n"
	    "      break\n"
	    "    }\n"
	    "    incr num\n"
	    "  }\n"
	    "}\n"
	    "if { [catch {get $topname} ret] } {\n"
	    "  %s\n"
	    "}\n",
	    buffer
	   );
}



extern "C" void
doit( char *dialog, char *compnent, ProAppData appdata )
{
    ProError status;
    ProMdl model;
    ProMdlType type;
    ProLine tmp_line;
    ProCharLine astr;
    wchar_t *w_output_file;
    wchar_t *w_name_file;
    wchar_t *tmp_str;
    char output_file[128];
    char name_file[128];
    char log_file[128];
    int n_selected_names;
    char **selected_names;
    char logger_type_str[128];
    int ret_status=0;
    struct creo_conv_info *cinfo = NULL;
    BU_GET(cinfo, struct creo_conv_info);

    creo_conv_info_init(cinfo);

    ProStringToWstring( tmp_line, "Not processing" );
    status = ProUILabelTextSet( "creo_brl", "curr_proc", tmp_line );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to update dialog label for currently processed part\n" );
    }
#if 0
    status = ProUIDialogActivate( "creo_brl", &ret_status );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Error in creo-brl Dialog, error = %d\n",
		status );
	fprintf( stderr, "\t dialog returned %d\n", ret_status );
    }
#endif
    /* get logger type */
    status = ProUIRadiogroupSelectednamesGet( "creo_brl", "log_file_type_rg", &n_selected_names, &selected_names );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get log file type\n" );
	ProUIDialogDestroy( "creo_brl" );
	return;
    }
    sprintf(logger_type_str,"%s", selected_names[0]);
    ProStringarrayFree(selected_names, n_selected_names);

    /* get the name of the log file */
    status = ProUIInputpanelValueGet( "creo_brl", "log_file", &tmp_str );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get log file name\n" );
	ProUIDialogDestroy( "creo_brl" );
	return;
    }
    ProWstringToString( cinfo->log_file, tmp_str );
    ProWstringFree( tmp_str );

    /* get the name of the output file */
    status = ProUIInputpanelValueGet( "creo_brl", "output_file", &w_output_file );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get output file name\n" );
	ProUIDialogDestroy( "creo_brl" );
	return;
    }
    ProWstringToString( cinfo->output_file, w_output_file );
    ProWstringFree( w_output_file );

    /* get the name of the part number to part name mapping file */
    status = ProUIInputpanelValueGet( "creo_brl", "name_file", &w_name_file );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get name of part number to part name mapping file\n" );
	ProUIDialogDestroy( "creo_brl" );
	return;
    }
    ProWstringToString( cinfo->name_file, w_name_file );
    ProWstringFree( w_name_file );

    /* get starting ident */
    status = ProUIInputpanelValueGet( "creo_brl", "starting_ident", &tmp_str );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get starting ident\n" );
	ProUIDialogDestroy( "creo_brl" );
	return;
    }

    ProWstringToString( astr, tmp_str );
    ProWstringFree( tmp_str );
    cinfo->reg_id = atoi( astr );
    V_MAX(reg_id, 1);

    /* get max error */
    status = ProUIInputpanelValueGet( "creo_brl", "max_error", &tmp_str );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get max tesellation error\n" );
	ProUIDialogDestroy( "creo_brl" );
	return;
    }

    ProWstringToString( astr, tmp_str );
    ProWstringFree( tmp_str );
    cinfo->max_error = atof( astr );

    /* get min error */
    status = ProUIInputpanelValueGet( "creo_brl", "min_error", &tmp_str );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get min tesellation error\n" );
	ProUIDialogDestroy( "creo_brl" );
	return;
    }

    ProWstringToString( astr, tmp_str );
    ProWstringFree( tmp_str );
    cinfo->min_error = atof( astr );

    V_MAX(max_error, min_error);

    /* get the max angle control */
    status = ProUIInputpanelValueGet( "creo_brl", "max_angle_ctrl", &tmp_str );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get angle control\n" );
	ProUIDialogDestroy( "creo_brl" );
	return;
    }

    ProWstringToString( astr, tmp_str );
    ProWstringFree( tmp_str );
    cinfo->max_angle_cntrl = atof( astr );

    /* get the min angle control */
    status = ProUIInputpanelValueGet( "creo_brl", "min_angle_ctrl", &tmp_str );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get angle control\n" );
	ProUIDialogDestroy( "creo_brl" );
	return;
    }

    ProWstringToString( astr, tmp_str );
    ProWstringFree( tmp_str );
    cinfo->min_angle_cntrl = atof( astr );

    V_MAX(cinfo->max_angle_cntrl, cinfo->min_angle_cntrl);

    /* get the max to min steps */
    status = ProUIInputpanelValueGet( "creo_brl", "isteps", &tmp_str );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get max to min steps\n" );
	ProUIDialogDestroy( "creo_brl" );
	return;
    }

    ProWstringToString( astr, tmp_str );
    ProWstringFree( tmp_str );
    cinfo->max_to_min_steps = atoi( astr );
    if (cinfo->max_to_min_steps <= 0) {
	cinfo->max_to_min_steps = 0;
	cinfo->error_increment = 0;
	cinfo->angle_increment = 0;
    } else {
	if (ZERO((cinfo->max_error - cinfo->min_error)))
	    cinfo->error_increment = 0;
	else
	    cinfo->error_increment = (cinfo->max_error - cinfo->min_error) / (double)cinfo->max_to_min_steps;

	if (ZERO((cinfo->max_angle_cntrl - cinfo->min_angle_cntrl)))
	    cinfo->angle_increment = 0;
	else
	    cinfo->angle_increment = (cinfo->max_angle_cntrl - cinfo->min_angle_cntrl) / (double)cinfo->max_to_min_steps;

	if (cinfo->error_increment == 0 && cinfo->angle_increment == 0)
	    cinfo->max_to_min_steps = 0;
    }

    /* check if user wants to do any CSG */
    status = ProUICheckbuttonGetState( "creo_brl", "facets_only", &cinfo->do_facets_only );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get checkbutton setting (facetize only)\n" );
	ProUIDialogDestroy( "creo_brl" );
	return;
    }

    /* check if user wants to eliminate small features */
    status = ProUICheckbuttonGetState( "creo_brl", "elim_small", &cinfo->do_elims );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get checkbutton setting (eliminate small features)\n" );
	ProUIDialogDestroy( "creo_brl" );
	return;
    }

    /* check if user wants surface normals in the BOT's */
    status = ProUICheckbuttonGetState( "creo_brl", "get_normals", &cinfo->get_normals );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "Failed to get checkbutton setting (extract surface normals)\n" );
	ProUIDialogDestroy( "creo_brl" );
	return;
    }

    if ( cinfo->do_elims ) {

	/* get the minimum hole diameter */
	status = ProUIInputpanelValueGet( "creo_brl", "min_hole", &tmp_str );
	if ( status != PRO_TK_NO_ERROR ) {
	    fprintf( stderr, "Failed to get minimum hole diameter\n" );
	    ProUIDialogDestroy( "creo_brl" );
	    return;
	}

	ProWstringToString( astr, tmp_str );
	ProWstringFree( tmp_str );
	cinfo->min_hole_diameter = atof( astr );
	V_MAX(cinfo->min_hole_diameter, 0.0);

	/* get the minimum chamfer dimension */
	status = ProUIInputpanelValueGet( "creo_brl", "min_chamfer", &tmp_str );
	if ( status != PRO_TK_NO_ERROR ) {
	    fprintf( stderr, "Failed to get minimum chamfer dimension\n" );
	    ProUIDialogDestroy( "creo_brl" );
	    return;
	}

	ProWstringToString( astr, tmp_str );
	ProWstringFree( tmp_str );
	cinfo->min_chamfer_dim = atof( astr );
	V_MAX(cinfo->min_chamfer_dim, 0.0);

	/* get the minimum round radius */
	status = ProUIInputpanelValueGet( "creo_brl", "min_round", &tmp_str );
	if ( status != PRO_TK_NO_ERROR ) {
	    fprintf( stderr, "Failed to get minimum round radius\n" );
	    ProUIDialogDestroy( "creo_brl" );
	    return;
	}

	ProWstringToString( astr, tmp_str );
	ProWstringFree( tmp_str );
	cinfo->min_round_radius = atof( astr );

	V_MAX(cinfo->min_round_radius, 0.0);

    } else {
	cinfo->min_hole_diameter = 0.0;
	cinfo->min_round_radius = 0.0;
	cinfo->min_chamfer_dim = 0.0;
    }

    /* open output file */
    if ( (outfp=fopen( output_file, "wb" ) ) == NULL ) {
	(void)ProMessageDisplay(msgfil, "USER_ERROR", "Cannot open output file" );
	ProMessageClear();
	fprintf( stderr, "Cannot open output file\n" );
	perror( "\t" );
	ProUIDialogDestroy( "creo_brl" );
	return;
    }

    /* open log file, if a name was provided */
    if ( strlen( log_file ) > 0 ) {
	if ( BU_STR_EQUAL( log_file, "stderr" ) ) {
	    logger = stderr;
	} else if ( (logger=fopen( log_file, "wb" ) ) == NULL ) {
	    (void)ProMessageDisplay(msgfil, "USER_ERROR", "Cannot open log file" );
	    ProMessageClear();
	    fprintf( stderr, "Cannot open log file\n" );
	    perror( "\t" );
	    ProUIDialogDestroy( "creo_brl" );
	    return;
	}

	/* Set logger type */
	if (BU_STR_EQUAL("Failure", logger_type_str))
	    cinfo->logger_type = LOGGER_TYPE_FAILURE;
	else if (BU_STR_EQUAL("Success", logger_type_str))
	    cinfo->logger_type = LOGGER_TYPE_SUCCESS;
	else if (BU_STR_EQUAL("Failure/Success", logger_type_str))
	    cinfo->logger_type = LOGGER_TYPE_FAILURE_OR_SUCCESS;
	else
	    cinfo->logger_type = LOGGER_TYPE_ALL;
    } else {
	cinfo->logger = (FILE *)NULL;
	cinfo->logger_type = LOGGER_TYPE_NONE;
    }

    /* open part name mapper file, if a name was provided */
    if ( strlen( name_file ) > 0 ) {
	FILE *name_fd;

	if ( cinfo->logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( cinfo->logger, "Opening part name map file (%s)\n", cinfo->name_file );
	}

	if ( (name_fd=fopen( cinfo->name_file, "rb" ) ) == NULL ) {
	    struct bu_vls error_msg = BU_VLS_INIT_ZERO;
	    int dialog_return=0;
	    wchar_t w_error_msg[512];

	    if ( cinfo->logger_type == LOGGER_TYPE_ALL ) {
		fprintf( cinfo->logger, "Failed to open part name map file (%s)\n", name_file );
		fprintf( cinfo->logger, "%s\n", strerror( errno ) );
	    }

	    (void)ProMessageDisplay(msgfil, "USER_ERROR", "Cannot open part name file" );
	    ProMessageClear();
	    fprintf( stderr, "Cannot open part name file\n" );
	    perror( cinfo->name_file );
	    status = ProUIDialogCreate( "creo_brl_gen_error", "creo_brl_gen_error" );
	    if ( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to create error dialog (%d)\n", status );
	    }
	    (void)ProUIPushbuttonActivateActionSet( "creo_brl_gen_error",
		    "ok_button",
		    kill_gen_error_dialog, NULL );
	    bu_vls_printf( &error_msg, "\n\tCannot open part name file (%s)\n\t",
		    cinfo->name_file );
	    bu_vls_strcat( &error_msg, strerror( errno ) );
	    ProStringToWstring( w_error_msg, bu_vls_addr( &error_msg ) );
	    status = ProUITextareaValueSet( "creo_brl_gen_error", "error_message", w_error_msg );
	    if ( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to set message for error dialog (%d)\n", status );
	    }
	    bu_vls_free( &error_msg );
	    status = ProUIDialogActivate( "creo_brl_gen_error", &dialog_return );
	    if ( status != PRO_TK_NO_ERROR ) {
		fprintf( stderr, "Failed to activate error dialog (%d)\n", status );
	    }
	    ProUIDialogDestroy( "creo_brl" );
	    return;
	}

	/* create a hash table of part numbers to part names */
	if ( cinfo->logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( logger, "Creating name hash\n" );
	}

	ProStringToWstring( tmp_line, "Processing part name file" );
	status = ProUILabelTextSet( "creo_brl", "curr_proc", tmp_line );
	if ( status != PRO_TK_NO_ERROR ) {
	    fprintf( stderr, "Failed to update dialog label for currently processed part\n" );
	}
	status = ProUIDialogActivate( "creo_brl", &ret_status );
	if ( status != PRO_TK_NO_ERROR ) {
	    fprintf( stderr, "Error in creo-brl Dialog, error = %d\n",
		    status );
	    fprintf( stderr, "\t dialog returned %d\n", ret_status );
	}

	cinfo->name_hash = create_name_hash( name_fd );
	fclose( name_fd );

    } else {
	if ( cinfo->logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( cinfo->logger, "No name hash used\n" );
	}
	/* create an empty hash table */
	cinfo->name_hash = bu_hash_create( 512 );
    }

    /* get the currently displayed model in Pro/E */
    status = ProMdlCurrentGet( &model );
    if ( status == PRO_TK_BAD_CONTEXT ) {
	(void)ProMessageDisplay(msgfil, "USER_NO_MODEL" );
	ProMessageClear();
	fprintf( stderr, "No model is displayed!!\n" );
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	ProUIDialogDestroy( "creo_brl" );
	if ( cinfo->name_hash ) {
	    free_hash_values( cinfo->name_hash );
	    bu_hash_tbl_free( cinfo->name_hash );
	    cinfo->name_hash = (struct bu_hash_tbl *)NULL;
	}
	return;
    }

    /* get its type */
    status = ProMdlTypeGet( model, &type );
    if ( status == PRO_TK_BAD_INPUTS ) {
	(void)ProMessageDisplay(msgfil, "USER_NO_TYPE" );
	ProMessageClear();
	fprintf( stderr, "Cannot get type of current model\n" );
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	ProUIDialogDestroy( "creo_brl" );
	if ( cinfo->name_hash ) {
	    free_hash_values( cinfo->name_hash );
	    bu_hash_tbl_free( cinfo->name_hash );
	    cinfo->name_hash = (struct bu_hash_tbl *)NULL;
	}
	return;
    }

    /* can only do parts and assemblies, no drawings, etc. */
    if ( type != PRO_MDL_ASSEMBLY && type != PRO_MDL_PART ) {
	(void)ProMessageDisplay(msgfil, "USER_TYPE_NOT_SOLID" );
	ProMessageClear();
	fprintf( stderr, "Current model is not a solid object\n" );
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	ProUIDialogDestroy( "creo_brl" );
	if ( cinfo->name_hash ) {
	    free_hash_values( cinfo->name_hash );
	    bu_hash_tbl_free( cinfo->name_hash );
	    cinfo->name_hash = (struct bu_hash_tbl *)NULL;
	}
	return;
    }

    ProUnitsystem us;
    ProUnititem lmu;
    ProUnititem mmu;
    ProUnitConversion conv;

    ProMdlPrincipalunitsystemGet(model, &us);
    ProUnitsystemUnitGet(&us, PRO_UNITTYPE_LENGTH, &lmu);
    ProUnitInit(model, L"mm", &mmu);
    ProUnitConversionGet(&lmu, &conv, &mmu);
    cinfo->creo_to_brl_conv = conv.scale;

    /* adjust tolerance for Pro/E units */
    cinfo->local_tol = tol_dist / creo_to_brl_conv;
    cinfo->local_tol_sq = local_tol * local_tol;

    cinfo->vert_tree_root = create_vert_tree();
    cinfo->norm_tree_root = create_vert_tree();

    /* output the top level object
     * this will recurse through the entire model
     */
    output_top_level_object( model, type );

    /* kill any references to empty parts */
    kill_empty_parts();

    /* let user know we are done */
    ProStringToWstring( tmp_line, "Conversion complete" );
    ProUILabelTextSet( "creo_brl", "curr_proc", tmp_line );

    /* free a bunch of stuff */
    if ( done_list_part.size() > 0 ) {
	std::set<wchar_t *, WStrCmp>::iterator d_it;
	for (d_it = done_list_part.begin(); d_it != done_list_part.end(); d_it++) {
	    bu_free(*d_it, "free wchar str copy");
	}
    }

    if ( done_list_asm.size() > 0 ) {
	std::set<wchar_t *, WStrCmp>::iterator d_it;
	for (d_it = done_list_asm.begin(); d_it != done_list_asm.end(); d_it++) {
	    bu_free(*d_it, "free wchar str copy");
	}
    }

    if ( part_tris ) {
	bu_free( (char *)part_tris, "part triangles" );
	part_tris = NULL;
    }

    if ( part_norms ) {
	bu_free( (char *)part_norms, "part normals" );
	part_norms = NULL;
    }

    free_vert_tree( vert_tree_root );
    vert_tree_root = NULL;
    free_vert_tree( norm_tree_root );
    norm_tree_root = NULL;

    max_tri = 0;

    free_empty_parts();

    if ( logger_type == LOGGER_TYPE_ALL ) {
	fprintf( logger, "Closing output file\n" );
    }

    fclose( outfp );

    if ( name_hash ) {
	if ( logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( logger, "freeing name hash\n" );
	}
	free_hash_values( name_hash );
	bu_hash_tbl_free( name_hash );
	name_hash = (struct bu_hash_tbl *)NULL;
    }
#if 0
    if ( brlcad_names.size() > 0 ) {
	std::set<struct bu_vls *, StrCmp>::iterator s_it;
	if ( logger_type == LOGGER_TYPE_ALL ) {
	    fprintf( logger, "freeing names\n" );
	}
	for (s_it = brlcad_names.begin(); s_it != brlcad_names.end(); s_it++) {
	    struct bu_vls *v = *s_it;
	    bu_vls_free(v);
	    BU_PUT(v, struct bu_vls);
	}
    }
#endif

    if ( logger_type != LOGGER_TYPE_NONE ) {
	if ( logger_type == LOGGER_TYPE_ALL )
	    fprintf( logger, "Closing logger file\n" );
	fclose( logger );
	logger = (FILE *)NULL;
	logger_type = LOGGER_TYPE_NONE;
    }

    return;
}



extern "C" void
elim_small_activate( char *dialog_name, char *button_name, ProAppData data )
{
    ProBoolean state;
    ProError status;

    status = ProUICheckbuttonGetState( dialog_name, button_name, &state );
    if ( status != PRO_TK_NO_ERROR ) {
	fprintf( stderr, "checkbutton activate routine: failed to get state\n" );
	return;
    }

    if ( state ) {
	status = ProUIInputpanelEditable( dialog_name, "min_hole" );
	if ( status != PRO_TK_NO_ERROR ) {
	    fprintf( stderr, "Failed to activate \"minimum hole diameter\"\n" );
	    return;
	}
	status = ProUIInputpanelEditable( dialog_name, "min_chamfer" );
	if ( status != PRO_TK_NO_ERROR ) {
	    fprintf( stderr, "Failed to activate \"minimum chamfer dimension\"\n" );
	    return;
	}
	status = ProUIInputpanelEditable( dialog_name, "min_round" );
	if ( status != PRO_TK_NO_ERROR ) {
	    fprintf( stderr, "Failed to activate \"minimum round radius\"\n" );
	    return;
	}
    } else {
	status = ProUIInputpanelReadOnly( dialog_name, "min_hole" );
	if ( status != PRO_TK_NO_ERROR ) {
	    fprintf( stderr, "Failed to de-activate \"minimum hole diameter\"\n" );
	    return;
	}
	status = ProUIInputpanelReadOnly( dialog_name, "min_chamfer" );
	if ( status != PRO_TK_NO_ERROR ) {
	    fprintf( stderr, "Failed to de-activate \"minimum chamfer dimension\"\n" );
	    return;
	}
	status = ProUIInputpanelReadOnly( dialog_name, "min_round" );
	if ( status != PRO_TK_NO_ERROR ) {
	    fprintf( stderr, "Failed to de-activate \"minimum round radius\"\n" );
	    return;
	}
    }
}

extern "C" void
do_quit( char *dialog, char *compnent, ProAppData appdata )
{
    ProUIDialogDestroy( "creo_brl" );
}

/* driver routine for converting CREO to BRL-CAD */
extern "C" int
creo_brl( uiCmdCmdId command, uiCmdValue *p_value, void *p_push_cmd_data )
{
    ProFileName msgfil;
    ProError status;
    int ret_status=0;

    ProStringToWstring(msgfil, CREO_BRL_MSG_FILE);

    ProMessageDisplay(msgfil, "USER_INFO", "Launching creo_brl...");

    /* use UI dialog */
    status = ProUIDialogCreate( "creo_brl", "creo_brl" );
    if ( status != PRO_TK_NO_ERROR ) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "Failed to create dialog box for creo-brl, error = %d\n", status );
	ProMessageDisplay(msgfil, "USER_INFO", bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return 0;
    }

    status = ProUICheckbuttonActivateActionSet( "creo_brl", "elim_small", elim_small_activate, NULL );
    if ( status != PRO_TK_NO_ERROR ) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "Failed to set action for \"eliminate small features\" checkbutton, error = %d\n", status );
	ProMessageDisplay(msgfil, "USER_INFO", bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return 0;
    }

    status = ProUIPushbuttonActivateActionSet( "creo_brl", "doit", doit, NULL );
    if ( status != PRO_TK_NO_ERROR ) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "Failed to set action for 'Go' button\n" );
	ProMessageDisplay(msgfil, "USER_INFO", bu_vls_addr(&vls));
	ProUIDialogDestroy( "creo_brl" );
	bu_vls_free(&vls);
	return 0;
    }

    status = ProUIPushbuttonActivateActionSet( "creo_brl", "quit", do_quit, NULL );
    if ( status != PRO_TK_NO_ERROR ) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "Failed to set action for 'Quit' button\n" );
	ProMessageDisplay(msgfil, "USER_INFO", bu_vls_addr(&vls));
	ProUIDialogDestroy( "creo_brl" );
	bu_vls_free(&vls);
	return 0;
    }

    status = ProUIDialogActivate( "creo_brl", &ret_status );
    if ( status != PRO_TK_NO_ERROR ) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "Error in creo-brl Dialog, error = %d\n",
		status );
	bu_vls_printf(&vls, "\t dialog returned %d\n", ret_status );
	ProMessageDisplay(msgfil, "USER_INFO", bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }

    return 0;
}



/* this routine determines whether the "creo-brl" menu item in Pro/E
 * should be displayed as available or greyed out
 */
extern "C" static uiCmdAccessState
creo_brl_access( uiCmdAccessMode access_mode )
{
    /* doing the correct checks appears to be unreliable */
    return ACCESS_AVAILABLE;
}

extern "C" int user_initialize()
{
    ProError status;
    ProCharLine astr;
    ProFileName msgfil;
    int i;
    uiCmdCmdId cmd_id;
    wchar_t errbuf[80];

    ProStringToWstring(msgfil, CREO_BRL_MSG_FILE);

    /* Pro/E says always check the size of w_char */
    status = ProWcharSizeVerify (sizeof (wchar_t), &i);
    if ( status != PRO_TK_NO_ERROR || (i != sizeof (wchar_t)) ) {
	sprintf(astr, "ERROR wchar_t Incorrect size (%d). Should be: %d",
		sizeof(wchar_t), i );
	status = ProMessageDisplay(msgfil, "USER_ERROR", astr);
	printf("%s\n", astr);
	ProStringToWstring(errbuf, astr);
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	return -1;
    }

    /* add a command that calls our creo-brl routine */
    status = ProCmdActionAdd( "CREO-BRL", (uiCmdCmdActFn)creo_brl, uiProe2ndImmediate,
	    creo_brl_access, PRO_B_FALSE, PRO_B_FALSE, &cmd_id );
    if ( status != PRO_TK_NO_ERROR ) {
	sprintf( astr, "Failed to add creo-brl action" );
	fprintf( stderr, "%s\n", astr);
	ProMessageDisplay(msgfil, "USER_ERROR", astr);
	ProStringToWstring(errbuf, astr);
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	return -1;
    }

    /* add a menu item that runs the new command */
    status = ProMenubarmenuPushbuttonAdd( "File", "CREO-BRL", "CREO-BRL", "CREO-BRL-HELP",
	    "File.psh_exit", PRO_B_FALSE, cmd_id, msgfil );
    if ( status != PRO_TK_NO_ERROR ) {
	sprintf( astr, "Failed to add creo-brl menu button" );
	fprintf( stderr, "%s\n", astr);
	ProMessageDisplay(msgfil, "USER_ERROR", astr);
	ProStringToWstring(errbuf, astr);
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	return -1;
    }

    ShowMsg();

    /* let user know we are here */
    ProMessageDisplay( msgfil, "OK" );
    (void)ProWindowRefresh( PRO_VALUE_UNUSED );

    return 0;
}

extern "C" void user_terminate()
{
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
