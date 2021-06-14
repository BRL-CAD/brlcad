/*         P O L Y G O N _ C I R C L E _ C O N T R O L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2021 United States Government as represented by
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
/** @file polygon_circle_control.cpp
 *
 */

#include "polygon_circle_control.h"

QCirclePolyControl::QCirclePolyControl(QString s)
    : QPushButton(s)
{
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
}

QCirclePolyControl::~QCirclePolyControl()
{
}

bool QCirclePolyControl::eventFilter(QObject *, QEvent *)
{
    printf("polygon circle filter\n");
    return false;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
