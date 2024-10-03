set(
  BENCHMARK_PIX
  bldg391.pix
  m35.pix
  moss.pix
  sphflake.pix
  star.pix
  world.pix
)

set(
  PIX_LOG
  bldg391.log
  m35.log
  moss.log
  sphflake.log
  star.log
  world.log
)

brlcad_adddata(BENCHMARK_PIX pix)
brlcad_adddata(PIX_LOG pix)

cmakefiles(${BENCHMARK_PIX})
cmakefiles(${PIX_LOG})
cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
