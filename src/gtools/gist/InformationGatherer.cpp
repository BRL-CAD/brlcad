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

#include "vmath.h"
#include "pch.h"
#include "InformationGatherer.h"
#include "bu/log.h"

static std::string getCmdPath(std::string exeDir, const char* cmd) {
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
            bu_exit(BRLCAD_ERROR, "Invalid argument for getSurfaceArea. Got (%s), aborting.\n", ia.what());
        }
    }
}

static double
parseVolume(std::string res)
{
    double ret = 0;
    // Extract volume value
    std::string vol_raw = res.substr(res.find("Average total volume:") + 22);
    vol_raw = vol_raw.substr(0, vol_raw.find("cu") - 1);
    if (vol_raw.find("inf") == std::string::npos) {
        try {
            ret = stod(vol_raw);
        } catch (const std::invalid_argument& ia) {
            bu_exit(BRLCAD_ERROR, "parseVolume got: (%s) %s, aborting.\n", vol_raw.c_str(), ia.what());
        }
    }

    return ret;
}

static double
parseMass(std::string res)
{
    double ret = 0;
    // Extract mass value
    std::string weight_raw = res.substr(res.find("Average total weight:") + 22);
    weight_raw = weight_raw.substr(0, weight_raw.find(" "));
    try {
        // weight cannot be inf or negative
        if (weight_raw.find("inf") == std::string::npos) {
            if (weight_raw[0] == '-') {
                weight_raw = weight_raw.substr(1);
                ret += stod(weight_raw);
            }
            else {
                ret += stod(weight_raw);
            }
        }
    } catch (const std::invalid_argument& ia) {
        bu_exit(BRLCAD_ERROR, "parseMass got: (%s) %s, aborting.\n", weight_raw.c_str(), ia.what());
    }

    return ret;
}


void
getVerificationData(struct ged* g, Options* opt, std::map<std::string, std::string> map, double &volume, double &mass, bool &UNUSED(hasDensities), double &surfArea00, double &surfArea090, double &surfArea900, std::string lUnit, std::string mUnit)
{
    //Get tops
    {
	const char* cmd[3] = { "tops", NULL, NULL };
	ged_exec(g, 1, cmd);
    }
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
	{
	    const char* cmd[3] = { "l", val.c_str(), NULL };
	    ged_exec(g, 2, cmd);
	}
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
    std::string units = lUnit + ",cu " + lUnit + "," + mUnit;
    std::string in_file = opt->getInFile();
    struct bu_process* p;
    int read_cnt = 0;
    char buffer[1024] = {0};
    std::string result = "";
    std::string no_density_msg = "Could not find any density information";

    for (size_t i = 0; i < toVisit.size(); i++) {
        std::string vval = toVisit[i];
        /* Get volume and mass */
        const char* vol_av[10] = { gqa.c_str(),
                                   "-Avm",
                                   "-q",
                                   "-g", "2",
                                   "-u", units.c_str(),
                                   in_file.c_str(),
                                   vval.c_str(),
                                   NULL };

        bu_process_create(&p, vol_av, BU_PROCESS_HIDE_WINDOW | BU_PROCESS_OUT_EQ_ERR);

	if (bu_process_pid(p) <= 0) {
            bu_exit(BRLCAD_ERROR, "Problem in getVerificationData process creation, aborting\n");
        }

        result = "";
        read_cnt = 0;
        while ((read_cnt = bu_process_read_n(p, BU_PROCESS_STDOUT, 1024-1, buffer)) > 0) {
            buffer[read_cnt] = '\0';
            result += buffer;
        }

        if (bu_process_wait_n(&p, 0) != 0) {
            continue;
        }

        // attempt to get both volume and mass in the same run - 
        // make sure we did not get 'no density file' error message
        if (result.find(no_density_msg) == std::string::npos) {
            volume += parseVolume(result);
            mass += parseMass(result);
            continue;
        }


        // no density data found. need to re-run, only getting volume data
        vol_av[1] = "-Av";
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

        if (bu_process_wait_n(&p, 0) != 0)
            continue;

        volume += parseVolume(result);
    }
}

std::string
formatDouble(double d)
{
    int precision = 2; // arbitrary

    std::stringstream ss;
    ss << std::setprecision(precision) << std::fixed << d;
    std::string str = ss.str();

    return str;
}


double
InformationGatherer::getBBVolume(std::string component)
{
    // Gather bounding box dimensions
    const char* cmd[4] = { "bb", "-qv", component.c_str(), NULL };
    ged_exec(g, 3, cmd);
    std::istringstream ss(bu_vls_addr(g->ged_result_str));

    // output always in form: "Bounding Box Volume: %d units^3"
    std::string bounding, box, volume, val, units;
    ss >> bounding >> box >> volume >> val >> units;

    // make sure we parsed string correctly and got valid value
    try {
        double bb_value = stod(val);
        return bb_value;
    } catch (const std::exception& e){
        return -1;
    }

    return 0;
}

