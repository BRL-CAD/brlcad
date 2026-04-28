/*                       E D I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/edit.cpp
 *
 *  Command line geometry editing.
 */

#include "common.h"

#include <climits>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "./uri.hh"

#include "bu/cmd.h"
#include "bu/opt.h"
#include "bn/rand.h"
#include "rt/edit.h"

#include "../ged_private.h"
#include "../dbi.h"
#include "./ged_edit.h"


/* ------------------------------------------------------------------ *
 * Helpers: build a ged_edit_geom_spec from a URI-parsed token
 * ------------------------------------------------------------------ */

/**
 * Attempt to resolve argv[i] as a geometry specifier via DbiState.
 * Fills *spec and returns true on success, false if the token does not
 * resolve to any known database object.
 *
 * Handles:
 *   "."         — batch marker (each object acts as its own reference)
 *   "name#frag" — URI with fragment (e.g. vertex key)
 *   "name?q"    — URI with query (e.g. wildcard feature set)
 *   "path"      — bare name or slash-separated path
 */
static bool
_resolve_geom_spec(ged_edit_geom_spec &spec, const char *token,
		   struct ged *gedp, DbiState *dbis)
{
    spec.raw = token;
    spec.is_batch = false;
    spec.dp = RT_DIR_NULL;

    /* Batch marker */
    if (BU_STR_EQUAL(token, ".")) {
	spec.is_batch = true;
	/* Batch marker is valid without a db lookup */
	return true;
    }

    /* Try URI parse: prefix with "g:" so the class sees a scheme */
    std::string path_str;
    try {
	uri obj_uri(std::string("g:") + std::string(token));
	path_str = obj_uri.get_path();

	if (obj_uri.get_fragment().length() > 0)
	    spec.fragment = obj_uri.get_fragment();
	if (obj_uri.get_query().length() > 0)
	    spec.query = obj_uri.get_query();

	if (path_str.empty())
	    path_str = token;

    } catch (const std::invalid_argument &) {
	path_str = token;
    }

    spec.path = path_str;

    /* When DbiState is available, use its richer path resolution.
     * When it is absent (older code paths that have not initialised
     * DbiState), fall back to a plain db_lookup so the command still
     * functions in non-GUI contexts.  URI fragments/queries are
     * silently ignored in the no-DbiState path. */
    if (dbis) {
	spec.hashes = dbis->digest_path(path_str.c_str());

	if (spec.hashes.empty())
	    return false;

	/* Single-element path — get the head dp */
	if (spec.hashes.size() == 1)
	    spec.dp = dbis->get_hdp(spec.hashes[0]);

	/* Multi-element path (comb instance) — dp stays RT_DIR_NULL */
	return (spec.hashes.size() > 1) || (spec.dp != RT_DIR_NULL);
    }

    /* No DbiState: plain directory lookup against gedp->dbip.
     * Only bare names are reliably resolvable; for slash-paths we use
     * the last component as a best-effort lookup. */
    {
	std::string bare = path_str;
	std::string::size_type slash = bare.rfind('/');
	if (slash != std::string::npos)
	    bare = bare.substr(slash + 1);

	struct directory *dp = db_lookup(gedp->dbip, bare.c_str(), LOOKUP_QUIET);
	if (dp == RT_DIR_NULL)
	    return false;

	spec.dp = dp;
	/* Use the directory address as a stand-in hash */
	spec.hashes.push_back((unsigned long long)(uintptr_t)dp);
	return true;
    }
}


/* ================================================================== *
 * Phase 2 helpers
 * ================================================================== */

/**
 * Initialise a minimal stack-allocated bview for scripted (CLI) rt_edit
 * operations.  Only the fields accessed by edit_srot/edit_stra/edit_sscale
 * are set; nothing is heap-allocated so no cleanup is required.
 */
static void
_edit_cli_view_init(struct bview *v)
{
    memset(v, 0, sizeof(*v));
    v->magic            = BV_MAGIC;
    v->gv_scale         = 1.0;
    v->gv_rotate_about  = 'k';   /* rotate about keypoint — no view matrices needed */
    v->gv_coord         = 'm';   /* model-space coordinates */
    MAT_IDN(v->gv_model2view);
    MAT_IDN(v->gv_view2model);
    MAT_IDN(v->gv_rotation);
    MAT_IDN(v->gv_center);
}


/**
 * Return the world-space keypoint of a named top-level primitive by
 * loading the geometry and calling OBJ[type].ft_keypoint() directly,
 * which is cheaper than constructing and destroying an rt_edit session.
 * Falls back to (0,0,0) for primitives that have no ft_keypoint handler.
 */
static int
_edit_get_obj_keypoint(point_t *kp, const char *name, struct ged *gedp)
{
    struct directory *dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	return BRLCAD_ERROR;

    struct rt_db_internal ip;
    RT_DB_INTERNAL_INIT(&ip);
    if (rt_db_get_internal(&ip, dp, gedp->dbip, bn_mat_identity) < 0)
	return BRLCAD_ERROR;

    VSETALL(*kp, 0.0);

    if (OBJ[ip.idb_type].ft_keypoint) {
	struct bn_tol tol = BN_TOL_INIT_TOL;
	OBJ[ip.idb_type].ft_keypoint(kp, NULL, bn_mat_identity, &ip, &tol);
    }

    rt_db_free_internal(&ip);
    return BRLCAD_OK;
}


/**
 * Apply a scripted edit to dp through the temporary edit buffer.
 *
 *   1. If dp already has a live buffer entry (from a previous -i operation),
 *      reuse it; otherwise create a fresh rt_edit from the on-disk geometry.
 *   2. Install a minimal CLI bview so edit_srot() resolves its rotate-about
 *      axis without dereferencing a null vp.
 *   3. Run do_edit(s); return early on error.
 *   4. flag_i == 0 (normal): promote es_int to disk and clear buffer entry.
 *      flag_i != 0: leave the entry in the buffer for a future operation.
 */
static int
_edit_xform_apply(struct ged *gedp,
		  struct directory *dp,
		  int flag_i,
		  std::function<int(struct rt_edit *)> do_edit)
{
    if (!gedp || !dp)
	return BRLCAD_ERROR;

    struct db_full_path dfp;
    db_full_path_init(&dfp);
    db_add_node_to_full_path(&dfp, dp);

    /* Re-use existing buffer entry, or create a fresh one */
    struct rt_edit *s = ged_edit_buf_get(gedp, &dfp);
    bool is_new = (s == NULL);

    if (is_new) {
	struct bn_tol tol = BN_TOL_INIT_TOL;
	s = rt_edit_create(&dfp, gedp->dbip, &tol, NULL);
	if (!s) {
	    db_free_full_path(&dfp);
	    return BRLCAD_ERROR;
	}
    }

    /* Temporarily install a minimal CLI bview (stack-allocated) */
    struct bview cli_v;
    _edit_cli_view_init(&cli_v);
    struct bview *saved_vp = s->vp;
    s->vp = &cli_v;

    int ret = do_edit(s);

    s->vp = saved_vp;

    if (ret != BRLCAD_OK) {
	if (is_new)
	    rt_edit_destroy(s);
	db_free_full_path(&dfp);
	return BRLCAD_ERROR;
    }

    if (is_new) {
	/* Transfer ownership to the buffer */
	ged_edit_buf_set(gedp, &dfp, s);
    }
    /* s is now in the buffer */

    int result;
    if (flag_i) {
	result = BRLCAD_OK;                       /* leave in buffer */
    } else {
	result = ged_edit_buf_promote(gedp, &dfp); /* write to disk  */
    }

    db_free_full_path(&dfp);
    return result;
}


/* ================================================================== *
 * Subcommand implementations
 * ================================================================== */

/* Forward declaration — full definition appears after cmd_tra (below) */
static int _parse_pos_or_obj(point_t *pos, const char **argv, int argc, struct ged *gedp);

/* ------------------------------------------------------------------ *
 * translate
 * ------------------------------------------------------------------ */
class cmd_translate : public ged_subcmd {
    public:
	std::string usage()   { return std::string("edit [options] [geometry] translate [-a|-r] [-x|-y|-z] [-k FROM] TO"); }
	std::string purpose() { return std::string("translate primitive or comb instance"); }
	int exec(struct ged *, void *, int, const char **);
};
static cmd_translate edit_translate_cmd;

