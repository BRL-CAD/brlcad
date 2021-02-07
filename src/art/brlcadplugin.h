/*                     B R L C A D P L U G I N . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
 /** @file art/artplugin.cpp
  *
  * Once you have appleseed installed, run BRL-CAD's CMake with APPLESEED_ROOT
  * set to enable this program:
  *
  * cmake .. -DAPPLESEED_ROOT=/path/to/appleseed -DBRLCAD_PNG=SYSTEM -DBRLCAD_ZLIB=SYSTEM
  *
  * (the appleseed root path should contain bin, lib and include directories)
  *
  * On Linux, if using the prebuilt binary you'll need to set LD_LIBRARY_PATH:
  * export LD_LIBRARY_PATH=/path/to/appleseed/lib
  *
  *
  * The example scene object used by helloworld is found at:
  * https://raw.githubusercontent.com/appleseedhq/appleseed/master/sandbox/examples/cpp/helloworld/data/scene.obj
  *
  * basic example helloworld code from
  * https://github.com/appleseedhq/appleseed/blob/master/sandbox/examples/cpp/helloworld/helloworld.cpp
  * has the following license:
  *
  * This software is released under the MIT license.
  *
  * Copyright (c) 2010-2013 Francois Beaune, Jupiter Jazz Limited
  * Copyright (c) 2014-2018 Francois Beaune, The appleseedhq Organization
  *
  * Permission is hereby granted, free of charge, to any person obtaining a copy
  * of this software and associated documentation files (the "Software"), to deal
  * in the Software without restriction, including without limitation the rights
  * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  * copies of the Software, and to permit persons to whom the Software is
  * furnished to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in
  * all copies or substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  * THE SOFTWARE.
  *
  */

#include "common.h"

/* brlcad headers */

#include <stdlib.h>
#include <string.h>
#include <vector>
#include <stdio.h>

#include "vmath.h"
#include "raytrace.h"

#include "art.h"

/* A BRLCAD object from geometry database */
const char* Model = "brlcad geometry";

struct BRLCAD_to_ASR
{
    fastf_t distance;
    asf::Vector3d normal;
};

struct BRLCAD_to_ASR brlcad_ray_info;

class APPLESEED_DLL_EXPORT BrlcadObjectFactory : public asr::IObjectFactory
{
public:
    void release() override;
    const char* get_model() const override;
    asf::Dictionary get_model_metadata() const override;
    asf::DictionaryArray get_input_metadata() const override;

    asf::auto_release_ptr<asr::Object> create(const char* name, const asr::ParamArray& params) const override;
    bool create(
	const char* name,
	const asr::ParamArray& params,
	const asf::SearchPaths& search_paths,
	const bool omit_loading_assets,
	asr::ObjectArray& objects) const override;
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
