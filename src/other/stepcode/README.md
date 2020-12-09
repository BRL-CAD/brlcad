   Travis-CI   |   AppVeyor CI
:-------------:|:---------------:
Linux, OSX (LLVM) | Windows (MSVC)
[![Build Status](https://travis-ci.org/stepcode/stepcode.svg?branch=master)](https://travis-ci.org/stepcode/stepcode) | [![Build status](https://ci.appveyor.com/api/projects/status/3fbr9t9gfa812oqu?svg=true)](https://ci.appveyor.com/project/mpictor/stepcode)

***********************************************************************
STEPcode v0.9 -- stepcode.org, github.com/stepcode/stepcode

* What is STEPcode? SC reads ISO10303-11 EXPRESS schemas and generates
  C++ source code that can read and write Part 21 files conforming
  to that schema. In addition to C++, SC includes experimental
  support for Python.

* Renamed in April/May 2012: SC was formerly known as STEP Class
  Libraries, SCL for short. It was renamed because the name wasn't
  accurate: the class libraries make up only a part of the code.

* Much of the work to update SC has been done by the developers of
  BRL-CAD, and SC (then STEP Class Library) was originally created at
  NIST in the 90's.

* For information on changes version-by-version, see the NEWS file

* Building and testing SCL - see the INSTALL file

* For more details on the libraries and executables, see the wiki:
  http://github.com/stepcode/stepcode/wiki/About-STEPcode

* For license details, see the COPYING file. Summary: 3-clause BSD.

***********************************************************************

***********************************************************************
CODING STANDARDS

SC's source has been reformatted with astyle. When making changes, try
to match the current formatting. The main points are:

  - K&R (Kernighan & Ritchie) brackets:
```C
   int Foo(bool isBar)
   {
       if (isBar) {
           bar();
           return 1;
       } else
           return 0;
   }
```
  - indents are 4 spaces
  - no tab characters
  - line endings are LF (linux), not CRLF (windows)
  - brackets around single-line conditionals
  - spaces inside parentheses and around operators
  - return type on the same line as the function name, unless that's
    too long
  - doxygen-style comments
    (see http://www.stack.nl/~dimitri/doxygen/docblocks.html)

If in doubt about a large patch, run astyle with the config file
misc/astyle.cfg.
Download astyle from http://sourceforge.net/projects/astyle/files/astyle/

***********************************************************************

For more info, see the wiki.
