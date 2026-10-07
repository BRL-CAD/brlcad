# Build a reproducible, evidence-only inventory of reparameterized BREP
# surfaces.  Sampling results describe imported representations and never
# grant ray-intersection authority.

foreach(required BREP_AUDIT OUTPUT_DIR DATABASES)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()
if(NOT EXISTS "${BREP_AUDIT}")
  message(FATAL_ERROR "BREP_AUDIT does not exist: ${BREP_AUDIT}")
endif()
if(NOT DEFINED DEPTH)
  set(DEPTH 0)
endif()
if(EXISTS "${OUTPUT_DIR}")
  message(FATAL_ERROR "Refusing to overwrite existing inventory: ${OUTPUT_DIR}")
endif()
file(MAKE_DIRECTORY "${OUTPUT_DIR}/runs")

set(csv_file "${OUTPUT_DIR}/databases.csv")
set(jsonl_file "${OUTPUT_DIR}/inventory.jsonl")
set(stderr_file "${OUTPUT_DIR}/stderr.log")
file(
  WRITE "${csv_file}"
  "database,database_sha256,objects,faces,status2_faces,revolution_faces,arc_profiles,line_profiles,nurbs_profiles,other_profiles,face_reversed,face_proxy_transposed,surface_proxies,revolution_transposed,profile_proxies,polycurves,closed_u,closed_v,periodic_u,periodic_v,singular_south,singular_east,singular_north,singular_west,mapping_unsampled,mapping_incomplete,mapping_domain_failures,mapping_round_trip_failures,mapping_tolerance_failures,mapping_nonseparable,mapping_nonmonotone,mapping_nonstrict,run_sha256\n"
)
file(WRITE "${jsonl_file}" "")
file(WRITE "${stderr_file}" "")

set(total_databases 0)
foreach(name IN ITEMS objects faces status2_faces revolution_faces arc_profiles
    line_profiles nurbs_profiles other_profiles face_reversed
    face_proxy_transposed surface_proxies revolution_transposed
    profile_proxies polycurves closed_u closed_v periodic_u periodic_v
    singular_south singular_east singular_north singular_west
    mapping_unsampled mapping_incomplete mapping_domain_failures
    mapping_round_trip_failures mapping_tolerance_failures
    mapping_nonseparable mapping_nonmonotone mapping_nonstrict)
  set(total_${name} 0)
endforeach()
set(total_maximum_normalized_round_trip_error 0.0)

