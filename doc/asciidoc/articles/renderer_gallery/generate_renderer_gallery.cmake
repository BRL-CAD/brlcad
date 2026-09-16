# generate_renderer_gallery.cmake -- regenerate the renderer showcase snapshot.
#
# Run via: cmake -DRT=... -DRTEDGE=... -DRTDEPTH=... -DRTXRAY=... -DRTHIDE=...
#                -DBWPNG=... -DFBCLEAR=... -DPLOT3FB=... -DFBPNG=...
#                -DMANIFEST=... -DIMGDIR=... -DADOC=... -DWORKDIR=...
#                -DDB=... -DSIZE=512 -DTITLE=... -P generate_renderer_gallery.cmake
#
# Cross-platform: uses only cmake / execute_process / file / string.  Tool argv
# is passed directly, and renderer stdin is supplied with INPUT_FILE.

cmake_minimum_required(VERSION 3.20)

foreach(v RT RTEDGE RTDEPTH RTXRAY RTHIDE BWPNG FBCLEAR PLOT3FB FBPNG MANIFEST IMGDIR ADOC WORKDIR DB)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "generate_renderer_gallery: -D${v}=... is required")
  endif()
endforeach()
if(NOT DEFINED SIZE)
  set(SIZE 512)
endif()
if(NOT DEFINED TITLE)
  set(TITLE "BRL-CAD Renderer Showcase")
endif()
if(NOT DEFINED PERSPECTIVE)
  set(PERSPECTIVE 55)
endif()
if(NOT DEFINED PROCESSORS)
  set(PROCESSORS 1)
endif()

set(RENDER_TIMEOUT 1800)

include("${MANIFEST}")

file(MAKE_DIRECTORY "${WORKDIR}")
file(MAKE_DIRECTORY "${IMGDIR}")

set(VIEW_SCRIPT "${WORKDIR}/cornell.view")
file(WRITE "${VIEW_SCRIPT}" "viewsize 6.000000000000000e+03;\n")
file(APPEND "${VIEW_SCRIPT}" "orientation 0.000000000000000e+00 9.961946980917455e-01 8.715574274765824e-02 0.000000000000000e+00;\n")
file(APPEND "${VIEW_SCRIPT}" "eye_pt 2.780000000000000e+02 3.709445330007912e+02 -4.544232590366239e+02;\n")
file(APPEND "${VIEW_SCRIPT}" "start 0; clean;\n")
file(APPEND "${VIEW_SCRIPT}" "end;\n")

function(parse_item rec)
  set(remaining "${rec}")
  foreach(fn name section tool output args title)
    string(FIND "${remaining}" "|" p)
    if(p LESS 0)
      set(D_${fn} "${remaining}" PARENT_SCOPE)
      set(remaining "")
    else()
      string(SUBSTRING "${remaining}" 0 ${p} v)
      set(D_${fn} "${v}" PARENT_SCOPE)
      math(EXPR np "${p} + 1")
      string(SUBSTRING "${remaining}" ${np} -1 remaining)
    endif()
  endforeach()
  set(D_caption "${remaining}" PARENT_SCOPE)
endfunction()

function(tool_path tool outvar)
  if(tool STREQUAL "rt")
    set(${outvar} "${RT}" PARENT_SCOPE)
  elseif(tool STREQUAL "rtedge")
    set(${outvar} "${RTEDGE}" PARENT_SCOPE)
  elseif(tool STREQUAL "rtdepth")
    set(${outvar} "${RTDEPTH}" PARENT_SCOPE)
  elseif(tool STREQUAL "rtxray")
    set(${outvar} "${RTXRAY}" PARENT_SCOPE)
  elseif(tool STREQUAL "rthide")
    set(${outvar} "${RTHIDE}" PARENT_SCOPE)
  else()
    message(FATAL_ERROR "Unknown renderer tool: ${tool}")
  endif()
endfunction()

set(SECTION_ORDER "")
set(N_OK 0)
set(N_FAIL 0)
set(FAILED "")

