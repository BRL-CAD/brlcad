/*                      I D E N T S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
/** @file burst/idents.cpp
 *
 */

#include "common.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <regex>

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "vmath.h"

#include "./burst.h"

#define DEBUG_IDENTS 0

int
findIdents(int ident, struct bu_ptbl *idpl)
{
#if DEBUG_IDENTS
    bu_log("findIdents(%d)\n", ident);
#endif
    for (size_t i = 0; i < BU_PTBL_LEN(idpl); i++) {
	struct ids *idp = (struct ids *)BU_PTBL_GET(idpl, i);
#if DEBUG_IDENTS
	bu_log("lower=%d, upper=%d\n", (int)idp->i_lower, (int)idp->i_upper);
#endif
	if (ident >= (int) idp->i_lower && ident <= (int) idp->i_upper) {
	    return 1;
	}
    }
#if DEBUG_IDENTS
    bu_log("returned 0\n");
#endif
    return 0;
}

struct colors *
findColors(int ident, struct bu_ptbl *colp)
{
    for (size_t i = 0; i < BU_PTBL_LEN(colp); i++) {
	struct colors *c = (struct colors *)BU_PTBL_GET(colp, i);
	if (ident >= (int) c->c_lower && ident <= (int)c->c_upper) {
	    return c;
	}
    }
    return NULL;
}


/*
  Free up idents.
*/
void
freeIdents(struct bu_ptbl *idp)
{
    for (size_t i = 0; i < BU_PTBL_LEN(idp); i++) {
	struct ids *id = (struct ids *)BU_PTBL_GET(idp, i);
	BU_PUT(id, struct ids);
    }
    bu_ptbl_reset(idp);
}

int
readIdents(struct bu_ptbl *idlist, const char *fname)
{
    std::ifstream fs;
    fs.open(fname);
    if (!fs.is_open()) {
	return 0;
    }

    freeIdents(idlist); /* free old list if it exists */

    std::regex num_regex("^\\s*([0-9]+)");
    std::regex range_regex("\\s*([0-9]+)[-]*([0-9]+)");
    std::string iline;
    while (std::getline(fs, iline)) {
	if (!iline.length()) continue;
	int lower;
	int upper;
	std::smatch nvar;
	if (!std::regex_search(iline, nvar, num_regex)) {
	    bu_log("Invalid ident line: %s\n", iline.c_str());
	    continue;
	}
	std::smatch rvar;
	if (std::regex_search(iline, rvar, range_regex)) {
	    lower = std::stoi(rvar[1]);
	    upper = std::stoi(rvar[2]);
	} else {
	    lower = std::stoi(nvar[1]);
	    upper = lower;
	}
	struct ids *idp;
	BU_GET(idp, struct ids);
	idp->i_lower = lower;
	idp->i_upper = upper;
	bu_ptbl_ins(idlist, (long *)idp);
    }

    fs.close();

    return 1;
}

/*
  Free up colors.
*/
void
freeColors(struct bu_ptbl *idp)
{
    for (size_t i = 0; i < BU_PTBL_LEN(idp); i++) {
	struct colors *id = (struct colors *)BU_PTBL_GET(idp, i);
	BU_PUT(id, struct colors);
    }
    bu_ptbl_reset(idp);
}
int
readColors(struct bu_ptbl *idlist, const char *fname)
{
    std::ifstream fs;
    fs.open(fname);
    if (!fs.is_open()) {
	return 0;
    }

    freeColors(idlist); /* free old list if it exists */

    std::string iline;
    std::regex color_regex("([0-9]+)\\s+([0-9]+)\\s+([0-9]+)\\s+([0-9]+)\\s+([0-9]+).*");
    while (std::getline(fs, iline)) {
	std::smatch parsevar;
	if (!std::regex_search(iline, parsevar, color_regex)) {
	    bu_log("Invalid colors line: %s\n", iline.c_str());
	    continue;
	}
	if (parsevar.size() != 6) {
	    bu_log("readColors(): only %zd items read\n", parsevar.size());
	    continue;
	}

	struct colors *cdp;
	BU_GET(cdp, struct colors);
	cdp->c_lower = std::stoi(parsevar[1]);
	cdp->c_upper = std::stoi(parsevar[2]);
	cdp->c_rgb[0] = std::stoi(parsevar[3]);
	cdp->c_rgb[1] = std::stoi(parsevar[4]);
	cdp->c_rgb[2] = std::stoi(parsevar[5]);
	bu_ptbl_ins(idlist, (long *)cdp);
    }

    fs.close();

    return 1;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

