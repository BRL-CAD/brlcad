/*                   G E D _ E D I T . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2024 United States Government as represented by
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
GED_EXPORT extern int _ged_rotate_eto(struct ged *gedp,
			   struct rt_eto_internal *eto,
			   const char *attribute,
			   matp_t rmat);

/* defined in rotate_extrude.c */
GED_EXPORT extern int _ged_rotate_extrude(struct ged *gedp,
			       struct rt_extrude_internal *extrude,
			       const char *attribute,
			       matp_t rmat);

/* defined in rotate_hyp.c */
GED_EXPORT extern int _ged_rotate_hyp(struct ged *gedp,
			   struct rt_hyp_internal *hyp,
			   const char *attribute,
			   matp_t rmat);

/* defined in rotate_tgc.c */
GED_EXPORT extern int _ged_rotate_tgc(struct ged *gedp,
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

/* defined in scale_part.c */
GED_EXPORT extern int _ged_scale_part(struct ged *gedp,
                           struct rt_part_internal *part,
                           const char *attribute,
                           fastf_t sf,
                           int rflag);

/* defined in edpipe.c */
GED_EXPORT extern int _ged_scale_pipe(struct ged *gedp,
                           struct rt_pipe_internal *pipe_internal,
                           const char *attribute,
                           fastf_t sf,
                           int rflag);

/* defined in scale_rhc.c */
GED_EXPORT extern int _ged_scale_rhc(struct ged *gedp,
                          struct rt_rhc_internal *rhc,
                          const char *attribute,
                          fastf_t sf,
                          int rflag);

/* defined in scale_rpc.c */
GED_EXPORT extern int _ged_scale_rpc(struct ged *gedp,
                          struct rt_rpc_internal *rpc,
                          const char *attribute,
                          fastf_t sf,
                          int rflag);

/* defined in scale_superell.c */
GED_EXPORT extern int _ged_scale_superell(struct ged *gedp,
                               struct rt_superell_internal *superell,
                               const char *attribute,
                               fastf_t sf,
                               int rflag);

/* defined in scale_tgc.c */
GED_EXPORT extern int _ged_scale_tgc(struct ged *gedp,
                          struct rt_tgc_internal *tgc,
                          const char *attribute,
                          fastf_t sf,
                          int rflag);

/* defined in scale_tor.c */
GED_EXPORT extern int _ged_scale_tor(struct ged *gedp,
                          struct rt_tor_internal *tor,
                          const char *attribute,
                          fastf_t sf,
                          int rflag);


/* defined in translate_extrude.c */
GED_EXPORT extern int _ged_translate_extrude(struct ged *gedp,
				  struct rt_extrude_internal *extrude,
				  const char *attribute,
				  vect_t tvec,
				  int rflag);

/* defined in translate_tgc.c */
GED_EXPORT extern int _ged_translate_tgc(struct ged *gedp,
			      struct rt_tgc_internal *tgc,
			      const char *attribute,
			      vect_t tvec,
			      int rflag);

/* defined in facedef.c */
GED_EXPORT extern int edarb_facedef(void *data, int argc, const char *argv[]);


extern int ged_edarb_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_protate_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_pscale_core(struct ged *gedp, int argc, const char *argv[]);
extern int ged_ptranslate_core(struct ged *gedp, int argc, const char *argv[]);

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
