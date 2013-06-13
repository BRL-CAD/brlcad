set(BENCHMARK_PIX
  bldg391.pix
  m35.pix
  moss.pix
  sphflake.pix
  star.pix
  world.pix
  )

set(OTHER_PIX
  golf_ball.pix
  lgt1.pix
  lgt2.pix
  lgt3.pix
  mworld.pix
  toilet.pix
  )

set(PIX_LOG
  bldg391.log
  m35.log
  moss.log
  sphflake.log
  star.log
  world.log
  )

set(SAVEVIEWS
  cube.rt
  )

BRLCAD_ADDDATA(BENCHMARK_PIX pix)
BRLCAD_ADDDATA(PIX_LOG pix)

CMAKEFILES(${BENCHMARK_PIX})
CMAKEFILES(${OTHER_PIX})
CMAKEFILES(${PIX_LOG})
CMAKEFILES(${SAVEVIEWS})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
