/*                       D M A P P . C P P
 * BRL-DM
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
/** @file cadapp.cxx
 *
 * Application level data and functionality implementations.
 *
 */

#include "dmapp.h"

int
DMApp::load_g(const char *filename, int argc, const char *argv[])
{
    if (argc < 0 || !argv)
	return -1;

    gedp = ged_open("db", filename, 1);
    if (!gedp) {
	w.statusBar()->showMessage("open failed");
	return -1;
    }

    // Let the GED commands know what bview to use
    gedp->ged_gvp = w.canvas->v;

    return 0;
}


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

