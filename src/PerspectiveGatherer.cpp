#include "PerspectiveGatherer.h"

std::string renderAmbientOcclusion()
{
    // hardcode filename until options come out
    std::string component = "all.g";
    std::string pathToInput = "../db/";
    std::string fileInput = "moss.g";
    std::string pathToOutput = "../output/";
    std::string fileOutput = "moss.png";
    std::cout << "Processing file: " << fileInput << std::endl;

    // FIX security vulnerability
    std::string inputname = pathToInput + fileInput;
    std::string outputname = pathToOutput + fileOutput;

    std::ifstream file;
    file.open(outputname);
    if (file) {
        std::string rmFile = "rm " + outputname;
        auto result2 = system(rmFile.c_str());
    }
    file.close();
    // TODO Need to somehow decide what to render if there are multiple components.
    // Maybe let user choose which one to render e.g. pass in all.g?

    // EX: ../../../../../build/bin/rt -C 255/255/255 -s 1024 -c "set ambSamples=64" ../db/moss.g all.g
    std::string render = "../../../../../build/bin/rt -C 255/255/255 -s 1024 -c \"set ambSamples=64\" -o " + outputname + " " + inputname + " " + component;
    auto result2 = system(render.c_str());
    std::cout << "Successlly generated ambient occlusion file\n";
	return outputname;
}

std::string renderPerspective(RenderingFace face)
{
    // hardcode filename until options come out
    std::string component = "all.g";
    std::string pathToInput = "../db/";
    std::string fileInput = "moss.g";
    std::string pathToOutput = "../output/";
    std::string fileOutput = "moss.png";
    std::cout << "Processing file: " << fileInput << std::endl;

    // FIX security vulnerability
    std::string inputname = pathToInput + fileInput;
    std::string outputname = pathToOutput + fileOutput;


    int a, e;
    if (face == FRONT) {
        a = 0, e = 0;
        outputname += "_front.png";
    } else if (face == RIGHT) {
        a = 90, e = 0;
        outputname += "_right.png";
    } else if (face == BACK) {
        a = 180, e = 0;
        outputname += "_back.png";
    } else if (face == LEFT) {
        a = 270, e = 0;
        outputname += "_left.png";
    } else if (face == TOP) {
        a = 0, e = 90; // may need to change "a"?
        outputname += "_top.png";
    } else if (face == BOTTOM) {
        a = 0, e = 270;
        outputname += "_bottom.png";
    }

    // FIX security vulnerability
    std::ifstream file;
    file.open(outputname);
    if (file) {
        std::string rmFile = "rm " + outputname;
        auto result2 = system(rmFile.c_str());
    }
    file.close();
    
    std::string render = "../../../../../build/bin/rtedge -s 1024 -W -R -a " + std::to_string(a) + " -e " + std::to_string(e) + " -o " + outputname + " " + filename + " " + component;
    auto result2 = system(render.c_str());
    std::cout << "Successlly generated perspective rendering file\n";
	return outputname;
}