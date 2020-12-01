cmake_minimum_required( VERSION 3.6.3 )

# executable is ${EXE}, input file is ${INFILE}

# set( ofile "SdaiAll.cc" )
set(ofile "entity/SdaiConnection_zone_interface_plane_relationship.cc")
execute_process( COMMAND ${EXE} ${INFILE}
  RESULT_VARIABLE CMD_RESULT )
if( NOT ${CMD_RESULT} EQUAL 0 )
  message(FATAL_ERROR "Error running ${EXE} on ${INFILE}")
endif( NOT ${CMD_RESULT} EQUAL 0 )

file( READ ${ofile} cxx LIMIT 4096 )

# ur = new Uniqueness_rule("UR1 : SELF\shape_aspect_relationship.name;\n");
#entity/SdaiConnection_zone_interface_plane_relationship.cc:57:    str.append( "SELF\\shape_aspect_relationship.name\n" );

# string( REGEX MATCH "new  *Uniqueness_rule *\\\( *\\\" *[uU][rR]1 *: *SELF *\\\\ *shape_aspect_relationship *\\. *name" match_result ${cxx} )
string( REGEX MATCH "str.append *\\\( *\\\" *SELF *\\\\\\\\ *shape_aspect_relationship *\\. *name" match_result ${cxx} )

if( match_result STREQUAL "" )
  message( FATAL_ERROR "exp2cxx output does not match input schema." )
endif( match_result STREQUAL "" )

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

