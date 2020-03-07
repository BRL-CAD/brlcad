/*                          N I R T . H
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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
/** @file nirt.h
 *
 * Header containing internal definitions for Natalie's Interactive Raytracer
 * implementation.
 *
 */

/* BRL-CAD includes */
#include "common.h"

#include <algorithm>
#include <string>
#include <sstream>

/* needed on mac in c90 mode */
#ifndef HAVE_DECL_FSEEKO
extern "C" int fseeko(FILE *, off_t, int);
extern "C" off_t ftello(FILE *);
#endif
#include <fstream>
#include <iomanip>
#include <set>
#include <vector>
#include <limits>

//#define NIRT_USE_UNICODE 1
#ifdef NIRT_USE_UNICODE
#  define delta_str "Δ"
#else
#  define delta_str "Delta"
#endif

#include "bu/app.h"
#include "bu/color.h"
#include "bu/cmd.h"
#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/units.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "analyze.h"

#include "bu/debug.h"
#include "nmg/debug.h"
#include "rt/debug.h"

#include "./debug_cmd.c"

/* NIRT segment types */
#define NIRT_MISS_SEG      1    /**< @brief Ray segment representing a miss */
#define NIRT_PARTITION_SEG 2    /**< @brief Ray segment representing a solid region */
#define NIRT_OVERLAP_SEG   3    /**< @brief Ray segment representing an overlap region */
#define NIRT_GAP_SEG       4    /**< @brief Ray segment representing a gap */
#define NIRT_ALL_SEG       5    /**< @brief Enable all values (for print) */

#define NIRT_SILENT_UNSET    0
#define NIRT_SILENT_YES      1
#define NIRT_SILENT_NO       -1

#define NIRT_OVLP_RESOLVE            0
#define NIRT_OVLP_REBUILD_FASTGEN    1
#define NIRT_OVLP_REBUILD_ALL        2
#define NIRT_OVLP_RETAIN             3

#define NIRT_PRINTF_SPECIFIERS "difeEgGs"
#define NIRT_OUTPUT_TYPE_SPECIFIERS "rhpfmog"

HIDDEN const char *nirt_cmd_tbl_defs =
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
"gap_los,        FLOAT,         ,"
"nirt_cmd,       STRING,        ,";


struct nirt_overlap {
    struct application *ap;
    struct partition *pp;
    struct region *reg1;
    struct region *reg2;
    fastf_t in_dist;
    fastf_t out_dist;
    point_t in_point;
    point_t out_point;
    struct nirt_overlap *forw;
    struct nirt_overlap *backw;
};
#define NIRT_OVERLAP_NULL ((struct nirt_overlap *)0)

struct nirt_seg {
    int type;
    point_t in;
    fastf_t d_in;
    point_t out;
    fastf_t d_out;
    fastf_t los;
    fastf_t scaled_los;
    struct bu_vls *path_name;
    struct bu_vls *reg_name;
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
    struct bu_vls *ov_reg1_name;
    int ov_reg1_id;
    struct bu_vls *ov_reg2_name;
    int ov_reg2_id;
    struct bu_vls *ov_sol_in;
    struct bu_vls *ov_sol_out;
    fastf_t ov_los;
    point_t ov_in;
    fastf_t ov_d_in;
    point_t ov_out;
    fastf_t ov_d_out;
    int surf_num_in;
    int surf_num_out;
    int claimant_count;
    struct bu_vls *claimant_list;
    struct bu_vls *claimant_listn; /* uses \n instead of space to separate claimants */
    struct bu_vls *attributes;
    point_t gap_in;
    fastf_t gap_los;
};

/* This record structure doesn't have to correspond exactly to the above list
 * of available values, but it needs to retain sufficient information to
 * support the ability to generate all of them upon request. */
struct nirt_output_record {
    point_t orig;
    fastf_t h;
    fastf_t v;
    fastf_t d_orig;
    vect_t dir;
    fastf_t a;
    fastf_t e;
    struct nirt_overlap ovlp_list;
    struct nirt_seg *seg;
};


/**
 * Design thoughts for nirt diffing:
 *
 * NIRT difference events come in two primary forms: "transition" differences
 * in the form of entry/exit hits, and differences in segments - the regions
 * between transitions.  In the context of a diff, a segment by definition
 * contains no transitions on *either* shotline path.  Even if a segment
 * contains no transitions along its own shotline, if a transition from the
 * other shot falls within its bounds the original segment will be treated as
 * two sequential segments of the same type for the purposes of comparison.
 *
 * Transitions are compared only to other transitions, and segments are
 * compared only to other segments.
 *
 * The comparison criteria are as follows:
 *
 * Transition points:
 *
 * 1.  DIST_PNT_PNT - if there is a transition on either path that does not align
 *     within the specified distance tolerance with a transition on the other
 *     path, the transition is unmatched and reported as a difference.
 * 2.  Obliquity delta - if two transition points align within the distance
 *     tolerance, the next test is the difference between their normals. If
 *     these match in direction and obliquity angle, the transition points
 *     are deemed to be matching.  Otherwise, a difference is reported on the
 *     transition point.
 *
 * Segments:
 *
 * 1.  The first comparison made between segments is their type. Type
 *     differences will always be reported as a difference.
 *
 * 2.  If types match, behavior will depend on options and the specific
 *     types being compared:
 *
 *     GAPS:       always match.
 *
 *     PARTITIONS: Match if all active criteria match.  If no criteria
 *                 are active, match.  Possible active criteria:
 *
 *                 Region name
 *                 Path name
 *                 Region ID
 *
 *     OVERLAPS:   Match if all active criteria match.  If no criteria
 *                 are active, match.  Possible active criteria:
 *                 
 *                 Overlap region set
 *                 Overlap path set
 *                 Overlap region id set
 *                 Selected "winning" partition in the overlap
 */


