cmake_minimum_required( VERSION 3.6.3 )

# executable is ${EXPPP}, input file is ${INFILE}

set( ofile "lost_var_out.exp" )
execute_process( COMMAND ${EXPPP} -o ${ofile} ${INFILE}
                RESULT_VARIABLE CMD_RESULT )
if( NOT ${CMD_RESULT} EQUAL 0 )
    message(FATAL_ERROR "Error running ${EXPPP} on ${INFILE}")
endif( NOT ${CMD_RESULT} EQUAL 0 )

# file( READ ${INFILE} pretty_in LIMIT 1024 )
file( READ ${ofile} pretty_out LIMIT 1024 )

#        VAR rmax_in : BOOLEAN
string( REGEX MATCH "VAR *rmax_in *: *BOOLEAN" match_result ${pretty_out} )
#    r : REAL (should not have VAR)
string( REGEX MATCH "VAR *r *: *REAL" match_result2 ${pretty_out} )

if( match_result STREQUAL "" )
    message( FATAL_ERROR "Pretty printer output does not match input - missing VAR." )
endif( match_result STREQUAL "" )
if( NOT match_result2 STREQUAL "" )
    message( FATAL_ERROR "Pretty printer output does not match input - extra VAR." )
endif( NOT match_result2 STREQUAL "" )
