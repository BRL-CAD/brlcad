/*                        M A I N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2023-2025 United States Government as represented by
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

/* Wrap bu_log to clean up code */
static void
statusPrint(std::string msg, int verbosePrinting)
{
    if (verbosePrinting)
        bu_log("%s\n", msg.c_str());
}

/**
 * Calls the necessary functions to generate the reports.
 *
 * @opt options to be used in report generation
 */
void
generateReport(Options opt)
{
    // create image frame
    IFPainter img(opt.getLength(), opt.getWidth());

    // create information gatherer
    InformationGatherer info(&opt);

    // read in all information from model file
    statusPrint("*Gathering file information: " + opt.getInFile(), opt.verbosePrinting());
    if (!info.gatherInformation(opt.getPreparer())) {
        bu_log("ERROR: Information gathering failed.\n");
        return;
    }
    statusPrint("...Finished gathering information", opt.verbosePrinting());

    // Correct units and measurement presentation
    if (opt.isOriginalUnitsLength()) {
        info.correctDefaultUnitsLength();
    }
    if (opt.isOriginalUnitsMass()) {
        info.correctDefaultUnitsMass();
    }

    info.checkScientificNotation();

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
    Position hierarchySection(imagePosition.right() - imagePosition.width()/3.5 - margin, imagePosition.height() - margin - header_footer_height - padding - vvHeight, imagePosition.width()/3.5, vvHeight);
    Position fileSection(imagePosition.right() - imagePosition.sixthWidth() - margin, topSection.bottom() + padding, imagePosition.sixthWidth(), hierarchySection.top() - topSection.bottom() - padding);
    Position renderSection(margin, topSection.bottom() + padding, fileSection.left() - margin - padding, bottomSection.top() - topSection.bottom() - 2*padding);

    // draw all sections
    statusPrint("*Creating report sections", opt.verbosePrinting());
    makeTopSection(img, info, topSection.x(), topSection.y(), topSection.width(), topSection.height());
    makeBottomSection(img, info, bottomSection.x(), bottomSection.y(), bottomSection.width(), bottomSection.height());
    statusPrint("    *Raytracing main renderings", opt.verbosePrinting());
    makeRenderSection(img, info, renderSection.x(), renderSection.y(), renderSection.width(), renderSection.height(), opt);
    statusPrint("    ...Finished renderings", opt.verbosePrinting());
    statusPrint("    *Writing file information", opt.verbosePrinting());
    makeFileInfoSection(img, info, fileSection.x(), fileSection.y(), fileSection.width(), fileSection.height(), opt);
    statusPrint("    ...Finished writing", opt.verbosePrinting());
    statusPrint("    *Building hierarchy graphic", opt.verbosePrinting());
    makeHierarchySection(img, info, hierarchySection.x(), hierarchySection.y(), hierarchySection.width(), hierarchySection.height(), opt);
    statusPrint("    ...Finished hierarchy", opt.verbosePrinting());
    statusPrint("    *Drawing Logos", opt.verbosePrinting());
    //brl-cad logo
    std::string model_logo_path = info.getModelLogoPath();
    img.drawTransparentImage(3250, 10, 200, 200, model_logo_path, 250);
    //branding logo
    if (opt.getLogopath() != ""){
        img.drawTransparentImage(3250, 2280, 200, 200, opt.getLogopath(), 250);
    }
    statusPrint("    ...Finished logos", opt.verbosePrinting());
    statusPrint(opt.getOpenGUI() ? "...Finished report" : std::string("...Finished report: " + opt.getOutFile()), opt.verbosePrinting());

    // paint renderings

    // optionally, display the scene
    if (opt.getOpenGUI()) {
        img.openInGUI();
    }

    // optionally, export the image
    if (opt.getExportToFile()) {
        img.exportToFile(opt.getOutFile());
    }
}

static void
handleFolder(Options& options)
{
    for (const auto & entry : std::filesystem::directory_iterator(options.getInFolder())) {
        std::string filename = entry.path().string();
        // ensure entry is a .g
        if (db_filetype(filename.c_str()) < 4)
            continue;

        options.setInFile(filename);

        // build reasonable output filepath
        struct bu_vls isolate_filename = BU_VLS_INIT_ZERO;
        (void)bu_path_component(&isolate_filename, filename.c_str(), BU_PATH_BASENAME_EXTLESS);
        std::string exportPath = options.getOutFolder() + BU_DIR_SEPARATOR + bu_vls_cstr(&isolate_filename) + "_report.png";
        options.setOutFile(exportPath);

        generateReport(options);
        statusPrint("", options.verbosePrinting()); // empty print to separate logging
    }
}


int
main(int argc, const char **argv)
{
    Options options;

    bu_setprogname(argv[0]);

    if (options.readParameters(argc, argv)) {
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
