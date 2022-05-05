/*                      R T _ F U N C T A B . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2022 United States Government as represented by
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
/** @addtogroup rt_functab
 * @brief Object-oriented interface to BRL-CAD geometry.
 *
 * These are the methods for a notional object class "brlcad_solid".
 * The data for each instance is found separately in struct soltab.
 * This table is indexed by ID_xxx value of particular solid found in
 * st_id, or directly pointed at by st_meth.
 *
 */
/** @{ */
/** @file rt/functab.h */

#ifndef RT_FUNCTAB_H
#define RT_FUNCTAB_H

#include "common.h"
#include "vmath.h"
#include "bu/parse.h"
#include "bu/vls.h"
#include "bn/tol.h"
#include "bv.h"
#include "rt/geom.h"
#include "rt/defines.h"
#include "rt/application.h"
#include "rt/db_internal.h"
#include "rt/db_instance.h"
#include "rt/hit.h"
#include "rt/resource.h"
#include "rt/rt_instance.h"
#include "rt/seg.h"
#include "rt/soltab.h"
#include "rt/tol.h"
#include "rt/view.h"
#include "rt/xray.h"
#include "pc.h"
#include "nmg.h"
#include "brep.h"


__BEGIN_DECLS

/**
 * This needs to be at the end of the raytrace.h header file, so that
 * all the structure names are known.  The "union record" and "struct
 * nmgregion" pointers are problematic, so generic pointers are used
 * when those header files have not yet been seen.
 *
 * DEPRECATED: the size of this structure will likely change with new
 * size for ft_label and new object callbacks.
 */
struct rt_functab {
    uint32_t magic;
    char ft_name[17]; /* current longest name is 16 chars, need one element for terminating NULL */
    char ft_label[9]; /* current longest label is 8 chars, need one element for terminating NULL */

    int ft_use_rpp;

    int (*ft_prep)(struct soltab *stp,
		   struct rt_db_internal *ip,
		   struct rt_i *rtip);
#define RTFUNCTAB_FUNC_PREP_CAST(_func) ((int (*)(struct soltab *, struct rt_db_internal *, struct rt_i *))((void (*)(void))_func))

    int (*ft_shot)(struct soltab *stp,
		   struct xray *rp,
		   struct application *ap, /* has resource */
		   struct seg *seghead);
#define RTFUNCTAB_FUNC_SHOT_CAST(_func) ((int (*)(struct soltab *, struct xray *, struct application *, struct seg *))((void (*)(void))_func))

    void (*ft_print)(const struct soltab *stp);
#define RTFUNCTAB_FUNC_PRINT_CAST(_func) ((void (*)(const struct soltab *))((void (*)(void))_func))

    void (*ft_norm)(struct hit *hitp,
		    struct soltab *stp,
		    struct xray *rp);
#define RTFUNCTAB_FUNC_NORM_CAST(_func) ((void (*)(struct hit *, struct soltab *, struct xray *))((void (*)(void))_func))

    int (*ft_piece_shot)(struct rt_piecestate *psp,
			 struct rt_piecelist *plp,
			 double dist, /* correction to apply to hit distances */
			 struct xray *ray, /* ray transformed to be near cut cell */
			 struct application *ap, /* has resource */
			 struct seg *seghead);  /* used only for PLATE mode hits */
#define RTFUNCTAB_FUNC_PIECE_SHOT_CAST(_func) ((int (*)(struct rt_piecestate *, struct rt_piecelist *, double dist, struct xray *, struct application *, struct seg *))((void (*)(void))_func))

    void (*ft_piece_hitsegs)(struct rt_piecestate *psp,
			     struct seg *seghead,
			     struct application *ap); /* has resource */
#define RTFUNCTAB_FUNC_PIECE_HITSEGS_CAST(_func) ((void (*)(struct rt_piecestate *, struct seg *, struct application *))((void (*)(void))_func))

    void (*ft_uv)(struct application *ap, /* has resource */
		  struct soltab *stp,
		  struct hit *hitp,
		  struct uvcoord *uvp);
#define RTFUNCTAB_FUNC_UV_CAST(_func) ((void (*)(struct application *, struct soltab *, struct hit *, struct uvcoord *))((void (*)(void))_func))

