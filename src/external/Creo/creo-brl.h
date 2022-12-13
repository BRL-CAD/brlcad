/**
 *                   C R E O - B R L . H 
 * BRL-CAD
 *
 * Copyright (c) 2017-2022 United States Government as represented by
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
/**
 * @file creo-brl.h
 */

#ifndef CREO_BRL_H
#define CREO_BRL_H

#include "common.h"

#include <set>
#include <map>
#include <vector>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>

#ifndef _WSTUDIO_DEFINED
# define _WSTUDIO_DEFINED
#endif

#ifndef TEST_BUILD

extern "C" {
#include <ProToolkit.h>
#include <ProArray.h>
#include <ProAsmcomp.h>
#include <ProAsmcomppath.h>
#include <ProAssembly.h>
#include <ProElement.h>
#include <ProFaminstance.h>
#include <ProFeatType.h>
#include <ProFeature.h>
#include <ProHole.h>
#include <ProMaterial.h>
#include <ProMdl.h>
#include <ProMdlUnits.h>
#include <ProMenuBar.h>
#include <ProMessage.h>
#include <ProMode.h>
#include <ProNotify.h>
#include <ProParameter.h>
#include <ProPart.h>
#include <ProSkeleton.h>
#include <ProSolid.h>
#include <ProSolidBody.h>
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
}

#else
extern "C" {
#include "shim.h"
}
#endif

extern "C" {
#include "vmath.h"
#include "bu.h"
#include "bn.h"
#include "bg.h"
#include "wdb.h"
#include "raytrace.h"
}

#define CREO_BRL_MSG_FILE "creo-brl-msg.txt"

#define LOGGER_TYPE_NONE                     -1
#define LOGGER_TYPE_FAILURE                   0
#define LOGGER_TYPE_SUCCESS                   1
#define LOGGER_TYPE_FAILURE_OR_SUCCESS        2
#define LOGGER_TYPE_ALL                       3

#define XFORM_NONE                            0
#define XFORM_X_TO_Z                          1
#define XFORM_Y_TO_Z                          2

#define FEAT_ID_BLOCK                        64  /* number of slots to allocate in above list */
#define NUM_HASH_TABLE_BINS                4096  /* number of bins for part number to part name hash table */
#define TRI_BLOCK                           512  /* number of triangles to malloc per call */
#define DONE_BLOCK                          512  /* number of slots to malloc when above array gets full */
#define NUM_OBJ_TYPES                       629
#define NUM_FEAT_TYPES                      314
#define FEAT_TYPE_OFFSET                    910

#define CREO_NAME_MAX                     240*2  /* max part name length Creo supports is 240 chars */
#define CREO_MSG_MAX                       4096  /* max message and/or response length */

#define MAX_MATL_NAME                        32  /* maximum allowed material name length  */
#define MAX_LINE_SIZE                        80  /* maximum allowed input line length     */
#define MAX_FILE_RECS                       256  /* maximum allowed material record count */

#define MAX_UNIQUE_NAMES                  65535  /* maximum unique name generation count */

#define MSG_FAIL                              0
#define MSG_SUCCESS                           1
#define MSG_DEBUG                             2
#define MSG_STATUS                            3  /* Output for Creo msg window only */
#define MSG_PLAIN                             4  

#define PRO_FEAT_DELETE_NO_OPTS               0  /* Feature delete options         */
#define PRO_FEAT_DELETE_CLIP                  1  /* Delete with children           */
#define PRO_FEAT_DELETE_FIX                   2  /* Delete without children        */
#define PRO_FEAT_DELETE_RELATION_DELETE       3  /* Delete with obsolete relations */
#define PRO_FEAT_DELETE_RELATION_COMMENT      4  /* Delete with relations comments */
#define PRO_FEAT_DELETE_CLIP_ALL              5  /* Delete with all included       */
#define PRO_FEAT_DELETE_INDIV_GP_MEMBERS      6  /* Delete within groups           */
#define PRO_FEAT_DELETE_CLIP_INDIV_GP_MEMBERS 7  /* Delete with group              */
#define PRO_FEAT_DELETE_KEEP_EMBED_DATUMS     8  /* Delete with datums             */

#define PRO_REGEN_NO_FLAGS                    0  /* Regeneration control flags     */
#define PRO_REGEN_CAN_FIX                     1  /* Allow UI in FixModel           */
#define PRO_REGEN_SKIP_DISALLOW_SYS_RECOVER   2  /*                                */
#define PRO_REGEN_UPDATE_INSTS                4  /* Update instances in memory.    */
#define PRO_REGEN_FORCE_REGEN                16  /* Force full regeneration        */
#define PRO_REGEN_UPDATE_ASSEMBLY_ONLY       32  /* Update assembly and sub-asm    */
#define PRO_REGEN_RESUME_EXCL_COMPS          64  /* Regenerate exclude components  */
#define PRO_REGEN_NO_RESOLVE_MODE           128  /* Regenerate in no resolve mode  */
#define PRO_REGEN_RESOLVE_MODE              256  /* Regenerate in resolve mode     */
#define PRO_REGEN_UNDO_IF_FAIL             1024  /* Undo failure, if possible      */

