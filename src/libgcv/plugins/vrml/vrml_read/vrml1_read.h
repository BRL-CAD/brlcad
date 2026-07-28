/*                    V R M L 1 _ R E A D . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef GCV_VRML1_READ_H
#define GCV_VRML1_READ_H

#include "common.h"

struct gcv_context;
struct gcv_opts;

int vrml1_read(struct gcv_context *context, const struct gcv_opts *gcv_options, const char *source_path);

#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
