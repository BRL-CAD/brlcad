set(nirt_format_files
  csv.nrt
  csv-gap.nrt
  default.nrt
  default-ged.nrt
  diff.nrt
  entryexit.nrt
  gap1.nrt
  gap2.nrt
  )
BRLCAD_ADDDATA(nirt_format_files nirt)

if(TARGET nirt)
  add_dependencies(nirt nirt_format_files_cp)
endif(TARGET nirt)

CMAKEFILES(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
