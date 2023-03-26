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

class InformationGatherer
{
private:
	std::map<std::string, std::string> infoMap;

public:
	InformationGatherer();
	~InformationGatherer();

	bool gatherInformation(std::string filePath, std::string name);

	std::string getInfo(std::string key);
};