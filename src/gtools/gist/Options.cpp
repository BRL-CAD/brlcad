/*                     O P T I O N S . C P P
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

#include "Options.h"


Options::Options()
{
    filepath = "";
    temppath = "../../../build/bin/";
    ppi = 300;
    width = 3508;
    length = 2480;
    isFolder = false;
    folderName = "";
    openGUI = false;
    exportToFile = false;
    overrideImages = false;
    exportFolderName = "";
    fileName = "";
    name = "N/A";
    classification = "";
    rightLeft = "Right hand";
    zY = "+Z-up";
    notes = "N/A";
    uLength = "m";
    defaultLength = true;
    uMass = "g";
    defaultMass = true;
}


Options::~Options()
{

}

int _param_set_std_str(struct bu_vls *msg, size_t argc, const char **argv, void *set_var) {
    std::string* s_set = (std::string*)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "bu_opt_str");

    if (s_set)
	*s_set = std::string(argv[0]);
    return 1;
}

bool Options::readParameters(int argc, const char **argv) {
    const char* usage = "[options] model.g";

    int print_help = 0;			// user requested help
    int param_overwrite = 0;		// user requested to overwrite existing report
    int param_ppi = 0;			// user requested ppi
    int param_Lhand = 0;		// user requested left-handed coordinates
    int param_Yup = 0;			// user requested y-up
    std::string param_Ulength = "";	// user requested length units
    std::string param_Umass = "";	// user requested mass units

    struct bu_opt_desc d[20];
    BU_OPT(d[0],  "p", "",     "filepath",        &_param_set_std_str,     &this->filepath,          "filepath to .g");
    BU_OPT(d[1],  "P", "",     "#",		  &bu_opt_int,             &param_ppi,               "pixels per inch (default 300ppi)");
    BU_OPT(d[2],  "F", "",     "path",            &_param_set_std_str,     &this->folderName,        "path to folder of .g models to generate");
    BU_OPT(d[3],  "E", "",     "dir_name",        &_param_set_std_str,     &this->exportFolderName,  "set export folder's name");
    BU_OPT(d[4],  "g", "",     "",                NULL,                    &this->openGUI,           "GUI output");
    BU_OPT(d[5],  "f", "",     "filepath",        &_param_set_std_str,     &this->fileName,          "set output name, MUST end in .png");
    BU_OPT(d[6],  "n", "",     "preparer_name",   &_param_set_std_str,     &this->name,              "name of preparer, to be us in report");
    BU_OPT(d[7],  "T", "",     "path/to/rt",      &_param_set_std_str,     &this->temppath,          "path to rt and rtwizard executables");
    BU_OPT(d[8],  "c", "",     "classification",  &_param_set_std_str,     &this->classification,    "classification of generated report, displayed in top/bottom banner");
    BU_OPT(d[9],  "o", "",     "",                NULL,                    &param_Lhand,             "use left-handed coordinate system (default is right-handed)");
    BU_OPT(d[10], "O", "",     "",                NULL,                    &param_Yup,               "use +Y-up geometry axis (default is +Z-up)");
    BU_OPT(d[11], "N", "",     "notes..",         &_param_set_std_str,     &this->notes,             "add additional notes to report");
    BU_OPT(d[12], "Z", "",     "",                NULL,                    &this->overrideImages,    "reuse previous renders, when running on the same model multiple times");
    BU_OPT(d[13], "r", "",     "",                NULL,                    &param_overwrite,         "overwrite existing report if it exists");
    BU_OPT(d[14], "t", "",     "top_component",   &_param_set_std_str,     &this->topComp,           "specify primary \"top\" component of the model");
    BU_OPT(d[15], "l", "",     "unit_str",        &_param_set_std_str,     &param_Ulength,           "specify length units");
    BU_OPT(d[16], "m", "",     "unit_str",        &_param_set_std_str,     &param_Umass,             "specify mass units");
    BU_OPT(d[17], "h", "help", "",                NULL,                    &print_help,              "Print help and exit");
    BU_OPT(d[18], "?", "",     "",                NULL,                    &print_help,              "");
    BU_OPT_NULL(d[19]);

    /* set progname and move on */
    const char* cmd_progname = argv[0];
    bu_setprogname(argv[0]);
    argc--; argv++;

    /* parse options */
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int ret_ac = bu_opt_parse(&msg, argc, argv, d);

    if (ret_ac) {   // asssume a leftover option is a .g file
	setFilepath(argv[0]);
	ret_ac--;
    }

    /* usage help */
    if (ret_ac != 0) {
	bu_log("%s\n", bu_vls_cstr(&msg));

	bu_vls_free(&msg);
	return false;
    }
    if (print_help) {
	char *options_help = bu_opt_describe(d, NULL);

        bu_log("\nUsage:  %s %s\n", cmd_progname, usage);
        bu_log("\nOptions:\n%s\n", options_help);

	bu_vls_free(&msg);
        bu_free(options_help, "options help str");
	return false;
    }

    /* use internal setters for options that require extra attention */
    if (param_ppi)
	setPPI(param_ppi);
    if (param_Lhand)
	setOrientationRightLeft(true);
    if (!param_Ulength.empty())
	setUnitLength(param_Ulength);
    if (!param_Umass.empty())
	setUnitMass(param_Umass);
    if (!this->folderName.empty())
	setIsFolder();
    if (!this->fileName.empty())
	setExportToFile();
    if (!this->temppath.empty())
	setTemppath(this->temppath);

    /* make sure we have a .g file or folder specified */
    if (!getIsFolder() && !bu_file_exists(this->filepath.c_str(), NULL)) {
	bu_log("\nUsage:  %s %s\n", cmd_progname, usage);
        bu_log("\nPlease specify the path to the file for report generation, use flag \"-?\" to see all options\n");
        return false;
    }

    /* make sure input .g exists */
    if (!bu_file_exists(this->filepath.c_str(), NULL)) {
	bu_log("ERROR: %s does not exist\n", this->filepath.c_str());
	return false;
    }

    /* make sure we're not unintentionally overwriting an existing report */
    if (!param_overwrite && bu_file_exists(this->fileName.c_str(), NULL)) {
	bu_log("output file \"%s\" already exists. Re-run with overwrite flag (-m) or remove file.", this->fileName.c_str());
	return false;
    }


    return true;
}


