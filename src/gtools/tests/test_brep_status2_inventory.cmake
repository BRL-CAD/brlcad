# Verify that the corpus-facing SurfaceTree audit records every status-2 face
# and that its sampled mapping observations remain evidence-only.  The native
# NURBS sphere is retained as a negative/status-1 control.

if(NOT DEFINED BREP_AUDIT OR NOT EXISTS "${BREP_AUDIT}")
  message(FATAL_ERROR "BREP_AUDIT is not an executable path: ${BREP_AUDIT}")
endif()
if(NOT DEFINED POSITIVE_DATABASE OR NOT EXISTS "${POSITIVE_DATABASE}")
  message(FATAL_ERROR "POSITIVE_DATABASE is unavailable: ${POSITIVE_DATABASE}")
endif()
if(NOT DEFINED NEGATIVE_DATABASE OR NOT EXISTS "${NEGATIVE_DATABASE}")
  message(FATAL_ERROR "NEGATIVE_DATABASE is unavailable: ${NEGATIVE_DATABASE}")
endif()

execute_process(
  COMMAND
    "${BREP_AUDIT}" --surface-trees-all --surface-tree-depth=0
    "${POSITIVE_DATABASE}"
  RESULT_VARIABLE positive_result
  OUTPUT_VARIABLE positive_output
  ERROR_VARIABLE positive_error
)
if(NOT positive_result EQUAL 0)
  message(
    FATAL_ERROR
      "status-2 inventory audit failed (${positive_result}): ${positive_error}"
  )
endif()

string(
  JSON object_count ERROR_VARIABLE json_error LENGTH "${positive_output}"
  objects
)
if(json_error)
  message(FATAL_ERROR "invalid status-2 inventory JSON: ${json_error}")
endif()
if(object_count LESS 1)
  message(FATAL_ERROR "status-2 inventory corpus contained no BREP objects")
endif()

string(
  JSON reported_status2 GET "${positive_output}" totals
  nurb_form_status_counts reparameterized
)
set(inventory_count 0)
set(arc_count 0)
set(line_count 0)
set(nurbs_count 0)
math(EXPR object_last "${object_count} - 1")
foreach(object_index RANGE 0 ${object_last})
  string(
    JSON object_inventory_count LENGTH "${positive_output}" objects
    ${object_index} status2_inventory
  )
  if(object_inventory_count EQUAL 0)
    continue()
  endif()
  math(EXPR inventory_last "${object_inventory_count} - 1")
  foreach(inventory_index RANGE 0 ${inventory_last})
    math(EXPR inventory_count "${inventory_count} + 1")
    set(entry_path objects ${object_index} status2_inventory ${inventory_index})
    string(JSON status GET "${positive_output}" ${entry_path} nurb_form_status)
    string(
      JSON surface_type GET "${positive_output}" ${entry_path}
      representation surface_type
    )
    string(
      JSON revolution GET "${positive_output}" ${entry_path}
      representation revolution present
    )
    string(
      JSON profile_type GET "${positive_output}" ${entry_path}
      representation revolution profile type
    )
    string(
      JSON profile_proxy GET "${positive_output}" ${entry_path}
      representation revolution profile proxy present
    )
    string(
      JSON profile_polycurve GET "${positive_output}" ${entry_path}
      representation revolution profile polycurve present
    )
    string(
      JSON evidence_only GET "${positive_output}" ${entry_path}
      parameter_mapping evidence_only
    )
    string(
      JSON grid_size GET "${positive_output}" ${entry_path}
      parameter_mapping grid_size
    )
    if(NOT status EQUAL 2 OR NOT surface_type STREQUAL "ON_RevSurface" OR
       NOT revolution OR profile_proxy OR profile_polycurve OR
       NOT evidence_only OR NOT grid_size EQUAL 9)
      message(
        FATAL_ERROR
          "invalid status-2 representation entry at object ${object_index}, face ${inventory_index}"
      )
    endif()

    if(profile_type STREQUAL "ON_ArcCurve")
      math(EXPR arc_count "${arc_count} + 1")
    elseif(profile_type STREQUAL "ON_LineCurve")
      math(EXPR line_count "${line_count} + 1")
    elseif(profile_type STREQUAL "ON_NurbsCurve")
      math(EXPR nurbs_count "${nurbs_count} + 1")
    else()
      message(FATAL_ERROR "unexpected status-2 profile type: ${profile_type}")
    endif()

    foreach(direction forward inverse)
      foreach(count_name successes domain_contained round_trip_successes
                         round_trip_within_tolerance)
        string(
          JSON observed GET "${positive_output}" ${entry_path}
          parameter_mapping ${direction} ${count_name}
        )
        if(NOT observed EQUAL 81)
          message(
            FATAL_ERROR
              "${direction} ${count_name}=${observed}, expected 81 at object ${object_index}, face ${inventory_index}"
          )
        endif()
      endforeach()
      foreach(boolean_name behavior_complete separable)
        string(
          JSON observed GET "${positive_output}" ${entry_path}
          parameter_mapping ${direction} ${boolean_name}
        )
        if(NOT observed)
          message(
            FATAL_ERROR
              "${direction} ${boolean_name} is false at object ${object_index}, face ${inventory_index}"
          )
        endif()
      endforeach()
      foreach(axis 0 1)
        string(
          JSON input_axis GET "${positive_output}" ${entry_path}
          parameter_mapping ${direction} output_input_axis ${axis}
        )
        string(
          JSON monotone GET "${positive_output}" ${entry_path}
          parameter_mapping ${direction} monotone ${axis}
        )
        string(
          JSON strictly_monotone GET "${positive_output}" ${entry_path}
          parameter_mapping ${direction} strictly_monotone ${axis}
        )
        string(
          JSON orientation GET "${positive_output}" ${entry_path}
          parameter_mapping ${direction} orientation ${axis}
        )
        if(NOT input_axis EQUAL axis OR NOT monotone OR
           NOT strictly_monotone OR NOT orientation STREQUAL "increasing")
          message(
            FATAL_ERROR
              "invalid ${direction} axis ${axis} behavior at object ${object_index}, face ${inventory_index}"
          )
        endif()
      endforeach()
    endforeach()
  endforeach()
