/*                      B I N U N I F . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2022 United States Government as represented by
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
/** @file rt/binunif.h
 *
 */

#ifndef RT_BINUNIF_H
#define RT_BINUNIF_H

#include "common.h"
#include "vmath.h"
#include "bu/vls.h"
#include "rt/defines.h"

__BEGIN_DECLS

/* defined in binary_obj.c */
RT_EXPORT extern int rt_mk_binunif(struct rt_wdb *wdbp,
				   const char *obj_name,
				   const char *file_name,
				   unsigned int minor_type,
				   size_t max_count);


/* defined in db5_bin.c */

/**
 * Free the storage associated with a binunif_internal object
 */
RT_EXPORT extern void rt_binunif_free(struct rt_binunif_internal *bip);

/**
 * Diagnostic routine
 */
RT_EXPORT extern void rt_binunif_dump(struct rt_binunif_internal *bip);


__END_DECLS

#endif /* RT_BINUNIF_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
