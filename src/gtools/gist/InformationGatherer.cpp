#include "pch.h"
#include "InformationGatherer.h"

#ifdef HAVE_WINDOWS_H
#	include <windows.h>
#	include <stdio.h>
#	include <aclapi.h>
#endif
#ifdef __APPLE__
#  include <pwd.h>
#  include <unistd.h>
#endif

void getSurfaceArea(Options* opt, std::map<std::string, std::string> map, std::string az, std::string el, std::string comp, double& surfArea, std::string unit) {
    //Run RTArea to get surface area
    std::string command = opt->getTemppath() + "rtarea -u " + unit + " -a " + az + " -e " + el + " " + opt->getFilepath() + " " + comp + " 2>&1";
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
        try {
            surfArea += stod(result);
        } catch (const std::invalid_argument& ia) {
            std::cerr << "Invalid argument for surface area: " << result << " " << ia.what() << '\n';
            bu_exit(BRLCAD_ERROR, "No input, aborting.\n");
        }
    }
}


void
getVerificationData(struct ged* g, Options* opt, std::map<std::string, std::string> map, double &volume, double &mass, bool &hasDensities, double &surfArea00, double &surfArea090, double &surfArea900, std::string lUnit, std::string mUnit)
{
    //Get tops
    const char* cmd[3] = { "tops", NULL, NULL };
    ged_exec(g, 1, cmd);
    std::stringstream ss(bu_vls_addr(g->ged_result_str));
    std::string val;
    std::map<std::string, bool> listed;
    std::vector<std::string> toVisit;
    while (ss >> val) {
        //Get surface area
        getSurfaceArea(opt, map, "0", "0", val, surfArea00, lUnit);
        getSurfaceArea(opt, map, "0", "90", val, surfArea090, lUnit);
        getSurfaceArea(opt, map, "90", "0", val, surfArea900, lUnit);
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

    //Get volume of region
    for (int i = 0; i < toVisit.size(); i++) {
        std::string val = toVisit[i];
        std::string command = opt->getTemppath() + "gqa -Av -q -g 2 -u " + lUnit + ",\"cu " + lUnit + "\" " + opt->getFilepath() + " " + val + " 2>&1";
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
            try {
                volume += stod(result);
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument for volume: " << result << " " << ia.what() << '\n';
                bu_exit(BRLCAD_ERROR, "No input, aborting.\n");
            }
        }
    }

    // See if component has densities
        std::string command = opt->getTemppath() + "mged -c " + opt->getFilepath() + " mater -d get * " + "2>&1";
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
        if (result.find("no density information found in database") != std::string::npos) {
            mass = 0;
            hasDensities = false;
            return;
        }


    for (int i = 0; i < toVisit.size(); i++) {
        std::string val = toVisit[i];

        //Get mass of region
        std::string command = opt->getTemppath() + "gqa -Am -q -g 2 -u " + lUnit + ",\"cu " + lUnit + "\"," + mUnit + " " + opt->getFilepath() + " " + val + " 2>&1";
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
        if (result.find("Average total weight:") != std::string::npos) {
            //Extract mass value
            result = result.substr(result.find("Average total weight:") + 22);
            result = result.substr(0, result.find(" "));
            //Mass cannot be negative or infinite

            try {
                if (result.find("inf") == std::string::npos) {
                    if (result[0] == '-') {
                        result = result.substr(1);
                        mass += stod(result);
                    }
                    else {
                        mass += stod(result);
                    }
                }
            } catch (const std::invalid_argument& ia) {
                std::cerr << "Invalid argument for mass: " << result << " " << ia.what() << '\n';
                bu_exit(BRLCAD_ERROR, "No input, aborting.\n");
            }
        }
    }
}

std::string formatDouble(double d)
{
    std::stringstream ss;
    ss << std::setprecision(2) << std::fixed << d;
    std::string str = ss.str();

    // size_t dotPos = str.find('.');
    // if (dotPos != std::string::npos) { // string has decimal point
    //     // remove trailing zeroes
    //     str = str.substr(0, str.find_last_not_of('0')+1);
    //     // if the decimal point is now the last character, remove that as well
    //     if(str.find('.') == str.size()-1)
    //     {
    //         str = str.substr(0, str.size()-1);
    //     }
    // }

    return str;
}


