#                     A U T O H E A D E R . C M A K E
#
# BRL-CAD
#
# Copyright (c) 2011-2016 United States Government as represented by the U.S.
# Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# 3. The name of the author may not be used to endorse or promote products
# derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# The purpose of this logic is to scan a set of C/C++ source files to identify
# all of the variables the CMake configure process may need to set.

# The global property C_DEFINE_VARS will hold the list of defined variables
# found.

# A user can suppress consideration of definitions that match one or a set of
# patterns by appending those patterns to the C_DEFINE_SKIPVARS global
# property.  There are some "standard" patterns that are automatically skipped
# - tests matching the "header guard" pattern are skipped, as are defines
# matching the "EXPORT" and "IMPORT" substrings, operating system and compiler
# identifiers, and various other flags that do not typically need user-defined
# configure stage testing.
set_property(GLOBAL APPEND PROPERTY C_DEFINE_SKIPVARS ".*EXPORT.*")
set_property(GLOBAL APPEND PROPERTY C_DEFINE_SKIPVARS ".*IMPORT.*")

function(autoheader_ignore_define def)
  set_property(GLOBAL APPEND PROPERTY C_DEFINE_SKIPVARS ".*[ (\t]+${def}[ )\t$]*.*")
endfunction(autoheader_ignore_define)

function(autoheader_ignore_defines deflist)
  foreach(def ${${deflist}})
    autoheader_ignore_define(${def})
  endforeach(def ${${deflist}})
endfunction(autoheader_ignore_defines)

# OS flags.  For performance, put most common
# comparisons first (that'll typicall be
# Windows related tests)
set(OS_FLAGS
  WIN32
  WIN32_LEAN_AND_MEAN
  _WIN32
  _WIN64
  _WINDOWS
  __WIN32__
  __WIN64
  __WINDOWS__
  __APPLE__
  BSD
  CRAY1
  CRAY2
  FREEBSD
  MSDOS
  SUNOS
  SYSV
  _AIX
  _BEOS
  _BEOS_
  _IBMR2
  _SCO_SV
  _SGI
  _ULTRIX
  _WINSOCKAPI_
  _XENIX
  __AIX
  __AIX__
  __ANDROID__
  __BEOS__
  __BeOS
  __CRAYXC
  __CRAYXE
  __CYGWIN__
  __DARWIN__
  __DOS__
  __FreeBSD__
  __GNU__
  __HAIKU__
  __LINUX__
  __MSDOS__
  __NT__
  __NetBSD
  __NetBSD__
  __OPENBSD
  __OS2__
  __OS400__
  __OpenBSD__
  __QNX__
  __SCO_VERSION__
  __TRU64__
  __aix
  __aix__
  __alpha
  __alpha__
  __apollo
  __bsdos__
  __convex__NATIVE
  __hppa
  __hpua
  __hpux
  __i386
  __i386__
  __ia64__
  __linux
  __linux__
  __minux
  __osf
  __osf__
  __ppc64__
  __ppc__
  __riscos
  __riscos__
  __sgi
  __sinix
  __sinix__
  __sp3__ # what is this? context indicates it is a platform...
  __sparc64__
  __sun
  __tru64
  __ultrix
  __ultrix__
  __x86_64__
  _sgi
  _sun
  _tru64
  convex_NATIVE
  eta10
  hpux
  ibm
  linux
  mips
  sco_sv
  sgi
  sun
  vax
  )

# Compiler flags - again, group common tests at front.
set(COMPILER_FLAGS
  __STDC__
  _MSC_VER
  __GNUC__
  __GLIBC_MINOR__
  __GLIBC__
  !GCC_PREREQ
  GCC_PREREQ
  ICC_PREREQ
  _CRAYC
  __APPLE_CC__
  __BORLANDC__
  __clang__
  __DECC
  __INTEL_COMPILER
  __MINGW32__
  __STDC_VERSION__
  __SUNPRO_C
  __SUNPRO_CC
  __TURBOC__
  __USE_GNU
  __WATCOMC__
  __WATCOM_INT64__
  __WATCOM__
  )

# Language flags
set(LANG_FLAGS
  __cplusplus
  __OBJC__
  )

# macros with define guards
set(MACRO_FLAGS
  UNUSED
  LIKELY
  UNLIKELY
  DEPRECATED
  )

autoheader_ignore_defines(OS_FLAGS)
autoheader_ignore_defines(COMPILER_FLAGS)
autoheader_ignore_defines(LANG_FLAGS)
autoheader_ignore_defines(MACRO_FLAGS)

#get_property(C_DEFINE_SKIPVARS GLOBAL PROPERTY C_DEFINE_SKIPVARS)
#message("C_DEFINE_SKIPVARS: ${C_DEFINE_SKIPVARS}")

