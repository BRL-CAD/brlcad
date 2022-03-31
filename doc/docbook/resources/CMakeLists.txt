add_subdirectory(brlcad)
add_subdirectory(other)

set(ignore_files
  CMakeLists.txt
  README
  )
CMAKEFILES(${ignore_files})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
