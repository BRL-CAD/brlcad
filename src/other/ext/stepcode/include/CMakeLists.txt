set(express_HDRS
  express/alg.h
  express/basic.h
  express/caseitem.h
  express/dict.h
  express/entity.h
  express/error.h
  express/expbasic.h
  express/expr.h
  express/express.h
  express/hash.h
  express/lexact.h
  express/linklist.h
  express/memory.h
  express/object.h
  express/resolve.h
  express/schema.h
  express/scope.h
  express/stmt.h
  express/symbol.h
  express/type.h
  express/variable.h
  )
install(FILES ${express_HDRS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/stepcode/express)

set(exppp_HDRS
  exppp/exppp.h
  )
install(FILES ${exppp_HDRS}
  DESTINATION ${INCLUDE_INSTALL_DIR}/stepcode/exppp)

install(FILES ordered_attrs.h
  sc_export.h
  sc_stdbool.h
  DESTINATION ${INCLUDE_INSTALL_DIR}/stepcode)

install(FILES ${SC_BINARY_DIR}/${INCLUDE_INSTALL_DIR}/sc_cf.h
  ${SC_BINARY_DIR}/${INCLUDE_INSTALL_DIR}/sc_version_string.h
  DESTINATION ${INCLUDE_INSTALL_DIR}/stepcode)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8

