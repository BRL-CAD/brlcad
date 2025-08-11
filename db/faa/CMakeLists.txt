set(FAA_SAMPLE_MODELS Business_Jet.fg4 Generic_Twin.fg4)

foreach(faa_input ${FAA_SAMPLE_MODELS})
  g_target(${faa_input} "BRL-CAD Geometry Models/Sample")
endforeach(faa_input ${FAA_SAMPLE_MODELS})

cmakefiles(
  ${FAA_SAMPLE_MODELS}
  CMakeLists.txt
  README
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
