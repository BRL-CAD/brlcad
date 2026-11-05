/*                      C O M B . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2026 United States Government as represented by
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
/** @file rt/comb.h
 *
 */

#ifndef RT_COMB_H
#define RT_COMB_H

#include "common.h"

#include "brep/boolean.h"

#include "vmath.h"
#include "bu/vls.h"
#include "rt/defines.h"

__BEGIN_DECLS

/* db_comb.c */


/**
 * Take an old v4 shader specification of the form
 *
 *   shadername arg1=value1 arg2=value2 color=1/2/3
 *
 * and convert it into the v5 {} list form
 *
 *   shadername {arg1 value1 arg2 value2 color 1/2/3}
 *
 * Note -- the input string is smashed with nulls.
 *
 * Note -- the v5 version is used everywhere internally, and in v5
 * databases.
 *
 * @return 1 error
 * @return 0 OK
 */
RT_EXPORT extern int rt_shader_to_list(const char *in, struct bu_vls *vls);

RT_EXPORT extern int rt_shader_to_key_eq(const char *in, struct bu_vls *vls);


/**
 * Import a combination record from a V4 database into internal form.
 */
RT_EXPORT extern int rt_comb_import4(struct rt_db_internal      *ip,
				     const struct bu_external   *ep,
				     const mat_t                matrix,         /* NULL if identity */
				     const struct db_i          *dbip);

RT_EXPORT extern int rt_comb_export4(struct bu_external                 *ep,
				     const struct rt_db_internal        *ip,
				     double                             local2mm,
				     const struct db_i                  *dbip);


RT_EXPORT extern void db_comb_describe(struct bu_vls    *str,
				       const struct rt_comb_internal    *comb,
				       int              verbose,
				       double           mm2local);

/**
 * OBJ[ID_COMBINATION].ft_describe() method
 */
RT_EXPORT extern int rt_comb_describe(struct bu_vls     *str,
				      const struct rt_db_internal *ip,
				      int               verbose,
				      double            mm2local);

/**
 * fills in rgb with the color for a given comb combination
 *
 * returns truthfully if a color could be got.  note that this routine
 * will not (and cannot) handle the color inherit/override flag as
 * that is set on some higher-level parent combination.
 *
 */
RT_EXPORT extern int rt_comb_get_color(struct db_i *dbip, unsigned char rgb[3], const struct rt_comb_internal *comb);


/**
 * change all matching object names in the comb tree from old_name to
 * new_name
 *
 * calling function must supply an initialized bu_ptbl, and free it
 * once done.
 */
RT_EXPORT extern int db_comb_mvall(struct directory *dp,
				   struct db_i *dbip,
				   const char *old_name,
				   const char *new_name,
				   struct bu_ptbl *stack);

/* db5_comb.c */

/**
 * Read a combination object in v5 external (on-disk) format, and
 * convert it into the internal format described in rtgeom.h
 *
 * This is an unusual conversion, because some of the data is taken
 * from attributes, not just from the object body.  By the time this
 * is called, the attributes will already have been cracked into
 * ip->idb_avs, we get the attributes from there.
 *
 * Returns -
 * 0 OK
 * -1 FAIL
 */
RT_EXPORT extern int rt_comb_import5(struct rt_db_internal *ip, const struct bu_external *ep, const mat_t mat, const struct db_i *dbip);

