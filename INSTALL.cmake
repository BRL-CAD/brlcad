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
  cmake ../brlcad-X.Y.Z -DBRLCAD-ENABLE_OPTIMIZED=ON
  make
  make benchmark
  make regress
  make install   # as root, e.g. sudo make install

If any of the listed steps fail, then something unexpected happened.
See the REPORTING PROBLEMS section of this document to report the
problem or the INSTALLING FROM SOURCE section for more comprehensive
instructions.

Once installed, add /usr/brlcad/bin to your path, and you should be
able to run one of the 400+ applications that constitute BRL-CAD.  For
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

By default, an unoptimized build of BRL-CAD will be configured
for compilation.  This is a "bare bones" compile that does not
add debug flags, optimization, or other features that are generally
useful.  There are several "build profiles" that may be invoked
to automatically set up several options for particular purposes -
see the BUILD TYPES section below.

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
define BRLCAD-ENABLE_ALL_LOCAL_LIBS for CMake:

-DBRLCAD-ENABLE_ALL_LOCAL_LIBS=ON

If the graphical interface (cmake-gui) is in use, it will list this
and other common options by default, allowing the user to check and
uncheck the ON/OFF status of various options.  This is often quicker
and more convenient than defining options on the command line, but
both will work.

You can also force on or off any individual 3rd party library by 
setting either of two variables:

-DBRLCAD_BUILD_LOCAL_<FEATURE>_FORCE_ON=ON
-DBRLCAD_BUILD_LOCAL_<FEATURE>_FORCE_OFF=ON

These options are not available by default in the graphical interface,
but can be accessed by enabling the Advanced view, which will list all
available options.  This list can be made somewhat more managable by
also enabling the Grouped option, which organizes variables into
groups based on name.  In this case, the above two variables would be
found under the BRLCAD group.

To obtain an optimized build (for example, for BRL-CAD Benchmark 
performance evaluation), enable BRLCAD-ENABLE_OPTIMIZED_BUILD

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
  be ../brlcad-install instead of /usr/brlcad.  (This avoids the need to
  constantly enable sudo to do an install, and lets a developer work on
  their build even when a system install is also present.)  In order to
  run the resulting install binaries, the developer should ensure that
  the local install path is first in their PATH environment variable -
  see INSTALLATION OPTIONS below.

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


CONFIGURATION OPTIONS
---------------------

The `configure' build script is used to set compilation options and
supports a wide selection of compilation directives.  See the ENABLE
OPTIONS and WITH OPTIONS lists below for the arguments presently
available to configure in more descriptive detail.  The brief summary
is that "enable" options focus INWARD and "with" options focus
OUTWARD.  There are various --enable-* flags that toggle how BRL-CAD
code is compiled.  There are various --with-* flags that toggle what
system services are used and how to find them.

In order to avoid compilation problems, you should set all compilation
options on the `configure' command line, using `VAR=value'.  For
example:

     ./configure CC=/usr/ucb/bin/cc

will cause the specified executable program to be used as the C
compiler.  The most common configuration tested is a default
debuggable build intended for installation into /usr/brlcad with no
external dependencies assumed:

    ./configure --enable-all --disable-strict

