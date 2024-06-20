/*                           P C H . H
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
#include "bu/opt.h"
#include "bu/app.h"
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


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