    void (*ft_curve)(struct curvature *cvp,
		     struct hit *hitp,
		     struct soltab *stp);
#define RTFUNCTAB_FUNC_CURVE_CAST(_func) ((void (*)(struct curvature *, struct hit *, struct soltab *))((void (*)(void))_func))

    int (*ft_classify)(const struct soltab * /*stp*/, const vect_t /*min*/, const vect_t /*max*/, const struct bn_tol * /*tol*/);
#define RTFUNCTAB_FUNC_CLASS_CAST(_func) ((int (*)(const struct soltab *, const vect_t, const vect_t, const struct bn_tol *))((void (*)(void))_func))

    void (*ft_free)(struct soltab * /*stp*/);
#define RTFUNCTAB_FUNC_FREE_CAST(_func) ((void (*)(struct soltab *))((void (*)(void))_func))

    int (*ft_plot)(struct bu_list * /*vhead*/,
		   struct rt_db_internal * /*ip*/,
		   const struct bg_tess_tol * /*ttol*/,
		   const struct bn_tol * /*tol*/,
		   const struct bview * /*view info*/);
#define RTFUNCTAB_FUNC_PLOT_CAST(_func) ((int (*)(struct bu_list *, struct rt_db_internal *, const struct bg_tess_tol *, const struct bn_tol *, const struct bview *))((void (*)(void))_func))

    int (*ft_adaptive_plot)(struct bu_list * /*vhead*/,
	                    struct rt_db_internal * /*ip*/,
			    const struct bn_tol * /*tol*/,
			    const struct bview * /* view info */,
			    fastf_t /* s_size */);
#define RTFUNCTAB_FUNC_ADAPTIVE_PLOT_CAST(_func) ((int (*)(struct bu_list *, struct rt_db_internal *, const struct bn_tol *, const struct bview *, fastf_t))((void (*)(void))_func))

    void (*ft_vshot)(struct soltab * /*stp*/[],
		     struct xray *[] /*rp*/,
		     struct seg * /*segp*/,
		     int /*n*/,
		     struct application * /*ap*/);
#define RTFUNCTAB_FUNC_VSHOT_CAST(_func) ((void (*)(struct soltab *[], struct xray *[], struct seg *, int, struct application *))((void (*)(void))_func))

    int (*ft_tessellate)(struct nmgregion ** /*r*/,
			 struct model * /*m*/,
			 struct rt_db_internal * /*ip*/,
			 const struct bg_tess_tol * /*ttol*/,
			 const struct bn_tol * /*tol*/);
#define RTFUNCTAB_FUNC_TESS_CAST(_func) ((int (*)(struct nmgregion **, struct model *, struct rt_db_internal *, const struct bg_tess_tol *, const struct bn_tol *))((void (*)(void))_func))
    int (*ft_tnurb)(struct nmgregion ** /*r*/,
		    struct model * /*m*/,
		    struct rt_db_internal * /*ip*/,
		    const struct bn_tol * /*tol*/);
#define RTFUNCTAB_FUNC_TNURB_CAST(_func) ((int (*)(struct nmgregion **, struct model *, struct rt_db_internal *, const struct bn_tol *))((void (*)(void))_func))

    void (*ft_brep)(ON_Brep ** /*b*/,
		    struct rt_db_internal * /*ip*/,
		    const struct bn_tol * /*tol*/);
#define RTFUNCTAB_FUNC_BREP_CAST(_func) ((void (*)(ON_Brep **, struct rt_db_internal *, const struct bn_tol *))((void (*)(void))_func))

    int (*ft_import5)(struct rt_db_internal * /*ip*/,
		      const struct bu_external * /*ep*/,
		      const mat_t /*mat*/,
		      const struct db_i * /*dbip*/,
		      struct resource * /*resp*/);
#define RTFUNCTAB_FUNC_IMPORT5_CAST(_func) ((int (*)(struct rt_db_internal *, const struct bu_external *, const mat_t, const struct db_i *, struct resource *))((void (*)(void))_func))

