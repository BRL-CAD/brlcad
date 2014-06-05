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

#include "bu.h"

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
 * individual test must return CTEST_PASS/CTEST_FAIL using the same
 * convention OR invert its value. */
const int CTEST_PASS  = 0;
const int CTEST_FAIL  = 1;

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

typedef enum {
    HEX         = 0x0001,
    HEX_RAW     = 0x0011,
    BINARY      = 0x0100,
    BINARY_RAW  = 0x1100
} hex_bin_enum_t;


/**
 * Dump a bitv into a detailed bit format for debugging.
 */
BU_TESTS_EXPORT extern void dump_bitv(const struct bu_bitv *);


/**
 * Get a random number from system entropy (typically used for seeding
 * the 'random' function).
 */
BU_TESTS_EXPORT extern long int bu_get_urandom_number();


/**
 * Get a random string of hex or binary characters (possibly with a
 * leading '0x' or '0b').
 */
BU_TESTS_EXPORT extern void random_hex_or_binary_string(struct bu_vls *v, const hex_bin_enum_t typ, const int nbytes);

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

