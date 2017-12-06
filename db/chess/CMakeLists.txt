set(CHESS_PARTS
  rook.asc
  pawn.asc
  )

foreach(piece ${CHESS_PARTS})
  ADD_G_TARGET(${piece} "BRL-CAD Geometry Models/Sample/Chess")
endforeach(piece ${CHESS_PARTS})

CMAKEFILES(${CHESS_PARTS})

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
