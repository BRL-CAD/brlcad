/*                        G - A D R T . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2009-2009 United States Government as represented by
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

/** @file g-adrt.c
 *
 * lame wrapper around the stuff in load_MySQL.c
 */

#include <stdio.h>

int adrt_mysql_shunt(int argc, char **argv);

int main(int argc, char **argv)
{
    if(argc <= 4) {
	printf("Usage: g-adrt [-r region.map] file.g mysql_hostname name_for_geometry [region list]\n");
	return 1;
    }

    return adrt_mysql_shunt(argc, argv);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
