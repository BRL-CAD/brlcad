/*                    S T R I N G _ U T I L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2015-2021 United States Government as represented by
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
/** @file vrml/string_util.cpp
 *
 * Implementation for string utility functions
 *
 */

#include "common.h"
#include <iostream>
#include <cstdio>
#include <string.h>
#include <cstdlib>

#include "bu.h"

#include "string_util.h"

using namespace std;

static char *ptr = NULL;   //pointer to current position in input file data
static const char *keyword[]= { "DEF", "USE", "PROTO"};

//Parses data from input one word at a time
char *
nextWord(char *inputstring, char *nextwd)
{
    if ((*inputstring) == '\0' ) {
	return inputstring;
    }

    char *strptr; //pointer to string
    char tempchar[MAXSTRSIZE] = "";
    strptr = inputstring;

    while (true) {
	switch (*strptr) {
	    case ' ':
	    case '\n':
	    case '\r':
	    case '\t':
	    case ',':
		strptr++;
		break;
	    //found comment so read to end of line
	    case '#':
		do {
		    strptr++;
		} while ((*strptr) != '\n' && (*strptr) != '\r' && (*strptr) != '\0');
		break;
	    default:
		if ((*strptr) == '\0' ) {
		    return strptr;
		}

		int count = 0;
		while (((*strptr) != ' ') && ((*strptr) != '\0') && ((*strptr) != '\n') && ((*strptr) != '\t')&& ((*strptr) != '\r')) {
		    tempchar[count] = (*strptr);
		    strptr++;
		    count++;
		}

		tempchar[count] = '\0';
		bu_strlcpy(nextwd, tempchar, count+1);
		return strptr;
	}
    }
}


bool
findKeyWord(char *inputstring, int kw)
{
    if (bu_strcmp(inputstring, keyword[kw]) == 0) {
	return 1;
    } else {
	return 0;
    }

}

void
stringcopy(string &str1, char *str2)
{
    int i;

    for (i = 0; str2[i] != '\0'; i++) {
	str1.push_back(str2[i]);
    }
    str1[i] = '\0';
}

int
stringcompare(string &str1, char *str2)
{
    return bu_strcmp(str1.c_str(), str2);
}

char *
getNextWord(char *instring, char *nextword)
{
    if (ptr == NULL) {
	ptr = nextWord(instring, nextword);
    }else {
	ptr = nextWord(ptr, nextword);
    }

    if (*ptr == '\0') {
	return 0;
    }else
	return ptr;
}

char *
getNextWord(char *nextword)
{
    ptr = nextWord(ptr, nextword);

    if (*ptr == '\0')
	return 0;
    else
	return ptr;

}

void
replaceStringChars(string &str, char ch, const char *rstring)
{
    int pos = str.find(ch);
    int n = static_cast<int>(string::npos);

    while (pos != n) {
	str.replace(pos, 1, rstring, 0, 3);
	pos = str.find(ch, pos + 2);
    }
}

void
formatString(char *instring)
{
    string str;
    stringcopy(str, instring);

    replaceStringChars(str, '#', "\n# ");
    replaceStringChars(str, '{', "\n{\n");
    replaceStringChars(str, '}', "\n}\n");
    replaceStringChars(str, '[', "\n[\n");
    replaceStringChars(str, ']', "\n]\n");
    replaceStringChars(str, ',', " , ");

    bu_strlcpy(instring, str.c_str(), str.size()+1);
}

void
getSFVec3f(float *p)
{
    int i;
    char val[MAXSTRSIZE];
    char *ch;

    for (i = 0; i < 3; i++) {
	ch = getNextWord(val);
	p[i] = atof(val);
	nextWord(ch, val);

	if (*val == '}') {
	    break;
	}
    }
}

void
getSFVec4f(float *p)
{
    int i;
    char val[MAXSTRSIZE];

    for (i = 0; i < 4; i++) {
	getNextWord(val);
	p[i] = atof(val);
    }
}

void
getInt(int &n)
{
    char val[MAXSTRSIZE];

    getNextWord(val);
    n = atoi(val);
}

void
getFloat(float &n)
{
    char val[MAXSTRSIZE];

    getNextWord(val);
    n = atof(val);
}

void
getCoordIndex(vector<int> &ccoordindex)
{
    int n;
    char val[MAXSTRSIZE];

    getNextWord(val);

    while (getNextWord(val)) {
	if (*val == ']') {
	    break;
	}

	if (*val == ',' || (bu_strcmp("-1", val) == 0)) {
	    continue;
	}
	n = atoi(val);
	ccoordindex.push_back(n);
    }
}

void
getPoint(vector<float> &cpoint)
{
    float n;
    char val[MAXSTRSIZE];

    getNextWord(val);

    while (getNextWord(val)) {
	if (*val == ']') {
	    break;
	}

	if (*val == ',') {
	    continue;
	}
	n = atof(val);
	cpoint.push_back(n);
    }
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
