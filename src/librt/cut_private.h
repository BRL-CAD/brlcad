/*                   C U T _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file librt/cut_private.h
 *
 * Private support for librt spatial partitioning method implementations.
 */

#ifndef LIBRT_CUT_PRIVATE_H
#define LIBRT_CUT_PRIVATE_H

#include "common.h"

#include "librt_private.h"

__BEGIN_DECLS

const char *rt_cut_method_name(int method);
void rt_cut_select_from_env(struct rt_i *rtip);

void rt_cut_nubsp_build(struct rt_i *rtip, const union cutter *root, int ncpu);
void rt_cut_null_build(struct rt_i *rtip, const union cutter *root, int ncpu);

union cutter *rt_ct_get(struct rt_i *rtip);
void rt_ct_free(struct rt_i *rtip, union cutter *cutp);
void rt_ct_release_storage(union cutter *cutp);
size_t rt_ct_piececount(const union cutter *cutp);

__END_DECLS

#endif /* LIBRT_CUT_PRIVATE_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
