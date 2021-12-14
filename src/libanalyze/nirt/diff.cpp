/*                        D I F F . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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

#include <map>
#include <regex>
#include <cmath>

#include "bu/cmd.h"

#include "./nirt.h"


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
 *                 TODO - Attributes?
 *
 *     OVERLAPS:   Match if all active criteria match.  If no criteria
 *                 are active, match.  Possible active criteria:
 *
 *                 Overlap region set
 *                 Overlap path set
 *                 Overlap region id set
 *                 Selected "winning" partition in the overlap
 */

typedef enum {
    NIRT_LEFT = 0,
    NIRT_RIGHT
} n_origin_t;

typedef enum {
    NIRT_T_IN = 0,
    NIRT_T_OUT
} n_transition_t;

typedef enum {
    NIRT_SEG_HIT = 0,
    NIRT_SEG_OVLP
} n_seg_t;

#define NIRT_DIFF_PNT_OBLIQ           0x1
#define NIRT_DIFF_PNT_TRANS_TYPE      0x2  /* Points are geometrically the same (within tolerance) but denote opposite state transitions */

#define NIRT_DIFF_OVLPS               0x1
#define NIRT_DIFF_SEG_REGNAME         0x2
#define NIRT_DIFF_SEG_PATHNAME        0x4
#define NIRT_DIFF_SEG_ID              0x8
#define NIRT_DIFF_SEG_OVLP_SELECTED   0x16

struct nirt_diff_state;

struct nirt_diff_ray_state {
    point_t orig;
    vect_t dir;
    std::vector<struct nirt_seg> old_segs;
    std::vector<struct nirt_seg> new_segs;
    struct nirt_diff_state *nds;
};

struct nirt_diff_state {

    struct bu_vls diff_file = BU_VLS_INIT_ZERO;
    struct nirt_diff_ray_state *cdiff = NULL;
    std::vector<struct nirt_diff_ray_state> rays;

    /* Diff element-specific settings */
    bool partitions = true;
    bool partition_reg_ids = true;
    bool partition_reg_names = true;
    bool partition_path_names = true;
    bool partition_obliq = true;
    bool overlaps = true;
    bool overlap_reg_names = true;
    bool overlap_reg_ids = true;
    bool overlap_obliq = true;

    /* Tolerances */
    fastf_t dist_tol = BN_TOL_DIST;
    fastf_t obliq_tol = BN_TOL_DIST;
    fastf_t los_tol = BN_TOL_DIST;
    fastf_t scaled_los_tol = BN_TOL_DIST;

    struct nirt_state *nss;
    bool run_complete = false;

    struct bu_opt_desc *d = NULL;
    const struct bu_cmdtab *cmds = NULL;
};

class half_segment {
    public:
	n_origin_t origin;
	n_transition_t type;
	double dist;
	double obliq;
	struct nirt_seg *seg;
	bool operator<(half_segment other) const
	{
	    if (origin == NIRT_LEFT && other.origin == NIRT_RIGHT) {
		return true;
	    }
	    if (origin == NIRT_RIGHT && other.origin == NIRT_LEFT) {
		return false;
	    }
	    if (type == NIRT_T_OUT && other.type == NIRT_T_IN) {
		return true;
	    }
	    if (type == NIRT_T_IN && other.type == NIRT_T_OUT) {
		return false;
	    }
	    return (seg < other.seg);
	}

	struct nirt_diff_state *nds;

	// Define an equality check that doesn't involve the origin type and
	// uses geometry tolerances.
	bool operator==(half_segment other) const
	{
	    if (type != other.type) {
		return false;
	    }
	    if (!NEAR_EQUAL(dist, other.dist, nds->dist_tol)) {
		return false;
	    }
	    if (!NEAR_EQUAL(obliq, other.obliq, nds->obliq_tol)) {
		return false;
	    }
	    return true;
	}
};

static bool
_nirt_partition_diff(struct nirt_diff_state *nds, struct nirt_seg *left, struct nirt_seg *right)
{
    if (!nds) return false;

    if (left->reg_id != right->reg_id) return true;
    if (left->reg_name != right->reg_name) return true;
    if (left->path_name != right->path_name) return true;

    fastf_t los_delta = fabs(left->los - right->los);
    if (los_delta > nds->los_tol) return true;

    fastf_t scaled_los_delta = fabs(left->scaled_los - right->scaled_los);
    if (scaled_los_delta > nds->scaled_los_tol) return true;

    fastf_t obliq_in_delta = fabs(left->obliq_in - right->obliq_in);
    if (obliq_in_delta > nds->obliq_tol) return true;
    
    fastf_t obliq_out_delta = fabs(left->obliq_out - right->obliq_out);
    if (obliq_out_delta > nds->obliq_tol) return true;
    
    return false;
}


static bool
_nirt_overlap_diff(struct nirt_diff_state *nds, struct nirt_seg *left, struct nirt_seg *right)
{
    if (!nds) return false;

    if (left->ov_reg1_name != right->ov_reg1_name) return true;
    if (left->ov_reg2_name != right->ov_reg2_name) return true;
    if (left->ov_reg1_id != right->ov_reg1_id) return true;
    if (left->ov_reg2_id != right->ov_reg2_id) return true;

    fastf_t ov_in_delta = DIST_PNT_PNT(left->ov_in, right->ov_in);
    if (ov_in_delta > nds->dist_tol) return true;
    
    fastf_t ov_out_delta = DIST_PNT_PNT(left->ov_out, right->ov_out);
    if (ov_out_delta > nds->dist_tol) return true;
   
    fastf_t ov_los_delta = fabs(left->ov_los - right->ov_los);
    if (ov_los_delta > nds->los_tol) return true;
  
    return false;
}