    int (*ft_export5)(struct bu_external * /*ep*/,
		      const struct rt_db_internal * /*ip*/,
		      double /*local2mm*/,
		      const struct db_i * /*dbip*/,
		      struct resource * /*resp*/);
#define RTFUNCTAB_FUNC_EXPORT5_CAST(_func) ((int (*)(struct bu_external *, const struct rt_db_internal *, double, const struct db_i *, struct resource *))((void (*)(void))_func))

    int (*ft_import4)(struct rt_db_internal * /*ip*/,
		      const struct bu_external * /*ep*/,
		      const mat_t /*mat*/,
		      const struct db_i * /*dbip*/,
		      struct resource * /*resp*/);
#define RTFUNCTAB_FUNC_IMPORT4_CAST(_func) ((int (*)(struct rt_db_internal *, const struct bu_external *, const mat_t, const struct db_i *, struct resource *))((void (*)(void))_func))

    int (*ft_export4)(struct bu_external * /*ep*/,
		      const struct rt_db_internal * /*ip*/,
		      double /*local2mm*/,
		      const struct db_i * /*dbip*/,
		      struct resource * /*resp*/);
#define RTFUNCTAB_FUNC_EXPORT4_CAST(_func) ((int (*)(struct bu_external *, const struct rt_db_internal *, double, const struct db_i *, struct resource *))((void (*)(void))_func))

    void (*ft_ifree)(struct rt_db_internal * /*ip*/);
#define RTFUNCTAB_FUNC_IFREE_CAST(_func) ((void (*)(struct rt_db_internal *))((void (*)(void))_func))

    int (*ft_describe)(struct bu_vls * /*str*/,
		       const struct rt_db_internal * /*ip*/,
		       int /*verbose*/,
		       double /*mm2local*/);
#define RTFUNCTAB_FUNC_DESCRIBE_CAST(_func) ((int (*)(struct bu_vls *, const struct rt_db_internal *, int, double))((void (*)(void))_func))

    int (*ft_xform)(struct rt_db_internal * /*op*/,
		    const mat_t /*mat*/, struct rt_db_internal * /*ip*/,
		    int /*free*/, struct db_i * /*dbip*/);
#define RTFUNCTAB_FUNC_XFORM_CAST(_func) ((int (*)(struct rt_db_internal *, const mat_t, struct rt_db_internal *, int, struct db_i *))((void (*)(void))_func))

    const struct bu_structparse *ft_parsetab;   /**< @brief rt_xxx_parse */
    size_t ft_internal_size;    /**< @brief sizeof(struct rt_xxx_internal) */
    uint32_t ft_internal_magic; /**< @brief RT_XXX_INTERNAL_MAGIC */

    int (*ft_get)(struct bu_vls *, const struct rt_db_internal *, const char *item);
#define RTFUNCTAB_FUNC_GET_CAST(_func) ((int (*)(struct bu_vls *, const struct rt_db_internal *, const char *))((void (*)(void))_func))

    int (*ft_adjust)(struct bu_vls *, struct rt_db_internal *, int /*argc*/, const char ** /*argv*/);
#define RTFUNCTAB_FUNC_ADJUST_CAST(_func) ((int (*)(struct bu_vls *, struct rt_db_internal *, int, const char **))((void (*)(void))_func))

    int (*ft_form)(struct bu_vls *, const struct rt_functab *);
#define RTFUNCTAB_FUNC_FORM_CAST(_func) ((int (*)(struct bu_vls *, const struct rt_functab *))((void (*)(void))_func))

    void (*ft_make)(const struct rt_functab *, struct rt_db_internal * /*ip*/);
#define RTFUNCTAB_FUNC_MAKE_CAST(_func) ((void (*)(const struct rt_functab *, struct rt_db_internal *))((void (*)(void))_func))

    int (*ft_params)(struct pc_pc_set *, const struct rt_db_internal * /*ip*/);
#define RTFUNCTAB_FUNC_PARAMS_CAST(_func) ((int (*)(struct pc_pc_set *, const struct rt_db_internal *))((void (*)(void))_func))

