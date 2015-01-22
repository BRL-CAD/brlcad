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


typedef int (*gcv_reader_fn)(const char *path, struct rt_wdb *wdbp,
			     const struct gcv_opts *options);
typedef int (*gcv_writer_fn)(const char *path, struct db_i *dbip,
			     const struct gcv_opts *options);


#define GCV_VERSION (unsigned char)1


struct gcv_plugin_info {
    unsigned char gcv_version;
    char *file_extensions;
    gcv_reader_fn reader_fn;
    gcv_writer_fn writer_fn;
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
