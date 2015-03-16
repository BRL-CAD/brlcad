/*                       S E T U P . C P P
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
/** @file setup.cpp
 *
 * Registration of libgcv static plugins.
 *
 */


#include "common.h"

#include "gcv_private.h"


#define TOKENPASTE(prefix, suffix) prefix ## suffix
#define TOKENPASTE2(prefix, suffix) TOKENPASTE(prefix, suffix)
#define PLUGIN(name) \
    extern "C" gcv_plugin_info name; \
    static const RegisterPlugin TOKENPASTE2(plugin_, __LINE__)((name))


class RegisterPlugin
{
public:
    RegisterPlugin(const gcv_plugin_info &info);
};


RegisterPlugin::RegisterPlugin(const gcv_plugin_info &info)
{
    gcv_plugin_register(&info);
}


// gcv static plugins

PLUGIN(gcv_plugin_conv_brlcad);
PLUGIN(gcv_plugin_conv_fastgen4_read);
PLUGIN(gcv_plugin_conv_fastgen4_write);
PLUGIN(gcv_plugin_conv_obj_read);
PLUGIN(gcv_plugin_conv_obj_write);
PLUGIN(gcv_plugin_conv_stl_read);
PLUGIN(gcv_plugin_conv_stl_write);
PLUGIN(gcv_plugin_conv_vrml_write);


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
