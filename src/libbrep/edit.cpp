/*                        E D I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 1994-2023 United States Government as represented by
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
/** @file edit.cpp
 *
 * Implementation of edit support for brep.
 *
 */

#include "brep/edit.h"

void *brep_create()
{
    ON_Brep* brep = new ON_Brep();
    return (void *)brep;
}

ON_NurbsCurve *brep_make_curve(int argc, const char **argv)
{
    ON_NurbsCurve *curve = new ON_NurbsCurve(3, true, 3, 4);
    curve->SetCV(0, ON_3dPoint(-0.1, -1.5, 0));
    curve->SetCV(1, ON_3dPoint(0.1, -0.5, 0));
    curve->SetCV(2, ON_3dPoint(0.1, 0.5, 0));
    curve->SetCV(3, ON_3dPoint(-0.1, 1.5, 0));
    curve->SetKnot(0, 0);
    curve->SetKnot(1, 0);
    curve->SetKnot(2, 0.5);
    curve->SetKnot(3, 1);
    curve->SetKnot(4, 1);
    if (argc == 3)
        curve->Translate(ON_3dVector(atof(argv[0]), atof(argv[1]), atof(argv[2])));
    return curve;
}
// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
