cmake_minimum_required( VERSION 3.6.3 )

# executable is ${EXPPP}, input file is ${INFILE}

set( ofile "inverse_qual_out.exp" )
execute_process( COMMAND ${EXPPP} -o ${ofile} ${INFILE}
  RESULT_VARIABLE CMD_RESULT )
if( NOT ${CMD_RESULT} EQUAL 0 )
  message(FATAL_ERROR "Error running ${EXPPP} on ${INFILE}")
endif( NOT ${CMD_RESULT} EQUAL 0 )

# file( READ ${INFILE} pretty_in LIMIT 1024 )
file( READ ${ofile} pretty_out LIMIT 1024 )

#  SELF\product_occurrence.occurrence_contexts : SET [1 : ?] OF assembly_component_relationship FOR related_view;
# one backslash balloons into 4 x(
string( REGEX MATCH " *SELF *\\\\ *product_occurrence *\\. *occurrence_contexts *: *SET *\\[ *1 *:" match_result ${pretty_out} )

if( match_result STREQUAL "" )
  message( FATAL_ERROR "Pretty printer output does not match input." )
endif( match_result STREQUAL "" )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

