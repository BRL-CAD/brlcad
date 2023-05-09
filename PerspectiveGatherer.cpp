#include "PerspectiveGatherer.h"

std::map<char, FaceDetails> getFaceDetails()
{
	std::map<char, FaceDetails> output;

	output['F'] = {FRONT, "Front", LENGTH, HEIGHT};
	output['T'] = {TOP, "Top", LENGTH, DEPTH};
	output['R'] = {RIGHT, "Right", DEPTH, HEIGHT};
	output['L'] = {LEFT, "Left", DEPTH, HEIGHT};
	output['B'] = {BACK, "Back", LENGTH, HEIGHT};
	output['b'] = {BOTTOM, "Bottom", LENGTH, DEPTH};
	//output['A'] = {DETAILED, "Ambient Occlusion"};

	return output;
}

std::string extractFileName(std::string filePath) {
    return filePath.substr(filePath.find_last_of("/\\") + 1);
}

std::string renderPerspective(RenderingFace face, Options& opt, std::string component, std::string ghost)
{
    // hardcode filename until options come out
    std::string pathToInput = opt.getFilepath();
    std::string fileInput = extractFileName(pathToInput);
    std::string pathToOutput = "output/";
    std::string fileOutput = fileInput.substr(0, fileInput.find_last_of("."));
    // std::cout << "Path to input: " << pathToInput << " " << fileOutput << std::endl;
    // std::cout << "Component: " << component << std::endl;

    // do directory traversal checks
    if (fileOutput.find("../") != std::string::npos) {
        std::cout << "ERROR: Output file name cannot contain ../\n";
        return "";
    }

    // std::cout << "Path to output: " << pathToOutput << std::endl;
    // std::cout << "Processing file: " << fileInput << std::endl;

    std::string fileString = component.substr(0, component.find("."));
    fileString = fileString.substr(0, fileString.find("/"));
    // std::cout << "File string: " << fileString << std::endl;
    std::string outputname = pathToOutput + fileOutput + "_" + fileString;
    std::replace(outputname.begin(), outputname.end(), ' ', '_');
    if (outputname.size() > 150) {
        outputname = outputname.substr(0, 150);
    }
    std::string render;

    int a, e;
    switch (face) {
        case FRONT:
            a = 0, e = 0;
            outputname += "_front.png";
            render = opt.getTemppath() + "rtedge -s 1024 -W -R -a " + std::to_string(a) + " -e " + std::to_string(e) + " -o " + outputname + " -c \"set bs=1\" " + pathToInput + " " + component;
            break;
        case RIGHT:
            a = 90, e = 0;
            outputname += "_right.png";
            render = opt.getTemppath() + "rtedge -s 1024 -W -R -a " + std::to_string(a) + " -e " + std::to_string(e) + " -o " + outputname + " -c \"set bs=1\" " + pathToInput + " " + component;
            break;
        case BACK:
            a = 180, e = 0;
            outputname += "_back.png";
            render = opt.getTemppath() + "rtedge -s 1024 -W -R -a " + std::to_string(a) + " -e " + std::to_string(e) + " -o " + outputname + " -c \"set bs=1\" " + pathToInput + " " + component;
            break;
        case LEFT:
            a = 270, e = 0;
            outputname += "_left.png";
            render = opt.getTemppath() + "rtedge -s 1024 -W -R -a " + std::to_string(a) + " -e " + std::to_string(e) + " -o " + outputname + " -c \"set bs=1\" " + pathToInput + " " + component;
            break;
        case TOP:
            a = 0, e = 90; // may need to change "a"?
            outputname += "_top.png";
            render = opt.getTemppath() + "rtedge -s 1024 -W -R -a " + std::to_string(a) + " -e " + std::to_string(e) + " -o " + outputname + " -c \"set bs=1\" " + pathToInput + " " + component;
            break;
        case BOTTOM:
            a = 0, e = 270;
            outputname += "_bottom.png";
            render = opt.getTemppath() + "rtedge -s 1024 -W -R -a " + std::to_string(a) + " -e " + std::to_string(e) + " -o " + outputname + " -c \"set bs=1\" " + pathToInput + " " + component;
            break;
        case DETAILED:
            a = 45, e = 45;
            outputname += "_detailed.png";
            render = opt.getTemppath() + "rt -C 255/255/255 -s 1024 -A 1.5 -W -R -c \"set ambSamples=64 ambSlow=1\" -o " + outputname + " " + pathToInput + " " + component;
            break;
        case GHOST:
            a = 35, e = 25;
            outputname += "_ghost.png";
            render = opt.getTemppath() + "rtwizard -s 1024 -a " + std::to_string(a) + " -e " + std::to_string(e) + " -i " + pathToInput + " -c " + component + " -g " + ghost + " -G 10 -o " + outputname;
            // render2 = "../../../build/bin/rtwizard -s 1024 -a " + a + " -e " + e + " -i " + pathToInput + " -g " + ghost + " -G 3 -o " + outputname;
            break;
        default:
            std::cerr<< "mark added this\n";
            break;
    }

    // std::cout << render << std::endl;


    if (opt.getOverrideImages() || std::remove(outputname.c_str()) != 0) {
       std::cerr << "Did not remove " << outputname << std::endl;
    }
    
    try {
        auto result2 = system(render.c_str());
    } catch (std::exception& e) {
        std::cerr << e.what() << std::endl;
    }

    std::cout << "Successfully generated perspective rendering png file\n";

	return outputname;
}