By default, BRL-CAD is configured to build the entire package and will
install completely isolated into the /usr/brlcad directory.
Configuration will prepare the build for an unoptimized compilation by
default and will attempt to utilize required system libraries if they
are available, otherwise compiling the required library dependencies
as needed.  Run `./configure --help' for a list of all of the possible
configuration options.


ENABLE OPTIONS:

The below --enable-* options can be used to enable or disable aspects
of compilation that relate to what and how BRL-CAD will be compiled.
Using --disable-* is the same as --enable-*=no and either can be used
interchangably.  As BRL-CAD is a large bazaar of functionality, there
are a lot of options and configurations possible.  See the section
labeled WITH OPTIONS below for a list of options that enable/disable
optional system functionality.

--enable-almost-everything enables compilation of all 3rd party
sources that are provided within a BRL-CAD source distribution.  If
used, this option effectively turns on all of the below documented
--enable-*-build options.  Default is "auto", 3rd party sources are
compiled only if they are not detected as being available and
functioning as expected.  Aliases:
	--enable-all
	--enable-all-build
	--enable-build-all
	--enable-all-builds
	--enable-everything
	--enable-everything-build
	--enable-build-everything

--enable-only-benchmark prepares the build system to only compile and
install the minimal set of libraries and binaries needed to run the
BRL-CAD Benchmark suite.  Default is "no".  Aliases:
	--enable-only-bench
	--enable-only-benchmarks
	--enable-bench-only
	--enable-benchmark-only
	--enable-benchmarks-only

--enable-only-rtserver prepares the build system to only compile and
install the minimal set of libraries needed for the Java-interfacing
"rtserver" interface (used by ARL for analysis purposes).  Default is
"no".  Aliases:
	--enable-only-rts
	--enable-only-librtserver
	--enable-rts-only
	--enable-rtserver-only
	--enable-librtserver-only

--enable-runtime-debug enables support for application and library
debugging facilities.  Disabling the run-time debugging facilities can
provide a significant (10%-30%) performance boost at the expense of
extensive error checking (that in turn help prevent corruption of your
data).  Default is "yes", and should only be disabled for read-only
render work where performance is critical.  Aliases:
	--enable-run-time-debug
	--enable-runtime-debugging
	--enable-run-time-debugging

--enable-strict-build causes all compilation warnings to be treated as
errors for certain portions of code that has undergone a more thorough
review.  This behavior of this flag is heavily influenced by
compilation settings (such as --enable-warnings) including
optimization flags (such as --enable-optimized).  Default is "yes".
Aliases:
        --enable-strict
        --enable-strict-compile
        --enable-strict-compile-flags

--enable-64bit-build attempts to force compilation to produce 64-bit
binaries and libraries.  Note that it merely *ATTEMPTS* to force
64-bit compilation but in all practicality and likelihood, you will
need to manually specify CFLAGS/CPPFLAGS/CXXFLAGS/LDFLAGS to specify
64-bit compilation.  Default is "auto", where the default compilation
mode as set by the compiler will be utilized.  Aliases:
	--enable-64bit
	--enable-64
	--enable-64-build
	--enable-64-bit
	--enable-64-bit-build

--enable-regex-build turns on compilation of the regular expression
library that is provided in a BRL-CAD source distribution.  Default is
"auto", meaning that libregex will be compiled and installed only if a
suitable system regex library is not found.  Aliases:
	--enable-regex
	--enable-libregex
	--enable-libregex-build

--enable-png-build turns on compilation of the Portable Network
Graphics library that is provided in a BRL-CAD source distribution.
Default is "auto", meaning that libpng will be compiled and installed
only if a suitable system PNG library is not found.  Aliases:
	--enable-png
	--enable-libpng
	--enable-libpng-build

--enable-zlib-build turns on compilation of the zlib library that is
provided in a BRL-CAD source distribution.  Default is "auto", meaning
that libz will be compiled and installed only if a suitable system
zlib is not found.  Aliases:
	--enable-zlib
	--enable-libz
	--enable-libz-build

--enable-urt-build turns on compilation of the Utah Raster Toolkit
that is provided in a BRL-CAD source distribution.  Default is "auto",
meaning that libutahrle will be compiled and installed only if a
suitable system rle library is not found.  Aliases:
	--enable-urt
	--enable-urtoolkit
	--enable-urtoolkit-build
	--enable-utahrle
	--enable-utahrle-build
	--enable-libutahrle
	--enable-libutahrle-build
	--enable-utah-raster-toolkit
	--enable-utah-raster-toolkit-build

--enable-opennurbs-build turns on compilation of the openNURBS library
that is provided in a BRL-CAD source distribution.  Default is "auto",
meaning that libopenNURBS will be compiled and installed only if a
suitable system openNURBS library is not found.  Aliases:
	--enable-opennurbs
	--enable-libopennurbs
	--enable-libopennurbs-build
	--enable-open-nurbs
	--enable-open-nurbs-build

--enable-step-build turns on compilation of the NIST Step Class 
Libraries that are provided in a BRL-CAD source distribution.  Default 
is "auto", meaning that step will be compiled and installed only if a
suitable system NIST SCL libraries are not found.  Aliases:
	--enable-step
	--enable-steplibs
	--enable-libstep
	--enable-libstep-build

--enable-termlib-build turns on compilation of the termlib library
that is provided in a BRL-CAD source distribution.  Default is "auto",
meaning that libtermlib will be compiled and installed only if a
suitable system termcap or termlib library is not found.  Aliases:
	--enable-termlib
	--enable-termcap
	--enable-termcap-build
	--enable-libtermlib
	--enable-libtermlib-build
	--enable-libtermcap
	--enable-libtermcap-build

--enable-tcl-build turns on compilation of the Tcl library that is
provided in a BRL-CAD source distribution.  Default is "auto", meaning
that libtcl will be compiled and installed only if a suitable system
Tcl library is not found.  Aliases:
	--enable-tcl
	--enable-libtcl
	--enable-libtcl-build

--enable-tk-build turns on compilation of the Tk library that is
provided in a BRL-CAD source distribution.  Default is "auto", meaning
that libtk will be compiled and installed only if a suitable system Tk
library is not found.  Aliases:
	--enable-tk
	--enable-libtk
	--enable-libtk-build

--enable-aquatk-build turns on compilation of the Aqua Tk options for
the Tk library on Mac OSX.  Default is "off". Aliases:
	--enable-aquatk
	--enable-aqua-tk
	--enable-aqua-tk-build
	--enable-tkaqua
	--enable-tkaqua-build
	--enable-tk-aqua-build

--enable-itcl-build turns on compilation of the Incr Tcl/Tk library
that is provided in a BRL-CAD source distribution.  Default is "auto",
meaning that libitcl and libitk will be compiled and installed only if
a suitable system library (for either) is not found.  Aliases:
	--enable-itcl
	--enable-itk
	--enable-itk-build
	--enable-libitcl
	--enable-libitcl-build
	--enable-libitk
	--enable-libitk-build
	--enable-incrtcl
	--enable-incrtcl-build

--enable-iwidgets-install turns on installation of the Iwidgets
library that is provided in a BRL-CAD source distribution.  Default is
"auto", meaning that the iwidgets tcl scripts will be installed only
if a suitable system Iwidgets functionality is not found.  Aliases:
	--enable-iwidgets
	--enable-iwidgets-build

--enable-tkhtml-build turns on compilation of the tkhtml library that is
provided in a BRL-CAD source distribution.  Default is "yes".
Aliases:
	--enable-tkhtml
	--enable-tkhtml-build

--enable-tkpng-build turns on compilation of the tkpng library that is
provided in a BRL-CAD source distribution.  Default is "auto", meaning
that libtkpng will be compiled and installed only if a suitable system
tkpng library is not found.  Aliases:
	--enable-tkpng
	--enable-tkpng-build
	--enable-libtkpng
	--enable-libtkpng-build

--enable-tktable-build turns on compilation of the tktable library that is
provided in a BRL-CAD source distribution.  Default is "yes". Aliases:
	--enable-tktable
	--enable-tktable-build

--enable-tnt-install turns on installation of the Template Numerical
Toolkit that is provided in a BRL-CAD source distribution.  Default is
"auto", meaning that the TNT headers will be installed only if
suitable system TNT functionality is not found.  Aliases:
	--enable-tnt
	--enable-tnt-build
	--enable-template-numerical-toolkit
	--enable-template-numerical-toolkit-build
	--enable-template-numerical-toolkit-install

--enable-jove-build turns on compilation of Jonathan's Own Version of
Emacs (jove), a lightweight text editor with functionality similar to
Emacs but with different key bindings that is included in a BRL-CAD
source distribution.  Default is "auto", meaning that jove will be
compiled and installed only if suitable system editor functionality is
not found.  Aliases:
	--enable-jove

--enable-ef-build turns on compilation of the Endgame Framework module
used to provide BRL-CAD geometry import support to the Endgame
Framework application development environment.  Default is "no".
Aliases:
	--enable-ef
	--enable-endgameframework
	--enable-endgameframework-build
	--enable-endgame-framework
	--enable-endgame-framework-build

--enable-unigraphics-build turns on compilation of the Unigraphics (NX)
importer used to provide conversion support from Unigraphics format to
a tessellated BRL-CAD geometry format.  Default is "no".  Aliases:
	--enable-unigraphics
	--enable-ug
	--enable-ug-build
	--enable-nx
	--enable-nx-build

--enable-models-install turns on installation of provided example
geometry models.  Default is "yes".  Aliases:
	--enable-models
	--enable-models-build
	--enable-geometry
	--enable-geometry-build
	--enable-geometry-install

--enable-documentation turns on documentation requiring Docbook based
compilation.  Default is "auto", meaning it will build the documentation
if suitable tools to do so are found. Aliases:
	--enable-documentation-install
	--enable-docs
	--enable-docs-install

--enable-optimized turns on optimized compilation.  Note that
optimized compilation may potentially change the warnings produced by
the compiler.  You may also need to specify --disable-strict or
--disable-warnings to avoid halting on compilation warnings.  Default
is "no".  Aliases:
	--enable-opt
	--enable-optimize
	--enable-optimization
	--enable-optimizations

--enable-debug turns on debug compilation (debugging symbols).
Default is "yes".  Aliases:
	--enable-debugging

--enable-profiling turns on profile compilation.  Default is "no".
Aliases:
	--enable-profile
	--enable-profiled

--enable-parallel turns on support for SMP architectures.  Default is "yes".
Aliases:
	--enable-parallel-build
	--enable-smp
	--enable-smp-build

--enable-dtrace turns on Sun dtrace support.  Default is "no".
Aliases:
	None

--enable-verbose turns on verbose compilation output effectively
implying --enable-warnings and --enable-progress as described below.
Default is "no".  Aliases:
	--enable-verbosity
	--enable-output-verbose
	--enable-verbose-output
	--enable-build-verbose
	--enable-verbose-build

--enable-warnings turns on verbose compilation warnings, causing
additional compilation flags to be given to the compiler in order to
provoke additional compilation warnings.  Default is "no".  Aliases:
	--enable-warning
	--enable-verbose-warnings
	--enable-warnings-verbose
	--enable-build-warnings
	--enable-warnings-build

--enable-progress turns on verbose compilation progress status,
showing unquelled libtool compilation steps.  Default is "no".
Aliases:
	--enable-verbose-progress
	--enable-progress-verbose
	--enable-build-progress
	--enable-progress-build

--enable-rtgl turns on compilation of an experimental Ray Trace OpenGL
display manager, providing shaded displays within MGED.  Default is
"no".  Aliases:
       None


WITH OPTIONS:

The below --with-* options enable/disable/locate OPTIONAL SYSTEM
FUNCTIONALITY that should be compiled against.  Contrary to the
--enable-* options, none of the --with-* options are required to
obtain a fully functional BRL-CAD install.  Using --without-* is the
same as --with-*=no and either can be used interchangably (though be
careful of options that expect a PATH instead of "no").  See the
section labeled ENABLE OPTIONS above for a list of options that
enable/disable all other aspects of BRL-CAD compilation.

--with-jdk[=PATH] turns on compilation of code that requires the Java
Development Kit.  The specified PATH should be provided.  Default is
"auto" meaning that if suitable Java functionality is detected, then
code that uses Java will be compiled (librtserver).  Aliases:
	--with-java

--with-proe is in flux with --enable-proe-build.  Consult the help for
current information.

--with-x11[=PATH] informs configure where X11 headers/libraries may be
located.  Default is "auto" meaning that standard/common paths will be
searched for X11 functionality.  Aliases:
	--with-x

--with-ogl informs configure that the OpenGL framebuffer and
display manager interfaces should be compiled (and hence they should
link against a system OpenGL library).  This option has absolutely
nothing to do with shaded displays of geometry in MGED.  Default is
"auto" meaning the interfaces will be automatically compiled if
suitable OpenGL headers and libraries are found.  Aliases:
	--with-opengl

--with-wgl informs configure that it should attempt to compile the
Windows GL framebuffer and display manager interfaces.  This option is
hard-enabled in the MSVC build files for BRL-CAD.  Default is "auto"
for configure meaning that the wgl interfaces will be compiled if the
Windows opengl32 dll is found.  Aliases:
	--with-windowsgl


COMPILATION OPTIONS
-------------------

If you are on a multiprocessor machine, you can compile in parallel
depending on the version of `make' being used.  For the GNU and BSD
make, you can use the -j argument.  For example, to use 4 CPUs:

  make -j4