int
cmd_translate::exec(struct ged *gedp, void *u_data, int argc, const char **argv)
{
    if (!gedp || !u_data || !argc || !argv)
	return BRLCAD_ERROR;

    struct ged_edit_ctx *ctx = (struct ged_edit_ctx *)u_data;
    if (ctx->dp == RT_DIR_NULL)
	return BRLCAD_ERROR;

    argc--; argv++;   /* skip "translate" */

    /* ---- Manual option scan ----------------------------------------
     * Supported options:
     *   -a           → absolute mode
     *   -r           → relative mode (default)
     *   -x / -y / -z → per-axis constraint
     *   -n           → natural origin (accepted, ignored)
     *   -k POS|OBJ|. → FROM reference position/object/self-ref
     * Positional args supply the TO destination (coords or object or .)
     * --------------------------------------------------------------- */
    int abs_flag = 0, rel_flag = 0;
    int x_only = 0, y_only = 0, z_only = 0;

    bool   have_k    = false;
    bool   k_is_self = false;   /* -k . → use own keypoint */
    point_t k_pos    = VINIT_ZERO;

    bool   to_is_self = false;  /* positional "." → use own keypoint */
    const char *to_name = NULL; /* positional object name */
    vect_t to_vec = VINIT_ZERO;
    bool   have_to = false;

    int i = 0;
    while (i < argc) {
	if (BU_STR_EQUAL(argv[i], "-a")) { abs_flag = 1; i++; continue; }
	if (BU_STR_EQUAL(argv[i], "-r")) { rel_flag = 1; i++; continue; }
	if (BU_STR_EQUAL(argv[i], "-x")) { x_only   = 1; i++; continue; }
	if (BU_STR_EQUAL(argv[i], "-y")) { y_only   = 1; i++; continue; }
	if (BU_STR_EQUAL(argv[i], "-z")) { z_only   = 1; i++; continue; }
	if (BU_STR_EQUAL(argv[i], "-n")) { i++;           continue; }

	if (BU_STR_EQUAL(argv[i], "-k") && i + 1 < argc) {
	    i++;
	    if (BU_STR_EQUAL(argv[i], ".")) {
		/* "." → use the target object's own keypoint as FROM */
		k_is_self = true;
		i++;
	    } else {
		/* Accept 1–3 floats OR an object name */
		point_t tmp = VINIT_ZERO;
		int nr = _parse_pos_or_obj(&tmp, argv + i, argc - i, gedp);
		if (nr < 1) {
		    bu_vls_printf(gedp->ged_result_str,
			"translate: bad -k argument '%s'\n", argv[i]);
		    return BRLCAD_ERROR;
		}
		VMOVE(k_pos, tmp);
		i += nr;
	    }
	    have_k = true;
	    continue;
	}

	/* Positional: TO destination — also accept negative numbers like "-5" */
	{
	    bool is_neg_num = (argv[i][0] == '-') &&
		(isdigit((unsigned char)argv[i][1]) || argv[i][1] == '.');
	    if ((argv[i][0] != '-' || is_neg_num) && !have_to) {
		if (BU_STR_EQUAL(argv[i], ".")) {
		    /* "." → use the target object's own keypoint as TO */
		    to_is_self = true;
		    have_to    = true;
		    i++;
		} else {
		    int n_coord_flags_l = x_only + y_only + z_only;
		    if (n_coord_flags_l == 0) {
			/* Try 3-vector, then 1-scalar, then object name */
			int vret = bu_opt_vect_t(NULL, argc - i, argv + i, &to_vec);
			if (vret > 0) {
			    have_to = true;
			    i += vret;
			} else {
			    fastf_t f;
			    if (bu_opt_fastf_t(NULL, 1, &argv[i], &f) >= 0) {
				to_vec[X] = to_vec[Y] = to_vec[Z] = f;
				have_to = true;
				i++;
			    } else {
				to_name = argv[i];
				have_to = true;
				i++;
			    }
			}
		    } else {
			/* Per-axis mode: each axis flag consumes one value or object name */
			if (x_only) {
			    if (bu_opt_fastf_t(NULL, 1, &argv[i], &to_vec[X]) >= 0) {
				i++;
			    } else {
				to_name = argv[i];
				i++;
			    }
			}
			if (y_only && i < argc) {
			    if (bu_opt_fastf_t(NULL, 1, &argv[i], &to_vec[Y]) >= 0) {
				i++;
			    } else if (!to_name) {
				to_name = argv[i];
				i++;
			    }
			}
			if (z_only && i < argc) {
			    if (bu_opt_fastf_t(NULL, 1, &argv[i], &to_vec[Z]) >= 0) {
				i++;
			    } else if (!to_name) {
				to_name = argv[i];
				i++;
			    }
			}
			have_to = true;
		    }
		}
		continue;
	    }
	}

	i++;  /* skip unrecognised tokens */
    }

    if (!have_to && !to_is_self && !to_name) {
	bu_vls_printf(gedp->ged_result_str, "translate: missing destination\n");
	return BRLCAD_ERROR;
    }

    int  flag_i    = ctx->flag_i;
    int  n_coord_flags = x_only + y_only + z_only;
    bool do_abs    = (abs_flag && !rel_flag);

    return _edit_xform_apply(gedp, ctx->dp, flag_i,
	[&](struct rt_edit *s) -> int
	{
	    /* Resolve FROM position (k) */
	    point_t from_kp = VINIT_ZERO;
	    bool     have_from = false;
	    if (k_is_self) {
		VMOVE(from_kp, s->e_keypoint);
		have_from = true;
	    } else if (have_k) {
		VMOVE(from_kp, k_pos);
		have_from = true;
	    }

	    /* Resolve TO position */
	    vect_t target;
	    if (to_is_self) {
		/* "." as TO: use own keypoint — meaningful when -k FROM is given */
		point_t own_kp;
		VMOVE(own_kp, s->e_keypoint);
		if (have_from) {
		    vect_t delta;
		    VSUB2(delta, own_kp, from_kp);
		    VADD2(target, s->e_keypoint, delta);
		} else {
		    /* No -k: translate by zero (no-op, but valid) */
		    VMOVE(target, s->e_keypoint);
		}
	    } else if (to_name) {
		/* TO is an object name */
		point_t to_kp;
		if (_edit_get_obj_keypoint(&to_kp, to_name, gedp) != BRLCAD_OK) {
		    bu_vls_printf(gedp->ged_result_str,
			"translate: cannot resolve object '%s'\n", to_name);
		    return BRLCAD_ERROR;
		}
		if (n_coord_flags > 0) {
		    /* Per-component extraction: use flagged axes from to_kp,
		     * keep other axes at current keypoint (absolute move). */
		    VMOVE(target, s->e_keypoint);
		    if (x_only) target[X] = to_kp[X];
		    if (y_only) target[Y] = to_kp[Y];
		    if (z_only) target[Z] = to_kp[Z];
		} else if (have_from) {
		    /* Move so that FROM keypoint coincides with TO keypoint */
		    vect_t delta;
		    VSUB2(delta, to_kp, from_kp);
		    VADD2(target, s->e_keypoint, delta);
		} else {
		    /* Absolute: move directly to the object's keypoint */
		    VMOVE(target, to_kp);
		}
	    } else if (do_abs) {
		/* Absolute translate: to_vec IS the target position */
		VMOVE(target, to_vec);
		/* Keep unspecified axes at the current keypoint */
		if (n_coord_flags > 0) {
		    if (!x_only) target[X] = s->e_keypoint[X];
		    if (!y_only) target[Y] = s->e_keypoint[Y];
		    if (!z_only) target[Z] = s->e_keypoint[Z];
		}
		/* When -k FROM is given alongside explicit coordinates,
		 * treat to_vec as the TO reference and move by (TO - FROM). */
		if (have_from) {
		    vect_t delta;
		    VSUB2(delta, target, from_kp);
		    VADD2(target, s->e_keypoint, delta);
		}
	    } else {
		/* Relative translate (default): to_vec is a delta */
		vect_t delta = VINIT_ZERO;
		if (n_coord_flags > 0) {
		    if (x_only) delta[X] = to_vec[X];
		    if (y_only) delta[Y] = to_vec[Y];
		    if (z_only) delta[Z] = to_vec[Z];
		} else {
		    VMOVE(delta, to_vec);
		}
		VADD2(target, s->e_keypoint, delta);
	    }

	    VMOVE(s->e_para, target);
	    s->e_inpara = 3;
	    rt_edit_set_edflag(s, RT_PARAMS_EDIT_TRANS);
	    rt_edit_process(s);
	    return BRLCAD_OK;
	});
}


/* ------------------------------------------------------------------ *
 * tra (alias: translate -r)
 * ------------------------------------------------------------------ */
class cmd_tra : public ged_subcmd {
    public:
	std::string usage()   { return std::string("edit [options] [geometry] tra X Y Z"); }
	std::string purpose() { return std::string("relative translate (alias for translate -r X Y Z)"); }
	int exec(struct ged *, void *, int, const char **);
};
static cmd_tra edit_tra_cmd;

