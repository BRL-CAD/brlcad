if(NOT DEFINED MKBUILDING_EXECUTABLE OR NOT DEFINED NIRT_EXECUTABLE OR NOT DEFINED OUTPUT_DIRECTORY)
  message(FATAL_ERROR "MKBUILDING_EXECUTABLE, NIRT_EXECUTABLE, and OUTPUT_DIRECTORY are required")
endif()

set(RAY_ORIGIN_Z_MM 50000)

function(check_roof_bearing building support x y label)
  set(database "${OUTPUT_DIRECTORY}/mkbuilding-roof-support-${building}.g")
  execute_process(
    COMMAND "${MKBUILDING_EXECUTABLE}" --force --preset "${building}" -o "${database}"
    RESULT_VARIABLE generator_result
    OUTPUT_VARIABLE generator_output
    ERROR_VARIABLE generator_error
    )
  if(NOT generator_result EQUAL 0)
    message(FATAL_ERROR
      "mkbuilding failed for ${building} (${generator_result}):\n${generator_output}\n${generator_error}"
      )
  endif()

  execute_process(
    COMMAND "${NIRT_EXECUTABLE}" -s -f csv-gap
      -e "xyz ${x} ${y} ${RAY_ORIGIN_Z_MM}; dir 0 0 -1; s; q"
      "${database}" "${building}"
    RESULT_VARIABLE nirt_result
    OUTPUT_VARIABLE nirt_output
    ERROR_VARIABLE nirt_error
    )
  if(NOT nirt_result EQUAL 0)
    message(FATAL_ERROR
      "nirt failed for ${building} ${label} (${nirt_result}):\n${nirt_output}\n${nirt_error}"
      )
  endif()

  string(REGEX MATCH
    "\"${building}_roof\\.r\"[^\n]*\n\"${building}_${support}\\.r\""
    bearing_hit "${nirt_output}"
    )
  if(NOT bearing_hit)
    message(FATAL_ERROR
      "Roof does not bear directly on ${support} for ${building} ${label}:\n${nirt_output}"
      )
  endif()
endfunction()

# Rays use millimeters and avoid automatically generated openings.  Open-frame
# presets are checked at both ends of the roof slope.
set(RECTANGULAR_WALL_X_MM 500)
set(RECTANGULAR_WALL_Y_MM 100)
set(STADIUM_SPRINGING_X_MM 55800)
set(STADIUM_CENTER_Y_MM 28000)
set(PARKING_COLUMN_COORDINATE_MM 277)
set(GRANDSTAND_LOW_COORDINATE_MM 177)
set(GRANDSTAND_HIGH_Y_MM 23823)
set(CARPORT_LOW_COORDINATE_MM 92)
set(CARPORT_HIGH_Y_MM 5908)

check_roof_bearing(museum walls ${RECTANGULAR_WALL_X_MM} ${RECTANGULAR_WALL_Y_MM} "wall plate")
check_roof_bearing(stadium walls ${STADIUM_SPRINGING_X_MM} ${STADIUM_CENTER_Y_MM} "dome springing")
check_roof_bearing(bunker walls ${RECTANGULAR_WALL_X_MM} ${RECTANGULAR_WALL_Y_MM} "wall plate")
check_roof_bearing(parking structure ${PARKING_COLUMN_COORDINATE_MM} ${PARKING_COLUMN_COORDINATE_MM} "top column")
check_roof_bearing(grandstand structure ${GRANDSTAND_LOW_COORDINATE_MM} ${GRANDSTAND_LOW_COORDINATE_MM} "low column")
check_roof_bearing(grandstand structure ${GRANDSTAND_LOW_COORDINATE_MM} ${GRANDSTAND_HIGH_Y_MM} "high column")
check_roof_bearing(carport structure ${CARPORT_LOW_COORDINATE_MM} ${CARPORT_LOW_COORDINATE_MM} "low post")
check_roof_bearing(carport structure ${CARPORT_LOW_COORDINATE_MM} ${CARPORT_HIGH_Y_MM} "high post")
