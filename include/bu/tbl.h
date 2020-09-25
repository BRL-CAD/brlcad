/*                           T B L . H
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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

#ifndef BU_TBL_H
#define BU_TBL_H

#include "common.h"

#include "bu/defines.h"
#include "bu/vls.h"


__BEGIN_DECLS

/**
 * @addtogroup bu_tbl
 *
 * @brief Routines for generally (i.e., non-mathematically) handling
 * numbers, strings, and other data for tabular printing.
 */
/** @{ */
/** @file bu/tbl.h */

/**
 * this is pulled from num.c where the guts to the table printer
 * currently resides, moved here for easy reference.
 *
 @code
 double vals[16] = {-1.123123123123, 0, 0, 1.0/0.0, 0, 123, 0, 0, 0, -2345, 123123.123123123123, 0, 123123123.123123123, 21, 1.0/0.0, 1};

 bu_num_print
 bu_num_print(vals, 16, 4, "my matrix\n[\n", "\t[", NULL, ", ", "]\n", "]\n]\n");
 -------------------------------------------------------------------------------
my matrix
[
	[-1.1231231231229999,     0,                  0, inf]
	[                  0,   123,                  0,   0]
	[                  0, -2345, 123123.12312312312,   0]
	[ 123123123.12312312,    21,                inf,   1]
]

 bu_num_print(vals+5, 3, 3, "POINT { ", NULL, NULL, " ", NULL, " }\n");
 ---------------------------------------------------------------------
POINT { 123 0 0 }

 bu_num_print(vals, 16, 4, "MATRIX [", " [", NULL, ", ", "]\n", "] ]\n");
 -----------------------------------------------------------------------
MATRIX [ [-1.1231231231229999,     0,                  0, inf]
         [                  0,   123,                  0,   0]
         [                  0, -2345, 123123.12312312312,   0]
         [ 123123123.12312312,    21,                inf,   1] ]

 bu_num_print(vals, 16, 4, NULL, NULL, NULL, " ", "    ", "\n");
 --------------------------------------------------------------
-1.1231231231229999 0 0 inf    0 123 0 0    0 -2345 123123.12312312312 0    123123123.12312312 21 inf 1

 bu_num_print(vals, 4, 4, NULL, NULL, NULL, NULL, NULL, NULL);
 ------------------------------------------------------------
-1.1231231231229999 0 0 inf

 bu_num_print(vals, 16, 4, NULL, NULL, "%017.2lf", " ", "\n", "\n");
 ------------------------------------------------------------------
-0000000000001.12 00000000000000.00 00000000000000.00               inf
00000000000000.00 00000000000123.00 00000000000000.00 00000000000000.00
00000000000000.00 -0000000002345.00 00000000123123.12 00000000000000.00
00000123123123.12 00000000000021.00               inf 00000000000001.00

 @endcode
 *
 */


struct bu_tbl_style {
    const char *before_tbl;
    const char *before_row;
    const char *before_col;
    const char *before_elm;
    const char *format_scan;
    const char *format_print;
    const char *after_elm;
    const char *after_col;
    const char *after_row;
    const char *after_tbl;
};


/**
 * set what to print at the beginning and end of the table.
 *
 * defaults: before="" after=""
 */
BU_EXPORT extern struct bu_tbl_style *
bu_tbl_style_tbl(struct bu_tbl_style *s, const char *before, const char *after);

/**
 * set what to print before and after each table row and how many rows
 * to display.
 *
 * defaults: before="" after="" maxrows=-1 (unlimited)
 */
BU_EXPORT extern struct bu_tbl_style *
bu_tbl_style_row(struct bu_tbl_style *s, const char *before, const char *after, size_t maxrows);

/**
 * set what to print before and after each table column and how many
 * columns to display.
 *
 * defaults: before="" after="" maxcols=1
 */
BU_EXPORT extern struct bu_tbl_style *
bu_tbl_style_col(struct bu_tbl_style *s, const char *before, const char *after, size_t maxcols);

/**
 * set what to print before and after each table element.
 *
 * defaults: before="" after=""
 */
BU_EXPORT extern struct bu_tbl_style *
bu_tbl_style_elm(struct bu_tbl_style *s, const char *before, const char *after);

/**
 * set scanf-style and printf-style format specifiers to use reading
 * and displaying table elements respectively
 *
 * defaults: scan="%lf" print="%.17g"
 */
BU_EXPORT extern struct bu_tbl_style *
bu_tbl_style_fmt(struct bu_tbl_style *s, const char *scan, const char *print);



/**
 *
 @code
struct bu_tbl_style s = BU_TBL_STYLE_INIT_ZERO;
bu_tbl_style_tbl(&s, "MATRIX [", "]\n");
bu_tbl_style_row(&s, "\t[", "]\n");
bu_tbl_print(s, vals, nvals, ncols);
 @endcode
 */


/**
 * format a set of data into a given vls string
 */
BU_EXPORT extern void
bu_tbl_vls(struct bu_vls *str, const struct bu_tbl_style *s, const double *vals, size_t nvals, size_t ncols);


/**
 * format a set of data into a given vls string
 */
BU_EXPORT extern void
bu_tbl_str(char *str, size_t maxlen, const struct bu_tbl_style *s, const double *vals, size_t nvals, size_t ncols);


/**
 * format a set of data and print them to the given file descriptor
 */
BU_EXPORT extern void
bu_tbl_print(int fd, const struct bu_tbl_style *s, const void *elms, size_t nelms, size_t selms);


__END_DECLS

/** @} */

#endif  /* BU_NUM_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
