/*                           T B L . H
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
 * Example 1:
 @code
my matrix
[
        [ -1.1231231231229999,     0,                  0, inf ]
        [                   0,   123,                  0,   0 ]
        [                   0, -2345, 123123.12312312312,   0 ]
        [  123123123.12312312,    21,                inf,   1 ]
]
 @endcode
 *
 * generated from this code:
 *
 @code
 double vals[16] = {-1.123123123123, 0, 0, 1.0/0.0, 0, 123, 0, 0, 0, -2345, 123123.123123123123, 0, 123123123.123123123, 21, 1.0/0.0, 1};

 struct bu_tbl *t = bu_tbl_create();

 bu_tbl_style(t, BU_TBL_STYLE_NONE);
 bu_tbl_printf(t, "my matrix");
 bu_tbl_style(t, BU_TBL_ROW_END);
 bu_tbl_style(t, BU_TBL_ROW_ALIGN_LEFT);
 bu_tbl_printf(t, "[");
 bu_tbl_style(t, BU_TBL_ROW_END);
 bu_tbl_style(t, BU_TBL_ROW_ALIGN_RIGHT);
 bu_tbl_printf(t, "[|%.17g,|%.17g,|%.17g,|%.17g|]", vals[0], vals[1], vals[2], vals[3]);
 bu_tbl_style(t, BU_TBL_ROW_END);
 bu_tbl_style(t, BU_TBL_ROW_ALIGN_RIGHT);
 bu_tbl_printf(t, "[|%.17g,|%.17g,|%.17g,|%.17g|]", vals[4], vals[5], vals[6], vals[7]);
 bu_tbl_style(t, BU_TBL_ROW_END);
 bu_tbl_style(t, BU_TBL_ROW_ALIGN_RIGHT);
 bu_tbl_printf(t, "[|%.17g,|%.17g,|%.17g,|%.17g|]", vals[8], vals[9], vals[10], vals[11]);
 bu_tbl_style(t, BU_TBL_ROW_END);
 bu_tbl_style(t, BU_TBL_ROW_ALIGN_RIGHT);
 bu_tbl_printf(t, "[|%.17g,|%.17g,|%.17g,|%.17g|]", vals[12], vals[13], vals[14], vals[15]);
 bu_tbl_style(t, BU_TBL_ROW_END);
 bu_tbl_style(t, BU_TBL_ROW_ALIGN_LEFT);
 bu_tbl_printf(t, "]");

 bu_tbl_destroy(t);
 @endcode
 *
 * Example 2:
 @code
-0000000000001.12 00000000000000.00 00000000000000.00               inf
00000000000000.00 00000000000123.00 00000000000000.00 00000000000000.00
00000000000000.00 -0000000002345.00 00000000123123.12 00000000000000.00
00000123123123.12 00000000000021.00               inf 00000000000001.00
 @endcode
 *
 *
 *
 @code
 double vals[16] = {-1.123123123123, 0, 0, 1.0/0.0, 0, 123, 0, 0, 0, -2345, 123123.123123123123, 0, 123123123.123123123, 21, 1.0/0.0, 1};

 struct bu_tbl *t = bu_tbl_create();

 bu_tbl_style(t, BU_TBL_STYLE_LIST);
 bu_tbl_printf(t, "%017.2lf|%017.2lf|%017.2lf|%017.2lf", vals[0], vals[1], vals[2], vals[3]);
 bu_tbl_style(t, BU_TBL_ROW_END);
 bu_tbl_printf(t, "%017.2lf|%017.2lf|%017.2lf|%017.2lf", vals[4], vals[5], vals[6], vals[7]);
 bu_tbl_style(t, BU_TBL_ROW_END);
 bu_tbl_printf(t, "%017.2lf|%017.2lf|%017.2lf|%017.2lf", vals[8], vals[9], vals[10], vals[11]);
 bu_tbl_style(t, BU_TBL_ROW_END);
 bu_tbl_printf(t, "%017.2lf|%017.2lf|%017.2lf|%017.2lf", vals[12], vals[13], vals[14], vals[15]);

 bu_tbl_destroy(t);
 @endcode
 *
 */


struct bu_tbl;


/**
 * static-initializer for bu_tbl objects on the stack
 */
#define BU_TBL_INIT_ZERO {0}

/**
 * always returns a pointer to a newly allocated table
 */
BU_EXPORT extern struct bu_tbl *
bu_tbl_create();


/**
 * releases all dynamic memory associated with the specified table
 */
BU_EXPORT extern void
bu_tbl_destroy(struct bu_tbl *);


/**
 * erases all cells in a table, but doesn't erase the table itself
 */
BU_EXPORT extern int
bu_tbl_clear(struct bu_tbl *);


/**
 * Table styles:
 *
 *  NONE 1  ------ ---  +-------+---+  ╭────────┬───╮  ╔════════╦═══╗
 *  2    3   LIST   1   | BASIC | 1 |  │ SINGLE │ 1 │  ║ DOUBLE ║ 1 ║
 *  4    5  ------ ---  +-------+---+  ├────────┼───┤  ╠════════╬═══╣
 *           2      3   | 2     | 3 |  │ 2      │ 3 │  ║ 2      ║ 3 ║
 *           4      5   | 4     | 5 |  │ 4      │ 5 │  ║ 4      ║ 5 ║
 *                      +-------+---+  ╰────────┴───╯  ╚════════╩═══╝
 */
enum bu_tbl_style {
    /* table border style */
    BU_TBL_STYLE_NONE,
    BU_TBL_STYLE_LIST,
    BU_TBL_STYLE_BASIC,
    BU_TBL_STYLE_SINGLE,
    BU_TBL_STYLE_DOUBLE,

    /* table alignment */
    BU_TBL_ALIGN_LEFT,
    BU_TBL_ALIGN_CENTER,
    BU_TBL_ALIGN_RIGHT,

    /* cell styling */
    BU_TBL_ROW_HEADER,

    /* insert a horizontal separator */
    BU_TBL_ROW_SEPARATOR,

    /* cell alignment */
    BU_TBL_ROW_ALIGN_LEFT,
    BU_TBL_ROW_ALIGN_CENTER,
    BU_TBL_ROW_ALIGN_RIGHT,
    BU_TBL_COL_ALIGN_LEFT,
    BU_TBL_COL_ALIGN_CENTER,
    BU_TBL_COL_ALIGN_RIGHT,

    /* go to next row beginning */
    BU_TBL_ROW_END
};


/**
 * sets table styling or formatting on cells printed next.
 */
BU_EXPORT extern struct bu_tbl *
bu_tbl_style(struct bu_tbl *, enum bu_tbl_style);


/**
 * get cell position for the current table insertion point.
 */
BU_EXPORT extern struct bu_tbl *
bu_tbl_is_at(struct bu_tbl *, size_t *row, size_t *col);


/**
 * set cell position of the current table insertion point.
 */
BU_EXPORT extern struct bu_tbl *
bu_tbl_go_to(struct bu_tbl *, size_t row, size_t col);


/**
 * print values into the table at the current insertion point.
 *
 * each column of the 'fmt' printf-style format specfier must be
 * delimited by a '|' character.
 *
 * any existing values will be overwritten.
 */
BU_EXPORT extern struct bu_tbl *
bu_tbl_printf(struct bu_tbl *, const char *fmt, ...);


/**
 * print a table into a vls
 */
BU_EXPORT extern void
bu_tbl_vls(struct bu_vls *str, const struct bu_tbl *t);


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
