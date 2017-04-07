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
#include <ProParameter.h>
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
#include "wdb.h"
#include "raytrace.h"
}


#define CREO_BRL_MSG_FILE "creo-brl-msg.txt"

#define LOGGER_TYPE_NONE -1
#define LOGGER_TYPE_FAILURE 0
#define LOGGER_TYPE_SUCCESS 1
#define LOGGER_TYPE_FAILURE_OR_SUCCESS 2
#define LOGGER_TYPE_ALL 3
#define FEAT_ID_BLOCK	64		/* number of slots to allocate in above list */
#define NUM_HASH_TABLE_BINS	4096	/* number of bins for part number to part name hash table */
#define TRI_BLOCK 512			/* number of triangles to malloc per call */
#define DONE_BLOCK 512			/* number of slots to malloc when above array gets full */
#define MAX_LINE_LEN		256	/* maximum allowed line length for part number to name map file */
#define CREO_NAME_ATTR "CREO_Name"
#define NUM_OBJ_TYPES 629
#define NUM_FEAT_TYPES 314
#define FEAT_TYPE_OFFSET 910

#define CREO_NAME_MAX 100000
#define CREO_MSG_MAX 100000

#define MSG_FAIL 0
#define MSG_OK 1
#define MSG_DEBUG 2

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
    /* Region ID */
    long int reg_id;	/* region ident number (incremented with each part) */

    /* File settings */
    FILE *logger;			/* log file */
    int logger_type;
    int curr_msg_type;
    int print_to_console;

    /* units - model */
    double creo_to_brl_conv;	/* inches to mm */
    double local_tol;	/* tolerance in Pro/E units */
    double local_tol_sq;	/* tolerance squared */

    /* facetization settings */
    ProBool do_facets_only;	/* flag to indicate no CSG should be done */
    ProBool get_normals;	/* flag to indicate surface normals should be extracted from geometry */
    ProBool do_elims;		/* flag to indicate that small features are to be eliminated */
    double max_error;	/* (mm) maximum allowable error in facetized approximation */
    double min_error;	/* (mm) maximum allowable error in facetized approximation */
    double tol_dist;	/* (mm) minimum distance between two distinct vertices */
    double max_angle_cntrl;	/* max angle control for tessellation ( 0.0 - 1.0 ) */
    double min_angle_cntrl;	/* min angle control for tessellation ( 0.0 - 1.0 ) */
    long int max_to_min_steps;	/* number of steps between max and min */
    double error_increment;
    double angle_increment;

    /* csg settings */
    double min_hole_diameter; /* if > 0.0, all holes features smaller than this will be deleted */
    double min_chamfer_dim;   /* if > 0.0, all chamfers with both dimensions less
         			  * than this value will be deleted */
    double min_round_radius;  /* if > 0.0, all rounds with radius less than this
         			  * value will be deleted */

    /* ------ Internal ------ */
    struct db_i *dbip;		/* output database */
    struct rt_wdb *wdbp;
    /* Note - these next two are a temporary cheat in that they're not global,
     * but part of local processing - properly handled, these should be wrapped
     * into a container struct with creo_conv_info at the local level...*/
    ProMdl curr_parent;
    struct wmember *wcmb;

    std::set<wchar_t *, WStrCmp> *parts;	/* list of all parts in CREO hierarchy */
    std::set<wchar_t *, WStrCmp> *assems;	/* list of all assemblies in CREO hierarchy */
    std::map<wchar_t *, int, WStrCmp> *assem_child_cnts; /* number of solid children in a given assembly */
    std::set<wchar_t *, WStrCmp> *empty;	/* list of all parts and assemblies in CREO that have no shape */
    std::map<wchar_t *, struct bu_vls *, WStrCmp> *name_map;  /* CREO names to BRL-CAD names */
    std::set<struct bu_vls *, StrCmp> *brlcad_names; /* set of active .g object names */
    std::vector<char *> *model_parameters;     /* model parameters to use when generating .g names */
    std::vector<char *> *attrs;     	/* attributes to preserve when transferring objects */
};

/* Generic container used when we need to pass around something in addition to creo_conv_info */
struct adata {
    struct creo_conv_info *cinfo;
    void *data;
};

extern "C" void kill_error_dialog(char *dialog, char *component, ProAppData appdata);
extern "C" void kill_gen_error_dialog(char *dialog, char *component, ProAppData appdata);

extern "C" void do_quit(char *dialog, char *compnent, ProAppData appdata);
extern "C" void free_csg_ops(struct creo_conv_info *);

extern "C" ProError do_feature_visit(ProFeature *feat, ProError status, ProAppData data);
extern "C" ProError creo_attribute_val(const char **val, const char *key, ProMdl m);
extern "C" char *get_brlcad_name(struct creo_conv_info *cinfo, char *part_name);
extern "C" void find_empty_assemblies(struct creo_conv_info *);
extern "C" void output_assembly(struct creo_conv_info *, ProMdl model);
extern "C" int output_part(struct creo_conv_info *, ProMdl model);
extern "C" ProError component_filter(ProFeature *, ProAppData *);

extern "C" ProError creo_log(struct creo_conv_info *, int, ProError, const char *, ...);

extern "C" long int wstr_to_long(struct creo_conv_info *, wchar_t *);
extern "C" double wstr_to_double(struct creo_conv_info *, wchar_t *);

extern "C" ProError ShowMsg();

#endif /*CREO_BRL_H*/

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
