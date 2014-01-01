/*              S U R F A C E I N T E R S E C T . H
 * BRL-CAD
 *
 * Copyright (c) 2009-2014 United States Government as represented by
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

#ifndef PROC_DB_SURFACEINTERSECT_H
#define PROC_DB_SURFACEINTERSECT_H

#include "common.h"

/* common interface headers */
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"
#include "bn.h"
#include "bu.h"
#include "vmath.h"
#include "opennurbs_array.h"


/**
 * Canonical_start, Canonical_end
 *
 * @brief canonical domain for ON_Curves
 */
#define Canonical_start (0.0)
#define Canonical_end (1.0)

/**
 * class Face_X_Event
 *
 * @brief analogous to ON_X_EVENT but between ON_BrepFaces
 */
class Face_X_Event{
public:
    ON_BrepFace *face1;
    ON_BrepFace *face2;
    ON_Curve *curve1;
    ON_Curve *curve2;
    ON_ClassArray<ON_Curve*> new_curves1;
    ON_ClassArray<ON_Curve*> new_curves2;
    ON_ClassArray<bool> loop_flags1;
    ON_ClassArray<bool> loop_flags2;
    ON_ClassArray<ON_X_EVENT> x;
    Face_X_Event();
    Face_X_Event(ON_BrepFace*, ON_BrepFace*);
    Face_X_Event(ON_BrepFace*, ON_BrepFace*, ON_Curve*, ON_Curve*);
    int Get_ON_X_Events(double);
    int Render_Curves();
};

#endif /* PROC_DB_SURFACEINTERSECT_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
