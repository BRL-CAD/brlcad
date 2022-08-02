/*		     S T R I N G _ U T I L . H
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
/** @file vrml/string_util.h
 *
 * Contains function declaration string utility functions
 *
 */

#ifndef STRING_UTIL_H
#define STRING_UTIL_H

#include "common.h"

#include <iostream>
#include <vector>
#include <string>

#define KWDEF          0
#define KWUSE          1
#define KWPROTO        2
#define MAXSTRSIZE     512

using namespace std;

char *nextWord(char *inputstring, char *nextwd);
bool findKeyWord(char *inputstring, int kw);
void stringcopy(string &str1, char *str2);
int stringcompare(string &str1, char *str2);
char *getNextWord(char *instring, char *nextword);
char *getNextWord(char *nextword);
void replaceStringChars(string &str, char ch,const char *rstring);
void formatString(char *instring);
int findFieldName(char *instring);
void getSFVec4f(float *p);
void getInt(int &n);
void getFloat(float &n);
void getCoordIndex(vector<int> &ccoordindex);
void getPoint(vector<float> &cpoint);

#endif


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
