/*                        D I F F . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
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
/** @file nirt.cpp
 *
 * Implementation of Natalie's Interactive Ray-Tracer (NIRT)
 * functionality specific to the diff subcommand.
 *
 */

/* BRL-CAD includes */
#include "common.h"

#include "./nirt.h"


#define SD_INIT(sd, l, r) \
    do {BU_GET(sd, struct nirt_seg_diff); sd->left = left; sd->right = right;} \
    while (0)

#define SD_FREE(sd) \
    do {BU_PUT(sd, struct nirt_seg_diff);} \
    while (0)


static struct nirt_seg_diff *
_nirt_partition_diff(struct nirt_state *nss, struct nirt_seg *left, struct nirt_seg *right)
{
    int have_diff = 0;
    struct nirt_seg_diff *sd;
    if (!nss || !nss->i->diff_settings) return NULL;
    fastf_t in_delta = DIST_PNT_PNT(left->in, right->in);
    fastf_t out_delta = DIST_PNT_PNT(left->in, right->in);
    fastf_t los_delta = fabs(left->los - right->los);
    fastf_t scaled_los_delta = fabs(left->scaled_los - right->scaled_los);
    fastf_t obliq_in_delta = fabs(left->obliq_in - right->obliq_in);
    fastf_t obliq_out_delta = fabs(left->obliq_out - right->obliq_out);
    if (bu_vls_strcmp(left->reg_name, right->reg_name)) have_diff = 1;
    if (bu_vls_strcmp(left->path_name, right->path_name)) have_diff = 1;
    if (left->reg_id != right->reg_id) have_diff = 1;
    if (in_delta > nss->i->diff_settings->dist_delta_tol) have_diff = 1;
    if (out_delta > nss->i->diff_settings->dist_delta_tol) have_diff = 1;
    if (los_delta > nss->i->diff_settings->los_delta_tol) have_diff = 1;
    if (scaled_los_delta > nss->i->diff_settings->scaled_los_delta_tol) have_diff = 1;
    if (obliq_in_delta > nss->i->diff_settings->obliq_delta_tol) have_diff = 1;
    if (obliq_out_delta > nss->i->diff_settings->obliq_delta_tol) have_diff = 1;
    if (have_diff) {
	SD_INIT(sd, left, right);
	sd->in_delta = in_delta;
	sd->out_delta = out_delta;
	sd->los_delta = los_delta;
	sd->scaled_los_delta = scaled_los_delta;
	sd->obliq_in_delta = obliq_in_delta;
	sd->obliq_out_delta = obliq_out_delta;
	return sd;
    }
    return NULL; 
}

static struct nirt_seg_diff *
_nirt_overlap_diff(struct nirt_state *nss, struct nirt_seg *left, struct nirt_seg *right)
{
    int have_diff = 0;
    struct nirt_seg_diff *sd;
    if (!nss || !nss->i->diff_settings) return NULL;
    fastf_t ov_in_delta = DIST_PNT_PNT(left->ov_in, right->ov_in);
    fastf_t ov_out_delta = DIST_PNT_PNT(left->ov_out, right->ov_out);
    fastf_t ov_los_delta = fabs(left->ov_los - right->ov_los);
    if (bu_vls_strcmp(left->ov_reg1_name, right->ov_reg1_name)) have_diff = 1;
    if (bu_vls_strcmp(left->ov_reg2_name, right->ov_reg2_name)) have_diff = 1;
    if (left->ov_reg1_id != right->ov_reg1_id) have_diff = 1;
    if (left->ov_reg2_id != right->ov_reg2_id) have_diff = 1;
    if (ov_in_delta > nss->i->diff_settings->dist_delta_tol) have_diff = 1;
    if (ov_out_delta > nss->i->diff_settings->dist_delta_tol) have_diff = 1;
    if (ov_los_delta > nss->i->diff_settings->los_delta_tol) have_diff = 1;
    if (have_diff) {
	SD_INIT(sd, left, right);
	sd->ov_in_delta = ov_in_delta;
	sd->ov_out_delta = ov_out_delta;
	sd->ov_los_delta = ov_los_delta;
	return sd;
    }
    return NULL; 
}

static struct nirt_seg_diff *
_nirt_gap_diff(struct nirt_state *nss, struct nirt_seg *left, struct nirt_seg *right)
{
    int have_diff = 0;
    struct nirt_seg_diff *sd;
    if (!nss || !nss->i->diff_settings) return NULL;
    fastf_t gap_in_delta = DIST_PNT_PNT(left->gap_in, right->gap_in);
    fastf_t gap_los_delta = fabs(left->gap_los - right->gap_los);
    if (gap_in_delta > nss->i->diff_settings->dist_delta_tol) have_diff = 1;
    if (gap_los_delta > nss->i->diff_settings->los_delta_tol) have_diff = 1;
    if (have_diff) {
	SD_INIT(sd, left, right);
	sd->gap_in_delta = gap_in_delta;
	sd->gap_los_delta = gap_los_delta;
	return sd;
    }
    return 0;
}

