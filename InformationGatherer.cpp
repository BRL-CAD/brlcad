#include "InformationGatherer.h"

double InformationGatherer::getVolume(std::string filePath, std::string component) {
    // Gather dimensions
    const char* cmd[3] = { "bb", component.c_str(), NULL };
    ged_exec(g, 2, cmd);
    std::stringstream ss(bu_vls_addr(g->ged_result_str));
    std::string val;
    int dim_idx = 0;
    while (ss >> val) {
        try {
            stod(val);
            dim_idx++;
            if (dim_idx == 4)
                return stod(val);
        } catch (const std::exception& e){
            continue;
        }
    }

    return 0;
}

int InformationGatherer::getNumEntities(std::string filePath, std::string component) {
    // Find number of entities 
    const char* cmd[4] = { "search", component.c_str(), "-type comb -not -type region", NULL };
    ged_exec(g, 3, cmd);
    std::stringstream ss(bu_vls_addr(g->ged_result_str));
    std::string val;
    int entities = 0;
    while (getline(ss, val)) {
        entities++;
    }
    return entities;
}

std::vector<ComponentData> InformationGatherer::getTops(std::string filePath) {
    const char* cmd[8] = { "search",  ".",  "-type", "comb", "-not", "-type", "region", NULL };
    ged_exec(g, 7, cmd);
    std::stringstream ss(bu_vls_addr(g->ged_result_str));
    std::string val;
    std::vector<ComponentData> allComps;
    while (getline(ss, val)) {
        int entities = getNumEntities(filePath, val);
        double volume = getVolume(filePath, val);
        allComps.push_back({entities, volume, val});
    }
    return allComps;

    // struct ged* g = ged_open("db", filePath.c_str(), 1);
	// const char* cmd[2] = { "tops", NULL };
	// ged_exec(g, 1, cmd);
    // std::stringstream ss(bu_vls_addr(g->ged_result_str));
    // std::vector<ComponentData> topComps;
    // std::string val;
    // while (ss >> val) {
    //     topComps.push_back({getNumEntities(filePath, val), getVolume(filePath, val), val});
    // }
    // return topComps;
}

