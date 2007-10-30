/*                      R E G T A B . H
 *
 * Copyright (c) 2007 TNO (Netherlands) and IABG mbH (Germany).
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
 *
 *                   DISCLAIMER OF LIABILITY
 *
 * (Replacement of points 15 and 16 of GNU Lesser General Public
 *       License in countries where they are not effective.)
 *
 * This program was carefully compiled and tested by the authors and
 * is made available to you free-of-charge in a high-grade Actual
 * Status. Reference is made herewith to the fact that it is not
 * possible to develop software programs so that they are error-free
 * for all application conditions. As the program is licensed free of
 * charge there is no warranty for the program, insofar as such
 * warranty is not a mandatory requirement of the applicable law,
 * such as in case of wilful acts. in ADDITION THERE IS NO LIABILITY
 * ALSO insofar as such liability is not a mandatory requirement of
 * the applicable law, such as in case of wilful acts.
 *
 * You declare you are in agreement with the use of the software under
 * the above-listed usage conditions and the exclusion of a guarantee
 * and of liability. Should individual provisions in these conditions
 * be in full or in part void, invalid or contestable, the validity of
 * all other provisions or agreements are unaffected by this. Further
 * the parties undertake at this juncture to agree a legally valid
 * replacement clause which most closely approximates the economic
 * objectives.
 *
 */

#ifndef REGTAB_INCLUDED
#define REGTAB_INCLUDED

#include <string>

#include "common.h"
#include "machine.h"
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
           char* description) : compnr(nr), addCreated(false), excludeCreated(false) {
        desc = description;
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