static struct nirt_seg_diff *
_nirt_segs_diff(struct nirt_state *nss, struct nirt_seg *left, struct nirt_seg *right)
{
    struct nirt_seg_diff *sd;
    if (!nss) return NULL;
    if (!left || !right || left->type != right->type) {
	/* Fundamental segment difference - no point going further, they're different */
	SD_INIT(sd, left, right);
	return sd;
    }
    switch(left->type) {
	case NIRT_MISS_SEG:
	    /* Types don't differ and they're both misses - we're good */
	    return NULL;
	case NIRT_PARTITION_SEG:
	    return _nirt_partition_diff(nss, left, right);
	case NIRT_GAP_SEG:
	    return _nirt_gap_diff(nss, left, right);
	case NIRT_OVERLAP_SEG:
	    return _nirt_overlap_diff(nss, left, right);
	default:
	    nerr(nss, "NIRT diff error: unknown segment type: %d\n", left->type);
	    return 0;
    }
}

static const char *
_nirt_seg_string(int type) {
    static const char *p = "Partition";
    static const char *g = "Gap";
    static const char *o = "Overlap";
    static const char *m = "Miss";
    switch (type) {
	case NIRT_MISS_SEG:
	    return m;
	case NIRT_PARTITION_SEG:
	    return p;
	case NIRT_GAP_SEG:
	    return g;
	case NIRT_OVERLAP_SEG:
	    return o;
	default:
	    return NULL;
    }
}

