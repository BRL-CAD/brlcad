#                      M I M E . C M A K E
# BRL-CAD
#
# Copyright (c) 2015 United States Government as represented by
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
#
# Generate libbu's mime type enums and C functions from text inputs
#

set(mime_h_file_tmp ${CMAKE_CURRENT_BINARY_DIR}/mime.h.tmp)
set(mime_c_file_tmp ${CMAKE_CURRENT_BINARY_DIR}/mime.c.tmp)

file(READ ${BRLCAD_SOURCE_DIR}/misc/mime.types MIME_TYPES)
file(READ ${BRLCAD_SOURCE_DIR}/misc/mime_cad.types CAD_TYPES)
set(h_contents "/* Mime type mappings automatically generated from mime.types\n * and mime_cad.types.\n * Do not edit these files directly - apply updates to include/bu source files. */\n\n")
set(h_contents "${h_contents}#ifndef BU_MIME_H\n")
set(h_contents "${h_contents}#define BU_MIME_H\n")
set(h_contents "${h_contents}\n#include \"common.h\"\n")
set(h_contents "${h_contents}#include \"bu/defines.h\"\n")
set(h_contents "${h_contents}\n__BEGIN_DECLS\n")

set(c_contents "/* Functions for mapping file extensions to mime type. Automatically\n * generated from mime.types and mime_cad.types.\n * Do not edit these files directly - apply updates to include/bu source files. */\n\n")
set(c_contents "${c_contents}\n#include \"common.h\"\n")
set(c_contents "${c_contents}#include \"bu/mime.h\"\n")
set(c_contents "${c_contents}#include \"bu/file.h\"\n")
set(c_contents "${c_contents}#include \"bu/str.h\"\n")

set(MIME_TYPES "${MIME_TYPES}\n${CAD_TYPES}")
string(REGEX REPLACE "\r?\n" ";" TYPES "${MIME_TYPES}")

set(ACTIVE_TYPES)
foreach(line ${TYPES})
  if(NOT ${line} MATCHES "[#]")
    set(elements)
    string(REPLACE " " ";" linescrub "${line}")
    string(REPLACE "\t" ";" linescrub "${linescrub}")
    foreach(e ${linescrub})
      set(elements ${elements} ${e})
    endforeach(e ${linescrub})
    list(GET elements 0 mtype)
    string(REGEX REPLACE "/.+" "" mgrp "${mtype}")
    string(REPLACE "-" "_DASH_" mgrp "${mgrp}")
    set(ACTIVE_TYPES ${ACTIVE_TYPES} ${mgrp})
  endif(NOT ${line} MATCHES "[#]")
endforeach(line ${TYPES})
list(REMOVE_DUPLICATES ACTIVE_TYPES)
list(SORT ACTIVE_TYPES)

foreach(line ${TYPES})
  if(NOT ${line} MATCHES "[#]")
    set(elements)
    string(REPLACE " " ";" linescrub "${line}")
    string(REPLACE "\t" ";" linescrub "${linescrub}")
    foreach(e ${linescrub})
      set(elements ${elements} ${e})
    endforeach(e ${linescrub})
    list(GET elements 0 mtype)
    list(REMOVE_AT elements 0)
    string(REGEX REPLACE "/.+" "" mime_group "${mtype}")
    string(REPLACE "-" "_DASH_" mime_group "${mime_group}")
    set(mime_type "${mtype}")
    string(TOUPPER "${mime_type}" mime_type)
    string(REPLACE "/" "_" mime_type "${mime_type}")
    string(REPLACE "." "_" mime_type "${mime_type}")
    string(REPLACE "+" "_PLUS_" mime_type "${mime_type}")
    string(REPLACE "-" "_DASH_" mime_type "${mime_type}")
    set(${mime_group}_types ${${mime_group}_types} ${mime_type})
    foreach(e ${elements})
      set(${mime_type}_extensions ${${mime_type}_extensions} ${e})
    endforeach(e ${elements})
  endif(NOT ${line} MATCHES "[#]")
endforeach(line ${TYPES})

# HEADER with typedefs

