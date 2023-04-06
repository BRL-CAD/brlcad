#include "InformationGatherer.h"

#ifdef HAVE_WINDOWS_H
#	include <windows.h>
#	include <stdio.h>
#	include <aclapi.h>
#endif 

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
	// TODO: this
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

	//Gather source file
	last = opt.getFilepath().find_last_of("\\");
	std::string file = opt.getFilepath().substr(last+1, opt.getFilepath().length()-1);
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

    //Gather name of preparer
    infoMap.insert(std::pair < std::string, std::string>("preparer", opt.getName()));

    //Gather classification
    std::string classification = opt.getClassification();
    for (int i = 0; i < classification.length(); i++) {
        classification[i] = toupper(classification[i]);
    }
    infoMap.insert(std::pair < std::string, std::string>("classification", classification));

	//Hard code other stuff into map for now
	infoMap.insert(std::pair<std::string, std::string>("lastUpdate", "3/24/2023"));
	//infoMap.insert(std::pair<std::string, std::string>("checksum", "120EA8A25E5D487BF68B5F7096440019"));

	return true;
}

std::string InformationGatherer::getInfo(std::string key)
{
	// TODO: this
	return infoMap[key];
}
