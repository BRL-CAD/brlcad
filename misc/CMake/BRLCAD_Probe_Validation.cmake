#   B R L C A D _ P R O B E _ V A L I D A T I O N . C M A K E
# BRL-CAD
#
# Copyright (c) 2011-2026 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###
# Differential-validation support for the configure-time probe system.
#
# BRL-CAD's feature/flag/type/library tests can run either one-at-a-time via
# try_compile (BRLCAD_ENABLE_PARALLEL_CONFIG_PROBES=OFF) or batched into
# parallel mini-project builds (=ON).  Batching - and, in a later phase,
# running those batches under a faster generator such as Ninja - is only
# acceptable if it produces *identical* test results to the reference path.
#
# Rather than assume equivalence, we make it checkable.  Every check_*
# result lands in the CMake cache, so brlcad_write_probe_snapshot() dumps
# the whole cache to a stable, sorted, one-record-per-line file.  Two
# configures (for example batched vs. reference, or Ninja vs. MSBuild) can
# then be compared with the native tool in misc/CMake/probe_validation/; any
# divergence in a test-result variable is a batch/generator bug to fix before
# trusting the faster path.
#
# A configure emits a snapshot only when BRLCAD_PROBE_SNAPSHOT_FILE is set
# to an output path, so this is zero cost for normal builds.

if(COMMAND brlcad_write_probe_snapshot)
  return()
endif(COMMAND brlcad_write_probe_snapshot)

# Write a snapshot of every cache variable to "outfile", sorted by name,
# one "NAME=VALUE" record per line.  Lines beginning with '#' are metadata
# describing the toolchain/generator that produced the snapshot; the diff
# tool treats them as context, not as comparable results.
function(brlcad_write_probe_snapshot outfile)
  get_cmake_property(_all_cache_vars CACHE_VARIABLES)
  list(SORT _all_cache_vars)

  # Metadata header - records the environment so a later diff can tell, at a
  # glance, whether it is comparing batch-vs-reference (same generator) or a
  # cross-generator run (e.g. Ninja vs. Visual Studio).
  set(_snapshot "# BRL-CAD configure probe snapshot\n")
  string(APPEND _snapshot "# generator=${CMAKE_GENERATOR}\n")
  string(APPEND _snapshot "# generator_platform=${CMAKE_GENERATOR_PLATFORM}\n")
  string(APPEND _snapshot "# generator_toolset=${CMAKE_GENERATOR_TOOLSET}\n")
  string(APPEND _snapshot "# build_type=${CMAKE_BUILD_TYPE}\n")
  string(APPEND _snapshot "# c_compiler=${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}\n")
  string(APPEND _snapshot "# cxx_compiler=${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\n")
  string(APPEND _snapshot "# parallel_config_probes=${BRLCAD_ENABLE_PARALLEL_CONFIG_PROBES}\n")
  # Recorded so the diff tool can neutralize build/source-dir paths embedded in
  # cache values (found library paths, output directories, etc.), which differ
  # between two build trees for reasons unrelated to test results.
  string(APPEND _snapshot "# binary_dir=${CMAKE_BINARY_DIR}\n")
  string(APPEND _snapshot "# source_dir=${CMAKE_SOURCE_DIR}\n")

  foreach(_v IN LISTS _all_cache_vars)
    set(_val "${${_v}}")
    # Keep every record on a single line so the file diffs cleanly.  CMake
    # list separators (';') are preserved verbatim; only embedded newlines
    # are escaped.
    string(REPLACE "\r" "" _val "${_val}")
    string(REPLACE "\n" "\\n" _val "${_val}")
    string(APPEND _snapshot "${_v}=${_val}\n")
  endforeach(_v IN LISTS _all_cache_vars)

  file(WRITE "${outfile}" "${_snapshot}")
  message(STATUS "Wrote configure probe snapshot: ${outfile}")
endfunction(brlcad_write_probe_snapshot)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
