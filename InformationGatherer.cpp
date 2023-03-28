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
	const char* cmd[5] = { "title", NULL, NULL, NULL, NULL };
	ged_exec(g, 1, cmd);
	infoMap.insert(std::pair<std::string, std::string>("title", bu_vls_addr(g->ged_result_str)));

    // Gather units
	cmd[0] = "units";
	ged_exec(g, 1, cmd);
	infoMap.insert(std::pair<std::string, std::string>("units", bu_vls_addr(g->ged_result_str)));

    // Gather dimensions
	cmd[0] = "bb";
	cmd[1] = "component";
	cmd[2] = NULL;
	ged_exec(g, 2, cmd);
    std::string result(bu_vls_addr(g->ged_result_str));
    std::stringstream ss(bu_vls_addr(g->ged_result_str));
    std::string val;
    std::vector<std::string> dim_data = {"dimX", "dimY", "dimZ", "volume"};
    int dim_idx = 0;
    while (ss >> val) {
        try {
            stod(val);
            infoMap[dim_data[dim_idx++]] = val;
        } catch (const std::exception& e){
            continue;
        }
    }
    // std::cout << "print info " << infoMap["dimX"] << " " << infoMap["dimY"] << " " << infoMap["dimZ"] << " " << infoMap["volume"] << std::endl;

	//Gather name of preparer
	infoMap.insert(std::pair < std::string, std::string>("preparer", name));

	//Gather filepath
	infoMap.insert(std::pair < std::string, std::string>("file", filePath));

	//Gather date of generation
	std::time_t now = time(0);
	tm* ltm = localtime(&now);
	std::string date = std::to_string(ltm->tm_mon+1) + "/" + std::to_string(ltm->tm_mday) + "/" + std::to_string(ltm->tm_year+1900);
	infoMap.insert(std::pair < std::string, std::string>("dateGenerated", date));

	//Hard code other stuff into map for now
	infoMap.insert(std::pair<std::string, std::string>("owner", "Ally Hoskinson"));
	infoMap.insert(std::pair<std::string, std::string>("version", "1.1"));
	infoMap.insert(std::pair<std::string, std::string>("lastUpdate", "3/24/2023"));
	infoMap.insert(std::pair<std::string, std::string>("classification", "Confidential"));
	infoMap.insert(std::pair<std::string, std::string>("checksum", "120EA8A25E5D487BF68B5F7096440019"));

	//Close database
	ged_close(g); 
	return true;
}

std::string InformationGatherer::getInfo(std::string key)
{
	// TODO: this
	return infoMap[key];
}