math(EXPR LAST "${RENDERER_GALLERY_ITEM_COUNT} - 1")
foreach(i RANGE 0 ${LAST})
  parse_item("${RENDERER_GALLERY_ITEM_${i}}")
  tool_path("${D_tool}" exe)
  separate_arguments(extra_args UNIX_COMMAND "${D_args}")

  set(wd "${WORKDIR}/${D_name}")
  file(REMOVE_RECURSE "${wd}")
  file(MAKE_DIRECTORY "${wd}")

  set(imgpath "${IMGDIR}/renderer_gallery_${D_name}.png")
  set(logpath "${wd}/${D_name}.log")
  file(REMOVE "${imgpath}")
  file(REMOVE "${logpath}")

  set(ok FALSE)

  if(D_output STREQUAL "png")
    set(cmd "${exe}" -B -M "-p${PERSPECTIVE}" "-s${SIZE}" "-P${PROCESSORS}")
    list(APPEND cmd ${extra_args} -o "${imgpath}" "${DB}" all.g)
    execute_process(
      COMMAND ${cmd}
      WORKING_DIRECTORY "${wd}" TIMEOUT ${RENDER_TIMEOUT}
      INPUT_FILE "${VIEW_SCRIPT}"
      RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
    file(WRITE "${logpath}" "${out}")
  elseif(D_output STREQUAL "bw")
    set(raw "${wd}/${D_name}.bw")
    file(REMOVE "${raw}")
    set(cmd "${exe}" -B -M "-p${PERSPECTIVE}" "-s${SIZE}" "-P${PROCESSORS}")
    list(APPEND cmd ${extra_args} -o "${raw}" "${DB}" all.g)
    execute_process(
      COMMAND ${cmd}
      WORKING_DIRECTORY "${wd}" TIMEOUT ${RENDER_TIMEOUT}
      INPUT_FILE "${VIEW_SCRIPT}"
      RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
    file(WRITE "${logpath}" "${out}")
    if(rc EQUAL 0 AND EXISTS "${raw}")
      execute_process(
        COMMAND "${BWPNG}" "-s${SIZE}" "${raw}"
        WORKING_DIRECTORY "${wd}" TIMEOUT ${RENDER_TIMEOUT}
        RESULT_VARIABLE conv_rc OUTPUT_FILE "${imgpath}" ERROR_VARIABLE conv_out)
      file(APPEND "${logpath}" "${conv_out}")
      if(NOT conv_rc EQUAL 0)
        set(rc ${conv_rc})
      endif()
    endif()
  elseif(D_output STREQUAL "plot3")
    set(raw "${wd}/${D_name}.plot3")
    set(fb "${wd}/${D_name}.fb")
    file(REMOVE "${raw}" "${fb}")
    set(cmd "${exe}" -B -M "-p${PERSPECTIVE}" "-s${SIZE}" "-P${PROCESSORS}")
    list(APPEND cmd ${extra_args} -o "${raw}" "${DB}" all.g)
    execute_process(
      COMMAND ${cmd}
      WORKING_DIRECTORY "${wd}" TIMEOUT ${RENDER_TIMEOUT}
      INPUT_FILE "${VIEW_SCRIPT}"
      RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE out)
    file(WRITE "${logpath}" "${out}")
    if(rc EQUAL 0 AND EXISTS "${raw}")
      execute_process(
        COMMAND "${FBCLEAR}" -F "${fb}" "-s${SIZE}" 255 255 255
        WORKING_DIRECTORY "${wd}" TIMEOUT ${RENDER_TIMEOUT}
        RESULT_VARIABLE clear_rc OUTPUT_VARIABLE clear_out ERROR_VARIABLE clear_out)
      file(APPEND "${logpath}" "${clear_out}")
      execute_process(
        COMMAND "${PLOT3FB}" -F "${fb}" -o "-s${SIZE}"
        WORKING_DIRECTORY "${wd}" TIMEOUT ${RENDER_TIMEOUT}
        INPUT_FILE "${raw}"
        RESULT_VARIABLE plot_rc OUTPUT_VARIABLE plot_out ERROR_VARIABLE plot_out)
      file(APPEND "${logpath}" "${plot_out}")
      execute_process(
        COMMAND "${FBPNG}" -F "${fb}" "-s${SIZE}" "${imgpath}"
        WORKING_DIRECTORY "${wd}" TIMEOUT ${RENDER_TIMEOUT}
        RESULT_VARIABLE fb_rc OUTPUT_VARIABLE fb_out ERROR_VARIABLE fb_out)
      file(APPEND "${logpath}" "${fb_out}")
      if(NOT clear_rc EQUAL 0)
        set(rc ${clear_rc})
      elseif(NOT plot_rc EQUAL 0)
        set(rc ${plot_rc})
      elseif(NOT fb_rc EQUAL 0)
        set(rc ${fb_rc})
      endif()
    endif()
  else()
    message(FATAL_ERROR "Unknown renderer output type: ${D_output}")
  endif()

  if(EXISTS "${imgpath}")
    file(SIZE "${imgpath}" isz)
    if(isz GREATER 0 AND rc EQUAL 0)
      set(ok TRUE)
    endif()
  endif()

  if(ok)
    file(
      CHMOD "${imgpath}"
      PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    )
    message(STATUS "[${D_name}] rendered ${imgpath} (${isz} bytes)")
    math(EXPR N_OK "${N_OK} + 1")
  else()
    if(EXISTS "${logpath}")
      file(READ "${logpath}" failure_log)
    else()
      set(failure_log "")
    endif()
    message(WARNING "[${D_name}] render failed (rc=${rc}). Output:\n${failure_log}")
    list(APPEND FAILED "${D_name}")
    math(EXPR N_FAIL "${N_FAIL} + 1")
  endif()

  set(R_${i}_name "${D_name}")
  set(R_${i}_section "${D_section}")
  set(R_${i}_title "${D_title}")
  set(R_${i}_caption "${D_caption}")
  set(R_${i}_ok "${ok}")
  if(NOT D_section IN_LIST SECTION_ORDER)
    list(APPEND SECTION_ORDER "${D_section}")
  endif()
endforeach()

set(adoc "= ${TITLE}\n")
string(APPEND adoc ":doctype: article\n")
string(APPEND adoc ":toc:\n")
string(APPEND adoc ":toclevels: 2\n")
string(APPEND adoc "\n")
string(APPEND adoc "This gallery shows BRL-CAD's image-producing ray-tracing front ends and\n")
string(APPEND adoc "selected `rt` output styles rendered against the sample Cornell box model.\n")
string(APPEND adoc "Each snapshot uses the same database, object, perspective, and saved view\n")
string(APPEND adoc "matrix from `db/cornell.rt`:\n")
string(APPEND adoc "\n----\n")
string(APPEND adoc "rt -B -M -p${PERSPECTIVE} -s${SIZE} -P${PROCESSORS} [style options] -o <name>.png cornell.g all.g < cornell.view\n")
string(APPEND adoc "----\n")
string(APPEND adoc "\n")
string(APPEND adoc "The scalar and plot outputs are converted with BRL-CAD image tools.  In\n")
string(APPEND adoc "particular, `rthide` is rendered as plot3 and rasterized with `plot3-fb` and\n")
string(APPEND adoc "`fb-png`.\n")
string(APPEND adoc "\n")
string(APPEND adoc "NOTE: This article and its images are a committed snapshot. Developers can\n")
string(APPEND adoc "regenerate them from the built tools by configuring with\n")
string(APPEND adoc "`-DBRLCAD_GENERATE_RENDERER_GALLERY=ON` and building the\n")
string(APPEND adoc "`renderer-gallery-snapshot` target.\n")

foreach(sec ${SECTION_ORDER})
  string(APPEND adoc "\n== ${sec}\n\n")
  string(APPEND adoc "[cols=\"1,3\"]\n[%noheader]\n|===\n")
  foreach(i RANGE 0 ${LAST})
    if(DEFINED R_${i}_ok AND R_${i}_ok AND R_${i}_section STREQUAL sec)
      string(APPEND adoc "|image:images/renderer_gallery_${R_${i}_name}.png[${R_${i}_name},${SIZE}]\n")
      string(APPEND adoc "|*${R_${i}_title}*: ${R_${i}_caption}\n")
    endif()
  endforeach()
  string(APPEND adoc "|===\n")
endforeach()

file(WRITE "${ADOC}" "${adoc}")

message(STATUS "")
message(STATUS "Renderer gallery snapshot: ${N_OK} rendered, ${N_FAIL} failed/skipped.")
if(FAILED)
  string(REPLACE ";" ", " failstr "${FAILED}")
  message(STATUS "Failed/skipped: ${failstr}")
endif()
message(STATUS "Article written: ${ADOC}")