set(mcstr "typedef enum {")
foreach(context ${ACTIVE_TYPES})
  string(TOUPPER "${context}" c)
  set(mcstr "${mcstr}\n    MIME_${c},")
endforeach(context ${ACTIVE_TYPES})
set(mcstr "${mcstr}\n    MIME_UNKNOWN")
set(mcstr "${mcstr}\n} mime_context_t;\n\n")
set(h_contents "${h_contents}\n${mcstr}")

foreach(c ${ACTIVE_TYPES})
  set(enum_str "typedef enum {")
  list(SORT ${c}_types)
  string(TOUPPER "${c}" c_u)
  # Default first enum for all types is an auto type
  set(enum_str "${enum_str}\n    MIME_${c_u}_AUTO,")
  foreach(type ${${c}_types})
    set(enum_str "${enum_str}\n    MIME_${type},")
  endforeach(type ${${c}_types})
  set(enum_str "${enum_str}\n    MIME_${c_u}_UNKNOWN")
  set(enum_str "${enum_str}\n} mime_${c}_t;\n\n")
  set(h_contents "${h_contents}\n${enum_str}")
endforeach(c ${ACTIVE_TYPES})
set(h_contents "${h_contents}\n__END_DECLS\n")
set(h_contents "${h_contents}\n#endif /*BU_MIME_H*/\n")

# C mapping functions - extension to type
foreach(c ${ACTIVE_TYPES})
  set(enum_str "HIDDEN int\nbu_file_mime_${c}(const char *ext)\n{")
  list(SORT ${c}_types)
  foreach(type ${${c}_types})
    foreach(ext ${${type}_extensions})
      set(enum_str "${enum_str}\n    if(BU_STR_EQUIV(ext, \"${ext}\")) return ((int)MIME_${type});\n")
    endforeach(ext ${${type}_extensions})
  endforeach(type ${${c}_types})
  set(enum_str "${enum_str}\n    return -1;\n}\n")
  set(c_contents "${c_contents}\n${enum_str}")
endforeach(c ${ACTIVE_TYPES})

# Public C mapping function - extension to type
set(mcstr "\nint bu_file_mime(const char *ext, mime_context_t context)\n{")
set(mcstr "${mcstr}\n    switch (context) {\n")
foreach(context ${ACTIVE_TYPES})
  string(TOUPPER "${context}" c)
  set(mcstr "${mcstr}        case MIME_${c}:\n")
  set(mcstr "${mcstr}             return bu_file_mime_${context}(ext);\n")
  set(mcstr "${mcstr}             break;\n")
endforeach(context ${ACTIVE_TYPES})
set(mcstr "${mcstr}        default:\n")
set(mcstr "${mcstr}             return -1;\n")
set(mcstr "${mcstr}             break;\n")
set(mcstr "${mcstr}    }\n")
set(mcstr "${mcstr}    return -1;\n")
set(mcstr "${mcstr}}\n")
set(c_contents "${c_contents}\n${mcstr}")

# C mapping functions - type to string
foreach(c ${ACTIVE_TYPES})
  set(enum_str "HIDDEN const char *\nbu_file_mime_str_${c}(int type)\n{")
  string(TOUPPER "${c}" c_u)
  set(enum_str "${enum_str}\n    const char *ret = NULL;")
  set(enum_str "${enum_str}\n    if (type < 0 || type >= MIME_${c_u}_UNKNOWN) { ret = bu_strdup(\"MIME_${c_u}_UNKNOWN\"); goto found_type; }\n")
  list(SORT ${c}_types)
  foreach(type ${${c}_types})
    set(enum_str "${enum_str}\n    if(type == (int)MIME_${type}) { ret = bu_strdup(\"MIME_${type}\"); goto found_type; }\n")
  endforeach(type ${${c}_types})
  set(enum_str "${enum_str}\n    found_type:\n")
  set(enum_str "${enum_str}\n    return ret;\n}\n")
  set(c_contents "${c_contents}\n${enum_str}")
endforeach(c ${ACTIVE_TYPES})

