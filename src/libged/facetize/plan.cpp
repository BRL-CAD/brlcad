/*                        P L A N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file libged/facetize/plan.cpp
 *
 * Instance-aware adjust planning pass for the Manifold facetize pipeline.
 *
 * Architecture follows npush's three-phase pattern:
 *
 *   Phase A - Walk: manually recurse through raw CSG tree nodes (not via
 *     db_walk_tree) to record every leaf instance with its full path key and
 *     boolean role (subtractive or not).  The manual walk gives direct access
 *     to OP_SUBTRACT at each node so the subtractive context is propagated
 *     cleanly through the recursion.
 *
 *   Phase B - Name: for each instance that has an ft_perturb-capable primitive
 *     type, assign a collision-safe variant name.  Pre-collecting all existing
 *     working-db names into a std::set<std::string> (the "dbnames" set) and
 *     inserting each newly assigned name into it as it is reserved prevents
 *     both collision with existing objects and cross-variant collisions within
 *     the same planning run.
 *
 *   Phase C - Rebuild & Create: build the lookup table (inst_to_variant) from
 *     the named instances first (mirrors npush's "clear+repopulate" step), then
 *     create the perturbed variant primitives in the working .g via ft_perturb
 *     hooks (ARB8->ARBN, ARBN, TGC, ELL, SPH, TOR).
 *
 * Role disambiguation in inst_to_variant:
 *   The same primitive can appear at identical tree depth in both a UNION and a
 *   SUBTRACT branch (e.g. "u A - B u C - A").  Both occurrences share the same
 *   db_path_to_string() key.  To resolve the ambiguity the map key encodes the
 *   boolean role as a suffix: path + "#base" for non-subtractive instances,
 *   path + "#sub" for subtractive ones.  _booltree_leaf_tess reads
 *   tsp->ts_sofar & TS_SOFAR_MINUS to reconstruct the correct key at booleval
 *   time.
 *
 * Variant naming convention (before collision avoidance):
 *   <srcname>__adj_<n>      - BASE variant (non-subtractive, index n)
 *   <srcname>__adj_sub_<n>  - SUB  variant (subtractive, index n)
 *
 * If the natural name exceeds PLAN_VARNAME_MAXLEN characters a hash-truncated
 * suffix keeps names stable and within a safe length.
 *
 * Perturbation magnitudes (deterministic, seeded per variant slot):
 *   BASE: 1.0x-1.9x BN_TOL_DIST
 *   SUB:  1.0x-1.9x max(BN_TOL_DIST, effective abs tess floor)
 *
 * Using the tess-floor-aware scale for SUB variants avoids a failure mode where
 * tiny BN_TOL_DIST-sized perturbations are later erased by primitive
 * tessellation distance clamping (default 0.05 mm for non-micro geometry),
 * leaving near-coplanar subtraction slivers in the BoT result.
 */

#include "common.h"

#include <cinttypes>
#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/hash.h"
#include "bu/path.h"
#include "bn/tol.h"
#include "rt/calc.h"
#include "rt/geom.h"
#include "raytrace.h"

#include "../ged_private.h"
#include "./ged_facetize.h"

/* Maximum variant name length before hash-truncation is applied. */
#define PLAN_VARNAME_MAXLEN 64

/* Suffix tokens for variant names (kept as constants to avoid duplication). */
static const char * const PLAN_BASE_SUFFIX = "__adj_";
static const char * const PLAN_SUB_SUFFIX  = "__adj_sub_";

/* Number of discrete jitter steps for deterministic perturbation factor.
 * Produces jitter in [0, (PLAN_JITTER_STEPS-1)/PLAN_JITTER_STEPS] range. */
#define PLAN_JITTER_STEPS 10

/* Low-32-bit mask used when folding a 64-bit hash to an 8-hex-digit suffix. */
#define PLAN_HASH_MASK_32 (0xFFFFFFFFULL)

/* Maximum number of characters kept from a source name in the hash-truncated
 * variant name form (leaves room for the suffix and 8-digit hash). */
#define PLAN_TRUNCATED_SRCNAME_LEN 40

/* ------------------------------------------------------------------ */
/* Instance record                                                      */
/* ------------------------------------------------------------------ */

