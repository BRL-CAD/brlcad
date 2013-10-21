/*              B U _ A R G _ P A R S E _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

#ifndef BU_ARG_PARSE_PRIVATE_H
#define BU_ARG_PARSE_PRIVATE_H

#include "tclap/CmdLine.h"
/* local customizations of TCLAP MUST follow the above header */

/* get common data from the C world */
#include "bu_arg_parse.h" /* includes bu.h */

// some customization of TCLAP classes
class BRLCAD_StdOutput : public TCLAP::StdOutput
{
  // example usage in main:
  //   CmdLine cmd("this is a message", ' ', "0.99" );
  //   // set the output
  //   BRLCAD_StdOutput brlstdout;
  //   cmd.setOutput(&brlstdout);
  //   // proceed normally ...

public:


  virtual void failure(TCLAP::CmdLineInterface& c, TCLAP::ArgException& e) {
    std::string progName = c.getProgramName();

    // trim path (user can use 'which prog' if he wants it
    std::string::size_type idx = progName.rfind('/');
    if (idx != std::string::npos)
      progName.erase(0, idx+1);

    std::cerr << "Input error: " << e.argId() << std::endl
              << "             " << e.error() << std::endl << std::endl;

    if (c.hasHelpAndVersion()) {
      std::cerr << "Brief usage: " << std::endl;

      _shortUsage(c, std::cerr );

      std::cerr << std::endl << "For complete usage and help type: "
                << std::endl << "   " << progName << " --help"
                << std::endl << std::endl;
    }
    else
      usage(c);

    exit(1);
  }

  virtual void usage(TCLAP::CmdLineInterface& c) {
    std::cout << std::endl << "Usage: " << std::endl << std::endl;

    _shortUsage(c, std::cout );

    std::cout << std::endl << std::endl << "Where: " << std::endl << std::endl;

    _longUsage(c, std::cout );

    std::cout << std::endl;

  }

  virtual void version(TCLAP::CmdLineInterface& c) {
    std::list<TCLAP::Arg*> args = c.getArgList(); // quieten compiler
    ; // do not show version
    //cout << "my version message: 0.1" << endl;
  }

  // use our own implementations:

  /**
   * Writes a brief usage message with short args.
   * \param c - The CmdLine object the output is generated for.
   * \param os - The stream to write the message to.
   */
  void _shortUsage(TCLAP::CmdLineInterface& c,
                   std::ostream& os) const;

  /**
   * Writes a longer usage message with long and short args,
   * provides descriptions and prints message.
   * \param c - The CmdLine object the output is generated for.
   * \param os - The stream to write the message to.
   */

  void _longUsage(TCLAP::CmdLineInterface& c,
                  std::ostream& os) const;

  /**
   * This function inserts line breaks and indents long strings
   * according the  params input. It will only break lines at spaces,
   * commas and pipes.
   * \param os - The stream to be printed to.
   * \param s - The string to be printed.
   * \param maxWidth - The maxWidth allowed for the output line.
   * \param indentSpaces - The number of spaces to indent the first line.
   * \param secondLineOffset - The number of spaces to indent the second
   * and all subsequent lines in addition to indentSpaces.
   */
  void spacePrint(std::ostream& os,
                  const std::string& s,
                  int maxWidth,
                  int indentSpaces,
                  int secondLineOffset) const;

};

inline void
BRLCAD_StdOutput::_shortUsage(TCLAP::CmdLineInterface& _cmd,
                              std::ostream& os) const
{
  std::list<TCLAP::Arg*> argList = _cmd.getArgList();
  std::string progName = _cmd.getProgramName();

  // trim path (user can use 'which prog' if he wants it
  std::string::size_type idx = progName.rfind('/');
  if (idx != std::string::npos)
      progName.erase(0, idx+1);

  TCLAP::XorHandler xorHandler = _cmd.getXorHandler();
  std::vector<std::vector<TCLAP::Arg*> > xorList = xorHandler.getXorList();

  std::string s = progName + " ";

  // first the xor
  for (int i = 0; static_cast<unsigned int>(i) < xorList.size(); i++) {
    s += " {";
    for (TCLAP::ArgVectorIterator it = xorList[i].begin();
         it != xorList[i].end(); it++ )
      s += (*it)->shortID() + "|";

    s[s.length()-1] = '}';
  }

  // then the rest
  for (TCLAP::ArgListIterator it = argList.begin(); it != argList.end(); it++)
    if (!xorHandler.contains((*it))) {

      // hack: skip two args we don't currently use
      const std::string& id((*it)->longID());

      /*
      // debug
      const std::string& id2((*it)->shortID());
      printf("DEBUG: short ID => '%s'; long ID => '%s'\n", id2.c_str(), id.c_str());
      */

      if (id.find("version") != std::string::npos
          || id.find("ignore_rest") !=  std::string::npos)
        continue;

      s += " " + (*it)->shortID();
    }

  // if the program name is too long, then adjust the second line offset
  int secondLineOffset = static_cast<int>(progName.length()) + 2;
  if (secondLineOffset > 75/2)
    secondLineOffset = static_cast<int>(75/2);

  spacePrint(os, s, 75, 3, secondLineOffset);

} // BRLCAD_StdOutput::_shortUsage