#define PRO_FEAT_RESUME_NO_OPTS               0  /* Feature resume options         */
#define PRO_FEAT_RESUME_INCLUDE_PARENTS       1  /* Resume with parents            */
#define PRO_FEAT_RESUME_ADD_TO_BUFFER         2  /* Resume with selection buffer   */

#define dmod(a,b) ((a) - floor(double((a))/double((b)))*(b))

struct StrCmp {
    bool operator()(struct bu_vls *str1, struct bu_vls *str2) const {
        return (bu_strcmp(bu_vls_addr(str1), bu_vls_addr(str2)) < 0);
    }
};

struct CharCmp {
    bool operator()(char *str1, char *str2) const {
        return (bu_strcmp(str1, str2) < 0);
    }
};


struct WStrCmp {
    bool operator()(wchar_t *str1, wchar_t *str2) const {
        return (wcscmp(str1, str2) < 0);
    }
};


/* structure to hold info about CSG operations for current part */
struct csg_ops {
    char op;
    struct bu_vls name;
    struct bu_vls dbput;
    struct csg_ops *next;
};

struct empty_parts {
    char *name;
    struct empty_parts *next;
};

struct creo_conv_info {

    struct bu_vls *out_fname;               /* output file name  */
    struct bu_vls *logger_str;              /* log file criteria */
    struct bu_vls *log_fname;               /* log file name     */

    FILE *fplog;                            /* log file settings */
    int   logger_type;
    int   curr_msg_type;

    FILE *fpmtl;                            /* material file data */
    char mtl_fname[MAXPATHLEN];
    char mtl_key[MAX_MATL_NAME];
    char mtl_str[MAX_FILE_RECS][MAX_LINE_SIZE + 1];
    int  mtl_id[MAX_FILE_RECS];
    int  mtl_los[MAX_FILE_RECS];
    int  mtl_ptr;
    int  mtl_rec;

    int  xform_mode;                        /* coordinate transformation mode */
    long int reg_id;                        /* region ident number (incremented with each part) */
    int  lmin;                              /* user-established minimum luminance threshold */

    /* units - model */
    double creo_to_brl_conv;                /* inches to mm */
    double local_tol;                       /* tolerance in Creo units */
    double local_tol_sq;                    /* tolerance squared */

    /* Facetization settings */
    ProBool do_facets_only;                 /* flag to indicate no CSG should be done */
    ProBool check_solidity;                 /* flag to control testing BoTs for solidity */
    ProBool get_normals;                    /* flag to indicate surface normals should be extracted from geometry */
    ProBool do_elims;                       /* flag to indicate that small features are to be eliminated */
    ProBool debug_bboxes;                   /* flag to indicate that bboxes should be written for parts that didn't convert */

    double max_error;                       /* maximum allowable error in facetized approximation, mm */
    double min_error;                       /* minimum allowable error in facetized approximation, mm */
    double tol_dist;                        /* minimum distance between two distinct vertices, mm */
    double max_angle_cntrl;                 /* max angle control for tessellation ( 0.0 - 1.0 ), deg */
    double min_angle_cntrl;                 /* min angle control for tessellation ( 0.0 - 1.0 ), deg */
    long int max_to_min_steps;              /* number of steps between max and min */
    double error_increment;
    double angle_increment;

    /* CSG settings */
    double min_hole_diameter;               /* if > 0.0, all holes features smaller than this will be deleted */
    double min_chamfer_dim;                 /* if > 0.0, all chamfers with both dimensions less */
                                            /* than this value will be deleted */
    double min_round_radius;                /* if > 0.0, all rounds with radius less than this */
                                            /* value will be deleted */

    /* Bounding box results */
    double bbox_vol;                        /* bounding box volume,        [L^3] */
    double bbox_area;                       /* bounding box surface area,  [L^2] */

    /* Tessellation results */
    double tess_chord;                      /* chord error, mm  */
    double tess_angle;                      /* angle error, deg */

    /* Conversion Process results */
    int asm_count;                          /* number of assemblies processed */
    int asm_total;                          /* number of assemblies found     */
    int prt_count;                          /* number of assemblies processed */
    int prt_total;                          /* number of parts found          */