/**
 * One recorded leaf instance from the planning walk.
 *
 * The vname field is initially empty and is filled during the naming phase
 * (analogous to iname in npush's dp_i).  A non-empty vname indicates that a
 * collision-safe variant name has been assigned and the instance is eligible
 * for variant creation.
 */
struct LeafInst {
	std::string src_name;  /**< source primitive name in working .g */
	std::string path_key;  /**< /root/.../prim  (matches db_path_to_string) */
	bool        is_sub;    /**< true iff on the RHS of a subtract in the tree */
	int         prim_type; /**< dp->d_minor_type */
	std::string vname;     /**< assigned variant name (empty until naming phase) */
};

/* ------------------------------------------------------------------ */
/* Planning walk (Phase A)                                              */
/* ------------------------------------------------------------------ */

/** State threaded through the recursive planning walk. */
struct PlanWalkCtx {
	struct db_i            *dbip;
	std::vector<std::string> path_stack; /**< name segments from root to current */
	std::vector<LeafInst>    instances;  /**< accumulated leaf instances */
};

static void plan_walk_dp(PlanWalkCtx *ctx, struct directory *dp, bool in_sub);

/**
 * Walk one CSG tree node, propagating the subtractive context.
 * OP_SUBTRACT marks its right operand (and all its descendants) as subtractive.
 */
static void
plan_walk_tree_node(PlanWalkCtx *ctx, union tree *tp, bool in_sub)
{
	if (!tp)
		return;

	switch (tp->tr_op) {
		case OP_DB_LEAF: {
			struct directory *dp =
				db_lookup(ctx->dbip, tp->tr_l.tl_name, LOOKUP_QUIET);
			if (dp)
				plan_walk_dp(ctx, dp, in_sub);
			break;
		}
		case OP_UNION:
		case OP_INTERSECT:
			plan_walk_tree_node(ctx, tp->tr_b.tb_left,  in_sub);
			plan_walk_tree_node(ctx, tp->tr_b.tb_right, in_sub);
			break;
		case OP_SUBTRACT:
			plan_walk_tree_node(ctx, tp->tr_b.tb_left,  in_sub);
			/* Right operand of a subtract is subtractive */
			plan_walk_tree_node(ctx, tp->tr_b.tb_right, true);
			break;
		case OP_NOT:
		case OP_GUARD:
		case OP_XNOP:
			plan_walk_tree_node(ctx, tp->tr_b.tb_left, in_sub);
			break;
		default:
			break;
	}
}

/**
 * Push dp onto the path stack, recurse into it (combination) or record it
 * (solid), then pop.  Path segments are directory names, so the concatenation
 * "/seg0/seg1/.../segN" exactly matches db_path_to_string() output.
 */
static void
plan_walk_dp(PlanWalkCtx *ctx, struct directory *dp, bool in_sub)
{
	ctx->path_stack.push_back(std::string(dp->d_namep));

	if (dp->d_flags & RT_DIR_COMB) {
		struct rt_db_internal intern;
		RT_DB_INTERNAL_INIT(&intern);
		if (rt_db_get_internal(&intern, dp, ctx->dbip, NULL,
				&rt_uniresource) >= 0) {
			struct rt_comb_internal *comb =
				(struct rt_comb_internal *)intern.idb_ptr;
			if (comb->tree)
				plan_walk_tree_node(ctx, comb->tree, in_sub);
			rt_db_free_internal(&intern);
		}
	} else if (dp->d_flags & RT_DIR_SOLID) {
		/* Build the path key that db_path_to_string() would produce */
		std::string path_key;
		for (const auto &seg : ctx->path_stack)
			path_key += "/" + seg;

		LeafInst inst;
		inst.src_name  = std::string(dp->d_namep);
		inst.path_key  = path_key;
		inst.is_sub    = in_sub;
		inst.prim_type = (int)dp->d_minor_type;
		/* vname is left empty until the naming phase */
		ctx->instances.push_back(inst);
	}

	ctx->path_stack.pop_back();
}

/* ------------------------------------------------------------------ */
/* Deterministic perturbation magnitude                                 */
/* ------------------------------------------------------------------ */

