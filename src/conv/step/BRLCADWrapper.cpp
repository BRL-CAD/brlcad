/*                 BRLCADWrapper.cpp
 * BRL-CAD
 *
 * Copyright (c) 1994-2014 United States Government as represented by
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
/** @file step/BRLCADWrapper.cpp
 *
 * C++ wrapper to BRL-CAD database functions.
 *
 */

#include "common.h"
#include <stdlib.h>

/* interface header */
#include "./BRLCADWrapper.h"

/* system headers */
#include <sstream>
#include <iostream>


int BRLCADWrapper::sol_reg_cnt = 0;


BRLCADWrapper::BRLCADWrapper()
    : outfp(NULL), dbip(NULL)
{
}


BRLCADWrapper::~BRLCADWrapper()
{
    Close();
}

bool
BRLCADWrapper::load(std::string &flnm)
{

    /* open brlcad instance */
    if ((dbip = db_open(flnm.c_str(), DB_OPEN_READONLY)) == DBI_NULL) {
	bu_log("Cannot open input file (%s)\n", flnm.c_str());
	return false;
    }
    if (db_dirbuild(dbip)) {
	bu_log("ERROR: db_dirbuild failed: (%s)\n", flnm.c_str());
	return false;
    }

    return true;
}


bool
BRLCADWrapper::OpenFile(std::string &flnm)
{
    //TODO: need to check to make sure we aren't overwriting

    /* open brlcad instance */
    if ((outfp = wdb_fopen(flnm.c_str())) == NULL) {
	bu_log("Cannot open output file (%s)\n", flnm.c_str());
	return false;
    }

    // hold on to output filename
    filename = flnm.c_str();

    mk_id(outfp, "Output from STEP converter step-g.");

    return true;
}


bool
BRLCADWrapper::WriteHeader()
{
    db5_update_attribute("_GLOBAL", "HEADERINFO", "test header attributes", outfp->dbip);
    db5_update_attribute("_GLOBAL", "HEADERCLASS", "test header classification", outfp->dbip);
    db5_update_attribute("_GLOBAL", "HEADERAPPROVED", "test header approval", outfp->dbip);
    return true;
}


bool
BRLCADWrapper::WriteSphere(double *center, double radius)
{
    point_t pnt;
    center[X] = 0.0;
    center[Y] = 0.0;
    center[Z] = 0.0;
    VMOVE(pnt, center);
    mk_sph(outfp, "s1", pnt, radius);
    return true;
}


/* This simple routine will replace diacritic characters(code >= 192) from the extended
 * ASCII set with a specific mapping from the standard ASCII set. This code was copied
 * and modified from a solution provided on stackoverflow.com at:
 *     (http://stackoverflow.com/questions/14094621/)
 */
std::string
BRLCADWrapper::ReplaceAccented( std::string &str ) {
    std::string retStr = "";
    const char *p = str.c_str();
    while ( (*p)!=0 ) {
        const char*
        //   "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖ×ØÙÚÛÜÝÞßàáâãäåæçèéêëìíîïðñòóôõö÷øùúûüýþÿ"
        tr = "AAAAAAECEEEEIIIIDNOOOOOx0UUUUYPsaaaaaaeceeeeiiiiOnooooo/0uuuuypy";
        unsigned char ch = (*p);
        if ( ch >=192 ) {
            retStr += tr[ ch-192 ];
        } else {
            retStr += *p;
        }
        ++p;
    }
    return retStr;
}


/*
 * Simplifying names for better behavior under our Tcl based tools. This routine
 * replaces spaces and non-alphanumeric characters with underscores. It also replaces
 * ASCII extended characters representing diacritics (code >= 192)  with specific
 * mapped ASCII characters below ASCII code 128.
 */
std::string
BRLCADWrapper::CleanBRLCADName(std::string &inname)
{
    std::string retStr = "";
    std::string name = ReplaceAccented(inname);
    char *cp;

    for (cp = (char *)name.c_str(); *cp != '\0'; ++cp) {
	if (*cp == '\'') {
	    // remove non-printable
	    continue;
	}
	if (*cp == ' ') {
	    // replace spaces with underscores
	    retStr += '_';
	} else {
	    if (!isalpha(*cp) && !isdigit(*cp)) {
		// replace non-alphanumeric characters with underscore
		retStr += '_';
	    } else {
		retStr += *cp;
	    }
	}
    }

    return retStr;
}


