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
    Bot& ret = m_list[name].bot;

    std::string botName(name);
    botName += ".bot";
    ret.setName(botName.c_str());

    return ret;
}


void RegionList::setAttributes
(
    const std::string&                        name,
    const std::map<std::string, std::string>& attributes
) {
    m_list[name].attributes = attributes;
}

void RegionList::writeAttributes
(
    rt_wdb* wdbp,
    const char* name,
    const std::map<std::string, std::string>& attributes
) {

    struct rt_db_internal     region_internal;
    struct directory*         dp;
    struct db_i*              dbip;
    bu_attribute_value_set*   avs;

    dbip = wdbp->dbip;
    dp = db_lookup(dbip, name, 0);

    rt_db_get_internal(&region_internal, dp, dbip, bn_mat_identity, &rt_uniresource);
    avs = &region_internal.idb_avs;
    for (std::map<std::string, std::string>::const_iterator it = attributes.begin(); it != attributes.end(); it++)
    {
	bu_avs_add(avs, it->first.c_str(), it->second.c_str());
    }

    bu_avs_print(avs, name);
    db5_update_attributes(dp, avs, dbip);
}


void RegionList::create
(
    rt_wdb* wdbp
) {
    wmember all_head;
    BU_LIST_INIT(&all_head.l);

    for (std::map<std::string, Region>::iterator it = m_list.begin(); it != m_list.end(); ++it) {
	const std::string&                 region_name       = it->first;
	Region&                            region            = it->second;
	Bot&                               bot               = region.bot;
	std::map<std::string, std::string> region_attributes = region.attributes;

	bot.write(wdbp);

	wmember bot_head;
	BU_LIST_INIT(&bot_head.l);

	mk_addmember(bot.getName(), &(bot_head.l), NULL, WMOP_UNION);
	mk_lfcomb(wdbp, region_name.c_str(), &bot_head, 0);

	if (region_attributes.size() > 0) {
	    RegionList::writeAttributes(wdbp, region_name.c_str(), region_attributes);
	}

	mk_addmember(region_name.c_str(), &(all_head.l), NULL, WMOP_UNION);
	
	mk_freemembers(&bot_head.l);
    }

    mk_lfcomb(wdbp, "all.g", &all_head, 0);
    mk_freemembers(&all_head.l);
}


void RegionList::printNames(void) const {
    for (std::map<std::string, Region>::const_iterator it = m_list.begin(); it != m_list.end(); ++it)
	std::cout << it->first << "\n";
}


void RegionList::printStat(void) const {
    std::cout << "Over all size: " << m_list.size() << "\n";
}

Bot& RegionList::getBot
(
    const std::string& name
) {
    return m_list[name].bot;
}

std::map<std::string, std::string>& RegionList::getAttributes
(
    const std::string& name
) {
    return m_list[name].attributes;
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
