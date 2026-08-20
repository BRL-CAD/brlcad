/*                    S O L I D R E P L I C A . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CONV_STEP_STEP_G_SOLIDREPLICA_H
#define CONV_STEP_STEP_G_SOLIDREPLICA_H

#include "SolidModel.h"

class CartesianTransformationOperator3D;

/** Exact STEP solid_replica support.
 *
 * The parent solid is materialized into an OpenNURBS BREP and the asserted
 * uniform Cartesian transformation is applied to the complete topology.
 * Parent solid families that cannot produce an exact BREP are rejected by
 * LoadONBrep; callers report and skip them without synthesizing geometry.
 */
class SolidReplica : public SolidModel
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

    SolidModel *parent_solid;
    CartesianTransformationOperator3D *transformation;
    bool converting;

public:
    SolidReplica();
    SolidReplica(STEPWrapper *sw, int step_id);
    virtual ~SolidReplica();

    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    virtual bool LoadONBrep(ON_Brep *brep);
    virtual void Print(int level);

    SolidModel *ParentSolid() const { return parent_solid; }
    CartesianTransformationOperator3D *Transformation() const { return transformation; }

    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_SOLIDREPLICA_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
