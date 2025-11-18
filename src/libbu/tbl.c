/*                           T B L . C
 * BRL-CAD
 *
 * Copyright (c) 2020-2025 United States Government as represented by
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
#include <stdint.h>
#include "bu/malloc.h"
#include "bu/exit.h"
#include "bu/assert.h"
#include "bu/vls.h"
#include "./fort.h"

#ifdef FT_HAVE_UTF8
/* =============================================================================
 * UTF-8 DISPLAY WIDTH SUPPORT
 *
 * We provide a UTF-8 aware width calculator and register it with libfort so the
 * table engine measures display columns rather than raw bytes. This fixes
 * alignment when cells contain multibyte characters, combining marks, or
 * East Asian wide characters.
 *
 * The implementation below:
 *   - Decodes UTF-8 to Unicode code points
 *   - Treats common combining mark ranges as width 0
 *   - Treats common East Asian wide ranges as width 2
 *   - Treats most printable characters as width 1
 *
 * This avoids changing the global locale and works consistently cross-platform.
 * =============================================================================
 */
static int
bu__uc_is_combining(uint32_t cp)
{
    /* Common combining mark ranges (not exhaustive, but covers typical use):
     *   0300–036F   Combining Diacritical Marks
     *   1AB0–1AFF   Combining Diacritical Marks Extended
     *   1DC0–1DFF   Combining Diacritical Marks Supplement
     *   20D0–20FF   Combining Diacritical Marks for Symbols
     *   FE20–FE2F   Combining Half Marks
     */
    return
	(cp >= 0x0300 && cp <= 0x036F) ||
	(cp >= 0x1AB0 && cp <= 0x1AFF) ||
	(cp >= 0x1DC0 && cp <= 0x1DFF) ||
	(cp >= 0x20D0 && cp <= 0x20FF) ||
	(cp >= 0xFE20 && cp <= 0xFE2F);
}

static int
bu__uc_is_east_asian_wide(uint32_t cp)
{
    /* Widely used ranges for width=2 (subset of Unicode EAW Wide/Fullwidth):
     * 1100–115F, 2329–232A, 2E80–303E, 3040–A4CF,
     * AC00–D7A3, F900–FAFF, FE10–FE19, FE30–FE6F,
     * FF00–FF60, FFE0–FFE6
     */
    return
	(cp >= 0x1100 && cp <= 0x115F) ||
	(cp == 0x2329 || cp == 0x232A) ||
	(cp >= 0x2E80 && cp <= 0x303E) ||
	(cp >= 0x3040 && cp <= 0xA4CF) ||
	(cp >= 0xAC00 && cp <= 0xD7A3) ||
	(cp >= 0xF900 && cp <= 0xFAFF) ||
	(cp >= 0xFE10 && cp <= 0xFE19) ||
	(cp >= 0xFE30 && cp <= 0xFE6F) ||
	(cp >= 0xFF00 && cp <= 0xFF60) ||
	(cp >= 0xFFE0 && cp <= 0xFFE6);
}

/* Decode next UTF-8 code point.
 * Returns number of bytes consumed (>=1). On invalid sequences, consumes 1 byte
 * and returns that raw byte as code point for forward progress. Returns 0 only
 * if n == 0.
 */
static size_t
bu__u8_next(const char *s, size_t n, uint32_t *out_cp)
{
    if (!s || n == 0) return 0;

    const unsigned char *p = (const unsigned char *)s;
    unsigned char c0 = p[0];

    if (c0 < 0x80) {
	*out_cp = c0;
	return 1;
    }

    /* Determine sequence length */
    size_t len = 0;
    uint32_t cp = 0;

    if ((c0 & 0xE0) == 0xC0) { /* 2-byte */
	len = 2;
	cp = c0 & 0x1F;
    } else if ((c0 & 0xF0) == 0xE0) { /* 3-byte */
	len = 3;
	cp = c0 & 0x0F;
    } else if ((c0 & 0xF8) == 0xF0) { /* 4-byte */
	len = 4;
	cp = c0 & 0x07;
    } else {
	/* Invalid leading byte */
	*out_cp = c0;
	return 1;
    }

    if (len > n) {
	/* Truncated sequence: treat first byte as standalone */
	*out_cp = c0;
	return 1;
    }

    for (size_t i = 1; i < len; i++) {
	unsigned char cx = p[i];
	if ((cx & 0xC0) != 0x80) {
	    /* Invalid continuation: consume first byte */
	    *out_cp = c0;
	    return 1;
	}
	cp = (cp << 6) | (cx & 0x3F);
    }

    /* Overlong / invalid range checks (conservative) */
    if ((len == 2 && cp < 0x80) ||
	(len == 3 && cp < 0x800) ||
	(len == 4 && cp < 0x10000) ||
	cp > 0x10FFFF ||
	(cp >= 0xD800 && cp <= 0xDFFF)) {
	*out_cp = c0;
	return 1;
    }

    *out_cp = cp;
    return len;
}

