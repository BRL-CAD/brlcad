/*                    W R I T E _ B R L . H
 *
 * Copyright (c) 2007 TNO (Netherlands) and IABG mbH (Germany).
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
 *
 *
 *                   DISCLAIMER OF LIABILITY
 *
 * (Replacement of points 15 and 16 of GNU Lesser General Public
 *       License in countries where they are not effective.)
 *
 * This program was carefully compiled and tested by the authors and
 * is made available to you free-of-charge in a high-grade Actual
 * Status. Reference is made herewith to the fact that it is not
 * possible to develop software programs so that they are error-free
 * for all application conditions. As the program is licensed free of
 * charge there is no warranty for the program, insofar as such
 * warranty is not a mandatory requirement of the applicable law,
 * such as in case of wilful acts. in ADDITION THERE IS NO LIABILITY
 * ALSO insofar as such liability is not a mandatory requirement of
 * the applicable law, such as in case of wilful acts.
 *
 * You declare you are in agreement with the use of the software under
 * the above-listed usage conditions and the exclusion of a guarantee
 * and of liability. Should individual provisions in these conditions
 * be in full or in part void, invalid or contestable, the validity of
 * all other provisions or agreements are unaffected by this. Further
 * the parties undertake at this juncture to agree a legally valid
 * replacement clause which most closely approximates the economic
 * objectives.
 *
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
