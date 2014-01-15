/*                     R E G I O N L I S T . H
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
/** @file RegionList.h
 *
 * RAW geometry file to BRL-CAD converter:
 * regions intermediate data structure declaration
 *
 *  Origin -
 *	IABG mbH (Germany)
 */

#ifndef CONV_RAW_REGIONLIST_H
#define CONV_RAW_REGIONLIST_H

#include "common.h"

#include <map>
#include <sstream>

#include "Bot.h"


class RegionList {
public:
    RegionList(void);

    Bot& addRegion(const std::string& name);

    void create(rt_wdb* wdbp);

    void printStat(void) const;

private:
    std::map<std::string, Bot> m_list;
};


static inline fastf_t
toValue(const char* string)
{
    fastf_t ret;
    std::istringstream buffer(string);

    buffer >> ret;

    return ret;
}


#endif /* CONV_RAW_REGIONLIST_H */
