# tcl.m4 --
#
#	This file provides a set of autoconf macros to help TEA-enable
#	a Tcl extension.
#
# Copyright (c) 1999-2000 Ajuba Solutions.
# Copyright (c) 2002 ActiveState Corporation.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

#------------------------------------------------------------------------
# TEA_PATH_TCLCONFIG --
#
#	Locate the tclConfig.sh file and perform a sanity check on
#	the Tcl compile flags
#
# Arguments:
#	none
#
# Results:
#
#	Adds the following arguments to configure:
#		--with-tcl=...
#
#	Defines the following vars:
#		TCL_BIN_DIR	Full path to the directory containing
#				the tclConfig.sh file
#------------------------------------------------------------------------

AC_DEFUN(TEA_PATH_TCLCONFIG, [
    if test x"${TEA_INITED}" = x ; then
	# Can't refer to exact macro name or it will be substituted
	AC_MSG_ERROR([Must call TEA INIT before PATH_TCLCONFIG])
    fi
    #
    # Ok, lets find the tcl configuration
    # First, look for one uninstalled.
    # the alternative search directory is invoked by --with-tcl
    #

    if test x"${no_tcl}" = x ; then
	# we reset no_tcl in case something fails here
	no_tcl=true
	AC_ARG_WITH(tcl, [  --with-tcl              directory containing tcl configuration (tclConfig.sh)], with_tclconfig=${withval})
	AC_MSG_CHECKING([for Tcl configuration])
	AC_CACHE_VAL(ac_cv_c_tclconfig,[

	    # First check to see if --with-tcl was specified.
	    if test x"${with_tclconfig}" != x ; then
		if test -f "${with_tclconfig}/tclConfig.sh" ; then
		    ac_cv_c_tclconfig=`(cd ${with_tclconfig}; pwd)`
		else
		    AC_MSG_ERROR([${with_tclconfig} directory doesn't contain tclConfig.sh])
		fi
	    fi

	    # then check for a private Tcl installation
	    if test x"${ac_cv_c_tclconfig}" = x ; then
		for i in \
			../libtcl \
			../../libtcl \
			../tcl \
			`ls -dr ../tcl[[8-9]].[[0-9]]* 2>/dev/null` \
			../../tcl \
			`ls -dr ../../tcl[[8-9]].[[0-9]]* 2>/dev/null` \
			../../../tcl \
			`ls -dr ../../../tcl[[8-9]].[[0-9]]* 2>/dev/null` ; do
		    if test -f "$i/unix/tclConfig.sh" ; then
			ac_cv_c_tclconfig=`(cd $i/unix; pwd)`
			break
		    fi
		done
	    fi

	    # check in a few common install locations
	    if test x"${ac_cv_c_tclconfig}" = x ; then
		for i in `ls -d ${exec_prefix}/lib 2>/dev/null` \
			`ls -d ${prefix}/lib 2>/dev/null` \
			`ls -d /usr/local/lib 2>/dev/null` \
			`ls -d /usr/contrib/lib 2>/dev/null` \
			`ls -d /usr/lib 2>/dev/null` \
			; do
		    if test -f "$i/tclConfig.sh" ; then
			ac_cv_c_tclconfig=`(cd $i; pwd)`
			break
		    fi
		done
	    fi

	    # check in a few other private locations
	    if test x"${ac_cv_c_tclconfig}" = x ; then
		for i in \
			${srcdir}/../tcl \
			`ls -dr ${srcdir}/../tcl[[8-9]].[[0-9]]* 2>/dev/null` ; do
		    if test -f "$i/unix/tclConfig.sh" ; then
		    ac_cv_c_tclconfig=`(cd $i/unix; pwd)`
		    break
		fi
		done
	    fi
	])

	if test x"${ac_cv_c_tclconfig}" = x ; then
	    TCL_BIN_DIR="# no Tcl configs found"
	    AC_MSG_WARN("Cannot find Tcl configuration definitions")
	    exit 0
	else
	    no_tcl=
	    TCL_BIN_DIR=${ac_cv_c_tclconfig}
	    AC_MSG_RESULT([found $TCL_BIN_DIR/tclConfig.sh])
	fi
    fi
])

#------------------------------------------------------------------------
# TEA_PATH_TKCONFIG --
#
#	Locate the tkConfig.sh file
#
# Arguments:
#	none
#
# Results:
#
#	Adds the following arguments to configure:
#		--with-tk=...
#
#	Defines the following vars:
#		TK_BIN_DIR	Full path to the directory containing
#				the tkConfig.sh file
#------------------------------------------------------------------------

AC_DEFUN(TEA_PATH_TKCONFIG, [
    #
    # Ok, lets find the tk configuration
    # First, look for one uninstalled.
    # the alternative search directory is invoked by --with-tk
    #

    if test x"${no_tk}" = x ; then
	# we reset no_tk in case something fails here
	no_tk=true
	AC_ARG_WITH(tk, [  --with-tk               directory containing tk configuration (tkConfig.sh)], with_tkconfig=${withval})
	AC_MSG_CHECKING([for Tk configuration])
	AC_CACHE_VAL(ac_cv_c_tkconfig,[

	    # First check to see if --with-tkconfig was specified.
	    if test x"${with_tkconfig}" != x ; then
		if test -f "${with_tkconfig}/tkConfig.sh" ; then
		    ac_cv_c_tkconfig=`(cd ${with_tkconfig}; pwd)`
		else
		    AC_MSG_ERROR([${with_tkconfig} directory doesn't contain tkConfig.sh])
		fi
	    fi

	    # then check for a private Tk library
	    if test x"${ac_cv_c_tkconfig}" = x ; then
		for i in \
			../libtk \
			../../libtk \
			../tk \
			`ls -dr ../tk[[8-9]].[[0-9]]* 2>/dev/null` \
			../../tk \
			`ls -dr ../../tk[[8-9]].[[0-9]]* 2>/dev/null` \
			../../../tk \
			`ls -dr ../../../tk[[8-9]].[[0-9]]* 2>/dev/null` ; do
		    if test -f "$i/unix/tkConfig.sh" ; then
			ac_cv_c_tkconfig=`(cd $i/unix; pwd)`
			break
		    fi
		done
	    fi
	    # check in a few common install locations
	    if test x"${ac_cv_c_tkconfig}" = x ; then
		for i in `ls -d ${exec_prefix}/lib 2>/dev/null` \
			`ls -d ${prefix}/lib 2>/dev/null` \
			`ls -d /usr/local/lib 2>/dev/null` \
			`ls -d /usr/contrib/lib 2>/dev/null` \
			`ls -d /usr/lib 2>/dev/null` \
			; do
		    if test -f "$i/tkConfig.sh" ; then
			ac_cv_c_tkconfig=`(cd $i; pwd)`
			break
		    fi
		done
	    fi
	    # check in a few other private locations
	    if test x"${ac_cv_c_tkconfig}" = x ; then
		for i in \
			${srcdir}/../tk \
			`ls -dr ${srcdir}/../tk[[8-9]].[[0-9]]* 2>/dev/null` ; do
		    if test -f "$i/unix/tkConfig.sh" ; then
			ac_cv_c_tkconfig=`(cd $i/unix; pwd)`
			break
		    fi
		done
	    fi
	])
	if test x"${ac_cv_c_tkconfig}" = x ; then
	    TK_BIN_DIR="# no Tk configs found"
	    AC_MSG_WARN("Cannot find Tk configuration definitions")
	    exit 0
	else
	    no_tk=
	    TK_BIN_DIR=${ac_cv_c_tkconfig}
	    AC_MSG_RESULT([found $TK_BIN_DIR/tkConfig.sh])
	fi
    fi

])

#------------------------------------------------------------------------
# TEA_LOAD_TCLCONFIG --
#
#	Load the tclConfig.sh file
#
# Arguments:
#	
#	Requires the following vars to be set:
#		TCL_BIN_DIR
#
# Results:
#
#	Subst the following vars:
#		TCL_BIN_DIR
#		TCL_SRC_DIR
#		TCL_LIB_FILE
#
#------------------------------------------------------------------------

AC_DEFUN(TEA_LOAD_TCLCONFIG, [
    AC_MSG_CHECKING([for existence of $TCL_BIN_DIR/tclConfig.sh])

    if test -f "$TCL_BIN_DIR/tclConfig.sh" ; then
        AC_MSG_RESULT([loading])
	. $TCL_BIN_DIR/tclConfig.sh
    else
        AC_MSG_RESULT([file not found])
    fi

    #
    # If the TCL_BIN_DIR is the build directory (not the install directory),
    # then set the common variable name to the value of the build variables.
    # For example, the variable TCL_LIB_SPEC will be set to the value
    # of TCL_BUILD_LIB_SPEC. An extension should make use of TCL_LIB_SPEC
    # instead of TCL_BUILD_LIB_SPEC since it will work with both an
    # installed and uninstalled version of Tcl.
    #

    if test -f $TCL_BIN_DIR/Makefile ; then
        TCL_LIB_SPEC=${TCL_BUILD_LIB_SPEC}
        TCL_STUB_LIB_SPEC=${TCL_BUILD_STUB_LIB_SPEC}
        TCL_STUB_LIB_PATH=${TCL_BUILD_STUB_LIB_PATH}
    fi

    #
    # eval is required to do the TCL_DBGX substitution
    #

    eval "TCL_LIB_FILE=\"${TCL_LIB_FILE}\""
    eval "TCL_LIB_FLAG=\"${TCL_LIB_FLAG}\""
    eval "TCL_LIB_SPEC=\"${TCL_LIB_SPEC}\""

    eval "TCL_STUB_LIB_FILE=\"${TCL_STUB_LIB_FILE}\""
    eval "TCL_STUB_LIB_FLAG=\"${TCL_STUB_LIB_FLAG}\""
    eval "TCL_STUB_LIB_SPEC=\"${TCL_STUB_LIB_SPEC}\""

    AC_SUBST(TCL_VERSION)
    AC_SUBST(TCL_BIN_DIR)
    AC_SUBST(TCL_SRC_DIR)

    AC_SUBST(TCL_LIB_FILE)
    AC_SUBST(TCL_LIB_FLAG)
    AC_SUBST(TCL_LIB_SPEC)

    AC_SUBST(TCL_STUB_LIB_FILE)
    AC_SUBST(TCL_STUB_LIB_FLAG)
    AC_SUBST(TCL_STUB_LIB_SPEC)

    #AC_SUBST(TCL_DBGX)
    AC_SUBST(TCL_LIBS)
    AC_SUBST(TCL_DEFS)
    AC_SUBST(TCL_EXTRA_CFLAGS)
    AC_SUBST(TCL_LD_FLAGS)
    AC_SUBST(TCL_SHLIB_LD_LIBS)
    #AC_SUBST(TCL_BUILD_LIB_SPEC)
    #AC_SUBST(TCL_BUILD_STUB_LIB_SPEC)
])

#------------------------------------------------------------------------
# TEA_LOAD_TKCONFIG --
#
#	Load the tkConfig.sh file
#
# Arguments:
#	
#	Requires the following vars to be set:
#		TK_BIN_DIR
#
# Results:
#
#	Sets the following vars that should be in tkConfig.sh:
#		TK_BIN_DIR
#------------------------------------------------------------------------

AC_DEFUN(TEA_LOAD_TKCONFIG, [
    AC_MSG_CHECKING([for existence of ${TK_BIN_DIR}/tkConfig.sh])

    if test -f "${TK_BIN_DIR}/tkConfig.sh" ; then
        AC_MSG_RESULT([loading])
	. $TK_BIN_DIR/tkConfig.sh
    else
        AC_MSG_RESULT([could not find ${TK_BIN_DIR}/tkConfig.sh])
    fi

    #
    # If the TK_BIN_DIR is the build directory (not the install directory),
    # then set the common variable name to the value of the build variables.
    # For example, the variable TK_LIB_SPEC will be set to the value
    # of TK_BUILD_LIB_SPEC. An extension should make use of TK_LIB_SPEC
    # instead of TK_BUILD_LIB_SPEC since it will work with both an
    # installed and uninstalled version of Tcl.
    #

    if test -f $TK_BIN_DIR/Makefile ; then
        TK_LIB_SPEC=${TK_BUILD_LIB_SPEC}
        TK_STUB_LIB_SPEC=${TK_BUILD_STUB_LIB_SPEC}
        TK_STUB_LIB_PATH=${TK_BUILD_STUB_LIB_PATH}
    fi

    #
    # eval is required to do the TK_DBGX substitution
    #

    eval "TK_LIB_FILE=\"${TK_LIB_FILE}\""
    eval "TK_LIB_FLAG=\"${TK_LIB_FLAG}\""
    eval "TK_LIB_SPEC=\"${TK_LIB_SPEC}\""

    eval "TK_STUB_LIB_FILE=\"${TK_STUB_LIB_FILE}\""
    eval "TK_STUB_LIB_FLAG=\"${TK_STUB_LIB_FLAG}\""
    eval "TK_STUB_LIB_SPEC=\"${TK_STUB_LIB_SPEC}\""

    AC_SUBST(TK_VERSION)
    AC_SUBST(TK_BIN_DIR)
    AC_SUBST(TK_SRC_DIR)

    AC_SUBST(TK_LIB_FILE)
    AC_SUBST(TK_LIB_FLAG)
    AC_SUBST(TK_LIB_SPEC)

    AC_SUBST(TK_STUB_LIB_FILE)
    AC_SUBST(TK_STUB_LIB_FLAG)
    AC_SUBST(TK_STUB_LIB_SPEC)

    AC_SUBST(TK_LIBS)
    AC_SUBST(TK_XINCLUDES)
])

#------------------------------------------------------------------------
# TEA_ENABLE_SHARED --
#
#	Allows the building of shared libraries
#
# Arguments:
#	none
#	
# Results:
#
#	Adds the following arguments to configure:
#		--enable-shared=yes|no
#
#	Defines the following vars:
#		STATIC_BUILD	Used for building import/export libraries
#				on Windows.
#
#	Sets the following vars:
#		SHARED_BUILD	Value of 1 or 0
#------------------------------------------------------------------------

AC_DEFUN(TEA_ENABLE_SHARED, [
    AC_MSG_CHECKING([how to build libraries])
    AC_ARG_ENABLE(shared,
	[  --enable-shared         build and link with shared libraries [--enable-shared]],
	[tcl_ok=$enableval], [tcl_ok=yes])

    if test "${enable_shared+set}" = set; then
	enableval="$enable_shared"
	tcl_ok=$enableval
    else
	tcl_ok=yes
    fi

    if test "$tcl_ok" = "yes" ; then
	AC_MSG_RESULT([shared])
	SHARED_BUILD=1
    else
	AC_MSG_RESULT([static])
	SHARED_BUILD=0
	AC_DEFINE(STATIC_BUILD)
    fi
    AC_SUBST(SHARED_BUILD)
])

#------------------------------------------------------------------------
# TEA_ENABLE_THREADS --
#
#	Specify if thread support should be enabled.  If "yes" is
#	specified as an arg (optional), threads are enabled by default.
#
# Arguments:
#	none
#	
# Results:
#
#	Adds the following arguments to configure:
#		--enable-threads
#
#	Sets the following vars:
#		THREADS_LIBS	Thread library(s)
#
#	Defines the following vars:
#		TCL_THREADS
#		_REENTRANT
#
#------------------------------------------------------------------------

AC_DEFUN(TEA_ENABLE_THREADS, [
    AC_ARG_ENABLE(threads, [  --enable-threads        build with threads],
	[tcl_ok=$enableval], [tcl_ok=$1])

    if test "$tcl_ok" = "yes"; then
	TCL_THREADS=1

	if test "${TEA_PLATFORM}" != "windows" ; then
	    # We are always OK on Windows, so check what this platform wants.
	    AC_DEFINE(USE_THREAD_ALLOC)
	    AC_DEFINE(_REENTRANT)
	    AC_DEFINE(_THREAD_SAFE)
	    AC_CHECK_LIB(pthread,pthread_mutex_init,tcl_ok=yes,tcl_ok=no)
	    if test "$tcl_ok" = "no"; then
		# Check a little harder for __pthread_mutex_init in the
		# same library, as some systems hide it there until
		# pthread.h is defined.	 We could alternatively do an
		# AC_TRY_COMPILE with pthread.h, but that will work with
		# libpthread really doesn't exist, like AIX 4.2.
		# [Bug: 4359]
		AC_CHECK_LIB(pthread, __pthread_mutex_init,
		    tcl_ok=yes, tcl_ok=no)
	    fi
	    
	    if test "$tcl_ok" = "yes"; then
		# The space is needed
		THREADS_LIBS=" -lpthread"
	    else
		AC_CHECK_LIB(pthreads, pthread_mutex_init,
		    tcl_ok=yes, tcl_ok=no)
		if test "$tcl_ok" = "yes"; then
		    # The space is needed
		    THREADS_LIBS=" -lpthreads"
		else
		    AC_CHECK_LIB(c, pthread_mutex_init,
			tcl_ok=yes, tcl_ok=no)
		    if test "$tcl_ok" = "no"; then
			AC_CHECK_LIB(c_r, pthread_mutex_init,
			    tcl_ok=yes, tcl_ok=no)
			if test "$tcl_ok" = "yes"; then
			    # The space is needed
			    THREADS_LIBS=" -pthread"
			else
			    TCL_THREADS=0
			    AC_MSG_WARN("Don t know how to find pthread lib on your system - thread support disabled")
			fi
		    fi
		fi
	    fi

	    # Does the pthread-implementation provide
	    # 'pthread_attr_setstacksize' ?

	    ac_saved_libs=$LIBS
	    LIBS="$LIBS $THREADS_LIBS"
	    AC_CHECK_FUNCS(pthread_attr_setstacksize)
	    LIBS=$ac_saved_libs
	    AC_CHECK_FUNCS(readdir_r)
	fi
    else
	TCL_THREADS=0
    fi
    # Do checking message here to not mess up interleaved configure output
    AC_MSG_CHECKING([for building with threads])
    if test "${TCL_THREADS}" = "1"; then
	AC_DEFINE(TCL_THREADS)
	AC_MSG_RESULT([yes])
    else
	AC_MSG_RESULT([no (default)])
    fi
    # TCL_THREADS sanity checking.  See if our request for building with
    # threads is the same as the way Tcl was built.  If not, warn the user.
    case ${TCL_DEFS} in
	*THREADS=1*)
	    if test "${TCL_THREADS}" = "0"; then
		AC_MSG_WARN([
    Building ${PACKAGE} without threads enabled, but building against a Tcl
    that IS thread-enabled.])
	    fi
	    ;;
	*)
	    if test "${TCL_THREADS}" = "1"; then
		AC_MSG_WARN([
    --enable-threads requested, but attempting building against a Tcl
    that is NOT thread-enabled.])
	    fi
	    ;;
    esac
    AC_SUBST(TCL_THREADS)
])

#------------------------------------------------------------------------
# TEA_ENABLE_SYMBOLS --
#
#	Specify if debugging symbols should be used
#	Memory (TCL_MEM_DEBUG) debugging can also be enabled.
#
# Arguments:
#	none
#	
#	Requires the following vars to be set:
#		CFLAGS_DEBUG
#		CFLAGS_OPTIMIZE
#		LDFLAGS_DEBUG
#		LDFLAGS_OPTIMIZE
#	
# Results:
#
#	Adds the following arguments to configure:
#		--enable-symbols
#
#	Defines the following vars:
#		CFLAGS_DEFAULT	Sets to CFLAGS_DEBUG if true
#				Sets to CFLAGS_OPTIMIZE if false
#		LDFLAGS_DEFAULT	Sets to LDFLAGS_DEBUG if true
#				Sets to LDFLAGS_OPTIMIZE if false
#		DBGX		Debug library extension
#
#------------------------------------------------------------------------

AC_DEFUN(TEA_ENABLE_SYMBOLS, [
    if test x"${TEA_INITED}" = x ; then
	# Can't refer to exact macro name or it will be substituted
	AC_MSG_ERROR([Must call TEA INIT before ENABLE_SYMBOLS])
    fi

    if test "${TEA_PLATFORM}" = "windows" ; then
	tcl_dbgx=d
    else
	tcl_dbgx=g
    fi

    AC_MSG_CHECKING([for build with symbols])
    AC_ARG_ENABLE(symbols, [  --enable-symbols        build with debugging symbols [--disable-symbols]],    [tcl_ok=$enableval], [tcl_ok=no])
    if test "$tcl_ok" = "no"; then
	CFLAGS_DEFAULT='$(CFLAGS_OPTIMIZE)'
	LDFLAGS_DEFAULT='$(LDFLAGS_OPTIMIZE)'
	DBGX=""
	TCL_DBGX=""
	AC_MSG_RESULT([no])
    else
	CFLAGS_DEFAULT='$(CFLAGS_DEBUG)'
	LDFLAGS_DEFAULT='$(LDFLAGS_DEBUG)'
	DBGX=${tcl_dbgx}
	TCL_DBGX=${tcl_dbgx}
	if test "$tcl_ok" = "yes"; then
	    AC_MSG_RESULT([yes (standard debugging)])
	fi
    fi

    AC_SUBST(TCL_DBGX)
    AC_SUBST(CFLAGS_DEFAULT)
    AC_SUBST(LDFLAGS_DEFAULT)

    if test "$tcl_ok" = "mem" -o "$tcl_ok" = "all"; then
	AC_DEFINE(TCL_MEM_DEBUG)
    fi

    if test "$tcl_ok" != "yes" -a "$tcl_ok" != "no"; then
	if test "$tcl_ok" = "all"; then
	    AC_MSG_RESULT([enabled symbols mem debugging])
	else
	    AC_MSG_RESULT([enabled $tcl_ok debugging])
	fi
    fi
])

#------------------------------------------------------------------------
# TEA_ENABLE_LANGINFO --
#
#	Allows use of modern nl_langinfo check for better l10n.
#	This is only relevant for Unix.
#
# Arguments:
#	none
#	
# Results:
#
#	Adds the following arguments to configure:
#		--enable-langinfo=yes|no (default is yes)
#
#	Defines the following vars:
#		HAVE_LANGINFO	Triggers use of nl_langinfo if defined.
#
#------------------------------------------------------------------------

AC_DEFUN(TEA_ENABLE_LANGINFO, [
    AC_ARG_ENABLE(langinfo,
	[  --enable-langinfo	  use nl_langinfo if possible to determine
			  encoding at startup, otherwise use old heuristic],
	[langinfo_ok=$enableval], [langinfo_ok=yes])

    HAVE_LANGINFO=0
    if test "$langinfo_ok" = "yes"; then
	if test "$langinfo_ok" = "yes"; then
	    AC_CHECK_HEADER(langinfo.h,[langinfo_ok=yes],[langinfo_ok=no])
	fi
    fi
    AC_MSG_CHECKING([whether to use nl_langinfo])
    if test "$langinfo_ok" = "yes"; then
	AC_TRY_COMPILE([#include <langinfo.h>],
		[nl_langinfo(CODESET);],[langinfo_ok=yes],[langinfo_ok=no])
	if test "$langinfo_ok" = "no"; then
	    langinfo_ok="no (could not compile with nl_langinfo)";
	fi
	if test "$langinfo_ok" = "yes"; then
	    AC_DEFINE(HAVE_LANGINFO)
	fi
    fi
    AC_MSG_RESULT([$langinfo_ok])
])

#--------------------------------------------------------------------
# TEA_CONFIG_CFLAGS
#
#	Try to determine the proper flags to pass to the compiler
#	for building shared libraries and other such nonsense.
#
# Arguments:
#	none
#
# Results:
#
#	Defines the following vars:
#
#       DL_OBJS -       Name of the object file that implements dynamic
#                       loading for Tcl on this system.
#       DL_LIBS -       Library file(s) to include in tclsh and other base
#                       applications in order for the "load" command to work.
#       LDFLAGS -      Flags to pass to the compiler when linking object
#                       files into an executable application binary such
#                       as tclsh.
#       LD_SEARCH_FLAGS-Flags to pass to ld, such as "-R /usr/local/tcl/lib",
#                       that tell the run-time dynamic linker where to look
#                       for shared libraries such as libtcl.so.  Depends on
#                       the variable LIB_RUNTIME_DIR in the Makefile.
#       SHLIB_CFLAGS -  Flags to pass to cc when compiling the components
#                       of a shared library (may request position-independent
#                       code, among other things).
#       SHLIB_LD -      Base command to use for combining object files
#                       into a shared library.
#       SHLIB_LD_LIBS - Dependent libraries for the linker to scan when
#                       creating shared libraries.  This symbol typically
#                       goes at the end of the "ld" commands that build
#                       shared libraries. The value of the symbol is
#                       "${LIBS}" if all of the dependent libraries should
#                       be specified when creating a shared library.  If
#                       dependent libraries should not be specified (as on
#                       SunOS 4.x, where they cause the link to fail, or in
#                       general if Tcl and Tk aren't themselves shared
#                       libraries), then this symbol has an empty string
#                       as its value.
#       SHLIB_SUFFIX -  Suffix to use for the names of dynamically loadable
#                       extensions.  An empty string means we don't know how
#                       to use shared libraries on this platform.
#       TCL_LIB_FILE -  Name of the file that contains the Tcl library, such
#                       as libtcl7.8.so or libtcl7.8.a.
#       TCL_LIB_SUFFIX -Specifies everything that comes after the "libtcl"
#                       in the shared library name, using the $VERSION variable
#                       to put the version in the right place.  This is used
#                       by platforms that need non-standard library names.
#                       Examples:  ${VERSION}.so.1.1 on NetBSD, since it needs
#                       to have a version after the .so, and ${VERSION}.a
#                       on AIX, since the Tcl shared library needs to have
#                       a .a extension whereas shared objects for loadable
#                       extensions have a .so extension.  Defaults to
#                       ${VERSION}${SHLIB_SUFFIX}.
#       TCL_NEEDS_EXP_FILE -
#                       1 means that an export file is needed to link to a
#                       shared library.
#       TCL_EXP_FILE -  The name of the installed export / import file which
#                       should be used to link to the Tcl shared library.
#                       Empty if Tcl is unshared.
#       TCL_BUILD_EXP_FILE -
#                       The name of the built export / import file which
#                       should be used to link to the Tcl shared library.
#                       Empty if Tcl is unshared.
#	CFLAGS_DEBUG -
#			Flags used when running the compiler in debug mode
#	CFLAGS_OPTIMIZE -
#			Flags used when running the compiler in optimize mode
#	EXTRA_CFLAGS -
#			Extra CFLAGS to pass to the compiler
#
#	Subst's the following vars:
#		DL_LIBS
#		CFLAGS_DEBUG
#		CFLAGS_OPTIMIZE
#		CFLAGS_WARNING
#
#		STLIB_LD
#		SHLIB_LD
#		SHLIB_CFLAGS
#		SHLIB_LDFLAGS
#		LDFLAGS_DEBUG
#		LDFLAGS_OPTIMIZE
#--------------------------------------------------------------------

AC_DEFUN(TEA_CONFIG_CFLAGS, [
    if test x"${TEA_INITED}" = x ; then
	# Can't refer to exact macro name or it will be substituted
	AC_MSG_ERROR([Must call TEA INIT before CONFIG_CFLAGS])
    fi

    # Step 0: Enable 64 bit support?

    AC_MSG_CHECKING([if 64bit support is enabled])
    AC_ARG_ENABLE(64bit,[  --enable-64bit          enable 64bit support (where applicable)], [do64bit=$enableval], [do64bit=no])
    AC_MSG_RESULT([$do64bit])
 
    # Step 0.b: Enable Solaris 64 bit VIS support?

    AC_MSG_CHECKING([if 64bit Sparc VIS support is requested])
    AC_ARG_ENABLE(64bit-vis,[  --enable-64bit-vis      enable 64bit Sparc VIS support], [do64bitVIS=$enableval], [do64bitVIS=no])
    AC_MSG_RESULT([$do64bitVIS])

    if test "$do64bitVIS" = "yes"; then
	# Force 64bit on with VIS
	do64bit=yes
    fi

    # Step 1: set the variable "system" to hold the name and version number
    # for the system.  This can usually be done via the "uname" command, but
    # there are a few systems, like Next, where this doesn't work.

    AC_MSG_CHECKING([system version (for dynamic loading)])
    if test -f /usr/lib/NextStep/software_version; then
	system=NEXTSTEP-`awk '/3/,/3/' /usr/lib/NextStep/software_version`
    else
	system=`uname -s`-`uname -r`
	if test "$?" -ne 0 ; then
	    AC_MSG_RESULT([unknown (can't find uname command)])
	    system=unknown
	else
	    # Special check for weird MP-RAS system (uname returns weird
	    # results, and the version is kept in special file).
	
	    if test -r /etc/.relid -a "X`uname -n`" = "X`uname -s`" ; then
		system=MP-RAS-`awk '{print $3}' /etc/.relid'`
	    fi
	    if test "`uname -s`" = "AIX" ; then
		system=AIX-`uname -v`.`uname -r`
	    fi
	    if test "${TEA_PLATFORM}" = "windows" ; then
		system=windows
	    fi
	    AC_MSG_RESULT([$system])
	fi
    fi

    # Step 2: check for existence of -ldl library.  This is needed because
    # Linux can use either -ldl or -ldld for dynamic loading.

    AC_CHECK_LIB(dl, dlopen, have_dl=yes, have_dl=no)

    # Step 3: set configuration options based on system name and version.

    do64bit_ok=no
    EXTRA_CFLAGS=""
    TCL_EXPORT_FILE_SUFFIX=""
    UNSHARED_LIB_SUFFIX=""
    TCL_TRIM_DOTS='`echo ${VERSION} | tr -d .`'
    ECHO_VERSION='`echo ${VERSION}`'
    TCL_LIB_VERSIONS_OK=ok
    CFLAGS_DEBUG=-g
    CFLAGS_OPTIMIZE=-O
    if test "$GCC" = "yes" ; then
	CFLAGS_WARNING="-Wall -Wconversion -Wno-implicit-int"
    else
	CFLAGS_WARNING=""
    fi
    TCL_NEEDS_EXP_FILE=0
    TCL_BUILD_EXP_FILE=""
    TCL_EXP_FILE=""
dnl FIXME: Replace AC_CHECK_PROG with AC_CHECK_TOOL once cross compiling is fixed.
dnl AC_CHECK_TOOL(AR, ar, :)
    AC_CHECK_PROG(AR, ar, ar)
    STLIB_LD='${AR} cr'
    case $system in
	windows)
	    # This is a 2-stage check to make sure we have the 64-bit SDK
	    # We have to know where the SDK is installed.
	    if test "$do64bit" = "yes" ; then
		if test "x${MSSDK}x" = "xx" ; then
		    MSSDK="C:/Progra~1/Microsoft SDK"
		fi
		# In order to work in the tortured autoconf environment,
		# we need to ensure that this path has no spaces
		MSSDK=`cygpath -w -s "$MSSDK" | sed -e 's!\\\!/!g'`
		if test ! -d "${MSSDK}/bin/win64" ; then
		    AC_MSG_WARN("could not find 64-bit SDK to enable 64bit mode")
		    do64bit="no"
		else
		    do64bit_ok="yes"
		fi
	    fi

	    if test "$GCC" != "yes" ; then
	        if test "${SHARED_BUILD}" = "0" ; then
		    runtime=-MT
	        else
		    runtime=-MD
	        fi

                if test "$do64bit" = "yes" ; then
		    # All this magic is necessary for the Win64 SDK RC1 - hobbs
		    export CC="${MSSDK}/Bin/Win64/cl.exe \
	                -I${MSSDK}/Include/prerelease \
                        -I${MSSDK}/Include/Win64/crt \
	                -I${MSSDK}/Include"
		    export RC="${MSSDK}/bin/rc.exe"
		    export lflags="-MACHINE:IA64 -LIBPATH:${MSSDK}/Lib/IA64 \
	                -LIBPATH:${MSSDK}/Lib/Prerelease/IA64"
		    export STLIB_LD="${MSSDK}/bin/win64/lib.exe -nologo ${lflags}"
		    export LINKBIN="${MSSDK}/bin/win64/link.exe ${lflags}"
		    CFLAGS_DEBUG="-nologo -Zi -Od -W3 ${runtime}d"
		    CFLAGS_OPTIMIZE="-nologo -O2 -Gs -W2 ${runtime}"
	        else
		    RC="rc"
		    STLIB_LD="lib -nologo"
    		    LINKBIN="link -link50compat"
		    CFLAGS_DEBUG="-nologo -Z7 -Od -W3 -WX ${runtime}d"
		    CFLAGS_OPTIMIZE="-nologo -O2 -Gs -GD -W2 ${runtime}"
		fi
	    fi

	    if test "$GCC" = "yes"; then
		# mingw gcc mode
		RC="windres"
		CFLAGS_DEBUG="-g"
		CFLAGS_OPTIMIZE="-O2"
		SHLIB_LD="$CC -shared"
		UNSHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}\$\{DBGX\}.a'
		LDFLAGS_CONSOLE="-wl,--subsystem,console ${lflags}"
		LDFLAGS_WINDOW="-wl,--subsystem,windows ${lflags}"
	    else
		SHLIB_LD="${LINKBIN} -dll -nologo"
		UNSHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}\$\{DBGX\}.lib'
		EXTRA_CFLAGS="-YX"
		# For information on what debugtype is most useful, see:
		# http://msdn.microsoft.com/library/en-us/dnvc60/html/gendepdebug.asp
		# This essentially turns it all on.
		LDFLAGS_DEBUG="-debug:full -debugtype:both -warn:2"
		LDFLAGS_OPTIMIZE="-release"
		LDFLAGS_CONSOLE="-link -subsystem:console ${lflags}"
		LDFLAGS_WINDOW="-link -subsystem:windows ${lflags}"
		PATHTYPE=-w
	    fi

	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".dll"
	    SHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}\$\{DBGX\}.dll'

	    TCL_LIB_VERSIONS_OK=nodots
	    # Bogus to avoid getting this turned off
	    DL_OBJS="tclLoadNone.obj"
    	    ;;
	AIX-*)
	    if test "${TCL_THREADS}" = "1" -a "$GCC" != "yes" ; then
		# AIX requires the _r compiler when gcc isn't being used
		if test "${CC}" != "cc_r" ; then
		    CC=${CC}_r
		fi
		AC_MSG_RESULT([Using $CC for compiling with threads])
	    fi
	    LIBS="$LIBS -lc"
	    SHLIB_CFLAGS=""
	    SHLIB_SUFFIX=".so"
	    SHLIB_LD_LIBS='${LIBS}'
	    if test "`uname -m`" = "ia64" ; then
		# AIX-5 uses ELF style dynamic libraries on IA-64, but not PPC
		SHLIB_LD="/usr/ccs/bin/ld -G -z text"
		# AIX-5 has dl* in libc.so
		DL_LIBS=""
		if test "$GCC" = "yes" ; then
		    LD_SEARCH_FLAGS='-Wl,-R,${LIB_RUNTIME_DIR}'
		else
		    LD_SEARCH_FLAGS='-R${LIB_RUNTIME_DIR}'
		fi
	    else
		SHLIB_LD="${TCL_SRC_DIR}/unix/ldAix /bin/ld -bhalt:4 -bM:SRE -bE:lib.exp -H512 -T512 -bnoentry"
		DL_LIBS="-ldl"
		LD_SEARCH_FLAGS='-L${LIB_RUNTIME_DIR}'
		TCL_NEEDS_EXP_FILE=1
		TCL_EXPORT_FILE_SUFFIX='${VERSION}\$\{DBGX\}.exp'
	    fi
	    DL_OBJS="tclLoadDl.o"
	    LDFLAGS=""

	    # AIX v<=4.1 has some different flags than 4.2+
	    if test "$system" = "AIX-4.1" -o "`uname -v`" -lt "4" ; then
		AC_LIBOBJ([tclLoadAix])
		DL_LIBS="-lld"
	    fi

	    # On AIX <=v4 systems, libbsd.a has to be linked in to support
	    # non-blocking file IO.  This library has to be linked in after
	    # the MATH_LIBS or it breaks the pow() function.  The way to
	    # insure proper sequencing, is to add it to the tail of MATH_LIBS.
	    # This library also supplies gettimeofday.
	    #
	    # AIX does not have a timezone field in struct tm. When the AIX
	    # bsd library is used, the timezone global and the gettimeofday
	    # methods are to be avoided for timezone deduction instead, we
	    # deduce the timezone by comparing the localtime result on a
	    # known GMT value.

	    AC_CHECK_LIB(bsd, gettimeofday, libbsd=yes, libbsd=no)
	    if test $libbsd = yes; then
	    	MATH_LIBS="$MATH_LIBS -lbsd"
	    	AC_DEFINE(USE_DELTA_FOR_TZ)
	    fi

	    # Check to enable 64-bit flags for compiler/linker on AIX 4+
	    if test "$do64bit" = "yes" -a "`uname -v`" -gt "3" ; then
		if test "$GCC" = "yes" ; then
		    AC_MSG_WARN("64bit mode not supported with GCC on $system")
		else 
		    do64bit_ok=yes
		    EXTRA_CFLAGS="-q64"
		    LDFLAGS="-q64"
		    RANLIB="${RANLIB} -X64"
		    AR="${AR} -X64"
		    SHLIB_LDFLAGS="-b64"
		fi
	    fi
	    ;;
	BSD/OS-2.1*|BSD/OS-3*)
	    SHLIB_CFLAGS=""
	    SHLIB_LD="shlicc -r"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    LDFLAGS=""
	    LD_SEARCH_FLAGS=""
	    ;;
	BSD/OS-4.*)
	    SHLIB_CFLAGS="-export-dynamic -fPIC"
	    SHLIB_LD="cc -shared"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    LDFLAGS="-export-dynamic"
	    LD_SEARCH_FLAGS=""
	    ;;
	dgux*)
	    SHLIB_CFLAGS="-K PIC"
	    SHLIB_LD="cc -G"
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    LDFLAGS=""
	    LD_SEARCH_FLAGS=""
	    ;;
	HP-UX-*.11.*)
	    # Use updated header definitions where possible
	    AC_DEFINE(_XOPEN_SOURCE_EXTENDED)

	    SHLIB_SUFFIX=".sl"
	    AC_CHECK_LIB(dld, shl_load, tcl_ok=yes, tcl_ok=no)
	    if test "$tcl_ok" = yes; then
		SHLIB_CFLAGS="+z"
		SHLIB_LD="ld -b"
		SHLIB_LD_LIBS='${LIBS}'
		DL_OBJS="tclLoadShl.o"
		DL_LIBS="-ldld"
		LDFLAGS="-Wl,-E"
		LD_SEARCH_FLAGS='-Wl,+s,+b,${LIB_RUNTIME_DIR}:.'
	    fi

	    # Users may want PA-RISC 1.1/2.0 portable code - needs HP cc
	    #EXTRA_CFLAGS="+DAportable"

	    # Check to enable 64-bit flags for compiler/linker
	    if test "$do64bit" = "yes" ; then
		if test "$GCC" = "yes" ; then
		    hpux_arch=`${CC} -dumpmachine`
		    case $hpux_arch in
			hppa64*)
			    # 64-bit gcc in use.  Fix flags for GNU ld.
			    do64bit_ok=yes
			    SHLIB_LD="${CC} -shared"
			    SHLIB_LD_LIBS=""
			    LD_SEARCH_FLAGS=''
			    ;;
			*)
			    AC_MSG_WARN("64bit mode not supported with GCC on $system")
			    ;;
		    esac
		else
		    do64bit_ok=yes
		    if test "`uname -m`" = "ia64" ; then
			EXTRA_CFLAGS="+DD64"
			LDFLAGS="+DD64 $LDFLAGS"
		    else
			EXTRA_CFLAGS="+DA2.0W"
			LDFLAGS="+DA2.0W $LDFLAGS"
		    fi
		fi
	    fi
	    ;;
	HP-UX-*.08.*|HP-UX-*.09.*|HP-UX-*.10.*)
	    SHLIB_SUFFIX=".sl"
	    AC_CHECK_LIB(dld, shl_load, tcl_ok=yes, tcl_ok=no)
	    if test "$tcl_ok" = yes; then
		SHLIB_CFLAGS="+z"
		SHLIB_LD="ld -b"
		SHLIB_LD_LIBS=""
		DL_OBJS="tclLoadShl.o"
		DL_LIBS="-ldld"
		LDFLAGS="-Wl,-E"
		LD_SEARCH_FLAGS='-Wl,+s,+b,${LIB_RUNTIME_DIR}:.'
	    fi
	    ;;
	IRIX-4.*)
	    SHLIB_CFLAGS="-G 0"
	    SHLIB_SUFFIX=".a"
	    SHLIB_LD="echo tclLdAout $CC \{$SHLIB_CFLAGS\} | `pwd`/tclsh -r -G 0"
	    SHLIB_LD_LIBS='${LIBS}'
	    DL_OBJS="tclLoadAout.o"
	    DL_LIBS=""
	    LDFLAGS="-Wl,-D,08000000"
	    LD_SEARCH_FLAGS='-L${LIB_RUNTIME_DIR}'
	    SHARED_LIB_SUFFIX='${VERSION}\$\{DBGX\}.a'
	    ;;
	IRIX-5.*)
	    SHLIB_CFLAGS=""
	    SHLIB_LD="ld -shared -rdata_shared"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS=""
	    LD_SEARCH_FLAGS='-Wl,-rpath,${LIB_RUNTIME_DIR}'
	    EXTRA_CFLAGS=""
	    LDFLAGS=""
	    ;;
	IRIX-6.*|IRIX64-6.5*)
	    SHLIB_CFLAGS=""
	    SHLIB_LD="ld -n32 -shared -rdata_shared"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS=""
	    LD_SEARCH_FLAGS='-Wl,-rpath,${LIB_RUNTIME_DIR}'
	    if test "$GCC" = "yes" ; then
		EXTRA_CFLAGS="-mabi=n32"
		LDFLAGS="-mabi=n32"
	    else
		case $system in
		    IRIX-6.3)
			# Use to build 6.2 compatible binaries on 6.3.
			EXTRA_CFLAGS="-n32 -D_OLD_TERMIOS"
			;;
		    *)
			EXTRA_CFLAGS="-n32"
			;;
		esac
		LDFLAGS="-n32"
	    fi
	    ;;
	IRIX64-6.*)
	    SHLIB_CFLAGS=""
	    SHLIB_LD="ld -n32 -shared -rdata_shared"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS=""
	    LDFLAGS=""
	    LD_SEARCH_FLAGS='-Wl,-rpath,${LIB_RUNTIME_DIR}'

	    # Check to enable 64-bit flags for compiler/linker

	    if test "$do64bit" = "yes" ; then
	        if test "$GCC" = "yes" ; then
	            AC_MSG_WARN([64bit mode not supported by gcc])
	        else
	            do64bit_ok=yes
	            SHLIB_LD="ld -64 -shared -rdata_shared"
	            EXTRA_CFLAGS="-64"
	            LDFLAGS="-64"
	        fi
	    fi
	    ;;
	Linux*)
	    SHLIB_CFLAGS="-fPIC"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"

	    # egcs-2.91.66 on Redhat Linux 6.0 generates lots of warnings 
	    # when you inline the string and math operations.  Turn this off to
	    # get rid of the warnings.

	    CFLAGS_OPTIMIZE="${CFLAGS_OPTIMIZE} -D__NO_STRING_INLINES -D__NO_MATH_INLINES"

	    if test "$have_dl" = yes; then
		SHLIB_LD="${CC} -shared"
		DL_OBJS="tclLoadDl.o"
		DL_LIBS="-ldl"
		LDFLAGS="-rdynamic"
		LD_SEARCH_FLAGS='-Wl,-rpath,${LIB_RUNTIME_DIR}'
	    else
		AC_CHECK_HEADER(dld.h, [
		    SHLIB_LD="ld -shared"
		    DL_OBJS="tclLoadDld.o"
		    DL_LIBS="-ldld"
		    LDFLAGS=""
		    LD_SEARCH_FLAGS=""])
	    fi
	    if test "`uname -m`" = "alpha" ; then
		EXTRA_CFLAGS="-mieee"
	    fi

	    ;;
	GNU*)
	    SHLIB_CFLAGS="-fPIC"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"

	    if test "$have_dl" = yes; then
		SHLIB_LD="${CC} -shared"
		DL_OBJS=""
		DL_LIBS="-ldl"
		LDFLAGS="-rdynamic"
		LD_SEARCH_FLAGS=""
	    else
		AC_CHECK_HEADER(dld.h, [
		    SHLIB_LD="ld -shared"
		    DL_OBJS=""
		    DL_LIBS="-ldld"
		    LDFLAGS=""
		    LD_SEARCH_FLAGS=""])
	    fi
	    if test "`uname -m`" = "alpha" ; then
		EXTRA_CFLAGS="-mieee"
	    fi
	    ;;
	MP-RAS-02*)
	    SHLIB_CFLAGS="-K PIC"
	    SHLIB_LD="cc -G"
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    LDFLAGS=""
	    LD_SEARCH_FLAGS=""
	    ;;
	MP-RAS-*)
	    SHLIB_CFLAGS="-K PIC"
	    SHLIB_LD="cc -G"
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    LDFLAGS="-Wl,-Bexport"
	    LD_SEARCH_FLAGS=""
	    ;;
	NetBSD-*|FreeBSD-[[1-2]].*|OpenBSD-*)
	    # Not available on all versions:  check for include file.
	    AC_CHECK_HEADER(dlfcn.h, [
		# NetBSD/SPARC needs -fPIC, -fpic will not do.
		SHLIB_CFLAGS="-fPIC"
		SHLIB_LD="ld -Bshareable -x"
		SHLIB_LD_LIBS=""
		SHLIB_SUFFIX=".so"
		DL_OBJS="tclLoadDl.o"
		DL_LIBS=""
		LDFLAGS=""
		LD_SEARCH_FLAGS='-Wl,-rpath,${LIB_RUNTIME_DIR}'
		AC_MSG_CHECKING([for ELF])
		AC_EGREP_CPP(yes, [
#ifdef __ELF__
	yes
#endif
		],
		    AC_MSG_RESULT([yes])
		    SHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}\$\{DBGX\}.so',
		    AC_MSG_RESULT([no])
		    SHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}\$\{DBGX\}.so.1.0'
		)
	    ], [
		SHLIB_CFLAGS=""
		SHLIB_LD="echo tclLdAout $CC \{$SHLIB_CFLAGS\} | `pwd`/tclsh -r"
		SHLIB_LD_LIBS='${LIBS}'
		SHLIB_SUFFIX=".a"
		DL_OBJS="tclLoadAout.o"
		DL_LIBS=""
		LDFLAGS=""
		LD_SEARCH_FLAGS='-L${LIB_RUNTIME_DIR}'
		SHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}\$\{DBGX\}.a'
	    ])

	    # FreeBSD doesn't handle version numbers with dots.

	    UNSHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}\$\{DBGX\}.a'
	    TCL_LIB_VERSIONS_OK=nodots
	    ;;
	FreeBSD-*)
	    # FreeBSD 3.* and greater have ELF.
	    SHLIB_CFLAGS="-fPIC"
	    SHLIB_LD="ld -Bshareable -x"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS=""
	    LDFLAGS="-export-dynamic"
	    LD_SEARCH_FLAGS='-Wl,-rpath,${LIB_RUNTIME_DIR}'
	    if test "${TCL_THREADS}" = "1" ; then
		# The -pthread needs to go in the CFLAGS, not LIBS
		LIBS=`echo $LIBS | sed s/-pthread//`
		EXTRA_CFLAGS="-pthread"
	    	LDFLAGS="$LDFLAGS -pthread"
	    fi
	    case $system in
	    FreeBSD-3.*)
	    	# FreeBSD-3 doesn't handle version numbers with dots.
	    	UNSHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}\$\{DBGX\}.a'
	    	SHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}\$\{DBGX\}.so'
	    	TCL_LIB_VERSIONS_OK=nodots
		;;
	    esac
	    ;;
	Rhapsody-*|Darwin-*)
	    SHLIB_CFLAGS="-fno-common"
	    SHLIB_LD="cc -dynamiclib \${LDFLAGS}"
	    TCL_SHLIB_LD_EXTRAS="-compatibility_version ${TCL_MAJOR_VERSION} -current_version \${VERSION} -install_name \${LIB_RUNTIME_DIR}/\${TCL_LIB_FILE} -prebind -seg1addr 0xa000000"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".dylib"
	    DL_OBJS="tclLoadDyld.o"
	    DL_LIBS=""
	    LDFLAGS="-prebind"
	    LD_SEARCH_FLAGS=""
	    CFLAGS_OPTIMIZE="-O3"
	    EXTRA_CFLAGS="-arch ppc -pipe"
	    ;;
	NEXTSTEP-*)
	    SHLIB_CFLAGS=""
	    SHLIB_LD="cc -nostdlib -r"
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadNext.o"
	    DL_LIBS=""
	    LDFLAGS=""
	    LD_SEARCH_FLAGS=""
	    ;;
	OS/390-*)
	    CFLAGS_OPTIMIZE=""      # Optimizer is buggy
	    AC_DEFINE(_OE_SOCKETS)  # needed in sys/socket.h
	    ;;      
	OSF1-1.0|OSF1-1.1|OSF1-1.2)
	    # OSF/1 1.[012] from OSF, and derivatives, including Paragon OSF/1
	    SHLIB_CFLAGS=""
	    # Hack: make package name same as library name
	    SHLIB_LD='ld -R -export $@:'
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadOSF.o"
	    DL_LIBS=""
	    LDFLAGS=""
	    LD_SEARCH_FLAGS=""
	    ;;
	OSF1-1.*)
	    # OSF/1 1.3 from OSF using ELF, and derivatives, including AD2
	    SHLIB_CFLAGS="-fPIC"
	    if test "$SHARED_BUILD" = "1" ; then
	        SHLIB_LD="ld -shared"
	    else
	        SHLIB_LD="ld -non_shared"
	    fi
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS=""
	    LDFLAGS=""
	    LD_SEARCH_FLAGS=""
	    ;;
	OSF1-V*)
	    # Digital OSF/1
	    SHLIB_CFLAGS=""
	    if test "$SHARED_BUILD" = "1" ; then
	        SHLIB_LD='ld -shared -expect_unresolved "*"'
	    else
	        SHLIB_LD='ld -non_shared -expect_unresolved "*"'
	    fi
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS=""
	    LDFLAGS=""
	    LD_SEARCH_FLAGS='-rpath ${LIB_RUNTIME_DIR}'
	    if test "$GCC" != "yes" ; then
		EXTRA_CFLAGS="-DHAVE_TZSET -std1"
	    fi
	    # see pthread_intro(3) for pthread support on osf1, k.furukawa
	    if test "${TCL_THREADS}" = "1" ; then
		EXTRA_CFLAGS="${EXTRA_CFLAGS} -DHAVE_PTHREAD_ATTR_SETSTACKSIZE"
		EXTRA_CFLAGS="${EXTRA_CFLAGS} -DTCL_THREAD_STACK_MIN=PTHREAD_STACK_MIN*64"
		LIBS=`echo $LIBS | sed s/-lpthreads//`
		if test "$GCC" = "yes" ; then
		    LIBS="$LIBS -lpthread -lmach -lexc"
		else
		    EXTRA_CFLAGS="${EXTRA_CFLAGS} -pthread"
		    LDFLAGS="-pthread"
		fi
	    fi

	    ;;
	QNX-6*)
	    # QNX RTP
	    # This may work for all QNX, but it was only reported for v6.
	    SHLIB_CFLAGS="-fPIC"
	    SHLIB_LD="ld -Bshareable -x"
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    # dlopen is in -lc on QNX
	    DL_LIBS=""
	    LDFLAGS=""
	    LD_SEARCH_FLAGS=""
	    ;;
	RISCos-*)
	    SHLIB_CFLAGS="-G 0"
	    SHLIB_LD="echo tclLdAout $CC \{$SHLIB_CFLAGS\} | `pwd`/tclsh -r -G 0"
	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".a"
	    DL_OBJS="tclLoadAout.o"
	    DL_LIBS=""
	    LDFLAGS="-Wl,-D,08000000"
	    LD_SEARCH_FLAGS='-L${LIB_RUNTIME_DIR}'
	    ;;
	SCO_SV-3.2*)
	    # Note, dlopen is available only on SCO 3.2.5 and greater. However,
	    # this test works, since "uname -s" was non-standard in 3.2.4 and
	    # below.
	    if test "$GCC" = "yes" ; then
	    	SHLIB_CFLAGS="-fPIC -melf"
	    	LDFLAGS="-melf -Wl,-Bexport"
	    else
	    	SHLIB_CFLAGS="-Kpic -belf"
	    	LDFLAGS="-belf -Wl,-Bexport"
	    fi
	    SHLIB_LD="ld -G"
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS=""
	    LD_SEARCH_FLAGS=""
	    ;;
	SINIX*5.4*)
	    SHLIB_CFLAGS="-K PIC"
	    SHLIB_LD="cc -G"
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    LDFLAGS=""
	    LD_SEARCH_FLAGS=""
	    ;;
	SunOS-4*)
	    SHLIB_CFLAGS="-PIC"
	    SHLIB_LD="ld"
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    LDFLAGS=""
	    LD_SEARCH_FLAGS='-L${LIB_RUNTIME_DIR}'

	    # SunOS can't handle version numbers with dots in them in library
	    # specs, like -ltcl7.5, so use -ltcl75 instead.  Also, it
	    # requires an extra version number at the end of .so file names.
	    # So, the library has to have a name like libtcl75.so.1.0

	    SHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}\$\{DBGX\}.so.1.0'
	    UNSHARED_LIB_SUFFIX='${TCL_TRIM_DOTS}\$\{DBGX\}.a'
	    TCL_LIB_VERSIONS_OK=nodots
	    ;;
	SunOS-5.[[0-6]]*)

	    # Note: If _REENTRANT isn't defined, then Solaris
	    # won't define thread-safe library routines.

	    AC_DEFINE(_REENTRANT)
	    AC_DEFINE(_POSIX_PTHREAD_SEMANTICS)

	    SHLIB_CFLAGS="-KPIC"

	    # Note: need the LIBS below, otherwise Tk won't find Tcl's
	    # symbols when dynamically loaded into tclsh.

	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    LDFLAGS=""
	    if test "$GCC" = "yes" ; then
		SHLIB_LD="$CC -shared"
		LD_SEARCH_FLAGS='-Wl,-R,${LIB_RUNTIME_DIR}'
	    else
		SHLIB_LD="/usr/ccs/bin/ld -G -z text"
		LD_SEARCH_FLAGS='-R ${LIB_RUNTIME_DIR}'
	    fi
	    ;;
	SunOS-5*)

	    # Note: If _REENTRANT isn't defined, then Solaris
	    # won't define thread-safe library routines.

	    AC_DEFINE(_REENTRANT)
	    AC_DEFINE(_POSIX_PTHREAD_SEMANTICS)

	    SHLIB_CFLAGS="-KPIC"
	    LDFLAGS=""
    
	    # Check to enable 64-bit flags for compiler/linker
	    if test "$do64bit" = "yes" ; then
		arch=`isainfo`
		if test "$arch" = "sparcv9 sparc" ; then
			if test "$GCC" = "yes" ; then
			    AC_MSG_WARN("64bit mode not supported with GCC on $system")
			else
			    do64bit_ok=yes
			    if test "$do64bitVIS" = "yes" ; then
				EXTRA_CFLAGS="-xarch=v9a"
			    	LDFLAGS="-xarch=v9a"
			    else
				EXTRA_CFLAGS="-xarch=v9"
			    	LDFLAGS="-xarch=v9"
			    fi
			fi
		else
		    AC_MSG_WARN("64bit mode only supported sparcv9 system")
		fi
	    fi
	    
	    # Note: need the LIBS below, otherwise Tk won't find Tcl's
	    # symbols when dynamically loaded into tclsh.

	    SHLIB_LD_LIBS='${LIBS}'
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    if test "$GCC" = "yes" ; then
		SHLIB_LD="$CC -shared"
		LD_SEARCH_FLAGS='-Wl,-R,${LIB_RUNTIME_DIR}'
	    else
		SHLIB_LD="/usr/ccs/bin/ld -G -z text"
		LD_SEARCH_FLAGS='-R ${LIB_RUNTIME_DIR}'
	    fi
	    ;;
	ULTRIX-4.*)
	    SHLIB_CFLAGS="-G 0"
	    SHLIB_SUFFIX=".a"
	    SHLIB_LD="echo tclLdAout $CC \{$SHLIB_CFLAGS\} | `pwd`/tclsh -r -G 0"
	    SHLIB_LD_LIBS='${LIBS}'
	    DL_OBJS="tclLoadAout.o"
	    DL_LIBS=""
	    LDFLAGS="-Wl,-D,08000000"
	    LD_SEARCH_FLAGS='-L${LIB_RUNTIME_DIR}'
	    if test "$GCC" != "yes" ; then
		EXTRA_CFLAGS="-DHAVE_TZSET -std1"
	    fi
	    ;;
	UNIX_SV* | UnixWare-5*)
	    SHLIB_CFLAGS="-KPIC"
	    SHLIB_LD="cc -G"
	    SHLIB_LD_LIBS=""
	    SHLIB_SUFFIX=".so"
	    DL_OBJS="tclLoadDl.o"
	    DL_LIBS="-ldl"
	    # Some UNIX_SV* systems (unixware 1.1.2 for example) have linkers
	    # that don't grok the -Bexport option.  Test that it does.
	    hold_ldflags=$LDFLAGS
	    AC_MSG_CHECKING([for ld accepts -Bexport flag])
	    LDFLAGS="${LDFLAGS} -Wl,-Bexport"
	    AC_TRY_LINK(, [int i;], found=yes, found=no)
	    LDFLAGS=$hold_ldflags
	    AC_MSG_RESULT([$found])
	    if test $found = yes; then
	    LDFLAGS="-Wl,-Bexport"
	    else
	    LDFLAGS=""
	    fi
	    LD_SEARCH_FLAGS=""
	    ;;
    esac

    if test "$do64bit" = "yes" -a "$do64bit_ok" = "no" ; then
    AC_MSG_WARN("64bit support being disabled -- don\'t know magic for this platform")
    fi

    # Step 4: If pseudo-static linking is in use (see K. B. Kenny, "Dynamic
    # Loading for Tcl -- What Became of It?".  Proc. 2nd Tcl/Tk Workshop,
    # New Orleans, LA, Computerized Processes Unlimited, 1994), then we need
    # to determine which of several header files defines the a.out file
    # format (a.out.h, sys/exec.h, or sys/exec_aout.h).  At present, we
    # support only a file format that is more or less version-7-compatible. 
    # In particular,
    #	- a.out files must begin with `struct exec'.
    #	- the N_TXTOFF on the `struct exec' must compute the seek address
    #	  of the text segment
    #	- The `struct exec' must contain a_magic, a_text, a_data, a_bss
    #	  and a_entry fields.
    # The following compilation should succeed if and only if either sys/exec.h
    # or a.out.h is usable for the purpose.
    #
    # Note that the modified COFF format used on MIPS Ultrix 4.x is usable; the
    # `struct exec' includes a second header that contains information that
    # duplicates the v7 fields that are needed.

    if test "x$DL_OBJS" = "xtclLoadAout.o" ; then
	AC_MSG_CHECKING([sys/exec.h])
	AC_TRY_COMPILE([#include <sys/exec.h>],[
	    struct exec foo;
	    unsigned long seek;
	    int flag;
#if defined(__mips) || defined(mips)
	    seek = N_TXTOFF (foo.ex_f, foo.ex_o);
#else
	    seek = N_TXTOFF (foo);
#endif
	    flag = (foo.a_magic == OMAGIC);
	    return foo.a_text + foo.a_data + foo.a_bss + foo.a_entry;
    ], tcl_ok=usable, tcl_ok=unusable)
	AC_MSG_RESULT([$tcl_ok])
	if test $tcl_ok = usable; then
	    AC_DEFINE(USE_SYS_EXEC_H)
	else
	    AC_MSG_CHECKING([a.out.h])
	    AC_TRY_COMPILE([#include <a.out.h>],[
		struct exec foo;
		unsigned long seek;
		int flag;
#if defined(__mips) || defined(mips)
		seek = N_TXTOFF (foo.ex_f, foo.ex_o);
#else
		seek = N_TXTOFF (foo);
#endif
		flag = (foo.a_magic == OMAGIC);
		return foo.a_text + foo.a_data + foo.a_bss + foo.a_entry;
	    ], tcl_ok=usable, tcl_ok=unusable)
	    AC_MSG_RESULT([$tcl_ok])
	    if test $tcl_ok = usable; then
		AC_DEFINE(USE_A_OUT_H)
	    else
		AC_MSG_CHECKING([sys/exec_aout.h])
		AC_TRY_COMPILE([#include <sys/exec_aout.h>],[
		    struct exec foo;
		    unsigned long seek;
		    int flag;
#if defined(__mips) || defined(mips)
		    seek = N_TXTOFF (foo.ex_f, foo.ex_o);
#else
		    seek = N_TXTOFF (foo);
#endif
		    flag = (foo.a_midmag == OMAGIC);
		    return foo.a_text + foo.a_data + foo.a_bss + foo.a_entry;
		], tcl_ok=usable, tcl_ok=unusable)
		AC_MSG_RESULT([$tcl_ok])
		if test $tcl_ok = usable; then
		    AC_DEFINE(USE_SYS_EXEC_AOUT_H)
		else
		    DL_OBJS=""
		fi
	    fi
	fi
    fi

    # Step 5: disable dynamic loading if requested via a command-line switch.

    AC_ARG_ENABLE(load, [  --disable-load          disallow dynamic loading and "load" command],
	[tcl_ok=$enableval], [tcl_ok=yes])
    if test "$tcl_ok" = "no"; then
	DL_OBJS=""
    fi

    if test "x$DL_OBJS" != "x" ; then
	BUILD_DLTEST="\$(DLTEST_TARGETS)"
    else
	echo "Can't figure out how to do dynamic loading or shared libraries"
	echo "on this system."
	SHLIB_CFLAGS=""
	SHLIB_LD=""
	SHLIB_SUFFIX=""
	DL_OBJS="tclLoadNone.o"
	DL_LIBS=""
	LDFLAGS=""
	LD_SEARCH_FLAGS=""
	BUILD_DLTEST=""
    fi

    # If we're running gcc, then change the C flags for compiling shared
    # libraries to the right flags for gcc, instead of those for the
    # standard manufacturer compiler.

    if test "$DL_OBJS" != "tclLoadNone.o" ; then
	if test "$GCC" = "yes" ; then
	    case $system in
		AIX-*)
		    ;;
		BSD/OS*)
		    ;;
		IRIX*)
		    ;;
		NetBSD-*|FreeBSD-*|OpenBSD-*)
		    ;;
		Rhapsody-*|Darwin-*)
		    ;;
		RISCos-*)
		    ;;
		SCO_SV-3.2*)
		    ;;
		ULTRIX-4.*)
		    ;;
		windows)
		    ;;
		*)
		    SHLIB_CFLAGS="-fPIC"
		    ;;
	    esac
	fi
    fi

    if test "$SHARED_LIB_SUFFIX" = "" ; then
	SHARED_LIB_SUFFIX='${VERSION}\$\{DBGX\}${SHLIB_SUFFIX}'
    fi
    if test "$UNSHARED_LIB_SUFFIX" = "" ; then
	UNSHARED_LIB_SUFFIX='${VERSION}\$\{DBGX\}.a'
    fi

    AC_SUBST(DL_LIBS)
    AC_SUBST(CFLAGS_DEBUG)
    AC_SUBST(CFLAGS_OPTIMIZE)
    AC_SUBST(CFLAGS_WARNING)
    AC_SUBST(EXTRA_CFLAGS)

    SHLIB_LDFLAGS='$(LDFLAGS_DEFAULT)'
    AC_SUBST(STLIB_LD)
    AC_SUBST(SHLIB_LD)
    AC_SUBST(SHLIB_CFLAGS)
    AC_SUBST(SHLIB_LDFLAGS)
    AC_SUBST(SHLIB_LD_LIBS)
    AC_SUBST(LDFLAGS_DEBUG)
    AC_SUBST(LDFLAGS_OPTIMIZE)
])

