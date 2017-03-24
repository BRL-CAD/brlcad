/* BrlcadAppWalker.cpp
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
 *  Implements the comb_pre_func, comb_post_func, and primitive_func operations
 *  that the BrlcadTreeWalker will call.
 */

/* Standard libraries */
#include <algorithm>

/* BRL-CAD libraries */
extern "C" {
#include "vmath.h"
#include "nmg.h"
#include "rt/geom.h"
#include "bu.h"
#include "rt/func.h"
#include "raytrace.h"
#include "rt/db4.h"
}

/* Local headers */
#include "BrlcadAppWalker.h"

//******************************************************************************
// NAME    constructor
//
// INPUTS
//    none
//
// DESCRIPTION
//    Construct the object.
//******************************************************************************
BrlcadAppWalker::BrlcadAppWalker()
{
  regionNeedsConversion = false;
}

//******************************************************************************
// NAME    destructor
//
// DESCRIPTION
//    Cleanup the object.
//******************************************************************************
BrlcadAppWalker::~BrlcadAppWalker()
{
}

void BrlcadAppWalker::comb_pre_func(struct directory *dp, 
				    BrlcadTreeWalker *btw)
{
  int regCount = btw->inRegion();
  //fprintf(stderr, "entering %s inRegion(%d)", dp->d_namep, regCount);
  //btw->printObjStack();
  //fprintf(stderr, "flags...");
  //btw->printFlags();
  //btw->printFlagStack();
  int tf = btw->getFlags();
  if (regCount == 1)
    {
      if (dp->d_flags & RT_DIR_REGION)
	{
	  regionNeedsConversion = false;
	}
    }
  else if (regCount > 1)
    {
      if (tf & FLAG_UNION)
	{
	  // Should never happen.  Union of region into region is bogus
	  regionNeedsConversion = true;
	}
      else if (tf & (FLAG_SUBTRACT|FLAG_INTERSECT|FLAG_NOT|FLAG_XOR))
	{
	  regionNeedsConversion = true;
	}
    }
}

void BrlcadAppWalker::comb_post_func(struct directory *dp, 
				     struct rt_comb_internal *comb, 
				     BrlcadTreeWalker *btw)
{
  int regCount = btw->inRegion();
  //fprintf(stderr, "leaving %s inRegion(%d)", dp->d_namep, regCount);
  //btw->printObjStack(); btw->printFlags();
  //btw->printFlagStack();
  
  //fprintf(stderr, "groupstack: %ld\n", m_groupStack.size());
  int tf = btw->getFlags();
  if (regCount == 1 )
    {
      if (dp->d_flags & RT_DIR_REGION)
	{
	  if (regionNeedsConversion)
	    if (m_nonBotRegions.empty())
	      m_nonBotRegions.push_back(dp->d_namep);
	    else if (std::find(m_nonBotRegions.begin(), 
			       m_nonBotRegions.end(), 
			       dp->d_namep) == m_nonBotRegions.end())
	      m_nonBotRegions.push_back(dp->d_namep);
        }
    }
}

void BrlcadAppWalker::primitive_func(struct directory *dp, 
				     BrlcadTreeWalker *btw)
{
  int tf = btw->getFlags();
  int inRegion = btw->inRegion();

  // btw->printObjStack(); btw->printFlags();

  if (inRegion > 0)
    {
      // if non-bot
      //   rnc = true
      //   add wireframe
      // else
      //   add triangles

    if (tf & (FLAG_SUBTRACT|FLAG_INTERSECT|FLAG_NOT|FLAG_XOR))
      {
	regionNeedsConversion = true;
      }

    struct rt_db_internal intern;
    btw->brlcad_get_internal(&intern, dp, NULL);

    if (intern.idb_minor_type == ID_BOT)
      {
	//fprintf(stderr, "%s is a bot\n", dp->d_namep);
	// add a drawable for the triangles from this BOT
	if (m_botRegions.empty())
	  m_botRegions.push_back(dp->d_namep);
	else if (std::find(m_botRegions.begin(), 
			   m_botRegions.end(), 
			   dp->d_namep) == m_botRegions.end())
	  m_botRegions.push_back(dp->d_namep);
      }
    else
      {
	//fprintf(stderr, "%s is a prim in region %s\n", dp->d_namep, m_currentGeode->getName().c_str());
	regionNeedsConversion = true;
      }
    }
  else
    {
        // Just a primitive hanging out in space in no region?
        struct rt_db_internal intern;
        btw->brlcad_get_internal(&intern, dp, NULL);

        if (intern.idb_minor_type == ID_BOT)
	  {
	    if (m_botRegions.empty())
	      m_botRegions.push_back(dp->d_namep);
	    else if (std::find(m_botRegions.begin(), 
			       m_botRegions.end(), 
			       dp->d_namep) == m_botRegions.end())
	      m_botRegions.push_back(dp->d_namep);
	  }
    }
    //fprintf(stderr, "%s done\n", dp->d_namep);
}
