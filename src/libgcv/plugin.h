/*                        P L U G I N . H
 * BRL-CAD
 *
 * Copyright (c) 2015 United States Government as represented by
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
/** @file plugin.h
 *
 * Registration of GCV conversion plugins.
 *
 */


#ifndef PLUGIN_H
#define PLUGIN_H


#include "common.h"

#include "gcv.h"


__BEGIN_DECLS


struct gcv_conversion_configuration {
    fastf_t tolerance;
};


typedef int (*gcv_importer)(const char *path, struct rt_wdb *wdbp,
			    const struct gcv_conversion_configuration *config);
typedef int (*gcv_exporter)(const char *path, const struct db_i *dbip,
			    const struct gcv_conversion_configuration *config);


void gcv_register_plugin(const char *file_extensions, gcv_importer importer,
			 gcv_exporter exporter);


struct gcv_plugin_info {
    char *file_extensions;
    gcv_importer importer;
    gcv_exporter exporter;
};


__END_DECLS


#endif


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
