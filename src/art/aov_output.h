/*                          A O V _ O U T P U T . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#ifndef ART_AOV_OUTPUT_H
#define ART_AOV_OUTPUT_H

#include "common.h"

#include <string>
#include <vector>

#include "renderer/modeling/aov/aovcontainer.h"

namespace renderer { class Frame; }

class ArtAOVOutput
{
public:
    bool assign(const std::string& value, std::string& reason);
    std::string value() const;
    bool populate(renderer::AOVContainer& aovs) const;
    bool write(const renderer::Frame& frame, const char *beauty_path) const;

private:
    std::vector<std::string> m_names;
};

#endif

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
