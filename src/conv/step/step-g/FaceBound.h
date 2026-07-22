/*                 FaceBound.h
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
/** @file step/FaceBound.h
 *
 * Class definition used to convert STEP "FaceBound" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef CONV_STEP_STEP_G_FACEBOUND_H
#define CONV_STEP_STEP_G_FACEBOUND_H

#include "TopologicalRepresentationItem.h"

#include <memory>
#include <vector>

// forward declaration of class
class Loop;
class ON_BoundingBox;
class ON_Brep;
class ON_3dPoint;
class ON_3dVector;
namespace brlcad { class PullbackContext; }

class FaceBound : public TopologicalRepresentationItem
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

protected:
    Loop *bound;
    int ON_face_index;
    bool inner;
    Boolean orientation;


public:
    FaceBound();
    virtual ~FaceBound();
    FaceBound(STEPWrapper *sw, int step_id);
    void SetInner() {
	inner = true;
    };
    void SetOuter() {
	inner = false;
    };
    bool IsInner() {
	return inner;
    }
    bool IsOuter() {
	return !inner;
    }
    ON_BoundingBox *GetEdgeBounds(ON_Brep *brep);
    bool GrowTopologyVertexBounds(ON_BoundingBox *bounds);
    bool GetEdgeAxisProjectionBounds(ON_Brep *brep,
	const ON_3dPoint &origin, const ON_3dVector &axis,
	double *minimum, double *maximum);
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    bool CreateONLoop(ON_Brep *brep);
    bool PrepareONBrep(ON_Brep *brep, std::vector<int> *edge_indices);
    bool FinishONBrep(ON_Brep *brep, int loop_index,
	const std::vector<int> &edge_indices,
	double item_scale_override = 0.0,
	std::shared_ptr<brlcad::PullbackContext> surface_cache =
	    std::shared_ptr<brlcad::PullbackContext>());
    virtual bool LoadONBrep(ON_Brep *brep);
    virtual void Print(int level);
    bool Oriented();
    void SetFaceIndex(int index) {
	ON_face_index = index;
    };

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_FACEBOUND_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
