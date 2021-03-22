cmake_minimum_required( VERSION 3.6.3 )

# executable is ${EXE}, input file is ${INFILE}

set( ofile "entity/SdaiAssembly_component.cc" )
execute_process( COMMAND ${EXE} ${INFILE}
  RESULT_VARIABLE CMD_RESULT )
if( NOT ${CMD_RESULT} EQUAL 0 )
  message(FATAL_ERROR "Error running ${EXE} on ${INFILE}")
endif( NOT ${CMD_RESULT} EQUAL 0 )

file( READ ${ofile} cxx LIMIT 10240 ) #9051 bytes now

# new Inverse_attribute("product_occurrence.occurrence_contexts"
string( REGEX MATCH "new *Inverse_attribute *\\\( *\\\" *product_occurrence *\\. *occurrence_contexts" match_result ${cxx} )

if( match_result STREQUAL "" )
  message( FATAL_ERROR "exp2cxx output does not match input schema." )
endif( match_result STREQUAL "" )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

