#include "pch.h"

void readParameters(int argc, char** argv);
void generateReport();

int main(int argc, char **argv) {
    readParameters(argc, argv);
    generateReport();
}

/**
 * Takes in a list of parameters and updates program variables accordingly
 * 
 * @argc the number of parameters
 * @argv the list of parameters
 */
void readParameters(int argc, char** argv)
{
    // TODO: this

    // i.e. if user specifies to open the file in a gui, we'll put that here...

    // normally, I use a class for this, but that might be overkill...
    // or we store some things in the information gatherer class

    // this may be moved into a separate class if we want to use certain values as constants...
}

/**
 * Calls the necessary functions to generate the reports. 
 */
void generateReport()
{
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

