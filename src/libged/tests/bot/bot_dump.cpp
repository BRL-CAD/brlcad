/*                     B O T _ D U M P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2018-2025 United States Government as represented by
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
/** @file bot_dump.cpp
 *
 * Testing routines for bot dump command.
 */

#include "common.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <string>

#include "bu/app.h"
#include "bu/exit.h"
#include "ged.h"

bool
txt_same(const char *f1, const char *f2)
{
    if (!f1 || !f2)
	return false;

    // Open files
    std::string sf1(f1);
    std::string sf2(f2);
    std::ifstream s1(sf1);
    std::ifstream s2(sf2);
    if (!s1.is_open() || !s2.is_open())
	return false;

    // Check line by line
    std::regex version_regex(".*BRL-CAD[(][0-9.]+[)].*");
    std::string l1, l2;
    while (std::getline(s1, l1) && std::getline(s2, l2)) {
	// Skip lines with BRL-CAD version in them - differences are expected
	if (std::regex_match(l1, version_regex) && std::regex_match(l2, version_regex)) {
	    bu_log("Note - skipping lines with BRL-CAD version present:\n#1: %s\n#2: %s\n", l1.c_str(), l2.c_str());
	    continue;
	}
        if (l1 != l2)
            return false;
    }

    // Check if one file has more lines than the other
    if (std::getline(s1, l1) || std::getline(s2, l2))
        return false;

    // All checks passed - files are the same
    bu_log("%s and %s are the same\n", f1, f2);
    return true;
}

bool
bin_same(const char *f1, const char *f2)
{
    if (!f1 || !f2)
	return false;

    // Open files
    std::string sf1(f1);
    std::string sf2(f2);
    std::ifstream s1(sf1);
    std::ifstream s2(sf2);
    if (!s1.is_open() || !s2.is_open())
	return false;

    // Check pieces of the files against each other
    int bsize = 4096;
    char b1[4096];
    char b2[4096];

    while (true) {
	s1.read(b1, bsize);
	s2.read(b2, bsize);

	std::streamsize rcnt1 = s1.gcount();
	std::streamsize rcnt2 = s2.gcount();

	// Check for length difference
	if (rcnt1 != rcnt2)
	   return false;

	// If we didn't read anything we're done
	if (rcnt1 == 0)
	    break;

	// Compare the contents
	if (std::memcmp(b1, b2, rcnt1) != 0)
	    return false;
    }

    // All checks passed - files are the same
    bu_log("%s and %s are the same\n", f1, f2);
    return true;
}

