This is a tool developed to export CREO geometry into the BRL-CAD .g file
format.  It will require CREO 3 to run, and to build it requires CREO 3 with
the development toolkits installed and proper licenses available, as well as a
BRL-CAD install built with the same compiler as that used to build CREO.

The CMake superstructure manages files and creates the installer, but it is
the nmake file (creo-brl-nmake.in) the contains the core of the compilation
logic.

Misc Notes:

When installing CREO 3, the development libraries are *not* part of the
default install - be sure to keep an eye out for the opportunity to install
extra components and enable them.  If you forget they can be added
post-install without requiring a full reinstall, but without those components
the converter cannot be built.

Before building a package of the converter for distribution, you need to
"unlock" the dll.  The CMake logic provides a convenience build target called
"UNLOCK" that will do this for you, but (at least as of CREO3) it will lock up
the Pro/Toolkit license for 15 minutes.

To avoid needing to unlock the dll after each build for testing, you need to
set up your CREO instance to load the toolkit.  See
https://www.ptcusercommunity.com/thread/121253