double InformationGatherer::getVolume(std::string component) {
    // Gather dimensions
    const char* cmd[3] = { "bb", component.c_str(), NULL };
    ged_exec(g, 2, cmd);
    std::stringstream ss(bu_vls_addr(g->ged_result_str));
    std::string val;
    int dim_idx = 0;
    while (ss >> val) {
        try {
            double temp = stod(val);
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
    const char* cmd[8] = { "search",  ".",  "-type", "comb", "-not", "-type", "region", NULL };
    ged_exec(g, 7, cmd);
    std::stringstream ss(bu_vls_addr(g->ged_result_str));
    std::string val;
    int entities = 0;
    while (getline(ss, val)) {
        entities++;
    }
    return entities;
}

void InformationGatherer::getMainComp() {
    if (opt->getTopComp() != "") {
        std::cout << "User input top: " << opt->getTopComp() << std::endl;
        // check if main comp exists
        const char* cmd[3] = { "l", opt->getTopComp().c_str(), NULL };
        ged_exec(g, 2, cmd);
        std::string res = bu_vls_addr(g->ged_result_str);
        if (res.size() == 0) {
            std::cerr << "Could not find component: " << opt->getTopComp() << std::endl;
            bu_exit(BRLCAD_ERROR, "aborting.\n");
        }

        int entities = getNumEntities(opt->getTopComp());
        double volume = getVolume(opt->getTopComp());
        largestComponents.push_back({entities, volume, opt->getTopComp()});
        return;
    }

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

    if (largestComponents.size() != 0) {
        return;
    } else {
        const char* cmd[5] = { "search",  ".",  "-type", "comb", NULL };

        ged_exec(g, 4, cmd);
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
}

int InformationGatherer::getEntityData(char* buf) {
    std::stringstream ss(buf);
    std::string token;
    int count = 0;
    while (getline(ss, token)) {
        count++;
    }
    return count;
}


void InformationGatherer::getSubComp() {
    // std::string prefix = "../../../build/bin/mged -c ../../../build/bin/share/db/moss.g ";

    if (!bu_file_exists((opt->getTemppath() + "mged").c_str(), NULL) && !bu_file_exists((opt->getTemppath() + "mged.exe").c_str(), NULL)) {
        bu_log("ERROR: File to executables (%s) is invalid or mged is missing\nPlease use (or check) the -T parameter\n", opt->getTemppath().c_str());
        bu_exit(BRLCAD_ERROR, "Bad folder, aborting.\n");
    }

    std::string pathToOutput = "output/sub_comp.txt";
    std::string retrieveSub = opt->getTemppath() + "mged -c " + opt->getFilepath() + " \"foreach {s} \\[ lt " + largestComponents[0].name + " \\] { set o \\[lindex \\$s 1\\] ; puts \\\"\\$o \\[llength \\[search \\$o \\] \\] \\\" }\" > " + pathToOutput;
    system(retrieveSub.c_str());

    if (!bu_file_exists(pathToOutput.c_str(), NULL)) {
        bu_log("ERROR: %s doesn't exist\n", pathToOutput.c_str());
        bu_log("Make sure to create output directory at the same level as main.cpp!\n");
        bu_exit(BRLCAD_ERROR, "No input, aborting.\n");
    }

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
        // std::cout << " in subcomp " << comp << " " << numEntities << " " << volume << std::endl;
    }
    sort(subComps.rbegin(), subComps.rend());
    largestComponents.reserve(largestComponents.size() + subComps.size());
    largestComponents.insert(largestComponents.end(), subComps.begin(), subComps.end());
    scFile.close();
}


InformationGatherer::InformationGatherer(Options* options)
{
    opt = options;
}

InformationGatherer::~InformationGatherer()
{

}

bool InformationGatherer::gatherInformation(std::string name)
{
    //Create folder output if needed
    std::string path = std::filesystem::current_path().string() + "\\output";
    std::cout << "Writing intermediate output files to " << path << std::endl;
    if (!std::filesystem::exists(std::filesystem::path(path))) {
        std::filesystem::create_directory("output");
    }

	//Open database
    std::string filePath = opt->getFilepath();
	g = ged_open("db", filePath.c_str(), 1);

	//Gather title
	const char* cmd[6] = { "title", NULL, NULL, NULL, NULL };
	ged_exec(g, 1, cmd);
    infoMap["title"] = bu_vls_addr(g->ged_result_str);


	//Gather DB Version
	cmd[0] = "dbversion";
    cmd[1] = NULL;
	ged_exec(g, 1, cmd);
    infoMap["version"] = bu_vls_addr(g->ged_result_str);

    // CHECK
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
    cmd[1] = NULL;
	ged_exec(g, 1, cmd);
	std::string result = bu_vls_addr(g->ged_result_str);
	std::size_t first = result.find_first_of("\'");
	std::size_t last = result.find_last_of("\'");
    infoMap["units"] = result.substr(first+1, last-first-1);

    //Get units to use
    std::string lUnit;
    if (opt->isOriginalUnitsLength()) {
        lUnit = infoMap["units"];
    }
    else {
        lUnit = opt->getUnitLength();
    }
    std::string mUnit;
    if (opt->isOriginalUnitsMass()) {
        mUnit = "g";
    }
    else {
        mUnit = opt->getUnitMass();
    }


    getMainComp();
    if (largestComponents.size() == 0)
        return false;

    getSubComp();
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
    std::vector<std::string> dim_data = {"dimX", "dimY", "dimZ", "bbVolume"};
    int dim_idx = 0;
    while (ss >> token) {
        try {
            double length = stod(token);
            double convFactor = bu_units_conversion(infoMap["units"].c_str()) / bu_units_conversion(lUnit.c_str());
            infoMap[dim_data[dim_idx]] = formatDouble(length*convFactor);

            int dimensions = 1;
            if (dim_idx == 3) {
                dimensions = 3;
            }

            Unit u = {lUnit, dimensions};
            unitsMap[dim_data[dim_idx]] = u;

            dim_idx++;
        } catch (const std::exception& e){
            continue;
        }
    }

    // gather group & assemblies
    cmd[0] = "search";
    cmd[1] = largestComponents[0].name.c_str();
    cmd[2] = "-above";
    cmd[3] = "-type";
    cmd[4] = "region";
    cmd[5] = NULL;
    ged_exec(g, 5, cmd);
    infoMap["groups_assemblies"] = std::to_string(getEntityData(bu_vls_addr(g->ged_result_str)));

    // gather primitive shapes
    cmd[2] = "-not";
    cmd[3] = "-type";
    cmd[4] = "comb";
    cmd[5] = NULL;
    ged_exec(g, 5, cmd);
    infoMap["primitives_mged"] = std::to_string(getEntityData(bu_vls_addr(g->ged_result_str)));

    // gather primitive shapes
    cmd[2] = "-type";
    cmd[3] = "region";
    cmd[4] = NULL;
    ged_exec(g, 4, cmd);
    infoMap["regions_parts"] = std::to_string(getEntityData(bu_vls_addr(g->ged_result_str)));

	//Gather name of preparer
    infoMap["preparer"] = name;

    //Gather volume and mass
    double volume = 0;
    double mass = 0;
    bool hasDensities = true;
    double surfArea00 = 0;
    double surfArea090 = 0;
    double surfArea900 = 0;
    getVerificationData(g, opt, infoMap, volume, mass, hasDensities, surfArea00, surfArea090, surfArea900, lUnit, mUnit);
    std::string vol = formatDouble(volume);
    std::string ma = formatDouble(mass);
    std::string surf00 = formatDouble(surfArea00);
    std::string surf090 = formatDouble(surfArea090);
    std::string surf900 = formatDouble(surfArea900);

    infoMap.insert(std::pair<std::string, std::string>("volume", vol));
    Unit u = {lUnit, 3};
    unitsMap["volume"] = u;

    infoMap.insert(std::pair<std::string, std::string>("surfaceArea00", surf00));
    infoMap.insert(std::pair<std::string, std::string>("surfaceArea090", surf090));
    infoMap.insert(std::pair<std::string, std::string>("surfaceArea900", surf900));
    u = {lUnit, 2};
    unitsMap["surfaceArea00"] = u;
    unitsMap["surfaceArea090"] = u;
    unitsMap["surfaceArea900"] = u;

    if (!hasDensities) {
        infoMap.insert(std::pair<std::string, std::string>("mass", "No Material Densities"));
        u = {mUnit, 0};
        unitsMap["mass"] = u;
    } else {
        if (mass == 0) {
            infoMap.insert(std::pair<std::string, std::string>("mass", "N/A"));
            u = {mUnit, 0};
            unitsMap["mass"] = u;
        }
        else {
            infoMap.insert(std::pair<std::string, std::string>("mass", ma));
            u = {mUnit, 1};
            unitsMap["mass"] = u;
        }
    }

    //Gather representation
    bool hasExplicit = false;
    bool hasImplicit = false;
    const char* tfilter = "-type brep -or -type bot -or -type vol -or -type sketch";
    if (db_search(NULL, NULL, tfilter, 0, NULL, g->dbip, NULL) > 0) {
        hasExplicit = true;
    }
    tfilter = "-below -type region -not -type comb -not -type brep -not -type bot -not -type vol -not -type sketch";
    if (db_search(NULL, NULL, tfilter, 0, NULL, g->dbip, NULL) > 0) {
        hasImplicit = true;
    }
    if (hasExplicit && hasImplicit) {
        infoMap.insert(std::pair<std::string, std::string>("representation", "Hybrid (Explicit and Implicit)"));
    }
    else if (hasImplicit) {
        infoMap.insert(std::pair<std::string, std::string>("representation", "Implicit w/ Booleans"));
    }
    else if (hasExplicit) {
        infoMap.insert(std::pair<std::string, std::string>("representation", "Explicit Boundary"));
    }
    else {
        infoMap.insert(std::pair<std::string, std::string>("representation", "ERROR: Type Unknown"));
    }

    //Gather assemblies
    tfilter = "-above -type region";
    int assemblies = db_search(NULL, 0, tfilter, 0, 0, g->dbip, NULL);
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

    hFile = CreateFile(TEXT(opt->getFilepath().c_str()), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

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
#ifdef __APPLE__
    struct stat fileInfo;
    stat(opt->getFilepath().c_str(), &fileInfo);
    uid_t ownerUID = fileInfo.st_uid;
    struct passwd *pw = getpwuid(ownerUID);
    if (pw == NULL) {
        owner = "N/A";
    } else {
        owner = pw->pw_name;
    }
#endif

    infoMap.insert(std::pair<std::string, std::string>("owner", owner));

    //Gather last date updated
    struct stat info;
    stat(opt->getFilepath().c_str(), &info);
    std::time_t update = info.st_mtime;
    tm* ltm = localtime(&update);
    std::string date = std::to_string(ltm->tm_mon + 1) + "/" + std::to_string(ltm->tm_mday) + "/" + std::to_string(ltm->tm_year + 1900);
    infoMap.insert(std::pair < std::string, std::string>("lastUpdate", date));

	//Gather source file
    std::size_t last1 = opt->getFilepath().find_last_of("/");
    std::size_t last2 = opt->getFilepath().find_last_of("\\");
    last = last1 < last2 ? last1 : last2;

	std::string file = opt->getFilepath().substr(last+1, opt->getFilepath().length()-1);

	infoMap.insert(std::pair < std::string, std::string>("file", file));

	//Gather date of generation
	std::time_t now = time(0);
	ltm = localtime(&now);
	date = std::to_string(ltm->tm_mon+1) + "/" + std::to_string(ltm->tm_mday) + "/" + std::to_string(ltm->tm_year+1900);
    infoMap["dateGenerated"] = date;

    //Gather name of preparer
    infoMap.insert(std::pair < std::string, std::string>("preparer", opt->getName()));

    //Gather classification
    std::string classification = opt->getClassification();
    for (int i = 0; i < classification.length(); i++) {
        classification[i] = toupper(classification[i]);
    }
    infoMap.insert(std::pair < std::string, std::string>("classification", classification));

    //Gather checksum
    struct bu_mapped_file* gFile = NULL;
    char* buf = NULL;
    gFile = bu_open_mapped_file(opt->getFilepath().c_str(), ".g file");
    picohash_ctx_t ctx;
    char digest[PICOHASH_MD5_DIGEST_LENGTH];
    std::string output;

    picohash_init_md5(&ctx);
    picohash_update(&ctx, gFile->buf, gFile->buflen);
    picohash_final(&ctx, digest);

    std::stringstream ss2;

    for (int i = 0; i < PICOHASH_MD5_DIGEST_LENGTH; i++) {
        std::stringstream ss3;
        std::string curHex;
        ss3 << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
        curHex = ss3.str();
        if (curHex.length() > 2) {
            //Chop off the f padding
            curHex = curHex.substr(curHex.length() - 2, 2);
        }
        ss2 << curHex;
    }

	infoMap.insert(std::pair<std::string, std::string>("checksum", ss2.str()));

	return true;
}

std::string InformationGatherer::getInfo(std::string key)
{
	return infoMap[key];
}

std::string InformationGatherer::getFormattedInfo(std::string key)
{
    auto it = unitsMap.find(key);
    if (it != unitsMap.end()) { // if units are mapped
        Unit u = unitsMap[key];
        if (u.power > 1) {
            return infoMap[key] + " " + u.unit + "^" + std::to_string(u.power);
        } else if (u.power == 1){
            return infoMap[key] + " " + u.unit;
        }
    }

    // no units if power == 0 or no units mapping
	return infoMap[key];
}

Unit InformationGatherer::getUnit(std::string name)
{
    auto it = unitsMap.find(name);
    if (it != unitsMap.end()) {
        return it->second;
    }

    throw std::runtime_error("Unit not found for key: " + name);
}

bool InformationGatherer::dimensionSizeCondition()
{
    if (std::stod(infoMap["dimX"]) > 200) return true;
    if (std::stod(infoMap["dimY"]) > 200) return true;
    if (std::stod(infoMap["dimZ"]) > 200) return true;
    return false;
}

void InformationGatherer::correctDefaultUnitsLength()
{
    // length conditionals
    bool isInches = infoMap["units"] == "in" || infoMap["units"] == "inch" || infoMap["units"] == "inches";
    bool isCentimeters = infoMap["units"] == "cm";
    bool isMillimeters = infoMap["units"] == "mm";

    std::string toUnits = "";

    if (dimensionSizeCondition()) {
        if (isInches ) {
            toUnits = "ft";
        } else if (isCentimeters) {
            toUnits = "m";
        } else if (isMillimeters) {
            toUnits = "cm";
        }
    }

    if (toUnits.empty()) return;

    for (auto& pair : unitsMap) {
        const std::string& key = pair.first;
        Unit& unit = pair.second;

        if(key == "mass") {
            continue;
        }

        double value = std::stod(getInfo(key));
        double convFactor = pow(bu_units_conversion(unit.unit.c_str()), unit.power) / pow(bu_units_conversion(toUnits.c_str()), unit.power);
        infoMap[key] = formatDouble(value*convFactor);
        unit.unit = toUnits;
    }
}

void InformationGatherer::correctDefaultUnitsMass()
{
    // mass conditionals
    bool isGrams = infoMap["units"] == "g";
    std::string toUnits = "";

    if (isGrams) {
        if (unitsMap["mass"].power != 0) { // mass exists
            if (std::stod(infoMap["mass"]) > 20000) { // mass greater than 20,000 g
                toUnits = "lbs";
            }
        }
    }

    if (toUnits.empty()) return;

    Unit& unit = unitsMap["mass"];

    double value = std::stod(getInfo("mass"));
    double convFactor = bu_units_conversion(unit.unit.c_str()) / bu_units_conversion(toUnits.c_str());
    infoMap["mass"] = formatDouble(value*convFactor);
    unit.unit = toUnits;
}

void InformationGatherer::checkScientificNotation() {
    for (auto& pair : unitsMap) {
        const std::string& key = pair.first;
        Unit& unit = pair.second;

        if (unit.power == 0) continue;
        if (getInfo(key).size() < 11) continue;

        double value;

        try {
            value = std::stod(getInfo(key));
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error("Value for " + key + " is invalid");
        } catch (const std::out_of_range& e) {
            throw std::runtime_error("Value for " + key + " is out of range");
        }

        std::stringstream ss;
        ss << std::scientific << std::setprecision(2) << value;
        infoMap[key] = ss.str();

    }
}
