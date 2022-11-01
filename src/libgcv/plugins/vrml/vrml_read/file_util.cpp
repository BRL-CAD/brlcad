/*			 F I L E _ U T I L . C P P
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
/** @file vrml/file_util.cpp
 *
 * Contain routines for file utility
 *
 */

#include "common.h"

#include "file_util.h"

#include <cstdio>
#include <cstring>
#include <iostream>

#ifndef HAVE_DECL_FSEEKO
#include "bio.h" /* for b_off_t */
extern "C" int fseeko(FILE *, b_off_t, int);
extern "C" b_off_t ftello(FILE *);
#endif
#include <fstream>

#include "bu.h"

using namespace std;
//Constructor with filename a parameter
FileUtil::FileUtil(const char *fname)
{
    filename = bu_strdup(fname);
}

FileUtil::~FileUtil()
{
    bu_free(filename, "filename");
}

//checks file type to note the format being processed and returns an int corresponding to file type
int
FileUtil::getFileType()
{
    unsigned char format[10];

    FILE *fp = fopen(filename, "rb");

    if (!fp)
	return FILE_TYPE_UNKNOWN; //return with unknown type if it could not open file
    if (fread(format, sizeof(unsigned char), 10, fp) != 10) {
	fclose(fp);
	return FILE_TYPE_UNKNOWN;
    }

    fclose(fp);

    int fileType = FILE_TYPE_UNKNOWN;

    //compares file formate with known formats to check for vrml version 1 or 2
    if (bu_strncmp((char *)format, "#VRML V2.0", 10) == 0) {
	    fileType = FILE_TYPE_VRML;  //vrml version 2
    }
    if (bu_strncmp((char *)format, "#VRML V1.0", 10) == 0) {
	    fileType = FILE_TYPE_VRML1; //vrml version 1
    }

    return fileType;
}

//Stores the file input in a char *
char *
FileUtil::storeFileInput()
{
    int size, i;
    ifstream infile(filename, ios::in);

    infile.seekg(0, ios::end);
    size = infile.tellg();  //Get file size
    infile.seekg(0, ios::beg);

    fileinput = new char[(2*size) + 1];

    for (i = 0; i < size; i++) {
	fileinput[i] = infile.get();
    }
    fileinput[i] = '\0';

    infile.close();

    return fileinput;
}

char *
FileUtil::getFileData()
{
    return this->fileinput;
}

void
FileUtil::freeFileInput()
{
    delete [] fileinput;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
