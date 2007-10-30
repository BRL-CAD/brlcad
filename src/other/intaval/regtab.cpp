/*                    R E G T A B . C P P
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
/** @file regtab.cpp
 *
 * table of regions
 *
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
   while (fscanf(fp, "%s%d%[^\n]", &name, &mat, &descr) != EOF)
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
      std::cout << "error: region " << key
                << " not in material list" << std::endl;
      exit(1);
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
