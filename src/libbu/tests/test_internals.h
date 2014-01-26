/*                  T E S T _ I N T E R N A L S . H
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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


#ifndef LIBBU_TESTS_TEST_INTERNALS_H
#define LIBBU_TESTS_TEST_INTERNALS_H

/* Define pass/fail per CMake/CTest testing convention; so any
 * individual test must return pass/fail using the same convention OR
 * invert its value. */
const int PASS  = 0;
const int FAIL  = 1;

const int FALSE = 0;
const int TRUE  = 1;

#endif /* LIBBU_TESTS_TEST_INTERNALS_H */


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

