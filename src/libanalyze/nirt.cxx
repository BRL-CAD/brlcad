/*                        N I R T . C X X
 * BRL-CAD
 *
 * Copyright (c) 2004-2018 United States Government as represented by
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
/** @file nirt.cxx
 *
 * Implementation of Natalie's Interactive Ray-Tracer (NIRT)
 * functionality.
 *
 */

/* BRL-CAD includes */
#include "common.h"

#include <string>
#include <sstream>
#include <set>
#include <vector>

extern "C" {
#include "bu/color.h"
#include "bu/cmd.h"
#include "bu/malloc.h"
#include "bu/units.h"
#include "bu/vls.h"
#include "analyze.h"
}

/* NIRT segment types */
#define NIRT_HIT_SEG      1    /**< @brief Ray segment representing a solid region */
#define NIRT_MISS_SEG     2    /**< @brief Ray segment representing a gap */
#define NIRT_AIR_SEG      3    /**< @brief Ray segment representing an air region */
#define NIRT_OVERLAP_SEG  4    /**< @brief Ray segment representing an overlap region */

#define SILENT_UNSET    0
#define SILENT_YES      1
#define SILENT_NO       -1

#define HORZ            0
#define VERT            1
#define DIST            2

#define DEBUG_INTERACT  0x001
#define DEBUG_SCRIPTS   0x002
#define DEBUG_MAT       0x004
#define DEBUG_BACKOUT   0x008
#define DEBUG_HITS      0x010
#define DEBUG_FMT        "\020\5HITS\4BACKOUT\3MAT\2SCRIPTS\1INTERACT"

#define OVLP_RESOLVE            0
#define OVLP_REBUILD_FASTGEN    1
#define OVLP_REBUILD_ALL        2
#define OVLP_RETAIN             3

#define NIRT_PRINTF_SPECIFIERS "difeEgGs"

static const char *ovals =
"x_orig,         FLOAT,         Ray origin X coordinate,"
"y_orig,         FLOAT,         Ray origin Y coordinate,"
"z_orig,         FLOAT,         Ray origin Z coordinate,"
"h,              FLOAT,         ,"
"v,              FLOAT,         ,"
"d_orig,         FLOAT,         ,"
"x_dir,          FNOUNIT,       Ray direction unit vector x component,"
"y_dir,          FNOUNIT,       Ray direction unit vector y component,"
"z_dir,          FNOUNIT,       Ray direction unit vector z component,"
"a,              FNOUNIT,       Azimuth,"
"e,              FNOUNIT,       Elevation,"
"x_in,           FLOAT,         ,"
"y_in,           FLOAT,         ,"
"z_in,           FLOAT,         ,"
"d_in,           FLOAT,         ,"
"x_out,          FLOAT,         ,"
"y_out,          FLOAT,         ,"
"z_out,          FLOAT,         ,"
"d_out,          FLOAT,         ,"
"los,            FLOAT,         ,"
"scaled_los,     FLOAT,         ,"
"path_name,      STRING,        ,"
"reg_name,       STRING,        ,"
"reg_id,         INT,           ,"
"obliq_in,       FNOUNIT,       ,"
"obliq_out,      FNOUNIT,       ,"
"nm_x_in,        FNOUNIT,       ,"
"nm_y_in,        FNOUNIT,       ,"
"nm_z_in,        FNOUNIT,       ,"
"nm_d_in,        FNOUNIT,       ,"
"nm_h_in,        FNOUNIT,       ,"
"nm_v_in,        FNOUNIT,       ,"
"nm_x_out,       FNOUNIT,       ,"
"nm_y_out,       FNOUNIT,       ,"
"nm_z_out,       FNOUNIT,       ,"
"nm_d_out,       FNOUNIT,       ,"
"nm_h_out,       FNOUNIT,       ,"
"nm_v_out,       FNOUNIT,       ,"
"ov_reg1_name,   STRING,        ,"
"ov_reg1_id,     INT,           ,"
"ov_reg2_name,   STRING,        ,"
"ov_reg2_id,     INT,           ,"
"ov_sol_in,      STRING,        ,"
"ov_sol_out,     STRING,        ,"
"ov_los,         FLOAT,         ,"
"ov_x_in,        FLOAT,         ,"
"ov_y_in,        FLOAT,         ,"
"ov_z_in,        FLOAT,         ,"
"ov_d_in,        FLOAT,         ,"
"ov_x_out,       FLOAT,         ,"
"ov_y_out,       FLOAT,         ,"
"ov_z_out,       FLOAT,         ,"
"ov_d_out,       FLOAT,         ,"
"surf_num_in,    INT,           ,"
"surf_num_out,   INT,           ,"
"claimant_count, INT,           ,"
"claimant_list,  STRING,        ,"
"claimant_listn, STRING,        ,"
"attributes,     STRING,        ,"
"x_gap_in,       FLOAT,         ,"
"y_gap_in,       FLOAT,         ,"
"z_gap_in,       FLOAT,         ,"
"gap_los,        FLOAT,         ,";

/* This record structure doesn't have to correspond exactly to the above list
 * of available values, but it needs to retain sufficient information to
 * support the ability to generate all of them upon request. */
struct nout_record {
    point_t orig;
    fastf_t h;
    fastf_t v;
    fastf_t d_orig;
    vect_t dir;
    fastf_t a;
    fastf_t e;
    point_t in;
    fastf_t d_in;
    point_t out;
    fastf_t d_out;
    fastf_t los;
    fastf_t scaled_los;
    struct bu_vls path_name;
    struct bu_vls reg_name;
    int reg_id;
    fastf_t obliq_in;
    fastf_t obliq_out;
    vect_t nm_in;
    fastf_t nm_d_in;
    fastf_t nm_h_in;
    fastf_t nm_v_in;
    vect_t nm_out;
    fastf_t nm_d_out;
    fastf_t nm_h_out;
    fastf_t nm_v_out;
    struct bu_vls ov_reg1_name;
    int ov_reg1_id;
    struct bu_vls ov_reg2_name;
    int ov_reg2_id;
    struct bu_vls ov_sol_in;
    struct bu_vls ov_sol_out;
    fastf_t ov_los;
    point_t ov_in;
    fastf_t ov_d_in;
    point_t ov_out;
    fastf_t ov_d_out;
    int surf_num_in;
    int surf_num_out;
    int claimant_count;
    struct bu_vls claimant_list;
    struct bu_vls claimant_listn; /* ?? */
    struct bu_vls attributes;
    point_t gap_in;
    fastf_t gap_los;
};

struct overlap {
    struct application *ap;
    struct partition *pp;
    struct region *reg1;
    struct region *reg2;
    fastf_t in_dist;
    fastf_t out_dist;
    point_t in_point;
    point_t out_point;
    struct ovlp_tag *forw;
    struct ovlp_tag *backw;
};
#define OVERLAP_NULL ((struct overlap *)0)



struct nirt_state {
    /* Output options */
    struct bu_color *hit_color;
    struct bu_color *miss_color;
    struct bu_color *overlap_color;
    int print_header;
    int print_ident_flag;
    int silent_flag;
    unsigned int rt_debug;
    unsigned int nirt_debug;

    /* Output */
    struct bu_vls *out;
    int out_accumulate;
    struct bu_vls *err;
    int err_accumulate;
    struct bu_list s_vlist; /* used by the segs vlblock */
    struct bn_vlblock *segs;
    //TODO - int segs_accumulate;
    int ret;  // return code to be returned by nirt_exec after execution

    /* Callbacks */
    nirt_hook_t h_state;   // any state change
    nirt_hook_t h_out;     // output changes
    nirt_hook_t h_err;     // err changes
    nirt_hook_t h_segs;    // segement list is changed
    nirt_hook_t h_objs;    // active list of objects in scene changes
    nirt_hook_t h_frmts;   // output formatting is changed
    nirt_hook_t h_view;    // the camera view is changed


    /* state variables */
    double azimuth;
    double base2local;
    double elevation;
    double local2base;
    int backout;
    int overlap_claims;
    int use_air;
    vect_t direct;
    vect_t grid;
    vect_t target;
    std::set<std::string> attrs;        // active attributes
    std::set<std::string> active_paths; // active paths for raytracer

