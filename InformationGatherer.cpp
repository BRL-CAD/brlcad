#include "pch.h"
#include "InformationGatherer.h"

double InformationGatherer::getVolume(std::string component) {
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

int InformationGatherer::getNumEntities(std::string component) {
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

void InformationGatherer::getMainComp() {
    const char* cmd[8] = { "search",  ".",  "-type", "comb", "-not", "-type", "region", NULL };

    ged_exec(g, 7, cmd);
    std::stringstream ss(bu_vls_addr(g->ged_result_str));
    std::string val;
    std::vector<ComponentData> topComponents;

    while (getline(ss, val)) {
        int entities = getNumEntities(val);
        double volume = getVolume(val);
        topComponents.push_back({entities, volume, val});
    }

    sort(topComponents.rbegin(), topComponents.rend());
    for (int i = 0; i < topComponents.size(); i++) {
        if (topComponents[i].volume != std::numeric_limits<double>::infinity()) {
            largestComponents.push_back(topComponents[i]);
            break;
        }
    }
}

void InformationGatherer::getSubComp() {
    // std::string prefix = "../../../build/bin/mged -c ../../../build/bin/share/db/moss.g ";
    std::string pathToOutput = "output/sub_comp.txt";
    std::string retrieveSub = opt->getTemppath() + "mged -c " + opt->getFilepath() + " \"foreach {s} \\[ lt " + largestComponents[0].name + " \\] { set o \\[lindex \\$s 1\\] ; puts \\\"\\$o \\[llength \\[search \\$o \\] \\] \\\" }\" > " + pathToOutput;
    std::cout << retrieveSub << std::endl;
    system(retrieveSub.c_str());
    std::fstream scFile(pathToOutput);
    if (!scFile.is_open()) {
        std::cerr << "failed to open file\n";
        return;
    }

    std::string comp;
    int numEntities = 0;
    std::vector<ComponentData> subComps;

    while (scFile >> comp >> numEntities) {
        double volume = getVolume(comp);
        subComps.push_back({numEntities, volume, comp});
        std::cout << " in subcomp " << comp << " " << numEntities << " " << volume << std::endl;
    }
    sort(subComps.rbegin(), subComps.rend());
    largestComponents.reserve(largestComponents.size() + subComps.size());
    largestComponents.insert(largestComponents.end(), subComps.begin(), subComps.end());
}


InformationGatherer::InformationGatherer(Options* options)
{
    opt = options;
}

InformationGatherer::~InformationGatherer()
{
	// TODO: this
}

bool InformationGatherer::gatherInformation(std::string name)
{
	// TODO: this

	//Open database
    std::string filePath = opt->getFilepath();
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


    getMainComp();
    getSubComp();
    if (largestComponents.size() == 0)
        return false;
    std::cout << "Largest Components\n";
    for (ComponentData x : largestComponents) {
        std::cout << x.name << " " << x.numEntities << " " << x.volume << std::endl;
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
