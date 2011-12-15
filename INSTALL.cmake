The Installation Guide to BRL-CAD
=================================

Please read this document if you are interested in installing BRL-CAD.

This document covers the basics of installing BRL-CAD from either a
source or binary distribution.  Please see the 'Reporting Problems'
section if you run into any trouble installing BRL-CAD.

Some platforms have additional platform-specific documentation
provided in the doc/ directory of the source distribution that should
be consulted if that is the platform you are installing on.  This
presently includes the following:

  doc/README.MacOSX	-- Apple Mac OS X
  doc/README.IRIX	-- SGI IRIX and IRIX64
  doc/README.Linux	-- Various Linux distributions
  doc/README.Windows    -- Microsoft Windows


TABLE OF CONTENTS
-----------------
  Introduction
  Table of Contents
  Quick Installation
  Installing from Binary
  Installing from Source
  Configuration Options
  Compilation Options
  Installation Options
  Testing Functionality
  Post-Installation
  Reporting Problems


QUICK INSTALLATION
------------------

Note that the downloaded files have two check sums calculated
automatically by the Source Forge file release system.  The values are
used to verify the integrity of the files as distributed.  Get those
data by clicking on the 'i' button at the end of each file's name.
You should ensure those data are retained and kept with the files if
you re-distribute them.


If you downloaded a binary distribution of BRL-CAD for your system,
please read the INSTALLING FROM BINARY section of this document down
below.  The rest of this quick installation section is only relevant
to source code distributions of BRL-CAD.

For the impatient or simplistic, the following steps should compile, 
test, and install an optimized BRL-CAD quickly into the /usr/brlcad
directory if CMake is installed on your system:

  gunzip brlcad-X.Y.Z.tar.gz
  tar -xvf brlcad-X.Y.Z.tar
  mkdir brlcad-build
  cd brlcad-build
  cmake ../brlcad-X.Y.Z -DBRLCAD_BUNDLED_LIBS=ON
  make
  make benchmark
  make regress
  make install   # as root, e.g. sudo make install

If any of the listed steps fail, then something unexpected happened.
See the REPORTING PROBLEMS section of this document to report the
problem or the INSTALLING FROM SOURCE section for more comprehensive
instructions.

Once installed, add /usr/brlcad/bin to your path, and you should be
able to run any of the 400+ applications that constitute BRL-CAD.  For
example, to run the MGED solid modeler:

PATH=/usr/brlcad/bin:$PATH ; export PATH
mged

If you use tcsh or another C-shell based command shell, use this
instead:

set path=( /usr/brlcad/bin $path ) ; rehash
mged


INSTALLING FROM BINARY
----------------------

There are a variety of different kinds of binary distributions of
BRL-CAD.  Some of the binary disributions are sufficiently generic and
are simply a binary commpressed tarball distribution.  Others are
specific to a particular platform such as Debian, Mac OS X, FreeBSD,
and Windows etc.


Generic Binary Distributions:

For the unspecialized binary distributions that are basically
compressed tarballs of the installation root, they should contain the
entire hierarchy of the distribution.  That is to say that they
contain /usr/brlcad in its entirety so that if you decompress, you
will have a `usr' directory that contains a single `brlcad' directory:

