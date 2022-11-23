# The Express parser uses the tools RE2C and Lemon to generate code
# from higher level inputs.  Depending on available tools and options, the
# SC build can either re-generate code as part of the build, or use cached
# files that are ready for compilation.
#
# SC_GENERATE_LEXER_PARSER is the "high level" control a user sets to determine
# how the SC build will interact (or not) with these tools.  AUTO (the
# default) means it will search for the necessary tools, and use them only if
# everything is found.  If not, it will fall back to the cached versions.  If
# this option is set to ON and the necessary tools are not found, the
# configure step will fail.  If it is set to OFF, SC will not even try to use
# the generators and will instead use the cached sources.

set(_valid_gen_states AUTO ON OFF)
set(_verbosity "QUIET")

set(SC_GENERATE_LEXER_PARSER "AUTO" CACHE
    STRING "Use Perplex, RE2C and Lemon to generate C source code.")
set_property(CACHE SC_GENERATE_LEXER_PARSER PROPERTY STRINGS ${_valid_gen_states})
string(TOUPPER "${SC_GENERATE_LEXER_PARSER}" SC_GENERATE_LEXER_PARSER)

if(NOT "${SC_GENERATE_LEXER_PARSER}" IN_LIST _valid_gen_states)
  message(WARNING "Unknown value ${SC_GENERATE_LEXER_PARSER} supplied for SC_GENERATE_LEXER_PARSER - defaulting to AUTO")
  message(WARNING "Valid options are AUTO, ON and OFF")
  set(SC_GENERATE_LEXER_PARSER "AUTO" CACHE STRING "Use Perplex, RE2C and Lemon to generate C source code.")
endif()

# If the generators have not been turned off, we need to check for them
if(NOT "${SC_GENERATE_LEXER_PARSER}" STREQUAL "OFF")
  # NOTE: lemon doesn't have a stable versioning system (it's always 1)
  find_package(LEMON ${_verbosity})
  find_package(RE2C ${_verbosity})
  find_package(PERPLEX ${_verbosity})

  if(LEMON_FOUND AND RE2C_FOUND)
    set(SC_GENERATE_LP_SOURCES 1)
    message(".. Found re2c, and lemon - can regenerate lexer/parser if necessary")
  else()
    if("${SC_GENERATE_LEXER_PARSER}" STREQUAL "ON")
      message(FATAL_ERROR "\nSC_GENERATE_LEXER_PARSER set to ON, but couldn't find lemon/re2c")
    else("${SC_GENERATE_LEXER_PARSER}" STREQUAL "ON")
      set(SC_GENERATE_LP_SOURCES 0)
    endif("${SC_GENERATE_LEXER_PARSER}" STREQUAL "ON")
  endif()
else(NOT "${SC_GENERATE_LEXER_PARSER}" STREQUAL "OFF")
  set(SC_GENERATE_LP_SOURCES 0)
endif(NOT "${SC_GENERATE_LEXER_PARSER}" STREQUAL "OFF")

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