    /* state alteration flags */
    bool b_state;   // updated for any state change
    bool b_out;     // updated when output changes
    bool b_err;     // updated when err changes
    bool b_segs;    // updated when segement list is changed
    bool b_objs;    // updated when active list of objects in scene changes
    bool b_frmts;   // updated when output formatting is changed
    bool b_view;    // updated when the camera view is changed

    /* internal containers for raytracing */
    struct application *ap;
    struct db_i *dbip;
    /* Note: Parallel structures are needed for operation w/ and w/o air */
    struct rt_i *rtip;
    struct resource *res;
    struct rt_i *rtip_air;
    struct resource *res_air;

    /* internal format specifier arrays */
    struct bu_attribute_value_set *val_types;
    struct bu_attribute_value_set *val_docs;
    std::vector<std::pair<std::string,std::string> > fmt_ray;
    std::vector<std::pair<std::string,std::string> > fmt_head;
    std::vector<std::pair<std::string,std::string> > fmt_part;
    std::vector<std::pair<std::string,std::string> > fmt_foot;
    std::vector<std::pair<std::string,std::string> > fmt_miss;
    std::vector<std::pair<std::string,std::string> > fmt_ovlp;
    std::vector<std::pair<std::string,std::string> > fmt_gap;

    void *u_data; // user data
};

/**************************
 * Internal functionality *
 **************************/

void lout(struct nirt_state *nss, const char *fmt, ...) _BU_ATTR_PRINTF23;
void lout(struct nirt_state *nss, const char *fmt, ...)
{
    va_list ap;
    if (nss->silent_flag != SILENT_YES) {
	struct bu_vls *vls = nss->out;
	BU_CK_VLS(vls);
	va_start(ap, fmt);
	bu_vls_vprintf(vls, fmt, ap);
	nss->b_state = nss->b_out = true;
    }
    va_end(ap);
}

void lerr(struct nirt_state *nss, const char *fmt, ...) _BU_ATTR_PRINTF23;
void lerr(struct nirt_state *nss, const char *fmt, ...)
{
    va_list ap;
    struct bu_vls *vls = nss->err;
    BU_CK_VLS(vls);
    va_start(ap, fmt);
    bu_vls_vprintf(vls, fmt, ap);
    nss->b_state = nss->b_err = true;
    va_end(ap);
}

void ldbg(struct nirt_state *nss, int flag, const char *fmt, ...)
{
    va_list ap;
    if (nss->silent_flag != SILENT_YES && (nss->nirt_debug & flag)) {
	struct bu_vls *vls = nss->out;
	BU_CK_VLS(vls);
	va_start(ap, fmt);
	bu_vls_vprintf(vls, fmt, ap);
	nss->b_state = nss->b_out = true;
    }
    va_end(ap);
}


/********************************
 * Conversions and Calculations *
 ********************************/

void grid2targ(struct nirt_state *nss)
{
    vect_t grid;
    double ar = nss->azimuth * DEG2RAD;
    double er = nss->elevation * DEG2RAD;
    VMOVE(grid, nss->grid);
    nss->target[X] = - grid[HORZ] * sin(ar) - grid[VERT] * cos(ar) * sin(er) + grid[DIST] * cos(ar) * cos(er);
    nss->target[Y] =   grid[HORZ] * cos(ar) - grid[VERT] * sin(ar) * sin(er) + grid[DIST] * sin(ar) * cos(er);
    nss->target[Z] =   grid[VERT] * cos(er) + grid[DIST] * sin(er);
}

void targ2grid(struct nirt_state *nss)
{
    vect_t target;
    double ar = nss->azimuth * DEG2RAD;
    double er = nss->elevation * DEG2RAD;
    VMOVE(target, nss->target);
    nss->grid[HORZ] = - target[X] * sin(ar) + target[Y] * cos(ar);
    nss->grid[VERT] = - target[X] * cos(ar) * sin(er) - target[Y] * sin(er) * sin(ar) + target[Z] * cos(er);
    nss->grid[DIST] =   target[X] * cos(er) * cos(ar) + target[Y] * cos(er) * sin(ar) + target[Z] * sin(er);
}

void dir2ae(struct nirt_state *nss)
{
    int zeroes = ZERO(nss->direct[Y]) && ZERO(nss->direct[X]);
    double square = sqrt(nss->direct[X] * nss->direct[X] + nss->direct[Y] * nss->direct[Y]);

    nss->azimuth = zeroes ? 0.0 : atan2 (-(nss->direct[Y]), -(nss->direct[X])) / DEG2RAD;
    nss->elevation = atan2(-(nss->direct[Z]), square) / DEG2RAD;
}

void ae2dir(struct nirt_state *nss)
{
    vect_t dir;
    double ar = nss->azimuth * DEG2RAD;
    double er = nss->elevation * DEG2RAD;

    dir[X] = -cos(ar) * cos(er);
    dir[Y] = -sin(ar) * cos(er);
    dir[Z] = -sin(er);
    VUNITIZE(dir);
    VMOVE(nss->direct, dir);
}

double backout(struct nirt_state *nss)
{
    double bov;
    point_t ray_point;
    vect_t diag, dvec, ray_dir, center_bsphere;
    fastf_t bsphere_diameter, dist_to_target, delta;

    if (!nss || !nss->backout) return 0.0;

    VMOVE(ray_point, nss->target);
    VMOVE(ray_dir, nss->direct);

    VSUB2(diag, nss->ap->a_rt_i->mdl_max, nss->ap->a_rt_i->mdl_min);
    bsphere_diameter = MAGNITUDE(diag);

    /*
     * calculate the distance from a plane normal to the ray direction through the center of
     * the bounding sphere and a plane normal to the ray direction through the aim point.
     */
    VADD2SCALE(center_bsphere, nss->ap->a_rt_i->mdl_max, nss->ap->a_rt_i->mdl_min, 0.5);

    dist_to_target = DIST_PT_PT(center_bsphere, ray_point);

    VSUB2(dvec, ray_point, center_bsphere);
    VUNITIZE(dvec);
    delta = dist_to_target*VDOT(ray_dir, dvec);

    /*
     * this should put us about a bounding sphere radius in front of the bounding sphere
     */
    bov = bsphere_diameter + delta;

    return bov;
}

/********************
 * Raytracing setup *
 ********************/

HIDDEN int
raytrace_gettrees(struct nirt_state *nss)
{
    int i = 0;
    int ocnt = 0;
    int acnt = 0;

    // Prepare C-style arrays for rt prep
    const char **objs = (const char **)bu_calloc(nss->active_paths.size() + 1, sizeof(char *), "objs");
    const char **attrs = (const char **)bu_calloc(nss->attrs.size() + 1, sizeof(char *), "attrs");
    std::set<std::string>::iterator s_it;
    for (s_it = nss->active_paths.begin(); s_it != nss->active_paths.end(); s_it++) {
	const char *o = (*s_it).c_str();
	objs[ocnt] = o;
	ocnt++;
    }
    objs[ocnt] = NULL;
    for (s_it = nss->attrs.begin(); s_it != nss->attrs.end(); s_it++) {
	const char *a = (*s_it).c_str();
	attrs[acnt] = a;
	acnt++;
    }
    attrs[acnt] = NULL;
    lout(nss, "Get trees...\n");

    i = rt_gettrees_and_attrs(nss->ap->a_rt_i, attrs, ocnt, objs, 1);
    bu_free(objs, "objs");
    bu_free(attrs, "objs");
    if (i) {
	lerr(nss, "rt_gettrees() failed\n");
	return -1;
    }

    return 0;
}

