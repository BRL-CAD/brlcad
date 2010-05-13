README.txt: Togl

    This is a Togl 2.X binary distribution for both users and developers.
    It is specific to a particular operating system (e.g., Windows, Mac
    OS X, Linux, etc.).  Since the C ABI should be same for all compilers
    on the same system, using Togl via the Tcl interface should work
    regardless of which compiler Tcl was compiled with.

    The files are named:

    	ToglTOGL_VERSION-TCL_VERSION-OS.SUFFIX

    For example, TOGL_VERSION=2.0, TCL_VERSION=8.4, OS=Linux,
    and SUFFIX=.tar.gz gives:

    	Togl2.0-8.4-Linux.tar.gz

    Togl is also available at:
        http://sourceforge.net/projects/togl/

    You can get any release of Togl from the file distributions
    link at the above URL.

    A copy of the online documentation is in the doc directory.

For users:

    Only the lib/Togl2.X directory (and its contents) need to be installed
    in your Tcl library.  Execute the following Tcl script to find the
    directories Tcl looks for packages in:

	puts $tcl_libPath

    and then copy the lib/Togl2.X directory into one of those directories.

For developers:

    The lib/Togl2.X directory (and its contents) is all that needs to be
    redistributed in your application distribution.

    If you wish to link with Togl, then you will need the include files
    and a link library for your compiler.  The compilers used are (OS-
    WINDOWING_SYSTEM):

    MacOSX: gcc 4.0.1, Mac OS X 10.4, ppc/i386
    Linux: gcc 3.3.6, Red Hat 7.1, i386
    Linux64: gcc 4.2.3 -Wl,--hash-style=both, Red Hat Server 5.1, x86_64
    Windows: Microsoft Visual Studio .NET 2003, Windows XP SP2, i386

File hierarchy:

	README.txt		this file
        bin/                    unused (empty)
        lib/
            Togl2.X/		Tcl package (place on Tcl's autopath)
		LICENSE		redistribution license
                pkgIndex.tcl	Tcl package index
                Togl2X.dll	Windows Tcl package binary
            Toglstub2X.a	Windows gcc/mingw link library
            Toglstub2X.lib	Windows Visual Studio link library
	    libToglstub2X.a	UNIX (Linux, IRIX, etc.) link library
	include/
	    togl.h		Main header file, includes others
	    toglDecls.h		API function declarations
	    togl_ws.h		Which windowing system togl was compiled with
	doc/			Documentation
	    *.html		Start with index.html

The contents of the include and lib directories can be placed verbatim
in the Tcl installataion hierachy.

Documentation is in the doc directory.  Start with doc/index.html in
your web browser.
