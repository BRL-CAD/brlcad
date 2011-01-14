/*                    R E G T A B . C P P
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
/** @file regtab.cpp
 *
 * INTAVAL Target Geometry File to BRL-CAD converter:
 * table of regions implementation
 *
 *  Origin -
 *	TNO (Netherlands)
 *	IABG mbH (Germany)
 */

#include <map>
#include <iostream>

#include "regtab.h"
#include "glob.h"



static std::map<std::string, Region*> regionTable;


int readMaterials(FILE *fp)
{
   char title[LINELEN];
   char name[20];
   int mat;

   if (fgets(title, LINELEN, fp) == NULL)
      return 1;

   char descr[LINELEN];
   while (fscanf(fp, "%s%d%[^\n]", name, &mat, descr) != EOF)
   {
      if (name[0] == '-')
         continue;

      Region *regionp;

      regionp = new Region(atoi(name), descr);
      regionp->addMaterial(mat);

      std::string key = name;
      regionTable[key] = regionp;

      fgetc(fp);     // newline lezen
   }
   return 0;
}


static Region *getRegion(int compnr)
{
   char    name[20];
   Region* regionp = 0;

   sprintf(name, "%d", compnr);

   std::string key = name;

   if (regionTable.find(key) == regionTable.end()) {
      std::cout << "WARNING: region " << key
                << " not in material list" << std::endl;
      char unknown[] = "UNKNOWN";
      if (regionTable.find(unknown) == regionTable.end()) {
	regionp = new Region(atoi(unknown), unknown);
	regionp->addMaterial(0);
	key = unknown;
	regionTable[key] = regionp;
      }
      else
	regionp = regionTable[unknown];
   }
   else
      regionp = regionTable[key];

   return regionp;
}


void addToRegion
(
    int   compnr,
    char* name
) {
    getRegion(compnr)->add(name);
}


void excludeFromRegion
(
    int   compnr,
    char* name
) {
    getRegion(compnr)->exclude(name);
}


void createRegions(struct rt_wdb* wdbp)
{
    struct wmember tophead;
    BU_LIST_INIT(&tophead.l);

    for(std::map<std::string, Region*>::iterator it = regionTable.begin();
        it != regionTable.end();
        ++it) {
        Region* regionp = it->second;

        std::cout << regionp->getDescription() << std::endl;

        if ((regionp->getMaterial() != 0) && (regionp->nonEmpty()))
            regionp->push(wdbp, &tophead);
        else
            std::cout << "Empty region: " << regionp->getCompNr() << (regionp->referred() ? " (referred)" : " (unreferred)") << std::endl;
    }

    mk_lfcomb(wdbp, "g_all", &tophead, 0);

    mk_freemembers(&tophead.l);
}
