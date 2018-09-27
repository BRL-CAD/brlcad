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
 * Implementation of path-finding functions for general app use.
 *
 */

#include "common.h"

#include <AppKit/AppKit.h>

#include "bu/str.h"


void
dir_cache_macosx(char *path, size_t len)
{
    NSAutoreleasePool *pool;
    NSArray *paths;
    NSString *cache;

    pool = [[NSAutoreleasePool alloc] init];
    paths = NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES);
    cache = [paths objectAtIndex:0];

    bu_strlcat(path, [cache UTF8String], len);

    [pool drain];
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