HIDDEN struct rt_i *
get_rtip(struct nirt_state *nss)
{
    if (!nss || nss->dbip == DBI_NULL) return NULL;

    if (nss->use_air) {
	if (nss->rtip_air == RTI_NULL) {
	    nss->rtip_air = rt_new_rti(nss->dbip); /* clones dbip, so we can operate on the copy */
	    nss->rtip_air->rti_dbip->dbi_fp = fopen(nss->dbip->dbi_filename, "rb"); /* get read-only fp */
	    if (nss->rtip_air->rti_dbip->dbi_fp == NULL) {
		rt_free_rti(nss->rtip_air);
		nss->rtip_air = RTI_NULL;
		return RTI_NULL;
	    }
	    nss->rtip_air->rti_dbip->dbi_read_only = 1;
	    rt_init_resource(nss->res_air, 0, nss->rtip_air);
	}
	return nss->rtip_air;
    }

    if (nss->rtip == RTI_NULL) {
	nss->rtip = rt_new_rti(nss->dbip); /* clones dbip, so we can operate on the copy */
	nss->rtip->rti_dbip->dbi_fp = fopen(nss->dbip->dbi_filename, "rb"); /* get read-only fp */
	if (nss->rtip->rti_dbip->dbi_fp == NULL) {
	    rt_free_rti(nss->rtip);
	    nss->rtip = RTI_NULL;
	    return RTI_NULL;
	}
	nss->rtip->rti_dbip->dbi_read_only = 1;
	rt_init_resource(nss->res, 0, nss->rtip);
    }
    return nss->rtip;
}

HIDDEN struct resource *
get_resource(struct nirt_state *nss)
{
    if (nss->use_air) return nss->res_air;
    return nss->res;
}

HIDDEN int
raytrace_prep(struct nirt_state *nss)
{
    std::set<std::string>::iterator s_it;
    lout(nss, "Prepping the geometry...\n");
    if (nss->rtip_air) {
	rt_prep(nss->rtip_air);
    } 
    if (nss->rtip) {
	rt_prep(nss->rtip);
    }
    lout(nss, "%s", (nss->active_paths.size() == 1) ? "Object" : "Objects");
    for (s_it = nss->active_paths.begin(); s_it != nss->active_paths.end(); s_it++) {
	lout(nss, " %s", (*s_it).c_str());
    }
    lout(nss, " processed\n");
    return 0;
}

/*********************
 * Output formatting *
 *********************/

unsigned int
fmt_sp_cnt(const char *fmt) {
    unsigned int fcnt = 0;
    const char *uos = NULL;
    for (uos = fmt; (*(uos + 1) != '"' && *(uos + 1) != '\0'); ++uos) {
	if (*uos == '%' && (*(uos + 1) == '%')) continue;
	if (*uos == '%') fcnt++;
    }
    return fcnt;
}

/* Note: because of split_fmt, we can assume at most one format specifier is
 * present in the fmt string */
int
fmt_sp_get(const char *fmt, std::string &fmt_sp)
{
    int found = 0;
    const char *uos = NULL;
    if (!fmt) return 0;
    /* Find uncommented '%' format specifier */
    for (uos = fmt; (*uos != '"' && *uos != '\0'); ++uos) {
	if (*uos == '%' && (*(uos + 1) == '%')) continue;
	if (*uos == '%') {
	    found++;
	    break;
	}
    }
    if (!found) return 0;

    // Find the terminating character of the specifier and build
    // the substring.
    std::string wfmt(uos);
    size_t ep = wfmt.find_first_of(NIRT_PRINTF_SPECIFIERS);
    if (ep == std::string::npos) return 0;
    fmt_sp = wfmt.substr(0, ep+1);
    return 1;
}

/* Return 0 if specifier type is acceptable for supplied key, 1 if key is not found,
 * 2 if the type isn't acceptable, and -1 if there's another error */
int
fmt_sp_key_check(struct nirt_state *nss, const char *key, std::string &fmt_sp)
{
    const char *type = NULL;
    if (!nss || !nss->val_types || fmt_sp.length() == 0) return -1;
    if (!key) return 1;
    type = bu_avs_get(nss->val_types, key);
    if (!type) return 1;
    switch (fmt_sp.c_str()[fmt_sp.length()-1]) {
	case 'd':
	case 'i':
	    if (BU_STR_EQUAL(type, "INT")) return 0;
	    return 2;
	    break;
	case 'f':
	case 'e':
	case 'E':
	case 'g':
	case 'G':
	    if (BU_STR_EQUAL(type, "FLOAT") || BU_STR_EQUAL(type, "FNOUNIT")) return 0;
	    return 2;
	    break;
	case 's':
	    if (BU_STR_EQUAL(type, "STRING")) return 0;
	    return 2;
	    break;
	default:
	    return 2;
    }
} 

int
split_fmt(const char *fmt, char ***breakout)
{
    int i = 0;
    int fcnt = 0;
    const char *up = NULL;
    const char *uos = NULL;
    char **fstrs = NULL;
    struct bu_vls specifier = BU_VLS_INIT_ZERO;

    if (!fmt) return -1;

    /* Count maximum possible number of format specifiers.  We have at most
     * fcnt+1 "units" to handle */
    fcnt = fmt_sp_cnt(fmt);
    fstrs = (char **)bu_calloc(fcnt + 1, sizeof(const char *), "output formatters");

    /* initialize to just after the initial '"' char */
    uos = (fmt[0] == '"') ? fmt + 1 : fmt;

    /* Find one specifier per substring breakout */
    fcnt = 0;
    while (*uos != '"' && *uos != '\0') {
	int have_specifier= 0;
	/* Find first '%' format specifier*/
	for (up = uos; (*uos != '"' && *uos != '\0'); ++uos) {
	    if (*uos == '%' && (*(uos + 1) == '%')) continue;
	    if (*uos == '\\' && (*(uos + 1) == '"')) continue;
	    if (*uos == '%' && have_specifier) break;
	    if (*uos == '%' && !have_specifier) have_specifier = 1;
	}

	/* Store the format. */
	bu_vls_trunc(&specifier, 0);
	while (up != uos) {
	    if (*up == '\\') {
		switch (*(up + 1)) {
		    case 'n':
			bu_vls_putc(&specifier, '\n');
			up += 2;
			break;
		    case '\042':
		    case '\047':
		    case '\134':
			bu_vls_printf(&specifier, "%c", *(up + 1));
			up += 2;
			break;
		    default:
			bu_vls_putc(&specifier, *up++);
			break;
		}
	    } else {
		bu_vls_putc(&specifier, *up++);
	    }
	}
	fstrs[fcnt] = bu_strdup(bu_vls_addr(&specifier));
	fcnt++;
    }
    if (fcnt) {
	(*breakout) = (char **)bu_calloc(fcnt, sizeof(const char *), "breakout");
	for (i = 0; i < fcnt; i++) {
	    (*breakout)[i] = fstrs[i];
	}
	bu_free(fstrs, "temp container");
	bu_vls_free(&specifier);
	return fcnt;
    } else {
	bu_free(fstrs, "temp container");
	bu_vls_free(&specifier);
	return -1;
    }
}


/************************
 * Raytracing Callbacks *
 ************************/

extern "C" int
nirt_if_hit(struct application *UNUSED(ap), struct partition *UNUSED(part_head), struct seg *UNUSED(finished_segs))
{
    bu_log("hit\n");
    return HIT;
}

extern "C" int
nirt_if_miss(struct application *UNUSED(ap))
{
    return MISS;
}

extern "C" int
nirt_if_overlap(struct application *ap, struct partition *pp, struct region *reg1, struct region *reg2, struct partition *InputHdp)
{
    bu_log("overlap\n");
    /* Match current BRL-CAD default behavior */
    return rt_defoverlap (ap, pp, reg1, reg2, InputHdp);
}

/**************
 *  Commands  *
 **************/

struct nirt_cmd_desc {
    const char *cmd;  // Must match the eventual bu_cmtab key
    const char *desc;
    const char *args;
};

