#include "pch.h"

void readParameters(int argc, char** argv, Options &opt, bool &h, bool &f);
void generateReport(Options opt);

int main(int argc, char **argv) {
    Options options;
    bool help = false;
    bool filepath = false;
    readParameters(argc, argv, options, help, filepath);
    //If user wants help, list all options and how to use program
    if (help) {
        bu_log("\nUsage:  %s [options] -p path/to/model.g\n", argv[0]);
        bu_log("\nOptions:\n");
        bu_log("    p = filepath\n");
        bu_log("    w = width of output and window\n");
        bu_log("    l = length of output and window\n");
        bu_log("    F = path specified is a folder of models\n");
        bu_log("    g = GUI output\n");
        bu_log("    f = filepath of png export, MUST end in .png\n");
        bu_log("    n = name of preparer, to be used in report\n");
        return 0;
    } 
    //If user has no arguments or did not specify filepath, give shortened help
    else if(argc < 2 || !filepath) {
        bu_log("\nUsage:  %s [options] -p path/to/model.g\n", argv[0]);
        bu_log("\nPlease specify the path to the file for report generation, use flag \"-?\" to see all options\n");
        return 0;
    }
    /*
    * Theoretically there would be something here to check that the model path is valid and I have some examples for ref
    * in rt but I can't test it myself because I can't get rt working in my local still (Ally)
    */
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
 */
void readParameters(int argc, char** argv, Options &opt, bool &h, bool &f)
{
    // TODO (Ally): Write this function to instantiate the options so it can be used in generateReport.

    // TODO (Ally): This includes reading in the filepath of the file and storing it somewhere.

    // TODO (Ally): Along with storing the filepath, you should give some sort of option/parameter for generating
    // reports for an entire folder of models!

    //Go into switch statement to get arguments as necessary
    /*
    * p = filepath
    * w = width of output and window
    * l = length of output and window
    * F = path specified is a folder of models
    * g = GUI output
    * f = filename of png export
    */

    int opts;

    while ((opts = bu_getopt(argc, argv, "Fg?p:w:l:f:n:")) != -1) {
        switch (opts) {
            case 'p':
                f = true;
                opt.setFilepath(bu_optarg);
                break;
            case 'w':
                opt.setWidth(atoi(bu_optarg));
                break;
            case 'l':
                opt.setLength(atoi(bu_optarg));
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
}

/**
 * Calls the necessary functions to generate the reports. 
 * 
 * @opt options to be used in report generation
 */
void generateReport(Options opt)
{
    // TODO (Ally): Incorporate the Options into this method, and set the correct bounds on IFPainter.
    
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

    // paint renderings
    makeRenderSection(img, info, 0, 0, 2400, 2400, opt);
    //makeRenderSection(img, info, 0, 100, 1200, 900, opt);

    // paint text sections (no method headers yet)
    // paintTitle
    // paintSidebar
    // etc...

    makeTopSection(img, info, opt.getWidth() / 150, opt.getWidth() / 150, opt.getWidth() - opt.getWidth() / 150 * 2, opt.getLength() / 25);

    makeBottomSection(img, info, opt.getWidth() / 150, opt.getLength() - opt.getLength() / 25 - opt.getWidth() / 150, opt.getWidth() - opt.getWidth() / 150 * 2, opt.getLength() / 25);


    // optionally, display the scene
    if (opt.getOpenGUI()) {
        img.openInGUI();
    }

    // optionally, export the image
    if (opt.getExportToFile()) {
        img.exportToFile(opt.getFileName());
    }
}
