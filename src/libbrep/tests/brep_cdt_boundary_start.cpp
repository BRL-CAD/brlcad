/*                    B R E P _ C D T _ B O U N D A R Y _ S T A R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include "bu/app.h"
#include "brep/cdt.h"

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;

    return cdt_test_boundary_start();
}