int
cmd_tra::exec(struct ged *gedp, void *u_data, int argc, const char **argv)
{
    if (!gedp || !u_data || !argc || !argv)
	return BRLCAD_ERROR;

    struct ged_edit_ctx *ctx = (struct ged_edit_ctx *)u_data;
    if (ctx->dp == RT_DIR_NULL)
	return BRLCAD_ERROR;

    argc--; argv++;   /* skip "tra" */

    vect_t delta = VINIT_ZERO;
    int vret = bu_opt_vect_t(NULL, argc, argv, &delta);
    if (vret < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", usage().c_str());
	return BRLCAD_ERROR;
    }

    return _edit_xform_apply(gedp, ctx->dp, ctx->flag_i,
	[&](struct rt_edit *s) -> int
	{
	    vect_t target;
	    VADD2(target, s->e_keypoint, delta);
	    VMOVE(s->e_para, target);
	    s->e_inpara = 3;
	    rt_edit_set_edflag(s, RT_PARAMS_EDIT_TRANS);
	    rt_edit_process(s);
	    return BRLCAD_OK;
	});
}


/* ------------------------------------------------------------------ *
 * Phase C helper: parse a position (1–3 floats) or object-name keypoint.
 * Returns the number of argv tokens consumed (≥ 1), or -1 on failure.
 * ------------------------------------------------------------------ */
static int
_parse_pos_or_obj(point_t *pos, const char **argv, int argc, struct ged *gedp)
{
    if (argc < 1 || !argv || !argv[0])
	return -1;

    fastf_t f0;
    if (bu_opt_fastf_t(NULL, 1, argv, &f0) >= 0) {
	/* Numeric — consume 1–3 floats */
	(*pos)[X] = (*pos)[Y] = (*pos)[Z] = f0;
	int n = 1;
	fastf_t f1, f2;
	if (n < argc && bu_opt_fastf_t(NULL, 1, argv + n, &f1) >= 0) {
	    (*pos)[Y] = f1;
	    n++;
	    if (n < argc && bu_opt_fastf_t(NULL, 1, argv + n, &f2) >= 0) {
		(*pos)[Z] = f2;
		n++;
	    }
	}
	return n;
    }

    /* Not numeric — try as an object name */
    if (_edit_get_obj_keypoint(pos, argv[0], gedp) == BRLCAD_OK)
	return 1;
    return -1;
}


/* ------------------------------------------------------------------ *
 * rotate
 * ------------------------------------------------------------------ */
class cmd_rotate : public ged_subcmd {
    public:
	std::string usage()   { return std::string("edit [options] [geometry] rotate [-R] [-x|-y|-z] [[-k AXIS_FROM] [-a|-r AXIS_TO] [-c CENTER] [-d DEGREES]] X [Y [Z]]"); }
	std::string purpose() { return std::string("rotate primitive or comb instance (Euler angles, or arbitrary-axis with -k/-a/-r)"); }
	int exec(struct ged *, void *, int, const char **);
};
static cmd_rotate edit_rotate_cmd;

