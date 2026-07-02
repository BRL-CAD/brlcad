brlcad_addexec(ged_create_parity_make "create_parity.c;make_cases.c" "libged;libwdb;librt;libbu" TEST)

foreach(t ged_create_parity_make ged_create_parity_in)
  if(TARGET ${t})
    add_dependencies(${t} ged_plugins)
  endif(TARGET ${t})
endforeach()

brlcad_add_test(NAME ged_create_parity_make COMMAND ged_create_parity_make)

cmakefiles(
  CMakeLists.txt
  create_parity.c
  create_parity.h
  make_cases.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