/**
 * Compute a deterministic perturbation factor for a variant slot.
 *
 * A hash of (src_name, role char, index) produces a jitter in
 * [0, (PLAN_JITTER_STEPS-1)/PLAN_JITTER_STEPS] steps, which is added to the
 * role-specific base offset:
 *   BASE (is_sub=false): [1.0, 1.9] x BN_TOL_DIST
 *   SUB  (is_sub=true):  [1.0, 1.9] x max(BN_TOL_DIST, abs tess floor)
 */
static fastf_t
variant_abs_tess_floor(fastf_t bbox_diag)
{
	/* Default primitive abs tess floor for non-micro geometry (mm). */
	fastf_t min_dtol = 0.05;
	const char *env = getenv("RT_PRIM_MIN_ABS_TOL");
	if (env) {
		char *end;
		double val = strtod(env, &end);
		if (end != env && val > 0.0)
			min_dtol = (fastf_t)val;
	}

	/* Micro-geometry uses a 1% of bbox-diagonal floor when diag < 1 mm. */
	if (bbox_diag > SMALL_FASTF && bbox_diag < 1.0)
		min_dtol = bbox_diag * 0.01;

	return min_dtol;
}

static fastf_t
variant_perturb_factor(const std::string &src_name, bool is_sub, int idx, fastf_t bbox_diag)
{
	struct bu_data_hash_state *hs = bu_data_hash_create();
	bu_data_hash_update(hs, src_name.c_str(), src_name.size());
	const char role = is_sub ? 's' : 'b';
	bu_data_hash_update(hs, &role, 1);
	bu_data_hash_update(hs, &idx, sizeof(idx));
	uint64_t h = bu_data_hash_val(hs);
	bu_data_hash_destroy(hs);

	double jitter = (double)(h % PLAN_JITTER_STEPS) / (double)PLAN_JITTER_STEPS;
	if (is_sub) {
		fastf_t sub_scale = variant_abs_tess_floor(bbox_diag);
		if (sub_scale < BN_TOL_DIST)
			sub_scale = BN_TOL_DIST;
		return (fastf_t)((1.0 + jitter) * sub_scale);
	}
	return (fastf_t)((1.0 + jitter) * BN_TOL_DIST);
}

/* ------------------------------------------------------------------ */
/* Phase B: collision-safe naming                                       */
/* ------------------------------------------------------------------ */

/**
 * Generate the "natural" (pre-collision-check) base name for a variant.
 *
 * BASE form:  <srcname>__adj_<idx>
 * SUB  form:  <srcname>__adj_sub_<idx>
 *
 * If the result would exceed PLAN_VARNAME_MAXLEN, the source name is
 * truncated to 40 characters and an 8-hex-digit hash of the full natural
 * form is appended to keep names stable and unique.
 * The PLAN_BASE_SUFFIX / PLAN_SUB_SUFFIX constants ensure the suffix token
 * is spelled identically in both the natural form and the hash-truncated form.
 */
static std::string
natural_variant_name(const std::string &src_name, bool is_sub, int idx)
{
	const char *sfx = is_sub ? PLAN_SUB_SUFFIX : PLAN_BASE_SUFFIX;

	char idx_buf[32];
	snprintf(idx_buf, sizeof(idx_buf), "%d", idx);

	std::string candidate = src_name + sfx + idx_buf;

	if ((int)candidate.size() <= PLAN_VARNAME_MAXLEN)
		return candidate;

	/* Hash-truncated form: first 40 chars of source + suffix + hash */
	uint64_t h = bu_data_hash(candidate.c_str(), candidate.size());
	char hbuf[20];
	snprintf(hbuf, sizeof(hbuf), "%08" PRIx64, (uint64_t)(h & PLAN_HASH_MASK_32));

	return src_name.substr(0, PLAN_TRUNCATED_SRCNAME_LEN) + sfx + hbuf;
}

/* ------------------------------------------------------------------ */
/* Phase C: variant primitive creation                                  */
/* ------------------------------------------------------------------ */

/**
 * Create one perturbed variant of @a src_name in @a wdbip via ft_perturb.
 * Writes the result under @a vname.
 *
 * @return BRLCAD_OK on success, BRLCAD_ERROR otherwise.
 */
