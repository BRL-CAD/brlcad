# Wrap the default message() function to also append ALL messages to a
# CMakeOutput.log file in addition to usual console printing.
# Note - only do this after calling project, since this override seems to do
# unexpected things to the messages returned by that command

function(message)

  # bleh, don't know a clean+safe way to avoid string comparing the
  # optional arg, so we extract it and test.
  list(GET ARGV 0 MessageType)

  if (MessageType STREQUAL FATAL_ERROR OR MessageType STREQUAL SEND_ERROR OR MessageType STREQUAL WARNING OR MessageType STREQUAL AUTHOR_WARNING OR MessageType STREQUAL STATUS)
    list(REMOVE_AT ARGV 0)
    _message(${MessageType} "${ARGV}")
    file(APPEND "${BRLCAD_BINARY_DIR}/CMakeFiles/CMakeOutput.log" "${MessageType}: ${ARGV}\n")
  else (MessageType STREQUAL FATAL_ERROR OR MessageType STREQUAL SEND_ERROR OR MessageType STREQUAL WARNING OR MessageType STREQUAL AUTHOR_WARNING OR MessageType STREQUAL STATUS)
    _message("${ARGV}")
    file(APPEND "${BRLCAD_BINARY_DIR}/CMakeFiles/CMakeOutput.log" "${ARGV}\n")
  endif (MessageType STREQUAL FATAL_ERROR OR MessageType STREQUAL SEND_ERROR OR MessageType STREQUAL WARNING OR MessageType STREQUAL AUTHOR_WARNING OR MessageType STREQUAL STATUS)

  # ~10% slower alternative that avoids adding '--' to STATUS messages
  # execute_process(COMMAND ${CMAKE_COMMAND} -E echo "${ARGV}")

endfunction(message)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

