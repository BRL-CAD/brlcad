set(CORE_HDRS
	baseType.h
	complexSupport.h
	dictdefs.h
	dispnode.h
	dispnodelist.h
	ExpDict.h
	instmgr.h
	mgrnodearray.h
	mgrnode.h
	mgrnodelist.h
	needFunc.h
	read_func.h
	Registry.h
	sdaiApplication_instance.h
	sdai.h
	sdaiSelect.h
	SingleLinkList.h
	STEPaggregate.h
	STEPattribute.h
	STEPattributeList.h
	STEPcomplex.h
	STEPundefined.h
	SubSuperIterators.h
	)

install(FILES ${CORE_HDRS}
  DESTINATION ${INCLUDE_DIR}/stepcode/core)

