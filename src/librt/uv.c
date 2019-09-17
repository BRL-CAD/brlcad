/*                            U V . C
 * BRL-CAD
 *
 * Copyright (c) 2019 United States Government as represented by
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
/** @file uv.c
 *
 * Implementation of UV texture mapping support.
 *
 */

#include "common.h"

#include "rt/db_internal.h"
#include "rt/db_io.h"
#include "rt/nongeom.h"
#include "rt/uv.h"


int
rt_texture_load(struct rt_texture *texture, const char *name, struct db_i *dbip)
{
    struct directory *dirEntry;
    size_t size = 1234; /* !!! fixme */

    RT_CK_DBI(dbip);

    if (!name || !texture)
	return 0;

    bu_log("Loading texture %s [%s]...", texture->tx_datasrc==TXT_SRC_AUTO?"from auto-determined datasource":texture->tx_datasrc==TXT_SRC_OBJECT?"from a database object":texture->tx_datasrc==TXT_SRC_FILE?"from a file":"from an unknown source (ERROR)", bu_vls_addr(&texture->tx_name));

    /* if the source is auto or object, we try to load the object */
    if ((texture->tx_datasrc==TXT_SRC_AUTO) || (texture->tx_datasrc==TXT_SRC_OBJECT)) {

        /* see if the object exists */
        if ((dirEntry=db_lookup(dbip, bu_vls_addr(&texture->tx_name), LOOKUP_QUIET)) == RT_DIR_NULL) {

            /* unable to find the texture object */
            if (texture->tx_datasrc!=TXT_SRC_AUTO) {
                return -1;
            }
        } else {
            struct rt_db_internal *dbip2;

            BU_ALLOC(dbip2, struct rt_db_internal);

            RT_DB_INTERNAL_INIT(dbip2);
            RT_CK_DB_INTERNAL(dbip2);
            RT_CK_DIR(dirEntry);

            /* the object was in the directory, so go get it */
            if (rt_db_get_internal(dbip2, dirEntry, dbip, NULL, NULL) <= 0) {
                /* unable to load/create the texture database record object */
                return -1;
            }

            RT_CK_DB_INTERNAL(dbip2);
            RT_CK_BINUNIF(dbip2->idb_ptr);

            /* keep the binary object pointer */
            texture->tx_binunifp = (struct rt_binunif_internal *)dbip2->idb_ptr; /* make it so */

            /* release the database instance pointer struct we created */
            RT_DB_INTERNAL_INIT(dbip2);
            bu_free(dbip2, "txt_load_datasource");

            /* check size of object */
            if (texture->tx_binunifp->count < size) {
                bu_log("\nWARNING: %s needs %zu bytes, binary object only has %zu\n", bu_vls_addr(&texture->tx_name), size, texture->tx_binunifp->count);
            } else if (texture->tx_binunifp->count > size) {
                bu_log("\nWARNING: Binary object is larger than specified texture size\n"
                       "\tBinary Object: %zu pixels\n\tSpecified Texture Size: %zu pixels\n"
                       "...continuing to load using image subsection...",
                       texture->tx_binunifp->count, size);
            }
        }
    }

    /* if we are auto and we couldn't find a database object match, or
     * if source is explicitly a file then we load the file.
     */
    if (((texture->tx_datasrc==TXT_SRC_AUTO) && (texture->tx_binunifp==NULL)) || (texture->tx_datasrc==TXT_SRC_FILE)) {

        texture->tx_mp = bu_open_mapped_file_with_path(dbip->dbi_filepath,        bu_vls_addr(&texture->tx_name), NULL);

        if (texture->tx_mp==NULL)
            return -1; /* FAIL */

	if (texture->tx_mp->buflen < size) {
            bu_log("\nWARNING: %s needs %zu bytes, file only has %zu\n", bu_vls_addr(&texture->tx_name), size, texture->tx_mp->buflen);
        } else if (texture->tx_mp->buflen > size) {
            bu_log("\nWARNING: Texture file size is larger than specified texture size\n"
                   "\tInput File: %zu pixels\n\tSpecified Texture Size: %zu pixels\n"
                   "...continuing to load using image subsection...",
                   texture->tx_mp->buflen, size);
        }
    }

    bu_log("done.\n");

    return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
