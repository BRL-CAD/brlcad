/*        S H E L L B A S E D S U R F A C E M O D E L . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file ShellBasedSurfaceModel.h
 *
 * Brief description
 *
 */

#ifndef CONV_STEP_STEP_G_SHELLBASEDSURFACEMODEL_H
#define CONV_STEP_STEP_G_SHELLBASEDSURFACEMODEL_H

#include "GeometricRepresentationItem.h"

// forward declaration of class
class OpenShell;
class STEPWrapper;
class ON_Brep;

typedef std::list<OpenShell *> LIST_OF_OPEN_SHELLS;

class ShellBasedSurfaceModel: public GeometricRepresentationItem
{
private:
    static string entityname;
    static EntityInstanceFunc GetInstance;

protected:
    LIST_OF_OPEN_SHELLS sbsm_boundary;

public:
    ShellBasedSurfaceModel();
    ShellBasedSurfaceModel(STEPWrapper *sw, int step_id);
    virtual ~ShellBasedSurfaceModel();
    bool Load(STEPWrapper *sw, SDAI_Application_instance *sse);
    ON_Brep *GetONBrep();
    virtual bool LoadONBrep(ON_Brep *brep);
    virtual void Print(int level);

    //static methods
    static STEPEntity *Create(STEPWrapper *sw, SDAI_Application_instance *sse);
};

#endif /* CONV_STEP_STEP_G_SHELLBASEDSURFACEMODEL_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