static bool
_nirt_segs_diff(struct nirt_diff_state *nds, struct nirt_seg *left, struct nirt_seg *right)
{
    /* Sanity */
    if (!nds || (!left && !right)) return false;
    if ((left && !right) || (!left && right)) return true;

    if (left->type != right->type) {
	/* Fundamental segment difference - no point going further, they're different */
	return true;
    }

    switch(left->type) {
	case NIRT_MISS_SEG:
	    /* Types don't differ and they're both misses - we're good */
	    return false;
	case NIRT_PARTITION_SEG:
	    return _nirt_partition_diff(nds, left, right);
	case NIRT_GAP_SEG:
	    /* With transitions managing the point differences, gaps don't diff with
	     * other gaps */
	    return false;
	case NIRT_OVERLAP_SEG:
	    return _nirt_overlap_diff(nds, left, right);
	default:
	    nerr(nds->nss, "NIRT diff error: unknown segment type: %d\n", left->type);
	    return 0;
    }
}


class diff_segment {
    public:
	bool done;
	n_origin_t origin;
	n_seg_t type;
	struct nirt_seg *seg;
	std::pair<long, long> trans_start = {LONG_MAX, LONG_MAX};
	std::pair<long, long> trans_end = {LONG_MAX, LONG_MAX};

	diff_segment()
	{
	    done = false;
	    seg = NULL;
	}

	diff_segment(const diff_segment &other)
	{
	    done = other.done;
	    origin = other.origin;
	    type = other.type;
	    seg = other.seg;
	    trans_start = other.trans_start;
	    trans_end = other.trans_end;
	}

	bool operator<(diff_segment &other) const
	{
	    if ((type == NIRT_SEG_HIT) && (other.type != NIRT_SEG_HIT)) {
		return true;
	    }
	    if ((type != NIRT_SEG_HIT) && (other.type == NIRT_SEG_HIT)) {
		return false;
	    }
	    if ((type == NIRT_SEG_OVLP) && (other.type != NIRT_SEG_OVLP)) {
		return true;
	    }
	    if ((type != NIRT_SEG_OVLP) && (other.type == NIRT_SEG_OVLP)) {
		return false;
	    }
	    return (seg < other.seg);
	}

	struct nirt_diff_state *nds;

	// Define an equality check that doesn't involve the origin type and
	// uses geometry tolerances.
	bool operator==(diff_segment &other) const
	{
	    if (type != other.type) {
		return false;
	    }
	    if (trans_start != other.trans_start) {
		return false;
	    }
	    if (trans_end != other.trans_end) {
		return false;
	    }
	    return _nirt_segs_diff(nds, seg, other.seg);
	}

	diff_segment& operator=(const diff_segment &other)
	{
	    done = other.done;
	    origin = other.origin;
	    type = other.type;
	    seg = other.seg;
	    trans_start = other.trans_start;
	    trans_end = other.trans_end;
	    return *this;
	}
};

class segment_bin {
    public:
	std::vector<diff_segment> left;
	std::vector<diff_segment> right;
	half_segment *start;
	half_segment *end;
};

// If we have a lot of small segments (i.e. nearby points) and large distance
// tolerances, we need to be able to decide when points are the same within
// tolerance.  Fuzziness of error bars around points means we're going to have
// to make some sort of binning decision, which needs to be consistent across
// multiple decisions.
//
// The intent here is to use the distance delta tolerance to trim the supplied
// distance's digits down and center the point between the two closest "steps"
// of the order of the tolerance along the ray.  We'll still use NEAR_EQUAL for
// comparisons, but after pre-processing according to the below procedure
// everything should be clamped away from the edges of the bin ranges.
//
// Once we're within a bin, points within that bin will be resolved only
// against other points in the same bin.  That means points which land on the
// edge of a bin near a point in another bin won't be identified as being the
// same despite being within tolerance (even if there is no nearby point
// actually in the shared bin), but some such partitioning is inevitable if we
// want to avoid the classic three point problem:
//
//                                A - B - C
//
// where A is within tol of B and B is within tol of C, but A is not within
// tol of C.  We can only do EITHER A == B or B == C: A == B == C would imply
// A == C which is not true.
//
// This is a basic alternative to a full-fledged decision tree implementation.
std::pair<long, long>
dist_bin(double dist, double dist_tol)
{
    bu_log("dist: %.15f\n", dist);

    // Get the exponent of the base 10 tolerance number (i.e. how may orders).
    int tolexp = (int)std::floor(std::log10(std::fabs(dist_tol)));

    if (abs(tolexp) > 12) {
	// Either a super large or super small tolerance - don't bin?
	return std::make_pair(LONG_MAX, LONG_MAX);
    }

    // Do the same for the distance value
    int distexp = (int)std::floor(std::log10(std::fabs(dist)));

    // If the tolerance is large compared to the value, clamp
    if (abs(tolexp) > (distexp)) {
	distexp = 0;
    }

    // Break the distance into larger and smaller components.  We'll need to
    // shift the value to truncate its significant digits, and we store the
    // larger component of the distance separately so we can do so with greater
    // safety in case the necessary shift would make for a very large number.
    double dmult = std::pow(10, (abs(distexp) - abs(tolexp)));
    long dA = std::trunc(dist/dmult)*dmult;
    double dB = dist - dA;

    // Now, find out how many digits we need to shift the distance left to
    // truncate it at dist_tol precision
    double tmult = std::pow(10, tolexp);

    // Shift and truncate the smaller portion of the distance
    long d1 = std::trunc(dB/tmult/10) * 10;

    // Add half the bin range to shift the new number into the
    // middle of a bin.
    long d2 = d1 + 5;

    // Reassemble the new binned distance value
    double dbinned = dA +  d2*tmult;

    // Debugging
    double b1 = dA + d1 * tmult;
    double b2 = dA + d1 * tmult + 1*std::pow(10,tolexp);
    bu_log("dist   : %.15f, tol: %15f\n", dist, dist_tol);
    bu_log("dA: %ld, dB: %.15f, d1: %ld, d2: %ld\n", dA, dB, d1, d2);
    bu_log("bin1: %.15f, dist - bin1: %.15f, bin2: %.15f, bin2 - dist: %.15f\n", b1, dist - b1, b2, b2 - dist);
    bu_log("dbinned: %.15f, delta: %.15f\n", dbinned, fabs(dist - dbinned));

    return std::make_pair(dA,d2);
}