#--------------------------------------------------------------------
# TEA_SERIAL_PORT
#
#	Determine which interface to use to talk to the serial port.
#	Note that #include lines must begin in leftmost column for
#	some compilers to recognize them as preprocessor directives,
#	and some build environments have stdin not pointing at a
#	pseudo-terminal (usually /dev/null instead.)
#
# Arguments:
#	none
#	
# Results:
#
#	Defines only one of the following vars:
#		HAVE_SYS_MODEM_H
#		USE_TERMIOS
#		USE_TERMIO
#		USE_SGTTY
#
#--------------------------------------------------------------------

AC_DEFUN(TEA_SERIAL_PORT, [
    AC_CHECK_HEADERS(sys/modem.h)
    AC_MSG_CHECKING([termios vs. termio vs. sgtty])
    AC_CACHE_VAL(tcl_cv_api_serial, [
    AC_TRY_RUN([
#include <termios.h>

int main() {
    struct termios t;
    if (tcgetattr(0, &t) == 0) {
	cfsetospeed(&t, 0);
	t.c_cflag |= PARENB | PARODD | CSIZE | CSTOPB;
	return 0;
    }
    return 1;
}], tcl_cv_api_serial=termios, tcl_cv_api_serial=no, tcl_cv_api_serial=no)
    if test $tcl_cv_api_serial = no ; then
	AC_TRY_RUN([
#include <termio.h>

int main() {
    struct termio t;
    if (ioctl(0, TCGETA, &t) == 0) {
	t.c_cflag |= CBAUD | PARENB | PARODD | CSIZE | CSTOPB;
	return 0;
    }
    return 1;
}], tcl_cv_api_serial=termio, tcl_cv_api_serial=no, tcl_cv_api_serial=no)
    fi
    if test $tcl_cv_api_serial = no ; then
	AC_TRY_RUN([
#include <sgtty.h>

int main() {
    struct sgttyb t;
    if (ioctl(0, TIOCGETP, &t) == 0) {
	t.sg_ospeed = 0;
	t.sg_flags |= ODDP | EVENP | RAW;
	return 0;
    }
    return 1;
}], tcl_cv_api_serial=sgtty, tcl_cv_api_serial=none, tcl_cv_api_serial=none)
    fi
    if test $tcl_cv_api_serial = no ; then
	AC_TRY_RUN([
#include <termios.h>
#include <errno.h>

int main() {
    struct termios t;
    if (tcgetattr(0, &t) == 0
	|| errno == ENOTTY || errno == ENXIO || errno == EINVAL) {
	cfsetospeed(&t, 0);
	t.c_cflag |= PARENB | PARODD | CSIZE | CSTOPB;
	return 0;
    }
    return 1;
}], tcl_cv_api_serial=termios, tcl_cv_api_serial=no, tcl_cv_api_serial=no)
    fi
    if test $tcl_cv_api_serial = no; then
	AC_TRY_RUN([
#include <termio.h>
#include <errno.h>

int main() {
    struct termio t;
    if (ioctl(0, TCGETA, &t) == 0
	|| errno == ENOTTY || errno == ENXIO || errno == EINVAL) {
	t.c_cflag |= CBAUD | PARENB | PARODD | CSIZE | CSTOPB;
	return 0;
    }
    return 1;
    }], tcl_cv_api_serial=termio, tcl_cv_api_serial=no, tcl_cv_api_serial=no)
    fi
    if test $tcl_cv_api_serial = no; then
	AC_TRY_RUN([
#include <sgtty.h>
#include <errno.h>

int main() {
    struct sgttyb t;
    if (ioctl(0, TIOCGETP, &t) == 0
	|| errno == ENOTTY || errno == ENXIO || errno == EINVAL) {
	t.sg_ospeed = 0;
	t.sg_flags |= ODDP | EVENP | RAW;
	return 0;
    }
    return 1;
}], tcl_cv_api_serial=sgtty, tcl_cv_api_serial=none, tcl_cv_api_serial=none)
    fi])
    case $tcl_cv_api_serial in
	termios) AC_DEFINE(USE_TERMIOS);;
	termio)  AC_DEFINE(USE_TERMIO);;
	sgtty)   AC_DEFINE(USE_SGTTY);;
    esac
    AC_MSG_RESULT([$tcl_cv_api_serial])
])

