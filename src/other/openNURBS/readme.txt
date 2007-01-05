/* $Header$ */
/* $NoKeywords: $ */

Legal Stuff:

  The openNURBS Initiative provides CAD, CAM, CAE, and computer
  graphics software developers the tools to accurately transfer
  3-D geometry between applications.

  The tools provided by openNURBS include:

    * C++ source code libraries to read and write the file format.

    * Quality assurance and revision control.

    * Various supporting libraries and utilities.

    * Technical support.

  Unlike other open development initiatives, alliances, or
  consortia:

    * Commercial use is encouraged.

    * The tools, support, and membership are free.

    * There are no restrictions. Neither copyright nor copyleft
	  restrictions apply.

    * No contribution of effort or technology is required from
	  the members, although it is encouraged.

  For more information, please see <http://www.openNURBS.org>.

  The openNURBS toolkit uses zlib for mesh and bitmap compression.
  The zlib source code distributed with openNURBS is a subset of what
  is available from zlib.  The zlib code itself has not been modified.
  See ftp://ftp.freesoftware.com/pub/infozip/zlib/zlib.html for more 
  details.

  Zlib has a generous license that is similar to the one for openNURBS.
  The zlib license shown below was copied from the zlib web page
  ftp://ftp.freesoftware.com/pub/infozip/zlib/zlib_license.html
  on 20 March 2000.

	  zlib.h -- interface of the 'zlib' general purpose compression library
	  version 1.1.3, July 9th, 1998

	  Copyright (C) 1995-1998 Jean-loup Gailly and Mark Adler

	  This software is provided 'as-is', without any express or implied
	  warranty.  In no event will the authors be held liable for any damages
	  arising from the use of this software.

	  Permission is granted to anyone to use this software for any purpose,
	  including commercial applications, and to alter it and redistribute it
	  freely, subject to the following restrictions:

	  1. The origin of this software must not be misrepresented; you must not
		 claim that you wrote the original software. If you use this software
		 in a product, an acknowledgment in the product documentation would be
		 appreciated but is not required.
	  2. Altered source versions must be plainly marked as such, and must not be
		 misrepresented as being the original software.
	  3. This notice may not be removed or altered from any source distribution.

	  Jean-loup Gailly        Mark Adler
	  jloup@gzip.org          madler@alumni.caltech.edu


	  The data format used by the zlib library is described by RFCs (Request for
	  Comments) 1950 to 1952 in the files ftp://ds.internic.net/rfc/rfc1950.txt
	  (zlib format), rfc1951.txt (deflate format) and rfc1952.txt (gzip format).

 
  Copyright (c) 1993-2006 Robert McNeel & Associates. All Rights Reserved.
  Rhinoceros is a registered trademark of Robert McNeel & Associates.

  THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED
  WARRANTY.  ALL IMPLIED WARRANTIES OF FITNESS FOR ANY PARTICULAR
  PURPOSE AND OF MERCHANTABILITY ARE HEREBY DISCLAIMED.

Background:

  The openNURBS Toolkit consists of the source code for a
  library that will read and write a openNURBS 3-D model files
  (.3DM).  More than 400 software development teams and 
  applications, including Rhino, exchange 3-D models using the
  openNURBS (.3DM) file format.

  For more information abount the openNURBS Initiative, please
  visit

    <http://www.openNURBS.org>

  For details about applications that exchange 3-D geometry using
  the openNURBS .3DM format, please visit

    <http://www.rhino3d.com>
  
Audience:

  The openNURBS Toolkit is intended for use by skilled C++ 
  programmers. The openNURBS Toolkit includes complete
  source code to create a library that will read and write
  openNURBS .3DM files.  In addition, source code for several
  example programs is included.