int
cmd_rotate::exec(struct ged *gedp, void *u_data, int argc, const char **argv)
{
    if (!gedp || !u_data || !argc || !argv)
	return BRLCAD_ERROR;

    struct ged_edit_ctx *ctx = (struct ged_edit_ctx *)u_data;
    if (ctx->dp == RT_DIR_NULL)
	return BRLCAD_ERROR;

    argc--; argv++;   /* skip "rotate" */

    /* ---- Manual option scan --------------------------------------- *
     * Supported options:                                               *
     *   -R                → angles are radians                        *
     *   -x / -y / -z      → Euler single-axis constrain               *
     *   -o                → override axis constraint (Euler mode)      *
     *   -k POS|OBJ        → axis-from (1st use) or angle-from (2nd)  *
     *   -a POS|OBJ        → axis-to   (1st use) or angle-to  (2nd)   *
     *   -r POS|OBJ        → axis-to relative or angle-to relative     *
     *   -c POS|OBJ|.      → rotation center (.=own keypoint)          *
     *   -O POS|OBJ        → angle origin (angle-from reference)       *
     *   -d DEGREES        → explicit rotation angle (axis mode)       *
     *                                                                  *
     * Two -k/-a pairs are supported in axis mode:                     *
     *   first  -k/-a: defines the rotation axis                       *
     *   second -k/-a: defines ANGLE_FROM / ANGLE_TO for implicit      *
     *                 angle derivation (no -d required when present)  *
     *                                                                  *
     * When axis mode is active and no explicit angle (-d or positional)*
     * is given, the rotation angle is derived automatically by         *
     * projecting the ANGLE_FROM → ANGLE_TO arc onto the plane         *
     * perpendicular to the axis through the rotation center.  When no *
     * explicit angle references are supplied, AXIS_FROM and AXIS_TO   *
     * serve double duty as ANGLE_FROM and ANGLE_TO.                   *
     * ----------------------------------------------------------------*/
    bool rad_flag = false;
    bool x_only = false, y_only = false, z_only = false;

    /* Axis mode state */
    bool have_axis_from [[maybe_unused]] = false, have_axis_to = false;
    bool axis_to_rel    = false;
    point_t axis_from   = VINIT_ZERO;   /* axis start position */
    point_t axis_to     = VINIT_ZERO;   /* axis end   position */

    /* Angle reference state (second -k/-a pair, or falls back to axis pts) */
    bool have_angle_from = false, have_angle_to = false;
    bool angle_to_rel [[maybe_unused]] = false;
    point_t angle_from_pos = VINIT_ZERO;
    point_t angle_to_pos   = VINIT_ZERO;

    /* Center / angle-origin */
    bool have_center = false, have_angle_origin = false;
    bool center_is_self = false;   /* -c . → use own keypoint as pivot */
    point_t center       = VINIT_ZERO;
    point_t angle_origin = VINIT_ZERO;

    /* Explicit angle override (-d) */
    bool have_d_angle = false;
    fastf_t d_angle   = 0.0;

    /* Positional angle values (Euler or single) */
    vect_t angles = VINIT_ZERO;
    int n_angle_vals = 0;

    /* Pending flag: last -k/-a/-r seen waits for a positional arg */
    int pending = 0;   /* 0 = none, 'k' = -k, 'a' = -a, 'r' = -r */

    int i = 0;
    while (i < argc) {
	/* ---- Boolean flags ---------------------------------------- */
	if (BU_STR_EQUAL(argv[i], "-R")) { rad_flag = true; i++; continue; }
	if (BU_STR_EQUAL(argv[i], "-x")) { x_only   = true; i++; continue; }
	if (BU_STR_EQUAL(argv[i], "-y")) { y_only   = true; i++; continue; }
	if (BU_STR_EQUAL(argv[i], "-z")) { z_only   = true; i++; continue; }
	if (BU_STR_EQUAL(argv[i], "-o")) { /* override — noted, ignored */ i++; continue; }
	if (BU_STR_EQUAL(argv[i], "-n")) { i++; continue; }   /* natural-origin modifier */

	/* ---- Two-token options ------------------------------------ */
	if (BU_STR_EQUAL(argv[i], "-d") && i + 1 < argc) {
	    i++;
	    if (bu_opt_fastf_t(NULL, 1, &argv[i], &d_angle) < 0) {
		bu_vls_printf(gedp->ged_result_str,
		    "rotate: bad -d angle '%s'\n", argv[i]);
		return BRLCAD_ERROR;
	    }
	    have_d_angle = true;
	    i++;
	    continue;
	}

	if (BU_STR_EQUAL(argv[i], "-c")) {
	    i++;
	    if (i < argc && BU_STR_EQUAL(argv[i], ".")) {
		/* "." → use the target object's own keypoint as center */
		center_is_self = true;
		i++;
	    } else {
		int nr = _parse_pos_or_obj(&center, argv + i, argc - i, gedp);
		if (nr < 1) {
		    bu_vls_printf(gedp->ged_result_str,
			"rotate: bad -c center argument '%s'\n",
			(i < argc) ? argv[i] : "(missing)");
		    return BRLCAD_ERROR;
		}
		have_center = true;
		i += nr;
	    }
	    continue;
	}

	if (BU_STR_EQUAL(argv[i], "-O")) {
	    i++;
	    int nr = _parse_pos_or_obj(&angle_origin, argv + i, argc - i, gedp);
	    if (nr < 1) {
		bu_vls_printf(gedp->ged_result_str,
		    "rotate: bad -O argument '%s'\n",
		    (i < argc) ? argv[i] : "(missing)");
		return BRLCAD_ERROR;
	    }
	    have_angle_origin = true;
	    i += nr;
	    continue;
	}

	/* ---- Pending-position options ------------------------------ */
	if (BU_STR_EQUAL(argv[i], "-k")) { pending = 'k'; i++; continue; }
	if (BU_STR_EQUAL(argv[i], "-a")) { pending = 'a'; i++; continue; }
	if (BU_STR_EQUAL(argv[i], "-r")) { pending = 'r'; i++; continue; }

	/* ---- Positional / pending argument ------------------------- */
	if (pending) {
	    point_t tmp_pos = VINIT_ZERO;
	    int nr = _parse_pos_or_obj(&tmp_pos, argv + i, argc - i, gedp);
	    if (nr < 1) {
		bu_vls_printf(gedp->ged_result_str,
		    "rotate: bad argument for -%c: '%s'\n",
		    pending, argv[i]);
		return BRLCAD_ERROR;
	    }
	    if (pending == 'k') {
		if (!have_axis_to) {
		    /* First -k: sets axis-from */
		    VMOVE(axis_from, tmp_pos);
		    have_axis_from = true;
		} else {
		    /* Second -k: sets angle-from reference */
		    VMOVE(angle_from_pos, tmp_pos);
		    have_angle_from = true;
		}
	    } else if (pending == 'a') {
		if (!have_axis_to) {
		    /* First -a: sets axis-to (absolute) */
		    VMOVE(axis_to, tmp_pos);
		    have_axis_to  = true;
		    axis_to_rel   = false;
		} else {
		    /* Second -a: sets angle-to reference (absolute) */
		    VMOVE(angle_to_pos, tmp_pos);
		    have_angle_to  = true;
		    angle_to_rel   = false;
		}
	    } else { /* 'r' */
		if (!have_axis_to) {
		    /* First -r: sets axis-to (relative to axis-from) */
		    VMOVE(axis_to, tmp_pos);
		    have_axis_to  = true;
		    axis_to_rel   = true;
		} else {
		    /* Second -r: sets angle-to reference (relative) */
		    VMOVE(angle_to_pos, tmp_pos);
		    have_angle_to  = true;
		    angle_to_rel   = true;
		}
	    }
	    pending = 0;
	    i += nr;
	    continue;
	}

	/* ---- Positional angle values ------------------------------- */
	if (argv[i][0] != '-') {
	    /* Read up to 3 floats (Euler angles or single rotation amount) */
	    int vret = bu_opt_vect_t(NULL, argc - i, argv + i, &angles);
	    if (vret > 0) {
		n_angle_vals = vret;
		i += vret;
	    } else {
		fastf_t f;
		if (bu_opt_fastf_t(NULL, 1, &argv[i], &f) >= 0) {
		    angles[X] = angles[Y] = angles[Z] = f;
		    n_angle_vals = 1;
		    i++;
		} else {
		    i++;   /* skip unrecognised */
		}
	    }
	    continue;
	}

	/* skip unrecognised flag tokens */
	i++;
    }

    /* ============================================================ *
     *  Mode dispatch                                                *
     * ============================================================ */

    if (have_axis_to) {
	/* ---- AXIS MODE: rotate around an explicit axis ----------- *
	 * Build the axis direction vector.  The rotation angle is     *
	 * either:                                                      *
	 *   (a) explicit via -d DEGREES or a trailing positional      *
	 *   (b) derived from projecting ANGLE_FROM→ANGLE_TO onto the  *
	 *       plane perpendicular to the axis through the center     *
	 *                                                              *
	 * When no explicit angle reference pair is supplied, AXIS_FROM *
	 * and AXIS_TO double as ANGLE_FROM and ANGLE_TO.              */

	/* Build axis direction */
	vect_t axis_dir;
	if (axis_to_rel) {
	    VADD2(axis_dir, axis_from, axis_to);
	} else {
	    VSUB2(axis_dir, axis_to, axis_from);
	}
	fastf_t axlen = MAGNITUDE(axis_dir);
	if (axlen < SMALL_FASTF) {
	    bu_vls_printf(gedp->ged_result_str,
		"rotate: axis-from and axis-to are the same point; "
		"cannot determine rotation axis\n");
	    return BRLCAD_ERROR;
	}
	VSCALE(axis_dir, axis_dir, 1.0 / axlen);

	/* Determine whether we use an explicit angle or compute it. */
	bool have_explicit_angle = (have_d_angle || n_angle_vals >= 1);
	fastf_t explicit_angle_rad = 0.0;
	if (have_explicit_angle) {
	    fastf_t aval = have_d_angle ? d_angle : angles[X];
	    explicit_angle_rad = rad_flag ? aval : aval * DEG2RAD;
	}

	/* Implicit angle references: default to (axis_from, axis_to)  *
	 * when no explicit angle reference pair was given.             */
	point_t af_ref, at_ref;
	VMOVE(af_ref, have_angle_from ? angle_from_pos : axis_from);
	VMOVE(at_ref, have_angle_to   ? angle_to_pos   : axis_to);

	/* Rotation center: -c CENTER overrides, then -O, then default */
	bool use_custom_center = have_center || center_is_self;
	point_t rot_center = VINIT_ZERO;
	if (have_center)      VMOVE(rot_center, center);
	else if (have_angle_origin) VMOVE(rot_center, angle_origin);
	/* center_is_self: resolved inside lambda from s->e_keypoint */

	return _edit_xform_apply(gedp, ctx->dp, ctx->flag_i,
	    [&](struct rt_edit *s) -> int
	    {
		/* Resolve actual rotation center */
		point_t actual_center;
		if (center_is_self) {
		    VMOVE(actual_center, s->e_keypoint);
		} else if (use_custom_center || have_angle_origin) {
		    VMOVE(actual_center, rot_center);
		} else {
		    VMOVE(actual_center, s->e_keypoint);
		}

		/* Determine the rotation angle in radians */
		fastf_t angle_rad;
		if (have_explicit_angle) {
		    angle_rad = explicit_angle_rad;
		} else {
		    /* Derive angle by projecting af_ref and at_ref onto   *
		     * the plane perpendicular to axis_dir through center. */
		    vect_t vf, vt;
		    VSUB2(vf, af_ref, actual_center);
		    VSUB2(vt, at_ref, actual_center);

		    /* Remove components along the rotation axis */
		    fastf_t df = VDOT(vf, axis_dir);
		    fastf_t dt = VDOT(vt, axis_dir);
		    vect_t vf_proj, vt_proj;
		    VJOIN1(vf_proj, vf, -df, axis_dir);
		    VJOIN1(vt_proj, vt, -dt, axis_dir);

		    fastf_t lf = MAGNITUDE(vf_proj);
		    fastf_t lt = MAGNITUDE(vt_proj);
		    if (lf < SMALL_FASTF || lt < SMALL_FASTF) {
			bu_vls_printf(gedp->ged_result_str,
			    "rotate: angle reference point lies on the "
			    "rotation axis; cannot derive rotation angle\n");
			return BRLCAD_ERROR;
		    }

		    /* Signed angle from vf_proj to vt_proj around axis_dir:
		     *   angle = atan2(dot(axis, cross(vf, vt)), dot(vf, vt)) */
		    vect_t cross_vf_vt;
		    VCROSS(cross_vf_vt, vf_proj, vt_proj);
		    angle_rad = atan2(VDOT(axis_dir, cross_vf_vt),
				     VDOT(vf_proj,  vt_proj));
		}

		/* Build a pure rotation matrix about the world origin,     *
		 * then apply it with the resolved center as the pivot.     */
		mat_t pure_rot;
		MAT_IDN(pure_rot);
		point_t world_origin = VINIT_ZERO;
		bn_mat_arb_rot(pure_rot, world_origin, axis_dir, angle_rad);

		VMOVE(s->e_keypoint, actual_center);
		s->e_keyfixed = 1;
		MAT_IDN(s->acc_rot_sol);
		MAT_COPY(s->incr_change, pure_rot);
		bn_mat_mul2(s->incr_change, s->acc_rot_sol);
		s->e_inpara = 0;
		rt_edit_set_edflag(s, RT_PARAMS_EDIT_ROT);
		rt_edit_process(s);
		return BRLCAD_OK;
	    });
    }

    /* ---- EULER MODE (existing path) ------------------------------ */

    int n_coord_flags = (int)x_only + (int)y_only + (int)z_only;

    if (n_coord_flags == 0) {
	if (n_angle_vals < 1) {
	    /* No positional angles: check for a -d angle */
	    if (have_d_angle) {
		angles[Z] = d_angle;
		n_angle_vals = 1;
	    } else {
		bu_vls_printf(gedp->ged_result_str,
		    "rotate: missing angle(s)\n");
		return BRLCAD_ERROR;
	    }
	} else if (n_angle_vals == 1) {
	    /* Single unconstrained angle — 180° is ambiguous */
	    fastf_t adeg = rad_flag ? (angles[X] * RAD2DEG) : angles[X];
	    if (NEAR_EQUAL(fabs(adeg), 180.0, 1e-10)) {
		bu_vls_printf(gedp->ged_result_str,
		    "rotate: single-angle 180° is ambiguous — "
		    "specify axis with -x/-y/-z or use three angles\n");
		return BRLCAD_ERROR;
	    }
	    /* Default single-angle axis: Z */
	    angles[Z] = angles[X];
	    angles[X] = 0.0;
	    angles[Y] = 0.0;
	}
	/* Two or three angles: already in angles[X/Y/Z] */
    } else if (n_coord_flags == 1) {
	/* Single axis flag: the angle is in the positional arg (may *
	 * already be in angles[X] after bu_opt_vect_t) or -d. */
	fastf_t a = (have_d_angle ? d_angle : (n_angle_vals >= 1 ? angles[X] : 0.0));
	VSETALL(angles, 0.0);
	if (x_only) angles[X] = a;
	else if (y_only) angles[Y] = a;
	else angles[Z] = a;
    } else {
	/* Multiple axis flags: one angle per flag in X/Y/Z order */
	vect_t tmp = VINIT_ZERO;
	int ci = 0;
	if (x_only && ci < n_angle_vals) { tmp[X] = angles[ci++]; }
	if (y_only && ci < n_angle_vals) { tmp[Y] = angles[ci++]; }
	if (z_only && ci < n_angle_vals) { tmp[Z] = angles[ci]; }
	VMOVE(angles, tmp);
    }

    /* Convert radians → degrees if -R was given (librt works in degrees) */
    if (rad_flag) {
	angles[X] *= RAD2DEG;
	angles[Y] *= RAD2DEG;
	angles[Z] *= RAD2DEG;
    }

    bool use_euler_center = have_center || center_is_self;
    point_t euler_center = VINIT_ZERO;
    if (have_center) VMOVE(euler_center, center);
    else if (have_angle_origin) {
	use_euler_center = true;
	VMOVE(euler_center, angle_origin);
    }
    /* center_is_self: resolved inside lambda */

    return _edit_xform_apply(gedp, ctx->dp, ctx->flag_i,
	[&](struct rt_edit *s) -> int
	{
	    if (center_is_self) {
		/* Own keypoint is already in s->e_keypoint; just pin it */
		s->e_keyfixed = 1;
	    } else if (use_euler_center) {
		VMOVE(s->e_keypoint, euler_center);
		s->e_keyfixed = 1;
	    }
	    /* Reset accumulated rotation so the supplied angles are applied
	     * relative to the current geometry state, not accumulated with
	     * any previous interactive rotation */
	    MAT_IDN(s->acc_rot_sol);
	    s->e_para[0] = angles[X];
	    s->e_para[1] = angles[Y];
	    s->e_para[2] = angles[Z];
	    s->e_inpara  = 3;
	    rt_edit_set_edflag(s, RT_PARAMS_EDIT_ROT);
	    rt_edit_process(s);
	    return BRLCAD_OK;
	});
}


