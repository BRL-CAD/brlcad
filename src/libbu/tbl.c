/*                           T B L . C
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

#include "common.h"

/* interface header */
#include "bu/tbl.h"

/* private implementation headers */
#include <stdarg.h>
#include <string.h>
#include "bu/malloc.h"
#include "bu/exit.h"
#include "bu/assert.h"
#include "bu/vls.h"
#include "./fort.h"


struct bu_tbl {
    struct ft_table *t;
};


struct bu_tbl *
bu_tbl_create()
{
    struct bu_tbl *tbl;

    tbl = (struct bu_tbl *)bu_calloc(1, sizeof(struct bu_tbl), "bu_tbl alloc");
    tbl->t = ft_create_table();
    if (!tbl->t)
	bu_bomb("INTERNAL ERROR: unable to create a table\n");

    return tbl;
}


void
bu_tbl_destroy(struct bu_tbl *tbl)
{
    BU_ASSERT(tbl);
    BU_ASSERT(tbl->t);

    ft_destroy_table(tbl->t);
    tbl->t = NULL;
    bu_free(tbl, "bu_tbl free");

    return;
}


int
bu_tbl_clear(struct bu_tbl *tbl)
{
    BU_ASSERT(tbl);
    BU_ASSERT(tbl->t);

    return ft_erase_range(tbl->t, 0, 0, ft_row_count(tbl->t), ft_col_count(tbl->t));
}


struct bu_tbl *
bu_tbl_style(struct bu_tbl *tbl, enum bu_tbl_style style)
{
    BU_ASSERT(tbl);
    BU_ASSERT(tbl->t);

    switch (style) {
	case BU_TBL_STYLE_NONE:
	    ft_set_border_style(tbl->t, FT_EMPTY_STYLE);
	    break;
	case BU_TBL_STYLE_BASIC:
	    ft_set_border_style(tbl->t, FT_BASIC_STYLE);
	    break;
	case BU_TBL_STYLE_SIMPLE:
	    ft_set_border_style(tbl->t, FT_SIMPLE_STYLE);
	    break;
	case BU_TBL_STYLE_SINGLE:
	    ft_set_border_style(tbl->t, FT_SOLID_ROUND_STYLE);
	    break;
	case BU_TBL_STYLE_DOUBLE:
	    ft_set_border_style(tbl->t, FT_NICE_STYLE);
	    break;
	case BU_TBL_ROW_HEADER:
	    ft_set_cell_prop(tbl->t, FT_CUR_ROW, FT_ANY_COLUMN, FT_CPROP_ROW_TYPE, FT_ROW_HEADER);
	    break;
	case BU_TBL_ROW_SEPARATOR:
	    ft_add_separator(tbl->t);
	    break;
	case BU_TBL_ALIGN_LEFT:
	    ft_set_default_tbl_prop(FT_CPROP_TEXT_ALIGN, FT_ALIGNED_LEFT);
	    break;
	case BU_TBL_ALIGN_CENTER:
	    ft_set_default_tbl_prop(FT_CPROP_TEXT_ALIGN, FT_ALIGNED_CENTER);
	    break;
	case BU_TBL_ALIGN_RIGHT:
	    ft_set_default_tbl_prop(FT_CPROP_TEXT_ALIGN, FT_ALIGNED_RIGHT);
	    break;

	case BU_TBL_ROW_ALIGN_LEFT:
	    ft_set_cell_prop(tbl->t, FT_CUR_ROW, FT_ANY_COLUMN, FT_CPROP_TEXT_ALIGN, FT_ALIGNED_LEFT);
	    break;
	case BU_TBL_ROW_ALIGN_CENTER:
	    ft_set_cell_prop(tbl->t, FT_CUR_ROW, FT_ANY_COLUMN, FT_CPROP_TEXT_ALIGN, FT_ALIGNED_CENTER);
	    break;
	case BU_TBL_ROW_ALIGN_RIGHT:
	    ft_set_cell_prop(tbl->t, FT_CUR_ROW, FT_ANY_COLUMN, FT_CPROP_TEXT_ALIGN, FT_ALIGNED_RIGHT);
	    break;

	case BU_TBL_COL_ALIGN_LEFT:
	    ft_set_cell_prop(tbl->t, FT_ANY_ROW, FT_CUR_COLUMN, FT_CPROP_TEXT_ALIGN, FT_ALIGNED_LEFT);
	    break;
	case BU_TBL_COL_ALIGN_CENTER:
	    ft_set_cell_prop(tbl->t, FT_ANY_ROW, FT_CUR_COLUMN, FT_CPROP_TEXT_ALIGN, FT_ALIGNED_CENTER);
	    break;
	case BU_TBL_COL_ALIGN_RIGHT:
	    ft_set_cell_prop(tbl->t, FT_ANY_ROW, FT_CUR_COLUMN, FT_CPROP_TEXT_ALIGN, FT_ALIGNED_RIGHT);
	    break;

	case BU_TBL_ROW_END:
	    ft_ln(tbl->t);
	    break;
    }

    return tbl;
}


struct bu_tbl *
bu_tbl_go_to(struct bu_tbl *tbl, size_t row, size_t col)
{
    BU_ASSERT(tbl);
    BU_ASSERT(tbl->t);

    ft_set_cur_cell(tbl->t, row, col);

    return tbl;
}


struct bu_tbl *
bu_tbl_is_at(struct bu_tbl *tbl, size_t *row, size_t *col)
{
    BU_ASSERT(tbl);
    BU_ASSERT(tbl->t);

    if (row)
	*row = ft_cur_row(tbl->t);
    if (col)
	*col = ft_cur_col(tbl->t);

    return tbl;
}


struct bu_tbl *
bu_tbl_printf(struct bu_tbl *tbl, const char *fmt, ...)
{
    char *cstr;

    va_list ap;
#define BUFSZ 4096
    char buf[BUFSZ] = {0};

    if (!fmt)
	return tbl;

    BU_ASSERT(tbl);
    BU_ASSERT(tbl->t);

    va_start(ap, fmt);
    vsnprintf(buf, BUFSZ, fmt, ap);
    va_end(ap);

    cstr = strtok(buf, "|");
    while (cstr) {
	ft_printf(tbl->t, "%s", cstr);
	cstr = strtok(NULL, "|");
    }

    return tbl;
}


void
bu_tbl_vls(struct bu_vls *str, const struct bu_tbl *tbl)
{
    if (!tbl || !tbl->t)
	return;
    BU_ASSERT(str);

    const char *tstr = ft_to_string(tbl->t);
    if (tstr) {
	bu_vls_strcat(str, tstr);
    }
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
