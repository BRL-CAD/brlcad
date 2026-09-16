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
function(render label out)
  set(img "${IMGDIR}/${out}")
  file(REMOVE "${img}")
  execute_process(
    COMMAND ${ARGN}
    WORKING_DIRECTORY "${WORKDIR}" TIMEOUT ${RENDER_TIMEOUT}
    RESULT_VARIABLE rc OUTPUT_VARIABLE log ERROR_VARIABLE log)
  if(EXISTS "${img}")
    file(SIZE "${img}" isz)
  else()
    set(isz 0)
  endif()
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

# --- Toy jeep: path trace, rt comparison, and adrt shading modes ------------
set(TJ "${DB}/toyjeep.g")
render(toyjeep_path   adrt_toyjeep_path.png
  ${ADRT} -s${SIZE} -H256 -p12 -a35 -e25 -o "${IMGDIR}/adrt_toyjeep_path.png" "${TJ}" all)
render(toyjeep_normal adrt_toyjeep_normal.png
  ${ADRT} -s${SIZE} -m normal -p12 -a35 -e25 -o "${IMGDIR}/adrt_toyjeep_normal.png" "${TJ}" all)
render(toyjeep_phong  adrt_toyjeep_phong.png
  ${ADRT} -s${SIZE} -m phong -p12 -a35 -e25 -o "${IMGDIR}/adrt_toyjeep_phong.png" "${TJ}" all)
render(toyjeep_depth  adrt_toyjeep_depth.png
  ${ADRT} -s${SIZE} -m depth -p12 -a35 -e25 -o "${IMGDIR}/adrt_toyjeep_depth.png" "${TJ}" all)
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
file(REMOVE "${CFAC}")
configure_file("${DB}/castle.g" "${CFAC}" COPYONLY)
execute_process(COMMAND ${MGED} -c "${CFAC}" facetize -o castle_only all.g/castle
  TIMEOUT ${RENDER_TIMEOUT} RESULT_VARIABLE frc OUTPUT_VARIABLE flog ERROR_VARIABLE flog)
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
