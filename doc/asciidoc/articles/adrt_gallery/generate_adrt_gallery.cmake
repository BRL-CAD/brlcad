# generate_adrt_gallery.cmake -- regenerate the ADRT path-tracing snapshot.
#
# Run via: cmake -DADRT=... -DRT=... -DMGED=... -DDB=<share/db dir>
#               -DIMGDIR=... -DWORKDIR=... -DSIZE=512
#               -P generate_adrt_gallery.cmake
#
# Reproduces the committed images for doc/asciidoc/articles/adrt_pathtrace.adoc:
# path-traced renders from the standalone `adrt` tool alongside `rt` full
# lighting and `rt` ambient occlusion, for the sample toyjeep, chessboard,
# castle, and Cornell box models.  Cross-platform: uses only cmake tooling.

cmake_minimum_required(VERSION 3.20)

foreach(v ADRT RT MGED DB IMGDIR WORKDIR)
  if(NOT DEFINED ${v})
    message(FATAL_ERROR "generate_adrt_gallery: -D${v}=... is required")
  endif()
endforeach()
if(NOT DEFINED SIZE)
  set(SIZE 512)
endif()

set(RENDER_TIMEOUT 1800)
set(AO "set ambSamples=128")

file(MAKE_DIRECTORY "${WORKDIR}")
file(MAKE_DIRECTORY "${IMGDIR}")

set(N_OK 0)
set(N_FAIL 0)
set(FAILED "")

# Run a render command whose -o target is "${IMGDIR}/<out>", and report on it.
# Retries a few times so a transient failure (e.g. a concurrent build briefly
# replacing a shared library) does not lose an image.
function(render label out)
  set(img "${IMGDIR}/${out}")
  file(REMOVE "${img}")
  set(isz 0)
  foreach(attempt RANGE 1 3)
    execute_process(
      COMMAND ${ARGN}
      WORKING_DIRECTORY "${WORKDIR}" TIMEOUT ${RENDER_TIMEOUT}
      RESULT_VARIABLE rc OUTPUT_VARIABLE log ERROR_VARIABLE log)
    if(EXISTS "${img}")
      file(SIZE "${img}" isz)
    endif()
    if(isz GREATER 0 AND rc EQUAL 0)
      break()
    endif()
  endforeach()
  if(isz GREATER 0 AND rc EQUAL 0)
    message(STATUS "[${label}] wrote ${out} (${isz} bytes)")
    math(EXPR N_OK "${N_OK} + 1" )
  else()
    message(WARNING "[${label}] FAILED (rc=${rc}):\n${log}")
    math(EXPR N_FAIL "${N_FAIL} + 1")
    list(APPEND FAILED "${out}")
  endif()
  set(N_OK "${N_OK}" PARENT_SCOPE)
  set(N_FAIL "${N_FAIL}" PARENT_SCOPE)
  set(FAILED "${FAILED}" PARENT_SCOPE)
endfunction()

# Facetize into a fresh, writable copy of a source .g, retrying a few times to
# ride out the transient mged failures that a concurrent build (churning the
# shared libraries) can cause.  The copy is recreated each attempt so a partial
# result never blocks the retry.  Succeeds once <checkobj> appears in the copy.
# ARGN is the mged sub-command (e.g. "facetize --regions all all_fac").
function(facetize_retry src workfile checkobj)
  set(ok FALSE)
  foreach(attempt RANGE 1 4)
    file(REMOVE "${workfile}")
    configure_file("${src}" "${workfile}" COPYONLY)
    file(CHMOD "${workfile}" PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)
    execute_process(
      COMMAND ${MGED} -c "${workfile}" ${ARGN}
      TIMEOUT ${RENDER_TIMEOUT} RESULT_VARIABLE rc OUTPUT_VARIABLE log ERROR_VARIABLE log)
    execute_process(
      COMMAND ${MGED} -c "${workfile}" tops
      OUTPUT_VARIABLE tops_out ERROR_VARIABLE tops_out)
    if(tops_out MATCHES "${checkobj}")
      set(ok TRUE)
      break()
    endif()
  endforeach()
  if(NOT ok)
    message(WARNING "facetize did not produce '${checkobj}' in ${workfile} after retries:\n${log}")
  endif()
endfunction()

# --- Toy jeep: facetize, then path trace, rt comparison, adrt shading modes --
# A few of the jeep's booleaned parts (front wheels, roll bar, windshield) fail
# on-the-fly NMG boolean evaluation and would be silently dropped on a direct
# load, so facetize the hierarchy first with the Manifold facetizer.  Using
# --regions keeps the region tree (and its inherited colors) intact.  rt traces
# the original CSG directly for the comparison images.
set(TJ "${DB}/toyjeep.g")
set(TJFAC "${WORKDIR}/toyjeep_fac.g")
facetize_retry("${TJ}" "${TJFAC}" all_fac facetize --regions all all_fac)
render(toyjeep_path   adrt_toyjeep_path.png
  ${ADRT} -s${SIZE} -H256 -p12 -a35 -e25 -o "${IMGDIR}/adrt_toyjeep_path.png" "${TJFAC}" all_fac)