const struct nirt_cmd_desc nirt_descs[] = {
    { "attr",           "select attributes",                             NULL },
    { "ae",             "set/query azimuth and elevation",               "azimuth elevation" },
    { "dir",            "set/query direction vector",                    "x-component y-component z-component" },
    { "hv",             "set/query gridplane coordinates",               "horz vert [dist]" },
    { "xyz",            "set/query target coordinates",                  "X Y Z" },
    { "s",              "shoot a ray at the target",                     NULL },
    { "backout",        "back out of model",                             NULL },
    { "useair",         "set/query use of air",                          "<0|1|2|...>" },
    { "units",          "set/query local units",                         "<mm|cm|m|in|ft>" },
    { "overlap_claims", "set/query overlap rebuilding/retention",        "<0|1|2|3>" },
    { "fmt",            "set/query output formats",                      "{rhpfmog} format item item ..." },
    { "print",          "query an output item",                          "item" },
    { "bot_minpieces",  "Get/Set value for rt_bot_minpieces (0 means do not use pieces, default is 32)", "min_pieces" },
    { "libdebug",       "set/query librt debug flags",                   "hex_flag_value" },
    { "debug",          "set/query nirt debug flags",                    "hex_flag_value" },
    { "q",              "quit",                                          NULL },
    { "?",              "display this help menu",                        NULL },
    { (char *)NULL,     NULL,                                            NULL}
};

const char *
get_desc_args(const char *key)
{
    const struct nirt_cmd_desc *d;
    for (d = nirt_descs; d->cmd; d++) {
	if (BU_STR_EQUAL(d->cmd, key)) return d->args;
    }
    return NULL;
}

extern "C" int
cm_attr(void *ns, int argc, const char *argv[])
{
    if (!ns) return -1;
    struct nirt_state *nss = (struct nirt_state *)ns;
    int i = 0;
    int ac = 0;
    int print_help = 0;
    int attr_print = 0;
    int attr_flush = 0;
    int val_attrs = 0;
    size_t orig_size = nss->attrs.size();
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    struct bu_opt_desc d[5];
    BU_OPT(d[0],  "h", "help",  "",  NULL,   &print_help, "print help and exit");
    BU_OPT(d[1],  "p", "print", "",  NULL,   &attr_print, "print active attributes");
    BU_OPT(d[2],  "f", "flush", "",  NULL,   &attr_flush, "clear attribute list");
    BU_OPT(d[3],  "v", "",      "",  NULL,   &val_attrs,  "validate attributes - only accept attributes used by one or more objects in the active database");
    BU_OPT_NULL(d[4]);
    const char *help = bu_opt_describe(d, NULL);
    const char *ustr = "Usage: attr <opts> <attribute_name_1> <attribute_name_2> ...\nNote: attr with no options and no attributes to add will print the list of active attributes.\nOptions:";

    argv++; argc--;

    if ((ac = bu_opt_parse(&optparse_msg, argc, (const char **)argv, d)) == -1) {
	lerr(nss, "Option parsing error: %s\n\n%s\n%s\n", bu_vls_addr(&optparse_msg), ustr, help);
	if (help) bu_free((char *)help, "help str");
	bu_vls_free(&optparse_msg);
	return -1;
    }
    bu_vls_free(&optparse_msg);

    if (attr_flush) nss->attrs.clear();

    if (print_help || (attr_print && ac > 0)) {
	lerr(nss, "%s\n%s", ustr, help);
	if (help) bu_free((char *)help, "help str");
	return -1;
    }
    if (attr_print || ac == 0) {
	std::set<std::string>::iterator a_it;
	for (a_it = nss->attrs.begin(); a_it != nss->attrs.end(); a_it++) {
	    lout(nss, "\"%s\"\n", (*a_it).c_str());
	}
	return 0;
    }

    for (i = 0; i < ac; i++) {
	const char *sattr = db5_standard_attribute(db5_standardize_attribute(argv[i]));
	if (val_attrs) {
	    //TODO - check against the .g for any usage of the attribute
	}
	nss->attrs.insert(argv[i]);
	/* If there is a standardized version of this attribute that is
	 * different from the supplied version, activate it as well. */
	if (sattr) nss->attrs.insert(sattr);
    }

    if (nss->active_paths.size() != orig_size) raytrace_prep(nss);
    return 0;
}

extern "C" int
az_el(void *ns, int argc, const char *argv[])
{
    double az = 0.0;
    double el = 0.0;
    int ret = 0;
    struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	lout(nss, "(az, el) = (%4.2f, %4.2f)\n", nss->azimuth, nss->elevation);
	return 0;
    }

    argv++; argc--;

    if (argc != 2) {
	lerr(nss, "Usage:  ae %s\n", get_desc_args("ae"));
	return -1;
    }

    if ((ret = bu_opt_fastf_t(&opt_msg, 1, argv, (void *)&az)) == -1) {
	lerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	goto azel_done;
    }

    if (fabs(az) > 360) {
	lerr(nss, "Error:  |azimuth| <= 360\n");
	ret = -1;
	goto azel_done;
    }

    argc--; argv++;
    if ((ret = bu_opt_fastf_t(&opt_msg, 1, argv, (void *)&el)) == -1) {
	lerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	goto azel_done;
    }

    if (fabs(el) > 90) {
	lerr(nss, "Error:  |elevation| <= 90\n");
	ret = -1;
	goto azel_done;
    }

    nss->azimuth = az;
    nss->elevation = el;
    ae2dir(nss);
    ret = 0;

azel_done:
    bu_vls_free(&opt_msg);
    return ret;
}

extern "C" int
dir_vect(void *ns, int argc, const char *argv[])
{
    vect_t dir = VINIT_ZERO;
    struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	lout(nss, "(x, y, z) = (%4.2f, %4.2f, %4.2f)\n", V3ARGS(nss->direct));
	return 0;
    }

    argc--; argv++;
    if (argc != 3) {
	lerr(nss, "Usage:  dir %s\n", get_desc_args("dir"));
	return -1;
    }

    if (bu_opt_vect_t(&opt_msg, 3, argv, (void *)&dir) == -1) {
	lerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	bu_vls_free(&opt_msg);
	return -1;
    }

    VUNITIZE(dir);
    VMOVE(nss->direct, dir);
    dir2ae(nss);
    return 0;
}

extern "C" int
grid_coor(void *ns, int argc, const char *argv[])
{
    vect_t grid = VINIT_ZERO;
    struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	VMOVE(grid, nss->grid);
	VSCALE(grid, grid, nss->base2local);
	lout(nss, "(h, v, d) = (%4.2f, %4.2f, %4.2f)\n", V3ARGS(grid));
	return 0;
    }

    argc--; argv++;
    if (argc != 2 && argc != 3) {
	lerr(nss, "Usage:  hv %s\n", get_desc_args("hv"));
	return -1;
    }
    if (bu_opt_fastf_t(&opt_msg, 1, argv, (void *)&(grid[HORZ])) == -1) {
	lerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	bu_vls_free(&opt_msg);
	return -1;
    }
    argc--; argv++;
    if (bu_opt_fastf_t(&opt_msg, 1, argv, (void *)&(grid[VERT])) == -1) {
	lerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	bu_vls_free(&opt_msg);
	return -1;
    }
    argc--; argv++;
    if (argc) {
	if (bu_opt_fastf_t(&opt_msg, 1, argv, (void *)&(grid[DIST])) == -1) {
	    lerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	    bu_vls_free(&opt_msg);
	    return -1;
	}
    } else {
	grid[DIST] = nss->grid[DIST];
    }

    VSCALE(grid, grid, nss->local2base);
    VMOVE(nss->grid, grid);
    grid2targ(nss);

    return 0;
}

extern "C" int
target_coor(void *ns, int argc, const char *argv[])
{
    vect_t target = VINIT_ZERO;
    struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	VMOVE(target, nss->target);
	VSCALE(target, target, nss->base2local);
	lout(nss, "(x, y, z) = (%4.2f, %4.2f, %4.2f)\n", V3ARGS(target));
	return 0;
    }

    argc--; argv++;
    if (argc != 3) {
	lerr(nss, "Usage:  xyz %s\n", get_desc_args("xyz"));
	return -1;
    }

    if (bu_opt_vect_t(&opt_msg, 3, argv, (void *)&target) == -1) {
	lerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	bu_vls_free(&opt_msg);
	return -1;
    }

    VSCALE(target, target, nss->local2base);
    VMOVE(nss->target, target);
    targ2grid(nss);

    return 0;
}

