/*                    D I R _ M A C O S X . M
 * BRL-CAD
 *
 * Copyright (c) 2018 United States Government as represented by
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
/** @file dir_macosx.m
 *
 * Mac-specific ObjC implementation of path-finding functions.
 *
 */

#include "common.h"

#include <Foundation/Foundation.h>

#include "bu/str.h"


static void
path_lookup(char *path, size_t len, NSSearchPathDirectory dir)
{
    NSURL *result;
    result = [[[NSFileManager defaultManager] URLsForDirectory:dir inDomains:NSUserDomainMask] objectAtIndex:0];
    if (result)
	bu_strlcat(path, [[result path] UTF8String], len);
}



void
dir_cache_macosx(char *path, size_t len)
{
    path_lookup(path, len, NSCachesDirectory);
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