int
_nirt_segs_analyze(struct nirt_diff_state *nds)
{
    std::vector<struct nirt_diff_ray_state> &dfs = nds->rays;
    for (size_t i = 0; i < dfs.size(); i++) {
	struct nirt_diff_ray_state *rstate = &(dfs[i]);
	std::map<long, std::map<long, std::map<n_transition_t, std::set<half_segment>>>> ordered_transitions;
	std::map<long, std::map<long, std::vector<diff_segment>>> ordered_subsegments;

	std::pair<long, long> key;
	// Map the segments into ordered transitions
	for (size_t j = 0; j < rstate->old_segs.size(); j++) {
	    struct nirt_seg *curr = &rstate->old_segs[j];
	    half_segment in_seg;
	    in_seg.dist = DIST_PNT_PNT(rstate->orig, curr->in);
	    in_seg.origin = NIRT_LEFT;
	    in_seg.type = NIRT_T_IN;
	    in_seg.seg = curr;
	    in_seg.obliq = curr->obliq_in;
	    in_seg.nds = nds;
	    key = dist_bin(in_seg.dist, nds->dist_tol);
	    ordered_transitions[key.first][key.second][NIRT_T_IN].insert(in_seg);

	    half_segment out_seg;
	    out_seg.dist = DIST_PNT_PNT(rstate->orig, curr->out);
	    out_seg.origin = NIRT_LEFT;
	    out_seg.type = NIRT_T_OUT;
	    out_seg.seg = curr;
	    out_seg.obliq = curr->obliq_out;
	    out_seg.nds = nds;
	    key = dist_bin(out_seg.dist, nds->dist_tol);
	    ordered_transitions[key.first][key.second][NIRT_T_OUT].insert(out_seg);
	}
	for (size_t j = 0; j < rstate->old_segs.size(); j++) {
	    struct nirt_seg *curr = &rstate->new_segs[j];
	    half_segment in_seg;
	    in_seg.dist = DIST_PNT_PNT(rstate->orig, curr->in);
	    in_seg.origin = NIRT_RIGHT;
	    in_seg.type = NIRT_T_IN;
	    in_seg.seg = curr;
	    in_seg.obliq = curr->obliq_in;
	    in_seg.nds = nds;
	    key = dist_bin(in_seg.dist, nds->dist_tol);
	    ordered_transitions[key.first][key.second][NIRT_T_IN].insert(in_seg);

	    half_segment out_seg;
	    out_seg.dist = DIST_PNT_PNT(rstate->orig, curr->out);
	    out_seg.origin = NIRT_RIGHT;
	    out_seg.type = NIRT_T_OUT;
	    out_seg.seg = curr;
	    out_seg.obliq = curr->obliq_out;
	    out_seg.nds = nds;
	    key = dist_bin(out_seg.dist, nds->dist_tol);
	    ordered_transitions[key.first][key.second][NIRT_T_OUT].insert(out_seg);
	}
	std::map<long, std::map<long, std::map<n_transition_t, std::set<half_segment>>>>::iterator o_it;
	std::vector<diff_segment> *prev_subsegs = NULL;
	for (o_it = ordered_transitions.begin(); o_it != ordered_transitions.end(); o_it++) {
	    std::cout << o_it->first;
	    std::map<long, std::map<n_transition_t, std::set<half_segment>>> &l2 = o_it->second;
	    std::map<long, std::map<n_transition_t, std::set<half_segment>>>::iterator l2_it;
	    for (l2_it = l2.begin(); l2_it != l2.end(); l2_it++) {
		std::pair<long, long> tkey = std::make_pair(o_it->first, l2_it->first);
		std::cout << "  " << l2_it->first << ":\n";
		// If we're in the bin, the points are (basically) the same within tolerance. At the same point,
		// out hits are processed first
		std::map<n_transition_t, std::set<half_segment>> &l3 = l2_it->second;
		std::map<n_transition_t, std::set<half_segment>>::reverse_iterator l3_it;
		for (l3_it = l3.rbegin(); l3_it != l3.rend(); l3_it++) {
		    std::vector<diff_segment> new_subsegs;

		    // At any given transition point, all segments active in the
		    // previous transition with an OUT point in the current bin are
		    // terminated and stored.  If any prior segment does not have
		    // an OUT point corresponding to the current transition bin the
		    // prior segment is still terminated and stored, but a new
		    // segment of the same type is started at the center bin point.
		    // Any IN points in the transition bin also start new segment
		    // definitions.  OUT points will start gap segments, and IN points
		    // will terminate them.
		    if (prev_subsegs) {
			for (size_t ps = 0; ps < prev_subsegs->size(); ps++) {
			    diff_segment &pseg = (*prev_subsegs)[ps];
			    pseg.nds = nds;
			    if (pseg.done) continue;
			    pseg.trans_end = tkey;
			    // If we have a nirt_seg being broken up by a
			    // transition point from the other side we need to
			    // special-case the subsegment creation
			    if (pseg.type == NIRT_SEG_HIT || pseg.type == NIRT_SEG_OVLP) {
				double dist = DIST_PNT_PNT(rstate->orig, (*prev_subsegs)[ps].seg->out);
				std::pair<long, long> out_key = dist_bin(dist, nds->dist_tol);
				if (out_key != tkey) {
				    // Not ending an existing nirt_seg here - add new diff_segment of same type
				    // with same origin, but with new start key
				    diff_segment cseg = pseg;
				    cseg.trans_start = tkey;
				    new_subsegs.push_back(cseg);
				    std::cout << "INFO: existing seg not closed\n";
				}
			    }
			}
		    }

		    std::set<const half_segment *> hseg_ptrs;

		    std::set<half_segment> &hsegs = l3_it->second;
		    std::set<half_segment>::iterator h_it;
		    for (h_it = hsegs.begin(); h_it != hsegs.end(); h_it++) {
			hseg_ptrs.insert(&(*h_it));
		    }

		    std::string ttype = (l3_it->first == NIRT_T_IN) ? std::string("NIRT_T_IN") : std::string("NIRT_T_OUT");
		    std::cout << "   " << ttype << ":" << hsegs.size() << "\n";


		    // With all segments defined in this fashion, they can be
		    // indexed by their starting transition bin only - they are all
		    // defined to end at the next transiton point and segment
		    // differencing can be handled locally.  As the transitions are
		    // walked, the segments starting at each transition can be found
		    // using a map container similar to the transition container with
		    // the same dual-long key indexing to assemble an overall diff report
		    // that is distance ordered.
		    //
		    // TODO - think about what to do for the case where the IN
		    // and OUT points of a segment are both in the same bin.
		    // In that case do we have no segment - its length is below
		    // tolerance - or do we define a degenerate segment so we
		    // can still report in the diff if we have an unmatched
		    // region_id or some such in the hits?  My initial though is
		    // we need the degenerate segment.  Regardless we'll
		    // need to check for it, since if we're processing OUT hits
		    // first we may encounter a case where an OUT hit has no
		    // corresponding segment in any previous logic and we need
		    // to hunt up the corresponding in hit in the current bin to
		    // prevent the assembly logic from getting confused.
		    while (hseg_ptrs.size()) {
			const half_segment *h = *(hseg_ptrs.begin());
			hseg_ptrs.erase(hseg_ptrs.begin());
			if (h->type == NIRT_T_OUT) {
			    bool paired = false;

			    if (prev_subsegs) {
				// If we have an out hit, check it against prev_subsegs to see if it
				// terminates one of them.
				for (size_t ps = 0; ps < prev_subsegs->size(); ps++) {
				    diff_segment &pseg = (*prev_subsegs)[ps];
				    // Don't need to check origins in this case - only the exact nirt_seg
				    // will match in this test
				    if (pseg.seg == h->seg) {
					paired = true;
				    }
				}
			    }

			    // If it doesn't terminate one of them, check to see if it terminates
			    // an IN point in the current bin
			    const half_segment *h_lin = NULL;
			    if (!paired) {
				std::set<const half_segment *>::iterator hp_it;
				for (hp_it = hseg_ptrs.begin(); hp_it != hseg_ptrs.end(); hp_it++) {
				    const half_segment *hc = *hp_it;
				    // Don't need to check origins in this case - only the exact nirt_seg
				    // will match in this test
				    if (h->seg == hc->seg) {
					// Pairing in bin - make degen segment
					std::cout << "degen seg\n";
					h_lin = hc;
					paired = true;
				    }
				}
			    }
			    if (paired && !h_lin) {
				// Standard case - out closing a previous seg.  This does not
				// require the introduction of a new segment
				std::cout << "closed prev seg\n";
			    }
			    if (paired && h_lin) {
				// Degen case - in and out pairing within bin.  We must introduce
				// a new degenerate segment with both this in and out point, and
				// remove the in point from the working set.
				std::cout << "degen seg\n";
				diff_segment cseg;
				cseg.trans_start = tkey;
				cseg.trans_end = tkey;
				cseg.origin = h_lin->origin;
				cseg.type = (h_lin->seg->type == NIRT_OVERLAP_SEG) ? NIRT_SEG_OVLP : NIRT_SEG_HIT;
				cseg.seg = h_lin->seg;
				new_subsegs.push_back(cseg);

			    }
			    if (!paired) {
				// Error - every out point must have an in point.
				std::cout << "error\n";
			    }
			}
			if (h->type == NIRT_T_IN) {
			    // In points always start new segment
			    std::cout << "new in seg\n";
			    diff_segment cseg;
			    cseg.trans_start = tkey;
			    cseg.origin = h->origin;
			    cseg.type = (h->seg->type == NIRT_OVERLAP_SEG) ? NIRT_SEG_OVLP : NIRT_SEG_HIT;
			    cseg.seg = h->seg;
			    new_subsegs.push_back(cseg);
			}
		    }
		    //
		    // Transitions are compared first (is a point only defined left or right,
		    // do the obliquities match?) then the sub-segments associated with that
		    // transition (more extensive criteria - region info, names, overlaps, etc.
		    // that define most of the reporting differences.)
		    ordered_subsegments[tkey.first][tkey.second] = new_subsegs;
		    prev_subsegs = &(ordered_subsegments[tkey.first][tkey.second]);
		}
	    }
	}
    }

    return 0;
}