struct nirt_seg_diff {
    struct nirt_seg *left;
    struct nirt_seg *right;
    fastf_t in_delta;
    fastf_t out_delta;
    fastf_t los_delta;
    fastf_t scaled_los_delta;
    fastf_t obliq_in_delta;
    fastf_t obliq_out_delta;
    fastf_t ov_in_delta;
    fastf_t ov_out_delta;
    fastf_t ov_los_delta;
    fastf_t gap_in_delta;
    fastf_t gap_los_delta;
};

struct nirt_diff {
    point_t orig;
    vect_t dir;
    std::vector<struct nirt_seg *> old_segs;
    std::vector<struct nirt_seg *> new_segs;
    std::vector<struct nirt_seg_diff *> diffs;
};

struct nirt_diff_settings {
    int report_partitions;
    int report_misses;
    int report_gaps;
    int report_overlaps;
    int report_partition_reg_ids;
    int report_partition_reg_names;
    int report_partition_path_names;
    int report_partition_dists;
    int report_partition_obliq;
    int report_overlap_reg_names;
    int report_overlap_reg_ids;
    int report_overlap_dists;
    int report_overlap_obliq;
    int report_gap_dists;
    fastf_t dist_delta_tol;
    fastf_t obliq_delta_tol;
    fastf_t los_delta_tol;
    fastf_t scaled_los_delta_tol;
};

struct nirt_state_impl {
    /* Output options */
    struct bu_color *hit_odd_color;
    struct bu_color *hit_even_color;
    struct bu_color *void_color;
    struct bu_color *overlap_color;
    int print_header;
    int print_ident_flag;

    /* Output */
    struct bu_vls *out;
    int out_accumulate;
    struct bu_vls *msg;
    struct bu_vls *err;
    int err_accumulate;
    struct bu_list s_vlist; /* used by the segs vlblock */
    struct bn_vlblock *segs;
    int plot_overlaps;
    //TODO - int segs_accumulate;
    int ret;  // return code to be returned by nirt_exec after execution

    /* Callbacks */
    nirt_hook_t h_state;   // any state change
    nirt_hook_t h_out;     // output
    nirt_hook_t h_msg;     // messages (not errors) not part of command output
    nirt_hook_t h_err;     // err changes
    nirt_hook_t h_segs;    // segement list is changed
    nirt_hook_t h_objs;    // active list of objects in scene changes
    nirt_hook_t h_frmts;   // output formatting is changed
    nirt_hook_t h_view;    // the camera view is changed


    /* state variables */
    double base2local;
    double local2base;
    int backout;
    int overlap_claims;
    int use_air;
    std::set<std::string> attrs;        // active attributes
    std::vector<std::string> active_paths; // active paths for raytracer
    struct nirt_output_record *vals;
    struct bu_vls *diff_file;
    struct nirt_diff *cdiff;
    std::vector<struct nirt_diff *> diffs;
    struct nirt_diff_settings *diff_settings;
    int diff_run;
    int diff_ready;

    /* state alteration flags */
    bool b_state;   // updated for any state change
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
    int need_reprep;

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

void nmsg(struct nirt_state *nss, const char *fmt, ...) _BU_ATTR_PRINTF23;
void nout(struct nirt_state *nss, const char *fmt, ...) _BU_ATTR_PRINTF23;
void nerr(struct nirt_state *nss, const char *fmt, ...) _BU_ATTR_PRINTF23;
void ndbg(struct nirt_state *nss, int flag, const char *fmt, ...);

size_t _nirt_find_first_unescaped(std::string &s, const char *keys, int offset);
size_t _nirt_find_first_unquoted(std::string &ts, const char *key, size_t offset);
void _nirt_trim_whitespace(std::string &s);
std::vector<std::string> _nirt_string_split(std::string s);

/* Based on tolerance, how many digits should we print? */
int _nirt_digits(fastf_t ftol);

/*
 * References for string to-from floating point issues:
 * https://stackoverflow.com/a/22458961
 * http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2006/n2005.pdf
 * https://docs.oracle.com/cd/E19957-01/806-3568/ncg_goldberg.html
 */
std::string _nirt_dbl_to_str(double d, size_t p);
double _nirt_str_to_dbl(std::string s, size_t p);

int _nirt_str_to_int(std::string s);

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