void Options::setFilepath(std::string f) {
    filepath = f;
}


void Options::setTemppath(std::string f) {
    if (f[f.size() - 1] != '/' && f[f.size() - 1] != '\\')
	f = f + '\\';
    temppath = f;
}


void Options::setPPI(int p) {
    ppi = p;
    width = int(ppi * 11.69);
    length = int(ppi * 8.27);
}


void Options::setIsFolder() {
    isFolder = true;
}


void Options::setOpenGUI() {
    openGUI = true;
}


void Options::setExportToFile() {
    exportToFile = true;
}


void Options::setOverrideImages() {
    overrideImages = true;
}


void Options::setExportFolder(std::string fldr) {
    exportFolderName = fldr;
}


void Options::setFolder(std::string n) {
    folderName = n;
}


void Options::setFileName(std::string n) {
    fileName = n;
}


void Options::setName(std::string n) {
    name = n;
}


void Options::setClassification(std::string c) {
    classification = c;
}


void Options::setOrientationRightLeft(bool rL) {
    if (rL) {
	rightLeft = "Left hand";
    }
}


void Options::setOrientationZYUp(bool zy) {
    if (zy) {
	zY = "+Y-up";
    }
}


void Options::setNotes(std::string n) {
    notes = n;
}


void Options::setTopComp(std::string t) {
    topComp = t;
}
void Options::setUnitLength(std::string l) {
    uLength = l;
    defaultLength = false;
}


void Options::setUnitMass(std::string m) {
    uMass = m;
    defaultMass = false;
}


std::string Options::getFilepath() {
    return filepath;
}


std::string Options::getFolder() {
    return folderName;
}


std::string Options::getExportFolder() {
    return exportFolderName;
}


std::string Options::getTemppath() {
    return temppath;
}


int Options::getWidth() {
    return width;
}


int Options::getLength() {
    return length;
}


bool Options::getIsFolder() {
    return isFolder;
}


bool Options::getOpenGUI() {
    return openGUI;
}


bool Options::getExportToFile() {
    return exportToFile;
}


bool Options::getOverrideImages() {
    return overrideImages;
}


std::string Options::getFileName() {
    return fileName;
}


std::string Options::getName() {
    return name;
}


std::string Options::getClassification() {
    return classification;
}


std::string Options::getOrientationRightLeft() {
    return rightLeft;
}


std::string Options::getOrientationZYUp() {
    return zY;
}


std::string Options::getNotes() {
    return notes;
}


std::string Options::getTopComp() {
    return topComp;
}


std::string Options::getUnitLength() {
    return uLength;
}


std::string Options::getUnitMass() {
    return uMass;
}


bool Options::isDefaultLength() {
    return defaultLength;
}


bool Options::isDefaultMass() {
    return defaultMass;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