/**
 * Return a RT_DIR_NULL terminated array of directory pointers that
 * holds the set of immediate children associated with comb.  The
 * caller is responsible for freeing the array, but not the directory
 * structures pointed to by the array.
 *
 * Optionally, pointers may also be supplied to collect arrays holding
 * the boolean operations and matrices associated with the comb entries.
 * For boolean operations, the caller is responsible for freeing the
 * array.  For matrices, both the array and the matrices themselves
 * must be freed by the caller.  The boolean operations array is zero
 * terminated, the matrix array is NULL terminated.  For example:
 *
 * @code
 * int i = 0;
 * struct directory *wdp;
 * struct directory **children = NULL;
 * int *bool_ops = NULL;
 * matp_t *matrices = NULL;
 * matp_t m;
 * db_comb_children(dbip, comb, &children, &bool_ops, &matrices);
 * if (db_comb_children(dbip, comb, &bool_ops, &matrices) > 0) {
 *    i = 0;
 *    wdp = children[0];
 *    while (wdp != RT_DIR_NULL) {
 *       char obuf[1024];
 *       bu_log("%s child %d: %d %s\n", dp->d_namep, i, bool_ops[ind], wdp->d_namep);
 *       if (mats[ind]){
 *          bn_mat_print_guts("", mats[i], obuf, 1024);
 *          bu_log("%s %s\n", wdp->d_namep, obuf);
 *       }
 *       i++;
 *       wdp = children[i];
 *    }
 * }
 * i = 0;
 * while (mats[i]) {
 *    bu_free(mats[i], "free matrix");
 *    i++;
 * }
 * bu_free(mats, "free mats array");
 * bu_free(bool_ops, "free ops");
 * bu_free(children, "free children struct directory ptr array");
 * @endcode
 */
RT_EXPORT extern int db_comb_children(struct db_i *dbip, struct rt_comb_internal *comb, struct directory ***children, int **bool_ops, matp_t **mats);

/** Describes whether a B-Rep conversion preserves exact source geometry. */
enum rt_brep_conversion_fidelity {
    RT_BREP_CONVERSION_FIDELITY_UNKNOWN = 0,
    RT_BREP_CONVERSION_FIDELITY_EXACT,
    RT_BREP_CONVERSION_FIDELITY_APPROXIMATE
};

#define RT_BREP_APPROXIMATION_PRIMITIVE      0x01u
#define RT_BREP_APPROXIMATION_TRANSFORM      0x02u
#define RT_BREP_APPROXIMATION_REGULARIZATION 0x04u
#define RT_BREP_APPROXIMATION_ARB_REPAIR     0x08u

/** Stable stage identifiers for the first failed CSG-to-B-Rep tree node. */
enum rt_brep_failure_stage {
    RT_BREP_FAILURE_STAGE_NONE = 0,
    RT_BREP_FAILURE_STAGE_TREE_STRUCTURE = 1,
    RT_BREP_FAILURE_STAGE_LEAF_LOOKUP = 2,
    RT_BREP_FAILURE_STAGE_LEAF_IMPORT = 3,
    RT_BREP_FAILURE_STAGE_LEAF_CONVERSION = 4,
    RT_BREP_FAILURE_STAGE_LEAF_TRANSFORM = 5,
    RT_BREP_FAILURE_STAGE_BOOLEAN_EVALUATION = 6,
    RT_BREP_FAILURE_STAGE_OUTPUT_VALIDATION = 7
};

/** Stable reason identifiers used with rt_brep_failure_stage. */
enum rt_brep_failure_reason {
    RT_BREP_FAILURE_REASON_NONE = 0,
    RT_BREP_FAILURE_REASON_INVALID_TREE = 1,
    RT_BREP_FAILURE_REASON_MISSING_OBJECT = 2,
    RT_BREP_FAILURE_REASON_INVALID_OBJECT = 3,
    RT_BREP_FAILURE_REASON_UNSUPPORTED_OBJECT = 4,
    RT_BREP_FAILURE_REASON_INVALID_TRANSFORM = 5,
    RT_BREP_FAILURE_REASON_BOOLEAN_INDETERMINATE = 6,
    RT_BREP_FAILURE_REASON_WORK_BUDGET = 7,
    RT_BREP_FAILURE_REASON_OUTPUT_INVALID = 8,
    RT_BREP_FAILURE_REASON_OUTPUT_NOT_SOLID = 9,
    RT_BREP_FAILURE_REASON_INTERNAL_ERROR = 10
};