Overview:

  Currently, the openNURBS Toolkit will read and write all
  information in version 2, 3, and 4 files and read 
  version 1 files.  Version 1 files were created by Rhino 1
  and the Rhino I/O toolkit, the predecessor to openNURBS.

  In addition, the openNURBS Toolkit provides NURBS evaluation
  tools and elementary geometric and 3d view manipulation
  tools.

Instructions:

  Three makefiles are included:

    opennurbs.sln for Microsoft Developer Studio 2005 (VC 8.0).

	opennurbs_vc60.dsw for Microsoft Developer Studio 6 with
	the November 2001 Platform SDK.

	makefile for Gnu's gcc version 3.2.2 20030222

  If you are using another comipler, then modify makefile
  to work with your compiler.


  NOTE WELL: 

  OpenNURBS uses UNICODE to store text information in
  3DM files.  At the time of this release, support for
  UNICODE / ASCII conversion on UNIX platforms is spotty at
  best.

  All memory allocations and frees are done through onmalloc(),
  onfree(), and onrealloc().  The makefile that ships with
  openNURBS simply has onmalloc() call malloc() and onfree() call
  free().  If you want to do something fancier, see the
  opennurbs_memory* files.

  In the code you write include only opennurbs.h.  The opennurbs.h
  header file includes the necessary openNURBS toolkit header
  files in the appropriate order.
  
  If you want to use Open GL to render openNURBS geometry, then
  you may want to include opennurbs_gl.h after openNURBS.h and
  add opennurbs_gl.cpp to your openNURBS Toolkit library.
  See example_gl.cpp for more details.

Getting Started:

  If your goal is to read and write 3dm files, then you may
  find that the class ONX_Model::Read() and ONX_Model::Write()
  to be an easy way to attain your goal.  The ONX_Model class
  is new in the October 2002 OpenNURBS release and is defined
  in opennurbs_extensions.cpp/h.  See example_read.cpp and
  example_write.cpp for more details.

  Compile the openNURBS Toolkit library.
    In order to compile the example programs you must link with
    the openNURBS Toolkit library.  Compile the library as
    described in the first paragraph of the Instructions section.

  Study example_read\example_read.cpp.

    All of the openNURBS geometry classes are derived from the
    ON_Geometry class.  If you need generic attribute information,
    there is probably a ON_Geometry member function that will
    answer your query.  See the Dump() function in example_read.cpp
    for code that demonstrates how to use the Cast() function
    to get at the actual class definitions.

  Study example_write\example_write.cpp.

    If you want to write points, meshes, NURBS curves, NURBS
    surfaces, or trimmed NURBS surfaces, you should be able
    to cut and paste most of what you need from the functions
    in example_write.cpp.  If you want to write trimmed surfaces
    or b-reps, then please study example_brep.cpp.

  Study example_brep\example_brep.cpp.

    If you want to write solid models or more general b-reps,
    then you should first work through example_write.cpp and
    then work through example_brep.cpp.

  The comments in the openNURBS Toolkit header files are intended
  to be the primary source of documentation.  I suggest that you
  use a development environment that has high quality tags
  capabilities and a good class browser.

  If you find a class or member function that could use more 
  documentation, please post a request for more information
  on the openNURBS support newsgroup 
  <news://news.openurbs.org/opennurbs>.

Support:

  If you have a question concerning the openNURBS Toolkit that
  is not answered in faq.txt or covered in the examples,
  then please post your question on the support newsgroup at
  <news://news.openurbs.org/opennurbs>.

  If you are at a location that does not permit accesss to
  <news://news.openurbs.org/opennurbs>, then you can use the web
  version of the newsgroup at
  <http://news2.mcneel.com/scripts/dnewsweb.exe?cmd=xover&group=opennurbs>
  
  You can search archived newsgroup messages at
  <http://news2.mcneel.com/scripts/dnewsweb.exe?cmd=f_search&utag=>