# Given a file and relative directory, construct the expected
# header guard ifdef string
macro(header_guard rdir fname ovar)
  string(REPLACE "${rdir}/" "" stage1 ${fname})
  string(REPLACE "/" "_" stage2 ${stage1})
  string(REPLACE "." "_" stage3 ${stage2})
  string(TOUPPER "${stage3}" stage4)
  set(${ovar} ${stage4})
endmacro(header_guard)

# Given an atom from a line (i.e. one conditional if the original line had
# and/or branches, or the original line if there were no and/or branches),
# extract the compile flag
macro(compile_def_flag atom ovar)
  string(REGEX REPLACE "^[ \t]*#*.*[!]*(ifdef|defined|elseif defined)[ \t]*[(]*" "" stage1 "${atom}")
  string(REGEX REPLACE "[/][*].*[*][/]" "" stage2 "${stage1}")
  string(REGEX REPLACE "[)][ \t]*" "" stage3 "${stage2}")
  string(REGEX REPLACE "[ \t]+$" "" stage4 "${stage3}")
  string(REGEX REPLACE "^[ \t]+" "" stage5 "${stage4}")
  set(${ovar} ${stage5})
endmacro(compile_def_flag)

function(autoheader_scan seed_dir)
  set(scan_files)
  set(extensions c h cxx cpp hxx hpp)
  get_property(C_DEFINE_SKIPVARS GLOBAL PROPERTY C_DEFINE_SKIPVARS)

  foreach(exten ${extensions})
    set(sfiles)
    file(GLOB_RECURSE sfiles "${seed_dir}/*.${exten}")
    set(scan_files ${scan_files} ${sfiles})
  endforeach(exten ${extensions})

  #message("found files: ${scan_files}")

  set(unique_keys)
  foreach(sf ${scan_files})
    header_guard("${seed_dir}" "${sf}" hg)
    set(SKIP_LIST ".*[ \t]+${hg}[ \t]*" ${C_DEFINE_SKIPVARS})
    file(STRINGS ${sf} SRC_LINES REGEX "^[ \t]*#.*(ifdef|defined[(]).+")
    set(def_list)
    foreach(line ${SRC_LINES})
      #message("line: ${line}")
      set(lset ${line})
      string(REPLACE "&&" ";" lset "${lset}")
      string(REPLACE "||" ";" lset "${lset}")
      foreach(l ${lset})
	#message("   atom: ${l}")
	set(skip_line 0)
	foreach(sk ${SKIP_LIST})
	  if(NOT skip_line)
	    #message("testing ${l} vs ${sk}")
	    if("${l}" MATCHES "${sk}")
	      #message("matched ${l} vs ${sk} - skipping")
	      set(skip_line 1)
	    endif("${l}" MATCHES "${sk}")
	  endif(NOT skip_line)
	endforeach(sk ${SKIP_LIST})
	if(NOT skip_line)
	  set(def_list ${def_list} ${l})
	endif(NOT skip_line)
      endforeach(l ${lset})
    endforeach(line ${SRC_LINES})
    if(def_list)
      #message("file ${sf} def lines:")
      foreach(l ${def_list})
	set(dflag)
	compile_def_flag(${l} dflag)
	#message("   line: ${l}")
	#message("   extracted: ${dflag}")
	set(unique_keys ${unique_keys} ${dflag})
      endforeach(l ${def_list})
    endif(def_list)
  endforeach(sf ${scan_files})
  list(REMOVE_DUPLICATES unique_keys)
  foreach(k ${unique_keys})
    set_property(GLOBAL APPEND PROPERTY C_DEFINE_VARS "${k}")
  endforeach(k ${unique_keys})
  get_property(C_DEFINE_VARS GLOBAL PROPERTY C_DEFINE_VARS)
  list(REMOVE_DUPLICATES C_DEFINE_VARS)
  list(SORT C_DEFINE_VARS)
  set_property(GLOBAL PROPERTY C_DEFINE_VARS "${C_DEFINE_VARS}")
endfunction(autoheader_scan)

# For testing convenience, the following will exercise the
# logic in cmake -P script mode.  Should be removed if/when
# this goes "live" in the real build.

set(LOCAL_SKIP DIRENT_ BRLCADBUILD BRLCAD_ DM_ FB_ NO_BOMBING_MACROS USE_BINARY_ATTRIBUTES
  IF_WGL IF_X IF_QT IF_OGL NO_DEBUG_CHECKING USE_OPENCL)
autoheader_ignore_defines(LOCAL_SKIP)

set_property(GLOBAL APPEND PROPERTY C_DEFINE_SKIPVARS ".*IMPORT.*")
autoheader_scan(${CMAKE_CURRENT_SOURCE_DIR}/include)
autoheader_scan(${CMAKE_CURRENT_SOURCE_DIR}/src/libbu)

get_property(C_DEFINE_VARS GLOBAL PROPERTY C_DEFINE_VARS)
message("keys:")
foreach(k ${C_DEFINE_VARS})
  message("      ${k}")
endforeach(k ${def_list})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