#define RT_BREP_FAILURE_TREE_PATH_MAX 128
#define RT_BREP_FAILURE_OBJECT_NAME_MAX 256
#define RT_BREP_FAILURE_SNAPSHOT_NAME_MAX 128

/**
 * Machine-readable description of the first failed conversion-tree node.
 *
 * A Boolean operation is one of enum op_type, or -1 when the failure did not
 * occur at a Boolean node.  Topology counts describe the operands supplied to
 * the failing operation and the output before it was discarded.  The
 * regularization scale is the requested multiple of modeling distance
 * tolerance, not a measured displacement.  A snapshot name is present only
 * when opt-in failure capture successfully wrote a standalone replay
 * database.
 */
struct rt_brep_failure_info {
    enum rt_brep_failure_stage stage;
    enum rt_brep_failure_reason reason;
    int status;
    int operation;
    int regularized;
    double regularization_scale;
    size_t left_faces;
    size_t left_edges;
    size_t right_faces;
    size_t right_edges;
    size_t output_faces;
    size_t output_edges;
    size_t output_naked_edges;
    /** Detailed evidence reported by the failed ON_Boolean call. */
    struct brep_boolean_failure_info boolean_failure;
    char tree_path[RT_BREP_FAILURE_TREE_PATH_MAX];
    char object_name[RT_BREP_FAILURE_OBJECT_NAME_MAX];
    char snapshot_name[RT_BREP_FAILURE_SNAPSHOT_NAME_MAX];
};

#define RT_BREP_FAILURE_INFO_INIT { \
    RT_BREP_FAILURE_STAGE_NONE, RT_BREP_FAILURE_REASON_NONE, 0, -1, 0, \
    0.0, 0, 0, 0, 0, 0, 0, 0, BREP_BOOLEAN_FAILURE_INFO_INIT, \
    {0}, {0}, {0} \
}

RT_EXPORT extern const char *
rt_brep_failure_stage_description(enum rt_brep_failure_stage stage);

RT_EXPORT extern const char *
rt_brep_failure_reason_description(enum rt_brep_failure_reason reason);

/**
 * Additional properties of a B-Rep conversion attempt.
 *
 * For an approximate conversion, @c declared_relative_tolerance is the
 * largest relative approximation scale declared by a primitive converter or
 * by a tolerance-bounded Boolean regularization retry.  It is suitable for
 * selecting a validation tolerance, but is not a certified Hausdorff-error
 * bound.
 */
struct rt_brep_conversion_info {
    enum rt_brep_conversion_fidelity fidelity;
    double declared_relative_tolerance;
    unsigned int approximation_flags;
    unsigned int regularization_attempts;
    unsigned int whole_tree_regularization_attempts;
    unsigned int repaired_arb_count;
    /** Status returned by the initial, non-whole-tree-retry evaluation. */
    int initial_status;
    /** First failed node from the exact evaluation, if any. */
    struct rt_brep_failure_info failure;
};

#define RT_BREP_CONVERSION_INFO_INIT { \
    RT_BREP_CONVERSION_FIDELITY_UNKNOWN, 0.0, 0, 0, 0, 0, -6, \
    RT_BREP_FAILURE_INFO_INIT \
}

#ifdef __cplusplus
/**
 * Convert a combination tree to a B-Rep and report a boolean_result status.
 * On any negative status, a non-NULL @p b is set to NULL.  The caller remains
 * responsible for any separately retained owner of the original output.
 */
RT_EXPORT extern int
rt_comb_brep_status(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol, const struct db_i *dbip);

/**
 * Convert a combination tree and also report whether its primitive
 * conversions are exact or tolerance-controlled approximations.  @p info may
 * be NULL.  Failed conversions report unknown fidelity.
 */
RT_EXPORT extern int
rt_comb_brep_status_info(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol, const struct db_i *dbip, struct rt_brep_conversion_info *info);

RT_EXPORT extern void
rt_comb_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol, const struct db_i *dbip);

