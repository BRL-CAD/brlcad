/*                    T G F - G . C P P
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
/** @file tgf-g.cpp
 *
 * main function
 *
 */

#include <iostream>
#include <fstream>

#include "regtab.h"
#include "read_dra.h"


int main
(
    int   argc,
    char* argv[]
) {
    int ret = 0;

    if (argc < 4) {
        std::cout << "Usage: " << argv[0] << " <DRA-mat> <DRA-geo> <BRL-g>" << std::endl;
        ret = 1;
    }
    else {
        FILE* in = fopen(argv[1], "r");

        if (in == 0) {
            std::cout << "Error reading DRA-mat file" << std::endl;
            ret = 1;
        }
        else {
            ret = readMaterials(in);
            fclose(in);

            if (ret == 0) {
                std::ifstream is(argv[2]);

                if (!is.is_open()) {
                    std::cout << "Error reading DRA-geo file" << std::endl;
                    ret = 1;
                }
                else {
                    struct rt_wdb* wdbp = wdb_fopen(argv[3]); // force create

                    conv(is, wdbp);
                    createRegions(wdbp);
                    wdb_close(wdbp);
                }
            }
        }
    }

    return ret;
}