void
_nirt_diff_create(struct nirt_state *nss)
{
    nss->i->diff_state = new nirt_diff_state;
    nss->i->diff_state->nss = nss;
}


void
_nirt_diff_destroy(struct nirt_state *nss)
{
    bu_vls_free(&nss->i->diff_state->diff_file);
    delete nss->i->diff_state;
}


void
_nirt_diff_add_seg(struct nirt_state *nss, struct nirt_seg *nseg)
{
    if (nss->i->diff_state && nss->i->diff_state->cdiff) {
	nss->i->diff_state->cdiff->new_segs.push_back(*nseg);
    }
}

static bool
parse_ray(struct nirt_diff_state *nds, std::string &line)
{
    // First up, find out what formatting version we need to deal with.
    // This is done so that as long as the RAY,#, prefix is respected,
    // we can take older diff.nrt files and use them in newer code while
    // preserving the ability to adjust the format if we need/want to.

    struct nirt_diff_ray_state df;
    df.nds = nds;

    std::regex ray_regex("RAY,([0-9]+),(.*)");

    std::smatch s1;
    if (!std::regex_search(line, s1, ray_regex)) {
	nerr(nds->nss, "Error processing ray line \"%s\"!\nUnable to identify formatting version\n", line.c_str());
	return false;
    }

    int ray_version = _nirt_str_to_int(s1[1]);
    std::string ray_data = s1[2];

    std::cerr << "version: " << ray_version << "\n";
    std::cerr << "data   : " << ray_data << "\n";

    if (ray_version == 1) {
	std::vector<std::string> substrs = _nirt_string_split(ray_data);
	if (substrs.size() != 6) {
	    nerr(nds->nss, "Error processing ray line \"%s\"!\nExpected 6 elements, found %zu\n", ray_data.c_str(), substrs.size());
	    return false;
	}
	VSET(df.orig, _nirt_str_to_dbl(substrs[0], 0), _nirt_str_to_dbl(substrs[1], 0), _nirt_str_to_dbl(substrs[2], 0));
	VSET(df.dir,  _nirt_str_to_dbl(substrs[3], 0), _nirt_str_to_dbl(substrs[4], 0), _nirt_str_to_dbl(substrs[5], 0));
	nds->rays.push_back(df);
	nds->cdiff = &(nds->rays[nds->rays.size() - 1]);
	return true;
    }

    nerr(nds->nss, "Error processing ray line \"%s\"!\nUnsupported version: %d\n", line.c_str(), ray_version);
    return false;
}