int
InformationGatherer::getNumEntities(std::string component)
{
    // Find number of unioned entities
    // TODO/NOTE: is union the best heuristic for 'entities'?
    struct directory* dp = db_lookup(g->dbip, component.c_str(), LOOKUP_QUIET);
    const char* filter = "-bool u";
    int entities = db_search(NULL, DB_SEARCH_HIDDEN | DB_SEARCH_QUIET, filter, 1, &dp, g->dbip, NULL);

    return entities > 0 ? entities : 0; // clamp errors to 0
}

void
InformationGatherer::getMainComp()
{
    const std::string topcomp = opt->getTopComp();
    if (!topcomp.empty()) {
	const char *topname = topcomp.c_str();
        // check if main comp exists
        const char* cmd[3] = { "exists", topname, NULL };
        ged_exec(g, 2, cmd);
        std::string res = bu_vls_addr(g->ged_result_str);
        if (res != "1") {
            bu_exit(BRLCAD_ERROR, "Coult not find component (%s), aborting.\n", topname);
        }

        int entities = getNumEntities(opt->getTopComp());
        double volume = getBBVolume(opt->getTopComp());
        largestComponents.push_back({entities, volume, opt->getTopComp()});
        return;
    }

    // get top level objects
    const char* cmd[3] = { "tops", "-n", NULL };

    ged_exec(g, 2, cmd);
    std::istringstream ss(bu_vls_addr(g->ged_result_str));
    std::string comp;
    std::vector<ComponentData> topComponents;

    while (ss >> comp) {
        int entities = getNumEntities(comp);
        double volume = getBBVolume(comp);
        topComponents.push_back({entities, volume, comp});
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
            double volume = getBBVolume(val2);
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
    std::string tclScript = "foreach {s} \\[ lt " + largestComponents[0].name + " \\] { set o \\[lindex \\$s 1\\] ; puts \\\"\\$o \\[llength \\[search \\$o \\] \\] \\\" }";
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
        double volume = getBBVolume(comp);
        subComps.push_back({numEntities, volume, comp});
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
InformationGatherer::gatherInformation(std::string UNUSED(name))
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
	} else if (count == 3) {
	    infoMap.insert(std::pair < std::string, std::string>("regions", res));
	} else if (count == 5) {
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
    } else {
        lUnit = opt->getUnitLength();
    }
    std::string mUnit;
    if (opt->isOriginalUnitsMass()) {
        mUnit = "g";
    } else {
        mUnit = opt->getUnitMass();
    }


    getMainComp();
    if (largestComponents.size() == 0) {
        return false;
    }

    getSubComp();

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
        infoMap.insert(std::pair<std::string, std::string>("mass", "Not Available"));
        u = {mUnit, 0};
        unitsMap["mass"] = u;
    } else {
        if (NEAR_ZERO(mass, SMALL_FASTF)) {
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
    std::string owner = "username";

    char buffer[1024];  //for name of owner
#ifdef HAVE_WINDOWS_H
    /*
    * Code primarily taken from https://learn.microsoft.com/en-us/windows/win32/secauthz/finding-the-owner-of-a-file-object-in-c--
    */
    DWORD buffer_len = sizeof(buffer) / sizeof(buffer[0]);
    if (!GetUserName(buffer, &buffer_len)) {
        owner = "Unknown";
    } else {
        owner = std::string(buffer);
    }
#else   // UNIX-like systems
    struct stat fileInfo;
    stat(opt->getInFile().c_str(), &fileInfo);
    uid_t ownerUID = fileInfo.st_uid;
    struct passwd *pw = getpwuid(ownerUID);
    if (pw && pw->pw_gecos && pw->pw_gecos[0]) {
        // Try to extract the full name from pw_gecos, if available
        std::string full_name(pw->pw_gecos);
        std::size_t comma_pos = full_name.find(',');
        if (comma_pos != std::string::npos) {
            owner = full_name.substr(0, comma_pos); // Full name is before the first comma
        } else {
            owner = full_name; // Use the whole string if there's no comma
        }
    } else {
        // Fallback to login name
        if (getlogin_r(buffer, sizeof(buffer)) == 0) {
            owner = std::string(buffer);
        } else {
            owner = "Unknown";
        }
    }
#endif
    if (opt->getOwner() != "") {
        owner = opt->getOwner();
    }
    infoMap.insert(std::pair<std::string, std::string>("owner", owner));

    //Default name of preparer to owner if "N/A"
    if (opt->getPreparer() == "N/A") {
        infoMap["preparer"] = owner;
    } else {
        infoMap["preparer"] = opt->getPreparer();
    }

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

    std::string file = opt->getInFile();

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

void
GetFullNameOrUsername(std::string& name)
{
    char buffer[1024];

    #if defined(_WIN32) || defined(_WIN64) // Windows systems

    DWORD buffer_len = sizeof(buffer) / sizeof(buffer[0]);
    if (!GetUserName(buffer, &buffer_len)) {
        name = "Unknown";
    } else {
        name = std::string(buffer);
    }

    #else // UNIX-like systems

    struct passwd *pw;
    uid_t uid = geteuid();
    pw = getpwuid(uid);
    if (pw && pw->pw_gecos && pw->pw_gecos[0]) {
        // Try to extract the full name from pw_gecos, if available
        std::string full_name(pw->pw_gecos);
        std::size_t comma_pos = full_name.find(',');
        if (comma_pos != std::string::npos) {
            name = full_name.substr(0, comma_pos); // Full name is before the first comma
        } else {
            name = full_name; // Use the whole string if there's no comma
        }
    } else {
        // Fallback to login name
        if (getlogin_r(buffer, sizeof(buffer)) == 0) {
            name = std::string(buffer);
        } else {
            name = "Unknown";
        }
    }

    #endif
}

std::string
InformationGatherer::getInfo(std::string key)
{
    return infoMap[key];
}

std::string
InformationGatherer::getFormattedInfo(std::string key)
{
    // get number, insert commas if long enough
    std::string number = infoMap[key];
    size_t decimal_pos = number.find_last_of(".");
    // start from end of whole number
    size_t start_pos = (decimal_pos != std::string::npos) ? decimal_pos : number.size();
    // offset 3 for potential first comma
    start_pos -= 3;
    // valid start position, start adding commas
    for (int i = start_pos; i > 0; i -= 3) {
        number.insert(i, ",");
    }

    // add units to string
    auto it = unitsMap.find(key);
    if (it != unitsMap.end()) { // if units are mapped
        Unit u = unitsMap[key];
        if (u.power > 1) {
            return number + " " + u.unit + "^" + std::to_string(u.power);
        } else if (u.power == 1){
            return number + " " + u.unit;
        }
    }

    // no units if power == 0 or no units mapping
    return number;
}

Unit
InformationGatherer::getUnit(std::string name)
{
    auto it = unitsMap.find(name);
    if (it != unitsMap.end()) {
        return it->second;
    }

    throw std::runtime_error("Unit not found for key: " + name);
}

bool
InformationGatherer::dimensionSizeCondition()
{
    int size = 200; // arbitrary value
    if (std::stod(infoMap["dimX"]) > size) return true;
    if (std::stod(infoMap["dimY"]) > size) return true;
    if (std::stod(infoMap["dimZ"]) > size) return true;
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

        if (key == "mass") {
            continue;
        }

        double value = std::stod(getInfo(key));
        double convFactor = pow(bu_units_conversion(unit.unit.c_str()), unit.power) / pow(bu_units_conversion(toUnits.c_str()), unit.power);
        infoMap[key] = formatDouble(value*convFactor);
        unit.unit = toUnits;
    }
}

void
InformationGatherer::correctDefaultUnitsMass()
{
    int size = 20000;
    std::string toUnits = "";

    bool isGrams = unitsMap["mass"].unit == "g";

    if (isGrams) {
        if (unitsMap["mass"].power != 0) { // mass exists
            if (std::stod(infoMap["mass"]) > size) { // mass greater than 20,000 g
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

void
InformationGatherer::checkScientificNotation()
{
    size_t size = 15; // arbitrary
    int precision = 2; // arbitrarty

    for (auto& pair : unitsMap) {
        const std::string& key = pair.first;
        Unit& unit = pair.second;

        if (unit.power == 0) continue;
        if (getInfo(key).size() < size) continue;

        double value;

        try {
            value = std::stod(getInfo(key));
        } catch (const std::invalid_argument& e) {
            throw std::runtime_error("Value for " + key + " is invalid");
        } catch (const std::out_of_range& e) {
            throw std::runtime_error("Value for " + key + " is out of range");
        }

        std::stringstream ss;
        ss << std::scientific << std::setprecision(precision) << value;
        infoMap[key] = ss.str();
    }
}

std::string
InformationGatherer::getModelLogoPath()
{
    /* NOTE: the idea here is to have a function that keys off of the file type
     * and returns the appropriate path to its logo. But as of now, only BRL-CAD
     * files are supported by gist.
     */
    const char* BRL_LOGO_FILE = "brlLogo-nobg.png";

    char buf[MAXPATHLEN] = {0};
    if (!bu_dir(buf, MAXPATHLEN, BU_DIR_DATA, "images", BRL_LOGO_FILE, NULL)) {
        return "";
    }

    return std::string(buf);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
