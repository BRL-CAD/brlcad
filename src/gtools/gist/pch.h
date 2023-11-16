#pragma once

// OpenCV header files
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include "opencv2/highgui/highgui.hpp"

// Necessary C++ header files
#include <iostream>
#include <cstdlib>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <ctime>
#include "picohash.h"
#include<cstdio>
#include<filesystem>

// Necessary C++ headers to get user account name for Windows and Unix
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <Lmcons.h>
#elif defined(__unix__) || defined(__unix) || defined(unix) || defined(__APPLE__) || defined(__MACH__) || defined(__FreeBSD__)
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#endif

// BRL-CAD header files
#include "bu/getopt.h"
#include "bu/log.h"
#include "bu/units.h"
#include "ged.h"

// Visualization project header files
#include "Options.h"
#include "IFPainter.h"
#include "InformationGatherer.h"
#include "PerspectiveGatherer.h"
#include "RenderHandler.h"
#include "FactsHandler.h"
#include "Position.h"