static bool
parse_hit(struct nirt_diff_state *nds, std::string &line)
{
    std::regex hit_regex("HIT,([0-9]+),(.*)");

    std::smatch s1;
    if (!std::regex_search(line, s1, hit_regex)) {
	nerr(nds->nss, "Error processing hit line \"%s\"!\nUnable to identify formatting version\n", line.c_str());
	return false;
    }

    if (!nds->cdiff) {
	nerr(nds->nss, "Error: Hit line found but no ray set.\n");
	return false;
    }

    int hit_version = _nirt_str_to_int(s1[1]);
    std::string hit_data = s1[2];

    if (hit_version == 1) {
	std::vector<std::string> substrs = _nirt_string_split(hit_data);

	if (substrs.size() != 15) {
	    nerr(nds->nss, "Error processing hit line \"%s\"!\nExpected 15 elements, found %zu\n", hit_data.c_str(), substrs.size());
	    return -1;
	}

	struct nirt_seg *segp = new struct nirt_seg;
	segp->type = NIRT_PARTITION_SEG;
	struct bu_vls rdecode = BU_VLS_INIT_ZERO;
	bu_vls_decode(&rdecode, substrs[0].c_str());
	segp->reg_name = std::string(bu_vls_cstr(&rdecode));
	bu_vls_free(&rdecode);
	struct bu_vls pdecode = BU_VLS_INIT_ZERO;
	bu_vls_decode(&pdecode, substrs[1].c_str());
	segp->path_name = std::string(bu_vls_cstr(&pdecode));
	bu_vls_free(&pdecode);
	segp->reg_id = _nirt_str_to_int(substrs[2]);
	VSET(segp->in, _nirt_str_to_dbl(substrs[3], 0), _nirt_str_to_dbl(substrs[4], 0), _nirt_str_to_dbl(substrs[5], 0));
	segp->d_in = _nirt_str_to_dbl(substrs[6], 0);
	VSET(segp->out, _nirt_str_to_dbl(substrs[7], 0), _nirt_str_to_dbl(substrs[8], 0), _nirt_str_to_dbl(substrs[9], 0));
	segp->d_out = _nirt_str_to_dbl(substrs[10], 0);
	segp->los = _nirt_str_to_dbl(substrs[11], 0);
	segp->scaled_los = _nirt_str_to_dbl(substrs[12], 0);
	segp->obliq_in = _nirt_str_to_dbl(substrs[13], 0);
	segp->obliq_out = _nirt_str_to_dbl(substrs[14], 0);
	nds->cdiff->old_segs.push_back(*segp);
	delete segp;
	return true;
    }

    nerr(nds->nss, "Error processing hit line \"%s\"!\nUnsupported version: %d\n", line.c_str(), hit_version);
    return false;

}
static bool
parse_overlap(struct nirt_diff_state *nds, std::string &line)
{
    std::regex overlap_regex("OVERLAP,([0-9]+),(.*)");

    std::smatch s1;
    if (!std::regex_search(line, s1, overlap_regex)) {
	nerr(nds->nss, "Error processing overlap line \"%s\"!\nUnable to identify formatting version\n", line.c_str());
	return false;
    }

    if (!nds->cdiff) {
	nerr(nds->nss, "Error: Overlap line found but no ray set.\n");
	return false;
    }

    int overlap_version = _nirt_str_to_int(s1[1]);
    std::string overlap_data = s1[2];

    if (overlap_version == 1) {
	std::vector<std::string> substrs = _nirt_string_split(overlap_data);
	if (substrs.size() != 11) {
	    nerr(nds->nss, "Error processing overlap line \"%s\"!\nExpected 11 elements, found %zu\n", overlap_data.c_str(), substrs.size());
	    return false;
	}
	struct nirt_seg *segp = new struct nirt_seg;
	segp->type = NIRT_OVERLAP_SEG;
	struct bu_vls r1decode = BU_VLS_INIT_ZERO;
	bu_vls_decode(&r1decode, substrs[0].c_str());
	segp->ov_reg1_name = std::string(bu_vls_cstr(&r1decode));
	bu_vls_free(&r1decode);
	struct bu_vls r2decode = BU_VLS_INIT_ZERO;
	bu_vls_decode(&r2decode, substrs[1].c_str());
	segp->ov_reg2_name = std::string(bu_vls_cstr(&r2decode));
	bu_vls_free(&r2decode);
	segp->ov_reg1_id = _nirt_str_to_int(substrs[2]);
	segp->ov_reg2_id = _nirt_str_to_int(substrs[3]);
	VSET(segp->ov_in, _nirt_str_to_dbl(substrs[4], 0), _nirt_str_to_dbl(substrs[5], 0), _nirt_str_to_dbl(substrs[6], 0));
	VSET(segp->ov_out, _nirt_str_to_dbl(substrs[7], 0), _nirt_str_to_dbl(substrs[8], 0), _nirt_str_to_dbl(substrs[9], 0));
	VMOVE(segp->in, segp->ov_in);
	VMOVE(segp->out, segp->ov_out);
	segp->ov_los = _nirt_str_to_dbl(substrs[10], 0);
	nds->cdiff->old_segs.push_back(*segp);
	delete segp;
	return true;
    }

    nerr(nds->nss, "Error processing overlap line \"%s\"!\nUnsupported version: %d\n", line.c_str(), overlap_version);
    return false;
}


static int
_nirt_cmd_msgs(void *ndsv, int argc, const char **argv, const char *us, const char *ps)
{
    struct nirt_diff_state *nds = (struct nirt_diff_state *)ndsv;
    struct nirt_state *nss = nds->nss;
    if (argc == 2 && BU_STR_EQUAL(argv[1], HELPFLAG)) {
	nout(nss, "%s\n%s\n", us, ps);
	return 1;
    }
    if (argc == 2 && BU_STR_EQUAL(argv[1], PURPOSEFLAG)) {
	nout(nss, "%s\n", ps);
	return 1;
    }
    return 0;
}