/** Opaque pool of persistent, crash-isolated B-Rep evaluation workers. */
struct rt_brep_worker_pool;

/** How a worker request completed independently of its Boolean status. */
enum rt_brep_worker_transport_status {
    RT_BREP_WORKER_TRANSPORT_OK = 0,
    RT_BREP_WORKER_START_FAILED,
    RT_BREP_WORKER_WRITE_FAILED,
    RT_BREP_WORKER_CRASHED,
    RT_BREP_WORKER_TIMED_OUT,
    RT_BREP_WORKER_PROTOCOL_ERROR,
    RT_BREP_WORKER_RESULT_ERROR,
    RT_BREP_WORKER_CANCELLED
};

/**
 * Limits for a persistent B-Rep worker pool.  Zero values select defaults.
 * Each worker evaluates one request at a time.  Parallelism is obtained by
 * using multiple processes, never concurrent OpenNURBS Boolean operations in
 * one process.
 */
struct rt_brep_worker_options {
    size_t worker_count;
    unsigned int request_timeout_ms;
    size_t max_jobs_per_worker;
    size_t max_worker_resident_bytes;
    /** Optional test/development override; normally resolved automatically. */
    const char *worker_executable;
};

#define RT_BREP_WORKER_OPTIONS_INIT { 1, 300000, 32, ((size_t)2 << 30), NULL }

/** Result returned by rt_brep_worker_pool_poll(). */
struct rt_brep_worker_result {
    uint64_t request_id;
    int boolean_status;
    enum rt_brep_worker_transport_status transport_status;
    struct rt_brep_conversion_info conversion_info;
    ON_Brep *brep;
    char *diagnostic;
    int worker_pid;
    size_t worker_job_count;
    size_t worker_resident_bytes;
};

#define RT_BREP_WORKER_RESULT_INIT { \
    0, BREP_BOOLEAN_INTERNAL_ERROR, RT_BREP_WORKER_TRANSPORT_OK, \
    RT_BREP_CONVERSION_INFO_INIT, NULL, NULL, 0, 0, 0 \
}

/**
 * Create a pool bound to an immutable, file-backed geometry database.
 * Workers open the database read-only and are restarted after crashes,
 * timeouts, protocol failures, job-count limits, or resident-memory limits.
 */
RT_EXPORT extern struct rt_brep_worker_pool *
rt_brep_worker_pool_create(const char *database_path,
    const struct rt_brep_worker_options *options);

/** Queue conversion and evaluation of one database object.  Returns zero on
 * rejection, otherwise a request identifier. */
RT_EXPORT extern uint64_t
rt_brep_worker_pool_submit_conversion(struct rt_brep_worker_pool *pool,
    const char *object_name, const struct bn_tol *tol);

/** Queue a Boolean between two B-Rep database objects. */
RT_EXPORT extern uint64_t
rt_brep_worker_pool_submit_boolean(struct rt_brep_worker_pool *pool,
    const char *left_name, const char *right_name, enum op_type operation);

/**
 * Advance the pool and return one completed request.  A return value of one
 * supplies @p result, zero means no request completed before @p wait_ms, and
 * minus one reports invalid arguments or an unrecoverable pool error.
 */
RT_EXPORT extern int
rt_brep_worker_pool_poll(struct rt_brep_worker_pool *pool,
    struct rt_brep_worker_result *result, unsigned int wait_ms);

/** Number of queued or active requests which have not yet been collected. */
RT_EXPORT extern size_t
rt_brep_worker_pool_pending(const struct rt_brep_worker_pool *pool);

/** Release fields owned by a collected result, including its B-Rep. */
RT_EXPORT extern void
rt_brep_worker_result_destroy(struct rt_brep_worker_result *result);

/** Cancel outstanding work, terminate the workers, and destroy the pool. */
RT_EXPORT extern void
rt_brep_worker_pool_destroy(struct rt_brep_worker_pool *pool);
#endif

__END_DECLS

#endif /* RT_COMB_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
