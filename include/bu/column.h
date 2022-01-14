/*                         C O L U M N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

#ifndef BU_COLUMN_H
#define BU_COLUMN_H

#include "common.h"
#include "bu/defines.h"
#include "bu/vls.h"

__BEGIN_DECLS

/** @addtogroup bu_vls
 *
 * @brief
 * Given a series of input strings and formatting parameters,
 * construct an output version of an input string with contents arranged into
 * multiple columns.
 *
 */
/** @{ */
/** @file bu/column.h */

struct bu_column_state_internal;

struct bu_column_state {
    /* User settings */
    int terminal_width;     /** default is 80 */
    int fill_order;         /** 0 is column first, 1 is row first */
    int column_cnt;         /** normally calculated but can be user specified */
    const char *delimiter;  /** string between columns - defaults to a single space */
    const char *row_prefix; /** string before first column in row - defaults to empty */
    const char *row_suffix; /** string after first column in row - defaults to empty */
    struct bu_column_state_internal *i;
};

#define BU_COLUMN_ALL -1

/**
 * Generate an output string based on the specified layout and contents in the
 * bu_column_state s.  Return the number of items printed, or -1 if there is
 * an error.
 *
 * Any uninitialized layout state will be calculated according to layout logic
 * derived from BSD UNIX's column and ls commands.  The following defaults are
 * assumed or calculated, but pre-existing non-empty default values will be
 * respected if set.  Note that if any of the parameters in bu_column_state are
 * changed, the caller must re-run bu_column_compute to re-generate the new
 * layout.  If column count and width are unset, they will be determined at
 * print time.
 *
 * Terminal width:   80
 * Column count:     input dependent
 * Column widths:    input dependent
 * Column alignment: left aligned
 * Column delimiter: space
 * Row prefix:       none
 * Row suffix:       none
 * Fill order:       column first
 *
 *
 */
BU_EXPORT extern int bu_column_print(struct bu_vls *o, struct bu_column_state *s);

/**
 * Add a string to the layout. Return codes:
 *
 * -1 could not add successfully, str is not in layout
 *  0 added
 *  n number of characters that could not be added.  If column count and/or
 *    width are set to fixed values that prevent the full contents of str from
 *    being added, the return code tells the caller how many characters had to
 *    be truncated to make it fit in the layout.  This will not happen with
 *    unset column count and width, since the layout will be finalized based on
 *    the inputs at print time.
 */
BU_EXPORT extern int bu_column_add(struct bu_column_state *s, const char *str);

/* Note - column numbering is 0 - n-1 where n is s->column_cnt To set all
 * alignments simultaneously, pass BU_COLUMN_ALL to colnum.  If a column number
 * is specified that does not correspond to a column present in the layout, or
 * an unknown alignment is specified, -1 will be returned. */
#define BU_COLUMN_ALIGN_LEFT 0
#define BU_COLUMN_ALIGN_RIGHT 1
#define BU_COLUMN_ALIGN_CENTER_LEFT_BIAS 2
#define BU_COLUMN_ALIGN_CENTER_RIGHT_BIAS 3
BU_EXPORT extern int bu_column_set_alignment(struct bu_column_state *s, int colnum, int alignment);

/* Note - column numbering is 0 - n-1 where n is s->column_cnt To set all
 * widths simultaneously, pass BU_COLUMN_ALL to colnum.  If a column number
 * is specified that does not correspond to a column present in the layout, -1
 * will be returned. If a width is specified that is larger than 1/2 the
 * terminal width, it will be clamped to the nearest value <= 1/2 the terminal
 * width and a -2 error will be returned. A value of <=0 for width is considered
 * unset, and the width will be calculated at print time in that case.*/
BU_EXPORT extern int bu_column_set_width(struct bu_column_state *s, int colnum, int width);

/* Clear all contents in the state, but do not reset the layout parameters */
BU_EXPORT extern int bu_column_clear(struct bu_column_state *s);

/* Reset the layout parameters, but do not clear the contents. */
BU_EXPORT extern int bu_column_reset(struct bu_column_state *s);


__END_DECLS

/** @} */

#endif  /* BU_COLUMN_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