/* ------------------------------------------------------------------ *
 * scale
 * ------------------------------------------------------------------ */
class cmd_scale : public ged_subcmd {
    public:
	std::string usage()   { return std::string("edit [options] [geometry] scale [-k FROM] [-c CENTER] [-a|-r TO] FACTOR"); }
	std::string purpose() { return std::string("scale primitive or comb instance (uniform or per-axis factor > 0)"); }
	int exec(struct ged *, void *, int, const char **);
};
static cmd_scale edit_scale_cmd;

int
cmd_scale::exec(struct ged *gedp, void *u_data, int argc, const char **argv)
{
    if (!gedp || !u_data || !argc || !argv)
	return BRLCAD_ERROR;

    struct ged_edit_ctx *ctx = (struct ged_edit_ctx *)u_data;
    if (ctx->dp == RT_DIR_NULL)
	return BRLCAD_ERROR;

    argc--; argv++;   /* skip "scale" */

    /* Manual scan: -k/-a/-r/-c each take 1–3 number or object-name args. */
    vect_t k_vec = VINIT_ZERO;
    vect_t a_vec = VINIT_ZERO;
    vect_t r_vec = VINIT_ZERO;
    bool have_k = false, have_a = false, have_r = false;

    bool have_center = false;
    bool center_is_self = false;   /* -c . → use own keypoint as pivot */
    point_t center_pos = VINIT_ZERO;

    vect_t pos_vals = VINIT_ZERO;
    int n_pos = 0;

    int i = 0;
    while (i < argc) {
	if (BU_STR_EQUAL(argv[i], "-k") && i + 1 < argc) {
	    i++;
	    point_t tmp = VINIT_ZERO;
	    int nr = _parse_pos_or_obj(&tmp, argv + i, argc - i, gedp);
	    if (nr < 1) {
		bu_vls_printf(gedp->ged_result_str,
		    "scale: bad -k value '%s'\n", argv[i]);
		return BRLCAD_ERROR;
	    }
	    VMOVE(k_vec, tmp);
	    i += nr;
	    have_k = true;
	} else if (BU_STR_EQUAL(argv[i], "-a") && i + 1 < argc) {
	    i++;
	    point_t tmp = VINIT_ZERO;
	    int nr = _parse_pos_or_obj(&tmp, argv + i, argc - i, gedp);
	    if (nr < 1) {
		bu_vls_printf(gedp->ged_result_str,
		    "scale: bad -a value '%s'\n", argv[i]);
		return BRLCAD_ERROR;
	    }
	    VMOVE(a_vec, tmp);
	    i += nr;
	    have_a = true;
	} else if (BU_STR_EQUAL(argv[i], "-r") && i + 1 < argc) {
	    i++;
	    point_t tmp = VINIT_ZERO;
	    int nr = _parse_pos_or_obj(&tmp, argv + i, argc - i, gedp);
	    if (nr > 0) {
		VMOVE(r_vec, tmp);
		i += nr;
	    } else {
		fastf_t f = 0.0;
		if (bu_opt_fastf_t(NULL, 1, &argv[i], &f) < 0) {
		    bu_vls_printf(gedp->ged_result_str,
			"scale: bad -r value '%s'\n", argv[i]);
		    return BRLCAD_ERROR;
		}
		r_vec[X] = r_vec[Y] = r_vec[Z] = f;
		i++;
	    }
	    have_r = true;
	} else if (BU_STR_EQUAL(argv[i], "-c") && i + 1 < argc) {
	    i++;
	    if (BU_STR_EQUAL(argv[i], ".")) {
		/* "." → use the target object's own keypoint as pivot */
		center_is_self = true;
		i++;
	    } else {
		int nr = _parse_pos_or_obj(&center_pos, argv + i, argc - i, gedp);
		if (nr < 1) {
		    bu_vls_printf(gedp->ged_result_str,
			"scale: bad -c center value '%s'\n", argv[i]);
		    return BRLCAD_ERROR;
		}
		have_center = true;
		i += nr;
	    }
	} else if (BU_STR_EQUAL(argv[i], "-n")) {
	    i++;
	} else if (argv[i][0] != '-') {
	    /* Positional: the scale factor (1 or 3 numbers) */
	    int vret = bu_opt_vect_t(NULL, argc - i, argv + i, &pos_vals);
	    if (vret > 0) {
		n_pos = vret;
		i += vret;
	    } else {
		fastf_t f = 0.0;
		if (bu_opt_fastf_t(NULL, 1, &argv[i], &f) < 0) {
		    bu_vls_printf(gedp->ged_result_str,
			"scale: bad factor value '%s'\n", argv[i]);
		    return BRLCAD_ERROR;
		}
		pos_vals[X] = pos_vals[Y] = pos_vals[Z] = f;
		n_pos = 1;
		i++;
	    }
	} else {
	    i++;   /* unknown flag — skip */
	}
    }

    /* ---- Determine scale factors from whatever was provided -------- */
    /* Helper: detect if the three vector components are all nearly equal */
    auto vec_is_uniform = [](const vect_t v) -> bool {
	return NEAR_EQUAL(v[X], v[Y], SMALL_FASTF) &&
	       NEAR_EQUAL(v[Y], v[Z], SMALL_FASTF);
    };

    /* Helper: compute a single (uniform) factor from a vector */
    auto vec_to_factor = [](const vect_t v) -> fastf_t {
	if (NEAR_EQUAL(v[X], v[Y], SMALL_FASTF) &&
	    NEAR_EQUAL(v[Y], v[Z], SMALL_FASTF))
	    return v[X];
	return sqrt(VDOT(v, v) / 3.0);
    };

    /* Factor vector (per-axis scale factors before any reference division) */
    vect_t factors = VINIT_ZERO;
    bool have_factors = false;

    if (have_k && have_a && have_r) {
	/* Full ratio model: (-k FROM -a SCALE_TO) defines a SCALE reference
	 * ruler; -r FACTOR is the desired result ruler.
	 * effective_factor[i] = FACTOR[i] / |SCALE_VEC[i]|
	 * This is the two-pair form from the original edit.c design:
	 *   SCALE = SCALE_TO - SCALE_FROM   (the reference "ruler")
	 *   FACTOR = r_vec                  (the desired result "ruler")
	 *   effective = FACTOR / |SCALE|    per axis
	 * Example: -k 5 10 15 -a 7 11 -2 -r 4 2 34
	 *   SCALE = (2, 1, 17) → effective = (4/2, 2/1, 34/17) = (2, 2, 2) */
	vect_t scale_vec;
	VSUB2(scale_vec, a_vec, k_vec);
	for (int j = 0; j < 3; j++) {
	    double sv = fabs(scale_vec[j]);
	    if (sv < SMALL_FASTF) {
		/* SCALE has zero extent on this axis; use FACTOR directly */
		factors[j] = r_vec[j];
	    } else {
		factors[j] = r_vec[j] / sv;
	    }
	}
	have_factors = true;
    } else if (have_r) {
	VMOVE(factors, r_vec);
	have_factors = true;
    } else if (have_k && have_a) {
	/* Factor from the FROM→TO reference vector */
	VSUB2(factors, a_vec, k_vec);
	have_factors = true;
    } else if (n_pos > 0) {
	VMOVE(factors, pos_vals);
	have_factors = true;
    }

    if (!have_factors) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", usage().c_str());
	return BRLCAD_ERROR;
    }

    /* Decide: uniform scale or per-axis scale? */
    bool is_uniform = vec_is_uniform(factors);

    if (is_uniform) {
	/* ---- Uniform scale path ----------------------------------- */
	fastf_t factor = factors[X];
	if (factor <= 0.0) {
	    bu_vls_printf(gedp->ged_result_str,
		"scale: factor must be > 0 (got %g)\n", factor);
	    return BRLCAD_ERROR;
	}

	return _edit_xform_apply(gedp, ctx->dp, ctx->flag_i,
	    [&](struct rt_edit *s) -> int
	    {
		if (center_is_self) {
		    /* Own keypoint is already in s->e_keypoint; just pin it */
		    s->e_keyfixed = 1;
		} else if (have_center) {
		    VMOVE(s->e_keypoint, center_pos);
		    s->e_keyfixed = 1;
		}
		/* Set es_scale directly; bypass the e_para/acc_sc_sol
		 * accumulator so that each CLI scale call is relative to
		 * the current geometry state. */
		s->es_scale = factor;
		s->e_inpara  = 0;
		rt_edit_set_edflag(s, RT_PARAMS_EDIT_SCALE);
		rt_edit_process(s);
		return BRLCAD_OK;
	    });
    }

    /* ---- Per-axis (anisotropic) scale path ------------------------ *
     * Build a diagonal scale matrix and apply via ft_mat.             *
     * Note: not all primitive types support anisotropic scale; the    *
     * operation will fail gracefully for those that do not provide     *
     * ft_mat.                                                          */

    /* Validate all three factors */
    if (factors[X] <= 0.0 || factors[Y] <= 0.0 || factors[Z] <= 0.0) {
	bu_vls_printf(gedp->ged_result_str,
	    "scale: all per-axis factors must be > 0 (got %g %g %g)\n",
	    factors[X], factors[Y], factors[Z]);
	return BRLCAD_ERROR;
    }

    /* suppress unused-variable warning for vec_to_factor which is only
     * needed for the uniform branch */
    (void)vec_to_factor;

    return _edit_xform_apply(gedp, ctx->dp, ctx->flag_i,
	[&](struct rt_edit *s) -> int
	{
	    struct rt_db_internal *ip = &s->es_int;
	    if (!ip->idb_meth || !ip->idb_meth->ft_mat) {
		bu_vls_printf(gedp->ged_result_str,
		    "scale: this primitive type does not support "
		    "per-axis (anisotropic) scale\n");
		return BRLCAD_ERROR;
	    }

	    /* Resolve the scale center in primitive-local space */
	    point_t c_world = VINIT_ZERO;
	    if (center_is_self) {
		VMOVE(c_world, s->e_keypoint);
	    } else if (have_center) {
		VMOVE(c_world, center_pos);
	    } else {
		/* Use the primitive keypoint */
		VMOVE(c_world, s->e_keypoint);
	    }

	    /* Build a 4×4 per-axis diagonal scale matrix */
	    mat_t diag;
	    MAT_IDN(diag);
	    diag[0]  = factors[X];
	    diag[5]  = factors[Y];
	    diag[10] = factors[Z];

	    /* Wrap it about the center point:
	     *   T(-c) * diag * T(c)                                    */
	    mat_t scale_about_c;
	    bn_mat_xform_about_pnt(scale_about_c, diag, c_world);

	    /* Combine with e_mat / e_invmat like edit_sscale does */
	    mat_t mat1, mat;
	    bn_mat_mul(mat1, scale_about_c, s->e_mat);
	    bn_mat_mul(mat, s->e_invmat, mat1);

	    (*ip->idb_meth->ft_mat)(ip, mat, ip);
	    return BRLCAD_OK;
	});
}


