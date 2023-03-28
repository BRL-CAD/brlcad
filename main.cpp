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

    while ((opts = bu_getopt(argc, argv, "Fg?p:P:f:n:")) != -1) {
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

    int XY_margin = opt.getWidth() / 150;

    makeTopSection(img, info, opt.getWidth() / 150, opt.getWidth() / 150, opt.getWidth() - opt.getWidth() / 150 * 2, opt.getLength() / 25);

    makeBottomSection(img, info, opt.getWidth() / 150, opt.getLength() - opt.getLength() / 25 - opt.getWidth() / 150, opt.getWidth() - opt.getWidth() / 150 * 2, opt.getLength() / 25);


    // Border width is 3, subtract or add 3 to calculations to align with other boxes
    // offsets are based on above section offset + height (or width) of box
    int fileSectionOffsetY = (XY_margin) + (opt.getLength() / 25) + (opt.getLength() / 250); 
    int fileSectionOffsetX = opt.getWidth() - (opt.getWidth() / 4) - (XY_margin);
    int fileSectionWidth = (opt.getWidth() / 4) - 3;
    int fileSectionHeight = (opt.getLength() / 2) - (XY_margin) -  (opt.getLength() / 25) - (opt.getLength() / 250) * 2;
    makeFileInfoSection(img, info, fileSectionOffsetX, fileSectionOffsetY, fileSectionWidth, fileSectionHeight);
    
    int VerficationOffsetY = (opt.getLength() - XY_margin - (opt.getLength() / 25) - (opt.getLength() / 250)) - fileSectionHeight; 
    makeVerificationSection(img, info, fileSectionOffsetX, VerficationOffsetY, fileSectionWidth, fileSectionHeight);
    
    int vvSectionHeight = (opt.getLength() - (opt.getLength() / 25) * 2 - (XY_margin / 2)) / 3;
    int vvSectionWidth = ((opt.getWidth() - (fileSectionWidth + 3) - (2*XY_margin)) / 2) - (opt.getLength() / 250);
    int vvOffsetY = (opt.getLength() - XY_margin - (opt.getLength() / 25) - (opt.getLength() / 250)) - vvSectionHeight;
    makeVVSection(img, info, XY_margin, vvOffsetY, vvSectionWidth, vvSectionHeight);

    // Has same height and width as V&V Checks, offset X by V&V checks width
    makeHeirarchySection(img, info, XY_margin + vvSectionWidth + (opt.getLength() / 250), vvOffsetY, vvSectionWidth, vvSectionHeight);
    
    // paint renderings
    makeRenderSection(img, info, XY_margin, 2 * XY_margin + opt.getLength() / 25, vvSectionWidth * 2, opt.getLength() - vvSectionHeight - 3 * (opt.getLength() / 25) / 2, opt);
    //img.drawRect(XY_margin, 2*XY_margin + opt.getLength() / 25, vvSectionWidth * 2, opt.getLength() - vvSectionHeight - 3 * (opt.getLength() / 25) / 2, 3, cv::Scalar(0, 0, 0));

    // optionally, display the scene
    if (opt.getOpenGUI()) {
        img.openInGUI();
    }

    // optionally, export the image
    if (opt.getExportToFile()) {
        img.exportToFile(opt.getFileName());
    }
}
