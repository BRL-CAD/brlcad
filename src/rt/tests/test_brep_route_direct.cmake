# Verify the opt-in direct and selective-publication BREP router variants
# preserve byte-exact legacy output on the 63-face negative control.

foreach(required_variable RT DB TEST_DIR)
  if(NOT DEFINED ${required_variable} OR "${${required_variable}}" STREQUAL "")
    message(FATAL_ERROR "${required_variable} is required")
  endif()
endforeach()

set(test_object recursive_child_linkage_complex_subtractions.s)
set(direct_image "${TEST_DIR}/brep-route-direct.pix")
set(publish_image "${TEST_DIR}/brep-route-publish.pix")
set(probe_image "${TEST_DIR}/brep-route-missing-root-probe.pix")
set(certificate_image "${TEST_DIR}/brep-route-missing-root-certificate.pix")
set(production_image "${TEST_DIR}/brep-route-production.pix")
set(legacy_image "${TEST_DIR}/brep-route-legacy.pix")
file(REMOVE "${direct_image}" "${publish_image}" "${probe_image}"
  "${certificate_image}" "${production_image}" "${legacy_image}")

execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E env
    "LIBRT_CACHE=off"
    "LIBRT_BREP_SOLVER="
    "LIBRT_BREP_ROUTE_DIRECT=1"
    "LIBRT_BREP_ROUTE_PUBLISH="
    "LIBRT_BREP_ROUTE_MISSING_ROOT_CERTIFICATE="
    "LIBRT_BREP_ROUTE_PROFILE=1"
    "LIBRT_BREP_PROFILE="
    "${RT}" -B -P 1 -s 64 -a 35 -e 25 -o "${direct_image}"
    "${DB}" "${test_object}"
  RESULT_VARIABLE direct_result
  OUTPUT_VARIABLE direct_stdout
  ERROR_VARIABLE direct_stderr
)
set(direct_log "${direct_stdout}${direct_stderr}")
if(NOT direct_result EQUAL 0 OR NOT EXISTS "${direct_image}")
  message(FATAL_ERROR
    "Direct routed render failed (${direct_result})\n${direct_log}"
  )
endif()
if(NOT direct_log MATCHES
    "BREP route profile: base=legacy authority=legacy shots=[1-9][0-9]* routed=[1-9][0-9]* unrouted=0 direct-transactions=[1-9][0-9]*")
  message(FATAL_ERROR "Direct route telemetry is missing or invalid\n${direct_log}")
endif()
if(NOT direct_log MATCHES "shadow-ms=0\\.000")
  message(FATAL_ERROR "Direct route unexpectedly ran a shadow transaction\n${direct_log}")
endif()
if(NOT direct_log MATCHES "optional-attempts=0 optional-complete=0")
  message(FATAL_ERROR
    "Direct route regressed to optional work on the 63-face negative control\n${direct_log}"
  )
endif()
if(NOT direct_log MATCHES
    "BREP route lane: name=complex-prepared rays=0 fraction=0\\.000000")
  message(FATAL_ERROR
    "Direct route selected the complex lane on the 63-face negative control\n${direct_log}"
  )
endif()

execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E env
    "LIBRT_CACHE=off"
    "LIBRT_BREP_SOLVER="
    "LIBRT_BREP_ROUTE_DIRECT=1"
    "LIBRT_BREP_ROUTE_PUBLISH=1"
    "LIBRT_BREP_ROUTE_MISSING_ROOT_CERTIFICATE="
    "LIBRT_BREP_ROUTE_PROFILE=1"
    "LIBRT_BREP_PROFILE="
    "${RT}" -B -P 1 -s 64 -a 35 -e 25 -o "${publish_image}"
    "${DB}" "${test_object}"
  RESULT_VARIABLE publish_result
  OUTPUT_VARIABLE publish_stdout
  ERROR_VARIABLE publish_stderr
)
set(publish_log "${publish_stdout}${publish_stderr}")
if(NOT publish_result EQUAL 0 OR NOT EXISTS "${publish_image}")
  message(FATAL_ERROR
    "Published routed render failed (${publish_result})\n${publish_log}"
  )
