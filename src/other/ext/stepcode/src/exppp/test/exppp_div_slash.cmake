cmake_minimum_required( VERSION 3.6.3 )

# executable is ${EXPPP}, input file is ${INFILE}

set( ofile "div_slash_out.exp" )
execute_process( COMMAND ${EXPPP} -o ${ofile} ${INFILE}
                RESULT_VARIABLE CMD_RESULT )
if( NOT ${CMD_RESULT} EQUAL 0 )
    message(FATAL_ERROR "Error running ${EXPPP} on ${INFILE}")
endif( NOT ${CMD_RESULT} EQUAL 0 )

# file( READ ${INFILE} pretty_in LIMIT 1024 )
file( READ ${ofile} pretty_out LIMIT 1024 )

#        VAR rmax_in : BOOLEAN
string( REGEX MATCH "result_i *: *INTEGER .*DIV" match_result ${pretty_out} )
#    r : REAL (should not have VAR)
string( REGEX MATCH "result_r *: *REAL .*/" match_result2 ${pretty_out} )

if( match_result STREQUAL "" )
    message( FATAL_ERROR "Pretty printer output does not match input - ." )
endif( match_result STREQUAL "" )
if( match_result2 STREQUAL "" )
    message( FATAL_ERROR "Pretty printer output does not match input - ." )
endif( match_result2 STREQUAL "" )