#--------------------------------------------------------------------
# TEA_MISSING_POSIX_HEADERS
#
#	Supply substitutes for missing POSIX header files.  Special
#	notes:
#	    - stdlib.h doesn't define strtol, strtoul, or
#	      strtod insome versions of SunOS
#	    - some versions of string.h don't declare procedures such
#	      as strstr
#
# Arguments:
#	none
#	
# Results:
#
#	Defines some of the following vars:
#		NO_DIRENT_H
#		NO_ERRNO_H
#		NO_VALUES_H
#		NO_LIMITS_H
#		NO_STDLIB_H
#		NO_STRING_H
#		NO_SYS_WAIT_H
#		NO_DLFCN_H
#		HAVE_UNISTD_H
#		HAVE_SYS_PARAM_H
#
#		HAVE_STRING_H ?
#
#--------------------------------------------------------------------

AC_DEFUN(TEA_MISSING_POSIX_HEADERS, [
    AC_MSG_CHECKING([dirent.h])
    AC_TRY_LINK([#include <sys/types.h>
#include <dirent.h>], [
#ifndef _POSIX_SOURCE
#   ifdef __Lynx__
	/*
	 * Generate compilation error to make the test fail:  Lynx headers
	 * are only valid if really in the POSIX environment.
	 */

	missing_procedure();
#   endif
#endif
DIR *d;
struct dirent *entryPtr;
char *p;
d = opendir("foobar");
entryPtr = readdir(d);
p = entryPtr->d_name;
closedir(d);
], tcl_ok=yes, tcl_ok=no)

    if test $tcl_ok = no; then
	AC_DEFINE(NO_DIRENT_H)
    fi

    AC_MSG_RESULT([$tcl_ok])
    AC_CHECK_HEADER(errno.h, , [AC_DEFINE(NO_ERRNO_H)])
    AC_CHECK_HEADER(float.h, , [AC_DEFINE(NO_FLOAT_H)])
    AC_CHECK_HEADER(values.h, , [AC_DEFINE(NO_VALUES_H)])
    AC_CHECK_HEADER(limits.h, , [AC_DEFINE(NO_LIMITS_H)])
    AC_CHECK_HEADER(stdlib.h, tcl_ok=1, tcl_ok=0)
    AC_EGREP_HEADER(strtol, stdlib.h, , tcl_ok=0)
    AC_EGREP_HEADER(strtoul, stdlib.h, , tcl_ok=0)
    AC_EGREP_HEADER(strtod, stdlib.h, , tcl_ok=0)
    if test $tcl_ok = 0; then
	AC_DEFINE(NO_STDLIB_H)
    fi
    AC_CHECK_HEADER(string.h, tcl_ok=1, tcl_ok=0)
    AC_EGREP_HEADER(strstr, string.h, , tcl_ok=0)
    AC_EGREP_HEADER(strerror, string.h, , tcl_ok=0)

    # See also memmove check below for a place where NO_STRING_H can be
    # set and why.

    if test $tcl_ok = 0; then
	AC_DEFINE(NO_STRING_H)
    fi

    AC_CHECK_HEADER(sys/wait.h, , [AC_DEFINE(NO_SYS_WAIT_H)])
    AC_CHECK_HEADER(dlfcn.h, , [AC_DEFINE(NO_DLFCN_H)])

    # OS/390 lacks sys/param.h (and doesn't need it, by chance).

    AC_HAVE_HEADERS(unistd.h sys/param.h)

])

