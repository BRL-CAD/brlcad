set(MANIFOLD_HDRS
  manifold/cross_section.h
  manifold/manifold.h
  manifold/public.h
  manifold/sdf.h
  )

if (ASSETIMPORT_LIBRARY)
  set(MANIFOLD_HDRS
    ${MANIFOLD_HDRS}
    ../meshIO/include/meshIO.h
    )
endif (ASSETIMPORT_LIBRARY)

install(FILES ${MANIFOLD_HDRS}
    DESTINATION ${INCLUDE_DIR}/manifold
  )


# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
