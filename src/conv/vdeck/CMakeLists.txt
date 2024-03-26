BRLCAD_ADDEXEC(vdeck "cgarbs.c;match.c;parsarg.c;vdeck.c" "librt;${M_LIBRARY}" FOLDER Conv)
target_include_directories(vdeck PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
CMAKEFILES(std.h vextern.h)
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
