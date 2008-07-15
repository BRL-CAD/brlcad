/**
 * Intersect a ray with an 'revolve' primitive object.
 *
 * Adding a new solid type:
 *	Design disk record
 *
 *	define rt_revolve_internal --- parameters for solid
 *	define revolve_specific --- raytracing form, possibly w/precomuted terms
 *	define rt_revolve_parse --- struct bu_structparse for "db get", "db adjust", ...
 *
 *	code import/export/describe/print/ifree/plot/prep/shot/curve/uv/tess
 *
 *	edit db.h add solidrec s_type define
 *	edit rtgeom.h to add rt_revolve_internal
 *      edit magic.h to add RT_REVOLVE_INTERNAL_MAGIC
 *	edit table.c:
 *		RT_DECLARE_INTERFACE()
 *		struct rt_functab entry
 *		rt_id_solid()
 *	edit raytrace.h to make ID_REVOLVE, increment ID_MAXIMUM
 *	edit db_scan.c to add the new solid to db_scan()
 *	edit Makefile.am to add g_revolve.c to compile
 *
 *	Then:
 *	go to src/libwdb and create mk_revolve() routine
 *	go to src/conv and edit g2asc.c and asc2g.c to support the new solid
 *	go to src/librt and edit tcl.c to add the new solid to
 *		rt_solid_type_lookup[]
 *		also add the interface table and to rt_id_solid() in table.c
 *	go to src/mged and create the edit support
 */

#include "common.h"

#include "bu.h"
#include "bn.h"


/* ray tracing form of solid, including precomputed terms */
struct revolve_specific {
    point_t		v3d;	/**< @brief vertex in 3d space  */
    vect_t		zUnit;	/**< @brief revolve axis in 3d space, y axis */
    vect_t		xUnit;	/**< @brief vector in start plane, x axis */
    vect_t		yUnit;
    vect_t		rEnd;
    fastf_t		ang;	/**< @brief angle to revolve */
    char		*sketch_name;	/**< @brief name of sketch */
    struct rt_sketch_internal *sk;	/**< @brief pointer to sketch */
    int			*ends;	/**< @brief indices of points at end of continuous path */
};

const struct bu_structparse rt_revolve_parse[] = {
    { "%f", 3, "V",   bu_offsetof(struct rt_revolve_internal, v3d[X]),  BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "axis",bu_offsetof(struct rt_revolve_internal, axis3d[X]),  BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 3, "R",   bu_offsetof(struct rt_revolve_internal, r[X]), BU_STRUCTPARSE_FUNC_NULL },
    { "%f", 1, "ang", bu_offsetof(struct rt_revolve_internal, ang),    BU_STRUCTPARSE_FUNC_NULL },
    { "%S", 1, "sk_name", bu_offsetof(struct rt_revolve_internal, sketch_name),  BU_STRUCTPARSE_FUNC_NULL },
    { {'\0', '\0', '\0', '\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL }
};

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
