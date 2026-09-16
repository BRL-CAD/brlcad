# generate_gallery.cmake -- regenerate the procedural geometry gallery snapshot.
#
# Run via:  cmake -DRT=... -DMGED=... -DBINDIR=... -DMANIFEST=... -DIMGDIR=...
#                 -DADOC=... -DINPUTDIR=... -DWORKDIR=... -DSIZE=512
#                 -DTITLE=... -P generate_gallery.cmake
#
# Drives every demo (manifest row) to produce a .g, ray traces it to a 512px
# PNG with rt, and emits the AsciiDoc article.  Cross-platform: uses only
# cmake / execute_process / file / string -- no bash, cmd, python, or shell.
# Tool argv is passed directly (no shell), so quoting bugs are impossible.

cmake_minimum_required(VERSION 3.20)

# ---- required inputs --------------------------------------------------------
foreach(v RT MGED MANIFEST IMGDIR ADOC INPUTDIR WORKDIR)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "generate_gallery: -D${v}=... is required")
  endif()
endforeach()
if(NOT DEFINED SIZE)
  set(SIZE 512)
endif()
if(NOT DEFINED TITLE)
  set(TITLE "BRL-CAD Procedural Geometry Gallery")
endif()
if(NOT DEFINED BINDIR)
  get_filename_component(BINDIR "${RT}" DIRECTORY)
endif()

set(GEN_TIMEOUT 300)    # seconds for a single geometry-generation step
set(RENDER_TIMEOUT 1800) # seconds for a single rt render

include("${MANIFEST}")

file(MAKE_DIRECTORY "${WORKDIR}")
file(MAKE_DIRECTORY "${IMGDIR}")

# ---- helpers ----------------------------------------------------------------

# Split a |-delimited record into D_name D_section D_driver D_exe D_args
# D_gfile D_objects D_autoview D_render D_caption.  Caption (last field) is the
# verbatim remainder, so embedded semicolons/commas are preserved.
function(parse_demo rec)
  set(remaining "${rec}")
  foreach(fn name section driver exe args gfile objects autoview render)
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

# Locate a demo executable by basename in BINDIR (handles the .exe suffix).
function(resolve_exe name outvar)
  foreach(cand "${name}" "${name}.exe")
    if(EXISTS "${BINDIR}/${cand}")
      set(${outvar} "${BINDIR}/${cand}" PARENT_SCOPE)
      return()
    endif()
  endforeach()
  set(${outvar} "" PARENT_SCOPE)
endfunction()

# Substitute @OUT@ and @IN:<file>@ tokens in a single argv element.  @OUT@ is
# substituted as a RELATIVE filename (the demo runs with WORKING_DIRECTORY set
# to its work dir), so legacy generators with small fixed-size filename buffers
# do not truncate a long absolute path.  @IN: paths stay absolute.
function(subst_tokens tok outrel outvar)
  if(tok MATCHES "^@IN:(.+)@$")
    set(${outvar} "${INPUTDIR}/${CMAKE_MATCH_1}" PARENT_SCOPE)
  else()
    string(REPLACE "@OUT@" "${outrel}" tok "${tok}")
    set(${outvar} "${tok}" PARENT_SCOPE)
  endif()
endfunction()

# ---- main loop --------------------------------------------------------------

set(SECTION_ORDER "")   # ordered unique list of section names (no ';' in names)
set(N_OK 0)
set(N_FAIL 0)
set(FAILED "")