extern "C" int
shoot(void *ns, int argc, const char **UNUSED(argv))
{
    int i;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns || !nss->ap || !nss->ap->a_rt_i) return -1;

    if (argc != 1) {
	lerr(nss, "Usage:  s\n");
	return -1;
    }

    double bov = backout(nss);
    for (i = 0; i < 3; ++i) {
	nss->target[i] = nss->target[i] + (bov * -1*(nss->direct[i]));
	nss->ap->a_ray.r_pt[i] = nss->target[i];
	nss->ap->a_ray.r_dir[i] = nss->direct[i];
    }

    ldbg(nss, DEBUG_BACKOUT,
	    "Backing out %g units to (%g %g %g), shooting dir is (%g %g %g)\n",
	    bov * nss->base2local,
	    nss->ap->a_ray.r_pt[0] * nss->base2local,
	    nss->ap->a_ray.r_pt[1] * nss->base2local,
	    nss->ap->a_ray.r_pt[2] * nss->base2local,
	    V3ARGS(nss->ap->a_ray.r_dir));

    // TODO - any necessary initialization for data collection by callbacks

    (void)rt_shootray(nss->ap);

    // Undo backout
    for (i = 0; i < 3; ++i) {
	nss->target[i] = nss->target[i] - (bov * -1*(nss->direct[i]));
    }

    return 0;
}

extern "C" int
backout(void *ns, int argc, const char *argv[])
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	lout(nss, "backout = %d\n", nss->backout);
	return 0;
    }

    if (argc == 2 && BU_STR_EQUAL(argv[1], "0")) {
	nss->backout = 0;
	return 0;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], "1")) {
	nss->backout = 1;
	return 0;
    }

    lerr(nss, "backout must be set to 0 (off) or 1 (on)\n");

    return -1;
}

extern "C" int
use_air(void *ns, int argc, const char *argv[])
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    if (!ns) return -1;
    if (argc == 1) {
	lout(nss, "use_air = %d\n", nss->use_air);
	return 0;
    }
    if (argc > 2) {
	lerr(nss, "Usage:  useair %s\n", get_desc_args("useair"));
	return -1;
    }
    if (bu_opt_int(NULL, 1, (const char **)&(argv[1]), (void *)&nss->use_air) == -1) {
	lerr(nss, "Option parsing error: %s\n\nUsage:  useair %s\n", bu_vls_addr(&optparse_msg), get_desc_args("useair"));
	return -1;
    }
    nss->ap->a_rt_i = get_rtip(nss);
    nss->ap->a_resource = get_resource(nss);
    return 0;
}

extern "C" int
nirt_units(void *ns, int argc, const char *argv[])
{
    double ncv = 0.0;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	lout(nss, "units = %s\n", bu_units_string(nss->local2base));
	return 0;
    }

    if (argc > 2) {
	lerr(nss, "Usage:  units %s\n", get_desc_args("units"));
	return -1;
    }

    if (BU_STR_EQUAL(argv[1], "default")) {
	nss->base2local = nss->dbip->dbi_base2local;
	nss->local2base = nss->dbip->dbi_local2base;
	return 0;
    }

    ncv = bu_units_conversion(argv[1]);
    if (ncv <= 0.0) {
	lerr(nss, "Invalid unit specification: '%s'\n", argv[1]);
	return -1;
    }
    nss->base2local = 1.0/ncv;
    nss->local2base = ncv;

    return 0;
}

extern "C" int
do_overlap_claims(void *ns, int argc, const char *argv[])
{
    int valid_specification = 0;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	switch (nss->overlap_claims) {
	    case OVLP_RESOLVE:
		lout(nss, "resolve (0)\n");
		return 0;
	    case OVLP_REBUILD_FASTGEN:
		lout(nss, "rebuild_fastgen (1)\n");
		return 0;
	    case OVLP_REBUILD_ALL:
		lout(nss, "rebuild_all (2)\n");
		return 0;
	    case OVLP_RETAIN:
		lout(nss, "retain (3)\n");
		return 0;
	    default:
		lerr(nss, "Error - overlap_clams is set to invalid value %d\n", nss->overlap_claims);
		return -1;
	}
    }

    if (argc > 2) {
	lerr(nss, "Usage:  overlap_claims %s\n", get_desc_args("overlap_claims"));
	return -1;
    }

    if (BU_STR_EQUAL(argv[1], "resolve") || BU_STR_EQUAL(argv[1], "0")) {
	nss->overlap_claims = OVLP_RESOLVE;
	valid_specification = 1;
    }
    if (BU_STR_EQUAL(argv[1], "rebuild_fastgen") || BU_STR_EQUAL(argv[1], "1")) {
	nss->overlap_claims = OVLP_REBUILD_FASTGEN;
	valid_specification = 1;
    }
    if (BU_STR_EQUAL(argv[1], "rebuild_all") || BU_STR_EQUAL(argv[1], "2")) {
	nss->overlap_claims = OVLP_REBUILD_ALL;
	valid_specification = 1;
    }
    if (BU_STR_EQUAL(argv[1], "retain") || BU_STR_EQUAL(argv[1], "3")) {
	nss->overlap_claims = OVLP_RETAIN;
	valid_specification = 1;
    }

    if (!valid_specification) {
	lerr(nss, "Invalid overlap_claims specification: '%s'\n", argv[1]);
	return -1;
    }

    if (nss->rtip) nss->rtip->rti_save_overlaps = (nss->overlap_claims > 0);
    if (nss->rtip_air) nss->rtip_air->rti_save_overlaps = (nss->overlap_claims > 0);

    return 0;
}

extern "C" int
format_output(void *ns, int argc, const char **argv)
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    char type;
    if (!ns) return -1;

    if (argc < 2 || strlen(argv[1]) != 1 || BU_STR_EQUAL(argv[1], "?")) {
	// TODO - need a *much* better help for fmt
	lerr(nss, "Usage:  fmt %s\n", get_desc_args("fmt"));
	return -1;
    }

    type = argv[1][0];
    if (!strchr("rhpfmog", type)) {
	lerr(nss, "Unknown fmt type %s\n", type);
	return -1;
    }

    argc--; argv++;
    if (argc == 1) {
	// TODO - print out current fmt str for the specified type
	// struct bu_vls ostr = BU_VLS_INIT_ZERO:
	// fmt_str(&ostr, nss, type);
	//lout(nss, "%s\n", bu_vls_addr(&ostr));
	//bu_vls_free(&ostr);
	return 0;
    }

    argc--; argv++;
    if (argc) {
	int fmt_cnt = 0;
	std::vector<std::pair<std::string,std::string> > fmt_tmp;
	const char *fmtstr = argv[0];
	char **fmts = NULL;
	if ((fmt_cnt = split_fmt(fmtstr, &fmts)) <= 0) {
	    lerr(nss, "Error parsing format string \"%s\"\n", fmtstr);
	    return -1;
	}
	argc--; argv++;

	for (int i = 0; i < fmt_cnt; i++) {
	    unsigned int fc = fmt_sp_cnt(fmts[i]);
	    const char *key = NULL;
	    if (fc > 0) {
		int key_check = 0;
		std::string fs;
		if (!argc) {
		    lerr(nss, "Error parsing format string \"%s\" - missing value for format specifier in substring \"%s\"\n", fmtstr, fmts[i]);
		    return -1;
		}
		key = argv[0];
		if (!fmt_sp_get(fmts[i],fs)) {
		    lerr(nss, "Error - could not find format specifier in fmt substring \"%s\"\n", fmts[i]);
		    return -1;
		}
		// check type of supplied format specifier value against fmt_sp substring
		key_check = fmt_sp_key_check(nss, key, fs);
		if (key_check) {
		    switch (key_check) {
			case 1:
			    lerr(nss, "Key \"%s\" supplied to format specifier \"%s\" is not a valid NIRT value key\n", key, fs.c_str());
			    return -1;
			    break;
			case 2:
			    lerr(nss, "NIRT value key \"%s\" has type %s, which does not match the type expected by format specifier \"%s\" \n", key, bu_avs_get(nss->val_types, key), fs.c_str());
			    return -1;
			    break;
			default:
			    lerr(nss, "Internal NIRT format processing error.\n");
			    return -1;
			    break;

		    }
		}
		std::cout << "fmtsubstr: " << fmts[i] << " , specifier: " << fs << "\n";
		argc--; argv++;
	    }
	    if (key) {
		fmt_tmp.push_back(std::make_pair(std::string(fmts[i]), std::string(key)));
	    } else {
		/* If there are no format specifiers, we don't need any key */
		fmt_tmp.push_back(std::make_pair(std::string(fmts[i]), ""));
		argc--; argv++;
	    }
	}

	if (argc) {
	    lerr(nss, "Error: fmt string \"%s\" has %d format specifiers, but has %d arguments specified\n", argv[2], fmt_cnt, fmt_cnt + argc);
	    return -1;
	}

	switch (type) {
	    case 'r':
		nss->fmt_ray.clear();
		nss->fmt_ray = fmt_tmp;
		break;
	    case 'h':
		break;
	    case 'p':
		break;
	    case 'f':
		break;
	    case 'm':
		break;
	    case 'o':
		break;
	    case 'g':
		break;
	    default:
		lerr(nss, "Unknown fmt type %s\n", argv[1]);
		return -1;
		break;
	}

	std::vector<std::pair<std::string,std::string> >::iterator f_it;
	for(f_it = nss->fmt_ray.begin(); f_it != nss->fmt_ray.end(); f_it++) {
	    bu_log("fmt[%c] %s\t\t\t:%s\n", type, (*f_it).first.c_str(), (*f_it).second.c_str());
	}
    }

    return 0;
}

