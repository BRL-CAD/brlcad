#include "InformationGatherer.h"

#ifdef HAVE_WINDOWS_H
#	include <windows.h>
#	include <stdio.h>
#	include <aclapi.h>
#endif 

void getSurfaceArea(Options& opt, std::map<std::string, std::string> map, std::string az, std::string el, std::string comp, double& surfArea) {
    std::string command = opt.getTemppath() + "rtarea -u " + map["units"] + " -a " + az + " -e " + el + " " + opt.getFilepath() + " " + comp + " 2>&1";
    char buffer[128];
    std::string result = "";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) throw std::runtime_error("popen() failed!");
    try {
        while (fgets(buffer, sizeof buffer, pipe) != NULL) {
            result += buffer;
        }
    }
    catch (...) {
        pclose(pipe);
        throw;
    }
    pclose(pipe);
    //If rtarea didn't fail, calculate area
    if (result.find("Total Exposed Area") != std::string::npos) {
        result = result.substr(result.find("Total Exposed Area"));
        result = result.substr(0, result.find("square") - 1);
        result = result.substr(result.find("=") + 1);
        surfArea += stod(result);
    }
}

void getVerificationData(struct ged* g, Options &opt, std::map<std::string, std::string> map, double &volume, double &mass, double &surfArea) {
    const char* cmd[3] = { "tops", NULL, NULL };
    ged_exec(g, 1, cmd);
    std::stringstream ss(bu_vls_addr(g->ged_result_str));
    std::string val;
    std::map<std::string, bool> listed;
    std::vector<std::string> toVisit;
    while (ss >> val) {
        //Get surface area
        getSurfaceArea(opt, map, "0", "0", val, surfArea);
        getSurfaceArea(opt, map, "90", "0", val, surfArea);
        getSurfaceArea(opt, map, "0", "90", val, surfArea);
        //Next, get regions underneath to calculate volume and mass
        const char* cmd[3] = { "l", val.c_str(), NULL };
        ged_exec(g, 2, cmd);
        std::stringstream ss2(bu_vls_addr(g->ged_result_str));
        std::string val2;
        std::getline(ss2, val2); // ignore first line
        //Get initial regions
        while (getline(ss2, val2)) {
            //Parse components
            val2 = val2.erase(0, val2.find_first_not_of(" ")); // left trim
            val2 = val2.erase(val2.find_last_not_of(" ") + 1); // right trim
            if (val2[0] == 'u') {
                val2 = val2.substr(val2.find(' ') + 1); // extract out u
                val2 = val2.substr(0, val2.find('['));
                if (val2.find(' ') != std::string::npos) {
                    val2 = val2.substr(0, val2.find(' '));
                }
                std::map<std::string, bool>::iterator it;
                it = listed.find(val2);
                if (it == listed.end()) {
                    listed[val2] = true;
                    toVisit.push_back(val2);
                }
            }
        }
    }
    for (int i = 0; i < toVisit.size(); i++) {
        std::string val = toVisit[i];
        //Get volume of region
        std::string command = opt.getTemppath() + "gqa -Av -g 2 -u " + map["units"] + ",\"cu " + map["units"] + "\" " + opt.getFilepath() + " " + val + " 2>&1";
        char buffer[128];
        std::string result = "";
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) throw std::runtime_error("popen() failed!");
        try {
            while (fgets(buffer, sizeof buffer, pipe) != NULL) {
                result += buffer;
            }
        }
        catch (...) {
            pclose(pipe);
            throw;
        }
        pclose(pipe); 
        //Extract volume value
        result = result.substr(result.find("Average total volume:") + 22);
        result = result.substr(0, result.find("cu") - 1);
        if (result.find("inf") == std::string::npos) {
            volume += stod(result);
        }
        //Get mass of region
        command = opt.getTemppath() + "gqa -Am -g 2 " + opt.getFilepath() + " " + val + " 2>&1";
        result = "";
        pipe = popen(command.c_str(), "r");
        if (!pipe) throw std::runtime_error("popen() failed!");
        try {
            while (fgets(buffer, sizeof buffer, pipe) != NULL) {
                result += buffer;
            }
        }
        catch (...) {
            pclose(pipe);
            throw;
        }
        pclose(pipe);
        std::cout << result << std::endl; 
    }
}

