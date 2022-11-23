 
cmake_minimum_required( VERSION 2.8 )

# executable is ${EXPPP}, input file is ${INFILE}

set( ofile "exppp_supertype_andor_out.exp" )
execute_process( COMMAND ${EXPPP} -o ${ofile} ${INFILE}
  RESULT_VARIABLE CMD_RESULT )
if( NOT ${CMD_RESULT} EQUAL 0 )
  message(FATAL_ERROR "Error running ${EXPPP} on ${INFILE}")
endif( NOT ${CMD_RESULT} EQUAL 0 )

# file( READ ${INFILE} pretty_in LIMIT 1024 )
file( READ ${ofile} pretty_out LIMIT 1024 )

# need to be 3 parens after path
string(FIND "${pretty_out}" "path ) ) )" match_result )
if( match_result LESS 1 )
  message( FATAL_ERROR "Pretty printer output does not match input." )
endif( match_result LESS 1 )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