    /* ------ Internal ------ */
    struct db_i   *dbip;                                             /* output database */
    struct rt_wdb *wdbp;
    std::set<wchar_t *, WStrCmp> *parts;                             /* list of all parts in Creo hierarchy */
    std::set<wchar_t *, WStrCmp> *assems;                            /* list of all assemblies in Creo hierarchy */
    std::set<wchar_t *, WStrCmp> *empty;                             /* list of all parts and assemblies in Creo that have no shape */
    std::map<wchar_t *, struct bu_vls *, WStrCmp> *region_name_map;  /* Creo names to BRL-CAD region names */
    std::map<wchar_t *, struct bu_vls *, WStrCmp> *assem_name_map;   /* Creo names to BRL-CAD assembly names */
    std::map<wchar_t *, struct bu_vls *, WStrCmp> *solid_name_map;   /* Creo names to BRL-CAD solid names */
    std::map<wchar_t *, struct bu_vls *, WStrCmp> *creo_name_map;    /* wchar Creo names to char versions */
    std::set<struct bu_vls *, StrCmp> *brlcad_names;                 /* set of active .g object names */
    std::set<struct bu_vls *, StrCmp> *creo_names;                   /* set of active creo id strings */
    std::vector<char *> *model_parameters;                           /* model parameters to use when generating .g names */
    std::vector<char *> *attrs;                                      /* attributes to preserve when transferring objects */
    int warn_feature_unsuppress;                                     /* flag to determine if we need to warn the user feature unsuppression failed */
};

/* Part processing container */
struct part_conv_info {
    struct creo_conv_info *cinfo;                                    /* global state */
    int csg_holes_supported;
    ProMdl model;
    std::vector<int> *suppressed_features;                           /* list of features to suppress when generating output. */
    std::vector<struct directory *> *subtractions;                   /* objects to subtract from primary shape. */

    /* generic feature suppression processing parameters */
    ProFeature *feat;
    ProFeattype type;
    double radius;
    double diameter;
    double distance1;
    double distance2;
    int got_diameter;
    int got_distance1;
};

/* Generic container used when we need to pass around something in addition to creo_conv_info */
struct adata {
    struct creo_conv_info *cinfo;
    void *data;
};

/* assembly */
extern "C" void find_empty_assemblies(struct creo_conv_info *);
extern "C" ProError output_assembly(struct creo_conv_info *, ProMdl model);
extern "C" ProError assembly_entry_matrix(struct creo_conv_info *, ProMdl parent, ProFeature *, mat_t *);

/* part */
extern "C" ProError output_part(struct creo_conv_info *, ProMdl model);

/* util */
extern "C" ProError component_filter(ProFeature *, ProAppData *);
extern "C" ProError creo_attribute_val(char **val, const char *key, ProMdl m);
extern "C" void creo_log(struct creo_conv_info *, int, const char *, ...);
extern "C" ProError creo_model_units(double *,ProMdl);
extern "C" char * creo_param_name(struct creo_conv_info *, wchar_t *, int);
extern "C" int find_matl(struct creo_conv_info *);
/*     "C" struct bu_vls * get_brlcad_name  (see comments below) */
extern "C" int get_mtl_input(FILE *, char *, int *, int *);
extern "C" void lower_case( char *);
extern "C" ProError PopupMsg(const char *, const char *);
extern "C" ProError regex_key(ProParameter *, ProError , ProAppData );
extern "C" int rgb4lmin(double *, int);
extern "C" wchar_t* stable_wchar(struct creo_conv_info *, wchar_t *);
extern "C" void trim(char *);
extern "C" double wstr_to_double(struct creo_conv_info *, wchar_t *);
extern "C" long int wstr_to_long(struct creo_conv_info *, wchar_t *);


/*
 * This function is highly important - it is responsible for all name
 * generation, translation and clean-up in the converter. A Creo name may map to
 * up to three distinct objects in BRL-CAD - a solid, a region, and an assembly.
 * Any may be given a unique human readable name using parameters on the Creo
 * object. There is also the char mapping of the wchar_t Creo name, which may or
 * may not correspond to any of the object names.
 */
#define N_REGION 1  /* Default - generate a unique, human readable name and append the specified suffix */
#define N_ASSEM  2  /* Default - generate a unique, human readable name and append the specified suffix */
#define N_SOLID  3  /* Default - generate a unique, human readable name and append the specified suffix */
#define N_CREO   4  /* return char mapping of Creo name */
extern "C" struct bu_vls *get_brlcad_name(struct creo_conv_info *cinfo, wchar_t *cname, const char *suffix, int flag);

/* CSG */
extern "C" int subtract_hole(struct part_conv_info *pinfo);



#endif /*CREO_BRL_H*/

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
