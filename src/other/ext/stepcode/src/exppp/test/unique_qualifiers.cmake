cmake_minimum_required( VERSION 3.6.3 )

# executable is ${EXPPP}, input file is ${INFILE}

set( ofile "unique_qual_out.exp" )
execute_process( COMMAND ${EXPPP} -o ${ofile} ${INFILE}
  RESULT_VARIABLE CMD_RESULT )
if( NOT ${CMD_RESULT} EQUAL 0 )
  message(FATAL_ERROR "Error running ${EXPPP} on ${INFILE}")
endif( NOT ${CMD_RESULT} EQUAL 0 )

# file( READ ${INFILE} pretty_in LIMIT 1024 )
file( READ ${ofile} pretty_out LIMIT 1024 )

#      ur1 : SELF\shape_aspect_relationship.name;
# one backslash balloons into 4, and I can't figure out how to escape the semicolon x(
string( REGEX MATCH "[uU][rR]1 *: *SELF *\\\\ *shape_aspect_relationship *\\. *name" match_result ${pretty_out} )

if( match_result STREQUAL "" )
  message( FATAL_ERROR "Pretty printer output does not match input." )
endif( match_result STREQUAL "" )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

