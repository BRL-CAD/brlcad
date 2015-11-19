/* INativeGeometry.h
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
 *  Interface class that specifies necessary functions to load and operate
 *  with native geometry.
 */

#ifndef INATIVEGEOMETRY_H
#define INATIVEGEOMETRY_H

// System headers
#include <string>

namespace GeomConversion {
  
  namespace Interface {
    // geometry types -- these will increase as more types are added
    enum GeometryType { BRLCAD, UNKNOWN };
    
    /**
     *  INativeGeometry
     *
     *  Defines an interface to load and operate with native geometry.
     */
    class INativeGeometry
    {
    public: 
      INativeGeometry(std::string filename) 
	: m_filename(filename)
	, m_geometryType(UNKNOWN) { ; }
      virtual ~INativeGeometry() { ; }
      
      GeometryType getGeometryType () { return m_geometryType; }
      virtual void* getGeometry() = 0;
      std::string getGeomtryFilename() { return m_filename; }
      
      virtual void loadGeometry(std::string, int tcount=1) = 0;
      virtual void cleanupGeometry() {;}
      
      // These are arrays of 3 elements!
      virtual double* getGeometryMin() = 0;
      virtual double* getGeometryMax() = 0;
      
      // These are needed for certain geometries, ignored for others
      virtual void loadGeometry(std::string, std::string, int tcount=1) {;}
      virtual int getMode() {return 2;}
      virtual int getMaterial() {return 1;}
      virtual float getThickness() {return 1.0;}
      virtual int getPosition() {return 2;}

    protected:
      std::string m_filename;
      GeometryType m_geometryType;
    };
    
  }  // namespace Interface

}  // namespace GeomConversion

#endif
