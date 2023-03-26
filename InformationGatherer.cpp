#include "InformationGatherer.h"

InformationGatherer::InformationGatherer()
{
	// TODO: this
}

InformationGatherer::~InformationGatherer()
{
	// TODO: this
}

bool InformationGatherer::gatherInformation(std::string filePath, std::string name)
{
	// TODO: this

	//Open database
	struct ged* g = ged_open("db", filePath.c_str(), 1);

	//Gather title
	const char* cmd[2] = { "title", NULL };
	ged_exec(g, 1, cmd);
	infoMap.insert(std::pair<std::string, std::string>("title", bu_vls_addr(g->ged_result_str)));

	//Gather name of preparer
	infoMap.insert(std::pair < std::string, std::string>("preparer", name));

	//Hard code other stuff into map for now
	infoMap.insert(std::pair<std::string, std::string>("owner", "Ally Hoskinson"));
	infoMap.insert(std::pair<std::string, std::string>("version", "1.1"));
	infoMap.insert(std::pair<std::string, std::string>("lastUpdate", "3/24/2023"));
	infoMap.insert(std::pair<std::string, std::string>("classification", "Confidential"));

	//Close database
	ged_close(g); 
	return true;
}

std::string InformationGatherer::getInfo(std::string key)
{
	// TODO: this
	return infoMap[key];
}