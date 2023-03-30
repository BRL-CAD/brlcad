#include "InformationGatherer.h"

double getVolume(std::string filePath, std::string component) {
    // Gather dimensions
    struct ged* g;
    g = ged_open("db", filePath.c_str(), 1);
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

std::vector<std::pair<double, std::string> > getTops(std::string filePath) {
    struct ged* g = ged_open("db", filePath.c_str(), 1);
	const char* cmd[2] = { "tops", NULL };
	ged_exec(g, 1, cmd);
    std::stringstream ss(bu_vls_addr(g->ged_result_str));
    std::vector<std::pair<double, std::string> > topComps;
    std::string val;
    while (ss >> val) {
        topComps.push_back({getVolume(filePath, val), val});
    }
    return topComps;
}

std::vector<std::pair<double, std::string> > lsComp(std::string filePath, std::string component) {
    struct ged* g = ged_open("db", filePath.c_str(), 1);
	const char* cmd[3] = { "l", component.c_str(), NULL };
	ged_exec(g, 2, cmd);
    std::stringstream ss(bu_vls_addr(g->ged_result_str));
    std::vector<std::pair<double, std::string> > comps;
    std::string val;
    std::getline(ss, val); // ignore first line
    while (getline(ss, val)) {
        val = val.erase(0, val.find_first_not_of(" ")); // left trim
        val = val.erase(val.find_last_not_of(" ") + 1); // right trim
        val = val.substr(val.find(' ')+1); // extract out u
        comps.push_back({getVolume(filePath, val), val});
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
	struct ged* g = ged_open("db", filePath.c_str(), 1);

	//Gather title
	const char* cmd[5] = { "title", NULL, NULL, NULL, NULL };
	ged_exec(g, 1, cmd);
	infoMap.insert(std::pair<std::string, std::string>("title", bu_vls_addr(g->ged_result_str)));

    // Gather units
	cmd[0] = "units";
	ged_exec(g, 1, cmd);
	infoMap.insert(std::pair<std::string, std::string>("units", bu_vls_addr(g->ged_result_str)));


    // Parse out hierarchy
    // IDEA: Think of it as a tree. We identify the childs with tops command.
    // We visit every child and get its volume, then push it into a max priority queue. 
    // If the volume is not inf, we can push it into the final list of components we are interested.
    // otherwise, we still visit its children but don't push it into final list of components
    // We can stop as soon as final list of components size == num parts interested (e.g. 5) or total parts
    // final list is guaranteed to be sorted by volume

    // std::vector<std::pair<double, std::string> > largestComponents;
    std::priority_queue<std::pair<double, std::string> > pq;
    // run tops command, push all children to pq
    std::vector<std::pair<double, std::string> > topComp = getTops(filePath);
    for (auto& x : topComp) {
        pq.push(x);
    }

    while (largestComponents.size() < 5 && !pq.empty()) {
        auto curComp = pq.top(); pq.pop();
        if (curComp.first != std::numeric_limits<double>::infinity()) {
            // add curComp to final list
            largestComponents.push_back(curComp);
        } 
        
        std::vector<std::pair<double, std::string> > childComp = lsComp(filePath, curComp.second);
        for (auto& x : childComp) {
            if (curComp.first != x.first)
                pq.push(x);
        }
    }

    for (auto& x : largestComponents) {
        std::cout << x.first << " " << x.second << std::endl;
    }



    // Gather dimensions
	cmd[0] = "bb";
	cmd[1] = "component";
	cmd[2] = NULL;
	ged_exec(g, 2, cmd);
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
	std::size_t last = filePath.find_last_of("\\");
	std::string file = filePath.substr(last+1, filePath.length()-1);
	infoMap.insert(std::pair < std::string, std::string>("file", file));

	//Gather date of generation
	std::time_t now = time(0);
	tm* ltm = localtime(&now);
	std::string date = std::to_string(ltm->tm_mon+1) + "/" + std::to_string(ltm->tm_mday) + "/" + std::to_string(ltm->tm_year+1900);
	infoMap.insert(std::pair < std::string, std::string>("dateGenerated", date));

	//Hard code other stuff into map for now
	infoMap.insert(std::pair<std::string, std::string>("owner", "Ally Hoskinson"));
	infoMap.insert(std::pair<std::string, std::string>("version", "1.1"));
	infoMap.insert(std::pair<std::string, std::string>("lastUpdate", "3/24/2023"));
	infoMap.insert(std::pair<std::string, std::string>("classification", "CONFIDENTIAL"));
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