extern "C" int
print_item(void *ns, int argc, const char **argv)
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	lout(nss, "%s\n", argv[0]);
	return 0;
    }


    // TODO - handle error
    // lerr(nss, "Usage:  print %s\n", get_desc_args("print"));


    bu_log("print_item\n");
    return 0;
}

extern "C" int
bot_minpieces(void *ns, int argc, const char **argv)
{
    /* Ew - rt_bot_minpieces is a librt global.  Why isn't it
     * a part of the specific rtip? */
    int ret = 0;
    long minpieces = 0;
    struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	lout(nss, "rt_bot_minpieces = %d\n", (unsigned int)rt_bot_minpieces);
	return 0;
    }

    argc--; argv++;

    if (argc > 1) {
	lerr(nss, "Usage:  bot_minpieces %s\n", get_desc_args("bot_minpieces"));
	return -1;
    }

    if ((ret = bu_opt_long(&opt_msg, 1, argv, (void *)&minpieces)) == -1) {
	lerr(nss, "%s\n", bu_vls_addr(&opt_msg));
	goto bot_minpieces_done;
    }

    if (minpieces < 0) {
	lerr(nss, "Error: rt_bot_minpieces cannot be less than 0\n");
	ret = -1;
	goto bot_minpieces_done;
    }

    if (rt_bot_minpieces != (size_t)minpieces) {
	rt_bot_minpieces = minpieces;
	bu_vls_free(&opt_msg);
	return raytrace_prep(nss);
    }

bot_minpieces_done:
    bu_vls_free(&opt_msg);
    return ret;
}

extern "C" int
librt_debug(void *ns, int argc, const char **argv)
{
    int ret = 0;
    long dflg = 0;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    /* Sigh... another librt global, not rtip specific... */
    if (argc == 1) {
	bu_vls_printb(&msg, "librt debug ", RT_G_DEBUG, DEBUG_FMT);
	lout(nss, "%s\n", bu_vls_addr(&msg));
	goto librt_nirt_debug_done;
    }

    argc--; argv++;

    if (argc > 1) {
	lerr(nss, "Usage:  libdebug %s\n", get_desc_args("libdebug"));
	return -1;
    }

    if ((ret = bu_opt_long_hex(&msg, 1, argv, (void *)&dflg)) == -1) {
	lerr(nss, "%s\n", bu_vls_addr(&msg));
	goto librt_nirt_debug_done;
    }

    if (dflg < 0 || dflg > UINT32_MAX) {
	lerr(nss, "Error: LIBRT debug flag cannot be less than 0 or greater than %d\n", UINT32_MAX);
	ret = -1;
	goto librt_nirt_debug_done;
    }

    RT_G_DEBUG = (uint32_t)dflg;
    bu_vls_printb(&msg, "librt debug ", RT_G_DEBUG, DEBUG_FMT);
    lout(nss, "%s\n", bu_vls_addr(&msg));

librt_nirt_debug_done:
    bu_vls_free(&msg);
    return ret;
}

extern "C" int
nirt_debug(void *ns, int argc, const char **argv)
{
    int ret = 0;
    long dflg = 0;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;

    if (argc == 1) {
	bu_vls_printb(&msg, "debug ", nss->nirt_debug, DEBUG_FMT);
	lout(nss, "%s\n", bu_vls_addr(&msg));
	goto nirt_debug_done;
    }

    argc--; argv++;

    if (argc > 1) {
	lerr(nss, "Usage:  debug %s\n", get_desc_args("debug"));
	return -1;
    }

    if ((ret = bu_opt_long_hex(&msg, 1, argv, (void *)&dflg)) == -1) {
	lerr(nss, "%s\n", bu_vls_addr(&msg));
	goto nirt_debug_done;
    }

    if (dflg < 0) {
	lerr(nss, "Error: NIRT debug flag cannot be less than 0\n");
	ret = -1;
	goto nirt_debug_done;
    }

    nss->nirt_debug = (unsigned int)dflg;
    bu_vls_printb(&msg, "debug ", nss->nirt_debug, DEBUG_FMT);
    lout(nss, "%s\n", bu_vls_addr(&msg));

nirt_debug_done:
    bu_vls_free(&msg);
    return ret;
}

extern "C" int
quit(void *ns, int UNUSED(argc), const char **UNUSED(argv))
{
    lout((struct nirt_state *)ns, "Quitting...\n");
    return 1;
}

extern "C" int
show_menu(void *ns, int UNUSED(argc), const char **UNUSED(argv))
{
    int longest = 0;
    const struct nirt_cmd_desc *d;
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    for (d = nirt_descs; d->cmd != NULL; d++) {
	int l = strlen(d->cmd);
	if (l > longest) longest = l;
    }
    for (d = nirt_descs; d->cmd != NULL; d++) {
	lout(nss, "%*s %s\n", longest, d->cmd, d->desc);
    }
    return 0;
}

extern "C" int
draw_cmd(void *ns, int argc, const char *argv[])
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    int i = 0;
    int ret = 0;
    if (!ns || argc < 2) return -1;
    for (i = 1; i < argc; i++) {
	bu_log("draw_cmd: drawing %s\n", argv[i]);
	nss->active_paths.insert(argv[i]);
    }
    ret = raytrace_gettrees(nss);
    if (ret) return ret;
    return raytrace_prep(nss);
}

extern "C" int
erase_cmd(void *ns, int UNUSED(argc), const char **UNUSED(argv))
{
    //struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    bu_log("erase_cmd\n");
    return 0;
}

extern "C" int
state_cmd(void *ns, int argc, const char *argv[])
{
    struct nirt_state *nss = (struct nirt_state *)ns;
    if (!ns) return -1;
    struct rt_i *rtip = nss->ap->a_rt_i;
    double base2local = rtip->rti_dbip->dbi_base2local;
    if (argc == 1) {
	// TODO
	return 0;
    }
    if (BU_STR_EQUAL(argv[1], "out_accumulate")) {
	int setting = 0;
	if (argc == 2) {
	    lout(nss, "%d\n", nss->out_accumulate);
	    return 0;
	}
	(void)bu_opt_int(NULL, 1, (const char **)&(argv[2]), (void *)&setting);
	nss->out_accumulate = setting;
	return 0;
    }
    if (BU_STR_EQUAL(argv[1], "err_accumulate")) {
	int setting = 0;
	if (argc == 2) {
	    lout(nss, "%d\n", nss->err_accumulate);
	    return 0;
	}
	(void)bu_opt_int(NULL, 1, (const char **)&(argv[2]), (void *)&setting);
	nss->err_accumulate = setting;
	return 0;
    }
    if (BU_STR_EQUAL(argv[1], "model_bounds")) {
	if (argc != 2) {
	    lerr(nss, "TODO - state cmd help\n");
	    return -1;
	}
	lout(nss, "model_min = (%g, %g, %g)    model_max = (%g, %g, %g)\n",
		rtip->mdl_min[X] * base2local,
		rtip->mdl_min[Y] * base2local,
		rtip->mdl_min[Z] * base2local,
		rtip->mdl_max[X] * base2local,
		rtip->mdl_max[Y] * base2local,
		rtip->mdl_max[Z] * base2local);
	return 0;
    }
    return 0;
}