extern "C" int
_nirt_diff_cmd_clear(void *ndsv, int argc, const char **argv)
{
    struct nirt_diff_state *nds = (struct nirt_diff_state *)ndsv;

    const char *usage_string = "diff [options] clear";
    const char *purpose_string = "reset NIRT diff state";
    if (_nirt_cmd_msgs(nds, argc, argv, usage_string, purpose_string)) {
	return 0;
    }

    argv++; argc--;

    nds->rays.clear();
    nds->cdiff = NULL;
    bu_vls_trunc(&nds->diff_file, 0);
    return 0;
}


extern "C" int
_nirt_diff_cmd_load(void *ndsv, int argc, const char **argv)
{
    size_t cmt = std::string::npos;
    struct nirt_diff_state *nds = (struct nirt_diff_state *)ndsv;
    struct nirt_state *nss = nds->nss;

    const char *usage_string = "diff [options] load <nirt_diff_file>";
    const char *purpose_string = "load pre-existing NIRT shotline information";
    if (_nirt_cmd_msgs(nds, argc, argv, usage_string, purpose_string)) {
	return 0;
    }

    argv++; argc--;

    if (argc != 1) {
	nerr(nss, "please specify a nirt diff file to load.\n");
	return -1;
    }

    std::string line;
    std::ifstream ifs;
    ifs.open(argv[0]);
    if (!ifs.is_open()) {
	nerr(nss, "error: could not open file %s\n", argv[0]);
	return -1;
    }

    const char *av0 = "diff";
    const char *av1 = "clear";
    const char *av[3];
    av[0] = av0;
    av[1] = av1;
    av[2] = NULL;
    _nirt_diff_cmd_clear(ndsv, 2, (const char **)av);

    if (nss->i->need_reprep) {
	/* if we need to (re)prep, do it now. failure is an error. */
	if (_nirt_raytrace_prep(nss)) {
	    nerr(nss, "error: raytrace prep failed!\n");
	    return -1;
	}
    } else {
	/* based on current settings, tell the ap which rtip to use */
	nss->i->ap->a_rt_i = _nirt_get_rtip(nss);
	nss->i->ap->a_resource = _nirt_get_resource(nss);
    }

    bu_vls_sprintf(&nds->diff_file, "%s", argv[0]);
    while (std::getline(ifs, line)) {
	/* if part of the line is commented, skip that part */
	cmt = _nirt_find_first_unescaped(line, "#", 0);
	if (cmt != std::string::npos) {
	    line.erase(cmt);
	}

	/* if the whole line was a comment, skip it */
	_nirt_trim_whitespace(line);
	if (!line.length()) continue;

	/* not a comment - has to be a valid type, or it's an error */
	int ltype = -1;
	ltype = (ltype < 0 && !line.compare(0, 4, "RAY,")) ? 0 : ltype;
	ltype = (ltype < 0 && !line.compare(0, 4, "HIT,")) ? 1 : ltype;
	ltype = (ltype < 0 && !line.compare(0, 8, "OVERLAP,")) ? 4 : ltype;
	if (ltype < 0) {
	    nerr(nss, "Error processing diff file, line \"%s\"!\nUnknown line type\n", line.c_str());
	    return -1;
	}

	/* Ray */
	if (ltype == 0) {
	    if (!parse_ray(nds, line)) {
		_nirt_diff_cmd_clear((void *)nds, 0, NULL);
		return -1;
	    }
	    continue;
	}

	/* Hit */
	if (ltype == 1) {
	    if (!parse_hit(nds, line)) {
		_nirt_diff_cmd_clear((void *)nds, 0, NULL);
		return -1;
	    }
	    continue;
	}

	/* Overlap */
	if (ltype == 4) {
	    if (!parse_overlap(nds, line)) {
		_nirt_diff_cmd_clear((void *)nds, 0, NULL);
		return -1;
	    }
	    continue;
	}
#ifdef NIRT_DIFF_DEBUG
	nerr(nss, "Warning - unknown line type, skipping: %s\n", line.c_str());
#endif
    }

    ifs.close();

    // New loading complete - we now need to do a new shot run
    nds->run_complete = false;

    // Done with if_hit and friends
    return 0;
}


// Thought - if we have rays but no pre-defined output, write out the
// expected output to stdout - in this mode diff will generate a diff
// input file from a supplied list of rays.
extern "C" int
_nirt_diff_cmd_run(void *ndsv, int argc, const char **argv)
{
    struct nirt_diff_state *nds = (struct nirt_diff_state *)ndsv;
    struct nirt_state *nss = nds->nss;

    const char *usage_string = "diff [options] run";
    const char *purpose_string = "compute new shotlines to compare with loaded data";
    if (_nirt_cmd_msgs(nds, argc, argv, usage_string, purpose_string)) {
	return 0;
    }

    argv++; argc--;

    // Clear any preexisting new_segs in case this is a repeated run
    for (unsigned int i = 0; i < nds->rays.size(); i++) {
	nds->rays[i].new_segs.clear();
    }

    // Temporarily suppress all formatting output for a silent diff run...
    nirt_fmt_state old_fmt = nds->nss->i->fmt;
    nds->nss->i->fmt.clear();

    for (unsigned int i = 0; i < nds->rays.size(); i++) {
	nds->cdiff = &(nds->rays[i]);
	VMOVE(nss->i->vals->dir,  nds->cdiff->dir);
	VMOVE(nss->i->vals->orig, nds->cdiff->orig);
	_nirt_targ2grid(nss);
	_nirt_dir2ae(nss);
	for (int ii = 0; ii < 3; ++ii) {
	    nss->i->ap->a_ray.r_pt[ii] = nss->i->vals->orig[ii];
	    nss->i->ap->a_ray.r_dir[ii] = nss->i->vals->dir[ii];
	}
	_nirt_init_ovlp(nss);
	(void)rt_shootray(nss->i->ap);
    }

    nds->nss->i->fmt = old_fmt;
    nds->run_complete = true;

    return 0;
}


