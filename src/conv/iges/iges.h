/*                          I G E S . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2020 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#ifndef CONV_IGES_IGES_H
#define CONV_IGES_IGES_H

#define CSG_MODE 1
#define FACET_MODE 2
#define TRIMMED_SURF_MODE 3

#define NAMESIZE 16 /* from db.h */

struct iges_properties
{
    char name[NAMESIZE+1];
    char material_name[32];
    char material_params[60];
    char region_flag;
    short ident;
    short air_code;
    short material_code;
    short los_density;
    short inherit;
    short color_defined;
    unsigned char color[3];

};

#endif /* CONV_IGES_IGES_H */


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