With the IRIX compiler, the -P option enables parallel build mode and
the PARALLEL environment variable sets the number of CPUs (default is
two):

  PARALLEL=4 make -P

If you would like to pass custom compiler or linker options, there are
several ways this may be achieved both during 'configure' and/or
during 'make'.  These flags include CFLAGS, CPPFLAGS, LDFLAGS, and
LIBS.  Configure supports these as either variables or as --with
options:

./configure CFLAGS="-O3" LDFLAGS="-pthread"
./configure --with-cppflags="-I/opt/include" --with-libs="-lz"

BRLCAD_C_FLAGS - may need to set up the others?


GNU and BSD make support the same flags as overrides of the default
provided flags as well:

make CFLAGS="-O0 -g -pg" LDFLAGS="-L/opt/malloc" LIBS="-lmalloc"


INSTALLATION OPTIONS
--------------------

By default, `make install' will install BRL-CAD's files into
`/usr/brlcad/bin', `/usr/brlcad/man', etc.  You can specify an
installation prefix other than `/usr/brlcad' by giving `configure' the
option `--prefix=PATH'.


Setting Up the Path:

Once installed, you will need to add /usr/brlcad/bin to your system
PATH.  For Bourne shell users, you can run the following:

  PATH=/usr/brlcad/bin:$PATH ; export PATH

For C shell users, this should do the same thing:

  set path=( /usr/brlcad/bin $path ) ; rehash

If you would like to provide BRL-CAD to multiple users, your shell
environment scripts should be edited so that /usr/brlcad/bin is added
to the system path.  This usually entails editing /etc/profile,
/etc/csh.login, /etc/bashrc, and possibly other files depending on
your platform.


Setting Up the Manual Pages Path:

It may be useful to add /usr/brlcad/man to your MANPATH in the same
manner as shown above for PATH.  Conversely, you can use the `brlman'
command that is already preconfigured to locate the BRL-CAD manpages.


