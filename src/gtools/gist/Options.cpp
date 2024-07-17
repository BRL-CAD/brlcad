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
    inFile = "";
    exeDir = "";
    workingDir = "";
    ppi = 300;
    width = 3508;
    length = 2480;
    isFolder = false;
    inFolderName = "";
    openGUI = false;
    exportToFile = false;
    reuseImages = false;
    outFolderName = "";
    outFile = "";
    preparer = "N/A";
    classification = "";
    coordinate_system = RIGHT_HAND;
    axis_orientation = POS_Z_UP;
    notes = "N/A";
    uLength = "m";
    originalUnitsLength = true;
    uMass = "g";
    originalUnitsMass = true;
    verbosePrint = 0;
}

Options::~Options()
{

}

static int _param_set_std_str(struct bu_vls *msg, size_t argc, const char **argv, void *set_var) {
    std::string* s_set = (std::string*)set_var;

    BU_OPT_CHECK_ARGV0(msg, argc, argv, "_param_set_std_str");

    if (s_set)
	*s_set = std::string(argv[0]);
    return 1;
}

bool Options::readParameters(int argc, const char **argv) {
    const char* usage = "[options] (-F model/dir | model.g)";

    int print_help = 0;			// user requested help
    int param_overwrite = 0;		// user requested to overwrite existing report
    int param_ppi = 0;			// user requested ppi
    int param_Lhand = 0;		// user requested left-handed coordinates
    int param_Yup = 0;			// user requested y-up
    std::string param_Ulength = "";	// user requested length units
    std::string param_Umass = "";	// user requested mass units
    std::string param_oFile = "";	// user supplied output file

    struct bu_opt_desc d[23];
    BU_OPT(d[0],  "i", "",     "filename.g",        &_param_set_std_str,     &this->inFile,         "input .g");
    BU_OPT(d[1],  "o", "",     "filename.png",      &_param_set_std_str,     &param_oFile,          "output file name");
    BU_OPT(d[2],  "F", "",     "folder",            &_param_set_std_str,     &this->inFolderName,   "folder of .g models to generate");
    BU_OPT(d[3],  "e", "",     "dir_name",          &_param_set_std_str,     &this->outFolderName,  "set export folder's name");
    BU_OPT(d[4],  "g", "",     "",                  NULL,                    &this->openGUI,        "display report in popup window");
    BU_OPT(d[5],  "f", "",     "",                  NULL,                    &param_overwrite,      "overwrite existing report if it exists");
    BU_OPT(d[6],  "Z", "",     "",                  NULL,                    &this->reuseImages,    "reuse renders if .working directory is found");
    BU_OPT(d[7],  "t", "",     "component",         &_param_set_std_str,     &this->topComp,        "specify primary component of the report");
    BU_OPT(d[8],  "m", "",     "path/to/image",     &_param_set_std_str,     &this->logopath,       "path to logo image to be used in report");
    BU_OPT(d[9],  "p", "",     "#",		    &bu_opt_int,             &param_ppi,            "pixels per inch (default 300ppi)");
    BU_OPT(d[10], "n", "",     "\"preparer name\"", &_param_set_std_str,     &this->preparer,       "name of preparer, to be used in report");
    BU_OPT(d[11], "r", "",     "\"owner name\"",    &_param_set_std_str,     &this->owner,          "name of model's owner, to be used in report");
    BU_OPT(d[12], "T", "",     "path/to/rt",        &_param_set_std_str,     &this->exeDir,         "path to rt and rtwizard executables");
    BU_OPT(d[13], "c", "",     "classification",    &_param_set_std_str,     &this->classification, "classification displayed in top/bottom banner");
    BU_OPT(d[14], "N", "",     "\"extra notes\"",   &_param_set_std_str,     &this->notes,          "add additional notes to report");
    BU_OPT(d[15], "L", "",     "",                  NULL,                    &param_Lhand,          "use left-handed coordinate system");
    BU_OPT(d[16], "A", "",     "",                  NULL,                    &param_Yup,            "use +Y-up geometry axis (default is +Z-up)");
    BU_OPT(d[17], "l", "",     "len_units",         &_param_set_std_str,     &param_Ulength,        "specify length units");
    BU_OPT(d[18], "w", "",     "wt_units",          &_param_set_std_str,     &param_Umass,          "specify weight units");
    BU_OPT(d[19], "v", "",     "",                  NULL,                    &this->verbosePrint,   "verbose printing");
    BU_OPT(d[20], "h", "help", "",                  NULL,                    &print_help,           "Print help and exit");
    BU_OPT(d[21], "?", "",     "",                  NULL,                    &print_help,           "");
    BU_OPT_NULL(d[22]);

    /* set progname and move on */
    const char* cmd_progname = argv[0];
    bu_setprogname(argv[0]);
    argc--; argv++;

    /* parse options */
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int ret_ac = bu_opt_parse(&msg, argc, argv, d);

    if (ret_ac) {   // asssume a leftover option is a .g file
	setInFile(argv[0]);
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
    if (param_oFile.empty()) {	// no output file supplied, default to gui pop-up
	bu_log("WARNING: no output file given, use (-o) to save report\n");
	setOpenGUI();
    } else if (!setOutFile(param_oFile))    // out file was supplied, make sure .png
	return false;
    if (param_ppi)
	setPPI(param_ppi);
    if (param_Lhand)
	setCoordSystem(LEFT_HAND);
    if (param_Yup)
	setUpAxis(POS_Y_UP);
    if (!param_Ulength.empty())
	setUnitLength(param_Ulength);
    if (!param_Umass.empty())
	setUnitMass(param_Umass);
    if (!this->inFolderName.empty())
	setIsFolder();
    if (!this->exeDir.empty())
	setExeDir(this->exeDir);

    /* make sure valid input .g or folder is supplied */
    if ((getIsFolder() && !bu_file_directory(this->inFolderName.c_str())) ||
        !bu_file_exists(this->inFile.c_str(), NULL)) {
	// which input was given
	std::string given_input = getIsFolder() ? this->inFolderName : this->inFile;

	bu_log("Usage: %s %s\n\n", cmd_progname, usage);
	bu_log("ERROR: given input \"%s\" does not exist. Please ensure the path is correct\n", given_input.c_str());
        return false;
    }

    /* make sure we're not unintentionally overwriting an existing report */
    if (bu_file_exists(this->outFile.c_str(), NULL)) {
	if (!param_overwrite) {
	    bu_log("ERROR: output file \"%s\" already exists. Re-run with force flag (-f) or remove file.", this->outFile.c_str());
	    return false;
	} else {
	    bu_file_delete(this->outFile.c_str());
	}
    }

    /* validate executable directory */
    // NOTE: assumes if we can find mged, all other needed executables are there as well
    if (!this->exeDir.empty()) {
	// user specified a directory with -T
	if (!bu_dir(NULL, 0, this->exeDir.c_str(), "mged", BU_DIR_EXT, NULL)) {
	    // couldn't find mged in exeDir
	    bu_log("ERROR: File to executables (%s) is invalid\nPlease use (or check) the (-T) parameter\n",this->exeDir.c_str());
	    return false;
	}
    } else {
	// see if we can find executables
	if (bu_dir(NULL, 0, BU_DIR_BIN, "mged", BU_DIR_EXT, NULL)) {
	    exeDir = std::string(bu_dir(NULL, 0, BU_DIR_BIN, NULL));
	} else {
	    // couldn't find mged in BU_DIR_BIN
	    bu_log("ERROR: Could not find BRL-CAD executables.\nPlease use (or check) the (-T) parameter\n");
	    return false;
	}
    }

    /* build working directory path if not supplied */
    if (getWorkingDir().empty()) {
	std::string working = getInFile();
	// if input file ends in .g - strip the g and replace with .working
	if (working.size() > 2 && working[working.size() - 2] == '.' && working[working.size() - 1] == 'g')
	    working.pop_back();
	working += "working";
	this->workingDir = working;
    }
    /* handle working directory existence */
    if (bu_file_directory(getWorkingDir().c_str())) {
	// found dir, should we have?
	if (getReuseImages() == false) {
	    bu_log("ERROR: Working directory (%s) found.\n", getWorkingDir().c_str());
	    bu_log("       Re-run with (-Z) to re-use work. Or remove working directory \"%s\".\n", getWorkingDir().c_str());
	    return false;
	}
	if (verbosePrinting())
	    bu_log("Using working directory: %s\n\n", getWorkingDir().c_str());
    } else {
	// create
	bu_mkdir(getWorkingDir().c_str());
	if (verbosePrinting())
	    bu_log("Intermediate files writing to: %s\n\n", getWorkingDir().c_str());
    }

    return true;
}


void Options::setInFile(std::string f) {
    inFile = f;
}


void Options::setExeDir(std::string f) {
    if (f[f.size() - 1] != '/' && f[f.size() - 1] != '\\')
	f = f + '\\';
    exeDir = f;
}

void
Options::setLogopath(std::string f)
{
	logopath = f;
}

void Options::setPPI(int p) {
    ppi = p;
    // TODO: magic numbers?
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


void Options::setReuseImages(bool reuse) {
    reuseImages = reuse;
}


void Options::setOutFolder(std::string fldr) {
    outFolderName = fldr;
}


void Options::setInFolder(std::string n) {
    inFolderName = n;
}


bool Options::setOutFile(std::string n) {
    // output file must be png
    auto dot = n.find_last_of('.');
    if (dot != std::string::npos) {
	// extension given, ensure it's png
	const std::string ext = n.substr(dot + 1);
	if (ext != "png") {
	    bu_log("ERROR: output file must be .png\n");
	    return false;
	}

	outFile = n;
    } else {
	// no extension given - append .png
	outFile = n + ".png";
    }

    setExportToFile();
    return true;
}


void Options::setPreparer(std::string n) {
    preparer = n;
}

void Options::setOwner(std::string n)
{
    owner = n;
}

void Options::setClassification(std::string c) {
    classification = c;
}


void Options::setCoordSystem(coord_system sys) {
    coordinate_system = sys;
}


void Options::setUpAxis(up_axis axis) {
    axis_orientation = axis;
}


void Options::setNotes(std::string n) {
    notes = n;
}


void Options::setTopComp(std::string t) {
    topComp = t;
}
void Options::setUnitLength(std::string l) {
    uLength = l;
    originalUnitsLength = false;
}


void Options::setUnitMass(std::string m) {
    uMass = m;
    originalUnitsMass = false;
}


std::string Options::getInFile() {
    return inFile;
}

std::string Options::getWorkingDir() {
    return workingDir;
}
std::string Options::getLogopath()
{
	return logopath;
}

std::string Options::getInFolder() {
    return inFolderName;
}


std::string Options::getOutFolder() {
    return outFolderName;
}


std::string Options::getExeDir() {
    return exeDir;
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


bool Options::getReuseImages() {
    return reuseImages;
}


std::string Options::getOutFile() {
    return outFile;
}


std::string Options::getPreparer() {
    return preparer;
}

std::string Options::getOwner()
{
    return owner;
}

std::string Options::getClassification()
{
    return classification;
}


Options::coord_system Options::getCoordSystem() {
    return coordinate_system;
}

std::string Options::getCoordSystem_asString() {
    switch (coordinate_system) {
	case RIGHT_HAND:
	    return "Right Hand";
	case LEFT_HAND:
	    return "Left Hand";
	default:
	    return "";
    }
}


Options::up_axis Options::getUpAxis() {
    return axis_orientation;
}

std::string Options::getUpAxis_asString() {
    switch (axis_orientation) {
	case POS_Y_UP:
	    return "+Y-up";
	case POS_Z_UP:
	    return "+Z-up";
	default:
	    return "";
    }
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


bool Options::isOriginalUnitsLength() {
    return originalUnitsLength;
}


bool Options::isOriginalUnitsMass() {
    return originalUnitsMass;
}

bool Options::verbosePrinting() {
    return verbosePrint;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
