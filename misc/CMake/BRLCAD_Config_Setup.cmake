# We will need a brlcad_config.h.in file to hold all the #cmakedefine
# statements, which will in turn be used to generate a brlcad_conf.h
# file.  In autotools this process is handled by autoheader - in the
# case of CMake we wrap the CHECK_* functions and the creation of the
# entry in the brlcad_config.h.in file into one step via a macro.
#
# To avoid hitting the disk I/O any harder than necessary, we store
# the eventual contents of brlcad_config.h.in as a CMake string until
# it is ready to process.  A global property is needed to hold the
# contents, because subdirectories (in particular, src/other) may
# have content to contribute to the top level config.h.in file and
# the default local variable scope in subdirectories means those
# changes would not automatically propagate back up.
#
# We also allow for multiple projects with this macro, in case
# subprojects are also managing a config.h.in a file of their own.

set(CONFIG_H_FILE "${BRLCAD_BINARY_DIR}/include/brlcad_config.h.in")
set_source_files_properties(${CONFIG_H_FILE} PROPERTIES GENERATED TRUE)

set(CMAKE_CURRENT_PROJECT BRLCAD)

define_property(GLOBAL PROPERTY BRLCAD_CONFIG_H_CONTENTS BRIEF_DOCS "config.h.in contents" FULL_DOCS "config.h.in contents for BRL-CAD project")
if(NOT COMMAND CONFIG_H_APPEND)
  macro(CONFIG_H_APPEND PROJECT_NAME NEW_CONTENTS)
    if(PROJECT_NAME)
      get_property(${PROJECT_NAME}_CONFIG_H_CONTENTS GLOBAL PROPERTY ${PROJECT_NAME}_CONFIG_H_CONTENTS)
      set(${PROJECT_NAME}_CONFIG_H_FILE_CONTENTS "${${PROJECT_NAME}_CONFIG_H_CONTENTS}${NEW_CONTENTS}")
      set_property(GLOBAL PROPERTY ${PROJECT_NAME}_CONFIG_H_CONTENTS "${${PROJECT_NAME}_CONFIG_H_FILE_CONTENTS}")
    endif(PROJECT_NAME)
  endmacro(CONFIG_H_APPEND NEW_CONTENTS)
endif(NOT COMMAND CONFIG_H_APPEND)

CONFIG_H_APPEND(BRLCAD "/**** Define statements for CMake ****/\n")
CONFIG_H_APPEND(BRLCAD "#ifndef __CONFIG_H__\n")
CONFIG_H_APPEND(BRLCAD "#define __CONFIG_H__\n")

# Set up some of the define statements for path information and other basics
CONFIG_H_APPEND(BRLCAD "#define PACKAGE \"brlcad\"\n")
CONFIG_H_APPEND(BRLCAD "#define PACKAGE_BUGREPORT \"http://brlcad.org\"\n")
CONFIG_H_APPEND(BRLCAD "#define PACKAGE_NAME \"BRL-CAD\"\n")
CONFIG_H_APPEND(BRLCAD "#define PACKAGE_STRING \"BRL-CAD ${BRLCAD_VERSION}\"\n")
CONFIG_H_APPEND(BRLCAD "#define PACKAGE_TARNAME \"brlcad\"\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_DATA \"${CMAKE_INSTALL_PREFIX}/${DATA_DIR}\"\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_ROOT \"${CMAKE_INSTALL_PREFIX}\"\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_BIN_DIR \"${BIN_DIR}\"\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_LIB_DIR \"${LIB_DIR}\"\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_INCLUDE_DIR \"${INCLUDE_DIR}\"\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_DATA_DIR \"${DATA_DIR}\"\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_DOC_DIR \"${DOC_DIR}\"\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_MAN_DIR \"${MAN_DIR}\"\n")

# Write out our version information
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_VERSION_MAJOR ${BRLCAD_VERSION_MAJOR}\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_VERSION_MINOR ${BRLCAD_VERSION_MINOR}\n")
CONFIG_H_APPEND(BRLCAD "#define BRLCAD_VERSION_PATCH ${BRLCAD_VERSION_PATCH}\n")

# Let programs know what the executable suffix is on this platform, if any
CONFIG_H_APPEND(BRLCAD "#define EXECUTABLE_SUFFIX \"${CMAKE_EXECUTABLE_SUFFIX}\"\n")


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