# Public C mapping function - type to string
set(mcstr "\nconst char *bu_file_mime_str(int t, mime_context_t context)\n{")
set(mcstr "${mcstr}\n    switch (context) {\n")
foreach(context ${ACTIVE_TYPES})
  string(TOUPPER "${context}" c)
  set(mcstr "${mcstr}        case MIME_${c}:\n")
  set(mcstr "${mcstr}             return bu_file_mime_str_${context}(t);\n")
  set(mcstr "${mcstr}             break;\n")
endforeach(context ${ACTIVE_TYPES})
set(mcstr "${mcstr}        default:\n")
set(mcstr "${mcstr}             return NULL;\n")
set(mcstr "${mcstr}             break;\n")
set(mcstr "${mcstr}    }\n")
set(mcstr "${mcstr}    return NULL;\n")
set(mcstr "${mcstr}}\n")
set(c_contents "${c_contents}\n${mcstr}")


# Public C mapping function - string to type
set(enum_str "HIDDEN int\nbu_file_mime_int(const char *str)\n{")
foreach(c ${ACTIVE_TYPES})
  list(SORT ${c}_types)
  foreach(type ${${c}_types})
    set(enum_str "${enum_str}\n  if (BU_STR_EQUAL(str, \"MIME_${type}\")) return (int)MIME_${type};\n")
  endforeach(type ${${c}_types})
endforeach(c ${ACTIVE_TYPES})
set(enum_str "${enum_str}\n  return -1;\n}\n")
set(c_contents "${c_contents}\n${enum_str}")

# C mapping functions - type to extension(s)
foreach(c ${ACTIVE_TYPES})
  set(enum_str "HIDDEN const char *\nbu_file_mime_ext_${c}(int type)\n{")
  string(TOUPPER "${c}" c_u)
  set(enum_str "${enum_str}\n    const char *ret = NULL;")
  set(enum_str "${enum_str}\n    if (type < 0 || type >= MIME_${c_u}_UNKNOWN) { goto found_type; }\n")
  list(SORT ${c}_types)
  foreach(type ${${c}_types})
    set(ext_str "${${type}_extensions}")
    string(REPLACE ";" "," ext_str "${ext_str}")
    set(enum_str "${enum_str}\n    if(type == (int)MIME_${type}) { ret = bu_strdup(\"${ext_str}\"); goto found_type; }\n")
  endforeach(type ${${c}_types})
  set(enum_str "${enum_str}\n    found_type:\n")
  set(enum_str "${enum_str}\n    return ret;\n}\n")
  set(c_contents "${c_contents}\n${enum_str}")
endforeach(c ${ACTIVE_TYPES})

# Public C mapping function - type to string
set(mcstr "\nconst char *bu_file_mime_ext(int t, mime_context_t context)\n{")
set(mcstr "${mcstr}\n    switch (context) {\n")
foreach(context ${ACTIVE_TYPES})
  string(TOUPPER "${context}" c)
  set(mcstr "${mcstr}        case MIME_${c}:\n")
  set(mcstr "${mcstr}             return bu_file_mime_ext_${context}(t);\n")
  set(mcstr "${mcstr}             break;\n")
endforeach(context ${ACTIVE_TYPES})
set(mcstr "${mcstr}        default:\n")
set(mcstr "${mcstr}             return NULL;\n")
set(mcstr "${mcstr}             break;\n")
set(mcstr "${mcstr}    }\n")
set(mcstr "${mcstr}    return NULL;\n")
set(mcstr "${mcstr}}\n")
set(c_contents "${c_contents}\n${mcstr}")

file(WRITE ${mime_h_file_tmp} "${h_contents}")
file(WRITE ${mime_c_file_tmp} "${c_contents}")

execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${mime_h_file_tmp} ${MIME_H_FILE})
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${mime_c_file_tmp} ${MIME_C_FILE})

execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${mime_h_file_tmp})
execute_process(COMMAND ${CMAKE_COMMAND} -E remove ${mime_c_file_tmp})

DISTCLEAN(${MIME_H_FILE})
DISTCLEAN(${MIME_C_FILE})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

