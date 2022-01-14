/*                   D B _ I N T E R N A L . H
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
/** @file db_internal.h
 *
 */

#ifndef RT_DB_INTERNAL_H
#define RT_DB_INTERNAL_H

#include "common.h"

/* system headers */
#include <stdio.h> /* for FILE */

/* interface headers */
#include "bu/magic.h"
#include "bu/avs.h"
#include "bn/mat.h"
#include "rt/defines.h"
#include "rt/resource.h"

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
#define RT_DB_INTERNAL_INIT_ZERO {RT_DB_INTERNAL_MAGIC, -1, -1, NULL, NULL, BU_AVS_INIT_ZERO}
#define RT_CK_DB_INTERNAL(_p) BU_CKMAG(_p, RT_DB_INTERNAL_MAGIC, "rt_db_internal")

/**
 * Get an object from the database, and convert it into its internal
 * (i.e., unserialized in-memory) representation.  Applies the
 * provided matrix transform only to the in-memory internal being
 * returned.
 *
 * Returns -
 * <0 On error
 * id On success.
 */
RT_EXPORT extern int rt_db_get_internal(struct rt_db_internal   *ip,
					const struct directory  *dp,
					const struct db_i       *dbip,
					const mat_t             mat,
					struct resource         *resp);

/**
 * Convert the internal representation of a solid to the external one,
 * and write it into the database.  On success only, the internal
 * representation is freed.
 *
 * Returns -
 * <0 error
 * 0 success
 */
RT_EXPORT extern int rt_db_put_internal(struct directory        *dp,
					struct db_i             *dbip,
					struct rt_db_internal   *ip,
					struct resource         *resp);

/**
 * Put an object in internal format out onto a file in external
 * format.  Used by LIBWDB.
 *
 * Can't really require a dbip parameter, as many callers won't have
 * one.
 *
 * THIS ROUTINE ONLY SUPPORTS WRITING V4 GEOMETRY.
 *
 * Returns -
 * 0 OK
 * <0 error
 */
RT_EXPORT extern int rt_fwrite_internal(FILE *fp,
					const char *name,
					const struct rt_db_internal *ip,
					double conv2mm);
RT_EXPORT extern void rt_db_free_internal(struct rt_db_internal *ip);

/**
 * Convert an object name to a rt_db_internal pointer
 *
 * Looks up the named object in the directory of the specified model,
 * obtaining a directory pointer.  Then gets that object from the
 * database and constructs its internal representation.  Returns
 * ID_NULL on error, otherwise returns the type of the object.
 */
RT_EXPORT extern int rt_db_lookup_internal(struct db_i *dbip,
					   const char *obj_name,
					   struct directory **dpp,
					   struct rt_db_internal *ip,
					   int noisy,
					   struct resource *resp);


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
