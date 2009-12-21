/**
 * Intersect a ray with an 'revolve' primitive object.
 *
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
    fastf_t		bounds[4];	/**< @brief 2D sketch bounds  */
};


const struct bu_structparse rt_revolve_parse[] = {
    { "%f", 3, "V",   bu_offsetof(struct rt_revolve_internal, v3d[X]),  BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "axis", bu_offsetof(struct rt_revolve_internal, axis3d[X]),  BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 3, "R",   bu_offsetof(struct rt_revolve_internal, r[X]), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%f", 1, "ang", bu_offsetof(struct rt_revolve_internal, ang),    BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { "%V", 1, "sk_name", bu_offsetof(struct rt_revolve_internal, sketch_name),  BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    { {'\0', '\0', '\0', '\0'}, 0, (char *)NULL, 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
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