const struct bu_cmdtab nirt_cmds[] = {
    { "attr",           cm_attr},
    { "ae",             az_el},
    { "dir",            dir_vect},
    { "hv",             grid_coor},
    { "xyz",            target_coor},
    { "s",              shoot},
    { "backout",        backout},
    { "useair",         use_air},
    { "units",          nirt_units},
    { "overlap_claims", do_overlap_claims},
    { "fmt",            format_output},
    { "print",          print_item},
    { "bot_minpieces",  bot_minpieces},
    { "libdebug",       librt_debug},
    { "debug",          nirt_debug},
    { "q",              quit},
    { "?",              show_menu},
    // New commands...
    { "draw",           draw_cmd},
    { "erase",          erase_cmd},
    { "state",          state_cmd},
    { (char *)NULL,     NULL}
};



/* Parse command line command and execute */
HIDDEN int
nirt_exec_cmd(NIRT *ns, const char *cmdstr)
{
    int ac;
    int ret = 0;
    char *cmdp = NULL;
    char **av = NULL;
    if (!ns || !cmdstr || strlen(cmdstr) > MAXPATHLEN) return -1;

    /* Take the quoting level down by one, now that we are about to execute */

    // TODO - should we be using bu_vls_decode for this instead of
    // bu_str_unescape?
    cmdp = (char *)bu_calloc(strlen(cmdstr)+2, sizeof(char), "unescaped str");
    bu_str_unescape(cmdstr, cmdp, strlen(cmdstr)+1);

    /* get an argc/argv array for bu_cmd style command execution */
    av = (char **)bu_calloc(strlen(cmdp)+1, sizeof(char *), "av");
    ac = bu_argv_from_string(av, strlen(cmdp), cmdp);

    if (bu_cmd(nirt_cmds, ac, (const char **)av, 0, (void *)ns, &ret) != BRLCAD_OK) {
	ret = -1;
    }

    bu_free(av, "free av array");
    bu_free(cmdp, "free unescaped copy of input cmd");
    return ret;
}

HIDDEN size_t
find_first_unescaped(std::string &s, const char *keys, int offset)
{
    int off = offset;
    int done = 0;
    size_t candidate = std::string::npos;
    while (!done) {
	candidate = s.find_first_of(keys, off);
	if (!candidate || candidate == std::string::npos) return candidate;
	if (s.at(candidate - 1) == '\\') {
	    off = candidate + 1;
	} else {
	    done = 1;
	}
    }
    return candidate;
}

/* Parse command line script into individual commands, and run
 * the supplied callback on each command. The semicolon ';' char
 * denotes the end of one command and the beginning of another, unless
 * the semicolon is inside single or double quotes*/
HIDDEN int
nirt_parse_script(NIRT *ns, const char *script, int (*nc)(NIRT *ns, const char *cmdstr))
{
    if (!ns || !script || !nc) return -1;
    int ret = 0;
    std::string s(script);
    std::string cmd;
    struct bu_vls tcmd = BU_VLS_INIT_ZERO;
    size_t pos = 0;
    size_t q_start, q_end;
    struct bu_vls echar = BU_VLS_INIT_ZERO;

    /* Start by initializing the position markers for quoted substrings. */
    q_start = find_first_unescaped(s, "'\"", 0);
    if (q_start != std::string::npos) {
	bu_vls_sprintf(&echar, "%c", s.at(q_start));
	q_end = find_first_unescaped(s, bu_vls_addr(&echar), q_start + 1);
    } else {
	q_end = std::string::npos;
    }

    /* Slice and dice to get individual commands. */
    while ((pos = s.find(";", pos)) != std::string::npos) {
	/* If we're inside matched quotes, only an un-escaped quote char means
	 * anything */
	if (q_end != std::string::npos && pos > q_start && pos < q_end) {
	    pos = q_end + 1;
	    continue;
	}
	/* Extract and operate on the command specific subset string */
	cmd = s.substr(0, pos);
	bu_vls_sprintf(&tcmd, "%s", cmd.c_str());
	bu_vls_trimspace(&tcmd);
	if (bu_vls_strlen(&tcmd) > 0) {
	    ret = (*nc)(ns, bu_vls_addr(&tcmd));
	    if (ret) goto nps_done;
	}

	/* Prepare the rest of the script string for processing */
	s.erase(0, pos + 1);
	q_start = find_first_unescaped(s, "'\"", 0);
	if (q_start != std::string::npos) {
	    bu_vls_sprintf(&echar, "%c", s.at(q_start));
	    q_end = find_first_unescaped(s, bu_vls_addr(&echar), q_start + 1);
	} else {
	    q_end = std::string::npos;
	}
	pos = 0;
    }

    /* If there's anything left over, it's a tailing command */
    bu_vls_sprintf(&tcmd, "%s", s.c_str());
    bu_vls_trimspace(&tcmd);
    if (bu_vls_strlen(&tcmd) > 0) {
	ret = (*nc)(ns, bu_vls_addr(&tcmd));
    }
nps_done:
    bu_vls_free(&tcmd);
    bu_vls_free(&echar);
    return ret;
}


//////////////////////////
/* Public API functions */
//////////////////////////

extern "C" int
nirt_alloc(NIRT **ns)
{
    unsigned int rgb[3] = {0, 0, 0};
    NIRT *n = NULL;

    if (!ns) return -1;

    /* Get memory */
    n = new nirt_state;
    BU_GET(n->hit_color, struct bu_color);
    BU_GET(n->miss_color, struct bu_color);
    BU_GET(n->overlap_color, struct bu_color);

    /* Strictly speaking these initializations are more
     * than an "alloc" function needs to do, but we want
     * a nirt state to be "functional" even though it
     * is not set up with a database (it's true/proper
     * initialization.)*/
    bu_color_from_rgb_chars(n->hit_color, (unsigned char *)&rgb);
    bu_color_from_rgb_chars(n->miss_color, (unsigned char *)&rgb);
    bu_color_from_rgb_chars(n->overlap_color, (unsigned char *)&rgb);
    n->print_header = 0;
    n->print_ident_flag = 0;
    n->silent_flag = SILENT_UNSET;
    n->rt_debug = 0;
    n->nirt_debug = 0;

    BU_GET(n->out, struct bu_vls);
    bu_vls_init(n->out);
    n->out_accumulate = 0;
    BU_GET(n->err, struct bu_vls);
    bu_vls_init(n->err);
    n->err_accumulate = 0;
    BU_LIST_INIT(&(n->s_vlist));
    n->segs = bn_vlblock_init(&(n->s_vlist), 32);
    n->ret = 0;

    n->h_state = NULL;
    n->h_out = NULL;
    n->h_err = NULL;
    n->h_segs = NULL;
    n->h_objs = NULL;
    n->h_frmts = NULL;
    n->h_view = NULL;

    n->azimuth = 0.0;
    n->base2local = 0.0;
    n->elevation = 0.0;
    n->local2base = 0.0;
    n->use_air = 0;
    VZERO(n->direct);
    VZERO(n->grid);
    VZERO(n->target);

    n->b_state = false;
    n->b_out = false;
    n->b_err = false;
    n->b_segs = false;
    n->b_objs = false;
    n->b_frmts = false;
    n->b_view = false;

    BU_GET(n->ap, struct application);
    n->dbip = DBI_NULL;
    BU_GET(n->res, struct resource);
    BU_GET(n->res_air, struct resource);
    n->rtip = RTI_NULL;
    n->rtip_air = RTI_NULL;

    BU_GET(n->val_types, struct bu_attribute_value_set);
    BU_GET(n->val_docs, struct bu_attribute_value_set);
    bu_avs_init_empty(n->val_types);
    bu_avs_init_empty(n->val_docs);

    /* Populate the output key and type information */
    {
	std::string s(ovals);
	std::string entry, key;
	std::stringstream ss(s);
	int place = 0;
	while (std::getline(ss, entry, ',')) {
	    std::string tentry;
	    size_t sb = entry.find_first_not_of(" \t");
	    size_t se = entry.find_last_not_of(" \t");
	    if (sb != std::string::npos && se != std::string::npos) {
		tentry = entry.substr(sb, se - sb + 1);
	    }
	    if (!place) { key = tentry; place++; continue; }
	    if (place == 1) {
		bu_avs_add(n->val_types, key.c_str(), tentry.c_str());
		place++;
		continue;
	    }
	    if (place == 2) {
		if (!tentry.empty()) {
		    bu_avs_add(n->val_docs, key.c_str(), tentry.c_str());
		}
		place = 0;
	    }
	}
    }

    n->u_data = NULL;

    (*ns) = n;
    return 0;
}

