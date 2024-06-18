/*         P E R S P E C T I V E G A T H E R E R . C P P
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

#include "PerspectiveGatherer.h"

std::map<char, FaceDetails>
getFaceDetails()
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


static std::string createOutputBase(std::string inFile, std::string workingDir, std::string component) {
    /* create output filename using the base of input file _component, with working directory path 
    i.e. workingDir/inFileBASE_COMPONENT */

    // get input file base name
    std::string inFileBase = inFile.substr(inFile.find_last_of("/\\") + 1); // strip at last path separator
    inFileBase = inFileBase.substr(0, inFileBase.find_last_of("."));        // remove .ext

    // it shouldn't, but make sure workingDir string does not end in path separator
    std::string working = workingDir;
    char lastChar = working[working.size() - 1];
    if (lastChar == '/' || lastChar == '\\') {
        working.pop_back();
    }

    // when doing ghost render for hierarchy '...' component is space separated list
    // in that case rename to etc to elim spaces and not have too long of a file name
    std::string comp = component;
    if (comp.find(" ") != std::string::npos) {
        comp = "etc";
    }

    // put it all together
    std::string ret = working + BU_DIR_SEPARATOR + inFileBase + "_" + comp;

    return ret;
}


static std::string getCmdPath(std::string exeDir, const char* cmd) {
    char buf[MAXPATHLEN] = {0};
    if (!bu_dir(buf, MAXPATHLEN, exeDir.c_str(), cmd, BU_DIR_EXT, NULL)) {
        bu_exit(BRLCAD_ERROR, "Coudn't find %s, aborting.\n", cmd);
    }

    return std::string(buf);
}


std::string
renderPerspective(RenderingFace face, Options& opt, std::string component, std::string UNUSED(ghost))
{
    std::string pathToInput = opt.getInFile();
    std::string outputname = createOutputBase(opt.getInFile(), opt.getWorkingDir(), component);

    std::string render;
    std::string cmd;
    // setup av for rtedge since that's the majority of our work. The outliers (rt / rtwizard) will
    // reset the av to their needs
    const char* av[20] = { NULL,    // av[0]: cmd
                           "-s",
                           "1024",
                           "-W",
                           "-R",
                           "-a",
                           NULL,    // av[6]: 'a' value
                           "-e",
                           NULL,    // av[8]: 'e' value
                           "-c",
                           "set bs=1",
                           "-o",
                           NULL,   // av[12]: output file name
                           pathToInput.c_str(),
                           component.c_str(),
                           NULL,   // EXTRA for other renders
                           NULL,   // EXTRA for other renders
                           NULL,   // EXTRA for other renders
                           NULL,   // EXTRA for other renders
                           NULL,   // null-termination
                          };

    switch (face) {
        case FRONT:
            cmd = getCmdPath(opt.getExeDir(), "rtedge");
            outputname += "_front.png";

            av[0] = cmd.c_str();
            av[6] = "0";
            av[8] = "0";
            av[12] = outputname.c_str();
            av[15] = NULL;
            break;
        case RIGHT:
            cmd = getCmdPath(opt.getExeDir(), "rtedge");
            outputname += "_right.png";

            av[0] = cmd.c_str();
            av[6] = "90";
            av[8] = "0";
            av[12] = outputname.c_str();
            av[15] = NULL;
            break;
        case BACK:
            cmd = getCmdPath(opt.getExeDir(), "rtedge");
            outputname += "_back.png";

            av[0] = cmd.c_str();
            av[6] = "180";
            av[8] = "0";
            av[12] = outputname.c_str();
            av[15] = NULL;
            break;
        case LEFT:
            cmd = getCmdPath(opt.getExeDir(), "rtedge");
            outputname += "_left.png";

            av[0] = cmd.c_str();
            av[6] = "270";
            av[8] = "0";
            av[12] = outputname.c_str();
            av[15] = NULL;
            break;
        case TOP:
            cmd = getCmdPath(opt.getExeDir(), "rtedge");
            outputname += "_top.png";

            av[0] = cmd.c_str();
            av[6] = "0";
            av[8] = "90";
            av[12] = outputname.c_str();
            av[15] = NULL;
            break;
        case BOTTOM:
            cmd = getCmdPath(opt.getExeDir(), "rtedge");
            outputname += "_bottom.png";

            av[0] = cmd.c_str();
            av[6] = "0";
            av[8] = "270";
            av[12] = outputname.c_str();
            av[15] = NULL;
            break;
        case DETAILED:
            cmd = getCmdPath(opt.getExeDir(), "rt");
            outputname += "_detailed.png";

            av[0] = cmd.c_str();
            av[1] = "-s"; av[2] = "1024";
            av[3] = "-W";
            av[4] = "-R";
            av[5] = "-a";  av[6] = "45";
            av[7] = "-e";  av[8] = "45";
            av[9] = "-C";  av[10] = "255/255/255";
            av[11] = "-A"; av[12] = "1.5";
            av[13] = "-c"; av[14] = "set ambSamples=64 ambSlow=1";
            av[15] = "-o"; av[16] = outputname.c_str();
            av[17] = pathToInput.c_str();
            av[18] = component.c_str();
            av[19] = NULL;
            break;
        case GHOST:
            cmd = getCmdPath(opt.getExeDir(), "rtwizard");
            outputname += "_ghost.png";

            av[0] = cmd.c_str();
            av[1] = "-s";  av[2] = "1024";
            av[3] = "-a";  av[4] = "35";
            av[5] = "-e";  av[6] = "25";
            av[7] = "-i";  av[8] = pathToInput.c_str();
            av[9] = "-c";  av[10] = component.c_str();
            av[11] = "-g"; av[12] = "ghost";
            av[13] = "-G"; av[14] = "10";
            av[15] = "-o"; av[16] = outputname.c_str();
            av[17] = NULL;
            break;
        default:
            /* how'd we get here? */
            break;
    }

    if (bu_file_exists(outputname.c_str(), NULL) && opt.getReuseImages()) {
        // reuse previous render
        if (opt.verbosePrinting())
            bu_log("\tFound %s, skipping\n", outputname.c_str());
        return outputname;
    } else {
        // sanity delete
        bu_file_delete(outputname.c_str());
    }

    struct bu_process* p;
    bu_process_create(&p, av, BU_PROCESS_HIDE_WINDOW);

    // TODO: do we care about output?

    (void)bu_process_wait_n(&p, 0);

    if (!bu_file_exists(outputname.c_str(), NULL)) {
        bu_log("ERROR: %s doesn't exist\n", outputname.c_str());
        bu_exit(BRLCAD_ERROR, "Rendering not generated, aborting.\n");
    } else if (opt.verbosePrinting()) {
        bu_log("\tGenerated %s\n", outputname.c_str());
    }

	return outputname;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
