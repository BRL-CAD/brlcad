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
* Appleseed - Download latest version at appleseed - A modern, open source production renderer (appleseedhq.net)
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

`./bin/gist -p ../../build/share/db/m35.g -f report.png`

### Windows OS Setup
1. Download and install OpenCV. Be sure to select “Windows” and the 4.6.0 version.
2. Download, install and compile BRL-CAD.
3. Open “BRLCAD.sln” in the build folder.
4. Search the Visual Studio Selection Explorer for “gist”. When found, right click and click “Set as Startup Project”.
5. Press the green play button towards the top middle to run!

## Environmental Variables/Files ##

No environmental variables are needed yet, but they will be listed here soon.

## Deployment ##

Since this application is a command line tool, no deployment is required.

## References ##

https://brlcad.org/wiki/Compiling
appleseed - A modern, open source production renderer (appleseedhq.net)

## Support ##

Admins looking for support should first look at the application help page and BRL-CAD wiki.
Users looking for help seek out assistance from the customer.
