/* BrlcadLoader.cpp
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
 *  08/19/13
 *
 * Authors(s):
 *  DRH
 *
 * Description:
 *  This class contains the functions to comb a BRL-CAD database file and
 *  return region names.
 */

/* Local headers */
#include "BrlcadLoader.h"
#include "BrlcadAppWalker.h"

//******************************************************************************
// NAME    constructor
//
// INPUTS
//    filename - target file name
//
// DESCRIPTION
//    Construct the object.
//******************************************************************************
BrlcadLoader::BrlcadLoader(const std::string filename)
  : GLoaderInterface(filename)
  , m_dbip(NULL)
{
}

//******************************************************************************
// NAME    destructor
//
// DESCRIPTION
//    Cleanup the object.
//******************************************************************************
BrlcadLoader::~BrlcadLoader()
{
}

//******************************************************************************
// NAME    startLoader
//
// INPUTS
//    none
//
// DESCRIPTION
//    Walk geometry to get all the region names within.
//******************************************************************************
void BrlcadLoader::startLoader()
{
  m_status = true;

  // First initialize the librt resource
  if (rt_uniresource.re_magic != RESOURCE_MAGIC)
    {
      rt_init_resource(&rt_uniresource, 0, NULL);
    }
  
    //
    // Open the database and load the geometry
    //
  if ((m_dbip = db_open(m_fileName.c_str(), "r")) == DBI_NULL)
      {
	m_status = false;
        return;
      }
  
    if (db_dirbuild(m_dbip))
      {
        m_status = false;
        return;
      }
    

    // update all objects with the correct number of
    // times the object is referenced by other objects
    db_update_nref(m_dbip, &rt_uniresource);

    BrlcadAppWalker appWalker;
    BrlcadTreeWalker tw(m_dbip, &rt_uniresource, appWalker);
    struct directory *dp;

    // Look at all items in the geometry file
    FOR_ALL_DIRECTORY_START(dp, m_dbip) {
      // if an item isn't referred to by any other object
      // this is a top level object we should start traversing
      // to construct the geometry
      if (dp->d_nref == 0)
	tw.brlcad_functree(dp);  // Traverse the tree, generating geometry
    }  FOR_ALL_DIRECTORY_END;
    //fprintf(stderr, "final\n");
    
    // set both bot and non-bot lists
    m_botRegionNames = appWalker.getBotRegions();
    m_nonbotRegionNames = appWalker.getNonBotRegions();
    
    db_close(m_dbip);
    m_dbip = NULL;
}