endif()
if(NOT publish_log MATCHES
    "BREP route profile: base=legacy authority=legacy shots=[1-9][0-9]* routed=[1-9][0-9]* unrouted=0 direct-transactions=[1-9][0-9]*.*optional-attempts=0 optional-complete=0 optional-published=0")
  message(FATAL_ERROR
    "Published route regressed to optional work on the 63-face negative control\n${publish_log}"
  )
endif()

# The possible-missing-root hull probe is a separately opt-in evidence path.
# It must observe the feature-heavy empty population without requesting or
# publishing optional work, and its output must remain the retained legacy
# transaction byte for byte.
execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E env
    "LIBRT_CACHE=off"
    "LIBRT_BREP_SOLVER="
    "LIBRT_BREP_ROUTE_DIRECT=1"
    "LIBRT_BREP_ROUTE_PUBLISH=1"
    "LIBRT_BREP_ROUTE_PROFILE=1"
    "LIBRT_BREP_ROUTE_MISSING_ROOT_PROBE=1"
    "LIBRT_BREP_ROUTE_MISSING_ROOT_CERTIFICATE="
    "LIBRT_BREP_PROFILE="
    "${RT}" -B -P 1 -s 64 -a 35 -e 25 -o "${probe_image}"
    "${DB}" "${test_object}"
  RESULT_VARIABLE probe_result
  OUTPUT_VARIABLE probe_stdout
  ERROR_VARIABLE probe_stderr
)
set(probe_log "${probe_stdout}${probe_stderr}")
if(NOT probe_result EQUAL 0 OR NOT EXISTS "${probe_image}")
  message(FATAL_ERROR
    "Missing-root probe render failed (${probe_result})\n${probe_log}"
  )
endif()
if(NOT probe_log MATCHES
    "optional-attempts=0 optional-complete=0 optional-published=0")
  message(FATAL_ERROR
    "Missing-root evidence probe changed optional routing\n${probe_log}"
  )
endif()
if(NOT probe_log MATCHES "missing-root-signal-rays=[1-9][0-9]*")
  message(FATAL_ERROR
    "Missing-root evidence probe observed no candidate rays\n${probe_log}"
  )
endif()
if(NOT publish_log MATCHES
    "BREP route lane: name=complex-prepared rays=0 fraction=0\\.000000.*published=0")
  message(FATAL_ERROR
    "Published route selected or published the complex lane on the 63-face negative control\n${publish_log}"
  )
endif()

# The bounded cell/span certificate is independently opt-in.  This negative
# control must completely prove its selected empty correction cells, request
# no optional transaction, and preserve the retained legacy image.
execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E env
    "LIBRT_CACHE=off"
    "LIBRT_BREP_SOLVER="
    "LIBRT_BREP_ROUTE_DIRECT=1"
    "LIBRT_BREP_ROUTE_PUBLISH=1"
    "LIBRT_BREP_ROUTE_PROFILE=1"
    "LIBRT_BREP_ROUTE_MISSING_ROOT_PROBE="
    "LIBRT_BREP_ROUTE_MISSING_ROOT_CERTIFICATE=1"
    "LIBRT_BREP_PROFILE="
    "${RT}" -B -P 1 -s 64 -a 35 -e 25 -o "${certificate_image}"
    "${DB}" "${test_object}"
  RESULT_VARIABLE certificate_result
  OUTPUT_VARIABLE certificate_stdout
  ERROR_VARIABLE certificate_stderr
)
set(certificate_log "${certificate_stdout}${certificate_stderr}")
if(NOT certificate_result EQUAL 0 OR NOT EXISTS "${certificate_image}")
  message(FATAL_ERROR
    "Missing-root certificate render failed (${certificate_result})\n${certificate_log}"
  )
endif()
if(NOT certificate_log MATCHES
    "BREP route missing-root certificate: associations=[1-9][0-9]* incomplete=0 root-free=[1-9][0-9]* root-positive=0.*complete-rays=[1-9][0-9]* triggered-rays=0")
  message(FATAL_ERROR
    "Missing-root certificate was not complete on the negative control\n${certificate_log}"
  )