math(EXPR LAST "${GALLERY_DEMO_COUNT} - 1")
foreach(i RANGE 0 ${LAST})
  parse_demo("${GALLERY_DEMO_${i}}")

  set(wd "${WORKDIR}/${D_name}")
  file(REMOVE_RECURSE "${wd}")
  file(MAKE_DIRECTORY "${wd}")
  set(out_rel "${D_name}.g")        # relative name passed to demo args (@OUT@)
  set(outg "${wd}/${out_rel}")      # absolute path for gfile resolution / mged

  resolve_exe("${D_exe}" exepath)
  if(NOT exepath)
    message(WARNING "[${D_name}] executable '${D_exe}' not found in ${BINDIR} -- skipping")
    list(APPEND FAILED "${D_name} (no exe)")
    math(EXPR N_FAIL "${N_FAIL} + 1")
    continue()
  endif()

  # Build the resolved argv list (quotes respected, tokens substituted).
  separate_arguments(rawargs UNIX_COMMAND "${D_args}")
  set(finalargs "")
  foreach(tok ${rawargs})
    subst_tokens("${tok}" "${out_rel}" rtok)
    list(APPEND finalargs "${rtok}")
  endforeach()

  # ---- run the generator step by driver -------------------------------------
  set(rc 0)
  set(genout "")
  if(D_driver STREQUAL "exec")
    execute_process(
      COMMAND "${exepath}" ${finalargs}
      WORKING_DIRECTORY "${wd}" TIMEOUT ${GEN_TIMEOUT}
      RESULT_VARIABLE rc OUTPUT_VARIABLE genout ERROR_VARIABLE genout)
  elseif(D_driver MATCHES "^stdin:(.+)$")
    set(infile "${INPUTDIR}/${CMAKE_MATCH_1}")
    execute_process(
      COMMAND "${exepath}" ${finalargs}
      WORKING_DIRECTORY "${wd}" TIMEOUT ${GEN_TIMEOUT}
      INPUT_FILE "${infile}"
      RESULT_VARIABLE rc OUTPUT_VARIABLE genout ERROR_VARIABLE genout)
  elseif(D_driver STREQUAL "tomged")
    # capture exe stdout (mged command stream), then pipe into mged
    execute_process(
      COMMAND "${exepath}" ${finalargs}
      WORKING_DIRECTORY "${wd}" TIMEOUT ${GEN_TIMEOUT}
      OUTPUT_FILE "${wd}/cmds.mged" ERROR_VARIABLE genout RESULT_VARIABLE rc)
    if(rc EQUAL 0)
      execute_process(
        COMMAND "${MGED}" -c "${outg}"
        WORKING_DIRECTORY "${wd}" TIMEOUT ${GEN_TIMEOUT}
        INPUT_FILE "${wd}/cmds.mged" OUTPUT_VARIABLE genout ERROR_VARIABLE genout
        RESULT_VARIABLE rc)
    endif()
  elseif(D_driver MATCHES "^mgedcmd:(.+)$")
    set(gedcmd "${CMAKE_MATCH_1}")
    file(WRITE "${wd}/cmds.mged" "${gedcmd}\nq\n")
    execute_process(
      COMMAND "${MGED}" -c "${outg}"
      WORKING_DIRECTORY "${wd}" TIMEOUT ${GEN_TIMEOUT}
      INPUT_FILE "${wd}/cmds.mged" OUTPUT_VARIABLE genout ERROR_VARIABLE genout
      RESULT_VARIABLE rc)
  else()
    message(WARNING "[${D_name}] unknown driver '${D_driver}' -- skipping")
    list(APPEND FAILED "${D_name} (bad driver)")
    math(EXPR N_FAIL "${N_FAIL} + 1")
    continue()
  endif()

  # ---- resolve the database to render ---------------------------------------
  if(D_gfile STREQUAL "@OUT@")
    set(gpath "${outg}")
  elseif(D_gfile STREQUAL "@GLOB@")
    file(GLOB gg "${wd}/*.g")
    list(LENGTH gg ng)
    if(ng EQUAL 0)
      set(gpath "")
    else()
      list(GET gg 0 gpath)
      if(ng GREATER 1)
        message(WARNING "[${D_name}] ${ng} .g files in workdir; using ${gpath}")
      endif()
    endif()
  else()
    set(gpath "${wd}/${D_gfile}")
  endif()

  if(NOT gpath OR NOT EXISTS "${gpath}")
    message(WARNING "[${D_name}] generation failed (rc=${rc}, no .g). Output:\n${genout}")
    list(APPEND FAILED "${D_name} (no .g)")
    math(EXPR N_FAIL "${N_FAIL} + 1")
    continue()
  endif()

  # ---- render with rt -------------------------------------------------------
  set(imgpath "${IMGDIR}/gallery_${D_name}.png")
  file(REMOVE "${imgpath}")   # rt refuses to overwrite an existing -o target

  set(rtcmd "${RT}" "-s${SIZE}")
  if(NOT D_render STREQUAL "noao")
    list(APPEND rtcmd -c "set ambSamples=64")
  endif()
  if(NOT D_autoview STREQUAL "")
    list(APPEND rtcmd -c "autoview ${D_autoview}")
  endif()
  list(APPEND rtcmd -A1 -o "${imgpath}" "${gpath}")
  separate_arguments(objlist UNIX_COMMAND "${D_objects}")
  foreach(o ${objlist})
    list(APPEND rtcmd "${o}")
  endforeach()

  execute_process(
    COMMAND ${rtcmd}
    WORKING_DIRECTORY "${wd}" TIMEOUT ${RENDER_TIMEOUT}
    RESULT_VARIABLE rc OUTPUT_VARIABLE rtout ERROR_VARIABLE rtout)

  set(ok FALSE)
  if(EXISTS "${imgpath}")
    file(SIZE "${imgpath}" isz)
    if(isz GREATER 3000)
      set(ok TRUE)
    endif()
  endif()

  if(ok)
    message(STATUS "[${D_name}] rendered ${imgpath} (${isz} bytes)")
    math(EXPR N_OK "${N_OK} + 1")
  else()
    message(WARNING "[${D_name}] rt render failed (rc=${rc}). Output:\n${rtout}")
    list(APPEND FAILED "${D_name} (render)")
    math(EXPR N_FAIL "${N_FAIL} + 1")
  endif()

  # ---- stash result for adoc emission ---------------------------------------
  set(R_${i}_name "${D_name}")
  set(R_${i}_section "${D_section}")
  set(R_${i}_caption "${D_caption}")
  set(R_${i}_ok "${ok}")
  if(NOT D_section IN_LIST SECTION_ORDER)
    list(APPEND SECTION_ORDER "${D_section}")
  endif()