extern "C" int
_nirt_diff_cmd_report(void *ndsv, int argc, const char **argv)
{
    struct nirt_diff_state *nds = (struct nirt_diff_state *)ndsv;

    const char *usage_string = "diff [options] report";
    const char *purpose_string = "report differences (if any) between old and new shotlines";
    if (_nirt_cmd_msgs(nds, argc, argv, usage_string, purpose_string)) {
	return 0;
    }

    argv++; argc--;

    // Report diff results according to the NIRT diff settings.
    if (!bu_vls_strlen(&nds->diff_file)) {
	nerr(nds->nss, "No diff file loaded - please load a diff file with \"diff load <filename>\"\n");
	return -1;
    } else {

	if (!nds->run_complete) {
	    const char *av0 = "diff";
	    const char *av1 = "run";
	    const char *av[3];
	    av[0] = av0;
	    av[1] = av1;
	    av[2] = NULL;
	    _nirt_diff_cmd_run(ndsv, 2, (const char **)av);
	}

	// Analyze the partition sets
	return _nirt_segs_analyze(nds);
    }
}


extern "C" int
_nirt_diff_cmd_settings(void *ndsv, int argc, const char **argv)
{
    struct nirt_diff_state *nds = (struct nirt_diff_state *)ndsv;
    struct nirt_state *nss = nds->nss;

    const char *usage_string = "diff [options] settings [key] [val]";
    const char *purpose_string = "print and change reporting and tolerance settings";
    if (_nirt_cmd_msgs(nds, argc, argv, usage_string, purpose_string)) {
	return 0;
    }

    argv++; argc--;

    if (!argc) {
	//print current settings
	nout(nss, "partitions:           %d\n", nds->partitions);
	nout(nss, "overlaps:             %d\n", nds->overlaps);
	nout(nss, "partition_reg_ids:    %d\n", nds->partition_reg_ids);
	nout(nss, "partition_reg_names:  %d\n", nds->partition_reg_names);
	nout(nss, "partition_path_names: %d\n", nds->partition_path_names);
	nout(nss, "partition_obliq:      %d\n", nds->partition_obliq);
	nout(nss, "overlap_reg_names:    %d\n", nds->overlap_reg_names);
	nout(nss, "overlap_reg_ids:      %d\n", nds->overlap_reg_ids);
	nout(nss, "overlap_obliq:        %d\n", nds->overlap_obliq);
	nout(nss, "dist_tol:              %g\n", nds->dist_tol);
	nout(nss, "obliq_tol:             %g\n", nds->obliq_tol);
	nout(nss, "los_tol:               %g\n", nds->los_tol);
	nout(nss, "scaled_los_tol:        %g\n", nds->scaled_los_tol);
	return 0;
    }
    if (argc == 1) {
	//print specific setting
	if (BU_STR_EQUAL(argv[0], "partitions")) nout(nss, "%d\n", nds->partitions);
	if (BU_STR_EQUAL(argv[0], "overlaps")) nout(nss, "%d\n", nds->overlaps);
	if (BU_STR_EQUAL(argv[0], "partition_reg_ids")) nout(nss, "%d\n", nds->partition_reg_ids);
	if (BU_STR_EQUAL(argv[0], "partition_reg_names")) nout(nss, "%d\n", nds->partition_reg_names);
	if (BU_STR_EQUAL(argv[0], "partition_path_names")) nout(nss, "%d\n", nds->partition_path_names);
	if (BU_STR_EQUAL(argv[0], "partition_obliq")) nout(nss, "%d\n", nds->partition_obliq);
	if (BU_STR_EQUAL(argv[0], "overlap_reg_names")) nout(nss, "%d\n", nds->overlap_reg_names);
	if (BU_STR_EQUAL(argv[0], "overlap_reg_ids")) nout(nss, "%d\n", nds->overlap_reg_ids);
	if (BU_STR_EQUAL(argv[0], "overlap_obliq")) nout(nss, "%d\n", nds->overlap_obliq);
	if (BU_STR_EQUAL(argv[0], "dist_tol")) nout(nss, "%g\n", nds->dist_tol);
	if (BU_STR_EQUAL(argv[0], "obliq_tol")) nout(nss, "%g\n", nds->obliq_tol);
	if (BU_STR_EQUAL(argv[0], "los_tol")) nout(nss, "%g\n", nds->los_tol);
	if (BU_STR_EQUAL(argv[0], "scaled_los_tol")) nout(nss, "%g\n", nds->scaled_los_tol);
	return 0;
    }
    if (argc == 2) {
	//set setting
	struct bu_vls opt_msg = BU_VLS_INIT_ZERO;
	bool *setting_bool = NULL;
	fastf_t *setting_fastf_t = NULL;
	if (BU_STR_EQUAL(argv[0], "partitions")) setting_bool = &(nds->partitions);
	if (BU_STR_EQUAL(argv[0], "overlaps")) setting_bool = &(nds->overlaps);
	if (BU_STR_EQUAL(argv[0], "partition_reg_ids")) setting_bool = &(nds->partition_reg_ids);
	if (BU_STR_EQUAL(argv[0], "partition_reg_names")) setting_bool = &(nds->partition_reg_names);
	if (BU_STR_EQUAL(argv[0], "partition_path_names")) setting_bool = &(nds->partition_path_names);
	if (BU_STR_EQUAL(argv[0], "partition_obliq")) setting_bool = &(nds->partition_obliq);
	if (BU_STR_EQUAL(argv[0], "overlap_reg_names")) setting_bool = &(nds->overlap_reg_names);
	if (BU_STR_EQUAL(argv[0], "overlap_reg_ids")) setting_bool = &(nds->overlap_reg_ids);
	if (BU_STR_EQUAL(argv[0], "overlap_obliq")) setting_bool = &(nds->overlap_obliq);
	if (BU_STR_EQUAL(argv[0], "dist_tol")) setting_fastf_t = &(nds->dist_tol);
	if (BU_STR_EQUAL(argv[0], "obliq_tol")) setting_fastf_t = &(nds->obliq_tol);
	if (BU_STR_EQUAL(argv[0], "los_tol")) setting_fastf_t = &(nds->los_tol);
	if (BU_STR_EQUAL(argv[0], "scaled_los_tol")) setting_fastf_t = &(nds->scaled_los_tol);

	if (setting_bool) {
	    if (bu_opt_bool(&opt_msg, 1, (const char **)&argv[1], (void *)setting_bool) == -1) {
		nerr(nss, "Error: bu_opt value read failure: %s\n", bu_vls_cstr(&opt_msg));
		bu_vls_free(&opt_msg);
		return -1;
	    }
	    return 0;
	}
	if (setting_fastf_t) {
	    if (BU_STR_EQUAL(argv[1], "BN_TOL_DIST") || BU_STR_EQUIV(argv[1], "default")) {
		*setting_fastf_t = BN_TOL_DIST;
	    } else {
		if (bu_opt_fastf_t(&opt_msg, 1, (const char **)&argv[1], (void *)setting_fastf_t) == -1) {
		    nerr(nss, "Error: bu_opt value read failure: %s\n", bu_vls_cstr(&opt_msg));
		    bu_vls_free(&opt_msg);
		    return -1;
		}
	    }
	    return 0;
	}
    }

    // anything else is an error
    return -1;
}