std::string
BRLCADWrapper::GetBRLCADName(std::string &name)
{
    struct bu_vls obj_name = BU_VLS_INIT_ZERO;
    int len = 0;
    char *cp,*tp;
    static int start = 1;

    for (cp = (char *)name.c_str(), len = 0; *cp != '\0'; ++cp, ++len) {
	if (*cp == '@') {
	    if (*(cp + 1) == '@')
		++cp;
	    else
		break;
	}
	if (*cp == '\'') {
	    // remove single quotes
	    continue;
	}
	if (*cp == ' ') {
	    // simply replace spaces with underscores
	    bu_vls_putc(&obj_name, '_');
	} else {
	    bu_vls_putc(&obj_name, *cp);
	}
    }
    bu_vls_putc(&obj_name, '\0');

    tp = (char *)((*cp == '\0') ? "" : cp + 1);

    do {
	bu_vls_trunc(&obj_name, len);
	bu_vls_printf(&obj_name, "%d", start++);
	bu_vls_strcat(&obj_name, tp);
    }
    while (db_lookup(outfp->dbip, bu_vls_addr(&obj_name), LOOKUP_QUIET) != RT_DIR_NULL);

    return bu_vls_addr(&obj_name);
}

static MAP_OF_BU_LIST_HEADS heads;
bool
BRLCADWrapper::AddMember(const std::string &combname,const std::string &member,mat_t mat)
{
    MAP_OF_BU_LIST_HEADS::iterator i = heads.find(combname);
    if (i != heads.end()) {
	struct bu_list *head = (*i).second;
	if (mk_addmember(member.c_str(), head, mat, WMOP_UNION) == WMEMBER_NULL)
	    return false;
    } else {
	struct bu_list *head = NULL;

	BU_ALLOC(head, struct bu_list);

	BU_LIST_INIT(head);
	if (mk_addmember(member.c_str(), head, mat, WMOP_UNION) == WMEMBER_NULL)
	    return false;
	heads[combname] = head;
    }

    return true;
}

bool
BRLCADWrapper::WriteCombs()
{
    MAP_OF_BU_LIST_HEADS::iterator i = heads.begin();
    while (i != heads.end()) {
	std::string combname = (*i).first;
	struct bu_list *head = (*i++).second;
	unsigned char rgb[] = {200, 180, 180};

	(void)mk_comb(outfp, combname.c_str(), head, 0, "plastic", "", rgb, 0, 0, 0, 0, 0, 0, 0);

	mk_freemembers(head);

	BU_FREE(head, struct bu_list);

    }
    heads.clear();
    return true;
}


void
BRLCADWrapper::getRandomColor(unsigned char *rgb)
{
    /* golden ratio */
    static fastf_t hsv[3] = { 0.0, 0.5, 0.95 };
    static double golden_ratio_conjugate = 0.618033988749895;
    static fastf_t h = drand48();

    h = fmod(h+golden_ratio_conjugate,1.0);
    *hsv = h * 360.0;
    bu_hsv_to_rgb(hsv,rgb);
}


bool
BRLCADWrapper::WriteBrep(std::string name, ON_Brep *brep, mat_t &mat)
{
    std::string sol = name + ".s";
    std::string reg = name;

    mk_brep(outfp, sol.c_str(), brep);
    unsigned char rgb[] = {200, 180, 180};

    BRLCADWrapper::getRandomColor(rgb);

    struct bu_list head;
    BU_LIST_INIT(&head);
    if (mk_addmember(sol.c_str(), &head, mat, WMOP_UNION) == WMEMBER_NULL)
	return false;

    if (mk_comb(outfp, reg.c_str(), &head, 1, "plastic", "", rgb, 0, 0, 0, 0, 0, 0, 0) > 0)
	return true;

    return false;
}

struct db_i *
BRLCADWrapper::GetDBIP()
{
    return dbip;
}


bool
BRLCADWrapper::Close()
{

    if (outfp) {
	wdb_close(outfp);
	outfp = NULL;
    }
    if (dbip) {
	db_close(dbip);
	dbip = NULL;
    }

    return true;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
