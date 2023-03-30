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

	//Gather primitives, regions, total objects
	cmd[0] = "summary";
	ged_exec(g, 1, cmd);
	char* res = strtok(bu_vls_addr(g->ged_result_str), " ");
	int count = 0;
	while (res != NULL) {
		if (count == 1) {
			infoMap.insert(std::pair < std::string, std::string>("primitives", res));
		}
		else if (count == 3) {
			infoMap.insert(std::pair < std::string, std::string>("regions", res));
		}
		else if (count == 5) {
			infoMap.insert(std::pair < std::string, std::string>("non-regions", res));
		}
		else if (count == 8) {
			infoMap.insert(std::pair < std::string, std::string>("total", res));
		}
		count++;
		res = strtok(NULL, " ");
	}

	//Gather DB Version
	cmd[0] = "dbversion";
	ged_exec(g, 1, cmd);
	infoMap.insert(std::pair<std::string, std::string>("version", bu_vls_addr(g->ged_result_str)));

    // Gather units
	cmd[0] = "units";
	ged_exec(g, 1, cmd);
	std::string result = bu_vls_addr(g->ged_result_str);
	std::size_t first = result.find_first_of("\'");
	std::size_t last = result.find_last_of("\'");
	infoMap.insert(std::pair<std::string, std::string>("units", result.substr(first+1, last-first-1))); 

    // Gather dimensions
	cmd[0] = "bb";
	cmd[1] = "component";
	cmd[2] = NULL;
	ged_exec(g, 2, cmd);
    result = (bu_vls_addr(g->ged_result_str));
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

	//Gather source file
	last = filePath.find_last_of("\\");
	std::string file = filePath.substr(last+1, filePath.length()-1);
	infoMap.insert(std::pair < std::string, std::string>("file", file));

	//Gather file extension
	last = file.find_last_of(".");
	std::string ext = file.substr(last, file.length() - 1);
	infoMap.insert(std::pair < std::string, std::string>("extension", ext));

	//Gather date of generation
	std::time_t now = time(0);
	tm* ltm = localtime(&now);
	std::string date = std::to_string(ltm->tm_mon+1) + "/" + std::to_string(ltm->tm_mday) + "/" + std::to_string(ltm->tm_year+1900);
	infoMap.insert(std::pair < std::string, std::string>("dateGenerated", date));

	//Hard code other stuff into map for now
	infoMap.insert(std::pair<std::string, std::string>("owner", "Ally Hoskinson"));
	infoMap.insert(std::pair<std::string, std::string>("lastUpdate", "3/24/2023"));
	//infoMap.insert(std::pair<std::string, std::string>("classification", "CONFIDENTIAL"));
	//infoMap.insert(std::pair<std::string, std::string>("checksum", "120EA8A25E5D487BF68B5F7096440019"));

	//Close database
	ged_close(g); 
	return true;
}

std::string InformationGatherer::getInfo(std::string key)
{
	// TODO: this
	return infoMap[key];
}