gunzip BRL-CAD_7.2.4_Linux_ia64.tar.gz
tar -xvf BRL-CAD_7.2.4_Linux_ia64.tar
sudo mkdir /usr/brlcad
sudo mv BRL-CAD_7.2.4_Linux_ia64/* /usr/brlcad/

Of course, there are other compression options possible including zip
and bzip2.  By default, BRL-CAD expects to be installed into
/usr/brlcad. On some platforms the binary may be relocatable, but this
is not guaranteed. It's recommended that you start from a source 
distribution if you would like to install into an alternate installation
location.

However, if you do desire to install and run BRL-CAD from a different
location, give it a try.. ;)  If it doesn't work (some platforms are more
problematic than others), you will need to compile and install from a
source distribution.


Mac OS X Disk Mounting Image:

Mount the .dmg and run the Installer .pkg contained therein.  This
will install into /usr/brlcad and will only require confirming that
your environment is set up properly (i.e. add /usr/brlcad/bin to your
path) as described in this document's Installation Options section.


INSTALLING FROM SOURCE
----------------------

There are a couple of ways to obtain the BRL-CAD sources, usually via
one of the following starting points:

  1) from a SVN checkout/export, or
  2) from a source distribution tarball

Using the latest SVN sources is recommended where possible since it
will have the latest changes.  See the corresponding section below for
details on how to install from either starting point.


Starting From a SVN Checkout/Export:

With CMake, there is no longer any difference between building from
a subversion checkout and a tarball - follow the Source Distribution
instructions below.

Starting From a Source Distribution:

There are many different ways to build BRL-CAD and depending on what
you need/want will determine which configuration options you should
use.  See the CONFIGURATION OPTIONS section below for details on how
to go about selecting which options are appropriate for you.

By default, the default configuration will prepare the build system
for installation into the /usr/brlcad directory (the 
CMAKE_INSTALL_PREFIX option may be used to change that).  This 
tradition goes back a couple decades and is a convenient means to 
isolate the BRL-CAD solid modeling system from your system, resolves 
conflicts, facilitates uninstalls, and simplifies upgrades.  The 
default configuration is performed by running `cmake'.  It is not
required to do the build in a directory different from your source
directory, but it is much cleaner and highly recommended - this guide
will illustrate the build process with the assumption that the
BRL-CAD source code is in the directory brlcad-7.2.4 and the
directory intended to hold the build output is brlcad-build, located
in the same parent directory as brlcad-7.2.4:

  .
  ./brlcad-7.2.4
  ./brlcad-build

To start the build process, cd into brlcad-build and run the following

  cmake ../brlcad-7.2.4

By default, a "Debug" configuration will be selected when 
configuring that adds debugging flags and sets a default
install location suitable for a debug build.  Alternately, 
the "Release" configuration can be set to automatically add
optimization flags and release style install settings.

By default, all components and functionality will be built except
jove.  However, BRL-CAD does require and include several 3rd party
components.  If your system does not include a sufficient version of
those required 3rd party components, they will be automatically
configured for compilation.  

If the autodetection mechanisms fail to produce a working configuration,
the next simplest approach is typically to enable ALL the third party
components - this is typically a well tested configuration, but will
increase both the build time and final install size of BRL-CAD on 
the system.  To set this variable on the command line, use -D to
define BRLCAD_BUNDLED_LIBS for CMake:

  -DBRLCAD_BUNDLED_LIBS=Bundled

If the graphical interface (cmake-gui) is in use, it will list this
and other common options by default, allowing the user to check and
uncheck the ON/OFF status of various options.  This is often quicker
and more convenient than defining options on the command line, but
both will work.

You can also force on or off any individual 3rd party library by 
setting the BRL-CAD variable for that feature to either on or off:

  -DBRLCAD_<FEATURE>=ON

See file './CMakeFiles/AllVariables.txt' for a list of library names
and labels.  For example, to NOT use OpenGL, set

  -DBRLCAD_ENABLE_OPENGL=OFF

To obtain an optimized build (for example, for BRL-CAD Benchmark
performance evaluation), enable BRLCAD_ENABLE_OPTIMIZED_BUILD:

  -DBRLCAD_ENABLE_OPTIMIZED_BUILD=ON

See the CONFIGURATION OPTIONS below for more details on all of the
possible settings.

Once configured, you should be able to succesfully build BRL-CAD via
make:

  make

See the COMPILATION OPTIONS section in this document for more details
on compile-time options including options for parallel build support.


Testing the Compilation:

To test BRL-CAD before installation, you can run the BRL-CAD benchmark.
The benchmark will report if the results are correct, testing a 
majority of the core functionality of BRL-CAD in addition to testing 
your system's performance:

  make benchmark

Note that the benchmark target will build ONLY the pieces required for
the benchmark tests, unless a general make has already been performed.

See the TESTING FUNCTIONALITY section of this document for more
details on how to ensure that the build completed successfully and
additional tests that may be run.


Installing the Compilation:

After the build successfully completes and assuming the benchmark also
produces correct results, installation may begin.  Like any package,
you must have sufficient filesystem permissions to install.  To
install into a system location, you can generally either become a
super user via the su or sudo commands:

  sudo make install

See the INSTALLATION OPTIONS section of this document for more details
on BRL-CAD installation options and post-install environment
preparations.

BUILD TYPES
-----------

As mentioned earlier, the default CMake settings are very minimalist and
not usually the most useful.  There are two "build types", controlled by
the CMAKE_BUILD_TYPE variable, that are useful for specific purposes:

* Debug (-DCMAKE_BUILD_TYPE=Debug) - Debug is the configuration that most
  developers will want to use when working on BRL-CAD.  It will add
  debug flags to the compile, and sets the default install directory to
  be /usr/brlcad/dev-X.Y.Z - in order to run the resulting installed 
  binaries, the developer should ensure that the dev-X.Y.Z  path is 
  first in their PATH environment variable.

* Release (-DCMAKE_BUILD_TYPE=Release) - A release build is intended for
  final consumption by end users and as such has optimizations enabled.
  It also sets the install path to /usr/brlcad/rel-X.Y.Z - best practice
  for release installation is to set up symbolic links in /usr/brlcad to
  point to the most current BRL-CAD release, while allowing older versions
  to remain installed on the system in case they are needed.

In both of these cases any individual variable may be overridden - for
example, setting -DCMAKE_INSTALL_PREFIX=/usr/brlcad in a Debug build will
override the ../brlcad-install default.  Build types are a convenient way
to bundle sets of settings, but they do not prevent overrides if a more
custom setup is needed.

#=======================================================================
...to be continued with more cmake info (paralleling the autotools
INSTALL)...
#=======================================================================

CONFIGURATION OPTIONS
---------------------

--- BRLCAD_BUNDLED_LIBS ---

Enables compilation of all 3rd party sources that are provided within a BRL-CAD
source distribution.  If used this option sets all other 3rd party library 
build flags to ON by default.  However, that setting can be overridden by 
manually setting individual variables. Default is "AUTO" - 3rd party sources 
are compiled only if they are not detected as being available and functioning 
as expected.

Aliases:  ENABLE_ALL


--- BRLCAD_ENABLE_RUNTIME_DEBUG ---

Enables support for application and library debugging facilities.  
Disabling the run-time debugging facilities can provide a significant 
(10%-30%) performance boost at the expense of extensive error 
checking (that in turn help prevent corruption of your data).  
Default is ;ON;, and should only be disabled for read-only render 
work where performance is critical.

Aliases:  ENABLE_RUNTIME_DEBUG, ENABLE_RUN_TIME_DEBUG, ENABLE_RUNTIME_DEBUGGING
          ENABLE_RUN_TIME_DEBUGGING


--- BRLCAD_ENABLE_STRICT ---

Causes all compilation warnings for C code to be treated as errors.  This is now
the default for BRL-CAD source code, and developers should address issues
discovered by these flags whenever possible rather than disabling strict
mode.

Aliases:  ENABLE_STRICT, ENABLE_STRICT_COMPILE, ENABLE_STRICT_COMPILE_FLAGS


--- BRLCAD_REGEX ---

Option for enabling and disabling compilation of the Regular Expression Library
provided with BRL-CAD's source distribution.  Default is AUTO, responsive to the
toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system version if 
BRLCAD_BUNDLED_LIBS is also AUTO.

Aliases:  ENABLE_REGEX


--- BRLCAD_ZLIB ---

Option for enabling and disabling compilation of the zlib
library provided with BRL-CAD's source distribution.  Default is AUTO, responsive to
the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system version if 
BRLCAD_BUNDLED_LIBS is also AUTO.

Aliases:  ENABLE_ZLIB, ENABLE_LIBZ


--- BRLCAD_LEMON ---

Option for enabling and disabling compilation of the lemon parser
provided with BRL-CAD's source distribution.  Default is AUTO, responsive to the
toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system version if 
BRLCAD_BUNDLED_LIBS is also AUTO.

Aliases:  ENABLE_LEMON


--- BRLCAD_RE2C ---

Option for enabling and disabling compilation of the re2c scanner utility
provided with BRL-CAD's source distribution.  Default is AUTO, responsive to the
toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system version if 
BRLCAD_BUNDLED_LIBS is also AUTO.

Aliases:  ENABLE_RE2C


--- BRLCAD_PERPLEX ---

Option for enabling and disabling compilation of the perplex scanner generator
provided with BRL-CAD's source distribution.  Default is AUTO, responsive to the
toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system version if 
BRLCAD_BUNDLED_LIBS is also AUTO.  perplex requires a working re2c.

Aliases:  ENABLE_PERPLEX


--- BRLCAD_XSLTPROC ---

Option for enabling and disabling compilation of the xsltproc XML
transformation utilty provided with BRL-CAD's source distribution.  Used 
for DocBook documentation processing.  Default is AUTO, responsive to the
toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system version if 
BRLCAD_BUNDLED_LIBS is also AUTO.

Aliases:  ENABLE_XSLTPROC


--- BRLCAD_XMLLINT ---

Option for enabling and disabling compilation of the xmllint XML
validation utilty provided with BRL-CAD's source distribution.  Used 
for DocBook documentation validation.  Default is AUTO, responsive to the
toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system version if 
BRLCAD_BUNDLED_LIBS is also AUTO.

Aliases:  ENABLE_XMLLINT


--- BRLCAD_TERMLIB ---

Option for enabling and disabling compilation of the termlib library
provided with BRL-CAD's source distribution.  Default is AUTO, responsive to the
toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system version if 
BRLCAD_BUNDLED_LIBS is also AUTO. (Except when building with Visual Studio,
where it is disabled. Windows does not support the termlib API.)

Aliases:  ENABLE_TERMLIB


--- BRLCAD_PNG ---

Option for enabling and disabling compilation of the Portable Network
Graphics library provided with BRL-CAD's source distribution.  Default is
AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing 
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.

Aliases:  ENABLE_PNG


--- BRLCAD_UTAHRLE ---

Option for enabling and disabling compilation of the Utah Raster Toolkit
library provided with BRL-CAD's source code.  Default is AUTO, responsive 
to the toplevel BRLCAD_BUNDLED_LIBS option and testing first for a system 
version if BRLCAD_BUNDLED_LIBS is also AUTO.

Aliases:  ENABLE_UTAHRLE


--- BRLCAD_TCL ---

Option for enabling and disabling compilation of the Tcl library 
provided with BRL-CAD's source code.  Default is AUTO, responsive 
to the toplevel BRLCAD_BUNDLED_LIBS option and testing first for 
a system version if BRLCAD_BUNDLED_LIBS is also AUTO.

Aliases:  ENABLE_TCL


--- BRLCAD_ITCL ---

Option for enabling and disabling compilation of the IncrTcl package for
Tcl objects provided with BRL-CAD's source distribution.  Default is AUTO,
auto-enabling if the BRLCAD_TCL option is set to BUNDLED and testing first for 
a system version if BRLCAD_TCL is set to AUTO or SYSTEM.  If BRLCAD_ITCL is 
set to BUNDLED, local copy is built even if a system version is present.

Aliases:  ENABLE_ITCL


--- BRLCAD_ITK ---

Option for enabling and disabling compilation of the IncrTcl itk package for
Tk objects provided with BRL-CAD's source distribution.  Default is AUTO,
auto-enabling if the BRLCAD_TCL option is set to BUNDLED and testing first for 
a system version if BRLCAD_TCL is set to AUTO or SYSTEM.  If BRLCAD_ITCL is 
set to BUNDLED, local copy is built even if a system version is present.  This
package will be disabled if BRLCAD_ENABLE_TK is OFF.

Aliases:  ENABLE_ITK


--- BRLCAD_TOGL ---

Option for enabling and disabling compilation of the Togl package for
Tcl/Tk OpenGL support provided with BRL-CAD's source distribution.  Default is AUTO,
auto-enabling if the BRLCAD_TCL option is set to BUNDLED and testing first for 
a system version if BRLCAD_TCL is set to AUTO or SYSTEM.  If BRLCAD_ITCL is 
set to BUNDLED, local copy is built even if a system version is present.  This
package will be disabled if either BRLCAD_ENABLE_OPENGL or BRLCAD_ENABLE_TK 
are OFF.

Aliases:  ENABLE_TOGL


--- BRLCAD_IWIDGETS ---

Option for enabling and disabling compilation of the IWidgets Tk widget 
package provided with BRL-CAD's source distribution.  Default is AUTO,
auto-enabling if the BRLCAD_TCL option is set to BUNDLED and testing first 
for a system version if BRLCAD_TCL is set to AUTO or SYSTEM.  If BRLCAD_ITCL 
is set to BUNDLED, local copy is built even if a system version is present.  
This package will be disabled if BRLCAD_ENABLE_TK is OFF.

Aliases:  ENABLE_IWIDGETS


--- BRLCAD_TKHTML ---

Option for enabling and disabling compilation of the Tkhtml HTML viewing
package provided with BRL-CAD's source distribution.  Default is AUTO,
auto-enabling if the BRLCAD_TCL option is set to BUNDLED and testing first 
for a system version if BRLCAD_TCL is set to AUTO or SYSTEM.  If BRLCAD_ITCL 
is set to BUNDLED, local copy is built even if a system version is present.  
This package will be disabled if BRLCAD_ENABLE_TK is OFF.

Aliases:  ENABLE_TKHTML


--- BRLCAD_TKPNG ---

Option for enabling and disabling compilation of the tkpng PNG image viewing
package provided with BRL-CAD's source distribution.  Default is AUTO,
auto-enabling if the BRLCAD_TCL option is set to BUNDLED and testing first 
for a system version if BRLCAD_TCL is set to AUTO or SYSTEM.  If BRLCAD_ITCL 
is set to BUNDLED, local copy is built even if a system version is present.  
This package will be disabled if BRLCAD_ENABLE_TK is OFF.

Aliases:  ENABLE_TKHTML


--- BRLCAD_TKTABLE ---

Option for enabling and disabling compilation of the Tktable graphical table
widget package provided with BRL-CAD's source distribution.  Default is AUTO,
auto-enabling if the BRLCAD_TCL option is set to BUNDLED and testing first 
for a system version if BRLCAD_TCL is set to AUTO or SYSTEM.  If BRLCAD_ITCL 
is set to BUNDLED, local copy is built even if a system version is present.  
This package will be disabled if BRLCAD_ENABLE_TK is OFF.

Aliases:  ENABLE_TKHTML


--- BRLCAD_OPENNURBS ---

Option for enabling and disabling compilation of the openNURBS
library provided with BRL-CAD's source code.  Default is
AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and 
testing first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.

Aliases:  ENABLE_OPENNURBS


--- BRLCAD_SCL ---

Option for enabling and disabling compilation of the NIST Step Class
Libraries provided with BRL-CAD's source code.  Default is
AUTO, responsive to the toplevel BRLCAD_BUNDLED_LIBS option and testing 
first for a system version if BRLCAD_BUNDLED_LIBS is also AUTO.

Aliases:  ENABLE_SCL, ENABLE_STEP, ENABLE_STEP_CLASS_LIBRARIES



*** Note - Do not add or edit configuration option descriptions and alias
   lists in this file - those entries are auto-generated from information in
   the toplevel CMakeLists.txt file and src/other/CMakeLists.txt - any changes
   should be made in those files.  The CMake configuration process will
   automatically re-generate INSTALL with the new descriptions and alias 
   information.