endforeach()

if(NOT inventory_count EQUAL reported_status2)
  message(
    FATAL_ERROR
      "status-2 inventory count ${inventory_count} does not match total ${reported_status2}"
  )
endif()
if(NOT inventory_count EQUAL 32 OR NOT arc_count EQUAL 12 OR
   NOT line_count EQUAL 16 OR NOT nurbs_count EQUAL 4)
  message(
    FATAL_ERROR
      "unexpected qualified status-2 corpus: total=${inventory_count}, arc=${arc_count}, line=${line_count}, nurbs=${nurbs_count}"
  )
endif()

execute_process(
  COMMAND
    "${BREP_AUDIT}" --surface-trees-all --surface-tree-depth=0
    "${NEGATIVE_DATABASE}"
  RESULT_VARIABLE negative_result
  OUTPUT_VARIABLE negative_output
  ERROR_VARIABLE negative_error
)
if(NOT negative_result EQUAL 0)
  message(
    FATAL_ERROR
      "status-1 inventory control failed (${negative_result}): ${negative_error}"
  )
endif()
string(JSON negative_objects LENGTH "${negative_output}" objects)
string(
  JSON negative_status2 GET "${negative_output}" totals
  nurb_form_status_counts reparameterized
)
string(
  JSON negative_exact GET "${negative_output}" totals
  nurb_form_status_counts exact
)
if(NOT negative_objects EQUAL 1 OR NOT negative_status2 EQUAL 0 OR
   NOT negative_exact EQUAL 1)
  message(
    FATAL_ERROR
      "native-NURBS control changed: objects=${negative_objects}, exact=${negative_exact}, status2=${negative_status2}"
  )
endif()
string(
  JSON negative_inventory LENGTH "${negative_output}" objects 0
  status2_inventory
)
if(NOT negative_inventory EQUAL 0)
  message(FATAL_ERROR "status-1 control was incorrectly added to status-2 inventory")
endif()

message(
  STATUS
    "status-2 inventory PASS: ${inventory_count} faces (${arc_count} arc, ${line_count} line, ${nurbs_count} NURBS); status-1 control excluded"
)
