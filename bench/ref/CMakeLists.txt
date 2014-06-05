set(BENCHMARK_PIX
  bldg391.pix
  m35.pix
  moss.pix
  sphflake.pix
  star.pix
  world.pix
  )


set(PIX_LOG
  bldg391.log
  m35.log
  moss.log
  sphflake.log
  star.log
  world.log
  )

BRLCAD_ADDDATA(BENCHMARK_PIX pix)
BRLCAD_ADDDATA(PIX_LOG pix)

CMAKEFILES(${BENCHMARK_PIX})
CMAKEFILES(${PIX_LOG})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
