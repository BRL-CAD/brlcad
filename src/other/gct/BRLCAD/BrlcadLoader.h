/* BrlcadLoader.h
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

#ifndef BRLCAD_LOADER_H
#define BRLCAD_LOADER_H

/* BRL-CAD libraries */
extern "C" {
#include "vmath.h"
#include "raytrace.h"
#include "rt/geom.h"
}

/* Interface headers */
#include "Interface/GLoaderInterface.h"

/* Class */
class BrlcadLoader : public GLoaderInterface
{
 public:
  BrlcadLoader(const std::string);
  ~BrlcadLoader();

  std::vector<std::string> getBotRegionNames() const { return m_botRegionNames; }
  std::vector<std::string> getNonbotRegionNames() const { return m_nonbotRegionNames; }

  virtual std::vector<std::string> getGeomNames() { return getNonbotRegionNames(); }
  virtual void startLoader();

 protected:
  struct db_i *m_dbip;
  std::vector<std::string> m_botRegionNames;
  std::vector<std::string> m_nonbotRegionNames;
};

#endif
