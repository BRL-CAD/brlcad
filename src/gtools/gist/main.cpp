/*                        M A I N . C P P
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


/**
 * Takes in a list of parameters and updates program variables accordingly
 *
 * @argc the number of parameters
 * @argv the list of parameters
 * @opt options to be used in report generation
 * @h flag for help
 * @f flag for filepath specification
 *
 * @returns if the given parameters are valid.
 */
static bool
readParameters(int argc, char** argv, Options &opt)
{
    /*
     * A list of parameters is as follows:
     * p = filepath
     * w = width of output and window
     * l = length of output and window
     * F = path specified is a folder of models
     * g = GUI output
     * f = filename of png export
     * E = folder name of png export
     */

    bool h = false; // user requested help
    bool hasFile = 0; // user specified filepath
    bool hasFolder = false; // user specified filepath

    int opts;


    while ((opts = bu_getopt(argc, argv, "g?Oop:F:P:f:n:T:E:N:l:m:c:t:Z")) != -1) {

        switch (opts) {
            case 'p':
                hasFile = true;
                opt.setFilepath(bu_optarg);
                break;
            case 'P':
                opt.setPPI(atoi(bu_optarg));
                break;
            case 'F':
                hasFolder = true;
                opt.setIsFolder();
                opt.setFolder(bu_optarg);
                break;
            case 'E':
                opt.setExportFolder(bu_optarg);
                break;
            case 'g':
                opt.setOpenGUI();
                break;
            case 'f':
                opt.setExportToFile();
                opt.setFileName(bu_optarg);
                break;
            case 'n':
                opt.setName(bu_optarg);
                break;
            case 'T':
                opt.setTemppath(bu_optarg);
                break;
            case 'c':
                opt.setClassification(bu_optarg);
                break;
            case 'o':
                opt.setOrientationRightLeft(true);
                break;
            case 'O':
                opt.setOrientationZYUp(true);
                break;
            case 'N':
                opt.setNotes(bu_optarg);
                break;
            case 'Z':
                opt.setOverrideImages();
                break;
            case 't':
                opt.setTopComp(bu_optarg);
                std::cout << "Top component: " << opt.getTopComp() << std::endl;
                break;
            case 'l':
                opt.setUnitLength(bu_optarg);
                break;
            case 'm':
                opt.setUnitMass(bu_optarg);
                break;
            case '?':
                h = true;
                break;
            default:
                std::cerr << "Unknown option\n";
                break;
        }
    }

    if (h) {
        bu_log("\nUsage:  %s [options] -p path/to/model.g\n", argv[0]);
        bu_log("\nOptions:\n");
        bu_log("    p = filepath\n");
        bu_log("    P = pixels per inch, default is 300ppi\n");
        bu_log("    F = path specified is a folder of models\n");
        bu_log("    E = path to folder to export reports. Used for processing folder of models\n");
        bu_log("    g = GUI output\n");
        bu_log("    f = filepath of png export, MUST end in .png\n");
        bu_log("    n = name of preparer, to be used in report\n");
        bu_log("    T = directory where rt and rtwizard executables are stored\n");
        bu_log("    c = classification of a file, to be displayed in uppercase on top and bottom of report. If the classification is \"confidential\", the header and footer will be red.\n");
        bu_log("    o = orientation of the file, default is right hand, flag will change orientation output to left hand");
        bu_log("    O = orientation of the file, default is +Z-up, flag will change orientation output to +Y-up");
        bu_log("    N = notes that a user would like to add to be specified in the report");
        bu_log("    Z = option to re-use pre-made renders in the output folder.  Should only be used when running on the same model multiple times.");
        bu_log("    t = option to specify the top component of the report. Useful when there are multiple tops");
        bu_log("    l = override the default length units in a file.");
        bu_log("    m = override the default mass units in a file.");
        return false;
    }
    //If user has no arguments or did not specify filepath, give shortened help
    else if (argc < 2 || (hasFolder == hasFile)) {
        bu_log("\nUsage:  %s [options] -p path/to/model.g\n", argv[0]);
        bu_log("\nPlease specify the path to the file for report generation, use flag \"-?\" to see all options\n");
        return false;
    } else if (!bu_file_exists(opt.getFilepath().c_str(), NULL)) {
        bu_log("ERROR: %s doesn't exist\n", opt.getFilepath().c_str());
        bu_exit(BRLCAD_ERROR, "No input, aborting.\n");
    } else if (bu_file_exists(opt.getFileName().c_str(), NULL)) {
        std::cout << "File already exists: " << opt.getFileName() << std::endl;
        std::cout << "Overwrite? (y/n): ";
        char token; std::cin >> token;
        if (token != 'y' && token != 'Y')
            return false;
    }

    return true;
}


