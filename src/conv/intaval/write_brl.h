/*                    W R I T E _ B R L . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file write_brl.h
 *
 * INTAVAL Target Geometry File to BRL-CAD converter:
 * primitives into BRL-CAD database writing functions declaration
 *
 *  Origin -
 *	TNO (Netherlands)
 *	IABG mbH (Germany)
 */

#ifndef WRITE_BRL_INCLUDED
#define WRITE_BRL_INCLUDED

#include "glob.h"
#include "regtab.h"


void addTriangle
(
    int*    faces,
    size_t& num_faces,
    int     a,
    int     b,
    int     c
);


void writeTitle
(
    rt_wdb* wdbp,
    char*   string
);


void writePipe
(
    rt_wdb* wdbp,
    Form&   form,
    bool    translate
);


void writeRectangularBox
(
    rt_wdb* wdbp,
    Form&   form,
    bool    translate
);


void writeSolidBot
(
    rt_wdb* wdbp,
    Form&   form,
    bool    translate
);


void writeRingModeBox
(
    rt_wdb* wdbp,
    Form&   form,
    bool    translate
);


void writePlateBot
(
    rt_wdb* wdbp,
    Form&   form,
    bool    translate
);


void writeCone
(
    rt_wdb* wdbp,
    Form&   form,
    bool    translate
);


void writeCylinder
(
    rt_wdb* wdbp,
    Form&   form,
    bool    translate
);


void writeArb8
(
    rt_wdb* wdbp,
    Form&   form,
    bool    translate
);


#endif // WRITE_BRL_INCLUDED
