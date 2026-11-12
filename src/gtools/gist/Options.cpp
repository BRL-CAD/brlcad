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

Options::Options()
{
    inFile = "";
    exeDir = "";
    workingDir = "";
    ppi = DEFAULT_REPORT_PPI;
    width = CANONICAL_REPORT_WIDTH;
    length = CANONICAL_REPORT_LENGTH;
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
    previewMode = 0;
}

Options::~Options()
{

}

struct gist_args {
    const char *input_file;
    const char *output_file;
    const char *input_folder;
    const char *output_folder;
    int open_gui;
    int overwrite;
    int reuse_images;
    const char *top_component;
    const char *logo_path;
    int ppi;
    const char *preparer;
    const char *owner;
    const char *executable_dir;
    const char *classification;
    const char *notes;
    int left_handed;
    int y_up;
    const char *density_file;
    const char *length_units;
    const char *mass_units;
    const char *working_dir;
    int cpu_count;
    int preview;
    int verbose;
    int print_help;
};

static const struct bu_arg_option gist_options[] = {
    BU_ARG_VALUE("i", "input", struct gist_args, input_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "filename.g", "Input database"),
    BU_ARG_VALUE("o", "output", struct gist_args, output_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "filename.png", "Output report filename"),
    BU_ARG_VALUE("F", "folder", struct gist_args, input_folder,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "folder", "Folder of databases to process"),
    BU_ARG_VALUE("e", "output-folder", struct gist_args, output_folder,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "dir_name", "Output folder name"),
    BU_ARG_FLAG("g", "gui", struct gist_args, open_gui, "Display report in a popup window"),
    BU_ARG_FLAG("f", "force", struct gist_args, overwrite, "Overwrite an existing report"),
    BU_ARG_FLAG("Z", "reuse-images", struct gist_args, reuse_images,
	"Reuse renders from an existing working directory"),
    BU_ARG_VALUE("t", "top-component", struct gist_args, top_component,
	BU_ARG_VALUE_DB_OBJECT, BU_ARG_REQUIRED, "component", "Primary report component"),
    BU_ARG_VALUE("m", "logo", struct gist_args, logo_path,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "image", "Logo image for the report"),
    BU_ARG_INTEGER_RANGE("p", "ppi", struct gist_args, ppi,
	1, 1, 0, 0, "#", "Pixels per inch"),
    BU_ARG_VALUE("n", "preparer", struct gist_args, preparer,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "name", "Report preparer name"),
    BU_ARG_VALUE("r", "owner", struct gist_args, owner,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "name", "Model owner name"),
    BU_ARG_VALUE("T", "executables", struct gist_args, executable_dir,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "directory", "Directory containing BRL-CAD executables"),
    BU_ARG_VALUE("c", "classification", struct gist_args, classification,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "classification", "Report banner classification"),
    BU_ARG_VALUE("N", "notes", struct gist_args, notes,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "text", "Additional report notes"),
    BU_ARG_FLAG("L", "left-handed", struct gist_args, left_handed,
	"Use a left-handed coordinate system"),
    BU_ARG_FLAG("A", "y-up", struct gist_args, y_up, "Use +Y as the up axis"),
    BU_ARG_VALUE("d", "density-file", struct gist_args, density_file,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file|0", "Material density file, or 0 to skip densities"),
    BU_ARG_VALUE("l", "length-units", struct gist_args, length_units,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "units", "Length units"),
    BU_ARG_VALUE("w", "mass-units", struct gist_args, mass_units,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "units", "Mass units"),
    BU_ARG_VALUE("a", "working-directory", struct gist_args, working_dir,
	BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "directory", "Directory for cached work"),
    BU_ARG_INTEGER_RANGE("P", "cpu-count", struct gist_args, cpu_count,
	1, 0, 0, 0, "#", "Number of CPUs to use (0 selects all available)"),
    BU_ARG_FLAG("Q", "preview", struct gist_args, preview,
	"Use lower render quality and skip verification"),
    BU_ARG_FLAG("v", "verbose", struct gist_args, verbose, "Print verbose status"),
    BU_ARG_FLAG("h", "help", struct gist_args, print_help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand gist_operands[] = {
    BU_ARG_OPERAND("model.g", BU_ARG_VALUE_FILE, 0, 1, "Input database", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_schema gist_schema = BU_ARG_SCHEMA(
    "gist", "Generate a geometry information summary report", gist_options,
    gist_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL));

static void
gist_help(const char *program)
{
    char *help = bu_arg_schema_help(&gist_schema, program);
    if (help)
	bu_log("%s", help);
    bu_free(help, "options help str");
}

bool Options::readParameters(int argc, const char **argv) {
    const char* usage = "[options] (-F model/dir | model.g)";
    struct gist_args args = {};
    args.ppi = -1;

    /* set progname and move on */
    const char* cmd_progname = argv[0];
    bu_setprogname(argv[0]);
    argc--; argv++;

    /* parse options */
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int operand_index = bu_arg_schema_parse_complete(&gist_schema, &args,
	&msg, argc, argv);

    /* usage help */
    if (operand_index < 0) {
	bu_log("ERROR: option parsing failed\n");
	if (msg.vls_len > 0)
	    bu_log("%s", bu_vls_cstr(&msg));

	bu_vls_free(&msg);
	return false;
    }
    bu_vls_free(&msg);
    if (args.print_help) {
	gist_help(cmd_progname);
	return false;
    }

    argc -= operand_index;
    argv += operand_index;
    int input_count = (args.input_file ? 1 : 0) +
	(args.input_folder ? 1 : 0) + argc;
    if (input_count != 1) {
	bu_log("Usage: %s %s\n\n", cmd_progname, usage);
	bu_log("ERROR: specify exactly one input database or folder\n");
	return false;
    }

    if (args.input_file)
	setInFile(args.input_file);
    else if (args.input_folder)
	setInFolder(args.input_folder);
    else
	setInFile(argv[0]);
    if (args.output_folder)
	setOutFolder(args.output_folder);
    if (args.open_gui)
	setOpenGUI();
    setReuseImages(args.reuse_images != 0);
    if (args.top_component)
	setTopComp(args.top_component);
    if (args.logo_path)
	setLogopath(args.logo_path);
    if (args.preparer)
	setPreparer(args.preparer);
    if (args.owner)
	setOwner(args.owner);
    if (args.executable_dir)
	setExeDir(args.executable_dir);
    if (args.classification)
	setClassification(args.classification);
    if (args.notes)
	setNotes(args.notes);
    if (args.working_dir)
	workingDir = args.working_dir;
    previewMode = args.preview;
    verbosePrint = args.verbose;

    const std::string output_file = args.output_file ? args.output_file : "";
    const std::string length_units = args.length_units ? args.length_units : "";
    const std::string mass_units = args.mass_units ? args.mass_units : "";
    const std::string density_file = args.density_file ? args.density_file : "";

    /* use internal setters for options that require extra attention */
    if (output_file.empty() && this->inFolderName.empty()) {
	// no output file supplied, and not working with folder: default to gui pop-up
	bu_log("WARNING: no output file given, use (-o) to save report\n");
	setOpenGUI();
    } else if (!setOutFile(output_file))    // out file was supplied, make sure .png
	return false;
    if (args.ppi != -1)
	setPPI(args.ppi);
    if (args.left_handed)
	setCoordSystem(LEFT_HAND);
    if (args.y_up)
	setUpAxis(POS_Y_UP);
    if (!length_units.empty())
	setUnitLength(length_units);
    if (!mass_units.empty())
	setUnitMass(mass_units);
    if (!this->inFolderName.empty()) {
	setIsFolder();
	setExportToFile();  // dont want to flood screen with gui popups if we're using a folder
    }
    setNCPU(args.cpu_count);
    if (!density_file.empty())
	setDensityFile(density_file);

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
    width = static_cast<int>(std::lround(
	static_cast<double>(CANONICAL_REPORT_WIDTH) * ppi / DEFAULT_REPORT_PPI));
    length = static_cast<int>(std::lround(
	static_cast<double>(CANONICAL_REPORT_LENGTH) * ppi / DEFAULT_REPORT_PPI));
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

bool Options::getPreviewMode() {
    return previewMode;
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
