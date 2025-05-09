/*                            N I R T . H
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
/** @addtogroup libanalyze
 *
 *  A library implementation of functionality originally developed in
 *  Natalie's Interactive Ray Tracer (NIRT)
 *
 */
/** @{ */
/** @file analyze/nirt.h */

#ifndef ANALYZE_NIRT_H
#define ANALYZE_NIRT_H

#include "common.h"
#include "analyze/defines.h"

#include "bu/opt.h"
#include "bu/vls.h"
#include "raytrace.h"

__BEGIN_DECLS

#define NIRT_SILENT_UNSET    0
#define NIRT_SILENT_YES      1
#define NIRT_SILENT_NO       -1

/** Allowed VALUES FOR overlap_claims in nirt_opt_vals */
#define NIRT_OVLP_RESOLVE            0
#define NIRT_OVLP_REBUILD_FASTGEN    1
#define NIRT_OVLP_REBUILD_ALL        2
#define NIRT_OVLP_RETAIN             3

/** Container to hold results of nirt option parsing */
struct nirt_opt_vals {
    int current_center;
    int file_cnt;
    int header_mode;
    int overlap_claims;
    int print_help;
    int read_matrix;
    int show_formats;
    int silent_mode;
    int use_air;
    int verbose_mode;
    struct bu_ptbl attrs;
    int fmt_set;
    int script_set;
    struct bu_ptbl init_scripts;
    struct bu_vls filename;
    struct bu_vls nirt_debug;
    vect_t center_model;
    /* File and colors to use when generating plotting output */
    struct bu_vls plotfile;
    struct bu_color color_odd;
    struct bu_color color_even;
    struct bu_color color_gap;
    struct bu_color color_ovlp;

};

#define NIRT_OPT_INIT {0, 0, 1, NIRT_OVLP_RESOLVE, 0, 0, 0, NIRT_SILENT_UNSET, 0, 0, BU_PTBL_INIT_ZERO, 0, 0, BU_PTBL_INIT_ZERO, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO, VINIT_ZERO, BU_VLS_INIT_ZERO, BU_COLOR_CYAN, BU_COLOR_YELLOW, BU_COLOR_PURPLE, BU_COLOR_WHITE}

/**
 * Given a nirt_opt_vals container, set up and return a bu_opt_desc that can
 * parse options from an argv array.  Caller is responsible for freeing the
 * resulting bu_opt_desc array. */
ANALYZE_EXPORT struct bu_opt_desc *nirt_opt_desc(struct nirt_opt_vals *o);

/**
 * Reset a nirt_opt_vals structure to its initial state. Structure is still
 * usable after this operation. */
ANALYZE_EXPORT void nirt_opt_vals_reset(struct nirt_opt_vals *o);

/**
 * Free all contents of a nirt_opt_vals structure.  Structure is unusable
 * after this operation. */
ANALYZE_EXPORT void nirt_opt_vals_free(struct nirt_opt_vals *o);

/**
 * Generate argv style options that attempts to generate an argv that will make
 * the option values in tgt look like those in src.  argv strings will be
 * inserted into tbl - caller is responsible for freeing them.
 *
 * There are a few caveats - options like silent or verbose that have only an
 * enabling flag will be turned on if they are off in tgt, but if they are off
 * in src and on in tgt there is no command line option to disable them once
 * set.
 *
 * Operations on init_scripts, performed if add_scripts is non-zero, are
 * additive ONLY - because it is legitimate to have multiple scripts and
 * settings with the same values, it can only add what is present in src to
 * what is already in tgt.
 */
ANALYZE_EXPORT void nirt_opt_mk_args(struct bu_ptbl *tbl, struct nirt_opt_vals *tgt, struct nirt_opt_vals *src, int add_scripts);


/** Opaque container to hold NIRT's state */
struct nirt_state_impl;
struct nirt_state {
    struct nirt_state_impl *i;
    struct bu_vls nirt_cmd;
    struct bu_vls nirt_format_file;
    struct bu_vls plotfile;
    struct bu_color color_odd;
    struct bu_color color_even;
    struct bu_color color_gap;
    struct bu_color color_ovlp;
};

/**
 * Perform non-database dependent initialization.  A newly initialized state
 * can accept some commands (like updates to the attribute list) but will not
 * be able to raytrace. To set up a raytracing environment, apply
 * nirt_init_dbip */
