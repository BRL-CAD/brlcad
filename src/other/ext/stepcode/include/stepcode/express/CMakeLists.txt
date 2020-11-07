set(EXPRESS_HDRS
	alg.h
	basic.h
	caseitem.h
	de_end.h
	decstart.h
	defstart.h
	dict.h
	entity.h
	error.h
	expbasic.h
	exppp.h
	expr.h
	express.h
	hash.h
	lexact.h
	linklist.h
	memory.h
	object.h
	ordered_attrs.h
	resolve.h
	schema.h
	scope.h
	stmt.h
	symbol.h
	type.h
	variable.h
	)

install(FILES ${EXPRESS_HDRS}
  DESTINATION ${INCLUDE_DIR}/stepcode/express)

