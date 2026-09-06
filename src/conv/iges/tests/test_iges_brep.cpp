/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "common.h"

#include <memory>
#include <vector>

#include "bu/app.h"
#include "bu/log.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "brep.h"

namespace {

constexpr int SAMPLES = 9;
/* Match the converter's default model resolution. */
constexpr double DISTANCE_TOLERANCE_MM = BN_TOL_DIST;

std::unique_ptr<ON_Brep>
load_brep(const char *path, const char *name)
{
    struct db_i *dbip = db_open(path, DB_OPEN_READONLY);
    if (!dbip || db_dirbuild(dbip))
	bu_exit(1, "Cannot open %s\n", path);
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (!dp || rt_db_get_internal(&intern, dp, dbip, NULL) < 0)
	bu_exit(1, "Cannot load %s\n", name);

    ON_Brep *brep = NULL;
    if (intern.idb_type == ID_BREP) {
	const auto *bi = static_cast<const struct rt_brep_internal *>(intern.idb_ptr);
	brep = new ON_Brep(*bi->brep);
    } else if (intern.idb_meth->ft_brep) {
	struct bn_tol tol = BN_TOL_INIT_TOL;
	brep = ON_Brep::New();
	intern.idb_meth->ft_brep(&brep, &intern, &tol);
    }
    rt_db_free_internal(&intern);
    db_close(dbip);
    if (!brep || !brep->IsValid() || !brep->m_F.Count())
	bu_exit(1, "%s has no valid BRep representation\n", name);
    return std::unique_ptr<ON_Brep>(brep);
}

bool
same_point(const ON_3dPoint &a, const ON_3dPoint &b)
{
    return a.IsValid() && b.IsValid() && a.DistanceTo(b) <= DISTANCE_TOLERANCE_MM;
}

bool
same_curve(const ON_Curve &a, const ON_Curve &b, bool reverse)
{
    ON_NurbsCurve na, nb;
    if (!a.GetNurbForm(na) || !b.GetNurbForm(nb))
	return false;
    for (int i = 0; i < SAMPLES; ++i) {
	const double fraction = double(i) / (SAMPLES - 1);
	if (!same_point(na.PointAt(na.Domain().ParameterAt(fraction)),
		nb.PointAt(nb.Domain().ParameterAt(reverse ? 1.0 - fraction : fraction))))
	    return false;
    }
    return true;
}

bool
same_face(const ON_BrepFace &a, const ON_BrepFace &b)
{
    ON_NurbsSurface na, nb;
    if (!a.GetNurbForm(na) || !b.GetNurbForm(nb) ||
	a.m_bRev != b.m_bRev || a.LoopCount() != b.LoopCount())
	return false;
    for (int u = 0; u < SAMPLES; ++u) {
	for (int v = 0; v < SAMPLES; ++v) {
	    const double fu = double(u) / (SAMPLES - 1);
	    const double fv = double(v) / (SAMPLES - 1);
	    if (!same_point(na.PointAt(na.Domain(0).ParameterAt(fu), na.Domain(1).ParameterAt(fv)),
		    nb.PointAt(nb.Domain(0).ParameterAt(fu), nb.Domain(1).ParameterAt(fv))))
		return false;
	}
    }
    return true;
}

}

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 5)
	bu_exit(1, "Usage: %s source.g primitive imported.g brep\n", argv[0]);
    ON::Begin();
    const auto source = load_brep(argv[1], argv[2]);
    const auto imported = load_brep(argv[3], argv[4]);
    if (source->IsSolid() != imported->IsSolid() ||
	source->m_F.Count() != imported->m_F.Count() ||
	source->m_E.Count() != imported->m_E.Count() ||
	source->m_L.Count() != imported->m_L.Count() ||
	source->m_T.Count() != imported->m_T.Count())
	bu_exit(1, "BRep round trip changed topology counts or solid status\n");

    /* Compare in NURBS parameter space: an analytic surface's angular
     * parameters need not match those of its rational NURBS form.  This
     * checks the exchange without depending on BRep ray-tracer support.
     * Shell traversal can reorder faces and edges during the exchange. */
    std::vector<bool> matched_faces(imported->m_F.Count(), false);
    for (int i = 0; i < source->m_F.Count(); ++i) {
	bool found = false;
	for (int j = 0; j < imported->m_F.Count(); ++j) {
	    if (!matched_faces[j] && same_face(source->m_F[i], imported->m_F[j])) {
		matched_faces[j] = true;
		found = true;
		break;
	    }
	}
	if (!found)
	    bu_exit(1, "BRep round trip changed face %d\n", i);
    }
    std::vector<bool> matched_edges(imported->m_E.Count(), false);
    for (int i = 0; i < source->m_E.Count(); ++i) {
	const ON_BrepEdge &a = source->m_E[i];
	bool found = false;
	for (int j = 0; j < imported->m_E.Count(); ++j) {
	    const ON_BrepEdge &b = imported->m_E[j];
	    if (!matched_edges[j] && a.m_ti.Count() == b.m_ti.Count() &&
		(same_curve(a, b, false) || same_curve(a, b, true))) {
		matched_edges[j] = true;
		found = true;
		break;
	    }
	}
	if (!found)
	    bu_exit(1, "BRep round trip changed edge %d\n", i);
    }
    bu_log("Matched %d faces and %d edges with preserved topology and solid status\n",
	source->m_F.Count(), source->m_E.Count());
    return 0;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 */
