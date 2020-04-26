# Copyright (c) 2005 Eolas Technologies Inc.
# All rights reserved.
#
# This Open Source project was made possible through the financial support
# of Eolas Technologies Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of the <ORGANIZATION> nor the names of its
#       contributors may be used to endorse or promote products derived from
#       this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

# hv3_encodings.tcl
#
#     This file contains wrappers around the Tcl built-in commands
#     [fconfigure] and [encoding]. The purpose is to support identifiers
#     like "windows-1257" as an alias for "cp1257". We need to replace
#     the original commands so that the http package sees our encoding
#     database.
#
#     To add new encoding aliases, entries should be added to the
#     global ::Hv3EncodingMap array. This array maps from identifiers
#     commonly used on the web to the canonical name used by Tcl. For
#     example, some Japanese websites use "shift_jis", but Tcl calls
#     this encoding "shiftjis". To work around this, we add the following
#     entry to ::Hv3EncodingMap:
#
#          set ::Hv3EncodingMap(shift_jis) shiftjis
#
#     Entries may be added to ::Hv3EncodingMap at any time (even before
#     this file is [source]ed).
#


rename encoding encoding_orig
rename fconfigure fconfigure_orig

# encoding convertfrom ?encoding? data
# encoding convertto ?encoding? string
# encoding names
#
proc encoding {args} {
  set argv $args

  # Handle [encoding names]
  #
  if {[llength $argv] == 1 && [lindex $argv 0] eq "names"} {
    return [concat [array names ::Hv3EncodingMap] [encoding_orig names]]
  }

  # Map any explicitly specified encoding.
  #
  if {[llength $argv] == 3} {
    set enc [string tolower [lindex $argv 1]]
    if {[info exists ::Hv3EncodingMap($enc)]} {
      lset argv 1 $::Hv3EncodingMap($enc)
    }
  }

  # Call the real [encoding] command.
  eval encoding_orig $argv
}

# fconfigure channelId name value ?name value ...?
#
proc fconfigure {args} {
  set argv $args
  for {set ii 1} {($ii+1) < [llength $argv]} {incr ii 2} {
    if {[lindex $argv $ii] eq "-encoding"} {
      set enc [string tolower [lindex $argv [expr {$ii+1}]]]
      if {[info exists ::Hv3EncodingMap($enc)]} {
	lset argv [expr {$ii+1}] $::Hv3EncodingMap($enc)
      }
    }
  }

  # Call the real [fconfigure] command.
  eval fconfigure_orig $argv
}

namespace eval ::hv3 {

  # The argument is an encoding name, which may or may not be known to Tcl.
  # Return the name of the Tcl encoding that will be used by Hv3.
  #
  proc encoding_resolve {enc} {
    set encoding [string tolower $enc]
    if {[info exists ::Hv3EncodingMap($encoding)]} {
      set ::Hv3EncodingMap($encoding)
    } else {
      encoding system
    }
  }

  # The two arguments are encoding names. This proc returns true if the
  # two encodings are handled identically by Hv3.
  #
  proc ::hv3::encoding_isequal {enc1 enc2} {
    string equal [::hv3::encoding_resolve $enc1] [::hv3::encoding_resolve $enc2]
  }
}

##########################################################################
# Below this point is where new encoding alias' can be added. See
# the comment in the file header for instructions.
#

# Build the mappings "database".
#
foreach name [encoding_orig names] {
  set ::Hv3EncodingMap($name) $name
  if {[string match cp* $name]} {
    set name2 "windows-[string range $name 2 end]"
    set ::Hv3EncodingMap($name2) $name
  }
  if {[string match iso* $name]} {
    set name2 "iso-[string range $name 3 end]"
    set ::Hv3EncodingMap($name2) $name
  }
}

# Deal with some Japanese encodings. Because of the dominance of
# Microsoft, websites that specify "shift_jis" or "shiftjis" as an
# encoding are usually better handled with cp932. So, if cp932 is
# present, use it in preference to the encoding Tcl calls shiftjis.
#
if {[lsearch [encoding_orig names] cp932]>=0} {
  set ::Hv3EncodingMap(shiftjis) cp932
  set ::Hv3EncodingMap(shift_jis) cp932
} else {
  set ::Hv3EncodingMap(shift_jis) shiftjis
}

# Various encodings best handled by pretending they are utf-8.
set ::Hv3EncodingMap(us-ascii) utf-8
set ::Hv3EncodingMap(iso-8559-1) utf-8

# Thai encoding.
set ::Hv3EncodingMap(windows-874) tis-620