std::vector<ComponentData> InformationGatherer::lsComp(std::string filePath, std::string component) {
	const char* cmd[3] = { "l", component.c_str(), NULL };
	ged_exec(g, 2, cmd);
    std::stringstream ss(bu_vls_addr(g->ged_result_str));
    std::vector<ComponentData> comps;
    std::string val;
    std::getline(ss, val); // ignore first line
    while (getline(ss, val)) {
        val = val.erase(0, val.find_first_not_of(" ")); // left trim
        val = val.erase(val.find_last_not_of(" ") + 1); // right trim
        val = val.substr(val.find(' ')+1); // extract out u
        comps.push_back({getNumEntities(filePath, val), getVolume(filePath, val), val});
    }
    return comps;
}



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
	g = ged_open("db", filePath.c_str(), 1);

	//Gather title
	const char* cmd[5] = { "title", NULL, NULL, NULL, NULL };
	ged_exec(g, 1, cmd);
    infoMap["title"] = bu_vls_addr(g->ged_result_str);

	//Gather primitives, regions, total objects
	cmd[0] = "summary";
	ged_exec(g, 1, cmd);
	char* res = strtok(bu_vls_addr(g->ged_result_str), " ");
	int count = 0;
	while (res != NULL) {
		if (count == 1) {
            infoMap["primitives"] = res;
		}
		else if (count == 3) {
            infoMap["regions"] = res;
		}
		else if (count == 5) {
            infoMap["non-regions"] = res;
		}
		else if (count == 8) {
            infoMap["total"] = res;
		}
		count++;
		res = strtok(NULL, " ");
	}

	//Gather DB Version
	cmd[0] = "dbversion";
	ged_exec(g, 1, cmd);
    infoMap["version"] = bu_vls_addr(g->ged_result_str);

    // Gather units
	cmd[0] = "units";
	ged_exec(g, 1, cmd);
	std::string result = bu_vls_addr(g->ged_result_str);
	std::size_t first = result.find_first_of("\'");
	std::size_t last = result.find_last_of("\'");
    infoMap["units"] = result.substr(first+1, last-first-1);


    // Parse out hierarchy
    // IDEA: Think of it as a tree. We identify the childs with tops command.
    // We visit every child and get its volume, then push it into a max priority queue. 
    // If the volume is not inf, we can push it into the final list of components we are interested.
    // otherwise, we still visit its children but don't push it into final list of components
    // We can stop as soon as final list of components size == num parts interested (e.g. 5) or total parts
    // final list is guaranteed to be sorted by volume

    std::vector<ComponentData> topComponents = getTops(filePath);
    sort(topComponents.rbegin(), topComponents.rend());
    // std::priority_queue<ComponentData> pq;
    // // run tops command, push all children to pq
    // std::vector<ComponentData> topComp = getTops(filePath);
    // std::cout << "TOPS" << std::endl;
    // for (auto& x : topComp) {
    //     pq.push(x);
    //     std::cout << x.numEntities << " " << x.volume << " " << x.name << std::endl;
    // }
    // std::cout << "END OF TOPS" << std::endl;

    // while (largestComponents.size() < 5 && !pq.empty()) {
    //     ComponentData curComp = pq.top(); pq.pop();
    //     if (curComp.volume != std::numeric_limits<double>::infinity()) {
    //         // add curComp to final list
    //         largestComponents.push_back(curComp);
    //     } 
        
    //     std::vector<ComponentData> childComp = lsComp(filePath, curComp.name);
    //     for (auto& x : childComp) {
    //         // if (curComp.volume != x.volume)
    //             pq.push(x);
    //     }
    // }

    // while (! pq.empty() ) {
    //     ComponentData qTop = pq.top();
    //     std::cout << qTop.numEntities << " " << qTop.volume << " " << qTop.name << "\n";
    //     pq.pop();
    // }

    int freq = 0;
    for (auto& x : topComponents) {
        std::cout << x.numEntities << " " << x.volume << " " << x.name << std::endl;
        if (x.volume != std::numeric_limits<double>::infinity()) {
            largestComponents.push_back(x);
            freq++;
        }
        if (freq == 5) {
            break;
        }
    }



    // Gather dimensions
	  cmd[0] = "bb";
	  cmd[1] = largestComponents[0].name.c_str();
	  cmd[2] = NULL;
	  ged_exec(g, 2, cmd);

    std::stringstream ss(bu_vls_addr(g->ged_result_str));
    std::string token;
    std::vector<std::string> dim_data = {"dimX", "dimY", "dimZ", "volume"};
    int dim_idx = 0;
    while (ss >> token) {
        try {
            stod(token);
            infoMap[dim_data[dim_idx++]] = token;
        } catch (const std::exception& e){
            continue;
        }
    }
    // std::cout << "print info " << infoMap["dimX"] << " " << infoMap["dimY"] << " " << infoMap["dimZ"] << " " << infoMap["volume"] << std::endl;

	//Gather name of preparer
    infoMap["preparer"] = name;

	//Gather source file
	last = filePath.find_last_of("\\");
	std::string file = filePath.substr(last+1, filePath.length()-1);
    infoMap["file"] = file;

	//Gather file extension
	last = file.find_last_of(".");
	std::string ext = file.substr(last, file.length() - 1);
    infoMap["extension"] = ext;

	//Gather date of generation
	std::time_t now = time(0);
	tm* ltm = localtime(&now);
	std::string date = std::to_string(ltm->tm_mon+1) + "/" + std::to_string(ltm->tm_mday) + "/" + std::to_string(ltm->tm_year+1900);
    infoMap["dateGenerated"] = date;

	//Hard code other stuff into map for now
    infoMap["owner"] = "Ally Hoskinson";
    infoMap["lastUpdate"] = "3/24/2023";
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
