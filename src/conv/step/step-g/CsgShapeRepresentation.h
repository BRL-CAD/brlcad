/*              C s g S h a p e R e p r e s e n t a t i o n . h
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#ifndef CONV_STEP_STEP_G_CSGSHAPEREPRESENTATION_H
#define CONV_STEP_STEP_G_CSGSHAPEREPRESENTATION_H

#include "ShapeRepresentation.h"

class CsgShapeRepresentation : public ShapeRepresentation
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

public:
    CsgShapeRepresentation();
    CsgShapeRepresentation(STEPWrapper *sw, int step_id);
    virtual ~CsgShapeRepresentation();
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_CSGSHAPEREPRESENTATION_H */
