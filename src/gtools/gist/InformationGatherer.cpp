/*         I N F O R M A T I O N G A T H E R E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2023-2024 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

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

#include "bu/log.h"

static std::string getCmdPath(std::string exeDir, char* cmd) {
    char buf[MAXPATHLEN] = {0};
    if (!bu_dir(buf, MAXPATHLEN, exeDir.c_str(), cmd, BU_DIR_EXT, NULL))
        bu_exit(BRLCAD_ERROR, "Coudn't find %s, aborting.\n", cmd);

    return std::string(buf);
}

void
getSurfaceArea(Options* opt, std::map<std::string, std::string> UNUSED(map), std::string az, std::string el, std::string comp, double& surfArea, std::string unit)
{
    // Run RTArea to get surface area
    std::string rtarea = getCmdPath(opt->getExeDir(), "rtarea");
    std::string in_file(opt->getInFile());  // need local copy for av array copy
    const char* rtarea_av[10] = {rtarea.c_str(),
                                 "-u", unit.c_str(),
                                 "-a", az.c_str(),
                                 "-e", el.c_str(),
                                 in_file.c_str(),
                                 comp.c_str(),
                                 NULL};
    struct bu_process* p;
    bu_process_create(&p, rtarea_av, BU_PROCESS_HIDE_WINDOW | BU_PROCESS_OUT_EQ_ERR);

    if (bu_process_pid(p) <= 0) {
        bu_exit(BRLCAD_ERROR, "Problem with getSurfaceArea, aborting\n");
    }

    char buffer[128];
    std::string result = "";
    int read_cnt = 0;
    while ((read_cnt = bu_process_read_n(p, BU_PROCESS_STDOUT, 128-1, buffer)) > 0) {
        /* NOTE: read does not ensure null-termination, thus buffersize-1 */
        buffer[read_cnt] = '\0';
        result += buffer;
    }

    // If rtarea didn't fail, calculate area
    if (bu_process_wait_n(&p, 0) == 0) {
        /* pull out the surface area */
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
getVerificationData(struct ged* g, Options* opt, std::map<std::string, std::string> map, double &volume, double &mass, double &surfArea00, double &surfArea090, double &surfArea900, std::string lUnit, std::string mUnit)
{
    //Get tops
    const char* tops_cmd[3] = { "tops", NULL, NULL };

    ged_exec(g, 1, tops_cmd);

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

    std::string gqa = getCmdPath(opt->getExeDir(), "gqa");
    std::string units = lUnit + ",\"cu " + lUnit + "\"," + mUnit;
    std::string in_file = opt->getInFile();
    struct bu_process* p;
    int read_cnt = 0;
    char buffer[1024] = {0};
    std::string result = "";

    for (size_t i = 0; i < toVisit.size(); i++) {
        std::string val2 = toVisit[i];
        /* Get volume of region */
        const char* vol_av[10] = { gqa.c_str(),
                                   "-Avm",
                                   "-q",
                                   "-g", "2",
                                   "-u", units.c_str(),
                                   in_file.c_str(),
                                   val2.c_str(),
                                   NULL };

        bu_process_create(&p, vol_av, BU_PROCESS_HIDE_WINDOW | BU_PROCESS_OUT_EQ_ERR);

	if (bu_process_pid(p) <= 0) {
            bu_exit(BRLCAD_ERROR, "Problem with getVerificationData volume, aborting\n");
        }

        result = "";
        read_cnt = 0;
        while ((read_cnt = bu_process_read_n(p, BU_PROCESS_STDOUT, 1024-1, buffer)) > 0) {
            buffer[read_cnt] = '\0';
            result += buffer;
        }

        if (bu_process_wait_n(&p, 0) == 0) {
            // Extract volume value
            std::string vol_raw = result.substr(result.find("Average total volume:") + 22);
            vol_raw = vol_raw.substr(0, vol_raw.find("cu") - 1);
            if (vol_raw.find("inf") == std::string::npos) {
                try {
                    volume += stod(vol_raw);
                } catch (const std::invalid_argument& ia) {
                    bu_exit(BRLCAD_ERROR, "getVerificationData volume got: (%s) %s, aborting.\n", vol_raw.c_str(), ia.what());
                }
            }

            // Extract mass value
            std::string weight_raw = result.substr(result.find("Average total weight:") + 22);
            weight_raw = weight_raw.substr(0, weight_raw.find(" "));
            try {
                // weight cannot be inf or negative
                if (weight_raw.find("inf") == std::string::npos) {
                    if (weight_raw[0] == '-') {
                        weight_raw = weight_raw.substr(1);
                        mass += stod(weight_raw);
                    }
                    else {
                        mass += stod(weight_raw);
                    }
                }
            } catch (const std::invalid_argument& ia) {
                bu_exit(BRLCAD_ERROR, "getVerificationData mass got: (%s) %s, aborting.\n", weight_raw.c_str(), ia.what());
            }
        }
    }
}


double
InformationGatherer::getVolume(std::string component)
{
    // Gather dimensions
    const char* cmd[3] = { "bb", component.c_str(), NULL };

    ged_exec(g, 2, cmd);

    std::stringstream ss(bu_vls_addr(g->ged_result_str));
    std::string val;
    int dim_idx = 0;

    while (ss >> val) {
        try {
            dim_idx++;
            if (dim_idx == 4)
                return stod(val);
        } catch (const std::exception& e) {
            continue;
        }
    }

    return 0;
}


int
InformationGatherer::getNumEntities(std::string UNUSED(component))
{
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


void
InformationGatherer::getMainComp()
{
    const std::string topcomp = opt->getTopComp();
    if (topcomp != "") {
	const char *topname = topcomp.c_str();
        std::cout << "User input top: " << topname << std::endl;
        // check if main comp exists
        const char* cmd[3] = { "l", topname, NULL };
        ged_exec(g, 2, cmd);
        std::string res = bu_vls_addr(g->ged_result_str);
        if (res.size() == 0) {
            std::cerr << "Could not find component: " << topname << std::endl;
            bu_exit(BRLCAD_ERROR, "aborting.\n");
        }

        int entities = getNumEntities(topname);
        double volume = getVolume(topname);
        largestComponents.push_back({entities, volume, topname});
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
    for (size_t i = 0; i < topComponents.size(); i++) {
        if (!EQUAL(topComponents[i].volume, std::numeric_limits<double>::infinity())) {
            largestComponents.push_back(topComponents[i]);
            break;
        }
    }

    if (largestComponents.size() != 0) {
        return;
    } else {
        const char* search_cmd[5] = { "search",  ".",  "-type", "comb", NULL };

        ged_exec(g, 4, search_cmd);
        std::stringstream ss2(bu_vls_addr(g->ged_result_str));
        std::string val2;
        std::vector<ComponentData> topComponents2;

        while (getline(ss2, val2)) {
            int entities = getNumEntities(val2);
            double volume = getVolume(val2);
            topComponents2.push_back({entities, volume, val2});
        }

        sort(topComponents2.rbegin(), topComponents2.rend());
        for (size_t i = 0; i < topComponents2.size(); i++) {
            if (!EQUAL(topComponents2[i].volume, std::numeric_limits<double>::infinity())) {
                largestComponents.push_back(topComponents2[i]);
                break;
            }
        }
    }
}


int
InformationGatherer::getEntityData(char* buf)
{
    std::stringstream ss(buf);
    std::string token;
    int count = 0;

    while (getline(ss, token)) {
        count++;
    }

    return count;
}


void
InformationGatherer::getSubComp()
{
    std::string mged = getCmdPath(opt->getExeDir(), "mged");
    std::string inFile = opt->getInFile();
    std::string tclScript = " \"foreach {s} \\[ lt " + largestComponents[0].name + " \\] { set o \\[lindex \\$s 1\\] ; puts \\\"\\$o \\[llength \\[search \\$o \\] \\] \\\" }\"";
    const char* av[5] = {mged.c_str(), "-c", inFile.c_str(), tclScript.c_str(), NULL};

    struct bu_process* p;
    bu_process_create(&p, av, BU_PROCESS_HIDE_WINDOW);

    int read_cnt = 0;
    char buf[1024] = {0};
    std::string result = "";
    while ((read_cnt = bu_process_read_n(p, BU_PROCESS_STDOUT, 1024-1, buf)) > 0) {
        buf[read_cnt] = '\0';
        result += buf;
    }

    (void)bu_process_wait_n(&p, 0);

    std::stringstream ss(result);
    std::string comp;
    int numEntities = 0;
    std::vector<ComponentData> subComps;

    while (ss >> comp >> numEntities) {
        double volume = getVolume(comp);
        subComps.push_back({numEntities, volume, comp});
        // std::cout << " in subcomp " << comp << " " << numEntities << " " << volume << std::endl;
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

}


bool
InformationGatherer::gatherInformation(std::string name)
{
    //Open database
    std::string filePath = opt->getInFile();
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
    if (opt->isDefaultLength()) {
        lUnit = infoMap["units"];
    }
    else {
        lUnit = opt->getUnitLength();
    }
    std::string mUnit;
    if (opt->isDefaultMass()) {
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
            std::stringstream ss2 = std::stringstream();
            ss2 << std::setprecision(5) << length*convFactor;
            infoMap[dim_data[dim_idx++]] = ss2.str();
        } catch (const std::exception& e) {
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
    double surfArea00 = 0;
    double surfArea090 = 0;
    double surfArea900 = 0;
    getVerificationData(g, opt, infoMap, volume, mass, surfArea00, surfArea090, surfArea900, lUnit, mUnit);
    ss = std::stringstream();
    ss << std::setprecision(5) << volume;
    std::string vol = ss.str();
    ss = std::stringstream();
    ss << std::setprecision(5) << mass;
    std::string ma = ss.str();
    ss = std::stringstream();
    ss << std::setprecision(5) << surfArea00;
    std::string surf00 = ss.str();
    ss = std::stringstream();
    ss << std::setprecision(5) << surfArea090;
    std::string surf090 = ss.str();
    ss = std::stringstream();
    ss << std::setprecision(5) << surfArea900;
    std::string surf900 = ss.str();

    infoMap.insert(std::pair<std::string, std::string>("volume", vol + " " + lUnit + "^3"));
    infoMap.insert(std::pair<std::string, std::string>("surfaceArea00", surf00 + " " + lUnit + "^2"));
    infoMap.insert(std::pair<std::string, std::string>("surfaceArea090", surf090 + " " + lUnit + "^2"));
    infoMap.insert(std::pair<std::string, std::string>("surfaceArea900", surf900 + " " + lUnit + "^2"));

    if (ZERO(mass)) {
        infoMap.insert(std::pair<std::string, std::string>("mass", "N/A"));
    } else {
        infoMap.insert(std::pair<std::string, std::string>("mass", ma + " " + mUnit));
    }

    //Gather representation
    bool hasExplicit = false;
    bool hasImplicit = false;
    const char* tfilter = "-type brep -or -type bot -or -type vol -or -type sketch";

    if (db_search(NULL, 0, tfilter, 0, NULL, g->dbip, NULL) > 0) {
        hasExplicit = true;
    }
    tfilter = "-below -type region -not -type comb -not -type brep -not -type bot -not -type vol -not -type sketch";
    if (db_search(NULL, 0, tfilter, 0, NULL, g->dbip, NULL) > 0) {
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
    int assemblies = db_search(NULL, 0, tfilter, 0, NULL, g->dbip, NULL);
    infoMap.insert(std::pair<std::string, std::string>("assemblies", std::to_string(assemblies)));

    //Gather entity total
    infoMap.insert(std::pair<std::string, std::string>("total", std::to_string(assemblies + stoi(infoMap["primitives"]) + stoi(infoMap["regions"]))));

    //Close database
    ged_close(g);

    //Next, use other commands to extract info

    //Gather filename
    std::string owner = "username";

#ifdef HAVE_WINDOWS_H
    /*
     * Code primarily taken from https://learn.microsoft.com/en-us/windows/win32/secauthz/finding-the-owner-of-a-file-object-in-c--
     */
    bool worked = true;
    DWORD dwRtnCode = 0;
    PSID pSidOwner = NULL;
    BOOL bRtnBool = TRUE;
    LPTSTR AcctName = NULL;
    LPTSTR DomainName = NULL;
    DWORD dwAcctName = 1, dwDomainName = 1;
    SID_NAME_USE eUse = SidTypeUnknown;
    HANDLE hFile;
    PSECURITY_DESCRIPTOR pSD = NULL;

    hFile = CreateFile(TEXT(opt->getInFile().c_str()), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) {
        worked = false;
    } else {
        dwRtnCode = GetSecurityInfo(hFile, SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION, &pSidOwner, NULL, NULL, NULL, &pSD);
        if (dwRtnCode != ERROR_SUCCESS) {
            worked = false;
        } else {
            bRtnBool = LookupAccountSid(NULL, pSidOwner, AcctName, (LPDWORD)&dwAcctName, DomainName, (LPDWORD)&dwDomainName, &eUse);
            AcctName = (LPTSTR)GlobalAlloc(GMEM_FIXED, dwAcctName * sizeof(wchar_t));
            if (AcctName == NULL) {
                worked = false;
            } else {
                DomainName = (LPTSTR)GlobalAlloc(GMEM_FIXED, dwDomainName * sizeof(wchar_t));
                if (DomainName == NULL) {
                    worked = false;
                } else {
                    bRtnBool = LookupAccountSid(NULL, pSidOwner, AcctName, (LPDWORD)&dwAcctName, DomainName, (LPDWORD)&dwDomainName, &eUse);
                    if (bRtnBool == FALSE) {
                        worked = false;
                    } else {
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
    stat(opt->getInFile().c_str(), &fileInfo);
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
    stat(opt->getInFile().c_str(), &info);
    std::time_t update = info.st_mtime;
    tm* ltm = localtime(&update);
    std::string date = std::to_string(ltm->tm_mon + 1) + "/" + std::to_string(ltm->tm_mday) + "/" + std::to_string(ltm->tm_year + 1900);
    infoMap.insert(std::pair < std::string, std::string>("lastUpdate", date));

    //Gather source file
    std::size_t last1 = opt->getInFile().find_last_of("/");
    std::size_t last2 = opt->getInFile().find_last_of("\\");
    last = last1 < last2 ? last1 : last2;

    std::string file = opt->getInFile().substr(last+1, opt->getInFile().length()-1);

    infoMap.insert(std::pair < std::string, std::string>("file", file));

    //Gather date of generation
    std::time_t now = time(0);
    ltm = localtime(&now);
    date = std::to_string(ltm->tm_mon+1) + "/" + std::to_string(ltm->tm_mday) + "/" + std::to_string(ltm->tm_year+1900);
    infoMap["dateGenerated"] = date;

    //Gather name of preparer
    infoMap.insert(std::pair < std::string, std::string>("preparer", opt->getPreparer()));

    //Gather classification
    std::string classification = opt->getClassification();
    for (size_t i = 0; i < classification.length(); i++) {
        classification[i] = toupper(classification[i]);
    }
    infoMap.insert(std::pair < std::string, std::string>("classification", classification));

    //Gather checksum
    struct bu_mapped_file* gFile = NULL;
    gFile = bu_open_mapped_file(opt->getInFile().c_str(), ".g file");
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


std::string
InformationGatherer::getInfo(std::string key)
{
    return infoMap[key];
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