static int
_nirt_diff_report(struct nirt_state *nss)
 {
    int reporting_diff = 0;
    int did_header = 0;
    struct bu_vls dreport = BU_VLS_INIT_ZERO;

    if (!nss || nss->i->diffs.size() == 0) return 0;
    std::vector<struct nirt_diff *> *dfs = &(nss->i->diffs); 
    for (size_t i = 0; i < dfs->size(); i++) {
	struct nirt_diff *d = (*dfs)[i];
	// Calculate diffs according to settings.  TODO - need to be more sophisticated about this - if a
	// segment is "missing" but the rays otherwise match, don't propagate the "offset" and report all
	// of the subsequent segments as different...
	size_t seg_max = (d->old_segs.size() > d->new_segs.size()) ? d->new_segs.size() : d->old_segs.size();
	for (size_t j = 0; j < seg_max; j++) {
	    struct nirt_seg_diff *sd = _nirt_segs_diff(nss, d->old_segs[j], d->new_segs[j]);
	    if (sd) d->diffs.push_back(sd);
	}
	if (d->diffs.size() > 0 && !did_header) {
	    bu_vls_printf(&dreport, "Diff Results (%s):\n", bu_vls_addr(nss->i->diff_file));
	    did_header = 1;
	}

	if (d->diffs.size() > 0) bu_vls_printf(&dreport, "Found differences along Ray:\n  xyz %.17f %.17f %.17f\n  dir %.17f %.17f %.17f\n \n", V3ARGS(d->orig), V3ARGS(d->dir));
	for (size_t j = 0; j < d->diffs.size(); j++) {
	    struct nirt_seg_diff *sd = d->diffs[j];
	    struct nirt_seg *left = sd->left;
	    struct nirt_seg *right = sd->right;
	    if (left->type != right->type) {
		bu_vls_printf(&dreport, "  Segment %zu type mismatch : Original %s, Current %s\n", j, _nirt_seg_string(sd->left->type), _nirt_seg_string(sd->right->type));
		nout(nss, "%s", bu_vls_addr(&dreport));
		bu_vls_free(&dreport);
		return 1;
	    }
	    switch (sd->left->type) {
		case NIRT_MISS_SEG:
		    if (!nss->i->diff_settings->report_misses) continue;
		    /* TODO */
		    break;
		case NIRT_PARTITION_SEG:
		    if (!nss->i->diff_settings->report_partitions) continue;
		    bu_vls_printf(&dreport, "  Segment difference(%s)[%zu]:\n", _nirt_seg_string(sd->left->type), j);
		    if (bu_vls_strcmp(left->reg_name, right->reg_name) && nss->i->diff_settings->report_partition_reg_names) {
			bu_vls_printf(&dreport, "    Region Name: '%s' -> '%s'\n", bu_vls_addr(left->reg_name), bu_vls_addr(right->reg_name));
			reporting_diff = 1;
		    }
		    if (bu_vls_strcmp(left->path_name, right->path_name) && nss->i->diff_settings->report_partition_path_names) {
			bu_vls_printf(&dreport, "    Path Name: '%s' -> '%s'\n", bu_vls_addr(left->path_name), bu_vls_addr(right->path_name));
			reporting_diff = 1;
		    }
		    if (left->reg_id != right->reg_id && nss->i->diff_settings->report_partition_reg_ids) {
			bu_vls_printf(&dreport, "    Region ID: %d -> %d\n", left->reg_id, right->reg_id);
			reporting_diff = 1;
		    }
		    if (sd->in_delta > nss->i->diff_settings->dist_delta_tol && nss->i->diff_settings->report_partition_dists) {
			std::string oval = _nirt_dbl_to_str(sd->in_delta, _nirt_digits(nss->i->diff_settings->dist_delta_tol));
			bu_vls_printf(&dreport, "    DIST_PNT_PNT(in_old,in_new): %s\n", oval.c_str());
			reporting_diff = 1;
		    }
		    if (sd->out_delta > nss->i->diff_settings->dist_delta_tol && nss->i->diff_settings->report_partition_dists) {
			std::string oval = _nirt_dbl_to_str(sd->out_delta, _nirt_digits(nss->i->diff_settings->dist_delta_tol));
			bu_vls_printf(&dreport, "    DIST_PNT_PNT(out_old,out_new): %s\n", oval.c_str());
			reporting_diff = 1;
		    }
		    if (sd->los_delta > nss->i->diff_settings->los_delta_tol && nss->i->diff_settings->report_partition_dists) {
			std::string oval = _nirt_dbl_to_str(sd->los_delta, _nirt_digits(nss->i->diff_settings->los_delta_tol));
			bu_vls_printf(&dreport, "    Line-Of-Sight(LOS) %s: %s\n", delta_str, oval.c_str());
			reporting_diff = 1;
		    }
		    if (sd->scaled_los_delta > nss->i->diff_settings->scaled_los_delta_tol && nss->i->diff_settings->report_partition_dists) {
			std::string oval = _nirt_dbl_to_str(sd->scaled_los_delta, _nirt_digits(nss->i->diff_settings->los_delta_tol));
			bu_vls_printf(&dreport, "    Scaled Line-Of-Sight %s: %s\n", delta_str, oval.c_str());
			reporting_diff = 1;
		    }
		    if (sd->obliq_in_delta > nss->i->diff_settings->obliq_delta_tol && nss->i->diff_settings->report_partition_obliq) {
			std::string oval = _nirt_dbl_to_str(sd->obliq_in_delta, _nirt_digits(nss->i->diff_settings->obliq_delta_tol));
			bu_vls_printf(&dreport, "    Input Normal Obliquity %s: %s\n", delta_str, oval.c_str());
			reporting_diff = 1;
		    }
		    if (sd->obliq_out_delta > nss->i->diff_settings->obliq_delta_tol && nss->i->diff_settings->report_partition_obliq) {
			std::string oval = _nirt_dbl_to_str(sd->obliq_out_delta, _nirt_digits(nss->i->diff_settings->obliq_delta_tol));
			bu_vls_printf(&dreport, "    Output Normal Obliquity %s: %s\n", delta_str, oval.c_str());
			reporting_diff = 1;
		    }
		    break;
		case NIRT_GAP_SEG:
		    if (!nss->i->diff_settings->report_gaps) continue;
		    bu_vls_printf(&dreport, "  Segment difference(%s)[%zu]:\n", _nirt_seg_string(sd->left->type), j);
		    if (sd->gap_in_delta > nss->i->diff_settings->dist_delta_tol && nss->i->diff_settings->report_gap_dists) {
			std::string oval = _nirt_dbl_to_str(sd->gap_in_delta, _nirt_digits(nss->i->diff_settings->dist_delta_tol));
			bu_vls_printf(&dreport, "    DIST_PNT_PNT(gap_in_old,gap_in_new): %s\n", oval.c_str());
			reporting_diff = 1;
		    }
		    if (sd->gap_los_delta > nss->i->diff_settings->los_delta_tol && nss->i->diff_settings->report_gap_dists) {
			std::string oval = _nirt_dbl_to_str(sd->gap_los_delta, _nirt_digits(nss->i->diff_settings->los_delta_tol));
			bu_vls_printf(&dreport, "    Gap Line-Of-Sight (LOS) %s: %s\n", delta_str, oval.c_str());
			reporting_diff = 1;
		    }
		    break;
		case NIRT_OVERLAP_SEG:
		    if (!nss->i->diff_settings->report_overlaps) continue;
		    bu_vls_printf(&dreport, "  Segment difference(%s)[%zu]:\n", _nirt_seg_string(sd->left->type), j);
		    if (bu_vls_strcmp(left->ov_reg1_name, right->ov_reg1_name) && nss->i->diff_settings->report_overlap_reg_names) {
			bu_vls_printf(&dreport, "    Region 1 Name: '%s' -> '%s'\n", bu_vls_addr(left->ov_reg1_name), bu_vls_addr(right->ov_reg1_name));
			reporting_diff = 1;
		    }
		    if (bu_vls_strcmp(left->ov_reg2_name, right->ov_reg2_name) && nss->i->diff_settings->report_overlap_reg_names) {
			bu_vls_printf(&dreport, "    Region 2 Name: '%s' -> '%s'\n", bu_vls_addr(left->ov_reg2_name), bu_vls_addr(right->ov_reg2_name));
			reporting_diff = 1;
		    }
		    if (left->ov_reg1_id != right->ov_reg1_id && nss->i->diff_settings->report_overlap_reg_ids) {
			bu_vls_printf(&dreport, "    Region 1 ID: %d -> %d\n", left->ov_reg1_id, right->ov_reg1_id);
			reporting_diff = 1;
		    }
		    if (left->ov_reg2_id != right->ov_reg2_id && nss->i->diff_settings->report_overlap_reg_ids) {
			bu_vls_printf(&dreport, "    Region 2 ID: %d -> %d\n", left->ov_reg2_id, right->ov_reg2_id);
			reporting_diff = 1;
		    }
		    if (sd->ov_in_delta > nss->i->diff_settings->dist_delta_tol && nss->i->diff_settings->report_overlap_dists) {
			std::string oval = _nirt_dbl_to_str(sd->ov_in_delta, _nirt_digits(nss->i->diff_settings->dist_delta_tol));
			bu_vls_printf(&dreport, "    DIST_PNT_PNT(ov_in_old, ov_in_new): %s\n", oval.c_str());
			reporting_diff = 1;
		    }
		    if (sd->ov_out_delta > nss->i->diff_settings->dist_delta_tol && nss->i->diff_settings->report_overlap_dists) {
			std::string oval = _nirt_dbl_to_str(sd->ov_out_delta, _nirt_digits(nss->i->diff_settings->dist_delta_tol));
			bu_vls_printf(&dreport, "    DIST_PNT_PNT(ov_out_old, ov_out_new): %s\n", oval.c_str());
			reporting_diff = 1;
		    }
		    if (sd->ov_los_delta > nss->i->diff_settings->los_delta_tol && nss->i->diff_settings->report_overlap_dists) {
			std::string oval = _nirt_dbl_to_str(sd->ov_los_delta, _nirt_digits(nss->i->diff_settings->los_delta_tol));
			bu_vls_printf(&dreport, "    Overlap Line-Of-Sight (LOS) %s: %s\n", delta_str, oval.c_str());
			reporting_diff = 1;
		    }
		    break;
		default:
		    nerr(nss, "NIRT diff error: unknown segment type: %d\n", left->type);
		    return 0;
	    }
	} 
	for (size_t j = 0; j < d->diffs.size(); j++) {
	    SD_FREE(d->diffs[j]);
	}
	d->diffs.clear();
    }

    if (reporting_diff) {
	nout(nss, "%s", bu_vls_addr(&dreport));
    } else {
	nout(nss, "No differences found\n");
    }
    bu_vls_free(&dreport);

    return reporting_diff;
}

