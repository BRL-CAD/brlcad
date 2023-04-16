#pragma once

#include "pch.h"

/**
 * The InformationGatherer class gathers a multitude of information about a 3D model
 * and stores it in an accessible place that can be used by other classes writing
 * the report.  This class itself works to gather all of the required information
 * as specified in Sean Morrison's specifications.
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