    /* Axis aligned bounding box */
    int (*ft_bbox)(struct rt_db_internal * /*ip*/,
		   point_t * /*min X, Y, Z of bounding RPP*/,
		   point_t * /*max X, Y, Z of bounding RPP*/,
		   const struct bn_tol *);
#define RTFUNCTAB_FUNC_BBOX_CAST(_func) ((int (*)(struct rt_db_internal *, point_t *, point_t *, const struct bn_tol *))((void (*)(void))_func))

    void (*ft_volume)(fastf_t * /*vol*/, const struct rt_db_internal * /*ip*/);
#define RTFUNCTAB_FUNC_VOLUME_CAST(_func) ((void (*)(fastf_t *, const struct rt_db_internal *))((void (*)(void))_func))

    void (*ft_surf_area)(fastf_t * /*area*/, const struct rt_db_internal * /*ip*/);
#define RTFUNCTAB_FUNC_SURF_AREA_CAST(_func) ((void (*)(fastf_t *, const struct rt_db_internal *))((void (*)(void))_func))

    void (*ft_centroid)(point_t * /*cent*/, const struct rt_db_internal * /*ip*/);
#define RTFUNCTAB_FUNC_CENTROID_CAST(_func) ((void (*)(point_t *, const struct rt_db_internal *))((void (*)(void))_func))

    int (*ft_oriented_bbox)(struct rt_arb_internal * /* bounding arb8 */,
			    struct rt_db_internal * /*ip*/,
			    const fastf_t);
#define RTFUNCTAB_FUNC_ORIENTED_BBOX_CAST(_func) ((int (*)(struct rt_arb_internal *, struct rt_db_internal *, const fastf_t))((void (*)(void))_func))

    /** get a list of the selections matching a query */
    struct rt_selection_set *(*ft_find_selections)(const struct rt_db_internal *,
						   const struct rt_selection_query *);
#define RTFUNCTAB_FUNC_FIND_SELECTIONS_CAST(_func) ((struct rt_selection_set *(*)(const struct rt_db_internal *, const struct rt_selection_query *))((void (*)(void))_func))

    /**
     * evaluate a logical selection expression (e.g. a INTERSECT b,
     * NOT a) to create a new selection
     */
    struct rt_selection *(*ft_evaluate_selection)(const struct rt_db_internal *,
						  int op,
						  const struct rt_selection *,
						  const struct rt_selection *);
#define RTFUNCTAB_FUNC_EVALUATE_SELECTION_CAST(_func) ((struct rt_selection *(*)(const struct rt_db_internal *, int op, const struct rt_selection *, const struct rt_selection *))((void (*)(void))_func))

    /** apply an operation to a selected subset of a primitive */
    int (*ft_process_selection)(struct rt_db_internal *,
				struct db_i *,
				const struct rt_selection *,
				const struct rt_selection_operation *);
#define RTFUNCTAB_FUNC_PROCESS_SELECTION_CAST(_func) ((int (*)(struct rt_db_internal *, struct db_i *, const struct rt_selection *, const struct rt_selection_operation *))((void (*)(void))_func))

    /** cache and uncache prep data for faster future lookup */
    int (*ft_prep_serialize)(struct soltab *stp, const struct rt_db_internal *ip, struct bu_external *external, size_t *version);
#define RTFUNCTAB_FUNC_PREP_SERIALIZE_CAST(_func) ((int (*)(struct soltab *, const struct rt_db_internal *, struct bu_external *, size_t *))((void (*)(void))_func))

    /** generate struct bv_scene_obj labels for the primitive */
    void (*ft_labels)(struct bu_ptbl *labels, const struct rt_db_internal *ip, struct bview *v);
#define RTFUNCTAB_FUNC_LABELS_CAST(_func) ((void (*)(struct bu_ptbl *, const struct rt_db_internal *, struct bview *))((void (*)(void))_func))

};


RT_EXPORT extern const struct rt_functab OBJ[];

#define RT_CK_FUNCTAB(_p) BU_CKMAG(_p, RT_FUNCTAB_MAGIC, "functab");

RT_EXPORT extern const struct rt_functab *rt_get_functab_by_label(const char *label);

__END_DECLS

#endif /* RT_FUNCTAB_H */
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