//#define NIRT_DIFF_DEBUG 1

extern "C" int
_nirt_cmd_diff(void *ns, int argc, const char *argv[])
{
    if (!ns) return -1;
    struct nirt_state *nss = (struct nirt_state *)ns;
    int ac = 0;
    int print_help = 0;
    double tol = 0;
    int have_ray = 0;
    size_t cmt = std::string::npos;
    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    struct bu_opt_desc d[3];
    // TODO - add reporting options for enabling/disabling region/path, partition length, normal, and overlap ordering diffs.
    // For example, if r1 and r2 have the same shape in space, we may want to suppress name based differences and look at
    // other aspects of the shotline.  Also need sorting options for output - max partition diff first, max normal diff first,
    // max numerical delta, etc...
    BU_OPT(d[0],  "h", "help",       "",             NULL,   &print_help, "print help and exit");
    BU_OPT(d[1],  "t", "tol",   "<val>",  &bu_opt_fastf_t,   &tol,        "set diff tolerance");
    BU_OPT_NULL(d[2]);
    const char *ustr = "Usage: diff [opts] shotfile";

    argv++; argc--;

    if ((ac = bu_opt_parse(&optparse_msg, argc, (const char **)argv, d)) == -1) {
	char *help = bu_opt_describe(d, NULL);
	nerr(nss, "Error: bu_opt value read failure: %s\n\n%s\n%s\n", bu_vls_addr(&optparse_msg), ustr, help);
	if (help) bu_free(help, "help str");
	bu_vls_free(&optparse_msg);
	return -1;
    }
    bu_vls_free(&optparse_msg);

    if (print_help || !argv[0]) {
	char *help = bu_opt_describe(d, NULL);
	nerr(nss, "%s\n%s", ustr, help);
	if (help) bu_free(help, "help str");
	return -1;
    }

    if (BU_STR_EQUAL(argv[0], "load")) {
	ac--; argv++;
	if (ac != 1) {
	    nerr(nss, "Please specify a NIRT diff file to load.\n");
	    return -1;
	}

	if (nss->i->diff_ready) {
	    nerr(nss, "Diff file already loaded.  To reset, run \"diff clear\"\n");
	    return -1;
	}

	std::string line;
	std::ifstream ifs;
	ifs.open(argv[0]);
	if (!ifs.is_open()) {
	    nerr(nss, "Error: could not open file %s\n", argv[0]);
	    return -1;
	}

	if (nss->i->need_reprep) {
	    /* If we need to (re)prep, do it now. Failure is an error. */
	    if (_nirt_raytrace_prep(nss)) {
		nerr(nss, "Error: raytrace prep failed!\n");
		return -1;
	    }
	} else {
	    /* Based on current settings, tell the ap which rtip to use */
	    nss->i->ap->a_rt_i = _nirt_get_rtip(nss);
	    nss->i->ap->a_resource = _nirt_get_resource(nss);
	}

	bu_vls_sprintf(nss->i->diff_file, "%s", argv[0]);
	nss->i->diff_run = 1;
	have_ray = 0;
	while (std::getline(ifs, line)) {
	    struct nirt_diff *df;

	    /* If part of the line is commented, skip that part */
	    cmt = _nirt_find_first_unescaped(line, "#", 0);
	    if (cmt != std::string::npos) {
		line.erase(cmt);
	    }

	    /* If the whole line was a comment, skip it */
	    _nirt_trim_whitespace(line);
	    if (!line.length()) continue;

	    /* Not a comment - has to be a valid type, or it's an error */
	    int ltype = -1;
	    ltype = (ltype < 0 && !line.compare(0, 4, "RAY,")) ? 0 : ltype;
	    ltype = (ltype < 0 && !line.compare(0, 4, "HIT,")) ? 1 : ltype;
	    ltype = (ltype < 0 && !line.compare(0, 4, "GAP,")) ? 2 : ltype;
	    ltype = (ltype < 0 && !line.compare(0, 5, "MISS,")) ? 3 : ltype;
	    ltype = (ltype < 0 && !line.compare(0, 8, "OVERLAP,")) ? 4 : ltype;
	    if (ltype < 0) {
		nerr(nss, "Error processing ray line \"%s\"!\nUnknown line type\n", line.c_str());
		return -1;
	    }

	    /* Ray */
	    if (ltype == 0) {
		if (have_ray) {
#ifdef NIRT_DIFF_DEBUG
		    bu_log("\n\nanother ray!\n\n\n");
#endif
		    // Have ray already - execute current ray, store results in
		    // diff database (if any diffs were found), then clear expected
		    // results and old ray
		    for (int i = 0; i < 3; ++i) {
			nss->i->ap->a_ray.r_pt[i] = nss->i->vals->orig[i];
			nss->i->ap->a_ray.r_dir[i] = nss->i->vals->dir[i];
		    }
		    // TODO - rethink this container...
		    _nirt_init_ovlp(nss);
		    (void)rt_shootray(nss->i->ap);
		    nss->i->diffs.push_back(nss->i->cdiff);
		}
		// Read ray
		df = new struct nirt_diff;
		// TODO - once we go to C++11, used std::regex_search and std::smatch to more flexibly get a substring
		std::string rstr = line.substr(7);
		have_ray = 1;
		std::vector<std::string> substrs = _nirt_string_split(rstr);
		if (substrs.size() != 6) {
		    nerr(nss, "Error processing ray line \"%s\"!\nExpected 6 elements, found %zu\n", line.c_str(), substrs.size());
		    delete df;
		    return -1;
		}
		VSET(df->orig, _nirt_str_to_dbl(substrs[0], 0), _nirt_str_to_dbl(substrs[1], 0), _nirt_str_to_dbl(substrs[2], 0));
		VSET(df->dir, _nirt_str_to_dbl(substrs[3], 0), _nirt_str_to_dbl(substrs[4], 0), _nirt_str_to_dbl(substrs[5], 0));
		VMOVE(nss->i->vals->dir, df->dir);
		VMOVE(nss->i->vals->orig, df->orig);
#ifdef NIRT_DIFF_DEBUG
		bu_log("Found RAY:\n");
		bu_log("origin   : %0.17f, %0.17f, %0.17f\n", V3ARGS(df->orig));
		bu_log("direction: %0.17f, %0.17f, %0.17f\n", V3ARGS(df->dir));
#endif
		_nirt_targ2grid(nss);
		_nirt_dir2ae(nss);
		nss->i->cdiff = df;
		continue;
	    }

	    /* Hit */
	    if (ltype == 1) {
		if (!have_ray) {
		    nerr(nss, "Error: Hit line found but no ray set.\n");
		    return -1;
		}

		// TODO - once we go to C++11, used std::regex_search and std::smatch to more flexibly get a substring
		std::string hstr = line.substr(7);
		std::vector<std::string> substrs = _nirt_string_split(hstr);
		if (substrs.size() != 15) {
		    nerr(nss, "Error processing hit line \"%s\"!\nExpected 15 elements, found %zu\n", hstr.c_str(), substrs.size());
		    return -1;
		}
		struct nirt_seg *seg;
		_nirt_seg_init(&seg);
		seg->type = NIRT_PARTITION_SEG;
		bu_vls_decode(seg->reg_name, substrs[0].c_str());
		//bu_vls_printf(seg->reg_name, "%s", substrs[0].c_str());
		bu_vls_decode(seg->path_name, substrs[1].c_str());
		seg->reg_id = _nirt_str_to_int(substrs[2]);
		VSET(seg->in, _nirt_str_to_dbl(substrs[3], 0), _nirt_str_to_dbl(substrs[4], 0), _nirt_str_to_dbl(substrs[5], 0));
		seg->d_in = _nirt_str_to_dbl(substrs[6], 0);
		VSET(seg->out, _nirt_str_to_dbl(substrs[7], 0), _nirt_str_to_dbl(substrs[8], 0), _nirt_str_to_dbl(substrs[9], 0));
		seg->d_out = _nirt_str_to_dbl(substrs[10], 0);
		seg->los = _nirt_str_to_dbl(substrs[11], 0);
		seg->scaled_los = _nirt_str_to_dbl(substrs[12], 0);
		seg->obliq_in = _nirt_str_to_dbl(substrs[13], 0);
		seg->obliq_out = _nirt_str_to_dbl(substrs[14], 0);
#ifdef NIRT_DIFF_DEBUG
		bu_log("Found %s:\n", line.c_str());
		bu_log("  reg_name: %s\n", bu_vls_addr(seg->reg_name));
		bu_log("  path_name: %s\n", bu_vls_addr(seg->path_name));
		bu_log("  reg_id: %d\n", seg->reg_id);
		bu_log("  in: %0.17f, %0.17f, %0.17f\n", V3ARGS(seg->in));
		bu_log("  out: %0.17f, %0.17f, %0.17f\n", V3ARGS(seg->out));
		bu_log("  d_in: %0.17f d_out: %0.17f\n", seg->d_in, seg->d_out);
		bu_log("  los: %0.17f  scaled_los: %0.17f\n", seg->los, seg->scaled_los);
		bu_log("  obliq_in: %0.17f  obliq_out: %0.17f\n", seg->obliq_in, seg->obliq_out);
#endif
		nss->i->cdiff->old_segs.push_back(seg);
		continue;
	    }

	    /* Gap */
	    if (ltype == 2) {
		if (!have_ray) {
		    nerr(nss, "Error: Gap line found but no ray set.\n");
		    return -1;
		}

		// TODO - once we go to C++11, used std::regex_search and std::smatch to more flexibly get a substring
		std::string gstr = line.substr(7);
		std::vector<std::string> substrs = _nirt_string_split(gstr);
		if (substrs.size() != 7) {
		    nerr(nss, "Error processing gap line \"%s\"!\nExpected 7 elements, found %zu\n", gstr.c_str(), substrs.size());
		    return -1;
		}
		struct nirt_seg *seg;
		_nirt_seg_init(&seg);
		seg->type = NIRT_GAP_SEG;
		VSET(seg->gap_in, _nirt_str_to_dbl(substrs[0], 0), _nirt_str_to_dbl(substrs[1], 0), _nirt_str_to_dbl(substrs[2], 0));
		VSET(seg->in, _nirt_str_to_dbl(substrs[3], 0), _nirt_str_to_dbl(substrs[4], 0), _nirt_str_to_dbl(substrs[5], 0));
		seg->gap_los = _nirt_str_to_dbl(substrs[6], 0);
#ifdef NIRT_DIFF_DEBUG
		bu_log("Found %s:\n", line.c_str());
		bu_log("  in: %0.17f, %0.17f, %0.17f\n", V3ARGS(seg->gap_in));
		bu_log("  out: %0.17f, %0.17f, %0.17f\n", V3ARGS(seg->in));
		bu_log("  gap_los: %0.17f\n", seg->gap_los);
#endif
		nss->i->cdiff->old_segs.push_back(seg);
		continue;
	    }

	    /* Miss */
	    if (ltype == 3) {
		if (!have_ray) {
		    nerr(nss, "Error: Miss line found but no ray set.\n");
		    return -1;
		}

		struct nirt_seg *seg;
		_nirt_seg_init(&seg);
		seg->type = NIRT_MISS_SEG;
#ifdef NIRT_DIFF_DEBUG
		bu_log("Found MISS\n");
#endif
		have_ray = 0;
		nss->i->cdiff->old_segs.push_back(seg);
		continue;
	    }

	    /* Overlap */
	    if (ltype == 4) {
		if (!have_ray) {
		    nerr(nss, "Error: Overlap line found but no ray set.\n");
		    return -1;
		}

		// TODO - once we go to C++11, used std::regex_search and std::smatch to more flexibly get a substring
		std::string ostr = line.substr(11);
		std::vector<std::string> substrs = _nirt_string_split(ostr);
		if (substrs.size() != 11) {
		    nerr(nss, "Error processing overlap line \"%s\"!\nExpected 11 elements, found %zu\n", ostr.c_str(), substrs.size());
		    return -1;
		}
		struct nirt_seg *seg;
		_nirt_seg_init(&seg);
		seg->type = NIRT_OVERLAP_SEG;
		bu_vls_decode(seg->ov_reg1_name, substrs[0].c_str());
		bu_vls_decode(seg->ov_reg2_name, substrs[1].c_str());
		seg->ov_reg1_id = _nirt_str_to_int(substrs[2]);
		seg->ov_reg2_id = _nirt_str_to_int(substrs[3]);
		VSET(seg->ov_in, _nirt_str_to_dbl(substrs[4], 0), _nirt_str_to_dbl(substrs[5], 0), _nirt_str_to_dbl(substrs[6], 0));
		VSET(seg->ov_out, _nirt_str_to_dbl(substrs[7], 0), _nirt_str_to_dbl(substrs[8], 0), _nirt_str_to_dbl(substrs[9], 0));
		seg->ov_los = _nirt_str_to_dbl(substrs[10], 0);
#ifdef NIRT_DIFF_DEBUG
		bu_log("Found %s:\n", line.c_str());
		bu_log("  ov_reg1_name: %s\n", bu_vls_addr(seg->ov_reg1_name));
		bu_log("  ov_reg2_name: %s\n", bu_vls_addr(seg->ov_reg2_name));
		bu_log("  ov_reg1_id: %d\n", seg->ov_reg1_id);
		bu_log("  ov_reg2_id: %d\n", seg->ov_reg2_id);
		bu_log("  ov_in: %0.17f, %0.17f, %0.17f\n", V3ARGS(seg->ov_in));
		bu_log("  ov_out: %0.17f, %0.17f, %0.17f\n", V3ARGS(seg->ov_out));
		bu_log("  ov_los: %0.17f\n", seg->ov_los);
#endif
		nss->i->cdiff->old_segs.push_back(seg);
		continue;
	    }
#ifdef NIRT_DIFF_DEBUG
	    nerr(nss, "Warning - unknown line type, skipping: %s\n", line.c_str());
#endif
	}

	if (have_ray) {
	    // Execute work.
	    for (int i = 0; i < 3; ++i) {
		nss->i->ap->a_ray.r_pt[i] = nss->i->vals->orig[i];
		nss->i->ap->a_ray.r_dir[i] = nss->i->vals->dir[i];
	    }
	    // TODO - rethink this container...
	    _nirt_init_ovlp(nss);
	    (void)rt_shootray(nss->i->ap);
	    nss->i->diffs.push_back(nss->i->cdiff);

	    // Thought - if we have rays but no pre-defined output, write out the
	    // expected output to stdout - in this mode diff will generate a diff
	    // input file from a supplied list of rays.
	}

	ifs.close();

	// Done with if_hit and friends
	nss->i->cdiff = NULL;
	nss->i->diff_run = 0;
	nss->i->diff_ready = 1;
	return 0;
    }

    if (BU_STR_EQUAL(argv[0], "report")) {
	// Report diff results according to the NIRT diff settings.
	if (!nss->i->diff_ready) {
	    nerr(nss, "No diff file loaded - please load a diff file with \"diff load <filename>\"\n");
	    return -1;
	} else {
	    return (_nirt_diff_report(nss) >= 0) ? 0 : -1;
	}
    }

    if (BU_STR_EQUAL(argv[0], "clear")) {
	// need clear command to scrub old nss->i->diffs
	for (size_t i = 0; i < nss->i->diffs.size(); i++) {
	    struct nirt_diff *df = nss->i->diffs[i];
	    for (size_t j = 0; j < df->old_segs.size(); j++) {
		_nirt_seg_free(df->old_segs[j]);
	    }
	    for (size_t j = 0; j < df->new_segs.size(); j++) {
		_nirt_seg_free(df->new_segs[j]);
	    }
	    delete nss->i->diffs[i];
	}
	nss->i->diffs.clear();
	nss->i->diff_ready = 0;
	return 0;
    }

    if (BU_STR_EQUAL(argv[0], "settings")) {
	ac--; argv++;
	if (!ac) {
	    //print current settings
	    nout(nss, "report_partitions:           %d\n", nss->i->diff_settings->report_partitions);
	    nout(nss, "report_misses:               %d\n", nss->i->diff_settings->report_misses);
	    nout(nss, "report_gaps:                 %d\n", nss->i->diff_settings->report_gaps);
	    nout(nss, "report_overlaps:             %d\n", nss->i->diff_settings->report_overlaps);
	    nout(nss, "report_partition_reg_ids:    %d\n", nss->i->diff_settings->report_partition_reg_ids);
	    nout(nss, "report_partition_reg_names:  %d\n", nss->i->diff_settings->report_partition_reg_names);
	    nout(nss, "report_partition_path_names: %d\n", nss->i->diff_settings->report_partition_path_names);
	    nout(nss, "report_partition_dists:      %d\n", nss->i->diff_settings->report_partition_dists);
	    nout(nss, "report_partition_obliq:      %d\n", nss->i->diff_settings->report_partition_obliq);
	    nout(nss, "report_overlap_reg_names:    %d\n", nss->i->diff_settings->report_overlap_reg_names);
	    nout(nss, "report_overlap_reg_ids:      %d\n", nss->i->diff_settings->report_overlap_reg_ids);
	    nout(nss, "report_overlap_dists:        %d\n", nss->i->diff_settings->report_overlap_dists);
	    nout(nss, "report_overlap_obliq:        %d\n", nss->i->diff_settings->report_overlap_obliq);
	    nout(nss, "report_gap_dists:            %d\n", nss->i->diff_settings->report_gap_dists);
	    nout(nss, "dist_delta_tol:              %g\n", nss->i->diff_settings->dist_delta_tol);
	    nout(nss, "obliq_delta_tol:             %g\n", nss->i->diff_settings->obliq_delta_tol);
	    nout(nss, "los_delta_tol:               %g\n", nss->i->diff_settings->los_delta_tol);
	    nout(nss, "scaled_los_delta_tol:        %g\n", nss->i->diff_settings->scaled_los_delta_tol);
	    return 0;
	}
	if (ac == 1) {
	    //print specific setting
	    if (BU_STR_EQUAL(argv[0], "report_partitions"))           nout(nss, "%d\n", nss->i->diff_settings->report_partitions);
	    if (BU_STR_EQUAL(argv[0], "report_misses"))               nout(nss, "%d\n", nss->i->diff_settings->report_misses);
	    if (BU_STR_EQUAL(argv[0], "report_gaps"))                 nout(nss, "%d\n", nss->i->diff_settings->report_gaps);
	    if (BU_STR_EQUAL(argv[0], "report_overlaps"))             nout(nss, "%d\n", nss->i->diff_settings->report_overlaps);
	    if (BU_STR_EQUAL(argv[0], "report_partition_reg_ids"))    nout(nss, "%d\n", nss->i->diff_settings->report_partition_reg_ids);
	    if (BU_STR_EQUAL(argv[0], "report_partition_reg_names"))  nout(nss, "%d\n", nss->i->diff_settings->report_partition_reg_names);
	    if (BU_STR_EQUAL(argv[0], "report_partition_path_names")) nout(nss, "%d\n", nss->i->diff_settings->report_partition_path_names);
	    if (BU_STR_EQUAL(argv[0], "report_partition_dists"))      nout(nss, "%d\n", nss->i->diff_settings->report_partition_dists);
	    if (BU_STR_EQUAL(argv[0], "report_partition_obliq"))      nout(nss, "%d\n", nss->i->diff_settings->report_partition_obliq);
	    if (BU_STR_EQUAL(argv[0], "report_overlap_reg_names"))    nout(nss, "%d\n", nss->i->diff_settings->report_overlap_reg_names);
	    if (BU_STR_EQUAL(argv[0], "report_overlap_reg_ids"))      nout(nss, "%d\n", nss->i->diff_settings->report_overlap_reg_ids);
	    if (BU_STR_EQUAL(argv[0], "report_overlap_dists"))        nout(nss, "%d\n", nss->i->diff_settings->report_overlap_dists);
	    if (BU_STR_EQUAL(argv[0], "report_overlap_obliq"))        nout(nss, "%d\n", nss->i->diff_settings->report_overlap_obliq);
	    if (BU_STR_EQUAL(argv[0], "report_gap_dists"))            nout(nss, "%d\n", nss->i->diff_settings->report_gap_dists);
	    if (BU_STR_EQUAL(argv[0], "dist_delta_tol"))              nout(nss, "%g\n", nss->i->diff_settings->dist_delta_tol);
	    if (BU_STR_EQUAL(argv[0], "obliq_delta_tol"))             nout(nss, "%g\n", nss->i->diff_settings->obliq_delta_tol);
	    if (BU_STR_EQUAL(argv[0], "los_delta_tol"))               nout(nss, "%g\n", nss->i->diff_settings->los_delta_tol);
	    if (BU_STR_EQUAL(argv[0], "scaled_los_delta_tol"))        nout(nss, "%g\n", nss->i->diff_settings->scaled_los_delta_tol);
	    return 0;
	}
	if (ac == 2) {
	    //set setting
	    struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
	    int *setting_int = NULL;
	    fastf_t *setting_fastf_t = NULL;
	    if (BU_STR_EQUAL(argv[0], "report_partitions"))           setting_int = &(nss->i->diff_settings->report_partitions);
	    if (BU_STR_EQUAL(argv[0], "report_misses"))               setting_int = &(nss->i->diff_settings->report_misses);
	    if (BU_STR_EQUAL(argv[0], "report_gaps"))                 setting_int = &(nss->i->diff_settings->report_gaps);
	    if (BU_STR_EQUAL(argv[0], "report_overlaps"))             setting_int = &(nss->i->diff_settings->report_overlaps);
	    if (BU_STR_EQUAL(argv[0], "report_partition_reg_ids"))    setting_int = &(nss->i->diff_settings->report_partition_reg_ids);
	    if (BU_STR_EQUAL(argv[0], "report_partition_reg_names"))  setting_int = &(nss->i->diff_settings->report_partition_reg_names);
	    if (BU_STR_EQUAL(argv[0], "report_partition_path_names")) setting_int = &(nss->i->diff_settings->report_partition_path_names);
	    if (BU_STR_EQUAL(argv[0], "report_partition_dists"))      setting_int = &(nss->i->diff_settings->report_partition_dists);
	    if (BU_STR_EQUAL(argv[0], "report_partition_obliq"))      setting_int = &(nss->i->diff_settings->report_partition_obliq);
	    if (BU_STR_EQUAL(argv[0], "report_overlap_reg_names"))    setting_int = &(nss->i->diff_settings->report_overlap_reg_names);
	    if (BU_STR_EQUAL(argv[0], "report_overlap_reg_ids"))      setting_int = &(nss->i->diff_settings->report_overlap_reg_ids);
	    if (BU_STR_EQUAL(argv[0], "report_overlap_dists"))        setting_int = &(nss->i->diff_settings->report_overlap_dists);
	    if (BU_STR_EQUAL(argv[0], "report_overlap_obliq"))        setting_int = &(nss->i->diff_settings->report_overlap_obliq);
	    if (BU_STR_EQUAL(argv[0], "report_gap_dists"))            setting_int = &(nss->i->diff_settings->report_gap_dists);
	    if (BU_STR_EQUAL(argv[0], "dist_delta_tol"))              setting_fastf_t = &(nss->i->diff_settings->dist_delta_tol);
	    if (BU_STR_EQUAL(argv[0], "obliq_delta_tol"))             setting_fastf_t = &(nss->i->diff_settings->obliq_delta_tol);
	    if (BU_STR_EQUAL(argv[0], "los_delta_tol"))               setting_fastf_t = &(nss->i->diff_settings->los_delta_tol);
	    if (BU_STR_EQUAL(argv[0], "scaled_los_delta_tol"))        setting_fastf_t = &(nss->i->diff_settings->scaled_los_delta_tol);

	    if (setting_int) {
		if (bu_opt_int(&opt_msg, 1, (const char **)&argv[1], (void *)setting_int) == -1) {
		    nerr(nss, "Error: bu_opt value read failure: %s\n", bu_vls_addr(&opt_msg));
		    bu_vls_free(&opt_msg);
		    return -1;
		} else {
		    return 0;
		}
	    }
	    if (setting_fastf_t) {
		if (BU_STR_EQUAL(argv[1], "BN_TOL_DIST")) {
		    *setting_fastf_t = BN_TOL_DIST;
		    return 0;
		} else {
		    if (bu_opt_fastf_t(&opt_msg, 1, (const char **)&argv[1], (void *)setting_fastf_t) == -1) {
			nerr(nss, "Error: bu_opt value read failure: %s\n", bu_vls_addr(&opt_msg));
			bu_vls_free(&opt_msg);
			return -1;
		    } else {
			return 0;
		    }
		}
	    }

	    // anything else is an error
	    return -1;
	}
    }

    nerr(nss, "Unknown diff subcommand: \"%s\"\n", argv[0]);

    return -1;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