foreach(database IN LISTS DATABASES)
  if(NOT EXISTS "${database}")
    message(FATAL_ERROR "inventory database does not exist: ${database}")
  endif()
  get_filename_component(database_name "${database}" NAME_WE)
  set(run_file "${OUTPUT_DIR}/runs/${database_name}.json")
  execute_process(
    COMMAND
      "${BREP_AUDIT}" --surface-trees-all
      "--surface-tree-depth=${DEPTH}" "${database}"
    RESULT_VARIABLE audit_result
    OUTPUT_VARIABLE audit_output
    ERROR_VARIABLE audit_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  string(STRIP "${audit_output}" audit_output)
  file(APPEND "${stderr_file}" "[${database}]\n${audit_error}\n")
  if(NOT audit_result EQUAL 0)
    message(FATAL_ERROR "inventory failed for ${database}: ${audit_error}")
  endif()
  string(JSON object_count ERROR_VARIABLE json_error LENGTH "${audit_output}" objects)
  if(json_error)
    message(FATAL_ERROR "invalid inventory JSON for ${database}: ${json_error}")
  endif()
  string(JSON face_count GET "${audit_output}" totals faces)
  file(WRITE "${run_file}" "${audit_output}\n")
  file(APPEND "${jsonl_file}" "${audit_output}\n")

  foreach(name IN ITEMS status2_faces revolution_faces arc_profiles
      line_profiles nurbs_profiles other_profiles face_reversed
      face_proxy_transposed surface_proxies revolution_transposed
      profile_proxies polycurves closed_u closed_v periodic_u periodic_v
      singular_south singular_east singular_north singular_west
      mapping_unsampled mapping_incomplete mapping_domain_failures
      mapping_round_trip_failures mapping_tolerance_failures
      mapping_nonseparable mapping_nonmonotone mapping_nonstrict)
    set(database_${name} 0)
  endforeach()

  if(object_count GREATER 0)
    math(EXPR object_last "${object_count} - 1")
    foreach(object_index RANGE 0 ${object_last})
      string(JSON object_json GET "${audit_output}" objects ${object_index})
      string(
        JSON inventory_count LENGTH "${object_json}" status2_inventory
      )
      if(inventory_count EQUAL 0)
        continue()
      endif()
      math(EXPR inventory_last "${inventory_count} - 1")
      foreach(inventory_index RANGE 0 ${inventory_last})
        math(EXPR database_status2_faces "${database_status2_faces} + 1")
        string(
          JSON entry_json GET "${object_json}" status2_inventory
          ${inventory_index}
        )
        string(JSON value GET "${entry_json}" representation face_reversed)
        if(value)
          math(EXPR database_face_reversed "${database_face_reversed} + 1")
        endif()
        string(
          JSON value GET "${entry_json}" representation
          face_proxy_transposed
        )
        if(value)
          math(EXPR database_face_proxy_transposed "${database_face_proxy_transposed} + 1")
        endif()
        string(
          JSON value GET "${entry_json}" representation
          surface_proxy present
        )
        if(value)
          math(EXPR database_surface_proxies "${database_surface_proxies} + 1")
        endif()
        string(
          JSON revolution GET "${entry_json}" representation
          revolution present
        )
        if(revolution)
          math(EXPR database_revolution_faces "${database_revolution_faces} + 1")
        endif()
        string(
          JSON value GET "${entry_json}" representation revolution
          transposed
        )
        if(value)
          math(EXPR database_revolution_transposed "${database_revolution_transposed} + 1")
        endif()
        string(
          JSON value GET "${entry_json}" representation revolution
          profile proxy present
        )
        if(value)
          math(EXPR database_profile_proxies "${database_profile_proxies} + 1")
        endif()
        string(
          JSON value GET "${entry_json}" representation revolution
          profile polycurve present
        )
        if(value)
          math(EXPR database_polycurves "${database_polycurves} + 1")
        endif()
        string(
          JSON profile_type GET "${entry_json}" representation
          revolution profile type
        )
        if(profile_type STREQUAL "ON_ArcCurve")
          math(EXPR database_arc_profiles "${database_arc_profiles} + 1")
        elseif(profile_type STREQUAL "ON_LineCurve")
          math(EXPR database_line_profiles "${database_line_profiles} + 1")
        elseif(profile_type STREQUAL "ON_NurbsCurve")
          math(EXPR database_nurbs_profiles "${database_nurbs_profiles} + 1")
        else()
          math(EXPR database_other_profiles "${database_other_profiles} + 1")
        endif()
        foreach(direction 0 1)
          string(
            JSON value GET "${entry_json}" representation closed
            ${direction}
          )
          if(value)
            if(direction EQUAL 0)
              math(EXPR database_closed_u "${database_closed_u} + 1")
            else()
              math(EXPR database_closed_v "${database_closed_v} + 1")
            endif()
          endif()
          string(
            JSON value GET "${entry_json}" representation periodic
            ${direction}
          )
          if(value)
            if(direction EQUAL 0)
              math(EXPR database_periodic_u "${database_periodic_u} + 1")
            else()
              math(EXPR database_periodic_v "${database_periodic_v} + 1")
            endif()
          endif()
        endforeach()
        foreach(side 0 1 2 3)
          string(
            JSON value GET "${entry_json}" representation
            singular_sides ${side}
          )
          if(value)
            if(side EQUAL 0)
              math(EXPR database_singular_south "${database_singular_south} + 1")
            elseif(side EQUAL 1)
              math(EXPR database_singular_east "${database_singular_east} + 1")
            elseif(side EQUAL 2)
              math(EXPR database_singular_north "${database_singular_north} + 1")
            else()
              math(EXPR database_singular_west "${database_singular_west} + 1")
            endif()
          endif()
        endforeach()

        string(JSON sampled GET "${entry_json}" parameter_mapping sampled)
        if(NOT sampled)
          math(EXPR database_mapping_unsampled "${database_mapping_unsampled} + 1")
        endif()
        set(face_incomplete FALSE)
        set(face_domain_failure FALSE)
        set(face_round_trip_failure FALSE)
        set(face_tolerance_failure FALSE)
        set(face_nonseparable FALSE)
        set(face_nonmonotone FALSE)
        set(face_nonstrict FALSE)
        foreach(direction forward inverse)
          string(
            JSON attempts GET "${entry_json}" parameter_mapping
            ${direction} attempts
          )
          foreach(count successes domain_contained round_trip_successes
              round_trip_within_tolerance)
            string(
              JSON ${count} GET "${entry_json}" parameter_mapping
              ${direction} ${count}
            )
          endforeach()
          if(NOT successes EQUAL attempts)
            set(face_incomplete TRUE)
          endif()
          if(NOT domain_contained EQUAL successes)
            set(face_domain_failure TRUE)
          endif()
          if(NOT round_trip_successes EQUAL successes)
            set(face_round_trip_failure TRUE)
          endif()
          if(NOT round_trip_within_tolerance EQUAL round_trip_successes)
            set(face_tolerance_failure TRUE)
          endif()
          string(
            JSON behavior_complete GET "${entry_json}"
            parameter_mapping ${direction} behavior_complete
          )
          if(NOT behavior_complete)
            set(face_incomplete TRUE)
          endif()
          string(
            JSON separable GET "${entry_json}" parameter_mapping
            ${direction} separable
          )
          if(NOT separable)
            set(face_nonseparable TRUE)
          endif()
          foreach(axis 0 1)
            string(
              JSON monotone GET "${entry_json}" parameter_mapping
              ${direction} monotone ${axis}
            )
            if(NOT monotone)
              set(face_nonmonotone TRUE)
            endif()
            string(
              JSON strictly_monotone GET "${entry_json}" parameter_mapping
              ${direction} strictly_monotone ${axis}
            )
            if(NOT strictly_monotone)
              set(face_nonstrict TRUE)
            endif()
            string(
              JSON normalized_error GET "${entry_json}" parameter_mapping
              ${direction} maximum_normalized_round_trip_error ${axis}
            )
            if(normalized_error GREATER total_maximum_normalized_round_trip_error)
              set(total_maximum_normalized_round_trip_error ${normalized_error})
            endif()
          endforeach()
        endforeach()
        if(face_incomplete)
          math(EXPR database_mapping_incomplete "${database_mapping_incomplete} + 1")
        endif()
        if(face_domain_failure)
          math(EXPR database_mapping_domain_failures "${database_mapping_domain_failures} + 1")
        endif()
        if(face_round_trip_failure)
          math(EXPR database_mapping_round_trip_failures "${database_mapping_round_trip_failures} + 1")
        endif()
        if(face_tolerance_failure)
          math(EXPR database_mapping_tolerance_failures "${database_mapping_tolerance_failures} + 1")
        endif()
        if(face_nonseparable)
          math(EXPR database_mapping_nonseparable "${database_mapping_nonseparable} + 1")
        endif()
        if(face_nonmonotone)
          math(EXPR database_mapping_nonmonotone "${database_mapping_nonmonotone} + 1")
        endif()
        if(face_nonstrict)
          math(EXPR database_mapping_nonstrict "${database_mapping_nonstrict} + 1")
        endif()
      endforeach()
    endforeach()
  endif()

  file(SHA256 "${database}" database_sha256)
  file(SHA256 "${run_file}" run_sha256)
  file(
    APPEND "${csv_file}"
    "\"${database}\",${database_sha256},${object_count},${face_count},${database_status2_faces},${database_revolution_faces},${database_arc_profiles},${database_line_profiles},${database_nurbs_profiles},${database_other_profiles},${database_face_reversed},${database_face_proxy_transposed},${database_surface_proxies},${database_revolution_transposed},${database_profile_proxies},${database_polycurves},${database_closed_u},${database_closed_v},${database_periodic_u},${database_periodic_v},${database_singular_south},${database_singular_east},${database_singular_north},${database_singular_west},${database_mapping_unsampled},${database_mapping_incomplete},${database_mapping_domain_failures},${database_mapping_round_trip_failures},${database_mapping_tolerance_failures},${database_mapping_nonseparable},${database_mapping_nonmonotone},${database_mapping_nonstrict},${run_sha256}\n"
  )
  math(EXPR total_databases "${total_databases} + 1")
  math(EXPR total_objects "${total_objects} + ${object_count}")
  math(EXPR total_faces "${total_faces} + ${face_count}")
  foreach(name IN ITEMS status2_faces revolution_faces arc_profiles
      line_profiles nurbs_profiles other_profiles face_reversed
      face_proxy_transposed surface_proxies revolution_transposed
      profile_proxies polycurves closed_u closed_v periodic_u periodic_v
      singular_south singular_east singular_north singular_west
      mapping_unsampled mapping_incomplete mapping_domain_failures
      mapping_round_trip_failures mapping_tolerance_failures
      mapping_nonseparable mapping_nonmonotone mapping_nonstrict)
    math(EXPR total_${name} "${total_${name}} + ${database_${name}}")
  endforeach()
endforeach()

file(SHA256 "${BREP_AUDIT}" audit_sha256)
file(SHA256 "${CMAKE_CURRENT_LIST_FILE}" driver_sha256)
file(SHA256 "${csv_file}" csv_sha256)
file(SHA256 "${jsonl_file}" jsonl_sha256)
file(SHA256 "${stderr_file}" stderr_sha256)
string(TIMESTAMP timestamp_utc "%Y-%m-%dT%H:%M:%SZ" UTC)
file(
  WRITE "${OUTPUT_DIR}/manifest.txt"
  "BREP status-2 representation inventory\n"
  "format=brlcad-brep-status2-inventory-v1\n"
  "timestamp_utc=${timestamp_utc}\n"
  "driver=${CMAKE_CURRENT_LIST_FILE}\n"
  "driver_sha256=${driver_sha256}\n"
  "brep_audit=${BREP_AUDIT}\n"
  "brep_audit_sha256=${audit_sha256}\n"
  "surface_tree_depth=${DEPTH}\n"
  "sampling=evidence-only deterministic 9x9 forward/inverse grids\n"
  "database_count=${total_databases}\n"
  "object_count=${total_objects}\n"
  "face_count=${total_faces}\n"
  "status2_face_count=${total_status2_faces}\n"
  "status2_revolution_faces=${total_revolution_faces}\n"
  "status2_arc_profiles=${total_arc_profiles}\n"
  "status2_line_profiles=${total_line_profiles}\n"
  "status2_nurbs_profiles=${total_nurbs_profiles}\n"
  "status2_other_profiles=${total_other_profiles}\n"
  "status2_face_reversed=${total_face_reversed}\n"
  "status2_face_proxy_transposed=${total_face_proxy_transposed}\n"
  "status2_surface_proxies=${total_surface_proxies}\n"
  "status2_revolution_transposed=${total_revolution_transposed}\n"
  "status2_profile_proxies=${total_profile_proxies}\n"
  "status2_polycurves=${total_polycurves}\n"
  "status2_closed_u=${total_closed_u}\n"
  "status2_closed_v=${total_closed_v}\n"
  "status2_periodic_u=${total_periodic_u}\n"
  "status2_periodic_v=${total_periodic_v}\n"
  "status2_singular_sides=${total_singular_south},${total_singular_east},${total_singular_north},${total_singular_west}\n"
  "mapping_unsampled_faces=${total_mapping_unsampled}\n"
  "mapping_incomplete_faces=${total_mapping_incomplete}\n"
  "mapping_domain_failure_faces=${total_mapping_domain_failures}\n"
  "mapping_round_trip_failure_faces=${total_mapping_round_trip_failures}\n"
  "mapping_tolerance_failure_faces=${total_mapping_tolerance_failures}\n"
  "mapping_nonseparable_faces=${total_mapping_nonseparable}\n"
  "mapping_nonmonotone_faces=${total_mapping_nonmonotone}\n"
  "mapping_nonstrict_faces=${total_mapping_nonstrict}\n"
  "maximum_normalized_round_trip_error=${total_maximum_normalized_round_trip_error}\n"
  "database_csv_sha256=${csv_sha256}\n"
  "inventory_jsonl_sha256=${jsonl_sha256}\n"
  "stderr_sha256=${stderr_sha256}\n"
)
message(
  STATUS
    "status-2 inventory complete: ${total_status2_faces}/${total_faces} faces in ${total_databases} databases; archive=${OUTPUT_DIR}"
)
