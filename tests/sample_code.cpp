#include "pch.h"

// /brlcad/build/share/db/moss.g
// /brlcad/src/gtools/rgen/visualization
// ../../../../../build/bin/mged ../db/moss.g tops
// ../../../../../build/bin/rt ../db/moss.g all.g
// EXAMPLE of running: ./a.out ../db/moss.g

using namespace cv;


int run_sample(int argc, char** argv) {
    if (argc == 2) {
        std::string filename = argv[1];
        std::cout << "Processing file: " << filename << std::endl;

        std::string genTops = "../../../../../build/bin/mged " + filename + " tops";
        auto result1 = system(genTops.c_str());

        std::string renderAll = "../../../../../build/bin/rt -C 255/255/255 -s 1024 -c \"set ambSamples=64\" " + filename + " all.g";
        auto result2 = system(renderAll.c_str());
        std::cout << "Success\n";
        return 0;
    }
    else {
        // To create an image
        // CV_8UC3 depicts : (3 channels,8 bit image depth)
        // Height  = 750 pixels, Width = 1500 pixels
        // Background is set to white
        Mat img(750, 1500, CV_8UC3, Scalar(255, 255, 255));

        putText(img, "Report:", Point(0, 50), FONT_HERSHEY_DUPLEX, 1.0, CV_RGB(0, 0, 0), 1);

        // check whether the image is loaded or not
        if (img.empty())
        {
            std::cout << "\n Image not created. You"
                " have done something wrong. \n";
            return -1;    // Unsuccessful.
        }

        // first argument: name of the window
        // second argument: flag- types: 
        // WINDOW_NORMAL If this is set, the user can 
        //               resize the window.
        // WINDOW_AUTOSIZE If this is set, the window size
        //                is automatically adjusted to fit 
        //                the displayed image, and you cannot
        //                change the window size manually.
        // WINDOW_OPENGL  If this is set, the window will be
        //                created with OpenGL support.
        namedWindow("Report", WINDOW_AUTOSIZE);

        //Saves to png
        imwrite("report.png", img);

        // first argument: name of the window
        // second argument: image to be shown(Mat object)
        imshow("Report", img);

        /*  Mat whiteMatrix(200, 200, CV_8UC3, Scalar(255, 255, 255));//Declaring a white matrix
            Point center(100, 100);//Declaring the center point
            int radius = 50; //Declaring the radius
            Scalar line_Color(0, 0, 0);//Color of the circle
            int thickness = 2;//thickens of the line
            namedWindow("whiteMatrix");//Declaring a window to show the circle
            circle(whiteMatrix, center,radius, line_Color, thickness);//Using circle()function to draw the line//
            imshow("WhiteMatrix", whiteMatrix);//Showing the circle//*/

        waitKey(0); //wait infinite time for a keypress

        // destroy the window with the name, "MyWindow"
        destroyWindow("Report");
        return 0;
    }
}

