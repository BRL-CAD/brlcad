/*                      R E G T A B . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file regtab.h
 *
 * INTAVAL Target Geometry File to BRL-CAD converter:
 * table of regions declarations
 *
 *  Origin -
 *	TNO (Netherlands)
 *	IABG mbH (Germany)
 */

#ifndef REGTAB_INCLUDED
#define REGTAB_INCLUDED

#include <string>

#include "common.h"

#include "vmath.h"
#include "wdb.h"


class Region;


int readMaterials
(
    FILE *fp
);


void addToRegion
(
    int compnr,
    char* name
);


void excludeFromRegion
(
    int compnr,
    char* name
);


void createRegions
(
    struct rt_wdb *wdbp
);


class Region {
public:
    Region() : addCreated(false), excludeCreated(false) {
	BU_LIST_INIT(&head.l)
    };

    Region(int   nr,
	   char* description) : compnr(nr), desc(description), addCreated(false), excludeCreated(false) {
	BU_LIST_INIT(&head.l);
    }

    ~Region() {
	mk_freemembers(&head.l);
    }

    int        getCompNr(void) {
	return compnr;
    }

    void        addMaterial(int mat) {
	material = mat;
    }

    int         getMaterial(void) {
	return material;
    }

    std::string getDescription(void) {
	return desc;
    }

    wmember*    getHead(void) {
	return &head;
    }

    void        push(rt_wdb*  wdbp,
		     wmember* tophead) {
	if (addCreated) {
	    char mName[20];
	    sprintf(mName, "m_%d", compnr);

	    mk_lfcomb(wdbp, mName, &addHead, 0);
	    mk_addmember(mName, &(head.l), NULL, WMOP_UNION);
	}

	if (excludeCreated) {
	    char sName[20];
	    sprintf(sName, "s_%d", compnr);

	    mk_lfcomb(wdbp, sName, &excludeHead, 0);
	    mk_addmember(sName, &(head.l), NULL, WMOP_SUBTRACT);
	}

	char name[20];
	sprintf(name, "r_%d", compnr);

	mk_lrcomb(wdbp,
		  name,  // name of the db element created
		  &head, // list of elements in the region
		  1,     // 1 = region
		  "plastic",
		  "sh=4 sp=0.5 di=0.5 re=0.1",
		  (unsigned char *)0,
		  1000,
		  0,
		  material,
		  100,
		  0);

	mk_addmember(name, &(tophead->l), NULL, WMOP_UNION);
    }

    bool        nonEmpty(void) {
	return addCreated;
    }

    bool        referred(void) {
	return excludeCreated;
    }

    void        add(char* name) {
	if (!addCreated) {
	    BU_LIST_INIT(&(addHead.l))
	    addCreated = true;
	}

	mk_addmember(name, &(addHead.l), NULL, WMOP_UNION);
    }

    void        exclude(char* name) {
	if (!excludeCreated) {
	    BU_LIST_INIT(&(excludeHead.l))
	    excludeCreated = true;
	}

	mk_addmember(name, &(excludeHead.l), NULL, WMOP_UNION);
    }

private:
    int         compnr;
    wmember     head;
    int         material;
    std::string desc;

    bool        addCreated;
    wmember     addHead;
    bool        excludeCreated;
    wmember     excludeHead;
};


#endif // REGTAB_INCLUDED
