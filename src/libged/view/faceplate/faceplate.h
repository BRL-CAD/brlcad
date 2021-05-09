/*                    F A C E P L A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file faceplate.h
 *
 * Private header for libged faceplate subcmds.
 *
 */

#ifndef GED_FACEPLATE_H
#define GED_FACEPLATE_H

#include "common.h"

#include "ged.h"

__BEGIN_DECLS
GED_EXPORT extern int _fp_cmd_model_axes(void *bs, int argc, const char **argv);
GED_EXPORT extern int _fp_cmd_view_axes(void *bs, int argc, const char **argv);
GED_EXPORT extern int _fp_cmd_grid(void *bs, int argc, const char **argv);
GED_EXPORT extern int _fp_cmd_irect(void *bs, int argc, const char **argv);
__END_DECLS

#endif /* GED_FACEPLATE_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