inline void
BRLCAD_StdOutput::_longUsage(TCLAP::CmdLineInterface& _cmd,
                             std::ostream& os) const
{
  std::list<TCLAP::Arg*> argList = _cmd.getArgList();
  std::string message = _cmd.getMessage();
  TCLAP::XorHandler xorHandler = _cmd.getXorHandler();
  std::vector<std::vector<TCLAP::Arg*> > xorList = xorHandler.getXorList();

  // first the xor
  for (int i = 0; static_cast<unsigned int>(i) < xorList.size(); i++) {
    for (TCLAP::ArgVectorIterator it = xorList[i].begin();
          it != xorList[i].end();
          it++) {
      spacePrint(os, (*it)->longID(), 75, 3, 3);
      spacePrint(os, (*it)->getDescription(), 75, 5, 0);

      if (it+1 != xorList[i].end())
        spacePrint(os, "-- OR --", 75, 9, 0);
    }
    os << std::endl << std::endl;
  }

  // then the rest
  for (TCLAP::ArgListIterator it = argList.begin(); it != argList.end(); it++) {
    if (!xorHandler.contains((*it))) {

      // hack: skip two args we don't currently use
      const std::string& id((*it)->longID());
      if (id.find("version") != std::string::npos
          || id.find("ignore_rest") !=  std::string::npos)
        continue;

      spacePrint(os, (*it)->longID(), 75, 3, 3);
      spacePrint(os, (*it)->getDescription(), 75, 5, 0);
      os << std::endl;
    }
  }

  os << std::endl;

  spacePrint(os, message, 75, 3, 0);

} // BRLCAD_StdOutput::_longUsage

inline void
BRLCAD_StdOutput::spacePrint(std::ostream& os,
                             const std::string& s,
                             int maxWidth,
                             int indentSpaces,
                             int secondLineOffset) const
{
  int len = static_cast<int>(s.length());

  if ((len + indentSpaces > maxWidth) && maxWidth > 0) {
    int allowedLen = maxWidth - indentSpaces;
    int start = 0;
    while (start < len)
    {
      // find the substring length
      // int stringLen = std::min<int>( len - start, allowedLen );
      // doing it this way to support a VisualC++ 2005 bug
      using namespace std;
      int stringLen = min<int>( len - start, allowedLen );

      // trim the length so it doesn't end in middle of a word
      if (stringLen == allowedLen)
        while ( stringLen >= 0 &&
                s[stringLen+start] != ' ' &&
                s[stringLen+start] != ',' &&
                s[stringLen+start] != '|' )
          stringLen--;

      // ok, the word is longer than the line, so just split
      // wherever the line ends
      if (stringLen <= 0)
        stringLen = allowedLen;

      // check for newlines
      for (int i = 0; i < stringLen; i++)
        if ( s[start+i] == '\n' )
          stringLen = i+1;

      // print the indent
      for (int i = 0; i < indentSpaces; i++)
        os << " ";

      if (start == 0) {
        // handle second line offsets
        indentSpaces += secondLineOffset;

        // adjust allowed len
        allowedLen -= secondLineOffset;
      }

      os << s.substr(start,stringLen) << std::endl;

      // so we don't start a line with a space
      while (s[stringLen+start] == ' ' && start < len)
        start++;

      start += stringLen;
    }
  }
  else {
    for (int i = 0; i < indentSpaces; i++)
      os << " ";
    os << s << std::endl;
  }

} // BRLCAD_StdOutput::spacePrint

#endif /* BU_ARG_PARSE_PRIVATE_H */
