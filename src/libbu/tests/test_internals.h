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

__BEGIN_DECLS
#ifndef BU_TESTS_EXPORT
#  if defined(BU_TESTS_DLL_EXPORTS) && defined(BU_TESTS_DLL_IMPORTS)
#    error "Only BU_TESTS_DLL_EXPORTS or BU_TESTS_DLL_IMPORTS can be defined, not both."
#  elif defined(BU_TESTS_DLL_EXPORTS)
#    define BU_TESTS_EXPORT __declspec(dllexport)
#  elif defined(BU_TESTS_DLL_IMPORTS)
#    define BU_TESTS_EXPORT __declspec(dllimport)
#  else
#    define BU_TESTS_EXPORT
#  endif
#endif


/* Define pass/fail per CMake/CTest testing convention; so any
 * individual test must return pass/fail using the same convention OR
 * invert its value. */
const int CTEST_PASS  = 0;
const int CTEST_FAIL  = 1;

const int FALSE = 0;
const int TRUE  = 1;

const unsigned BITS_PER_BYTE = 8;

BU_TESTS_EXPORT extern void dump_bitv(const struct bu_bitv *);

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