/* ------------------------------------------------------------------ *
 * checkpoint
 * ------------------------------------------------------------------ */
class cmd_checkpoint : public ged_subcmd {
    public:
	std::string usage()   { return std::string("edit [options] [geometry] checkpoint"); }
	std::string purpose() { return std::string("save a restore-point for the current edit session"); }
	int exec(struct ged *, void *, int, const char **);
};
static cmd_checkpoint edit_checkpoint_cmd;

int
cmd_checkpoint::exec(struct ged *gedp, void *u_data, int argc, const char **argv)
{
    if (!gedp || !u_data || !argc || !argv)
	return BRLCAD_ERROR;

    struct ged_edit_ctx *ctx = (struct ged_edit_ctx *)u_data;
    if (ctx->dp == RT_DIR_NULL)
	return BRLCAD_ERROR;

    struct db_full_path dfp;
    db_full_path_init(&dfp);
    db_add_node_to_full_path(&dfp, ctx->dp);

    /* Use existing buffer entry or create a new one */
    struct rt_edit *s = ged_edit_buf_get(gedp, &dfp);
    bool is_new = (s == NULL);
    if (is_new) {
	struct bn_tol tol = BN_TOL_INIT_TOL;
	s = rt_edit_create(&dfp, gedp->dbip, &tol, NULL);
	if (!s) {
	    db_free_full_path(&dfp);
	    return BRLCAD_ERROR;
	}
	ged_edit_buf_set(gedp, &dfp, s);
    }

    int ret = rt_edit_checkpoint(s);
    db_free_full_path(&dfp);
    return ret;
}


/* ------------------------------------------------------------------ *
 * revert
 * ------------------------------------------------------------------ */
class cmd_revert : public ged_subcmd {
    public:
	std::string usage()   { return std::string("edit [options] [geometry] revert"); }
	std::string purpose() { return std::string("restore the edit session to the last checkpoint"); }
	int exec(struct ged *, void *, int, const char **);
};
static cmd_revert edit_revert_cmd;

int
cmd_revert::exec(struct ged *gedp, void *u_data, int argc, const char **argv)
{
    if (!gedp || !u_data || !argc || !argv)
	return BRLCAD_ERROR;

    struct ged_edit_ctx *ctx = (struct ged_edit_ctx *)u_data;
    if (ctx->dp == RT_DIR_NULL)
	return BRLCAD_ERROR;

    struct db_full_path dfp;
    db_full_path_init(&dfp);
    db_add_node_to_full_path(&dfp, ctx->dp);

    struct rt_edit *s = ged_edit_buf_get(gedp, &dfp);
    if (!s) {
	bu_vls_printf(gedp->ged_result_str,
	    "revert: no active edit session for '%s'\n", ctx->dp->d_namep);
	db_free_full_path(&dfp);
	return BRLCAD_ERROR;
    }

    int ret = rt_edit_revert(s);
    if (ret == BRLCAD_OK && !ctx->flag_i)
	ret = ged_edit_buf_promote(gedp, &dfp);

    db_free_full_path(&dfp);
    return ret;
}


/* ------------------------------------------------------------------ *
 * reset
 * ------------------------------------------------------------------ */
class cmd_reset : public ged_subcmd {
    public:
	std::string usage()   { return std::string("edit [options] [geometry] reset"); }
	std::string purpose() { return std::string("abandon in-buffer edit state and revert to on-disk geometry"); }
	int exec(struct ged *, void *, int, const char **);
};
static cmd_reset edit_reset_cmd;

int
cmd_reset::exec(struct ged *gedp, void *u_data, int argc, const char **argv)
{
    if (!gedp || !u_data || !argc || !argv)
	return BRLCAD_ERROR;

    struct ged_edit_ctx *ctx = (struct ged_edit_ctx *)u_data;
    if (ctx->dp == RT_DIR_NULL)
	return BRLCAD_ERROR;

    struct db_full_path dfp;
    db_full_path_init(&dfp);
    db_add_node_to_full_path(&dfp, ctx->dp);

    ged_edit_buf_abandon(gedp, &dfp);
    db_free_full_path(&dfp);
    return BRLCAD_OK;
}


/* ------------------------------------------------------------------ *
 * mat  — apply a raw 4×4 matrix to the primitive
 * ------------------------------------------------------------------ */
class cmd_mat : public ged_subcmd {
    public:
	std::string usage()   { return std::string("edit [options] [geometry] mat M00 M01 ... M33"); }
	std::string purpose() { return std::string("apply a 4x4 matrix (row-major, 16 values) to the primitive"); }
	int exec(struct ged *, void *, int, const char **);
};
static cmd_mat edit_mat_cmd;

