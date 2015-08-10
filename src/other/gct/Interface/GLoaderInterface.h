/* GLoaderInterface.h
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
 *  Interface class that specifies necessary functions to load and query
 *  for possible geometry names.
 */

#ifndef GLOADERINTERFACE_H
#define GLOADERINTERFACE_H

/* Standard libraries */
#include <string>
#include <vector>

class GLoaderInterface
{
 public:
  GLoaderInterface(const std::string filename)
    : m_fileName(filename)
    , m_status(true) { ; }
  
  std::string getFileName() const { return m_fileName; }
  bool getStatus() { return m_status; }  // true = good, false = fail

  virtual std::vector<std::string> getGeomNames() = 0;
  virtual void startLoader() = 0;
  
 protected:
  std::string m_fileName;
  bool m_status;
};

#endif
