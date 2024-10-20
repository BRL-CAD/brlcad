set(
  CHESS_PARTS
  bishop.asc
  chessboard.asc
  king.asc
  knight.asc
  pawn.asc
  queen.asc
  rook.asc
)

foreach(piece ${CHESS_PARTS})
  g_target(${piece} "BRL-CAD Geometry Models/Sample/Chess")
endforeach(piece ${CHESS_PARTS})

cmakefiles(
  ${CHESS_PARTS}
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
