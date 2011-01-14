/*                         R E A D _ D R A . C P P
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
/** @file read_dra.cpp
 *
 * INTAVAL Target Geometry File to BRL-CAD converter:
 * reads the INTAVAL data in
 *
 *  Origin -
 *	TNO (Netherlands)
 *	IABG mbH (Germany)
 */

#include <iostream>

#include "glob.h"
#include "regtab.h"
#include "write_brl.h"
#include "read_dra.h"


size_t addBotPoint
(
    Form& form,
    int   x,
    int   y,
    int   z
) {
    size_t ret = form.bot.num_vertices;

    // search for duplicate vertex
    for(size_t i = 0; i < form.bot.num_vertices; ++i) {
        if ((form.bot.vertices[i * 3]     == x) &&
            (form.bot.vertices[i * 3 + 1] == y) &&
            (form.bot.vertices[i * 3 + 2] == z)) {
            ret = i;
            break;
        }
    }

    if (ret == form.bot.num_vertices) {
	// add a new vertex
	form.bot.vertices[form.bot.num_vertices * 3]     = x;
	form.bot.vertices[form.bot.num_vertices * 3 + 1] = y;
	form.bot.vertices[form.bot.num_vertices * 3 + 2] = z;

	++form.bot.num_vertices;
    }

    return ret; // return index of vertex
}


void addBotTriangle
(
    Form& form,
    int   a,
    int   b,
    int   c
) {
    // all three points on a line?
    int ax = form.bot.vertices[a * 3] - form.bot.vertices[b * 3];
    int ay = form.bot.vertices[a * 3 + 1] - form.bot.vertices[b * 3 + 1];
    int az = form.bot.vertices[a * 3 + 2] - form.bot.vertices[b * 3 + 2];
    int bx = form.bot.vertices[a * 3] - form.bot.vertices[c * 3];
    int by = form.bot.vertices[a * 3 + 1] - form.bot.vertices[c * 3 + 1];
    int bz = form.bot.vertices[a * 3 + 2] - form.bot.vertices[c * 3 + 2];

    int di = ay * bz - az * by;
    int dj = az * bx - ax * bz;
    int dk = ax * by - ay * bx;

    if ((di != 0) || (dj != 0) || (dk != 0))
        addTriangle(form.bot.faces, form.bot.num_faces, a, b, c);
}


void readCadTypeBot
(
    std::istream& is,
    Form&         form
) {
    form.bot.num_faces    = 0; // unknown yet how many different faces are used, there may be some degenerated ones
    form.bot.num_vertices = 0; // unknown yet how many different points are used

    for(size_t i = 0; i < form.npts; ++i) {
        int x;
        int y;
        int z;

        is >> x >> y >> z;
        int a = addBotPoint(form, x, y, z);
        is >> x >> y >> z;
        int b = addBotPoint(form, x, y, z);
        is >> x >> y >> z;
        int c = addBotPoint(form, x, y, z);

        addBotTriangle(form, a, b, c);
    }
}


void readLongFormBot
(
    std::istream& is,
    Form&         form
) {
    form.bot.num_faces    = 0; // unknown yet how many different faces are used
    form.bot.num_vertices = 0; // unknown yet how many different points are used

    int x;
    int y;
    int z;

    is >> x >> y >> z;
    int a = addBotPoint(form, x, y, z);
    is >> x >> y >> z;
    int b = addBotPoint(form, x, y, z);
    is >> x >> y >> z;
    int c = addBotPoint(form, x, y, z);

    addBotTriangle(form, a, b, c);

    for (size_t i = 3; i < form.npts; ++i) {
        a = b;
        b = c;

        is >> x >> y >> z;
        c = addBotPoint(form, x, y, z);

        addBotTriangle(form, a, b, c);
    }
}


void readCadTypeBox
(
    std::istream& is,
    Form&         form
) {
    is >> form.compnr;
    is >> form.s_compnr;

    form.npts = -form.id - 20000;

    readCadTypeBot(is, form);

    std::cout << "CadTypeBox ("
              << form.compnr
              << ") with "
              << form.npts
              << " triangles"
              << std::endl;
}


void readRingModeBox
(
    std::istream& is,
    Form&         form
) {
    is >> form.compnr;
    is >> form.s_compnr;
    is >> form.thickness;
    is >> form.width;

    form.npts = -form.id - 10000;

    for (size_t i = 0; i < form.npts; ++i)
        is >> form.pt[i][0] >> form.pt[i][1] >> form.pt[i][2];

    std::cout << "RingModeBox ("
              << form.compnr
              << ") with "
              << form.npts
              << " points"
              << std::endl;
}


void readPlateModeBox
(
    std::istream& is,
    Form&         form
) {
    is >> form.compnr;
    is >> form.s_compnr;
    is >> form.thickness;

    form.npts = -form.id;

    readLongFormBot(is, form);

    std::cout << "PlateModeBox ("
              << form.compnr
              << ") with "
              << form.npts
              << " points"
              << std::endl;
}