static int
create_variant_in_working_g(struct db_i       *wdbip,
				const std::string &src_name,
				const std::string &vname,
				bool               is_sub,
				int                idx,
				int                verbosity,
				fastf_t           *out_factor)
{
	struct directory *src_dp =
		db_lookup(wdbip, src_name.c_str(), LOOKUP_QUIET);
	if (!src_dp)
		return BRLCAD_ERROR;

	struct rt_db_internal src_intern;
	RT_DB_INTERNAL_INIT(&src_intern);
	if (rt_db_get_internal(&src_intern, src_dp, wdbip, NULL,
				&rt_uniresource) < 0)
		return BRLCAD_ERROR;

	int prim_type = src_intern.idb_type;
	if (!OBJ[prim_type].ft_perturb) {
		rt_db_free_internal(&src_intern);
		return BRLCAD_ERROR;
	}

	point_t bmin, bmax;
	/* If bounds fail, keep -1 and fall back to the non-micro/default floor. */
	fastf_t bbox_diag = -1.0;
	if (rt_bound_internal(wdbip, src_dp, bmin, bmax) == 0)
		bbox_diag = DIST_PNT_PNT(bmin, bmax);

	fastf_t factor = variant_perturb_factor(src_name, is_sub, idx, bbox_diag);
	if (out_factor) *out_factor = factor;

	if (verbosity > 1) {
		bu_log("[PLAN_PERTURB] src=%s  role=%s  idx=%d  bbox_diag=%.4f  factor=%.6f mm  variant=%s\n",
		       src_name.c_str(),
		       is_sub ? "SUB" : "BASE",
		       idx,
		       (double)bbox_diag,
		       (double)factor,
		       vname.c_str());
	}

	struct rt_db_internal *var_intern = NULL;
	int ret = OBJ[prim_type].ft_perturb(&var_intern, &src_intern, 0, factor);
	rt_db_free_internal(&src_intern);

	if (ret != BRLCAD_OK || !var_intern) {
		if (var_intern) {
			rt_db_free_internal(var_intern);
			BU_PUT(var_intern, struct rt_db_internal);
		}
		return BRLCAD_ERROR;
	}

	int vtype = var_intern->idb_type;
	struct directory *var_dp = db_diradd(wdbip, vname.c_str(),
						RT_DIR_PHONY_ADDR, 0,
						RT_DIR_SOLID,
						(void *)&vtype);
	int write_ret = BRLCAD_ERROR;
	if (var_dp) {
		write_ret = (rt_db_put_internal(var_dp, wdbip, var_intern,
						&rt_uniresource) >= 0)
				? BRLCAD_OK : BRLCAD_ERROR;
	}

	rt_db_free_internal(var_intern);
	BU_PUT(var_intern, struct rt_db_internal);
	return write_ret;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

FacetizeVariantPlan *
_ged_facetize_build_variant_plan(struct _ged_facetize_state *s,
					int                         argc,
					struct directory           **dpa)
{
	if (!s || !s->dbip || argc <= 0 || !dpa)
		return NULL;

	/* ============================================================
	 * Phase A: Walk - collect all leaf instances with path keys
	 *                 and boolean roles.
	 * ============================================================ */
	PlanWalkCtx ctx;
	ctx.dbip = s->dbip;

	for (int i = 0; i < argc; i++) {
		if (dpa[i])
			plan_walk_dp(&ctx, dpa[i], false);
	}

	FacetizeVariantPlan *plan = new FacetizeVariantPlan();

	if (ctx.instances.empty())
		return plan;

	/* ============================================================
	 * Phase B: Name - assign collision-safe variant names.
	 *
	 * Open the working .g to pre-collect all existing names into
	 * a std::set<std::string> (the "dbnames" set, same pattern as
	 * npush).  For each instance eligible for a variant, generate
	 * the natural name, then increment a suffix counter until the
	 * name is not in dbnames, then insert the chosen name into
	 * dbnames to reserve it for subsequent instances.
	 * ============================================================ */
	struct db_i *wdbip =
		db_open(bu_vls_cstr(s->wfile), DB_OPEN_READWRITE);
	if (!wdbip) {
		delete plan;
		return NULL;
	}
	if (db_dirbuild(wdbip) < 0) {
		db_close(wdbip);
		delete plan;
		return NULL;
	}
	db_update_nref(wdbip, &rt_uniresource);

	/* Pre-collect all names currently in the working .g */
	std::set<std::string> dbnames;
	{
		struct directory *dp;
		FOR_ALL_DIRECTORY_START(dp, wdbip)
			if (dp->d_namep)
				dbnames.insert(std::string(dp->d_namep));
		FOR_ALL_DIRECTORY_END;
	}

	/* Per-role, per-source counters to generate the natural name index */
	std::map<std::string, int> base_counters;
	std::map<std::string, int> sub_counters;

	for (auto &inst : ctx.instances) {

		/* Only create variants for primitives with ft_perturb support */
		struct directory *src_dp =
			db_lookup(wdbip, inst.src_name.c_str(), LOOKUP_QUIET);
		if (!src_dp || !OBJ[src_dp->d_minor_type].ft_perturb) {
			plan->n_perturb_fallbacks++;
			continue;
		}

		int &counter = inst.is_sub
					? sub_counters[inst.src_name]
					: base_counters[inst.src_name];
		int idx = counter++;

		/* Generate a collision-safe variant name (npush dbnames pattern) */
		int label = idx;
		std::string vname = natural_variant_name(inst.src_name, inst.is_sub, label);
		while (dbnames.find(vname) != dbnames.end() && label < INT_MAX) {
			label++;
			vname = natural_variant_name(inst.src_name, inst.is_sub, label);
		}
		if (label == INT_MAX) {
			/* Extremely unlikely name exhaustion */
			plan->n_perturb_fallbacks++;
			continue;
		}

		/* Reserve the name so subsequent variants cannot collide with it */
		dbnames.insert(vname);
		inst.vname = vname;
	}

	/* ============================================================
	 * Phase C: Rebuild lookup table then create variant primitives.
	 *
	 * Populate plan->inst_to_variant from the now-named instances
	 * first, so the lookup table is fully consistent before any
	 * database writes happen (mirrors npush's "clear+repopulate"
	 * instances-set step).
	 * ============================================================ */

	for (const auto &inst : ctx.instances) {
		if (inst.vname.empty())
			continue;
		/* Encode role in the key so that u A … - A at the same depth
		 * maps to distinct entries (one "#base", one "#sub"). */
		std::string role_key = inst.path_key +
			(inst.is_sub ? "#sub" : "#base");
		plan->inst_to_variant[role_key] = inst.vname;
		plan->n_adjusted_instances++;
		if (inst.is_sub) plan->n_sub_variants++;
	}

	/* Create variant primitives in the working .g.
	 * Track per-source creation indices separately from the naming indices
	 * so the perturb factor matches the assigned name's semantic index. */
	std::map<std::string, int> create_idx_base;
	std::map<std::string, int> create_idx_sub;
	for (const auto &inst : ctx.instances) {
		if (inst.vname.empty())
			continue;

		/* Skip if a primitive with this name was already written
		 * (handles the rare path_key duplicate case). */
		if (db_lookup(wdbip, inst.vname.c_str(), LOOKUP_QUIET) != RT_DIR_NULL)
			continue;

		int &cidx = inst.is_sub
				? create_idx_sub[inst.src_name]
				: create_idx_base[inst.src_name];
		int variant_idx = cidx++;

		fastf_t used_factor = 0.0;
		int cret = create_variant_in_working_g(wdbip,
						inst.src_name,
						inst.vname,
						inst.is_sub,
						variant_idx,
						s->verbosity,
						&used_factor);
		if (cret == BRLCAD_OK) {
			plan->variant_names.push_back(inst.vname);
			FacetizeVariantPlan::VariantRec rec;
			rec.src_name = inst.src_name;
			rec.factor   = used_factor;
			plan->variant_recs[inst.vname] = rec;
		} else {
			/* Creation failed: remove from lookup table so booleval falls back */
			std::string role_key = inst.path_key +
				(inst.is_sub ? "#sub" : "#base");
			plan->inst_to_variant.erase(role_key);
			plan->n_adjusted_instances--;
			if (inst.is_sub) plan->n_sub_variants--;
			plan->n_perturb_fallbacks++;
		}
	}

	db_close(wdbip);
	return plan;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
