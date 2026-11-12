/*               Q G P L U G I N V E R S I O N . H
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

#ifndef QGPLUGINVERSION_H
#define QGPLUGINVERSION_H

#define QGTCAD_PLUGIN_API_VERSION 1

#define QGTCAD_PLUGIN_STRINGIFY_(value) #value
#define QGTCAD_PLUGIN_STRINGIFY(value) QGTCAD_PLUGIN_STRINGIFY_(value)

#define QGTCAD_IID_PREFIX "org.brlcad.qtcad."
#define QGTCAD_IID(name) \
    QGTCAD_IID_PREFIX name "/" \
    QGTCAD_PLUGIN_STRINGIFY(QGTCAD_PLUGIN_API_VERSION)

#endif /* QGPLUGINVERSION_H */

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