static int
bu__uc_display_width(uint32_t cp)
{
    /* C0/C1 control chars, DEL -> width 0 */
    if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0))
	return 0;

    if (bu__uc_is_combining(cp))
	return 0;

    if (bu__uc_is_east_asian_wide(cp))
	return 2;

    /* Default printable char width */
    return 1;
}

/* libfort callback: compute visible width of a UTF-8 string between [beg, end). */
static int bu__fort_u8width_cb(const void *beg, const void *end, size_t *width)
{
    if (!width) return -1;
    const char *s = (const char *)beg;
    const char *e = (const char *)end;
    if (!s || !e || e < s) { *width = 0; return -1; }

    size_t off = 0;
    size_t n = (size_t)(e - s);
    size_t w = 0;

    while (off < n) {
        uint32_t cp = 0;
        size_t step = bu__u8_next(s + off, n - off, &cp);
        if (step == 0) break;
        w += (size_t)bu__uc_display_width(cp);
        off += step;
    }

    *width = w;
    return 0;
}
#endif

struct bu_tbl {
    struct ft_table *t;
};


struct bu_tbl *
bu_tbl_create(void)
{
    struct bu_tbl *tbl = NULL;

    tbl = (struct bu_tbl *)bu_calloc(1, sizeof(struct bu_tbl), "bu_tbl alloc");
    tbl->t = ft_create_table();
    if (!tbl->t)
	bu_bomb("INTERNAL ERROR: unable to create a table\n");

#ifdef FT_HAVE_UTF8
    /* Register UTF-8 display width function globally for libfort */
    ft_set_u8strwid_func(bu__fort_u8width_cb);
#endif

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
	    ft_set_cell_prop(tbl->t, FT_ANY_ROW, FT_ANY_COLUMN, FT_CPROP_LEFT_PADDING, 0);
	    ft_set_cell_prop(tbl->t, FT_ANY_ROW, FT_ANY_COLUMN, FT_CPROP_RIGHT_PADDING, 1);
	    break;
	case BU_TBL_STYLE_LIST:
	    ft_set_border_style(tbl->t, FT_SIMPLE_STYLE);
	    ft_set_cell_prop(tbl->t, FT_ANY_ROW, FT_ANY_COLUMN, FT_CPROP_LEFT_PADDING, 0);
	    ft_set_cell_prop(tbl->t, FT_ANY_ROW, FT_ANY_COLUMN, FT_CPROP_RIGHT_PADDING, 0);
	    break;
	case BU_TBL_STYLE_BASIC:
	    ft_set_border_style(tbl->t, FT_BASIC_STYLE);
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
	    ft_set_default_cell_prop(FT_CPROP_TEXT_ALIGN, FT_ALIGNED_LEFT);
	    break;
	case BU_TBL_ALIGN_CENTER:
	    ft_set_default_cell_prop(FT_CPROP_TEXT_ALIGN, FT_ALIGNED_CENTER);
	    break;
	case BU_TBL_ALIGN_RIGHT:
	    ft_set_default_cell_prop(FT_CPROP_TEXT_ALIGN, FT_ALIGNED_RIGHT);
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
    char *cstr = NULL;

    va_list ap;
#define BUFSZ 4096
    char buf[BUFSZ];
    char *back = NULL;
    char *last = NULL;
    size_t zeros = 0;

    if (!fmt)
	return tbl;

    BU_ASSERT(tbl);
    BU_ASSERT(tbl->t);

    memset(buf, 255, BUFSZ);

    va_start(ap, fmt);
    vsnprintf(buf, BUFSZ, fmt, ap);
    va_end(ap);

#ifdef FT_HAVE_UTF8
#define TBL_WRITE(table, s) ft_u8nwrite((table)->t, 1, (const void *)(s))
#else
#define TBL_WRITE(table, s) ft_nwrite((table)->t, 1, (const char *)(s))
#endif

    cstr = strtok(buf, "|");
    if (cstr) {
	/* strtok collapses empty tokens, so check */
	back = cstr;
	zeros = 0;
	back--;
	while ((*back == '\0' || *back == '|') && buf <= back) {
	    zeros++;
	    back--;
	}
	while (zeros--) {
	    TBL_WRITE(tbl, "");
	}
	TBL_WRITE(tbl, cstr);
	last = cstr;
    }

    while (cstr) {
	cstr = strtok(NULL, "|");

	if (cstr) {
	    /* strtok collapses empty tokens, so check */
	    back = cstr;
	    zeros = 0;
	    back -= 2;
	    while ((*back == '\0' || *back == '|') && buf <= back) {
		zeros++;
		back--;
	    }
	    while (zeros--) {
		TBL_WRITE(tbl, "");
	    }

	    TBL_WRITE(tbl, cstr);
	    last = cstr;
	}
    }

    if (!last)
	return tbl;

    zeros = 0;
    last += 2;

    if (!last)
	return tbl;

    while ((*last == '\0' || *last == '|')) {
	zeros++;
	last++;
    }
    while (zeros--) {
	TBL_WRITE(tbl, "");
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
