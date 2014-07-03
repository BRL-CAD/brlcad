/*                     R E G I O N L I S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2012-2014 United States Government as represented by
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
/** @file RegionList.cpp
 *
 * RAW geometry file to BRL-CAD converter:
 * regions intermediate data structure implementation
 *
 *  Origin -
 *	IABG mbH (Germany)
 */

#include "RegionList.h"


RegionList::RegionList(void) : m_list() {}


Bot& RegionList::addRegion(const std::string& name)
{
    Bot& ret = m_list[name];

    std::string botName(name);
    botName += ".bot";
    ret.setName(botName);

    return ret;
}


void RegionList::create(rt_wdb* wdbp)
{
    wmember allRegions;
    BU_LIST_INIT(&allRegions.l);

    for (std::map<std::string, Bot>::iterator it = m_list.begin();
	 it != m_list.end();
	 ++it) {
	it->second.write(wdbp);

	wmember regionContent;
	BU_LIST_INIT(&regionContent.l);
	mk_addmember(it->second.name().c_str(), &regionContent.l, 0, WMOP_UNION);

	int id = (int)toValue(it->first.c_str());

	mk_lrcomb(wdbp,
		  it->first.c_str(),
		  &regionContent,
		  1,
		  "plastic",
		  "sh=4 sp=0.5 di=0.5 re=0.1",
		  0,
		  id,
		  0,
		  0,
		  100,
		  0);
	mk_addmember(it->first.c_str(), &allRegions.l, 0, WMOP_UNION);
	mk_freemembers(&regionContent.l);
    }

    mk_lfcomb(wdbp, "all.g", &allRegions, 0);
    mk_freemembers(&allRegions.l);
}


void RegionList::printStat(void) const
{
    std::cout << "Over all size: " << m_list.size() << '\n';
}
