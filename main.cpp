#include "pch.h"

void readParameters(int argc, char** argv, Options &opt);
void generateReport();

int main(int argc, char **argv) {
    Options options;
    readParameters(argc, argv, options);
    generateReport();
}

/**
 * Takes in a list of parameters and updates program variables accordingly
 * 
 * @argc the number of parameters
 * @argv the list of parameters
 */
void readParameters(int argc, char** argv, Options &opt)
{
    // TODO (Ally): Write this function to instantiate the options so it can be used in generateReport.

    // TODO (Ally): This includes reading in the filepath of the file and storing it somewhere.

    // TODO (Ally): Along with storing the filepath, you should give some sort of option/parameter for generating
    // reports for an entire folder of models!

    // i.e. if user specifies to open the file in a gui, we'll put that here...

    // normally, I use a class for this, but that might be overkill...
    // or we store some things in the information gatherer class

    // this may be moved into a separate class if we want to use certain values as constants...

    int opts;

    if (argc > 2) {
        /*while ((opts = getopt(argc, argv, "p:t:e:f:m:i:c:")) != -1) {
            switch (opts) {

                case 'p':
                    p = atoi(optarg);
                    break;
                case 't':
                    t = atof(optarg);
                    break;
                case 'e':
                    e = atoi(optarg);
                    dataPointReq = true;
                    break;
                case 'f':
                    filename = optarg;
                    reqFile = true;
                    break;
                case 'm':
                    m = atoi(optarg);
                    break;
                case 'c':
                    newChan = true;
                    c = atoi(optarg);
                    break;
                case 'i':
                    method = optarg;
                    break; 
            }
        } */
    }
}

/**
 * Calls the necessary functions to generate the reports. 
 */
void generateReport()
{
    // TODO (Ally): Incorporate the Options into this method, and set the correct bounds on IFPainter.
    
    // create image frame
    IFPainter img(1500, 1000);

    // create information gatherer
    InformationGatherer info;

    // read in all information from model file
    if (!info.gatherInformation("null"))
    {
        std::cerr << "Error on Information Gathering.  Report Generation skipped..." << std::endl;
        return;
    }

    // paint renderings
    makeRenderSection(img, info, 0, 100, 1200, 900);

    // paint text sections (no method headers yet)
    // paintTitle
    // paintSidebar
    // etc...


    // optionally, display the scene
    img.openInGUI();

    // optionally, export the image
    // img.exportToFile("report.png");
}