#--------------------------------------------------------------------
# TEA_PATH_X
#
#	Locate the X11 header files and the X11 library archive.  Try
#	the ac_path_x macro first, but if it doesn't find the X stuff
#	(e.g. because there's no xmkmf program) then check through
#	a list of possible directories.  Under some conditions the
#	autoconf macro will return an include directory that contains
#	no include files, so double-check its result just to be safe.
#
#	This should be called after TEA_CONFIG_CFLAGS as setting the
#	LIBS line can confuse some configure macro magic.
#
# Arguments:
#	none
#	
# Results:
#
#	Sets the following vars:
#		XINCLUDES
#		XLIBSW
#		LIBS (appends to)
#
#--------------------------------------------------------------------

AC_DEFUN(TEA_PATH_X, [
    if test "${TEA_PLATFORM}" = "unix" ; then
	TEA_PATH_UNIX_X
    fi
])

AC_DEFUN(TEA_PATH_UNIX_X, [
    AC_PATH_X
    not_really_there=""
    if test "$no_x" = ""; then
	if test "$x_includes" = ""; then
	    AC_TRY_CPP([#include <X11/XIntrinsic.h>], , not_really_there="yes")
	else
	    if test ! -r $x_includes/X11/Intrinsic.h; then
		not_really_there="yes"
	    fi
	fi
    fi
    if test "$no_x" = "yes" -o "$not_really_there" = "yes"; then
	AC_MSG_CHECKING([for X11 header files])
	XINCLUDES="# no special path needed"
	AC_TRY_CPP([#include <X11/Intrinsic.h>], , XINCLUDES="nope")
	if test "$XINCLUDES" = nope; then
	    dirs="/usr/unsupported/include /usr/local/include /usr/X386/include /usr/X11R6/include /usr/X11R5/include /usr/include/X11R5 /usr/include/X11R4 /usr/openwin/include /usr/X11/include /usr/sww/include"
	    for i in $dirs ; do
		if test -r $i/X11/Intrinsic.h; then
		    AC_MSG_RESULT([$i])
		    XINCLUDES=" -I$i"
		    break
		fi
	    done
	fi
    else
	if test "$x_includes" != ""; then
	    XINCLUDES=-I$x_includes
	else
	    XINCLUDES="# no special path needed"
	fi
    fi
    if test "$XINCLUDES" = nope; then
	AC_MSG_RESULT([could not find any!])
	XINCLUDES="# no include files found"
    fi

    if test "$no_x" = yes; then
	AC_MSG_CHECKING([for X11 libraries])
	XLIBSW=nope
	dirs="/usr/unsupported/lib /usr/local/lib /usr/X386/lib /usr/X11R6/lib /usr/X11R5/lib /usr/lib/X11R5 /usr/lib/X11R4 /usr/openwin/lib /usr/X11/lib /usr/sww/X11/lib"
	for i in $dirs ; do
	    if test -r $i/libX11.a -o -r $i/libX11.so -o -r $i/libX11.sl; then
		AC_MSG_RESULT([$i])
		XLIBSW="-L$i -lX11"
		x_libraries="$i"
		break
	    fi
	done
    else
	if test "$x_libraries" = ""; then
	    XLIBSW=-lX11
	else
	    XLIBSW="-L$x_libraries -lX11"
	fi
    fi
    if test "$XLIBSW" = nope ; then
	AC_CHECK_LIB(Xwindow, XCreateWindow, XLIBSW=-lXwindow)
    fi
    if test "$XLIBSW" = nope ; then
	AC_MSG_RESULT([could not find any!  Using -lX11.])
	XLIBSW=-lX11
    fi
    if test x"${XLIBSW}" != x ; then
	LIBS="${LIBS} ${XLIBSW}"
    fi
])

#--------------------------------------------------------------------
# TEA_BLOCKING_STYLE
#
#	The statements below check for systems where POSIX-style
#	non-blocking I/O (O_NONBLOCK) doesn't work or is unimplemented. 
#	On these systems (mostly older ones), use the old BSD-style
#	FIONBIO approach instead.
#
# Arguments:
#	none
#	
# Results:
#
#	Defines some of the following vars:
#		HAVE_SYS_IOCTL_H
#		HAVE_SYS_FILIO_H
#		USE_FIONBIO
#		O_NONBLOCK
#
#--------------------------------------------------------------------

AC_DEFUN(TEA_BLOCKING_STYLE, [
    AC_CHECK_HEADERS(sys/ioctl.h)
    AC_CHECK_HEADERS(sys/filio.h)
    AC_MSG_CHECKING([FIONBIO vs. O_NONBLOCK for nonblocking I/O])
    if test -f /usr/lib/NextStep/software_version; then
	system=NEXTSTEP-`awk '/3/,/3/' /usr/lib/NextStep/software_version`
    else
	system=`uname -s`-`uname -r`
	if test "$?" -ne 0 ; then
	    system=unknown
	else
	    # Special check for weird MP-RAS system (uname returns weird
	    # results, and the version is kept in special file).
	
	    if test -r /etc/.relid -a "X`uname -n`" = "X`uname -s`" ; then
		system=MP-RAS-`awk '{print $3}' /etc/.relid'`
	    fi
	    if test "`uname -s`" = "AIX" ; then
		system=AIX-`uname -v`.`uname -r`
	    fi
	fi
    fi
    case $system in
	# There used to be code here to use FIONBIO under AIX.  However, it
	# was reported that FIONBIO doesn't work under AIX 3.2.5.  Since
	# using O_NONBLOCK seems fine under AIX 4.*, I removed the FIONBIO
	# code (JO, 5/31/97).

	OSF*)
	    AC_DEFINE(USE_FIONBIO)
	    AC_MSG_RESULT([FIONBIO])
	    ;;
	SunOS-4*)
	    AC_DEFINE(USE_FIONBIO)
	    AC_MSG_RESULT([FIONBIO])
	    ;;
	ULTRIX-4.*)
	    AC_DEFINE(USE_FIONBIO)
	    AC_MSG_RESULT([FIONBIO])
	    ;;
	*)
	    AC_MSG_RESULT([O_NONBLOCK])
	    ;;
    esac
])

