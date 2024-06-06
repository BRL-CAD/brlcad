/*                   R E G I O N _ L I S T . H
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
/** @file region_list.h
 *
 * LS Dyna keyword file to BRL-CAD converter:
 * intermediate region structure declaration
 */

#ifndef REGIONLIST_INCLUDED
#define REGIONLIST_INCLUDED

#include "common.h"

#include <map>

#include "bot.h"

struct Region {
    Bot                                bot;
    std::map<std::string, std::string> attributes;
};

class RegionList {
public:
    RegionList(void);

    Bot& addRegion(const std::string& name);

    Bot&                                getBot(const std::string& name);
    std::map<std::string, std::string>& getAttributes(const std::string& name) ;

    void deleteRegion(const std::string& name);

    void setAttributes(const std::string& name,
		       const std::map<std::string, std::string>& attributes);

    void writeAttributes(rt_wdb* wdbp,
			 const char* name,
			 const std::map<std::string, std::string>& attributes);

    void create(rt_wdb* wdbp);

    void printNames(void) const;
    void printStat(void) const;

private:
    std::map<std::string, Region> m_list;
};


#endif // REGIONLIST_INCLUDED


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