ANALYZE_EXPORT int nirt_init(struct nirt_state *ns);

/**
 * Initialize a struct nirt_state state for a particular database.  After this
 * step a nirt instance is ready to raytrace. */
ANALYZE_EXPORT int nirt_init_dbip(struct nirt_state *ns, struct db_i *dbip);

/**
 * Clear those aspects of a struct nirt_state state specific to a database
 * instance. */
ANALYZE_EXPORT int nirt_clear_dbip(struct nirt_state *ns);

/**
 * Clean up and free the internals of a NIRT state. */
ANALYZE_EXPORT void nirt_destroy(struct nirt_state *ns);

/**
 * Execute nirt commands.  Runs either the supplied script.
 *
 * Returns -1 if there was any sort of error, 0 if the script executed
 * successfully without a quit call, and 1 if a quit command was encountered
 * during execution. See the man(1) nirt manual page for documentation of
 * valid script commands and options.
 */
ANALYZE_EXPORT int nirt_exec(struct nirt_state *ns, const char *script);

/* Flags for clearing/resetting/reporting the struct nirt_state state */
#define NIRT_ALL      0x1    /**< @brief reset to initial state or report all state */
#define NIRT_OUT      0x2    /**< @brief output log*/
#define NIRT_MSG      0x4    /**< @brief output log*/
#define NIRT_ERR      0x8    /**< @brief error log */
#define NIRT_SEGS     0x10   /**< @brief segment list */
#define NIRT_OBJS     0x20   /**< @brief 'active' objects from the scene */
#define NIRT_FRMTS    0x40   /**< @brief available pre-defined output formats */
#define NIRT_VIEW     0x80   /**< @brief the current view (ae/dir/center/etc.) */

/**
 * Associate a pointer to user data with the struct nirt_state state, unless
 * u_data is NULL. Returns the current u_data pointer - so to extract the
 * current struct nirt_state data pointer value, supply a NULL argument to
 * u_data. If u_data is non-NULL, the current data pointer will be overwritten
 * - the caller should save the old pointer if they need it before setting the
 * new one. */
ANALYZE_EXPORT void *nirt_udata(struct nirt_state *ns, void *u_data);

/**
 * Mechanism for setting callback hooks executed when the specified state is
 * changed after a nirt_exec call.  struct nirt_state_ALL will be executed
 * last, and is run if set and if any of the other states change. Hook
 * functions will be passed the current value of the u_data pointer. */
typedef int (*nirt_hook_t)(struct nirt_state *ns, void *u_data);
ANALYZE_EXPORT void nirt_hook(struct nirt_state *ns, nirt_hook_t hf, int flag);

/**
 * Reset some or all of the struct nirt_state state, depending on the supplied
 * flags. If other flags are provided with struct nirt_state_ALL, struct
 * nirt_state_ALL will skip the clearing step(s) specified by the other
 * flag(s).  So, for example, if a caller wishes to reset the struct nirt_state
 * state but retain the existing scripts for reuse they could call with
 * nirt_clear with struct nirt_state_ALL|struct nirt_state_SCRIPTS.  Note that
 * the struct nirt_state_FRMTS, struct nirt_state_OUT and struct nirt_state_ERR
 * flags are no-ops for nirt_clear. */
ANALYZE_EXPORT void nirt_clear(struct nirt_state *ns, int flags);

/**
 * Report command output.  For SEGS, SCRIPTS, OBJS and FRMTS reports a textual
 * list of the output.  Unlike clear, which takes the type as combinable flags,
 * nirt_log expects only one type. Returns -1 if output can't be printed for
 * any reason (NULL input or unknown output_type) and 0 otherwise. */
ANALYZE_EXPORT void nirt_log(struct bu_vls *o, struct nirt_state *ns, int output_type);

/**
 * Reports available commands and their options. Returns -1 if help can't be
 * printed for any reason (NULL input or unknown output type) and 0 otherwise.
 */
ANALYZE_EXPORT int nirt_help(struct bu_vls *h, struct nirt_state *ns, bu_opt_format_t ofmt);

/**
 * Return any line segments generated by processed commands in segs.  Returns
 * number of line segments in segs, or -1 if there was an error. */
ANALYZE_EXPORT int nirt_line_segments(struct bv_vlblock **segs, struct nirt_state *ns);


__END_DECLS

#endif /* ANALYZE_NIRT_H */

/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