#--------------------------------------------------------------------
# TEA_TIME_HANLDER
#
#	Checks how the system deals with time.h, what time structures
#	are used on the system, and what fields the structures have.
#
# Arguments:
#	none
#	
# Results:
#
#	Defines some of the following vars:
#		USE_DELTA_FOR_TZ
#		HAVE_TM_GMTOFF
#		HAVE_TM_TZADJ
#		HAVE_TIMEZONE_VAR
#
#--------------------------------------------------------------------

AC_DEFUN(TEA_TIME_HANDLER, [
    AC_CHECK_HEADERS(sys/time.h)
    AC_HEADER_TIME
    AC_STRUCT_TIMEZONE

    AC_CHECK_FUNCS(gmtime_r localtime_r)

    AC_MSG_CHECKING([tm_tzadj in struct tm])
    AC_CACHE_VAL(tcl_cv_member_tm_tzadj,
	AC_TRY_COMPILE([#include <time.h>], [struct tm tm; tm.tm_tzadj;],
	    tcl_cv_member_tm_tzadj=yes, tcl_cv_member_tm_tzadj=no))
    AC_MSG_RESULT([$tcl_cv_member_tm_tzadj])
    if test $tcl_cv_member_tm_tzadj = yes ; then
	AC_DEFINE(HAVE_TM_TZADJ)
    fi

    AC_MSG_CHECKING([tm_gmtoff in struct tm])
    AC_CACHE_VAL(tcl_cv_member_tm_gmtoff,
	AC_TRY_COMPILE([#include <time.h>], [struct tm tm; tm.tm_gmtoff;],
	    tcl_cv_member_tm_gmtoff=yes, tcl_cv_member_tm_gmtoff=no))
    AC_MSG_RESULT([$tcl_cv_member_tm_gmtoff])
    if test $tcl_cv_member_tm_gmtoff = yes ; then
	AC_DEFINE(HAVE_TM_GMTOFF)
    fi

    #
    # Its important to include time.h in this check, as some systems
    # (like convex) have timezone functions, etc.
    #
    AC_MSG_CHECKING([long timezone variable])
    AC_CACHE_VAL(tcl_cv_var_timezone,
	AC_TRY_COMPILE([#include <time.h>],
	    [extern long timezone;
	    timezone += 1;
	    exit (0);],
	    tcl_cv_timezone_long=yes, tcl_cv_timezone_long=no))
    AC_MSG_RESULT([$tcl_cv_timezone_long])
    if test $tcl_cv_timezone_long = yes ; then
	AC_DEFINE(HAVE_TIMEZONE_VAR)
    else
	#
	# On some systems (eg IRIX 6.2), timezone is a time_t and not a long.
	#
	AC_MSG_CHECKING([time_t timezone variable])
	AC_CACHE_VAL(tcl_cv_timezone_time,
	    AC_TRY_COMPILE([#include <time.h>],
		[extern time_t timezone;
		timezone += 1;
		exit (0);],
		tcl_cv_timezone_time=yes, tcl_cv_timezone_time=no))
	AC_MSG_RESULT([$tcl_cv_timezone_time])
	if test $tcl_cv_timezone_time = yes ; then
	    AC_DEFINE(HAVE_TIMEZONE_VAR)
	fi
    fi
])

#--------------------------------------------------------------------
# TEA_BUGGY_STRTOD
#
#	Under Solaris 2.4, strtod returns the wrong value for the
#	terminating character under some conditions.  Check for this
#	and if the problem exists use a substitute procedure
#	"fixstrtod" (provided by Tcl) that corrects the error.
#	Also, on Compaq's Tru64 Unix 5.0,
#	strtod(" ") returns 0.0 instead of a failure to convert.
#
# Arguments:
#	none
#	
# Results:
#
#	Might defines some of the following vars:
#		strtod (=fixstrtod)
#
#--------------------------------------------------------------------

AC_DEFUN(TEA_BUGGY_STRTOD, [
    AC_CHECK_FUNC(strtod, tcl_strtod=1, tcl_strtod=0)
    if test "$tcl_strtod" = 1; then
	AC_MSG_CHECKING([for Solaris2.4/Tru64 strtod bugs])
	AC_TRY_RUN([
	    extern double strtod();
	    int main()
	    {
		char *string = "NaN", *spaceString = " ";
		char *term;
		double value;
		value = strtod(string, &term);
		if ((term != string) && (term[-1] == 0)) {
		    exit(1);
		}
		value = strtod(spaceString, &term);
		if (term == (spaceString+1)) {
		    exit(1);
		}
		exit(0);
	    }], tcl_ok=1, tcl_ok=0, tcl_ok=0)
	if test "$tcl_ok" = 1; then
	    AC_MSG_RESULT([ok])
	else
	    AC_MSG_RESULT([buggy])
	    AC_LIBOBJ([fixstrtod])
	    AC_DEFINE(strtod, fixstrtod)
	fi
    fi
])

#--------------------------------------------------------------------
# TEA_TCL_LINK_LIBS
#
#	Search for the libraries needed to link the Tcl shell.
#	Things like the math library (-lm) and socket stuff (-lsocket vs.
#	-lnsl) are dealt with here.
#
# Arguments:
#	Requires the following vars to be set in the Makefile:
#		DL_LIBS
#		LIBS
#		MATH_LIBS
#	
# Results:
#
#	Subst's the following var:
#		TCL_LIBS
#		MATH_LIBS
#
#	Might append to the following vars:
#		LIBS
#
#	Might define the following vars:
#		HAVE_NET_ERRNO_H
#
#--------------------------------------------------------------------

AC_DEFUN(TEA_TCL_LINK_LIBS, [
    #--------------------------------------------------------------------
    # On a few very rare systems, all of the libm.a stuff is
    # already in libc.a.  Set compiler flags accordingly.
    # Also, Linux requires the "ieee" library for math to work
    # right (and it must appear before "-lm").
    #--------------------------------------------------------------------

    AC_CHECK_FUNC(sin, MATH_LIBS="", MATH_LIBS="-lm")
    AC_CHECK_LIB(ieee, main, [MATH_LIBS="-lieee $MATH_LIBS"])

    #--------------------------------------------------------------------
    # Interactive UNIX requires -linet instead of -lsocket, plus it
    # needs net/errno.h to define the socket-related error codes.
    #--------------------------------------------------------------------

    AC_CHECK_LIB(inet, main, [LIBS="$LIBS -linet"])
    AC_CHECK_HEADER(net/errno.h, AC_DEFINE(HAVE_NET_ERRNO_H))

    #--------------------------------------------------------------------
    #	Check for the existence of the -lsocket and -lnsl libraries.
    #	The order here is important, so that they end up in the right
    #	order in the command line generated by make.  Here are some
    #	special considerations:
    #	1. Use "connect" and "accept" to check for -lsocket, and
    #	   "gethostbyname" to check for -lnsl.
    #	2. Use each function name only once:  can't redo a check because
    #	   autoconf caches the results of the last check and won't redo it.
    #	3. Use -lnsl and -lsocket only if they supply procedures that
    #	   aren't already present in the normal libraries.  This is because
    #	   IRIX 5.2 has libraries, but they aren't needed and they're
    #	   bogus:  they goof up name resolution if used.
    #	4. On some SVR4 systems, can't use -lsocket without -lnsl too.
    #	   To get around this problem, check for both libraries together
    #	   if -lsocket doesn't work by itself.
    #--------------------------------------------------------------------

    tcl_checkBoth=0
    AC_CHECK_FUNC(connect, tcl_checkSocket=0, tcl_checkSocket=1)
    if test "$tcl_checkSocket" = 1; then
	AC_CHECK_FUNC(setsockopt, , [AC_CHECK_LIB(socket, setsockopt,
	    LIBS="$LIBS -lsocket", tcl_checkBoth=1)])
    fi
    if test "$tcl_checkBoth" = 1; then
	tk_oldLibs=$LIBS
	LIBS="$LIBS -lsocket -lnsl"
	AC_CHECK_FUNC(accept, tcl_checkNsl=0, [LIBS=$tk_oldLibs])
    fi
    AC_CHECK_FUNC(gethostbyname, , [AC_CHECK_LIB(nsl, gethostbyname,
	    [LIBS="$LIBS -lnsl"])])
    
    # Don't perform the eval of the libraries here because DL_LIBS
    # won't be set until we call TEA_CONFIG_CFLAGS

    TCL_LIBS='${DL_LIBS} ${LIBS} ${MATH_LIBS}'
    AC_SUBST(TCL_LIBS)
    AC_SUBST(MATH_LIBS)
])

#--------------------------------------------------------------------
# TEA_TCL_EARLY_FLAGS
#
#	Check for what flags are needed to be passed so the correct OS
#	features are available.
#
# Arguments:
#	None
#	
# Results:
#
#	Might define the following vars:
#		_ISOC99_SOURCE
#		_LARGEFILE64_SOURCE
#
#--------------------------------------------------------------------

AC_DEFUN(TEA_TCL_EARLY_FLAG,[
    AC_CACHE_VAL([tcl_cv_flag_]translit($1,[A-Z],[a-z]),
	AC_TRY_COMPILE([$2], $3, [tcl_cv_flag_]translit($1,[A-Z],[a-z])=no,
	    AC_TRY_COMPILE([[#define ]$1[ 1
]$2], $3,
		[tcl_cv_flag_]translit($1,[A-Z],[a-z])=yes,
		[tcl_cv_flag_]translit($1,[A-Z],[a-z])=no)))
    if test ["x${tcl_cv_flag_]translit($1,[A-Z],[a-z])[}" = "xyes"] ; then
	AC_DEFINE($1)
	tcl_flags="$tcl_flags $1"
    fi
])

AC_DEFUN(TEA_TCL_EARLY_FLAGS,[
    AC_MSG_CHECKING([for required early compiler flags])
    tcl_flags=""
    TEA_TCL_EARLY_FLAG(_ISOC99_SOURCE,[#include <stdlib.h>],
	[char *p = (char *)strtoll; char *q = (char *)strtoull;])
    TEA_TCL_EARLY_FLAG(_LARGEFILE64_SOURCE,[#include <sys/stat.h>],
	[struct stat64 buf; int i = stat64("/", &buf);])
    if test "x${tcl_flags}" = "x" ; then
	AC_MSG_RESULT([none])
    else
	AC_MSG_RESULT([${tcl_flags}])
    fi
])

#--------------------------------------------------------------------
# TEA_TCL_64BIT_FLAGS
#
#	Check for what is defined in the way of 64-bit features.
#
# Arguments:
#	None
#	
# Results:
#
#	Might define the following vars:
#		TCL_WIDE_INT_IS_LONG
#		TCL_WIDE_INT_TYPE
#		HAVE_STRUCT_DIRENT64
#		HAVE_STRUCT_STAT64
#		HAVE_TYPE_OFF64_T
#
#--------------------------------------------------------------------

AC_DEFUN(TEA_TCL_64BIT_FLAGS, [
    AC_MSG_CHECKING([for 64-bit integer type])
    AC_CACHE_VAL(tcl_cv_type_64bit,[
	AC_TRY_COMPILE(,[__int64 value = (__int64) 0;],
           tcl_cv_type_64bit=__int64,tcl_cv_type_64bit=none
           AC_TRY_RUN([#include <unistd.h>
		int main() {exit(!(sizeof(long long) > sizeof(long)));}
		], tcl_cv_type_64bit="long long"))])
    if test "${tcl_cv_type_64bit}" = none ; then
	AC_MSG_RESULT([using long])
    else
	AC_DEFINE_UNQUOTED(TCL_WIDE_INT_TYPE,${tcl_cv_type_64bit})
	AC_MSG_RESULT([${tcl_cv_type_64bit}])

	# Now check for auxiliary declarations
	AC_MSG_CHECKING([for struct dirent64])
	AC_CACHE_VAL(tcl_cv_struct_dirent64,[
	    AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/dirent.h>],[struct dirent64 p;],
		tcl_cv_struct_dirent64=yes,tcl_cv_struct_dirent64=no)])
	if test "x${tcl_cv_struct_dirent64}" = "xyes" ; then
	    AC_DEFINE(HAVE_STRUCT_DIRENT64)
	fi
	AC_MSG_RESULT([${tcl_cv_struct_dirent64}])

	AC_MSG_CHECKING([for struct stat64])
	AC_CACHE_VAL(tcl_cv_struct_stat64,[
	    AC_TRY_COMPILE([#include <sys/stat.h>],[struct stat64 p;
],
		tcl_cv_struct_stat64=yes,tcl_cv_struct_stat64=no)])
	if test "x${tcl_cv_struct_stat64}" = "xyes" ; then
	    AC_DEFINE(HAVE_STRUCT_STAT64)
	fi
	AC_MSG_RESULT([${tcl_cv_struct_stat64}])

	AC_MSG_CHECKING([for off64_t])
	AC_CACHE_VAL(tcl_cv_type_off64_t,[
	    AC_TRY_COMPILE([#include <sys/types.h>],[off64_t offset;
],
		tcl_cv_type_off64_t=yes,tcl_cv_type_off64_t=no)])
	if test "x${tcl_cv_type_off64_t}" = "xyes" ; then
	    AC_DEFINE(HAVE_TYPE_OFF64_T)
	fi
	AC_MSG_RESULT([${tcl_cv_type_off64_t}])
    fi
])

##
## Here ends the standard Tcl configuration bits and starts the
## TEA specific functions
##

#------------------------------------------------------------------------
# TEA_INIT --
#
#	Init various Tcl Extension Architecture (TEA) variables.
#	This should be the first called TEA_* macro.
#
# Arguments:
#	none
#
# Results:
#
#	Defines and substs the following vars:
#		CYGPATH
#		EXEEXT
#	Defines only:
#		TEA_INITED
#		TEA_PLATFORM (windows or unix)
#
# "cygpath" is used on windows to generate native path names for include
# files. These variables should only be used with the compiler and linker
# since they generate native path names.
#
# EXEEXT
#	Select the executable extension based on the host type.  This
#	is a lightweight replacement for AC_EXEEXT that doesn't require
#	a compiler.
#------------------------------------------------------------------------

AC_DEFUN(TEA_INIT, [
    AC_MSG_CHECKING([for correct TEA configuration])
    if test x"${PACKAGE}" = x ; then
	AC_MSG_ERROR([
The PACKAGE variable must be defined by your TEA configure.in])
    fi
    AC_MSG_RESULT([ok])
    TEA_INITED=ok
    AC_PROG_CC
    case "`uname -s`" in
	*win32*|*WIN32*|*CYGWIN_NT*|*CYGWIN_9*|*CYGWIN_ME*|*MINGW32_*)
	    AC_CHECK_PROG(CYGPATH, cygpath, cygpath -w, echo)
	    EXEEXT=".exe"
	    TEA_PLATFORM="windows"
	    ;;
	*)
	    CYGPATH=echo
	    EXEEXT=""
	    TEA_PLATFORM="unix"
	    ;;
    esac

    # am_init_automake performs a ac_prog_install check so need to handle
    # problematic /usr/brl/bin/install on irix
    if test "x`uname -s`" = "xIRIX64" ; then
	PATH="`echo $PATH | sed 's/\/brl\/bin/bin/g'`"
    fi

    # Check if exec_prefix is set. If not use fall back to prefix
    if test x$exec_prefix = xNONE ; then exec_prefix=$prefix ; fi

    AC_SUBST(EXEEXT)
    AC_SUBST(CYGPATH)
])

#------------------------------------------------------------------------
# TEA_PREFIX --
#
#	Handle the --prefix=... option by defaulting to what Tcl gave
#
# Arguments:
#	none
#
# Results:
#
#	If --prefix or --exec-prefix was not specified, $prefix and
#	$exec_prefix will be set to the values given to Tcl when it was
#	configured.
#------------------------------------------------------------------------
AC_DEFUN(TEA_PREFIX, [
    # Should be AC_MSG_NOTICE, but that requires autoconf 2.50
    if test "${prefix}" = "NONE"; then
	prefix_default=yes
	if test x"${TCL_PREFIX}" != x; then
	    AC_MSG_WARN([--prefix defaulting to TCL_PREFIX ${TCL_PREFIX}])
	    prefix=${TCL_PREFIX}
	else
	    prefix=/usr/local
	fi
    fi
    if test "${exec_prefix}" = "NONE" -a x"${prefix_default}" = x"yes" ; then
	if test x"${TCL_EXEC_PREFIX}" != x; then
	    AC_MSG_WARN([--exec-prefix defaulting to TCL_EXEC_PREFIX ${TCL_EXEC_PREFIX}])
	    exec_prefix=${TCL_EXEC_PREFIX}
	else
	    exec_prefix=$prefix
	fi
    fi
])

#------------------------------------------------------------------------
# TEA_PROG_CC --
#
#	Do compiler checks the way we want.  This is just a replacement
#	for AC_PROG_CC in TEA configure.in files to make them cleaner.
#
# Arguments:
#	none
#
# Results:
#
#	Sets up CC var and other standard bits we need to make executables.
#------------------------------------------------------------------------
AC_DEFUN(TEA_SETUP_COMPILER, [
    # If the user did not set CFLAGS, set it now to keep
    # the AC_PROG_CC macro from adding "-g -O2".
    if test "${CFLAGS+set}" != "set" ; then
	CFLAGS=""
    fi

    AC_PROG_CC

    #------------------------------------------------------------------------
    # If we're using GCC, see if the compiler understands -pipe. If so, use it.
    # It makes compiling go faster.  (This is only a performance feature.)
    #------------------------------------------------------------------------

    if test -z "$no_pipe" -a -n "$GCC"; then
	AC_MSG_CHECKING([if the compiler understands -pipe])
	OLDCC="$CC"
	CC="$CC -pipe"
	AC_TRY_COMPILE(,, AC_MSG_RESULT([yes]), CC="$OLDCC"
	    AC_MSG_RESULT([no]))
    fi

    AC_PROG_INSTALL

    #--------------------------------------------------------------------
    # Checks to see if the make program sets the $MAKE variable.
    #--------------------------------------------------------------------

    AC_PROG_MAKE_SET

    #--------------------------------------------------------------------
    # Find ranlib
    #--------------------------------------------------------------------

    AC_PROG_RANLIB

    #--------------------------------------------------------------------
    # Determines the correct binary file extension (.o, .obj, .exe etc.)
    #--------------------------------------------------------------------

    AC_OBJEXT
    AC_EXEEXT

    #--------------------------------------------------------------------
    # Common compiler flag setup
    #--------------------------------------------------------------------

    TEA_TCL_EARLY_FLAGS
    TEA_TCL_64BIT_FLAGS
    #TEA_C_BIGENDIAN
    if test "${TEA_PLATFORM}" = "unix" ; then
	TEA_MISSING_POSIX_HEADERS
	TEA_BUGGY_STRTOD
    fi
])

#------------------------------------------------------------------------
# TEA_MAKE_LIB --
#
#	Generate a line that can be used to build a shared/unshared library
#	in a platform independent manner.
#
# Arguments:
#	none
#
#	Requires:
#
# Results:
#
#	Defines the following vars:
#	CFLAGS -	Done late here to note disturb other AC macros
#       MAKE_LIB -      Command to execute to build the Tcl library;
#                       differs depending on whether or not Tcl is being
#                       compiled as a shared library.
#	MAKE_SHARED_LIB	Makefile rule for building a shared library
#	MAKE_STATIC_LIB	Makefile rule for building a static library
#	MAKE_STUB_LIB	Makefile rule for building a stub library
#------------------------------------------------------------------------

AC_DEFUN(TEA_MAKE_LIB, [

    if test "${TEA_PLATFORM}" = "windows" -a "$GCC" != "yes"; then
	MAKE_STATIC_LIB="\${STLIB_LD} -out:\[$]@ \$(${PACKAGE}_OBJECTS)"
	MAKE_SHARED_LIB="\${SHLIB_LD} \${SHLIB_LDFLAGS} \${SHLIB_LD_LIBS} \$(LDFLAGS) -out:\[$]@ \$(${PACKAGE}_OBJECTS)"
	MAKE_STUB_LIB="\${STLIB_LD} -out:\[$]@ \$(${PACKAGE}stub_OBJECTS)"
    else
	MAKE_STATIC_LIB="\${STLIB_LD} \[$]@ \$(${PACKAGE}_OBJECTS)"
	MAKE_SHARED_LIB="\${SHLIB_LD} -o \[$]@ \$(${PACKAGE}_OBJECTS) \${SHLIB_LDFLAGS} \${SHLIB_LD_LIBS}"
	if test "x$RANLIB" = "x" -o "x`uname -s`" = "xIRIX64" -o "x`uname -s`" = "xIRIX" ; then
		MAKE_STUB_LIB="\${STLIB_LD} \[$]@ \$(${PACKAGE}stub_OBJECTS)"
	else
		MAKE_STUB_LIB="\${STLIB_LD} \[$]@ \$(${PACKAGE}stub_OBJECTS) ; $RANLIB \[$]@"
	fi
    fi

    if test "${SHARED_BUILD}" = "1" ; then
	MAKE_LIB="${MAKE_SHARED_LIB} "
    else
	MAKE_LIB="${MAKE_STATIC_LIB} "
    fi

    #--------------------------------------------------------------------
    # Shared libraries and static libraries have different names.
    # Use the double eval to make sure the ${DBGX} in the suffix is
    # substituted.
    #--------------------------------------------------------------------

    if test "${TEA_PLATFORM}" = "windows" ; then
	if test "${SHARED_BUILD}" = "1" ; then
	    # We force the unresolved linking of symbols that are really in
	    # the private libraries of Tcl and Tk.
	    SHLIB_LD_LIBS="${SHLIB_LD_LIBS} \"`${CYGPATH} ${TCL_BIN_DIR}/${TCL_STUB_LIB_FILE}`\""
	    if test x"${TK_BIN_DIR}" != x ; then
		SHLIB_LD_LIBS="${SHLIB_LD_LIBS} \"`${CYGPATH} ${TK_BIN_DIR}/${TK_STUB_LIB_FILE}`\""
	    fi
	    eval eval "${PACKAGE}_LIB_FILE=${PACKAGE}${SHARED_LIB_SUFFIX}"
	    RANLIB=:
	else
	    eval eval "${PACKAGE}_LIB_FILE=${PACKAGE}${UNSHARED_LIB_SUFFIX}"
	fi
	# Some packages build there own stubs libraries
	eval eval "${PACKAGE}stub_LIB_FILE=${PACKAGE}stub${UNSHARED_LIB_SUFFIX}"
    else
	if test "${SHARED_BUILD}" = "1" ; then
	    SHLIB_LD_LIBS="${SHLIB_LD_LIBS} ${TCL_STUB_LIB_SPEC}"
	    if test x"${TK_BIN_DIR}" != x ; then
		SHLIB_LD_LIBS="${SHLIB_LD_LIBS} ${TK_STUB_LIB_SPEC}"
	    fi
	    eval eval "${PACKAGE}_LIB_FILE=lib${PACKAGE}${SHARED_LIB_SUFFIX}"
	    RANLIB=:
	else
	    eval eval "${PACKAGE}_LIB_FILE=lib${PACKAGE}${UNSHARED_LIB_SUFFIX}"
	fi
	# Some packages build there own stubs libraries
	eval eval "${PACKAGE}stub_LIB_FILE=lib${PACKAGE}stub${UNSHARED_LIB_SUFFIX}"
    fi

    # These are escaped so that only CFLAGS is picked up at configure time.
    # The other values will be substituted at make time.
    CFLAGS="${CFLAGS} \${CFLAGS_DEFAULT} \${CFLAGS_WARNING}"
    if test "${SHARED_BUILD}" = "1" ; then
	CFLAGS="${CFLAGS} \${SHLIB_CFLAGS}"
    fi

    AC_SUBST(MAKE_LIB)
    AC_SUBST(MAKE_SHARED_LIB)
    AC_SUBST(MAKE_STATIC_LIB)
    AC_SUBST(MAKE_STUB_LIB)
])

#------------------------------------------------------------------------
# TEA_LIB_SPEC --
#
#	Compute the name of an existing object library located in libdir
#	from the given base name and produce the appropriate linker flags.
#
# Arguments:
#	basename	The base name of the library without version
#			numbers, extensions, or "lib" prefixes.
#	extra_dir	Extra directory in which to search for the
#			library.  This location is used first, then
#			$prefix/$exec-prefix, then some defaults.
#
# Requires:
#	TEA_INIT and TEA_PREFIX must be called first.
#
# Results:
#
#	Defines the following vars:
#		${basename}_LIB_NAME	The computed library name.
#		${basename}_LIB_SPEC	The computed linker flags.
#------------------------------------------------------------------------

AC_DEFUN(TEA_LIB_SPEC, [
    AC_MSG_CHECKING([for $1 library])

    # Look in exec-prefix for the library (defined by TEA_PREFIX).

    tea_lib_name_dir="${exec_prefix}/lib"

    # Or in a user-specified location.

    if test x"$2" != x ; then
	tea_extra_lib_dir=$2
    else
	tea_extra_lib_dir=NONE
    fi

    for i in \
	    `ls -dr ${tea_extra_lib_dir}/$1[[0-9]]*.lib 2>/dev/null ` \
	    `ls -dr ${tea_extra_lib_dir}/lib$1[[0-9]]* 2>/dev/null ` \
	    `ls -dr ${tea_lib_name_dir}/$1[[0-9]]*.lib 2>/dev/null ` \
	    `ls -dr ${tea_lib_name_dir}/lib$1[[0-9]]* 2>/dev/null ` \
	    `ls -dr /usr/lib/$1[[0-9]]*.lib 2>/dev/null ` \
	    `ls -dr /usr/lib/lib$1[[0-9]]* 2>/dev/null ` \
	    `ls -dr /usr/local/lib/$1[[0-9]]*.lib 2>/dev/null ` \
	    `ls -dr /usr/local/lib/lib$1[[0-9]]* 2>/dev/null ` ; do
	if test -f "$i" ; then
	    tea_lib_name_dir=`dirname $i`
	    $1_LIB_NAME=`basename $i`
	    $1_LIB_PATH_NAME=$i
	    break
	fi
    done

    if test "${TEA_PLATFORM}" = "windows"; then
	$1_LIB_SPEC=\"`${CYGPATH} ${$1_LIB_PATH_NAME} 2>/dev/null`\"
    else
	# Strip off the leading "lib" and trailing ".a" or ".so"

	tea_lib_name_lib=`echo ${$1_LIB_NAME}|sed -e 's/^lib//' -e 's/\.[[^.]]*$//' -e 's/\.so.*//'`
	$1_LIB_SPEC="-L${tea_lib_name_dir} -l${tea_lib_name_lib}"
    fi

    if test "x${$1_LIB_NAME}" = x ; then
	AC_MSG_ERROR([not found])
    else
	AC_MSG_RESULT([${$1_LIB_SPEC}])
    fi
])

#------------------------------------------------------------------------
# TEA_PRIVATE_TCL_HEADERS --
#
#	Locate the private Tcl include files
#
# Arguments:
#
#	Requires:
#		TCL_SRC_DIR	Assumes that TEA_LOAD_TCLCONFIG has
#				 already been called.
#
# Results:
#
#	Substs the following vars:
#		TCL_TOP_DIR_NATIVE
#		TCL_GENERIC_DIR_NATIVE
#		TCL_UNIX_DIR_NATIVE
#		TCL_WIN_DIR_NATIVE
#		TCL_BMAP_DIR_NATIVE
#		TCL_TOOL_DIR_NATIVE
#		TCL_PLATFORM_DIR_NATIVE
#		TCL_BIN_DIR_NATIVE
#		TCL_INCLUDES
#------------------------------------------------------------------------

AC_DEFUN(TEA_PRIVATE_TCL_HEADERS, [
    AC_MSG_CHECKING([for Tcl private include files])

    if test "${TEA_PLATFORM}" = "windows"; then
	TCL_TOP_DIR_NATIVE=\"`${CYGPATH} ${TCL_SRC_DIR}`\"
	TCL_GENERIC_DIR_NATIVE=\"`${CYGPATH} ${TCL_SRC_DIR}/generic`\"
	TCL_UNIX_DIR_NATIVE=\"`${CYGPATH} ${TCL_SRC_DIR}/unix`\"
	TCL_WIN_DIR_NATIVE=\"`${CYGPATH} ${TCL_SRC_DIR}/win`\"
	TCL_BMAP_DIR_NATIVE=\"`${CYGPATH} ${TCL_SRC_DIR}/bitmaps`\"
	TCL_TOOL_DIR_NATIVE=\"`${CYGPATH} ${TCL_SRC_DIR}/tools`\"
	TCL_COMPAT_DIR_NATIVE=\"`${CYGPATH} ${TCL_SRC_DIR}/compat`\"
	TCL_PLATFORM_DIR_NATIVE=${TCL_WIN_DIR_NATIVE}
    else
	TCL_TOP_DIR_NATIVE='$(TCL_SRC_DIR)'
	TCL_GENERIC_DIR_NATIVE='$(TCL_TOP_DIR_NATIVE)/generic'
	TCL_UNIX_DIR_NATIVE='$(TCL_TOP_DIR_NATIVE)/unix'
	TCL_WIN_DIR_NATIVE='$(TCL_TOP_DIR_NATIVE)/win'
	TCL_BMAP_DIR_NATIVE='$(TCL_TOP_DIR_NATIVE)/bitmaps'
	TCL_TOOL_DIR_NATIVE='$(TCL_TOP_DIR_NATIVE)/tools'
	TCL_COMPAT_DIR_NATIVE='$(TCL_TOP_DIR_NATIVE)/compat'
	TCL_PLATFORM_DIR_NATIVE=${TCL_UNIX_DIR_NATIVE}
    fi

    AC_SUBST(TCL_TOP_DIR_NATIVE)
    AC_SUBST(TCL_GENERIC_DIR_NATIVE)
    AC_SUBST(TCL_UNIX_DIR_NATIVE)
    AC_SUBST(TCL_WIN_DIR_NATIVE)
    AC_SUBST(TCL_BMAP_DIR_NATIVE)
    AC_SUBST(TCL_TOOL_DIR_NATIVE)
    AC_SUBST(TCL_PLATFORM_DIR_NATIVE)

    TCL_INCLUDES="-I${TCL_GENERIC_DIR_NATIVE} -I${TCL_PLATFORM_DIR_NATIVE}"
    AC_SUBST(TCL_INCLUDES)
    AC_MSG_RESULT([Using srcdir found in tclConfig.sh: ${TCL_SRC_DIR}])
])

#------------------------------------------------------------------------
# TEA_PUBLIC_TCL_HEADERS --
#
#	Locate the installed public Tcl header files
#
# Arguments:
#	None.
#
# Requires:
#	CYGPATH must be set
#
# Results:
#
#	Adds a --with-tclinclude switch to configure.
#	Result is cached.
#
#	Substs the following vars:
#		TCL_INCLUDES
#------------------------------------------------------------------------

AC_DEFUN(TEA_PUBLIC_TCL_HEADERS, [
    AC_MSG_CHECKING([for Tcl public headers])

    AC_ARG_WITH(tclinclude, [  --with-tclinclude      directory containing the public Tcl header files], with_tclinclude=${withval})

    AC_CACHE_VAL(ac_cv_c_tclh, [
	# Use the value from --with-tclinclude, if it was given

	if test x"${with_tclinclude}" != x ; then
	    if test -f "${with_tclinclude}/tcl.h" ; then
		ac_cv_c_tclh=${with_tclinclude}
	    else
		AC_MSG_ERROR([${with_tclinclude} directory does not contain tcl.h])
	    fi
	else
	    # Check order: pkg --prefix location, Tcl's --prefix location,
	    # directory of tclConfig.sh, and Tcl source directory.
	    # Looking in the source dir is not ideal, but OK.

	    eval "temp_includedir=${includedir}"
	    list="`ls -d ${temp_includedir}      2>/dev/null` \
		`ls -d ${TCL_PREFIX}/include     2>/dev/null` \
		`ls -d ${TCL_BIN_DIR}/../include 2>/dev/null` \
		`ls -d ${TCL_SRC_DIR}/generic    2>/dev/null`"
	    if test "${TEA_PLATFORM}" != "windows" -o "$GCC" = "yes"; then
		list="$list /usr/local/include /usr/include"
	    fi
	    for i in $list ; do
		if test -f "$i/tcl.h" ; then
		    ac_cv_c_tclh=$i
		    break
		fi
	    done
	fi
    ])

    # Print a message based on how we determined the include path

    if test x"${ac_cv_c_tclh}" = x ; then
	AC_MSG_ERROR([tcl.h not found.  Please specify its location with --with-tclinclude])
    else
	AC_MSG_RESULT([${ac_cv_c_tclh}])
    fi

    # Convert to a native path and substitute into the output files.

    INCLUDE_DIR_NATIVE=`${CYGPATH} ${ac_cv_c_tclh}`

    TCL_INCLUDES=-I\"${INCLUDE_DIR_NATIVE}\"

    AC_SUBST(TCL_INCLUDES)
])

#------------------------------------------------------------------------
# TEA_PRIVATE_TK_HEADERS --
#
#	Locate the private Tk include files
#
# Arguments:
#
#	Requires:
#		TK_SRC_DIR	Assumes that TEA_LOAD_TKCONFIG has
#				 already been called.
#
# Results:
#
#	Substs the following vars:
#		TK_INCLUDES
#------------------------------------------------------------------------

AC_DEFUN(TEA_PRIVATE_TK_HEADERS, [
    AC_MSG_CHECKING([for Tk private include files])

    if test "${TEA_PLATFORM}" = "windows"; then
	TK_TOP_DIR_NATIVE=\"`${CYGPATH} ${TK_SRC_DIR}`\"
	TK_UNIX_DIR_NATIVE=\"`${CYGPATH} ${TK_SRC_DIR}/unix`\"
	TK_WIN_DIR_NATIVE=\"`${CYGPATH} ${TK_SRC_DIR}/win`\"
	TK_GENERIC_DIR_NATIVE=\"`${CYGPATH} ${TK_SRC_DIR}/generic`\"
	TK_XLIB_DIR_NATIVE=\"`${CYGPATH} ${TK_SRC_DIR}/xlib`\"
	TK_PLATFORM_DIR_NATIVE=${TK_WIN_DIR_NATIVE}

	TK_INCLUDES="-I${TK_GENERIC_DIR_NATIVE} -I${TK_PLATFORM_DIR_NATIVE} -I${TK_XLIB_DIR_NATIVE}"
    else
	TK_TOP_DIR_NATIVE='$(TK_SRC_DIR)'
	TK_GENERIC_DIR_NATIVE='$(TK_TOP_DIR_NATIVE)/generic'
	TK_UNIX_DIR_NATIVE='$(TK_TOP_DIR_NATIVE)/unix'
	TK_WIN_DIR_NATIVE='$(TK_TOP_DIR_NATIVE)/win'
	TK_PLATFORM_DIR_NATIVE=${TK_UNIX_DIR_NATIVE}

	TK_INCLUDES="-I${TK_GENERIC_DIR_NATIVE} -I${TK_PLATFORM_DIR_NATIVE}"
    fi

    AC_SUBST(TK_TOP_DIR_NATIVE)
    AC_SUBST(TK_UNIX_DIR_NATIVE)
    AC_SUBST(TK_WIN_DIR_NATIVE)
    AC_SUBST(TK_GENERIC_DIR_NATIVE)
    AC_SUBST(TK_XLIB_DIR_NATIVE)
    AC_SUBST(TK_PLATFORM_DIR_NATIVE)

    AC_SUBST(TK_INCLUDES)
    AC_MSG_RESULT([Using srcdir found in tkConfig.sh: ${TK_SRC_DIR}])
])

#------------------------------------------------------------------------
# TEA_PUBLIC_TK_HEADERS --
#
#	Locate the installed public Tk header files
#
# Arguments:
#	None.
#
# Requires:
#	CYGPATH must be set
#
# Results:
#
#	Adds a --with-tkinclude switch to configure.
#	Result is cached.
#
#	Substs the following vars:
#		TK_INCLUDES
#------------------------------------------------------------------------

AC_DEFUN(TEA_PUBLIC_TK_HEADERS, [
    AC_MSG_CHECKING([for Tk public headers])

    AC_ARG_WITH(tkinclude, [  --with-tkinclude      directory containing the public Tk header files.], with_tkinclude=${withval})

    AC_CACHE_VAL(ac_cv_c_tkh, [
	# Use the value from --with-tkinclude, if it was given

	if test x"${with_tkinclude}" != x ; then
	    if test -f "${with_tkinclude}/tk.h" ; then
		ac_cv_c_tkh=${with_tkinclude}
	    else
		AC_MSG_ERROR([${with_tkinclude} directory does not contain tk.h])
	    fi
	else
	    # Check order: pkg --prefix location, Tcl's --prefix location,
	    # directory of tclConfig.sh, and Tcl source directory.
	    # Looking in the source dir is not ideal, but OK.

	    eval "temp_includedir=${includedir}"
	    list="`ls -d ${temp_includedir}      2>/dev/null` \
		`ls -d ${TK_PREFIX}/include      2>/dev/null` \
		`ls -d ${TCL_PREFIX}/include     2>/dev/null` \
		`ls -d ${TCL_BIN_DIR}/../include 2>/dev/null` \
		`ls -d ${TK_SRC_DIR}/generic     2>/dev/null`"
	    if test "${TEA_PLATFORM}" != "windows" -o "$GCC" = "yes"; then
		list="$list /usr/local/include /usr/include"
	    fi
	    for i in $list ; do
		if test -f "$i/tk.h" ; then
		    ac_cv_c_tkh=$i
		    break
		fi
	    done
	fi
    ])

    # Print a message based on how we determined the include path

    if test x"${ac_cv_c_tkh}" = x ; then
	AC_MSG_ERROR([tk.h not found.  Please specify its location with --with-tkinclude])
    else
	AC_MSG_RESULT([${ac_cv_c_tkh}])
    fi

    # Convert to a native path and substitute into the output files.

    INCLUDE_DIR_NATIVE=`${CYGPATH} ${ac_cv_c_tkh}`

    TK_INCLUDES=-I\"${INCLUDE_DIR_NATIVE}\"

    AC_SUBST(TK_INCLUDES)
])

#------------------------------------------------------------------------
# TEA_PROG_TCLSH
#	Locate a tclsh shell in the following directories:
#		${TCL_BIN_DIR}		${TCL_BIN_DIR}/../bin
#		${exec_prefix}/bin	${prefix}/bin
#		${PATH}
#
# Arguments
#	none
#
# Results
#	Subst's the following values:
#		TCLSH_PROG
#------------------------------------------------------------------------

AC_DEFUN(TEA_PROG_TCLSH, [
    AC_MSG_CHECKING([for tclsh])

    AC_CACHE_VAL(ac_cv_path_tclsh, [
	search_path=`echo ${TCL_BIN_DIR}:${TCL_BIN_DIR}/../bin:${exec_prefix}/bin:${prefix}/bin:${PATH} | sed -e 's/:/ /g'`
	for dir in $search_path ; do
	    for j in `ls -r $dir/tclsh[[8-9]]*${EXEEXT} 2> /dev/null` \
		    `ls -r $dir/tclsh*${EXEEXT} 2> /dev/null` ; do
		if test x"$ac_cv_path_tclsh" = x ; then
		    if test -f "$j" ; then
			ac_cv_path_tclsh=$j
			break
		    fi
		fi
	    done
	done
    ])

    if test -f "$ac_cv_path_tclsh" ; then
	TCLSH_PROG=$ac_cv_path_tclsh
	AC_MSG_RESULT([$TCLSH_PROG])
    else
	AC_MSG_RESULT([no])
#	AC_MSG_ERROR([No tclsh found in PATH:  $search_path])
    fi
    AC_SUBST(TCLSH_PROG)
])

#------------------------------------------------------------------------
# TEA_PROG_WISH
#	Locate a wish shell in the following directories:
#		${TK_BIN_DIR}		${TK_BIN_DIR}/../bin
#		${TCL_BIN_DIR}		${TCL_BIN_DIR}/../bin
#		${exec_prefix}/bin	${prefix}/bin
#		${PATH}
#
# Arguments
#	none
#
# Results
#	Subst's the following values:
#		WISH_PROG
#------------------------------------------------------------------------

AC_DEFUN(TEA_PROG_WISH, [
    AC_MSG_CHECKING([for wish])

    AC_CACHE_VAL(ac_cv_path_wish, [
	search_path=`echo ${TK_BIN_DIR}:${TK_BIN_DIR}/../bin:${TCL_BIN_DIR}:${TCL_BIN_DIR}/../bin:${exec_prefix}/bin:${prefix}/bin:${PATH} | sed -e 's/:/ /g'`
	for dir in $search_path ; do
	    for j in `ls -r $dir/wish[[8-9]]*${EXEEXT} 2> /dev/null` \
		    `ls -r $dir/wish*${EXEEXT} 2> /dev/null` ; do
		if test x"$ac_cv_path_wish" = x ; then
		    if test -f "$j" ; then
			ac_cv_path_wish=$j
			break
		    fi
		fi
	    done
	done
    ])

    if test -f "$ac_cv_path_wish" ; then
	WISH_PROG=$ac_cv_path_wish
	AC_MSG_RESULT([$WISH_PROG])
    else
	AC_MSG_ERROR([No wish found in PATH:  $search_path])
    fi
    AC_SUBST(WISH_PROG)
])

#------------------------------------------------------------------------
# TEA_PATH_CONFIG --
#
#	Locate the ${1}Config.sh file and perform a sanity check on
#	the ${1} compile flags.  These are used by packages like
#	[incr Tk] that load *Config.sh files from more than Tcl and Tk.
#
# Arguments:
#	none
#
# Results:
#
#	Adds the following arguments to configure:
#		--with-$1=...
#
#	Defines the following vars:
#		$1_BIN_DIR	Full path to the directory containing
#				the $1Config.sh file
#------------------------------------------------------------------------

AC_DEFUN(TEA_PATH_CONFIG, [
    #
    # Ok, lets find the $1 configuration
    # First, look for one uninstalled.
    # the alternative search directory is invoked by --with-$1
    #

    if test x"${no_$1}" = x ; then
	# we reset no_$1 in case something fails here
	no_$1=true
	AC_ARG_WITH($1, [  --with-$1              directory containing $1 configuration ($1Config.sh)], with_$1config=${withval})
	AC_MSG_CHECKING([for $1 configuration])
	AC_CACHE_VAL(ac_cv_c_$1config,[

	    # First check to see if --with-$1 was specified.
	    if test x"${with_$1config}" != x ; then
		if test -f "${with_$1config}/$1Config.sh" ; then
		    ac_cv_c_$1config=`(cd ${with_$1config}; pwd)`
		else
		    AC_MSG_ERROR([${with_$1config} directory doesn't contain $1Config.sh])
		fi
	    fi

	    # then check for a private $1 installation
	    if test x"${ac_cv_c_$1config}" = x ; then
		for i in \
			../$1 \
			`ls -dr ../$1[[8-9]].[[0-9]]* 2>/dev/null` \
			../../$1 \
			`ls -dr ../../$1[[8-9]].[[0-9]]* 2>/dev/null` \
			../../../$1 \
			`ls -dr ../../../$1[[8-9]].[[0-9]]* 2>/dev/null` \
			${srcdir}/../$1 \
			`ls -dr ${srcdir}/../$1[[8-9]].[[0-9]]* 2>/dev/null` \
			; do
		    if test -f "$i/$1Config.sh" ; then
			ac_cv_c_$1config=`(cd $i; pwd)`
			break
		    fi
		    if test -f "$i/unix/$1Config.sh" ; then
			ac_cv_c_$1config=`(cd $i/unix; pwd)`
			break
		    fi
		done
	    fi

	    # check in a few common install locations
	    if test x"${ac_cv_c_$1config}" = x ; then
		for i in `ls -d ${exec_prefix}/lib 2>/dev/null` \
			`ls -d ${prefix}/lib 2>/dev/null` \
			`ls -d /usr/local/lib 2>/dev/null` \
			`ls -d /usr/contrib/lib 2>/dev/null` \
			`ls -d /usr/lib 2>/dev/null` \
			; do
		    if test -f "$i/$1Config.sh" ; then
			ac_cv_c_$1config=`(cd $i; pwd)`
			break
		    fi
		done
	    fi
	])

	if test x"${ac_cv_c_$1config}" = x ; then
	    $1_BIN_DIR="# no $1 configs found"
	    AC_MSG_WARN("Cannot find $1 configuration definitions")
	    exit 0
	else
	    no_$1=
	    $1_BIN_DIR=${ac_cv_c_$1config}
	    AC_MSG_RESULT([found $$1_BIN_DIR/$1Config.sh])
	fi
    fi
])

#------------------------------------------------------------------------
# TEA_LOAD_CONFIG --
#
#	Load the $1Config.sh file
#
# Arguments:
#	
#	Requires the following vars to be set:
#		$1_BIN_DIR
#
# Results:
#
#	Subst the following vars:
#		$1_SRC_DIR
#		$1_LIB_FILE
#		$1_LIB_SPEC
#
#------------------------------------------------------------------------

AC_DEFUN(TEA_LOAD_CONFIG, [
    AC_MSG_CHECKING([for existence of ${$1_BIN_DIR}/$1Config.sh])

    if test -f "${$1_BIN_DIR}/$1Config.sh" ; then
        AC_MSG_RESULT([loading])
	. ${$1_BIN_DIR}/$1Config.sh
    else
        AC_MSG_RESULT([file not found])
    fi

    #
    # If the $1_BIN_DIR is the build directory (not the install directory),
    # then set the common variable name to the value of the build variables.
    # For example, the variable $1_LIB_SPEC will be set to the value
    # of $1_BUILD_LIB_SPEC. An extension should make use of $1_LIB_SPEC
    # instead of $1_BUILD_LIB_SPEC since it will work with both an
    # installed and uninstalled version of Tcl.
    #

    if test -f ${$1_BIN_DIR}/Makefile ; then
	AC_MSG_WARN([Found Makefile - using build library specs for $1])
        $1_LIB_SPEC=${$1_BUILD_LIB_SPEC}
        $1_STUB_LIB_SPEC=${$1_BUILD_STUB_LIB_SPEC}
        $1_STUB_LIB_PATH=${$1_BUILD_STUB_LIB_PATH}
    fi

    AC_SUBST($1_VERSION)
    AC_SUBST($1_SRC_DIR)

    AC_SUBST($1_LIB_FILE)
    AC_SUBST($1_LIB_SPEC)

    AC_SUBST($1_STUB_LIB_FILE)
    AC_SUBST($1_STUB_LIB_SPEC)
    AC_SUBST($1_STUB_LIB_PATH)
])