Configuring permissions uniformly with group read/write access:

find /usr/brlcad -type d -exec chmod ug+rwx {} \; -exec chmod o+rx {} \; -o -type f -exec chmod ug+rw {} \; -exec chmod o+r {} \;

The above 'find' line will set up permissions across a /usr/brlcad
rooted installation such that 'user' and 'group' will have read-write
access and 'other' will have read access consistently across all
files.  This is a common configuration used by BRL-CAD developers that
allows shared management of installations amongst developers while
having all users maintain read-only access.



TESTING FUNCTIONALITY
---------------------

Testing a Compilation:

After BRL-CAD is compiled, you can test the build via:

  make test

This will run a series of tests on the compiled sources to make sure
that they behave as they should.  If those tests succeed, another
useful test is to ensure that the geometric library computations are
correct by running the BRL-CAD Benchmark:

  make benchmark

The benchmark will run for several minutes computing frames of
well-known ray-trace images, and comparing the validity of the results
as well as overall performance metrics.  The benchmark should report
CORRECT after each section of output and a overal VGR metric of
performance.


Testing an Installation:

After BRL-CAD is installed, simply running `rt' and/or `mged' are good
tests of basic functionality.  Running `rt' without any options should
display version information and a usage message.  Running `mged'
without any options will start the GUI-based solid modeler
application.  Running "benchmark" should invoke the BRL-CAD Benchmark
and test the performance of the binaries as installed on a given
system.


POST-INSTALLATION
-----------------


REPORTING PROBLEMS
------------------

Please report any bugs encountered to the project bug tracker at
http://sourceforge.net/tracker/?group_id=105292&atid=640802

Similarly, please post any request for feature enhancements or support
to http://sourceforge.net/tracker/?group_id=105292&atid=640805 and
http://sourceforge.net/tracker/?group_id=105292&atid=640803
respectively.


---
$Revision$