extern "C" int
nirt_init(NIRT *ns, struct db_i *dbip)
{
    if (!ns || !dbip) return -1;

    RT_CK_DBI(dbip);

    ns->dbip = dbip;

    /* initialize the application structure */
    RT_APPLICATION_INIT(ns->ap);
    ns->ap->a_hit = nirt_if_hit;        /* branch to if_hit routine */
    ns->ap->a_miss = nirt_if_miss;      /* branch to if_miss routine */
    ns->ap->a_overlap = nirt_if_overlap;/* branch to if_overlap routine */
    ns->ap->a_logoverlap = rt_silent_logoverlap;
    ns->ap->a_onehit = 0;               /* continue through shotline after hit */
    ns->ap->a_purpose = "NIRT ray";
    ns->ap->a_rt_i = get_rtip(ns);         /* rt_i pointer */
    ns->ap->a_resource = get_resource(ns); /* note: resource is initialized by get_rtip */
    ns->ap->a_zero1 = 0;           /* sanity check, sayth raytrace.h */
    ns->ap->a_zero2 = 0;           /* sanity check, sayth raytrace.h */
    ns->ap->a_uptr = (void *)ns;

    /* By default use the .g units */
    ns->base2local = dbip->dbi_base2local;
    ns->local2base = dbip->dbi_local2base;

    /* Initial direction is -x */
    VSET(ns->direct, -1, 0, 0);

    return 0;
}

void
nirt_destroy(NIRT *ns)
{
    if (!ns) return;
    bu_vls_free(ns->err);
    bu_vls_free(ns->out);
    bn_vlist_cleanup(&(ns->s_vlist));
    bn_vlblock_free(ns->segs);

    if (ns->rtip != RTI_NULL) rt_free_rti(ns->rtip);
    if (ns->rtip_air != RTI_NULL) rt_free_rti(ns->rtip_air);

    BU_PUT(ns->res, struct resource);
    BU_PUT(ns->res_air, struct resource);
    BU_PUT(ns->err, struct bu_vls);
    BU_PUT(ns->out, struct bu_vls);
    BU_PUT(ns->ap, struct application);
    BU_PUT(ns->hit_color, struct bu_color);
    BU_PUT(ns->miss_color, struct bu_color);
    BU_PUT(ns->overlap_color, struct bu_color);
    delete ns;
}

/* Note - defined inside the execute-once do-while loop to allow for putting a
 * semicolon at the end of a NIRT_HOOK() statement (makes code formatters
 * happier) */
#define NIRT_HOOK(btype,htype) \
    do { if (ns->btype && ns->htype){(*ns->htype)(ns, ns->u_data);} } \
    while (0)

int
nirt_exec(NIRT *ns, const char *script)
{
    ns->ret = 0;
    ns->b_state = false;
    ns->b_out = false;
    ns->b_err = false;
    ns->b_segs = false;
    ns->b_objs = false;
    ns->b_frmts = false;
    ns->b_view = false;

    /* Unless told otherwise, clear the textual outputs
     * before each nirt_exec call. */
    if (!ns->out_accumulate) bu_vls_trunc(ns->out, 0);
    if (!ns->err_accumulate) bu_vls_trunc(ns->err, 0);

    if (script) {
	ns->ret = nirt_parse_script(ns, script, &nirt_exec_cmd);
    } else {
	return 0;
    }

    NIRT_HOOK(b_state, h_state);
    NIRT_HOOK(b_out, h_out);
    NIRT_HOOK(b_err, h_err);
    NIRT_HOOK(b_segs, h_segs);
    NIRT_HOOK(b_objs, h_objs);
    NIRT_HOOK(b_frmts, h_frmts);
    NIRT_HOOK(b_view, h_view);

    return ns->ret;
}

void *
nirt_udata(NIRT *ns, void *u_data)
{
    if (!ns) return NULL;
    if (u_data) ns->u_data = u_data;
    return ns->u_data;
}

void
nirt_hook(NIRT *ns, nirt_hook_t hf, int flag)
{
    if (!ns || !hf) return;
    switch (flag) {
	case NIRT_ALL:
	    ns->h_state = hf;
	    break;
	case NIRT_OUT:
	    ns->h_out = hf;
	    break;
	case NIRT_ERR:
	    ns->h_err = hf;
	    break;
	case NIRT_SEGS:
	    ns->h_segs = hf;
	    break;
	case NIRT_OBJS:
	    ns->h_objs = hf;
	    break;
	case NIRT_FRMTS:
	    ns->h_frmts = hf;
	    break;
	case NIRT_VIEW:
	    ns->h_view = hf;
	    break;
	default:
	    return;
    }
}

#define NCFC(nf) ((flags & nf) && !all_set) || (all_set &&  !((flags & nf)))

void
nirt_clear(NIRT *ns, int flags)
{
    int all_set;
    if (!ns) return;
    all_set = (flags & NIRT_ALL) ? 1 : 0;

    if (NCFC(NIRT_OUT)) {
	bu_vls_trunc(ns->out, 0);
    }

    if (NCFC(NIRT_ERR)) {
	bu_vls_trunc(ns->err, 0);
    }

    if (NCFC(NIRT_SEGS)) {
    }

    if (NCFC(NIRT_OBJS)) {
    }

    if (NCFC(NIRT_FRMTS)) {
    }

    if (NCFC(NIRT_VIEW)) {
    }

    /* Standard outputs handled - now, if NIRT_ALL was
     * supplied, clear any internal state. */
    if (flags == NIRT_ALL) {
    }

}

void
nirt_log(struct bu_vls *o, NIRT *ns, int output_type)
{
    if (!o || !ns) return;
    switch (output_type) {
	case NIRT_ALL:
	    bu_vls_sprintf(o, "%s", "NIRT_ALL: TODO\n");
	    break;
	case NIRT_OUT:
	    bu_vls_strcpy(o, bu_vls_addr(ns->out));
	    break;
	case NIRT_ERR:
	    bu_vls_strcpy(o, bu_vls_addr(ns->err));
	    break;
	case NIRT_SEGS:
	    bu_vls_sprintf(o, "%s", "NIRT_SEGS: TODO\n");
	    break;
	case NIRT_OBJS:
	    bu_vls_sprintf(o, "%s", "NIRT_OBJS: TODO\n");
	    break;
	case NIRT_FRMTS:
	    bu_vls_sprintf(o, "%s", "NIRT_FRMTS: TODO\n");
	    break;
	case NIRT_VIEW:
	    bu_vls_sprintf(o, "%s", "NIRT_VIEW: TODO\n");
	    break;
	default:
	    return;
    }

}

int
nirt_help(struct bu_vls *h, NIRT *ns,  bu_opt_format_t UNUSED(output_type))
{
    if (!h || !ns) return -1;
    return 0;
}



// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
