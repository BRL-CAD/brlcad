SET(hv3_TCLSCRIPTS
	bookmarks.html
	bugreport.html
	combobox.tcl
	hv3_bookmarks.tcl
	hv3bridge.c
	hv3_browser.tcl
	hv3_bugreport.tcl
	hv3_common.tcl
	hv3_console.tcl
	hv3_db.tcl
	hv3_debug.tcl
	hv3_doctype.tcl
	hv3_dom2.tcl
	hv3_dom3.tcl
	hv3_dom_compiler.tcl
	hv3_dom_containers.tcl
	hv3_dom_core.tcl
	hv3_dom_events.tcl
	hv3_dom_html.tcl
	hv3_dom_ns.tcl
	hv3_dom_style.tcl
	hv3_dom.tcl
	hv3_dom_xmlhttp.tcl
	hv3_download.tcl
	hv3_encodings.tcl
	hv3events.c
	hv3_file.tcl
	hv3format.c
	hv3_form.tcl
	hv3_frameset.tcl
	hv3_frames.tcl
	hv3function.c
	hv3_history.tcl
	hv3_home.tcl
	hv3_http.tcl
	hv3_icons.tcl
	hv3_image.tcl
	hv3_log.tcl
	hv3_main.tcl
	hv3_nav.tcl
	hv3_notebook.tcl
	hv3_object.tcl
	hv3_polipo.tcl
	hv3_profile.tcl
	hv3_prop.tcl
	hv3_request.tcl
	hv3see.c
	hv3_string.tcl
	hv3_style.tcl
	hv3.tcl
	hv3timeout.c
	hv3_url.tcl
	hv3_util.tcl
	hv3_widgets.tcl
	hvinit.c
	hv.tcl
	index.html
	license_snit.txt
	license.txt
	main.tcl
	nogif.fig
	nogif.gif
	nogifsm.gif
	snit2.tcl
	snit.tcl
	ssinit.c
	ss.tcl
	tst_main.tcl
)
file(COPY ${hv3_TCLSCRIPTS} DESTINATION ${CMAKE_BINARY_DIR}/lib/hv3)
install(FILES ${hv3_TCLSCRIPTS} DESTINATION lib/hv3)

file(WRITE ${CMAKE_BINARY_DIR}/lib/hv3/pkgIndex.tcl "package ifneeded snit 1.0 [list source [file join $dir snit.tcl]]\n")
file(APPEND ${CMAKE_BINARY_DIR}/lib/hv3/pkgIndex.tcl "package ifneeded hv3 0.1 [list source [file join $dir hv3.tcl]]")
install(FILES ${CMAKE_BINARY_DIR}/lib/hv3/pkgIndex.tcl DESTINATION lib/hv3)

SET(hv3_launch
"#!/bin/sh
#\\
exec ${CMAKE_BINARY_DIR}/bin/wish $0 $\@
source ${CMAKE_BINARY_DIR}/lib/hv3/hv3_main.tcl
")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/hv3 "${hv3_launch}")
file(COPY ${CMAKE_CURRENT_BINARY_DIR}/hv3
	DESTINATION ${CMAKE_BINARY_DIR}/bin
	FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
	GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

set(hv3_bat_launch
"\@ECHO OFF
SETLOCAL
SET WISH=%~dp0wish
SET HV3=%~dp0..\\lib\\hv3\\hv3_main.tcl
START /B \"\" \"%WISH%\" \"%HV3%\" %1
")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/hv3.bat "${hv3_bat_launch}")
file(COPY ${CMAKE_CURRENT_BINARY_DIR}/hv3.bat
	DESTINATION ${CMAKE_BINARY_DIR}/bin
	FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ
	GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
