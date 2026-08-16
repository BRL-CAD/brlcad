/*                          N U M . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
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

#ifndef BU_NUM_H
#define BU_NUM_H

#include "common.h"

#include <stddef.h>

#include "bu/defines.h"

__BEGIN_DECLS

/**
 * Print numeric values as a table on standard output.
 *
 * Values are emitted left-to-right in rows of @p cols entries. Optional
 * strings control the table and row delimiters, number format, and column
 * separator. A NULL number format uses %.17g.
 */
BU_EXPORT extern void bu_num_print(const double *vals,
				   size_t nvals,
				   size_t cols,
				   const char *tbl_start,
				   const char *row_start,
				   const char *num_fmt,
				   const char *col_sep,
				   const char *row_end,
				   const char *tbl_end);

__END_DECLS

#endif /* BU_NUM_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
