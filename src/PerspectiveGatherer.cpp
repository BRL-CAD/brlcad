#include "PerspectiveGatherer.h"

std::string renderAmbientOcclusion()
{
    // hardcode filename until options come out
    std::string component = "all.g";
    std::string filename = "../db/moss.g";
    std::string outputname = "../output/moss.png";
    std::cout << "Processing file: " << filename << std::endl;

    // TODO Need to somehow decide what to render if there are multiple components.
    // Maybe let user choose which one to render e.g. pass in all.g?

    // EX: ../../../../../build/bin/rt -C 255/255/255 -s 1024 -c "set ambSamples=64" ../db/moss.g all.g
    std::string render = "../../../../../build/bin/rt -C 255/255/255 -s 1024 -c \"set ambSamples=64\" -o " + outputname + " " + filename + " " + component;
    auto result2 = system(render.c_str());
    std::cout << "Successlly generated ambient occlusion file\n";
	return outputname;
}

std::string renderPerspective(RenderingFace face)
{
    std::string component = "all.g";
    std::string filename = "../db/moss.g";
    std::string outputname = "../output/moss_front.png"; // LOL how to use enum?
    std::cout << "Processing file: " << filename << std::endl;

    int a, e;
    if (face == FRONT)
        a = 0, e = 0;
    else if (face == RIGHT)
        a = 90, e = 0;
    else if (face == BACK)
        a = 180, e = 0;
    else if (face == LEFT)
        a = 270, e = 0;
    else if (face == TOP)
        a = 0, e = 90; // may need to change "a"?
    else if (face == BOTTOM)
        a = 0, e = 270;
    
    std::string render = "../../../../../build/bin/rtedge -s 1024 -a " + std::to_string(a) + " -e " + std::to_string(e) + " -o " + outputname + " " + filename + " " + component;
    auto result2 = system(render.c_str());
    std::cout << "Successlly generated perspective rendering file\n";
	return outputname;
}