/**
 * Calls the necessary functions to generate the reports.
 *
 * @opt options to be used in report generation
 */
static void
generateReport(Options opt)
{
    // create image frame
    IFPainter img(opt.getLength(), opt.getWidth());

    // create information gatherer
    InformationGatherer info(&opt);

    // read in all information from model file
    if (!info.gatherInformation(opt.getName())) {
        std::cerr << "Error on Information Gathering.  Report Generation skipped..." << std::endl;
        return;
    }

    // Define commonly used ratio variables
    int margin = opt.getWidth() / 150;
    int header_footer_height = opt.getLength() / 25;
    int padding = opt.getLength() / 250;
    int vvHeight = (opt.getLength() - 2*header_footer_height - 2*margin) / 3;


    // Has same height and width as V&V Checks, offset X by V&V checks width
    // makeHeirarchySection(img, info, XY_margin + vvSectionWidth + (opt.getLength() / 250), vvOffsetY, vvSectionWidth, vvSectionHeight, opt);

    // define the position of all sections in the report
    Position imagePosition(0, 0, opt.getWidth(), opt.getLength());
    Position topSection(margin, margin, imagePosition.width() - 2*margin, header_footer_height);
    Position bottomSection(margin, imagePosition.bottom() - header_footer_height - margin, imagePosition.width() - 2*margin, header_footer_height);
    Position hierarchySection(imagePosition.right() - imagePosition.thirdWidth() - margin, imagePosition.height() - margin - header_footer_height - padding - vvHeight, imagePosition.thirdWidth(), vvHeight);
    Position fileSection(imagePosition.right() - imagePosition.sixthWidth() - margin, topSection.bottom() + padding, imagePosition.sixthWidth(), hierarchySection.top() - topSection.bottom() - padding);
    Position renderSection(margin, topSection.bottom() + padding, fileSection.left() - margin - padding, bottomSection.top() - topSection.bottom() - 2*padding);

    // draw all sections
    makeTopSection(img, info, topSection.x(), topSection.y(), topSection.width(), topSection.height());
    makeBottomSection(img, info, bottomSection.x(), bottomSection.y(), bottomSection.width(), bottomSection.height());
    makeFileInfoSection(img, info, fileSection.x(), fileSection.y(), fileSection.width(), fileSection.height(), opt);
    makeRenderSection(img, info, renderSection.x(), renderSection.y(), renderSection.width(), renderSection.height(), opt);
    makeHeirarchySection(img, info, hierarchySection.x(), hierarchySection.y(), hierarchySection.width(), hierarchySection.height(), opt);

    // paint renderings

    // optionally, display the scene
    if (opt.getOpenGUI()) {
        img.openInGUI();
    }

    // optionally, export the image
    if (opt.getExportToFile()) {
        img.exportToFile(opt.getFileName());
    }
}


/**
 * Checks if we are processing a folder of models
 */
static void
handleFolder(Options& options)
{
    int cnt = 1;

    for (const auto & entry : std::filesystem::directory_iterator(options.getFolder())) {
        options.setFilepath(entry.path().string());
        options.setExportToFile();
        std::string filename = options.getFilepath();
        filename = filename.substr(filename.find_last_of("/\\") + 1);
        filename = filename.substr(0, filename.find_last_of("."));
        std::cout << "Processing: " << filename << std::endl;
        std::string exportPath = options.getExportFolder() + "/" + filename + "_report.png";
        options.setFileName(exportPath);
        generateReport(options);
        std::cout << "Finished Processing: " << cnt++ << std::endl;
    }
}


int
main(int argc, char **argv)
{
    Options options;

    if (readParameters(argc, argv, options)) {
        if (options.getIsFolder()) {
            handleFolder(options);
        } else {
            generateReport(options);
        }
    }
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
