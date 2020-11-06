set(DAI_HDRS
	sdaiApplication_instance_set.h
	sdaiBinary.h
	sdaiDaObject.h
	sdaiEntity_extent.h
	sdaiEntity_extent_set.h
	sdaiEnum.h
	sdaiModel_contents.h
	sdaiModel_contents_list.h
	sdaiObject.h
	sdaiSession_instance.h
	sdaiString.h
	)

install(FILES ${DAI_HDRS}
  DESTINATION ${INCLUDE_DIR}/stepcode/dai)


