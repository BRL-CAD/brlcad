#include "InformationGatherer.h"

InformationGatherer::InformationGatherer()
{
	// TODO: this
}

InformationGatherer::~InformationGatherer()
{
	// TODO: this
}

bool InformationGatherer::gatherInformation(std::string filePath)
{
	// TODO: this

	//Gather title
	const char* cmd[2] = {"title", NULL};
	struct ged* g = ged_open("db", filePath.c_str(), 1);

	ged_exec(g, 1, cmd);
	printf("Title: %s\n", bu_vls_addr(g->ged_result_str));
	ged_close(g); 
	return true;
}

std::string InformationGatherer::getInfo(std::string key)
{
	// TODO: this
	return "";
}