double getVolume(struct ged* g, std::string filePath, std::string component) {
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

std::vector<std::pair<double, std::string> > getTops(struct ged* g, std::string filePath) {
	const char* cmd[2] = { "tops", NULL };
	ged_exec(g, 1, cmd);
    std::stringstream ss(bu_vls_addr(g->ged_result_str));
    std::vector<std::pair<double, std::string> > topComps;
    std::string val;
    while (ss >> val) {
        topComps.push_back({getVolume(g, filePath, val), val});
    }
    return topComps;
}

std::vector<std::pair<double, std::string> > lsComp(struct ged* g, std::string filePath, std::string component) {
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
        comps.push_back({getVolume(g, filePath, val), val});
    }
    return comps;
}



InformationGatherer::InformationGatherer()
{
}

InformationGatherer::~InformationGatherer()
{

}

bool InformationGatherer::gatherInformation(Options &opt)
{
	//First, extract info using MGED commands

	//Open database
	struct ged* g = ged_open("db", opt.getFilepath().c_str(), 1);

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
		count++;
		res = strtok(NULL, " ");
	}

    // Gather units
	cmd[0] = "units";
	ged_exec(g, 1, cmd);
	std::string result = bu_vls_addr(g->ged_result_str);
	std::size_t first = result.find_first_of("\'");
	std::size_t last = result.find_last_of("\'");
	infoMap.insert(std::pair<std::string, std::string>("units", result.substr(first+1, last-first-1))); 

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
    std::vector<std::pair<double, std::string> > topComp = getTops(g, opt.getFilepath());
    for (auto& x : topComp) {
        pq.push(x);
    }

    while (largestComponents.size() < 5 && !pq.empty()) {
        auto curComp = pq.top(); pq.pop();
        if (curComp.first != std::numeric_limits<double>::infinity()) {
            // add curComp to final list
            largestComponents.push_back(curComp);
        } 
        
        std::vector<std::pair<double, std::string> > childComp = lsComp(g, opt.getFilepath(), curComp.second);
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
      cmd[3] = NULL;
      cmd[4] = NULL;
	  ged_exec(g, 2, cmd);

    std::stringstream ss(bu_vls_addr(g->ged_result_str));
    std::string val;
    std::vector<std::string> dim_data = {"dimX", "dimY", "dimZ", "volume2"};
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

    //Gather volume and mass
    double volume = 0;
    double mass = 0;
    double surfArea = 0;
    getVerificationData(g, opt, infoMap, volume, mass, surfArea);
    ss = std::stringstream();
    ss << volume;
    std::string vol = ss.str();
    ss = std::stringstream();
    ss << surfArea;
    std::string surf = ss.str();
    infoMap.insert(std::pair<std::string, std::string>("volume", vol + " cu " + infoMap["units"]));
    infoMap.insert(std::pair<std::string, std::string>("surfaceArea", surf + " " + infoMap["units"] + "^2"));
    if (mass == 0) {
        infoMap.insert(std::pair<std::string, std::string>("mass", "N/A"));
    }
    else {
        infoMap.insert(std::pair<std::string, std::string>("mass", std::to_string(mass)));
    }

    //Gather representation
    bool hasExplicit = false;
    bool hasImplicit = false;
    const char* tfilter = "-type brep";
    if (db_search(NULL, NULL, tfilter, 0, NULL, g->dbip, NULL) > 0) {
        hasExplicit = true;
    }
    tfilter = "-below -type region -not -type comb -not -type brep";
    if (db_search(NULL, NULL, tfilter, 0, NULL, g->dbip, NULL) > 0) {
        hasImplicit = true;
    }
    if (hasExplicit && hasImplicit) {
        infoMap.insert(std::pair<std::string, std::string>("representation", "Hybrid (Explicit and Implicit)"));
    }
    else if (hasImplicit) {
        infoMap.insert(std::pair<std::string, std::string>("representation", "Implicit with Booleans"));
    }
    else if (hasExplicit) {
        infoMap.insert(std::pair<std::string, std::string>("representation", "Explicit Boundary"));
    }
    else {
        infoMap.insert(std::pair<std::string, std::string>("representation", "ERROR: Type Unknown"));
    }

    //Gather assemblies
    tfilter = "-above -type region";
    int assemblies = db_search(NULL, NULL, tfilter, 0, NULL, g->dbip, NULL);
    infoMap.insert(std::pair<std::string, std::string>("assemblies", std::to_string(assemblies)));

    //Gather entity total
    infoMap.insert(std::pair<std::string, std::string>("total", std::to_string(assemblies + stoi(infoMap["primitives"]) + stoi(infoMap["regions"]))));

    //Close database
    ged_close(g);

    //Next, use other commands to extract info

    //Gather filename

    bool worked = true;
    std::string owner = "username";

#ifdef HAVE_WINDOWS_H
    /*
    * Code primarily taken from https://learn.microsoft.com/en-us/windows/win32/secauthz/finding-the-owner-of-a-file-object-in-c--
    */
    DWORD dwRtnCode = 0;
    PSID pSidOwner = NULL;
    BOOL bRtnBool = TRUE;
    LPTSTR AcctName = NULL;
    LPTSTR DomainName = NULL;
    DWORD dwAcctName = 1, dwDomainName = 1;
    SID_NAME_USE eUse = SidTypeUnknown;
    HANDLE hFile;
    PSECURITY_DESCRIPTOR pSD = NULL;

    hFile = CreateFile(TEXT(opt.getFilepath().c_str()), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        worked = false;
    }
    else {
        dwRtnCode = GetSecurityInfo(hFile, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &pSidOwner, NULL, NULL, NULL, &pSD);
        if (dwRtnCode != ERROR_SUCCESS) {
            worked = false;
        }
        else {
            bRtnBool = LookupAccountSid(NULL, pSidOwner, AcctName, (LPDWORD)&dwAcctName, DomainName, (LPDWORD)&dwDomainName, &eUse);
            AcctName = (LPTSTR)GlobalAlloc(GMEM_FIXED, dwAcctName * sizeof(wchar_t));
            if (AcctName == NULL) {
                worked = false;
            }
            else {
                DomainName = (LPTSTR)GlobalAlloc(GMEM_FIXED, dwDomainName * sizeof(wchar_t));
                if (DomainName == NULL) {
                    worked = false;
                }
                else {
                    bRtnBool = LookupAccountSid(NULL, pSidOwner, AcctName, (LPDWORD)&dwAcctName, DomainName, (LPDWORD)&dwDomainName, &eUse);
                    if (bRtnBool == FALSE) {
                        worked = false;
                    }
                    else {
                        owner = AcctName;
                    }
                }
            }
        }
    }
    CloseHandle(hFile);
#endif

    infoMap.insert(std::pair<std::string, std::string>("owner", owner));

    //Gather last date updated
    struct stat info;
    stat(opt.getFilepath().c_str(), &info);
    std::time_t update = info.st_mtime;
    tm* ltm = localtime(&update);
    std::string date = std::to_string(ltm->tm_mon + 1) + "/" + std::to_string(ltm->tm_mday) + "/" + std::to_string(ltm->tm_year + 1900);
    infoMap.insert(std::pair < std::string, std::string>("lastUpdate", date));

	//Gather source file
    last = opt.getFilepath().find_last_of("/");
#ifdef HAVE_WINDOWS_H
	last = opt.getFilepath().find_last_of("\\");
#endif
	std::string file = opt.getFilepath().substr(last+1, opt.getFilepath().length()-1);

	infoMap.insert(std::pair < std::string, std::string>("file", file));

	//Gather file extension
	last = file.find_last_of(".");
	std::string ext = file.substr(last, file.length() - 1);
	infoMap.insert(std::pair < std::string, std::string>("extension", ext));

	//Gather date of generation
	std::time_t now = time(0);
	ltm = localtime(&now);
	date = std::to_string(ltm->tm_mon+1) + "/" + std::to_string(ltm->tm_mday) + "/" + std::to_string(ltm->tm_year+1900);
	infoMap.insert(std::pair < std::string, std::string>("dateGenerated", date));

    //Gather name of preparer
    infoMap.insert(std::pair < std::string, std::string>("preparer", opt.getName()));

    //Gather classification
    std::string classification = opt.getClassification();
    for (int i = 0; i < classification.length(); i++) {
        classification[i] = toupper(classification[i]);
    }
    infoMap.insert(std::pair < std::string, std::string>("classification", classification));

	//Hard code other stuff into map for now
	infoMap.insert(std::pair<std::string, std::string>("checksum", "120EA8A25E5D487BF68B"));

	return true;
}

std::string InformationGatherer::getInfo(std::string key)
{
	return infoMap[key];
}
