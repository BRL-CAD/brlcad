set(EDITOR_HDRS
	STEPfile.h
	SdaiHeaderSchema.h
	SdaiHeaderSchemaClasses.h
	SdaiSchemaInit.h
	cmdmgr.h
	editordefines.h
	seeinfodefault.h
	)

install(FILES ${EDITOR_HDRS}
  DESTINATION ${INCLUDE_DIR}/stepcode/editor)


