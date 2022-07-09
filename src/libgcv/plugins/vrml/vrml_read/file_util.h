/*                         F I L E _ U T I L . H
 * BRL-CAD
 *
 * Copyright (c) 2015-2022 United States Government as represented by
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
 *
 */
/** @file vrml/file_util.h
 *
 * Class definition for file utility
 *
 */

#ifndef FILE_UTIL_H
#define FILE_UTIL_H

#include "common.h"

#include <iostream>

#define FILEUTIL_TYPE_UNKNOWN    0
#define FILEUTIL_TYPE_VRML1      1
#define FILEUTIL_TYPE_VRML       2

using namespace std;

class FileUtil
{
private:
    char *filename;
    char *fileinput;

public:
    FileUtil(const char *filename);
    ~FileUtil();
    int getFileType();
    char *storeFileInput();
    char *getFileData();
    void freeFileInput();
};

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
