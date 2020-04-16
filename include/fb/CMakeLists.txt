set(fb_headers
  defines.h
  calltable.h
  )
BRLCAD_MANAGE_FILES(fb_headers ${INCLUDE_DIR}/brlcad/fb)

# For libdm
set(fb_noinstall_headers
  fb_ogl.h
  fb_osgl.h
  fb_qt.h
  fb_wgl.h
  fb_X.h
  )

CMAKEFILES(${fb_noinstall_headers})
CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
