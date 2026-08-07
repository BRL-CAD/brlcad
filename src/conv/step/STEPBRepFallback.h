/*                    S T E P B R E P F A L L B A C K . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_STEPBREPFALLBACK_H
#define CONV_STEP_STEPBREPFALLBACK_H

#include "common.h"

#include "ON_Brep.h"
#include "raytrace.h"

namespace brlcad {
namespace step {

/** Invoke librt's primitive-to-BRep callback using its historical in-place
 * allocation contract.  A few deliberately unsupported callbacks signal
 * failure by replacing the pointer with NULL, so retain and release the
 * original allocation in that case.  Only a BRep containing faces is useful
 * to STEP's solid exporter.  The caller owns a non-NULL result. */
inline ON_Brep *
BRepFallback(struct rt_db_internal *internal, const struct bn_tol *tolerance)
{
    if (!internal || !internal->idb_meth || !internal->idb_meth->ft_brep)
	return NULL;
    ON_Brep *allocated = ON_Brep::New();
    ON_Brep *result = allocated;
    if (rt_obj_brep(&result, internal, tolerance) < 0)
	result = NULL;
    if (result != allocated) delete allocated;
    if (!result || !result->m_F.Count()) {
	delete result;
	return NULL;
    }
    return result;
}

} // namespace step
} // namespace brlcad

#endif /* CONV_STEP_STEPBREPFALLBACK_H */
