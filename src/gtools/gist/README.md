# README

## Introduction ##

The goal of this effort is to auto-generate beautiful concise summaries of 3D geometry models in BRL-CAD, an open source computer-aided design (CAD) system. Leveraging capabilities in BRL-CAD, we worked on creating a tool that produces a novel “one page” summary report that includes visual, textual, and contextual information about a given 3D model.

This tool was developed by Zhuo (Danny) Chen, Allyson Hoskinson, Andrew Plant, Mark Sturtevant, and Michael Tao, under the mentorship of Sean Morrison and Chris McGregor. Further development by Yida Zou, Sam Hollenbeck, Leo Feng, Rohan Dhawan, and Timothy Pham, under the mentorship of Sean Morrison and Chris McGregor.

## Requirements ##

This code has been run and tested on:

* C++
* Visual Studio
* CMake

## External Deps  ##

* BRL-CAD - Download latest version at https://brlcad.org/wiki/Compiling
* OpenCV C++ - Download lastest version at https://docs.opencv.org/4.x/df/d65/tutorial_table_of_content_introduction.html
* Boost - Download required version at Boost C++ Libraries download | SourceForge.net
* CMake - Download latest CMake at https://cmake.org/
* Git - Download latest version at https://git-scm.com/book/en/v2/Getting-Started-Installing-Git

## Installation ##

This tool is part of the BRL-CAD repository which you can download using git from below:

 `git clone https://github.com/BRL-CAD/brlcad.git`

## Tests ##

No test suites are available yet, but will be listed here soon.

## Execute Code ##
### Setting up BRL-CAD
To build the code, run the following command in the `build` directory through terminal

`cmake .. -DBRLCAD_ENABLE_STRICT=NO -DBRLCAD_BUNDLED_LIBS=ON -DCMAKE_BUILD_TYPE=Release`

Then, run the following code to compile BRL-CAD

`make`

 * This tool can also be individually made through
   
   `make gist`

To run BRL-CAD, run one of the two commands

`bin/mged`
`bin/archer`

### Running the report generator through command line

Open terminal, and navigate to `brlcad/build`. 

Run the program using

`./bin/gist -p path/to/file.g -f report.png`

Example

`./bin/gist -p ../build/share/db/m35.g -f report.png`

### Command Line Arguments

You can see these by running:
`./bin/gist -h `

    p = filepath
    P = pixels per inch, default is 300ppi
    F = path specified is a folder of models
    E = path to folder to export reports. Used for processing folder of models
    g = GUI output
    f = filepath of png export, MUST end in .png
    w = override name of owner of geometry file (defauts to system name), to be used in report
    n = name of preparer, to be used in report
    T = directory where rt and rtwizard executables are stored
    c = classification of a file, to be displayed in uppercase on top and bottom of report.
           * If the classification is a security access label, a corresponding color will be applied to the border
           * Options: UNCLASSIFIED, CONFIDENTIAL, SECRET, TOP SECRET, <CUSTOM>
    o = orientation of the file, default is right hand, flag will change orientation output to left hand
    O = orientation of the file, default is +Z-up, flag will change orientation output to +Y-up
    N = notes that a user would like to add to be specified in the report
    Z = option to re-use pre-made renders in the output folder.  Should only be used when running on the same model multiple times.
    t = option to specify the top component of the report. Useful when there are multiple tops
    l = override the default length units in a file.
    L = filepath for optional logo.
    m = override the default mass units in a file.
    
All options that allow entering in custom text should use double quotation marks ("") if you want to include spaces.

### Windows OS Setup
1. Download and install OpenCV. Be sure to select “Windows” and the 4.6.0 version.
2. Download, install and compile BRL-CAD.
3. Open “BRLCAD.sln” in the build folder.
4. Search the Visual Studio Selection Explorer for “gist”. When found, right click and click “Set as Startup Project”.
5. Press the green play button towards the top middle to run!

## Environmental Variables/Files ##

Every environmental needed to install BRL-CAD is needed. Consult the BRL-CAD documentation for more information.

For this project specifically, the OpenCV Environmental Variable is needed which is:

OpenCV_DIR

## Deployment ##

Since this application is a command line tool, no deployment is required.

## References ##

https://brlcad.org/wiki/Compiling

## Support ##

Admins looking for support should first look at the application help page and BRL-CAD wiki.
Users looking for help seek out assistance from the customer.