extern "C" int
_nirt_diff_help(void *ndsv, int argc, const char **argv)
{
    struct nirt_diff_state *nds = (struct nirt_diff_state *)ndsv;
    struct nirt_state *nss = nds->nss;

    if (!argc || !argv || BU_STR_EQUAL(argv[0], "help")) {
	nout(nss, "diff [options] subcommand [args]\n");
	if (nds->d) {
	    char *option_help = bu_opt_describe(nds->d, NULL);
	    if (option_help) {
		nout(nss, "Options:\n%s\n", option_help);
		bu_free(option_help, "help str");
	    }
	}
	nout(nss, "Available subcommands:\n");
	const struct bu_cmdtab *ctp = NULL;
	int ret;
	const char *helpflag[2];
	helpflag[1] = PURPOSEFLAG;
	for (ctp = nds->cmds; ctp->ct_name != (char *)NULL; ctp++) {
	    nout(nss, "  %s\t\t", ctp->ct_name);
	    if (!BU_STR_EQUAL(ctp->ct_name, "help")) {
		helpflag[0] = ctp->ct_name;
		bu_cmd(nds->cmds, 2, helpflag, 0, (void *)nds, &ret);
	    } else {
		nout(nss, "print help and exit\n");
	    }
	}
    } else {
	int ret;
	const char **helpargv = (const char **)bu_calloc(argc+1, sizeof(char *), "help argv");
	helpargv[0] = argv[0];
	helpargv[1] = HELPFLAG;
	for (int i = 1; i < argc; i++) {
	    helpargv[i+1] = argv[i];
	}
	bu_cmd(nds->cmds, argc+1, helpargv, 0, (void *)nds, &ret);
	bu_free(helpargv, "help argv");
	return ret;
    }

    return 0;
}


const struct bu_cmdtab _nirt_diff_cmds[] = {
    { "load", _nirt_diff_cmd_load},
    { "run", _nirt_diff_cmd_run},
    { "report", _nirt_diff_cmd_report},
    { "clear", _nirt_diff_cmd_clear},
    { "settings", _nirt_diff_cmd_settings},
    { (char *)NULL, NULL}
};


//#define NIRT_DIFF_DEBUG 1

extern "C" int
_nirt_cmd_diff(void *ns, int argc, const char *argv[])
{
    if (!ns) return -1;
    struct nirt_state *nss = (struct nirt_state *)ns;
    struct nirt_diff_state *nds = nss->i->diff_state;
    int help = 0;
    struct bu_opt_desc d[2];
    BU_OPT(d[0],  "h", "help",       "",             NULL,   &help, "print help and exit");
    BU_OPT_NULL(d[1]);

    // Need for help printing
    nds->d = (struct bu_opt_desc *)d;
    nds->cmds = _nirt_diff_cmds;

    argv++; argc--;

    // Diff does everything in mm
    double l2base = nss->i->local2base;
    double base2l = nss->i->base2local;

    nss->i->local2base = 1.0;
    nss->i->base2local = 1.0;

    // High level options are only defined prior to the subcommand
    int cmd_pos = -1;
    for (int i = 0; i < argc; i++) {
	if (bu_cmd_valid(_nirt_diff_cmds, argv[i]) == BRLCAD_OK) {
	    cmd_pos = i;
	    break;
	}
    }

    int acnt = (cmd_pos >= 0) ? cmd_pos : argc;

    struct bu_vls optparse_msg = BU_VLS_INIT_ZERO;
    if (bu_opt_parse(&optparse_msg, acnt, argv, d) == -1) {
	nerr(nss, "%s", bu_vls_cstr(&optparse_msg));
	bu_vls_free(&optparse_msg);
	nss->i->local2base = l2base;
	nss->i->base2local = base2l;
	return -1;
    }
    bu_vls_free(&optparse_msg);

    if (help) {
	if (cmd_pos >= 0) {
	    argc = argc - cmd_pos;
	    argv = &argv[cmd_pos];
	    _nirt_diff_help(nds, argc, argv);
	} else {
	    _nirt_diff_help(nds, 0, NULL);
	}
	nss->i->local2base = l2base;
	nss->i->base2local = base2l;
	return 0;
    }

    // Must have a subcommand
    if (cmd_pos == -1) {
	nerr(nss, ": no valid subcommand specified\n");
	_nirt_diff_help(nds, 0, NULL);
	nss->i->local2base = l2base;
	nss->i->base2local = base2l;
	return -1;
    }

    argc = argc - cmd_pos;
    argv = &argv[cmd_pos];

    int ret;
    if (bu_cmd(_nirt_diff_cmds, argc, argv, 0, (void *)nds, &ret) == BRLCAD_OK) {
	nss->i->local2base = l2base;
	nss->i->base2local = base2l;
	return ret;
    }

    nss->i->local2base = l2base;
    nss->i->base2local = base2l;
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
