# Minimum required version of CMake
CMAKE_MINIMUM_REQUIRED(VERSION 2.8)
IF(COMMAND CMAKE_POLICY)
	CMAKE_POLICY(SET CMP0003 NEW)
ENDIF(COMMAND CMAKE_POLICY)

PROJECT(PDCURSES_WIN32A)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}
	)

set(PDCURSES_SOURCES
    pdcurses/addch.c
    pdcurses/addchstr.c
    pdcurses/addstr.c
    pdcurses/attr.c
    pdcurses/beep.c
    pdcurses/bkgd.c
    pdcurses/border.c
    pdcurses/clear.c
    pdcurses/color.c
    pdcurses/delch.c
    pdcurses/deleteln.c
    pdcurses/deprec.c
    pdcurses/getch.c
    pdcurses/getstr.c
    pdcurses/getyx.c
    pdcurses/inch.c
    pdcurses/inchstr.c
    pdcurses/initscr.c
    pdcurses/inopts.c
    pdcurses/insch.c
    pdcurses/insstr.c
    pdcurses/instr.c
    pdcurses/kernel.c
    pdcurses/keyname.c
    pdcurses/mouse.c
    pdcurses/move.c
    pdcurses/outopts.c
    pdcurses/overlay.c
    pdcurses/pad.c
    pdcurses/panel.c
    pdcurses/printw.c
    pdcurses/refresh.c
    pdcurses/scanw.c
    pdcurses/scr_dump.c
    pdcurses/scroll.c
    pdcurses/slk.c
    pdcurses/termattr.c
    pdcurses/terminfo.c
    pdcurses/touch.c
    pdcurses/util.c
    pdcurses/window.c
    pdcurses/debug.c
)

set(PDCURSES_WIN32A_SRCS
    win32a/pdcclip.c
    win32a/pdcdisp.c
    win32a/pdcgetsc.c
    win32a/pdckbd.c
    win32a/pdcscrn.c
    win32a/pdcsetsc.c
    win32a/pdcutil.c
    )

add_definitions(
	-DPDC_DLL_BUILD
	)

SET(CMAKE_SHARED_LINKER_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/win32a/pdcurses.def")
add_library(pdcurses SHARED ${PDCURSES_SOURCES} ${PDCURSES_WIN32A_SRCS})
INSTALL(TARGETS pdcurses DESTINATION ${LIB_DIR})
add_library(pdcurses-static STATIC ${PDCURSES_SOURCES} ${PDCURSES_WIN32A_SRCS})
INSTALL(TARGETS pdcurses-static ARCHIVE DESTINATION ${LIB_DIR})

IF(PDCURSES_BUILD_DEMOS)
include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/demos
)

add_executable(testcurs demos/testcurs.c)
target_link_libraries(testcurs pdcurses user32.lib gdi32.lib advapi32.lib shell32.lib)

add_executable(newdemo demos/newdemo.c)
target_link_libraries(newdemo pdcurses user32.lib gdi32.lib advapi32.lib shell32.lib)

add_executable(xmas demos/xmas.c)
target_link_libraries(xmas pdcurses user32.lib gdi32.lib advapi32.lib shell32.lib)

add_executable(tuidemo demos/tui.c demos/tuidemo.c)
target_link_libraries(tuidemo pdcurses user32.lib gdi32.lib advapi32.lib shell32.lib)

add_executable(firework demos/firework.c)
target_link_libraries(firework pdcurses user32.lib gdi32.lib advapi32.lib shell32.lib)

add_executable(ptest demos/ptest.c)
target_link_libraries(ptest pdcurses user32.lib gdi32.lib advapi32.lib shell32.lib)

add_executable(rain demos/rain.c)
target_link_libraries(rain pdcurses user32.lib gdi32.lib advapi32.lib shell32.lib)

add_executable(worm demos/worm.c)
target_link_libraries(worm pdcurses user32.lib gdi32.lib advapi32.lib shell32.lib)
ENDIF(PDCURSES_BUILD_DEMOS)
