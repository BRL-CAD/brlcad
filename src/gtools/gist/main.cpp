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
    if (!info.gatherInformation(opt.getPreparer())) {
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
        img.exportToFile(opt.getOutFile());
    }
}


/**
 * Checks if we are processing a folder of models
 */
static void
handleFolder(Options& options)
{
    int cnt = 1;

    for (const auto & entry : std::filesystem::directory_iterator(options.getInFolder())) {
        options.setInFile(entry.path().string());
        options.setExportToFile();
        std::string filename = options.getInFile();
        filename = filename.substr(filename.find_last_of("/\\") + 1);
        filename = filename.substr(0, filename.find_last_of("."));
        std::cout << "Processing: " << filename << std::endl;
        std::string exportPath = options.getOutFolder() + "/" + filename + "_report.png";
        options.setOutFile(exportPath);
        generateReport(options);
        std::cout << "Finished Processing: " << cnt++ << std::endl;
    }
}


int
main(int argc, const char **argv)
{
    Options options;

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