render(toyjeep_normal adrt_toyjeep_normal.png
  ${ADRT} -s${SIZE} -m normal -p12 -a35 -e25 -o "${IMGDIR}/adrt_toyjeep_normal.png" "${TJFAC}" all_fac)
render(toyjeep_phong  adrt_toyjeep_phong.png
  ${ADRT} -s${SIZE} -m phong -p12 -a35 -e25 -o "${IMGDIR}/adrt_toyjeep_phong.png" "${TJFAC}" all_fac)
render(toyjeep_depth  adrt_toyjeep_depth.png
  ${ADRT} -s${SIZE} -m depth -p12 -a35 -e25 -o "${IMGDIR}/adrt_toyjeep_depth.png" "${TJFAC}" all_fac)
render(toyjeep_spall  adrt_toyjeep_spall.png
  ${ADRT} -s${SIZE} -m spall -a35 -e25 -A45 -o "${IMGDIR}/adrt_toyjeep_spall.png" "${TJ}" all)
render(toyjeep_rt     adrt_toyjeep_rt.png
  ${RT} -s${SIZE} -a35 -e25 -A0.4 -o "${IMGDIR}/adrt_toyjeep_rt.png" "${TJ}" all)
render(toyjeep_rt_ao  adrt_toyjeep_rt_ao.png
  ${RT} -s${SIZE} -a35 -e25 -A1.1 -c "${AO}" -o "${IMGDIR}/adrt_toyjeep_rt_ao.png" "${TJ}" all)

# --- Chessboard: bright sky lifts the flat, light-colored tiles -------------
set(CH "${DB}/chess/chessboard.g")
render(chess_path   adrt_chess_path.png
  ${ADRT} -s${SIZE} -H768 -p12 -a40 -e35 -C 255/255/255 -o "${IMGDIR}/adrt_chess_path.png" "${CH}" board.g)
render(chess_rt     adrt_chess_rt.png
  ${RT} -s${SIZE} -a40 -e35 -A0.4 -o "${IMGDIR}/adrt_chess_rt.png" "${CH}" board.g)
render(chess_rt_ao  adrt_chess_rt_ao.png
  ${RT} -s${SIZE} -a40 -e35 -A1.1 -c "${AO}" -o "${IMGDIR}/adrt_chess_rt_ao.png" "${CH}" board.g)

# --- Castle: facetize the CSG to a colored BoT, then path trace -------------
# The castle is one region unioning many instanced solids; on-the-fly NMG
# evaluation fails, so convert it once with the Manifold facetizer.  rt still
# traces the original CSG natively for the comparison images.
set(CFAC "${WORKDIR}/castle_fac.g")
facetize_retry("${DB}/castle.g" "${CFAC}" castle_only facetize -o castle_only all.g/castle)
execute_process(COMMAND ${MGED} -c "${CFAC}" r castle_r u castle_only
  TIMEOUT 120 OUTPUT_QUIET ERROR_QUIET)
execute_process(COMMAND ${MGED} -c "${CFAC}" mater castle_r plastic 255 255 100 0
  TIMEOUT 120 OUTPUT_QUIET ERROR_QUIET)
render(castle_path   adrt_castle_path.png
  ${ADRT} -s${SIZE} -H256 -p12 -a135 -e30 -o "${IMGDIR}/adrt_castle_path.png" "${CFAC}" castle_r)
render(castle_rt     adrt_castle_rt.png
  ${RT} -s${SIZE} -a135 -e30 -A0.4 -o "${IMGDIR}/adrt_castle_rt.png" "${DB}/castle.g" castle)
render(castle_rt_ao  adrt_castle_rt_ao.png
  ${RT} -s${SIZE} -a135 -e30 -A1.1 -c "${AO}" -o "${IMGDIR}/adrt_castle_rt_ao.png" "${DB}/castle.g" castle)

# --- Cornell box: global illumination from the emissive ceiling panel -------
set(CB "${DB}/cornell-kunigami.g")
render(cornell_path  adrt_cornell_path.png
  ${ADRT} -s${SIZE} -H512 -E 278,370.9,-454 -L 278,274,279 -p25 -o "${IMGDIR}/adrt_cornell_path.png" "${CB}" all.g)
render(cornell_phong adrt_cornell_phong.png
  ${ADRT} -s${SIZE} -m phong -E 278,370.9,-454 -L 278,274,279 -p25 -o "${IMGDIR}/adrt_cornell_phong.png" "${CB}" all.g)

message(STATUS "")
message(STATUS "ADRT path-tracing snapshot: ${N_OK} rendered, ${N_FAIL} failed.")
if(FAILED)
  string(REPLACE ";" ", " failstr "${FAILED}")
  message(STATUS "Failed: ${failstr}")
endif()
