# README

## Introduction ##

The goal of this effort is to auto-generate beautiful concise summaries of 3D geometry models in BRL-CAD, an open source computer-aided design (CAD) system.  BRL-CAD is used by academia, industry, and government to represent and visualize 3D models, to help analyze safety, and communicate suitability.  Developing this prototype will involve elements of computer graphics, verification and validation, physically-based rendering, automatic layout algorithms, graphic design, C/C++, Qt, and more. Leveraging capabilities in BRL-CAD, you’ll work on creating a tool that produces a novel “one page” summary report (which you will also help design) that includes visual, textual, and contextual information about a given 3D model.

## Requirements ##

This code has been run and tested on:

* C++
* Visual Studio
* BRL-CAD
* Appleseed


## External Deps  ##

* BRL-CAD - Download latest version at https://brlcad.org/wiki/Compiling 
* Appleseed - Download latest version at appleseed - A modern, open source production renderer (appleseedhq.net)
* Boost - Download required version at Boost C++ Libraries download | SourceForge.net
* CMake - Download latest CMake at https://cmake.org/
* Git - Download latest version at https://git-scm.com/book/en/v2/Getting-Started-Installing-Git 

## Installation ##

Download this code repository by using git:

 `git clone https://github.com/SP23-CSCE482/visualization-sprint-1.git`


## Tests ##

No test suites are available yet, but will be listed here soon.

## Execute Code ##
To build the code, run the following command in the build directory through terminal
`cmake .. -DBRLCAD_ENABLE_STRICT=NO -DBRLCAD_BUNDLED_LIBS=ON -DCMAKE_BUILD_TYPE=Release`

Then, run the following code to compile
`make`

To run BRL-CAD, run one of the two commands
`bin/mged`
`bin/archer`

Currently, we do not need the exact instructions for running the report. However, it will be ran through the terminal in the future. 

## Environmental Variables/Files ##

No environmental variables are needed yet, but they will be listed here soon.


## Deployment ##

To deploy the code, we are planning to allow the user to input parameters into the terminals 

Which then will output a novel “one page” summary report (which you will also help design) that 

includes visual, textual, and contextual information about a given 3D model. Once we get further 

into coding more informtion will be provided in this read-me


## References ##

https://brlcad.org/wiki/Compiling 
appleseed - A modern, open source production renderer (appleseedhq.net)

## Support ##

Admins looking for support should first look at the application help page and BRL-CAD wiki.
Users looking for help seek out assistance from the customer.

