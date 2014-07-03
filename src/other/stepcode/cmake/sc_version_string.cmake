#use git for a pretty commit id
#uses 'git describe --tags', so tags are required in the repo
#create a tag with 'git tag <name>' and 'git push --tags'

# http://stackoverflow.com/questions/3780667
# http://www.cmake.org/pipermail/cmake/2009-February/027014.html

#sc_version_string.h defines sc_version() which returns a pretty commit description and a build timestamp.

set(SC_IS_SUBBUILD "@SC_IS_SUBBUILD@")

set(VERS_FILE ${SOURCE_DIR}/SC_VERSION.txt )
if( EXISTS ${SOURCE_DIR}/.git )
    find_package(Git QUIET)
    if(GIT_FOUND)
        execute_process(COMMAND ${GIT_EXECUTABLE} describe --tags RESULT_VARIABLE res_var OUTPUT_VARIABLE GIT_COMMIT_ID )
        if( NOT ${res_var} EQUAL 0 )
            file( READ ${VERS_FILE} GIT_COMMIT_ID LIMIT 255 )
	    if(NOT SC_IS_SUBBUILD)
              message( WARNING "Git failed (probably no tags in repo). Build will contain revision info from ${VERS_FILE}." )
	    endif(NOT SC_IS_SUBBUILD)
        endif()
    else(GIT_FOUND)
        file( READ ${VERS_FILE} GIT_COMMIT_ID LIMIT 255 )
	if(NOT SC_IS_SUBBUILD)
          message( WARNING "Git not found. Build will contain revision info from ${VERS_FILE}." )
	endif(NOT SC_IS_SUBBUILD)
    endif(GIT_FOUND)
else()
    file( READ ${VERS_FILE} GIT_COMMIT_ID LIMIT 255 )
    if(NOT SC_IS_SUBBUILD)
      message( WARNING "Git failed ('.git' not found). Build will contain revision info from ${VERS_FILE}." )
    endif(NOT SC_IS_SUBBUILD)
endif()

string( REPLACE "\n" "" GIT_COMMIT_ID ${GIT_COMMIT_ID} )

set( vstring "//sc_version_string.h - written by cmake. Changes will be lost!\n"
             "#ifndef SC_VERSION_STRING\n"
             "#define SC_VERSION_STRING\n\n"
             "/*\n** Returns a string like \"test-1-g5e1fb47, built at TIME on DATE\", where test is the\n"
             "** name of the last tagged git revision, 1 is the number of commits since that tag,\n"
             "** 'g' is unknown, 5e1fb47 is the first 7 chars of the git sha1 commit id, and TIME\n"
             "** and DATE are substituted by the compiler.\n*/\n\n"
             "const char* sc_version() {\n"
             "    return \"git commit id ${GIT_COMMIT_ID}, built at \" __TIME__ \" on \" __DATE__ \;\n"
             "}\n\n"
             "#endif\n"
   )

file(WRITE sc_version_string.h.txt ${vstring} )
# copy the file to the final header only if the version changes
# reduces needless rebuilds
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        sc_version_string.h.txt ${BINARY_DIR}/include/sc_version_string.h)
if(NOT SC_IS_SUBBUILD)
  message("-- sc_version_string.h is up-to-date.")
endif(NOT SC_IS_SUBBUILD)
