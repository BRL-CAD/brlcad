/*                           P C H . H
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

#include "common.h"

#ifdef DEBUG
#define OPENCV_QUELL_DEBUG
// Due to OpenCV's opencv2/core/cvdef.h defining a debug_build_guard namespace
// that keys off of whether DEBUG is defined, we need to make sure it is unset
// here - otherwise Windows Release builds will end up being unable to link
// properly - the gist includes of opencv will be looking for the namespace
// prefixed names rather than the Release names.
#undef DEBUG
#endif

// OpenCV header files
#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include "opencv2/highgui/highgui.hpp"

#ifdef OPENCV_QUELL_DEBUG
#define DEBUG 1
#endif

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
#ifdef HAVE_WINDOWS_H
#  include <Lmcons.h>
#else
#  include <pwd.h>
#endif

// BRL-CAD header files
#include "bio.h"
#include "bu/opt.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/units.h"
#include "bu/path.h"
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

