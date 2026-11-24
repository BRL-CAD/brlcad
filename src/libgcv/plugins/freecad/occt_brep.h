// Copyright (c) 2026, open2open contributors
// SPDX-License-Identifier: MIT

#ifndef GCV_FREECAD_OCCT_BREP_H
#define GCV_FREECAD_OCCT_BREP_H

class ON_Brep;
class ON_TextLog;
class TopoDS_Shape;

namespace freecad {

bool occt_to_brep(const TopoDS_Shape &shape, ON_Brep &brep,
                  double linear_tolerance, ON_TextLog *log);

}

#endif
