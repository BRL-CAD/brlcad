/*                 R E G I O N _ L I S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024 United States Government as represented by
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
/** @file region_list.cpp
 *
 * LS Dyna keyword file to BRL-CAD converter:
 * intermediate region structure implementation
 */

#include "common.h"

#include <iostream>

#include "wdb.h"

#include "region_list.h"


RegionList::RegionList(void) : m_list() {}


Bot& RegionList::addRegion
(
    const std::string& name
) {
    Bot& ret = m_list[name];

    std::string botName(name);
    botName += ".bot";
    ret.setName(botName.c_str());

    return ret;
}


void RegionList::create
(
    rt_wdb* wdbp
) {
    wmember all_head;
    BU_LIST_INIT(&all_head.l);

    for (std::map<std::string, Bot>::iterator it = m_list.begin(); it != m_list.end(); ++it) {
	const std::string& region_name = it->first;
	Bot&               bot         = it->second;

	bot.write(wdbp);

	wmember bot_head;
	BU_LIST_INIT(&bot_head.l);

	mk_addmember(bot.getName(), &(bot_head.l), NULL, WMOP_UNION);
	mk_lfcomb(wdbp, region_name.c_str(), &bot_head, 0);
	mk_addmember(region_name.c_str(), &(all_head.l), NULL, WMOP_UNION);

	mk_freemembers(&bot_head.l);
    }

    mk_lfcomb(wdbp, "all.g", &all_head, 0);
    mk_freemembers(&all_head.l);
}


void RegionList::printNames(void) const {
    for (std::map<std::string, Bot>::const_iterator it = m_list.begin(); it != m_list.end(); ++it)
	std::cout << it->first << "\n";
}


void RegionList::printStat(void) const {
    std::cout << "Over all size: " << m_list.size() << "\n";
}

Bot& RegionList::getBot
(
    const std::string& name
) {
    return m_list[name];
}

void RegionList::deleteRegion
(
    const std::string& name
) {
    m_list.erase(name);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
