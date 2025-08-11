/*                       O P T I O N S . H
 * BRL-CAD
 *
 * Copyright (c) 2023-2025 United States Government as represented by
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

#pragma once

#include "pch.h"

/*
 * The Options class holds report-related parameters that can be specified by the user, such as
 * specifications on which renders should be present to the screen, report dimensions, section dimensions,
 * etc.
 */

class Options
{
public:
    Options();
    ~Options();

    bool readParameters(int argc, const char **argv);

    // Options for orientation settings
    enum coord_system { RIGHT_HAND, LEFT_HAND };
    enum up_axis { POS_Z_UP, POS_Y_UP };

    //Setter functions
    void setInFile(std::string f);
    void setExeDir(std::string f);
    void setLogopath(std::string f);
    void setPPI(int p);
    void setIsFolder();
    void setOpenGUI();
    void setExportToFile();
    void setReuseImages(bool reuse);
    void setOutFolder(std::string fldr);
    bool setOutFile(std::string n);
    void setInFolder(std::string n);
    void setPreparer(std::string n);
    void setOwner(std::string n);
    void setClassification(std::string c);
    void setCoordSystem(coord_system sys);
    void setUpAxis(up_axis axis);
    void setNotes(std::string n);
    void setTopComp(std::string t);
    void setUnitLength(std::string l);
    void setUnitMass(std::string m);


    //Getter functions
    std::string getInFile();
    std::string getExeDir();
    std::string getWorkingDir();
    std::string getLogopath();
    int getWidth();
    int getLength();
    bool getIsFolder();
    bool getOpenGUI();
    bool getExportToFile();
    bool getReuseImages();
    std::string getOutFile();
    std::string getInFolder();
    std::string getOutFolder();
    std::string getPreparer();
    std::string getOwner();
    std::string getClassification();
    coord_system getCoordSystem();
    std::string getCoordSystem_asString();
    up_axis getUpAxis();
    std::string getUpAxis_asString();
    std::string getNotes();
    std::string getTopComp();
    std::string getUnitLength();
    std::string getUnitMass();

    bool isOriginalUnitsLength();
    bool isOriginalUnitsMass();

    bool verbosePrinting();

private:
    // Path to input file that will be used to generate report
    std::string inFile;
    // Path to directory with BRL-CAD executables (rt/rtarea/mged/gqa/..)
    std::string exeDir;
    // Path to working directory where output is stored. Default pattern is "inFile".working
    std::string workingDir;
    // Path to option logo image file
    std::string logopath;
    // Pixels per inch
    int ppi;
    // Dimensions of the output, in pixels
    int width;
    int length;
    // Whether path is a folder with multiple files or not
    bool isFolder;
    // Whether user specifies to open GUI as well
    int openGUI;
    //Whether user decides to export to a png
    bool exportToFile;
    // Whether or not to override pre-existing rt/rtwizard output files
    int reuseImages;
    // Name of export file
    std::string outFile;
    // Name of folder that contains input models
    std::string inFolderName;
    // Name of folder you want to create report(s).png in
    std::string outFolderName;
    // Name of preparer
    std::string preparer;
    // Name of owner (override)
    std::string owner;
    // Classification word
    std::string classification;
    // Orientation
    coord_system coordinate_system;
    up_axis axis_orientation;
    // Notes
    std::string notes;

    // top component
    std::string topComp;

    // Unit length
    std::string uLength;
    bool originalUnitsLength;
    // Unit mass
    std::string uMass;
    bool originalUnitsMass;

    // print verbose status messages
    int verbosePrint;
};

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
