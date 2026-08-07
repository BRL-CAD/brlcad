/*                 FaceSurface.h
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
/** @file step/FaceSurface.h
 *
 * Class definition used to convert STEP "FaceSurface" to BRL-CAD BREP
 * structures.
 *
 */

#ifndef CONV_STEP_STEP_G_FACESURFACE_H
#define CONV_STEP_STEP_G_FACESURFACE_H

#include "Face.h"
#include "GeometricRepresentationItem.h"

// forward declaration of class
class Surface;

class FaceSurface: public Face , public GeometricRepresentationItem
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

protected:
    Surface *face_geometry;
    Boolean same_sense;


public:
    FaceSurface();
    virtual ~FaceSurface();
    FaceSurface(STEPWrapper *sw, int step_id);
    void AddFace(ON_Brep *brep);
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual bool LoadONBrep(ON_Brep *brep);
    /** Serially build the face surface, loops, and 3-D edge topology.  The
     * expensive pcurve phase is deliberately deferred to FinishONBrep(). */
    bool PrepareONBrep(ON_Brep *brep,
	std::vector<Face::PreparedBound> *prepared_bounds);
    /** Complete trims on an independently owned face BREP. */
    bool FinishONBrep(ON_Brep *brep,
	const std::vector<Face::PreparedBound> &prepared_bounds,
	double item_scale_override = 0.0);
    size_t PullbackSpanEstimate() const;
    virtual void Print(int level);

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_FACESURFACE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
