#include "pch.h"

bool readParameters(int argc, char** argv, Options &opt);
void generateReport(Options opt);

int main(int argc, char **argv) {
    Options options;
    if (readParameters(argc, argv, options))
        generateReport(options);
}

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
bool readParameters(int argc, char** argv, Options &opt)
{
    /*
    * A list of parameters is as follows:
    * p = filepath
    * w = width of output and window
    * l = length of output and window
    * F = path specified is a folder of models
    * g = GUI output
    * f = filename of png export
    */

    bool h = false; // user requested help
    bool f = false; // user specified filepath

    int opts;

    while ((opts = bu_getopt(argc, argv, "Fg?p:P:f:n:T:")) != -1) {
        switch (opts) {
            case 'p':
                f = true;
                opt.setFilepath(bu_optarg);
                break;
            case 'P':
                opt.setPPI(atoi(bu_optarg));
                break;
            case 'F':
                opt.setIsFolder();
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
            case '?':
                h = true;
                break;
        } 
    } 

    if (h) {
        bu_log("\nUsage:  %s [options] -p path/to/model.g\n", argv[0]);
        bu_log("\nOptions:\n");
        bu_log("    p = filepath\n");
        bu_log("    P = pixels per inch, default is 300ppi\n");
        bu_log("    F = path specified is a folder of models\n");
        bu_log("    g = GUI output\n");
        bu_log("    f = filepath of png export, MUST end in .png\n");
        bu_log("    n = name of preparer, to be used in report\n");
        bu_log("    T = temporary directory to store intermediate files\n");
        return false;
    }
    //If user has no arguments or did not specify filepath, give shortened help
    else if (argc < 2 || !f) {
        bu_log("\nUsage:  %s [options] -p path/to/model.g\n", argv[0]);
        bu_log("\nPlease specify the path to the file for report generation, use flag \"-?\" to see all options\n");
        return false;
    }

    /*
    * Theoretically there would be something here to check that the model path is valid and I have some examples for ref
    * in rt but I can't test it myself because I can't get rt working in my local still (Ally)
    */

    return true;
}

/**
 * Calls the necessary functions to generate the reports. 
 * 
 * @opt options to be used in report generation
 */
void generateReport(Options opt)
{
    // create image frame
    IFPainter img(opt.getLength(), opt.getWidth());

    // create information gatherer
    InformationGatherer info;

    // read in all information from model file
    if (!info.gatherInformation(opt.getFilepath(), opt.getName()))
    {
        std::cerr << "Error on Information Gathering.  Report Generation skipped..." << std::endl;
        return;
    }

    // Define commonly used ratio variables
    int XY_margin = opt.getWidth() / 150;
    int margin = opt.getWidth() / 150;
    int header_footer_height = opt.getLength() / 25;
    int padding = opt.getLength() / 250;
    int border_px = 3;
    int vvHeight = (opt.getLength() - 2*header_footer_height - 2*margin) / 3;
    

    // Has same height and width as V&V Checks, offset X by V&V checks width
    // makeHeirarchySection(img, info, XY_margin + vvSectionWidth + (opt.getLength() / 250), vvOffsetY, vvSectionWidth, vvSectionHeight, opt);

    // define the position of all sections in the report
    Position imagePosition(0,0,opt.getWidth(), opt.getLength());
    Position topSection(margin, margin, imagePosition.width() - 2*margin, header_footer_height);
    Position bottomSection(margin, imagePosition.bottom() - header_footer_height - margin, imagePosition.width() - 2*margin, header_footer_height);
    Position fileSection(imagePosition.right() - imagePosition.quarterWidth() - margin, topSection.bottom() + padding, imagePosition.quarterWidth() - border_px, (imagePosition.height() / 2) - margin - header_footer_height - (padding) - 2*border_px);
    Position verificationSection(fileSection.x(),fileSection.bottom() + padding, fileSection.width(), fileSection.height());
    Position vvSection(margin, imagePosition.height() - margin - header_footer_height - padding - vvHeight, ((imagePosition.width() - fileSection.width() - 2*margin) / 2) - padding, vvHeight);
    Position hierarchySection(vvSection.right() + padding, vvSection.y(), vvSection.width(), vvSection.height());
    Position renderSection(margin, topSection.bottom() + padding, imagePosition.width() - fileSection.width() - 2*margin, imagePosition.height() - margin - header_footer_height - vvHeight - 2 * padding - border_px);


    // draw all sections
    makeTopSection(img, info, topSection.x(), topSection.y(), topSection.width(), topSection.height());
    makeBottomSection(img, info, bottomSection.x(), bottomSection.y(), bottomSection.width(), bottomSection.height());
    makeFileInfoSection(img, info, fileSection.x(), fileSection.y(), fileSection.width(), fileSection.height());
    makeVerificationSection(img, info, verificationSection.x(), verificationSection.y(), verificationSection.width(), verificationSection.height());
    makeVVSection(img, info, vvSection.x(), vvSection.y(), vvSection.width(), vvSection.height());
    makeHeirarchySection(img, info, hierarchySection.x(), hierarchySection.y(), hierarchySection.width(), hierarchySection.height(), opt);
    makeRenderSection(img, info, renderSection.x(), renderSection.y(), renderSection.width(), renderSection.height(), opt);
    
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