int
cmd_mat::exec(struct ged *gedp, void *u_data, int argc, const char **argv)
{
    if (!gedp || !u_data || !argc || !argv)
	return BRLCAD_ERROR;

    struct ged_edit_ctx *ctx = (struct ged_edit_ctx *)u_data;
    if (ctx->dp == RT_DIR_NULL)
	return BRLCAD_ERROR;

    argc--; argv++;   /* skip "mat" */

    if (argc < 16) {
	bu_vls_printf(gedp->ged_result_str,
	    "mat: need 16 values (4x4 matrix, row-major order)\n");
	return BRLCAD_ERROR;
    }

    mat_t mat;
    for (int mi = 0; mi < 16; mi++) {
	if (bu_opt_fastf_t(NULL, 1, &argv[mi], &mat[mi]) < 0) {
	    bu_vls_printf(gedp->ged_result_str,
		"mat: bad matrix element [%d]: '%s'\n", mi, argv[mi]);
	    return BRLCAD_ERROR;
	}
    }

    return _edit_xform_apply(gedp, ctx->dp, ctx->flag_i,
	[&](struct rt_edit *s) -> int
	{
	    struct rt_db_internal *ip = &s->es_int;
	    if (!ip->idb_meth || !ip->idb_meth->ft_mat) {
		bu_vls_printf(gedp->ged_result_str,
		    "mat: primitive type does not support matrix application\n");
		return BRLCAD_ERROR;
	    }
	    (*ip->idb_meth->ft_mat)(ip, mat, ip);
	    return BRLCAD_OK;
	});
}


// Perturb command
class cmd_perturb : public ged_subcmd {
    public:
	std::string usage()   { return std::string("edit [options] [geometry] perturb factor"); }
	std::string purpose() { return std::string("perturb primitive or primitives below comb by the specified factor (must be greater than 0)"); }
	int exec(struct ged *, void *, int, const char **);
	struct ged_edit_ctx *ctx;
    private:
	int dp_perturb(struct directory *dp);
	fastf_t factor = 0;
};
static cmd_perturb edit_perturb_cmd;

