/*           I N F O R M A T I O N G A T H E R E R . H
 * BRL-CAD
 *
 * Copyright (c) 2023 United States Government as represented by
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

#pragma once

#include "pch.h"

/**
 * The InformationGatherer class gathers a multitude of information
 * about a 3D model and stores it in an accessible place that can be
 * used by other classes writing the report.  This class itself works
 * to gather all of the required information as specified in Sean
 * Morrison's specifications.
 */

// TODO: Arrange the class for this.
// this class below is a placeholder; it may not be the optimal layout

class Options;


struct ComponentData {
    int numEntities;
    double volume;
    std::string name;

    bool operator<(const ComponentData& other) const {
	if (numEntities != other.numEntities)
	    return numEntities < other.numEntities;
	if (volume != other.volume)
	    return volume < other.volume;
	return name < other.name;
    }
};


class InformationGatherer
{
private:
    Options* opt;
    struct ged* g;
    std::map<std::string, std::string> infoMap;
    double getVolume(std::string component);
    int getNumEntities(std::string component);
    void getMainComp();
    void getSubComp();
    int getEntityData(char* buf);

public:
    std::vector<ComponentData> largestComponents;
    InformationGatherer(Options* options);
    ~InformationGatherer();

    bool gatherInformation(std::string name);

    std::string getInfo(std::string key);
};


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
