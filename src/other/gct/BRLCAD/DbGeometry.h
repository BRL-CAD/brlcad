/* DbGeometry.h
 *
 * ----------------------------------------------------------------------
 *
 * Copyright (c) 2014 SURVICE Engineering. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * ----------------------------------------------------------------------
 *
 * Revision Date:
 *  08/20/13
 *
 * Authors(s):
 *  DRH
 *
 * Description:
 *  Implements the INativeGeometry interface for BRL-CAD targets.
 */

#ifndef BRLCADDBGEOMETRY_H
#define BRLCADDBGEOMETRY_H

// BRL-CAD headers
extern "C" {
#include "bu.h"
#include "raytrace.h"
}

// Local headers
#include "Interface/INativeGeometry.h"

namespace GeomConversion {
  
  namespace BRLCAD {
    
    /**
     *  DbGeometry
     *
     *  Implements the INativeGeometry interface for BRL-CAD targets.
     */
    class DbGeometry : public Interface::INativeGeometry
    {
    public:
      DbGeometry(std::string);
      ~DbGeometry();
      
      virtual void* getGeometry();
      struct resource* getResource(int);
      
      virtual void loadGeometry(std::string, int tcount=1);
      virtual void cleanupGeometry();
      
      virtual double* getGeometryMin();
      virtual double* getGeometryMax();
      
    private:
      struct rt_i *m_rtip;
    };
    
  } // namespace DbGeometry

}  // namespace GeomConversion

#endif