endif()
if(NOT certificate_log MATCHES
    "optional-attempts=0 optional-complete=0 optional-published=0")
  message(FATAL_ERROR
    "Root-free certificate unexpectedly requested optional work\n${certificate_log}"
  )
endif()

# Exercise the actual unprofiled production path separately.  Profiling keeps
# the full classifier active for diagnostic populations, while production may
# reject a nonempty stream before the second leaf/root pass when no retained
# root is a numerical trim contact.
execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E env
    "LIBRT_CACHE=off"
    "LIBRT_BREP_SOLVER="
    "LIBRT_BREP_ROUTE_DIRECT=1"
    "LIBRT_BREP_ROUTE_PUBLISH=1"
    "LIBRT_BREP_ROUTE_PROFILE="
    "LIBRT_BREP_ROUTE_MISSING_ROOT_PROBE="
    "LIBRT_BREP_ROUTE_MISSING_ROOT_CERTIFICATE=1"
    "LIBRT_BREP_PROFILE="
    "${RT}" -B -P 1 -s 64 -a 35 -e 25 -o "${production_image}"
    "${DB}" "${test_object}"
  RESULT_VARIABLE production_result
  OUTPUT_VARIABLE production_stdout
  ERROR_VARIABLE production_stderr
)
if(NOT production_result EQUAL 0 OR NOT EXISTS "${production_image}")
  message(FATAL_ERROR
    "Unprofiled production route failed (${production_result})\n${production_stdout}${production_stderr}"
  )
endif()

execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -E env
    "LIBRT_CACHE=off"
    "LIBRT_BREP_SOLVER=legacy"
    "LIBRT_BREP_ROUTE_DIRECT="
    "LIBRT_BREP_ROUTE_PUBLISH="
    "LIBRT_BREP_ROUTE_MISSING_ROOT_PROBE="
    "LIBRT_BREP_ROUTE_MISSING_ROOT_CERTIFICATE="
    "LIBRT_BREP_ROUTE_PROFILE="
    "LIBRT_BREP_PROFILE="
    "${RT}" -B -P 1 -s 64 -a 35 -e 25 -o "${legacy_image}"
    "${DB}" "${test_object}"
  RESULT_VARIABLE legacy_result
  OUTPUT_VARIABLE legacy_stdout
  ERROR_VARIABLE legacy_stderr
)
if(NOT legacy_result EQUAL 0 OR NOT EXISTS "${legacy_image}")
  message(FATAL_ERROR
    "Legacy control render failed (${legacy_result})\n${legacy_stdout}${legacy_stderr}"
  )
endif()

file(SHA256 "${direct_image}" direct_sha256)
file(SHA256 "${publish_image}" publish_sha256)
file(SHA256 "${probe_image}" probe_sha256)
file(SHA256 "${certificate_image}" certificate_sha256)
file(SHA256 "${production_image}" production_sha256)
file(SHA256 "${legacy_image}" legacy_sha256)
if(NOT direct_sha256 STREQUAL legacy_sha256)
  message(FATAL_ERROR
    "Direct route output differs from legacy: ${direct_sha256} != ${legacy_sha256}"
  )
endif()
if(NOT publish_sha256 STREQUAL legacy_sha256)
  message(FATAL_ERROR
    "Published route output differs from legacy: ${publish_sha256} != ${legacy_sha256}"
  )
endif()
if(NOT probe_sha256 STREQUAL legacy_sha256)
  message(FATAL_ERROR
    "Missing-root probe output differs from legacy: ${probe_sha256} != ${legacy_sha256}"
  )
endif()
if(NOT certificate_sha256 STREQUAL legacy_sha256)
  message(FATAL_ERROR
    "Missing-root certificate output differs from legacy: ${certificate_sha256} != ${legacy_sha256}"
  )
endif()
if(NOT production_sha256 STREQUAL legacy_sha256)
  message(FATAL_ERROR
    "Unprofiled production route output differs from legacy: ${production_sha256} != ${legacy_sha256}"
  )
endif()

file(REMOVE "${direct_image}" "${publish_image}" "${probe_image}"
  "${certificate_image}" "${production_image}" "${legacy_image}")
