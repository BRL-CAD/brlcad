/*                   G E D _ E D I T . H
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
/** @file ged_edit.h
 *
 * Private header for libged edit cmd and related commands.
 *
 */

#ifndef LIBGED_EDIT_GED_PRIVATE_H
#define LIBGED_EDIT_GED_PRIVATE_H

#include "common.h"

#ifdef __cplusplus
#include <set>
#include <string>
#include <vector>
#endif
#include <time.h>

#include "ged.h"
#include "../ged_private.h"

__BEGIN_DECLS

/* defined in rotate_eto.c */
extern int _ged_rotate_eto(struct ged *gedp,
			   struct rt_eto_internal *eto,
			   const char *attribute,
			   matp_t rmat);

/* defined in rotate_extrude.c */
extern int _ged_rotate_extrude(struct ged *gedp,
			       struct rt_extrude_internal *extrude,
			       const char *attribute,
			       matp_t rmat);

/* defined in rotate_hyp.c */
extern int _ged_rotate_hyp(struct ged *gedp,
			   struct rt_hyp_internal *hyp,
			   const char *attribute,
			   matp_t rmat);

/* defined in rotate_tgc.c */
extern int _ged_rotate_tgc(struct ged *gedp,
			   struct rt_tgc_internal *tgc,
			   const char *attribute,
			   matp_t rmat);

/* defined in scale_ehy.c */
extern int _ged_scale_ehy(struct ged *gedp,
			  struct rt_ehy_internal *ehy,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

/* defined in scale_ell.c */
extern int _ged_scale_ell(struct ged *gedp,
			  struct rt_ell_internal *ell,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

/* defined in scale_epa.c */
extern int _ged_scale_epa(struct ged *gedp,
			  struct rt_epa_internal *epa,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

/* defined in scale_eto.c */
extern int _ged_scale_eto(struct ged *gedp,
			  struct rt_eto_internal *eto,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

/* defined in scale_extrude.c */
extern int _ged_scale_extrude(struct ged *gedp,
			      struct rt_extrude_internal *extrude,
			      const char *attribute,
			      fastf_t sf,
			      int rflag);

/* defined in scale_hyp.c */
extern int _ged_scale_hyp(struct ged *gedp,
			  struct rt_hyp_internal *hyp,
			  const char *attribute,
			  fastf_t sf,
			  int rflag);

/* defined in edit_metaball.c */
extern int _ged_scale_metaball(struct ged *gedp,
                              struct rt_metaball_internal *mbip,
                              const char *attribute,
                              fastf_t sf,
                              int rflag);

/* defined in scale_part.c */
extern int _ged_scale_part(struct ged *gedp,
                           struct rt_part_internal *part,
                           const char *attribute,
                           fastf_t sf,
                           int rflag);

/* defined in edpipe.c */
extern int _ged_scale_pipe(struct ged *gedp,
                           struct rt_pipe_internal *pipe_internal,
                           const char *attribute,
                           fastf_t sf,
                           int rflag);

/* defined in scale_rhc.c */
extern int _ged_scale_rhc(struct ged *gedp,
                          struct rt_rhc_internal *rhc,
                          const char *attribute,
                          fastf_t sf,
                          int rflag);

/* defined in scale_rpc.c */
extern int _ged_scale_rpc(struct ged *gedp,
                          struct rt_rpc_internal *rpc,
                          const char *attribute,
                          fastf_t sf,
                          int rflag);

/* defined in scale_superell.c */
extern int _ged_scale_superell(struct ged *gedp,
                               struct rt_superell_internal *superell,
                               const char *attribute,
                               fastf_t sf,
                               int rflag);

/* defined in scale_tgc.c */
extern int _ged_scale_tgc(struct ged *gedp,
                          struct rt_tgc_internal *tgc,
                          const char *attribute,
                          fastf_t sf,
                          int rflag);

/* defined in scale_tor.c */
extern int _ged_scale_tor(struct ged *gedp,
                          struct rt_tor_internal *tor,
                          const char *attribute,
                          fastf_t sf,
                          int rflag);


/* defined in translate_extrude.c */
extern int _ged_translate_extrude(struct ged *gedp,
				  struct rt_extrude_internal *extrude,
				  const char *attribute,
				  vect_t tvec,
				  int rflag);

/* defined in translate_tgc.c */
extern int _ged_translate_tgc(struct ged *gedp,
			      struct rt_tgc_internal *tgc,
			      const char *attribute,
			      vect_t tvec,
			      int rflag);

/* defined in facedef.c */
extern int edarb_facedef(void *data, int argc, const char *argv[]);


extern int ged_edarb_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_protate_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_pscale_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_ptranslate_core(struct ged *gedp, int argc, const char *argv[]);


#define HELPFLAG "--print-help"
#define PURPOSEFLAG "--print-purpose"


#ifdef __cplusplus

/**
 * One resolved geometry specifier from the edit command line.
 *
 * Each token that resolves to a geometry object (or the batch marker ".") is
 * parsed into this structure during Pass 2 of the three-pass parser.
 */
struct ged_edit_geom_spec {
    std::string raw;       /**< @brief original argv token */
    std::string path;      /**< @brief resolved path string (no URI components) */
    std::string fragment;  /**< @brief URI fragment, e.g. "V1" from "obj.s#V1" */
    std::string query;     /**< @brief URI query, e.g. "V*" from "obj.s?V*"   */
    bool is_batch;         /**< @brief true when token was "." (each-object)   */
    std::vector<unsigned long long> hashes; /**< @brief from DbiState::digest_path */
    struct directory *dp;  /**< @brief head dp; RT_DIR_NULL for comb instances */
};


/**
 * Top-level parsing context for the edit command.
 *
 * Created once per ged_edit_core() invocation and passed to subcommands.
 *
 * Pass 1 (global opts) fills the flag_* fields.
 * Pass 2 (geometry specifiers) fills geom_specs and from_selection.
 * Pass 3 (subcommand dispatch) uses geom_specs and the subcommand args.
 */
struct ged_edit_ctx {
    struct ged *gedp;
    int verbosity;
    float *prand;

    /* Conflict arbiter flags (set by Pass 1 global options) */
    int flag_S;  /**< @brief -S: operate on selection, ignoring cmd-line specifier */
    int flag_f;  /**< @brief -f: force: apply op, write to disk, clear conflict     */
    int flag_F;  /**< @brief -F: abandon: discard intermediate state, use on-disk   */
    int flag_i;  /**< @brief -i: intermediate: apply to temp buf only (no disk write)*/

    /* Resolved geometry specifiers (populated by Pass 2) */
    std::vector<ged_edit_geom_spec> geom_specs;
    bool from_selection; /**< @brief true if geom_specs came from selection fallback */

    /* Conflict state: set when an explicit specifier conflicts with the
     * active selection and no arbiter flag was given. */
    bool has_conflict;

    /* Convenience: dp for the primary (first) object.
     * RT_DIR_NULL when the primary spec is a multi-segment comb path. */
    struct directory *dp;
};

#endif


__END_DECLS

#endif /* LIBGED_EDIT_GED_PRIVATE_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