int
main(int ac, char *av[])
{
    struct ged *gedp = NULL;
    const char *s_av[15] = {NULL};
    char input_file[MAXPATHLEN] = {0};
    const char *ofile = NULL;

    if (ac != 2)
	bu_exit(EXIT_FAILURE, "%s <directory>", av[0]);

    if (!bu_file_directory(av[1])) {
	printf("ERROR: [%s] is not a directory.  Expecting directory holding input .g files and control outputs\n", av[1]);
	return 2;
    }

    /* Start with minimal case - a facetized arb4 */

    bu_dir(input_file, MAXPATHLEN, av[1], "arb4.g", NULL);
    gedp = ged_open("db", input_file, 1);
    if (!gedp)
	bu_exit(EXIT_FAILURE, "Could not open %s", input_file);

    // DXF
    ofile = "arb4_out.dxf";
    s_av[0] = "bot";
    s_av[1] = "dump";
    s_av[2] = "-t";
    s_av[3] = "dxf";
    s_av[4] = "-o";
    s_av[5] = ofile;
    s_av[6] = "arb4.bot";
    ged_exec_bot(gedp, 7, s_av);

    if (bu_vls_strlen(gedp->ged_result_str))
	bu_log("%s\n", bu_vls_cstr(gedp->ged_result_str));
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_dir(input_file, MAXPATHLEN, av[1], "arb4.dxf", NULL);
    if (!txt_same(input_file, ofile))
	bu_exit(EXIT_FAILURE, "Difference found between %s and %s", input_file, ofile);
    std::remove(ofile);

    // OBJ
    ofile = "arb4_out.obj";
    s_av[0] = "bot";
    s_av[1] = "dump";
    s_av[2] = "-t";
    s_av[3] = "obj";
    s_av[4] = "-o";
    s_av[5] = ofile;
    s_av[6] = "arb4.bot";
    ged_exec_bot(gedp, 7, s_av);

    if (bu_vls_strlen(gedp->ged_result_str))
	bu_log("%s\n", bu_vls_cstr(gedp->ged_result_str));
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_dir(input_file, MAXPATHLEN, av[1], "arb4.obj", NULL);
    if (!txt_same(input_file, ofile))
	bu_exit(EXIT_FAILURE, "Difference found between %s and %s", input_file, ofile);
    std::remove(ofile);


    // OBJ - with normals
    ofile = "arb4_norm_out.obj";
    s_av[0] = "bot";
    s_av[1] = "dump";
    s_av[2] = "-t";
    s_av[3] = "obj";
    s_av[4] = "-n";
    s_av[5] = "-o";
    s_av[6] = ofile;
    s_av[7] = "arb4.bot";
    ged_exec_bot(gedp, 8, s_av);

    if (bu_vls_strlen(gedp->ged_result_str))
	bu_log("%s\n", bu_vls_cstr(gedp->ged_result_str));
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_dir(input_file, MAXPATHLEN, av[1], "arb4_norm.obj", NULL);
    if (!txt_same(input_file, ofile))
	bu_exit(EXIT_FAILURE, "Difference found between %s and %s", input_file, ofile);
    std::remove(ofile);


    // SAT
    ofile = "arb4_out.sat";
    s_av[0] = "bot";
    s_av[1] = "dump";
    s_av[2] = "-t";
    s_av[3] = "sat";
    s_av[4] = "-o";
    s_av[5] = ofile;
    s_av[6] = "arb4.bot";
    ged_exec_bot(gedp, 7, s_av);

    if (bu_vls_strlen(gedp->ged_result_str))
	bu_log("%s\n", bu_vls_cstr(gedp->ged_result_str));
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_dir(input_file, MAXPATHLEN, av[1], "arb4.sat", NULL);
    if (!txt_same(input_file, ofile))
	bu_exit(EXIT_FAILURE, "Difference found between %s and %s", input_file, ofile);
    std::remove(ofile);

    // STL - ASCII
    ofile = "arb4_out.stl";
    s_av[0] = "bot";
    s_av[1] = "dump";
    s_av[2] = "-t";
    s_av[3] = "stl";
    s_av[4] = "-o";
    s_av[5] = ofile;
    s_av[6] = "arb4.bot";
    ged_exec_bot(gedp, 7, s_av);

    if (bu_vls_strlen(gedp->ged_result_str))
	bu_log("%s\n", bu_vls_cstr(gedp->ged_result_str));
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_dir(input_file, MAXPATHLEN, av[1], "arb4.stl", NULL);
    if (!txt_same(input_file, ofile))
	bu_exit(EXIT_FAILURE, "Difference found between %s and %s", input_file, ofile);
    std::remove(ofile);

    // STL - units
    ofile = "arb4_units.stl";
    s_av[0] = "bot";
    s_av[1] = "dump";
    s_av[2] = "-t";
    s_av[3] = "stl";
    s_av[4] = "-u";
    s_av[5] = "10";
    s_av[6] = "-o";
    s_av[7] = ofile;
    s_av[8] = "arb4.bot";
    ged_exec_bot(gedp, 9, s_av);

    if (bu_vls_strlen(gedp->ged_result_str))
	bu_log("%s\n", bu_vls_cstr(gedp->ged_result_str));
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_dir(input_file, MAXPATHLEN, av[1], "arb4_units.stl", NULL);
    if (!txt_same(input_file, ofile))
	bu_exit(EXIT_FAILURE, "Difference found between %s and %s", input_file, ofile);
    std::remove(ofile);


    // STL - binary
    ofile = "arb4_binary_out.stl";
    s_av[0] = "bot";
    s_av[1] = "dump";
    s_av[2] = "-t";
    s_av[3] = "stl";
    s_av[4] = "-b";
    s_av[5] = "-o";
    s_av[6] = ofile;
    s_av[7] = "arb4.bot";
    ged_exec_bot(gedp, 8, s_av);

    if (bu_vls_strlen(gedp->ged_result_str))
	bu_log("%s\n", bu_vls_cstr(gedp->ged_result_str));
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_dir(input_file, MAXPATHLEN, av[1], "arb4_binary.stl", NULL);
    if (!txt_same(input_file, ofile))
	bu_exit(EXIT_FAILURE, "Difference found between %s and %s", input_file, ofile);
    std::remove(ofile);

    ged_close(gedp);

    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

