/*                     O P T I O N S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2023-2026 United States Government as represented by
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

#include "bu/cmdschema.h"

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
    ncpu = 0; // all available
    verbosePrint = 0;
}

Options::~Options()
{

}

struct gist_args {
    int print_help;
    int overwrite;
    int ppi;
    int left_handed;
    int y_up;
    std::string input_file;
    std::string output_file;
    std::string input_folder;
    std::string output_folder;
    int open_gui;
    int reuse_images;
    std::string top_component;
    std::string logo_path;
    std::string preparer;
    std::string owner;
    std::string executable_dir;
    std::string classification;
    std::string notes;
    std::string density_file;
    std::string length_units;
    std::string mass_units;
    std::string working_dir;
    int ncpu;
    int verbose;
};


static int
gist_string_parse(struct bu_vls *msg, const char *arg, void *storage)
{
    if (!arg || !arg[0]) {
	if (msg)
	    bu_vls_printf(msg, "string argument expected\n");
	return -1;
    }
    if (storage)
	*((std::string *)storage) = arg;
    return 0;
}


static const struct bu_cmd_option gist_options[] = {
    BU_CMD_CUSTOM("i", "input", gist_args, input_file, gist_string_parse, "filename.g", "Input .g file"),
    BU_CMD_CUSTOM("o", "output", gist_args, output_file, gist_string_parse, "filename.png", "Output file name"),
    BU_CMD_CUSTOM("F", "folder", gist_args, input_folder, gist_string_parse, "folder", "Folder of .g models to generate"),
    BU_CMD_CUSTOM("e", "export-folder", gist_args, output_folder, gist_string_parse, "directory", "Set export folder name"),
    BU_CMD_FLAG("g", "gui", gist_args, open_gui, "Display report in a popup window"),
    BU_CMD_FLAG("f", "force", gist_args, overwrite, "Overwrite an existing report"),
    BU_CMD_FLAG("Z", "reuse-images", gist_args, reuse_images, "Reuse renders from an existing .working directory"),
    BU_CMD_CUSTOM("t", "top-component", gist_args, top_component, gist_string_parse, "component", "Specify the primary report component"),
    BU_CMD_CUSTOM("m", "logo", gist_args, logo_path, gist_string_parse, "image", "Path to logo image"),
    BU_CMD_INTEGER("p", "ppi", gist_args, ppi, "pixels", "Pixels per inch (default 300)"),
    BU_CMD_CUSTOM("n", "preparer", gist_args, preparer, gist_string_parse, "name", "Preparer name"),
    BU_CMD_CUSTOM("r", "owner", gist_args, owner, gist_string_parse, "name", "Model owner name"),
    BU_CMD_CUSTOM("T", "executables", gist_args, executable_dir, gist_string_parse, "directory", "Path to rt and rtwizard executables"),
    BU_CMD_CUSTOM("c", "classification", gist_args, classification, gist_string_parse, "text", "Classification in the report banner"),
    BU_CMD_CUSTOM("N", "notes", gist_args, notes, gist_string_parse, "text", "Additional report notes"),
    BU_CMD_FLAG("L", "left-handed", gist_args, left_handed, "Use left-handed coordinates"),
    BU_CMD_FLAG("A", "y-up", gist_args, y_up, "Use +Y-up geometry (default: +Z-up)"),
    BU_CMD_CUSTOM("d", "density-file", gist_args, density_file, gist_string_parse, "file", "Material density file, or 0 to skip density calculations"),
    BU_CMD_CUSTOM("l", "length-units", gist_args, length_units, gist_string_parse, "units", "Length units"),
    BU_CMD_CUSTOM("w", "weight-units", gist_args, mass_units, gist_string_parse, "units", "Weight units"),
    BU_CMD_CUSTOM("a", "working-directory", gist_args, working_dir, gist_string_parse, "directory", "Directory for cached work"),
    BU_CMD_INTEGER("P", "processors", gist_args, ncpu, "count", "Number of CPUs to use"),
    BU_CMD_FLAG("v", "verbose", gist_args, verbose, "Enable verbose output"),
    BU_CMD_FLAG("h", "help", gist_args, print_help, "Print help and exit"),
    BU_CMD_ALIAS_SHORT("?", "help", 1),
    BU_CMD_OPTION_NULL
};


static const struct bu_cmd_operand gist_operands[] = {
    BU_CMD_OPERAND("input", BU_CMD_VALUE_FILE, 0, 1,
	"Input .g file when --input or --folder is not used", NULL),
    BU_CMD_OPERAND_NULL
};


static const struct bu_cmd_schema gist_schema = {
    "gist",
    "Generate an image-based report from a BRL-CAD model or directory.",
    gist_options,
    gist_operands,
    BU_CMD_PARSE_INTERSPERSED,
    NULL
};

bool Options::readParameters(int argc, const char **argv) {
    const char* usage = "[options] (-F model/dir | model.g)";
    struct gist_args args = {};
    int parsed = 0;
    int operand_count = 0;
    const char **operands = NULL;
    /* Set program name before parsing and preserve the ordinary input-file
     * shorthand as the single positional operand. */
    const char* cmd_progname = argv[0];
    bu_setprogname(argv[0]);
    argc--; argv++;

    struct bu_vls msg = BU_VLS_INIT_ZERO;
    parsed = bu_cmd_schema_parse(&gist_schema, &args, &msg, argc, argv);
    if (parsed >= 0) {
        operand_count = argc - parsed;
        operands = argv + parsed;
        if (operand_count) {
            args.input_file = operands[0];
            operand_count--;
        }
    }

    if (parsed < 0) {
        bu_log("ERROR: option parsing failed\n");
        if (msg.vls_len > 0)
            bu_log("%s\n", bu_vls_cstr(&msg));
        bu_vls_free(&msg);
        return false;
    }
    if (operand_count != 0) {
        bu_log("ERROR: bad / unknown option (%s). Use -h for all options\n", operands[1]);
        bu_vls_free(&msg);
        return false;
    }
    if (args.print_help) {
        char *options_help = bu_cmd_schema_describe(&gist_schema);
        bu_log("\nUsage:  %s %s\n", cmd_progname, usage);
        bu_log("\nOptions:\n%s\n", options_help ? options_help : "");
        bu_vls_free(&msg);
        bu_free(options_help, "options help str");
        return false;
    }

    this->inFile = args.input_file;
    this->inFolderName = args.input_folder;
    this->outFolderName = args.output_folder;
    this->openGUI = args.open_gui;
    this->reuseImages = args.reuse_images;
    this->topComp = args.top_component;
    this->logopath = args.logo_path;
    this->preparer = args.preparer;
    this->owner = args.owner;
    this->exeDir = args.executable_dir;
    this->classification = args.classification;
    this->notes = args.notes;
    this->workingDir = args.working_dir;

    /* use internal setters for options that require extra attention */
    if (args.output_file.empty() && this->inFolderName.empty()) {
        // no output file supplied, and not working with folder: default to gui pop-up
        bu_log("WARNING: no output file given, use (-o) to save report\n");
        setOpenGUI();
    } else if (!setOutFile(args.output_file))    // out file was supplied, make sure .png
        return false;
    if (args.ppi)
        setPPI(args.ppi);
    if (args.left_handed)
        setCoordSystem(LEFT_HAND);
    if (args.y_up)
        setUpAxis(POS_Y_UP);
    if (!args.length_units.empty())
        setUnitLength(args.length_units);
    if (!args.mass_units.empty())
        setUnitMass(args.mass_units);
    if (!this->inFolderName.empty()) {
        setIsFolder();
        setExportToFile();  // dont want to flood screen with gui popups if we're using a folder
    }
    if (!this->exeDir.empty())
        setExeDir(this->exeDir);
    setNCPU(args.ncpu);
    if (!args.density_file.empty())
        setDensityFile(args.density_file);

    /* make sure valid input .g or folder is supplied */
    if ((getIsFolder() && !bu_file_directory(this->inFolderName.c_str())) ||
        (!getIsFolder() && !bu_file_exists(this->inFile.c_str(), NULL))) {
	// which input was given
	std::string given_input = getIsFolder() ? this->inFolderName : this->inFile;

	bu_log("Usage: %s %s\n\n", cmd_progname, usage);
	bu_log("ERROR: given input \"%s\" does not exist. Please ensure the path is correct\n", given_input.c_str());
        return false;
    }

    /* handle default folder output dir */
    if (getIsFolder() && getOutFolder().empty()) {
	setOutFolder(getInFolder() + BU_DIR_SEPARATOR + "GIST_REPORTS");
    }

    /* make sure we're not unintentionally overwriting an existing report */
    if (bu_file_exists(this->outFile.c_str(), NULL)) {
	if (!args.overwrite) {
	    bu_log("ERROR: output file \"%s\" already exists. Re-run with force flag (-f) or remove file.", this->outFile.c_str());
	    return false;
	} else {
	    bu_file_delete(this->outFile.c_str());
	}
    } else if (bu_file_directory(this->outFolderName.c_str())) {
	if (!args.overwrite) {
	    bu_log("ERROR: output folder \"%s\" already exists. Re-run with force flag (-f) or remove directory.", this->outFolderName.c_str());
	    return false;
	} else {
	    bu_dirclear(this->outFolderName.c_str());
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
	// is this file or folder run
	// if we're working with a folder, add a nested GIST_REPORTS dir for working dir
	std::string working = getIsFolder() ? getInFolder() + BU_DIR_SEPARATOR + "GIST_REPORTS" : getInFile();

	// if input file ends in .g - strip the extension
	if (working.size() > 2 && working[working.size() - 2] == '.' && working[working.size() - 1] == 'g') {
	    working.pop_back();
	    working.pop_back();
	}

	working += ".working";
	this->workingDir = working;
    }
    /* handle working directory existence */
    if (bu_file_directory(getWorkingDir().c_str()) && getReuseImages() == false) {
	// found dir, but re-use wasn't requested
	bu_log("ERROR: Working directory \"%s\" found.\n", getWorkingDir().c_str());
	bu_log("       Re-run with (-Z) to reuse work. Or remove working directory.\n");
	return false;
    } else {
	// create
	bu_mkdir(getWorkingDir().c_str());
    }
    if (verbosePrinting())
	bu_log("Writing intermediate files to working directory: %s\n\n", getWorkingDir().c_str());

    /* initialize empty output folder if needed */
    if (getIsFolder() && !bu_file_directory(this->outFolderName.c_str()))
	bu_mkdir(this->outFolderName.c_str());

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


void Options::setNCPU(int cpus) {
    ncpu = cpus;
}

void Options::setDensityFile(std::string file) {
    // special case - '0' means we want to skip density calculations (ie dont need to verify file exists)
    if (file == "0") {
	densityFile = "0";
	return;
    }

    // verify this is a path and it exists
    std::filesystem::path path = file;
    if (!std::filesystem::exists(path)) {
	bu_log("WARNING: density file (%s) not found.\n", file.c_str());
	return;
    }

    // TODO: should the setter validate the density file before setting?

    densityFile = path.lexically_normal().string();
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


size_t Options::getNCPU() {
    size_t cpus = bu_avail_cpus();

    if (ncpu < 0)
	cpus += ncpu;
    else if (ncpu > 0)
	cpus = ncpu;

    if (cpus < 1)
	return 1;
    return cpus;
}

std::string Options::getDensityFile() {
    return densityFile;
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
