Using installed BRL-CAD rel version at rev 60688.

To use dstep I installed the following:

via git clone:
-------------

git clone https://github.com/jacob-carlborg/dstep.git
git clone https://github.com/SiegeLord/Tango-D2.git

via Debian packages:
-------------------

dub         # see http://d-apt.sourceforge.net/
libcurl-dev # openssl version

via local install:
-----------------

libclang

TODO
====

Decide how the D interface system fits into the build system:

1.  We need to know some system characteristics to properly create
    modules.

2.  Is completely-automatic generation of interface modules possible?

3.  What is the status of D standard modules vs. the C library?

Proposed layout in the build directory

  # all public api (for headers found in installed dir
  # "$BRLCAD_ROOT/include/brlcad" and its sub-dirs)
  ./di/bu.d       # module: brlcad.bu (includes all bu/*)
  ...

  # other brlcad-specific (for headers found in installed dirs

  # "$BRLCAD_ROOT/include"
  ./di/other/X.d  # module: brlcad.other.tcl
  ...

  # for subdirs
  # "$BRLCAD_ROOT/include/X"
  ./di/X/Y.d      # module: brlcad.X.Y
  ...






