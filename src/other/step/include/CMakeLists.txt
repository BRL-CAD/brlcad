SET(express_HDRS
  express/alg.h
  express/basic.h
  express/caseitem.h
  express/de_end.h
  express/decstart.h
  express/defstart.h 
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
install(FILES ${express_HDRS} DESTINATION include/express)

SET(exppp_HDRS
  exppp/exppp.h
)
install(FILES ${exppp_HDRS} DESTINATION include/exppp)

SET(SCL_noinst_HEADERS
  sclprefixes.h 
)
