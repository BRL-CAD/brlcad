/* DbGeometry.cpp
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

// Local headers
#include "DbGeometry.h"

namespace GeomConversion {

  namespace BRLCAD {

    static struct resource resource[MAX_PSW];
  
    //*******************************************************************
    // NAME    constructor
    //
    // INPUTS
    //    filename - target file name
    //
    // DESCRIPTION
    //    Construct the object.
    //*******************************************************************
    DbGeometry::DbGeometry(std::string filename)
      : Interface::INativeGeometry(filename)
      , m_rtip(RTI_NULL)
    {
      char titleBuf[1028];
      m_rtip = rt_dirbuild(filename.c_str(), titleBuf, sizeof(titleBuf));
      if (m_rtip == RTI_NULL)
	return;  // throw exception???
    
      /* Initialize all the per-CPU memory resources.  The number of
       * processors can change at runtime, init them all.
       */
      for (int i=0; i < MAX_PSW; i++)
	{
	  rt_init_resource(&(resource[i]), i, m_rtip);
	  bn_rand_init(resource[i].re_randptr, i);
	}
    
      m_geometryType = Interface::BRLCAD;
    }
  
    //*******************************************************************
    // NAME    destructor
    //
    // DESCRIPTION
    //    Cleanup the object.
    //*******************************************************************
    DbGeometry::~DbGeometry()
    {
      // cleanup rt_i
      if (m_rtip != RTI_NULL)
	{
	  rt_free_rti(m_rtip);
	  m_rtip = RTI_NULL;
	}
    }
  
    //*******************************************************************
    // NAME    getGeometry
    //
    // INPUTS
    //    none
    //
    // DESCRIPTION
    //    Return a reference to the internal target data.
    //*******************************************************************
    void* DbGeometry::getGeometry()
    {
      return ((void*) m_rtip);
    }
  
    //*******************************************************************
    // NAME    getResources
    //
    // INPUTS
    //    i - index of the desired resource
    //
    // DESCRIPTION
    //    Return a reference to CPU resource for the given index.
    //*******************************************************************
    struct resource* DbGeometry::getResource(int i)
    {
      return &(resource[i]);
    }
  
    //*******************************************************************
    // NAME    loadGeometry
    //
    // INPUTS
    //    rname - region name
    //    tcount - thread count
    //
    // DESCRIPTION
    //    Load a given piece of the geometry.
    //*******************************************************************
    void DbGeometry::loadGeometry(std::string rname, int tcount)
    {
      m_rtip->useair = 1; // convert air objects when found
    
      if (rt_gettree(m_rtip, rname.c_str()))
	{
	  bu_log("BRLCAD::DbGeometry::loadRegion - Error loading <%s>\n", 
		 rname.c_str());
	  return;
	}
    
      rt_prep_parallel(m_rtip, tcount);
    }
  
    //*******************************************************************
    // NAME    cleanupGeometry
    //
    // INPUTS
    //    none
    //
    // DESCRIPTION
    //    Perform cleanup of any geometry.
    //*******************************************************************
    void DbGeometry::cleanupGeometry()
    {
      rt_clean(m_rtip);
    }
  
    //*******************************************************************
    // NAME    getGeometryMin
    //
    // INPUTS
    //    none
    //
    // DESCRIPTION
    //    Return minimum 3D dimension of the loaded geometry.
    //*******************************************************************
    double* DbGeometry::getGeometryMin()
    {
      return m_rtip->rti_pmin;
    }
  
    //*******************************************************************
    // NAME    getGeometryMax
    //
    // INPUTS
    //    none
    //
    // DESCRIPTION
    //    Return maximum 3D dimension of the loaded geometry.
    //*******************************************************************
    double* DbGeometry::getGeometryMax()
    {
      return m_rtip->rti_pmax;
    }
  
  } // namespace BRLCAD

}  // namespace GeomConversion