endforeach()

# ---- emit the AsciiDoc article ---------------------------------------------

set(AO_LINE "rt -s${SIZE} -c \"set ambSamples=64\" [-c \"autoview <objs>\"] -A1 -o <name>.png <db>.g <objects>")

set(adoc "= ${TITLE}\n")
string(APPEND adoc ":doctype: article\n")
string(APPEND adoc ":toc:\n")
string(APPEND adoc ":toclevels: 2\n")
string(APPEND adoc "\n")
string(APPEND adoc "This gallery shows the procedural-geometry demonstration programs in BRL-CAD's\n")
string(APPEND adoc "`src/proc-db` and `src/shapes` directories. Each model is built from source by its\n")
string(APPEND adoc "generator program and then ray traced with `rt` -- no hand modeling. The renders are\n")
string(APPEND adoc "auto-framed and auto-lit by the ray tracer using ambient occlusion:\n")
string(APPEND adoc "\n----\n${AO_LINE}\n----\n")
string(APPEND adoc "\n")
string(APPEND adoc "Some renders use the `rt -c \"autoview <objs>\"` command to frame a sub-scene (for\n")
string(APPEND adoc "example, to exclude a distant light source from the view).\n")
string(APPEND adoc "\n")
string(APPEND adoc "NOTE: This article and its images are a committed snapshot. Developers can regenerate\n")
string(APPEND adoc "them from the built tools by configuring with `-DBRLCAD_GENERATE_GALLERY=ON` and\n")
string(APPEND adoc "building the `procedural-gallery-snapshot` target.\n")

foreach(sec ${SECTION_ORDER})
  string(APPEND adoc "\n== ${sec}\n\n")
  string(APPEND adoc "[cols=\"1,3\"]\n[%noheader]\n|===\n")
  foreach(i RANGE 0 ${LAST})
    if(DEFINED R_${i}_ok AND R_${i}_ok AND R_${i}_section STREQUAL sec)
      set(nm "${R_${i}_name}")
      string(APPEND adoc "|image:images/gallery_${nm}.png[${nm},${SIZE}]\n")
      string(APPEND adoc "|*${nm}*: ${R_${i}_caption}\n")
    endif()
  endforeach()
  string(APPEND adoc "|===\n")
endforeach()

# Known limitations
if(DEFINED GALLERY_NR_COUNT AND GALLERY_NR_COUNT GREATER 0)
  string(APPEND adoc "\n== Known limitations\n\n")
  string(APPEND adoc "A few programs are part of the source tree but are not shown above, because the\n")
  string(APPEND adoc "geometry they produce cannot be ray traced in the current tree:\n\n")
  math(EXPR NRLAST "${GALLERY_NR_COUNT} - 1")
  foreach(j RANGE 0 ${NRLAST})
    set(rec "${GALLERY_NR_${j}}")
    string(FIND "${rec}" "|" p)
    string(SUBSTRING "${rec}" 0 ${p} nrname)
    math(EXPR np "${p} + 1")
    string(SUBSTRING "${rec}" ${np} -1 nrreason)
    string(APPEND adoc "* *${nrname}* -- ${nrreason}\n")
  endforeach()
endif()

file(WRITE "${ADOC}" "${adoc}")

# ---- summary ----------------------------------------------------------------
message(STATUS "")
message(STATUS "Gallery snapshot: ${N_OK} rendered, ${N_FAIL} failed/skipped.")
if(FAILED)
  string(REPLACE ";" ", " failstr "${FAILED}")
  message(STATUS "Failed/skipped: ${failstr}")
endif()
message(STATUS "Article written: ${ADOC}")