void readPipe
(
    std::istream& is,
    Form&         form
) {
    is >> form.compnr;
    is >> form.s_compnr;

    if (form.id == 1)
        form.npts = 2;
    else
        is >> form.npts;

    for (size_t i = 0; i<form.npts; ++i)
        is >> form.pt[i][0] >> form.pt[i][1] >> form.pt[i][2];

    is >> form.radius1;

    std::cout << "Pipe ("
              << form.compnr
              << ") with "
              << form.npts
              << " points"
              << std::endl;
}


void readRectangularBox
(
    std::istream& is,
    Form&         form
) {
    is >> form.compnr;
    is >> form.s_compnr;

    form.npts = 2;

    for (size_t i = 0; i < form.npts; ++i)
        is >> form.pt[i][0] >> form.pt[i][1] >> form.pt[i][2];

    std::cout << "RectangularBox ("
              << form.compnr
              << ") with "
              << form.npts
              << " points"
              << std::endl;
}


void readCone
(
    std::istream& is,
    Form&         form
) {
    is >> form.compnr;
    is >> form.s_compnr;

    form.npts = 2;

    is >> form.pt[0][0] >> form.pt[0][1] >> form.pt[0][2];
    is >> form.radius1;
    is >> form.pt[1][0] >> form.pt[1][1] >> form.pt[1][2];
    is >> form.radius2;

    std::cout << "Cone ("
              << form.compnr
              << ") with "
              << form.npts
              << " points"
              << std::endl;
}


void readCylinder
(
    std::istream& is,
    Form&         form
) {
    is >> form.compnr;
    is >> form.s_compnr;

    form.npts = 2;

    for (size_t i = 0; i < form.npts; ++i)
        is >> form.pt[i][0] >> form.pt[i][1] >> form.pt[i][2];

    is >> form.radius1;

    std::cout << "Cylinder ("
              << form.compnr
              << ") with "
              << form.npts
              << " points"
              << std::endl;
}


void readArb8
(
    std::istream& is,
    Form&         form
) {
    is >> form.compnr;
    is >> form.s_compnr;

    form.npts = 8;

    for (size_t i = 0; i < form.npts; ++i)
        is >> form.pt[i][0] >> form.pt[i][1] >> form.pt[i][2];

    std::cout << "Arb8 ("
              << form.compnr
              << ") with "
              << form.npts
              << " points"
              << std::endl;
}


void readLongForm
(
    std::istream& is,
    Form&         form
) {
    is >> form.compnr;
    is >> form.s_compnr;

    form.npts = form.id;

    readLongFormBot(is, form);

    std::cout << "LongForm ("
              << form.compnr
              << ") with "
              << form.npts
              << " points"
              << std::endl;
}


//
// Does the conversion
//
void conv
(
    std::istream& is,
    rt_wdb*       wdbp
) {
    char title[LINELEN];
    is.getline(title, LINELEN);

    if (is) {
        writeTitle(wdbp, title);

        Form form;
        bool translatedShape = false;
        int  id;

        is >> id;

        while (is) {
            if (id == 0) {
                // identical shape, dimensions and orientation
                // form is grotendeels al gevuld
                // lees compnr en s_compnr  en translatie
                // alleen schrijfactie voor form.id nog nodig

                is >> form.compnr;
                is >> form.s_compnr;

                is >> form.tr_vec[0] >> form.tr_vec[1] >> form.tr_vec[2];
                translatedShape = true;

                std::cout << "Identical shape" << std::endl;
            }
            else {
                form.id         = id;
                translatedShape = false;
            }

            if (form.id <= -20000) {
                // CAD type box
                if (!translatedShape)
                    readCadTypeBox(is, form);

                writeSolidBot(wdbp, form, translatedShape);
            }
            else if (form.id <= -10000) {
                // ring mode box
                if (!translatedShape)
                    readRingModeBox(is, form);

                writeRingModeBox(wdbp, form, translatedShape);
            }
            else if (form.id <= -3) {
                // plate mode box
                if (!translatedShape)
                    readPlateModeBox(is, form);

                writePlateBot(wdbp, form, translatedShape);
            }
            else if ((form.id == -1) || (form.id == 1)) {
                // pipe
                if (!translatedShape)
                    readPipe(is, form);

                writePipe(wdbp, form, translatedShape);
            }
            else if (form.id == 2) {
                // rectangular box
                if (!translatedShape)
                    readRectangularBox(is, form);

                writeRectangularBox(wdbp, form, translatedShape);
            }
            else if (form.id == 3) {
                // cone
                if (!translatedShape)
                    readCone(is, form);

                writeCone(wdbp, form, translatedShape);
            }
            else if (form.id == 5) {
                // cylinder
                if (!translatedShape)
                    readCylinder(is, form);

                writeCylinder(wdbp, form, translatedShape);
            }
            else if (form.id == 8) {
                // arb8
                if (!translatedShape)
                    readArb8(is, form);

                writeArb8(wdbp, form, translatedShape);
            }
            else {
                // long form
                if (!translatedShape)
                    readLongForm(is, form);

                writeSolidBot(wdbp, form, translatedShape);
            }

            is >> id;
        }
    }
}
