/* BrlcadAppWalker.h
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

#ifndef BRLCADAPPWALKER_H
#define BRLCADAPPWALKER_H

/* Standard libraries */
#include <string>
#include <vector>

/* Linterface headers */
#include "Interface/AppWalkerInterface.h"

/* Local headers */
#include "BrlcadTreeWalker.h"

class BrlcadAppWalker : public AppWalkerInterface
{
 public:
  BrlcadAppWalker();
  ~BrlcadAppWalker();
  
  virtual void comb_pre_func(struct directory *dp, 
			     BrlcadTreeWalker *btw);
  virtual void comb_post_func(struct directory *dp, 
			      struct rt_comb_internal *comb, 
			      BrlcadTreeWalker *btw);
  virtual void primitive_func(struct directory *dp, 
			      BrlcadTreeWalker *btw);

  std::vector<std::string> getBotRegions() { return m_botRegions; }
  std::vector<std::string> getNonBotRegions() { return m_nonBotRegions; }

 private:
  std::vector<std::string> m_botRegions;
  std::vector<std::string> m_nonBotRegions;
  bool regionNeedsConversion; // region needs conversion
};

#endif