int
cmd_perturb::dp_perturb(struct directory *dp)
{
    fastf_t lfactor = factor + factor*0.1*bn_rand_half(ctx->prand);
    bu_log("%s: %g\n", dp->d_namep, lfactor);
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    struct db_i *dbip = ctx->gedp->dbip;
    if (rt_db_get_internal(&intern, dp, dbip, NULL) < 0) {
	bu_log("rt_db_get_internal failed for %s\n", dp->d_namep);
	return BRLCAD_ERROR;
    }

    if (!intern.idb_meth || !intern.idb_meth->ft_perturb) {
	return BRLCAD_ERROR;
    }

    struct rt_db_internal *pintern;
    if (intern.idb_meth->ft_perturb(&pintern, &intern, 1, lfactor) != BRLCAD_OK) {
	bu_log("librt perturbation failed for %s\n", dp->d_namep);
	return BRLCAD_ERROR;
    }
    if (!pintern) {
	bu_log("librt perturbation failed for %s\n", dp->d_namep);
	return BRLCAD_ERROR;
    }

    std::string oname(dp->d_namep);
    db_delete(dbip, dp);
    db_dirdelete(dbip, dp);
    struct directory *ndp = db_diradd(dbip, oname.c_str(), RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&pintern->idb_type);
    if (ndp == RT_DIR_NULL) {
	bu_log("Cannot add %s to directory\n", oname.c_str());
	rt_db_free_internal(pintern);
	return BRLCAD_ERROR;
    }

    if (rt_db_put_internal(ndp, dbip, pintern) < 0) {
	bu_log("Failed to write %s to database\n", oname.c_str());
	rt_db_free_internal(pintern);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

int
cmd_perturb::exec(struct ged *gedp, void *u_data, int argc, const char **argv)
{
    if (!gedp || !u_data || !argc || !argv)
	return BRLCAD_ERROR;

    ctx = (struct ged_edit_ctx *)u_data;
    if (ctx->dp == RT_DIR_NULL)
	return BRLCAD_ERROR;

    argc--; argv++;

    if (argc < 1 || !argv) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", usage().c_str());
	return BRLCAD_ERROR;
    }

    if (bu_opt_fastf_t(NULL, 1, argv, (void *)&factor) != 1) {
	bu_vls_printf(gedp->ged_result_str, "%s\n", usage().c_str());
	return BRLCAD_ERROR;
    }
    if (NEAR_ZERO(factor, SMALL_FASTF))
	return BRLCAD_OK;

    struct bu_ptbl objs = BU_PTBL_INIT_ZERO;
    if (db_search(&objs, DB_SEARCH_RETURN_UNIQ_DP, "-type shape", 1, &ctx->dp, ctx->gedp->dbip, NULL, NULL, NULL) < 0) {
	bu_vls_printf(gedp->ged_result_str, "search error\n");
	return BRLCAD_ERROR;
    }
    if (!BU_PTBL_LEN(&objs)) {
	bu_vls_printf(gedp->ged_result_str, "no solids\n");
	return BRLCAD_OK;
    }

    int ret = BRLCAD_OK;
    for (size_t i = 0; i < BU_PTBL_LEN(&objs); i++) {
	struct directory *odp = (struct directory *)BU_PTBL_GET(&objs, i);
	int oret = dp_perturb(odp);
	if (oret != BRLCAD_OK)
	    ret = BRLCAD_ERROR;
    }
    bu_ptbl_free(&objs);

    DbiState *dbis = (DbiState *)ctx->gedp->dbi_state;
    if (dbis)
	dbis->update();

    return ret;
}


/* ================================================================== *
 * Main entry point — three-pass parser
 * ================================================================== */

extern "C" int
ged_edit_core(struct ged *gedp, int argc, const char *argv[])
{
    int help = 0;

    /* ---- Initialise context ---------------------------------------- */
    struct ged_edit_ctx ctx;
    ctx.gedp         = gedp;
    ctx.verbosity    = 0;
    ctx.prand        = NULL;
    ctx.flag_S       = 0;
    ctx.flag_f       = 0;
    ctx.flag_F       = 0;
    ctx.flag_i       = 0;
    ctx.from_selection = false;
    ctx.has_conflict = false;
    ctx.dp           = RT_DIR_NULL;
    bn_rand_init(ctx.prand, 0);

    bu_vls_trunc(gedp->ged_result_str, 0);

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    /* Skip past the "edit" command name */
    argc--; argv++;

    /* ---- Build subcommand map --------------------------------------- */
    std::map<std::string, ged_subcmd *> edit_cmds;
    edit_cmds["rot"]        = &edit_rotate_cmd;
    edit_cmds["rotate"]     = &edit_rotate_cmd;
    edit_cmds["tra"]        = &edit_tra_cmd;
    edit_cmds["translate"]  = &edit_translate_cmd;
    edit_cmds["sca"]        = &edit_scale_cmd;
    edit_cmds["scale"]      = &edit_scale_cmd;
    edit_cmds["perturb"]    = &edit_perturb_cmd;
    edit_cmds["checkpoint"] = &edit_checkpoint_cmd;
    edit_cmds["revert"]     = &edit_revert_cmd;
    edit_cmds["reset"]      = &edit_reset_cmd;
    edit_cmds["mat"]        = &edit_mat_cmd;

    /* ---- Global option descriptors --------------------------------- */
    struct bu_opt_desc d[8];
    BU_OPT(d[0], "h", "help",         "",  NULL, &help,        "Print help");
    BU_OPT(d[1], "v", "verbose",      "",  NULL, &ctx.verbosity,"Verbose output");
    BU_OPT(d[2], "S", "selection",    "",  NULL, &ctx.flag_S,  "Operate on selection (ignore cmd-line specifier)");
    BU_OPT(d[3], "f", "force",        "",  NULL, &ctx.flag_f,  "Force: apply op, write to disk, clear conflict");
    BU_OPT(d[4], "F", "abandon",      "",  NULL, &ctx.flag_F,  "Abandon: discard intermediate state, use on-disk");
    BU_OPT(d[5], "i", "intermediate", "",  NULL, &ctx.flag_i,  "Intermediate: apply to temp buffer only (no disk write)");
    BU_OPT_NULL(d[6]);

    const char *bargs_help = "[options] <geometry_specifier> subcommand [args]";

    if (!argc) {
	_ged_subcmd2_help(gedp, (struct bu_opt_desc *)d, edit_cmds, "edit", bargs_help, 0, NULL);
	return BRLCAD_OK;
    }

    /* Note whether the first token looks like an option.  If so we
     * can't reliably distinguish option errors from bad geometry specs. */
    bool maybe_opts = (argv[0][0] == '-');

    /* ---- Pass 1: find positions of first geometry spec and subcommand
     *              so we know how many leading tokens to feed to
     *              bu_opt_parse as global options.                      */
    DbiState *dbis = (DbiState *)gedp->dbi_state;

    int geom_pos = INT_MAX;
    int cmd_pos  = INT_MAX;
    std::vector<unsigned long long> gs;

    for (int i = 0; i < argc; i++) {
	/* Check if this token matches a known subcommand name */
	if (edit_cmds.find(std::string(argv[i])) != edit_cmds.end()) {
	    if (cmd_pos == INT_MAX)
		cmd_pos = i;
	    break;
	}

	/* Try to resolve as a geometry specifier */
	ged_edit_geom_spec spec;
	if (_resolve_geom_spec(spec, argv[i], gedp, dbis)) {
	    if (geom_pos == INT_MAX) {
		geom_pos = i;
		gs = spec.hashes;
	    }
	    break;
	}
    }

    /* With no geometry or command found yet — all remaining tokens are
     * candidates for options.  Parse them all: if -h is among them, print
     * help and return OK; otherwise report the first token as invalid. */
    if (geom_pos == INT_MAX && cmd_pos == INT_MAX) {
	if (maybe_opts) {
	    struct bu_vls opterrs = BU_VLS_INIT_ZERO;
	    bu_opt_parse(&opterrs, argc, argv, d);
	    bu_vls_free(&opterrs);
	    if (help) {
		_ged_subcmd2_help(gedp, (struct bu_opt_desc *)d, edit_cmds,
		    "edit", bargs_help, 0, NULL);
		return BRLCAD_OK;
	    }
	    _ged_subcmd2_help(gedp, (struct bu_opt_desc *)d, edit_cmds,
		"edit", bargs_help, 0, NULL);
	} else {
	    bu_vls_printf(gedp->ged_result_str,
		"Invalid geometry specifier: %s\n", argv[0]);
	}
	return BRLCAD_ERROR;
    }

    /* The option prefix is everything before the first geom or cmd token */
    int opt_prefix_len = (geom_pos < cmd_pos) ? geom_pos : cmd_pos;

    /* Parse global options from the prefix */
    if (opt_prefix_len > 0) {
	struct bu_vls opterrs = BU_VLS_INIT_ZERO;
	int opt_ret = bu_opt_parse(&opterrs, opt_prefix_len, argv, d);
	if (opt_ret < 0) {
	    bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_cstr(&opterrs));
	    _ged_subcmd2_help(gedp, (struct bu_opt_desc *)d, edit_cmds,
		"edit", bargs_help, 0, NULL);
	    bu_vls_free(&opterrs);
	    return BRLCAD_ERROR;
	}
	bu_vls_free(&opterrs);

	/* Shift remaining tokens to front */
	int remaining = argc - opt_prefix_len;
	for (int i = 0; i < remaining; i++)
	    argv[i] = argv[opt_prefix_len + i];
	argc -= opt_prefix_len;

	/* Adjust positions after shift */
	if (geom_pos != INT_MAX) geom_pos -= opt_prefix_len;
	if (cmd_pos  != INT_MAX) cmd_pos  -= opt_prefix_len;
    }

    /* Handle -h after option processing */
    if (help) {
	const char *cmd_name_for_help = (cmd_pos != INT_MAX) ? argv[cmd_pos] : "edit";
	_ged_subcmd2_help(gedp, (struct bu_opt_desc *)d, edit_cmds,
	    cmd_name_for_help, bargs_help, 0, NULL);
	return BRLCAD_OK;
    }

    /* Sanity: geometry must come before command if both present */
    if (geom_pos != INT_MAX && cmd_pos != INT_MAX &&
	    (geom_pos > cmd_pos || cmd_pos != geom_pos + 1)) {
	_ged_subcmd2_help(gedp, (struct bu_opt_desc *)d, edit_cmds,
	    "edit", bargs_help, 0, NULL);
	return BRLCAD_ERROR;
    }

    /* ---- Pass 2: collect geometry specifiers ----------------------- */

    /* Re-resolve the geometry specifier(s) into ged_edit_geom_spec entries.
     * The current implementation collects a single specifier (the one at
     * geom_pos).  Multi-object editing (dot batch, multiple paths) is
     * supported structurally but dispatched one-at-a-time. */
    const char *geom_str = NULL;
    if (geom_pos != INT_MAX) {
	geom_str = argv[geom_pos];
	ged_edit_geom_spec spec;
	if (_resolve_geom_spec(spec, geom_str, gedp, dbis)) {
	    ctx.geom_specs.push_back(spec);
	}
    }

    /* Selection fallback: if no explicit specifier was given, use the
     * active "default" selection state. */
    if (ctx.geom_specs.empty() && !ctx.flag_F && dbis) {
	std::vector<BSelectState *> ss = dbis->get_selected_states("default");
	if (!ss.empty()) {
	    std::vector<std::string> sel_paths = ss[0]->list_selected_paths();
	    for (const std::string &spath : sel_paths) {
		ged_edit_geom_spec spec;
		if (_resolve_geom_spec(spec, spath.c_str(), gedp, dbis))
		    ctx.geom_specs.push_back(spec);
	    }
	    if (!ctx.geom_specs.empty())
		ctx.from_selection = true;
	}
    }

    /* Selection conflict arbiter: explicit specifier present AND the same
     * object is also in the active selection → require an arbiter flag. */
    if (dbis && !ctx.geom_specs.empty() && !ctx.from_selection && !ctx.flag_S &&
	    !ctx.flag_f && !ctx.flag_F && !ctx.flag_i) {
	std::vector<BSelectState *> ss = dbis->get_selected_states("default");
	if (!ss.empty()) {
	    std::vector<std::string> sel_paths = ss[0]->list_selected_paths();
	    for (const auto &gspec : ctx.geom_specs) {
		for (const std::string &spath : sel_paths) {
		    if (gspec.path == spath) {
			ctx.has_conflict = true;
			bu_vls_printf(gedp->ged_result_str,
			    "Conflict: \"%s\" has both an explicit command-line "
			    "specifier and an active selection.\n"
			    "Use -S to operate on the selection, -f to force "
			    "a disk write, -F to abandon the intermediate "
			    "state, or -i to edit the temp buffer.\n",
			    gspec.path.c_str());
			return BRLCAD_ERROR;
		    }
		}
	    }
	}
    }

    /* If -S flag is set, discard explicit specifiers and use selection */
    if (ctx.flag_S && dbis) {
	ctx.geom_specs.clear();
	std::vector<BSelectState *> ss = dbis->get_selected_states("default");
	if (!ss.empty()) {
	    std::vector<std::string> sel_paths = ss[0]->list_selected_paths();
	    for (const std::string &spath : sel_paths) {
		ged_edit_geom_spec spec;
		if (_resolve_geom_spec(spec, spath.c_str(), gedp, dbis))
		    ctx.geom_specs.push_back(spec);
	    }
	    ctx.from_selection = true;
	}
    }

    /* Set convenience dp from first resolved specifier */
    if (!ctx.geom_specs.empty()) {
	ctx.dp = ctx.geom_specs[0].dp;
    }

    /* Advance past the geometry token */
    if (geom_str) {
	argc--; argv++;
    }

    /* ---- Pass 3: subcommand dispatch ------------------------------- */

    if (!argc || !argv[0]) {
	if (!ctx.geom_specs.empty() || ctx.from_selection) {
	    /* Object specified but no subcommand */
	    bu_vls_printf(gedp->ged_result_str,
		"No subcommand specified for \"%s\"\n",
		geom_str ? geom_str : "(selection)");
	} else {
	    _ged_subcmd2_help(gedp, (struct bu_opt_desc *)d, edit_cmds,
		"edit", bargs_help, 0, NULL);
	}
	return BRLCAD_ERROR;
    }

    std::string cmd_str(argv[0]);
    auto e_it = edit_cmds.find(cmd_str);
    if (e_it == edit_cmds.end()) {
	bu_vls_printf(gedp->ged_result_str,
	    "Unknown subcommand: %s\n", argv[0]);
	_ged_subcmd2_help(gedp, (struct bu_opt_desc *)d, edit_cmds,
	    "edit", bargs_help, 0, NULL);
	return BRLCAD_ERROR;
    }

    /* Must have a geometry specifier before dispatching */
    if (ctx.geom_specs.empty()) {
	bu_vls_printf(gedp->ged_result_str,
	    "No valid geometry specifier found; nothing to edit.\n");
	_ged_subcmd2_help(gedp, (struct bu_opt_desc *)d, edit_cmds,
	    "edit", bargs_help, 0, NULL);
	return BRLCAD_ERROR;
    }

    return e_it->second->exec(gedp, &ctx, argc, argv);
}



/* ------------------------------------------------------------------ *
 * Plugin entry point and command registration
 * (moved here from edit.c as part of Phase E retirement)
 * ------------------------------------------------------------------ */

#include "../include/plugin.h"
#include "./ged_edit.h"

#define GED_EDIT_COMMANDS(X, XID) \
    X(edit, ged_edit_core, GED_CMD_DEFAULT) \
    X(edarb, ged_edarb_core, GED_CMD_DEFAULT) \
    X(protate, ged_protate_core, GED_CMD_DEFAULT) \
    X(pscale, ged_pscale_core, GED_CMD_DEFAULT) \
    X(ptranslate, ged_ptranslate_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_EDIT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_edit", 1, GED_EDIT_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8


