/*                   D B _ I N T E R N A L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2015 United States Government as represented by
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
/** @file db_internal.h
 *
 */

#ifndef RT_DB_INTERNAL_H
#define RT_DB_INTERNAL_H

#include "common.h"
#include "bu/magic.h"
#include "bu/avs.h"

__BEGIN_DECLS

struct rt_functab; /* forward declaration */

/**
 * A handle on the internal format of a BRL-CAD database object.
 */
struct rt_db_internal {
    uint32_t            idb_magic;
    int                 idb_major_type;
    int                 idb_minor_type; /**< @brief ID_xxx */
    const struct rt_functab *idb_meth;  /**< @brief for ft_ifree(), etc. */
    void *              idb_ptr;
    struct bu_attribute_value_set idb_avs;
};
#define idb_type                idb_minor_type
#define RT_DB_INTERNAL_INIT(_p) { \
        (_p)->idb_magic = RT_DB_INTERNAL_MAGIC; \
        (_p)->idb_major_type = -1; \
        (_p)->idb_minor_type = -1; \
        (_p)->idb_meth = (const struct rt_functab *) ((void *)0); \
        (_p)->idb_ptr = ((void *)0); \
        bu_avs_init_empty(&(_p)->idb_avs); \
    }
#define RT_CK_DB_INTERNAL(_p) BU_CKMAG(_p, RT_DB_INTERNAL_MAGIC, "rt_db_internal")


__END_DECLS

#endif /* RT_DB_INTERNAL_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
