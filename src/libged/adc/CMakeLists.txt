set(ADC_LIBS libged libbu)

ged_plugin_library(ged-adc
  adc.c
  LIBS ${ADC_LIBS}
)

cmakefiles(
  CMakeLists.txt
  adc.c
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