Files:

  Instructions:

    readme.txt
    faq.txt

  Source code for openNURBS Toolkit:

    opennurbs*.h
	opennurbs*.c
	opennurbs*.cpp

  Source code for zlib compression tools (used by openNURBS)

    zlib/*.h
    zlib/*.c

  "Standard" UNIX + GNU tools makefile:
	
    makefile (makes libraries and examples)

  Microsoft Visual C++ 6.0 makefiles:
	
    opennurbs_vc60.dsw - project workspace for building everything

	opennurbs_vc60.dsp         - makefile for DLL openNURBS library
	opennurbs_static_vc60.dsp  - makefile for static openNURBS library
	zlib\zlib_vc60.dsp         - makefile for zlib
	example*\example*_vc60.dsp - makefiles for examples

  Microsoft Visual Studio 2005 makefiles:
	
    opennurbs.sln - project workspace for building everything

	opennurbs.vcproj           - makefile for DLL openNURBS library
	opennurbs_staticlib.vcproj - makefile for static openNURBS library
	zlib\zlib.vcproj           - makefile for zlib
	example*\example*.vcproj   - makefiles for examples


  Example programs an code fragments:

    example_dump\example_dump.cpp

    example_read\example_read.cpp

    example_write\example_write.cpp
    example_write\example_texture.bmp

    example_brep\example_brep.cpp

    example_gl\example_gl.cpp
    opennurbs_gl.cpp
    opennurbs_gl.h

  ONX_Model class with easy to use Read() and Write()
  member functions and members for managing all the information
  in an openNURBS 3DM file.

	opennurbs_extensions.cpp
	opennurbs_extensions.h

Unix and Apple:

  The source code, readme.txt, and makefile files in the
  opennurbs.zip archive are ASCII files with CR+LF end-of-line
  (eol) markers.
  
  If your compiler or make does not like the ASCII eol
  flavor that comes in the openNURBS.zip archive, then
  you need to change the end of line marker.

  Generally, Windows make programs and compilers prefer 
  CR+LF eols, Unix make programs and compilers prefer
  LF eols, and Apple make programs and compilers prefer
  CR eols.

Using Gnu Tools:

  The openNURBS Toolkit source code and makefile have been 
  tested with Gnu's g++ version 2.95.3 using Linix 2.4.5
  on an Intel CPU.

  If you are using g++ on a Linix OS, then you can use the
  Linix "fromdos" command in a script like

    #!/bin/csh -f
    foreach file (makefile *.c *.h *.cpp *.txt)
      mv $file junk.temp
      fromdos < junk.temp > $file
      rm junk.temp
    end

  to change the eol markers.

  NOTE WELL: 

  OpenNURBS uses UNICODE to store text information in
  3DM files.  At the time of this release, support for
  UNICODE / ASCII conversion on UNIX platforms is spotty
  at best.

Using SGI's IRIX and SGI development tools:

  You can use the "to_unix" command to change the eol
  markers.  In the makefile, use the "echo" definition
  for RANLIB.  The SGI compiler is supported.  If you
  discover any problems using the SGI compiler, please
  post details on the support newsgroup at
  <news://news.openurbs.org/opennurbs>.

Using SUN's Solaris and SUN's development tools:

  You can use the "dos2unix" command in a script like

    #!/bin/csh -f
    foreach file (makefile *.c *.h *.cpp *.txt)
      dos2unix $file $file
    end

  to change the eol markers.
    
Endean issues:

  The openNURBS Toolkit is designed to work correctly on both
  big and little endean CPUs.  (Generally, Intel CPUs use little
  endean byte order and MIPS, Motorola, and Sparc CPUs
  use big endean byte order.)  If you encounter big endean 
  problems, please post the details on the support newsgroup at
  <news://news.openurbs.org/opennurbs>.

Examples:

  example_read\example_read.cpp:

    Create a program by compiling example_read.cpp and
	linking with the openNURBS library.  The code in 
	example_read.cpp illustrates how to read an openNURBS .3DM file.

  example_write\example_write.cpp:

    Create a program by compiling example_write.cpp and
	linking with the openNURBS library.  The code in 
	example_write.cpp illustrates how to write layers, units system
	and tolerances, viewports, spotlights, points, meshes, 
	NURBS curves, NURBS surfaces, trimmed NURBS surfaces, texture
	and bump map information, render material name, and material
	definitions to an openNURBS .3DM file.  
	
	The bitmap in example_write\example_texture.bmp is used for a
    rendering material texture in example_write\example_write.cpp.

  example_brep\example_brep.cpp:

    Create a program by compiling example_brep.cpp and
	linking with the openNURBS library.  The code in 
	example_write.cpp illustrates how to write a solid model.

  example_dump\example_dump.cpp:

    Create a program by compiling example_dump.cpp and
	linking with the openNURBS library.  The code in 
    example_dump demonstrates the low level structure of an
	openNURBS .3DM file.

  example_userdata\example_userdata.cpp:

    Create a program by compiling example_userdata.cpp and
	linking with the openNURBS library.  The code in 
    example_userdata demonstrates how to create and manage
	arbitrary user defined information in .3DM files.

  example_gl\example_gl.cpp: ( requires .\opennurbs_gl.cpp )

    You should understand example_read.cpp and chapters 1 through 11
    of Jackie Neider, Tom Davis, and Mason Woo's 
    _Open_GL_Programming_Guide_ ( ISBN 0-201-63274-8 ) before you
    work on example_gl.cpp.  The _Open_GL_Programming_Guide_ is
    published by Addison-Wesley and can be ordered from most book
    stores and web based booksellers.
    
    The purpose of this example is to demonstrate how to use the
    ON_GL() calls in .\opennurbs_gl.cpp.  The example_gl.cpp code is
    pedagogical and is not intended to be of commercial quality.
    It uses the Open GL auxiliary library to manage the event
    handling.
    
    The GL code in this example has been tested with MSVC 6.0. 
    For other platforms, you may need to make some changes to
    example_gl.cpp.

    Create the program by compiling example_gl.cpp and
    opennurbs_gl.cpp, and linking with the openNURBS library,
	Open GL auxiliary library, Open GL utility library, and the
	standard Open GL libraries.  The code in opennurbs_gl.cpp is
	pedantic.  It demonstrates how to use the Open GL triangle 
	display functions to render Rhino meshes and the Open GL NURBS
    utility functions to render Rhino curves, surfaces and B-reps.
    The code in example_gl.cpp is modeled on the examples found in
    the _Open_GL_Programming_Guide_.  The Open GL example code
	will require substantial enhancements before it would be
	suitable for use in commercial quality applitions.

    A quick and dirty mouse and keyboard viewing interface is
    provided.
    
    Zoom window:
      Left mouse down + drag + left mouse up
    
    Dolly sideways:
      Right mouse down + drag + right mouse up
    
    Rotate:
      Arrow keys
      
    View extents:
      Press the Z, z, E or e keys.
      
Biblography:

  Bohm, Wolfgang, Gerald Farin, Jurgen Kahman (1984).
  A survey of curve and surface methods in CAGD,
  Computer Aided Geometric Design Vol 1. 1-60

  DeBoor, Carl. (1978).
  A Practical Guide To Splines,
  Springer Verlag; ISBN: 0387953663

  Farin, Gerald. (1997).
  Curves and Surfaces for Computer-Aided Geometric Design: 
  A Practical Guide, 4th edition,
  Academic Press; ISBN: 0122490541
  
  Rockwood, Alyn and Peter Chambers. (1996)
  Interactive Curves and Surfaces: A Multimedia Tutorial on CAGD, 
  Morgan Kaufmann Publishers; ISBN: 1558604057

  Piegl, Les and Wayne Tiller. (1995).
  The NURBS Book, 
  Springer Verlag; ISBN: 3540615458
