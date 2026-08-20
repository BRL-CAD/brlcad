/*                 Path.h
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file step/Path.h
 *
 * Class definition used to convert STEP "Path" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef CONV_STEP_STEP_G_PATH_H
#define CONV_STEP_STEP_G_PATH_H

#include "common.h"

#include <list>
#include <memory>
#include <vector>

#include "TopologicalRepresentationItem.h"

// forward declaration of class
class ON_BoundingBox;
class ON_Brep;
class ON_3dPoint;
class ON_3dVector;
class OrientedEdge;
namespace brlcad { class PullbackContext; }
typedef list<OrientedEdge *> LIST_OF_ORIENTED_EDGES;

typedef struct trim_curve2d {
    double start[2];
    double end[2];
    int curve_index;
} TrimCurve2d;
typedef list<TrimCurve2d *> LIST_OF_TRIMS;
typedef list<LIST_OF_TRIMS *> LIST_OF_TRIM_PATHS;

class Path : public TopologicalRepresentationItem
{
private:
    LIST_OF_TRIM_PATHS paths;
    static string entityname;
    static EntityInstanceFunc GetInstance;
    bool isSeam(LIST_OF_ORIENTED_EDGES::iterator i);
    LIST_OF_ORIENTED_EDGES::iterator getNext(LIST_OF_ORIENTED_EDGES::iterator i);
    LIST_OF_ORIENTED_EDGES::iterator getPrev(LIST_OF_ORIENTED_EDGES::iterator i);

protected:
    LIST_OF_ORIENTED_EDGES edge_list;
    int ON_path_index;

public:
    Path();
    virtual ~Path();
    Path(STEPWrapper *sw, int step_id);
    virtual ON_BoundingBox *GetEdgeBounds(ON_Brep *brep);
    bool GrowTopologyVertexBounds(ON_BoundingBox *bounds);
    bool GetEdgeAxisProjectionBounds(ON_Brep *brep,
	const ON_3dPoint &origin, const ON_3dVector &axis,
	double *minimum, double *maximum);
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual bool LoadONBrep(ON_Brep *brep);
    /** Load the immutable 3-D edge topology needed by a later trim job and
     * retain its destination-BREP edge indices.  Face batching calls this
     * serially because some STEP curve adapters cache endpoint information. */
    bool PrepareONBrepEdges(ON_Brep *brep, std::vector<int> *edge_indices);
    bool LoadONTrimmingCurves(ON_Brep *brep);
    /** Construct this path's trims using caller-owned topology indices.  This
     * form is safe for an independently owned face BREP and does not consult
     * mutable destination indices cached on shared STEP entities. */
    bool LoadONTrimmingCurves(ON_Brep *brep, int loop_index,
	const std::vector<int> &edge_indices,
	double item_scale_override = 0.0,
	std::shared_ptr<brlcad::PullbackContext> surface_cache =
	    std::shared_ptr<brlcad::PullbackContext>());
    virtual void Print(int level);
    void SetPathIndex(int index) {
	ON_path_index = index;
    };
    bool ShiftSurfaceSeam(ON_Brep *brep, double *t);
    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_PATH_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
