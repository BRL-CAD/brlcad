IF(NOT MAN_DIR)
	SET(MAN_DIR "man")
ENDIF(NOT MAN_DIR)

FIND_PROGRAM(SH_EXEC NAMES sh dash bash DOC "path to shell executable")
FIND_PROGRAM(SED_EXEC NAMES sed DOC "path to sed executable")
MARK_AS_ADVANCED(SH_EXEC)
MARK_AS_ADVANCED(SED_EXEC)

FILE(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/man1)
FILE(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/man3)
FILE(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/mann)

IF(SED_EXEC AND SH_EXEC AND NOT TCLMAN_GENERATED)
	MESSAGE("Generating Tcl man pages...")

	SET(tcl_MAN1
		tclsh.1
		)
	FOREACH(manfile ${tcl_MAN1})
		EXEC_PROGRAM(${SH_EXEC} ARGS ${CMAKE_CURRENT_SOURCE_DIR}/../unix/installManPage
			${CMAKE_CURRENT_SOURCE_DIR}/${manfile}
			${CMAKE_CURRENT_BINARY_DIR}/man1 OUTPUT_VARIABLE MAN1_SH)
	ENDFOREACH(manfile ${tcl_MAN1})
	
	SET(tcl_MAN3
		Access.3
		AddErrInfo.3
		Alloc.3
		AllowExc.3
		AppInit.3
		AssocData.3
		Async.3
		BackgdErr.3
		Backslash.3
		BoolObj.3
		ByteArrObj.3
		CallDel.3
		ChnlStack.3
		CmdCmplt.3
		Concat.3
		CrtChannel.3
		CrtChnlHdlr.3
		CrtCloseHdlr.3
		CrtCommand.3
		CrtFileHdlr.3
		CrtInterp.3
		CrtMathFnc.3
		CrtObjCmd.3
		CrtSlave.3
		CrtTimerHdlr.3
		CrtTrace.3
		DString.3
		DetachPids.3
		DictObj.3
		DoOneEvent.3
		DoWhenIdle.3
		DoubleObj.3
		DumpActiveMemory.3
		Encoding.3
		Ensemble.3
		Environment.3
		Eval.3
		Exit.3
		ExprLong.3
		ExprLongObj.3
		FileSystem.3
		FindExec.3
		GetCwd.3
		GetHostName.3
		GetIndex.3
		GetInt.3
		GetOpnFl.3
		GetStdChan.3
		GetTime.3
		GetVersion.3
		Hash.3
		Init.3
		InitStubs.3
		IntObj.3
		Interp.3
		Limit.3
		LinkVar.3
		ListObj.3
		Namespace.3
		Notifier.3
		Object.3
		ObjectType.3
		OpenFileChnl.3
		OpenTcp.3
		Panic.3
		ParseCmd.3
		PkgRequire.3
		Preserve.3
		PrintDbl.3
		RecEvalObj.3
		RecordEval.3
		RegConfig.3
		RegExp.3
		SaveResult.3
		SetChanErr.3
		SetErrno.3
		SetRecLmt.3
		SetResult.3
		SetVar.3
		Signal.3
		Sleep.3
		SourceRCFile.3
		SplitList.3
		SplitPath.3
		StaticPkg.3
		StdChannels.3
		StrMatch.3
		StringObj.3
		SubstObj.3
		TCL_MEM_DEBUG.3
		Tcl_Main.3
		Thread.3
		ToUpper.3
		TraceCmd.3
		TraceVar.3
		Translate.3
		UniCharIsAlpha.3
		UpVar.3
		Utf.3
		WrongNumArgs.3
		)
	FOREACH(manfile ${tcl_MAN3})
		EXEC_PROGRAM(${SH_EXEC} ARGS ${CMAKE_CURRENT_SOURCE_DIR}/../unix/installManPage
			${CMAKE_CURRENT_SOURCE_DIR}/${manfile}
			${CMAKE_CURRENT_BINARY_DIR}/man3 OUTPUT_VARIABLE MAN3_SH)
	ENDFOREACH(manfile ${tcl_MAN3})

	SET(tcl_MANN
		Tcl.n
		after.n
		append.n
		apply.n
		array.n
		bgerror.n
		binary.n
		break.n
		case.n
		catch.n
		cd.n
		chan.n
		clock.n
		close.n
		concat.n
		continue.n
		dde.n
		dict.n
		encoding.n
		eof.n
		error.n
		eval.n
		exec.n
		exit.n
		expr.n
		fblocked.n
		fconfigure.n
		fcopy.n
		file.n
		fileevent.n
		filename.n
		flush.n
		for.n
		foreach.n
		format.n
		gets.n
		glob.n
		global.n
		history.n
		http.n
		if.n
		incr.n
		info.n
		interp.n
		join.n
		lappend.n
		lassign.n
		library.n
		lindex.n
		linsert.n
		list.n
		llength.n
		load.n
		lrange.n
		lrepeat.n
		lreplace.n
		lreverse.n
		lsearch.n
		lset.n
		lsort.n
		mathfunc.n
		mathop.n
		memory.n
		msgcat.n
		namespace.n
		open.n
		package.n
		packagens.n
		pid.n
		pkgMkIndex.n
		platform.n
		platform_shell.n
		proc.n
		puts.n
		pwd.n
		re_syntax.n
		read.n
		refchan.n
		regexp.n
		registry.n
		regsub.n
		rename.n
		return.n
		safe.n
		scan.n
		seek.n
		set.n
		socket.n
		source.n
		split.n
		string.n
		subst.n
		switch.n
		tcltest.n
		tclvars.n
		tell.n
		time.n
		tm.n
		trace.n
		unknown.n
		unload.n
		unset.n
		update.n
		uplevel.n
		upvar.n
		variable.n
		vwait.n
		while.n
		)
	FOREACH(manfile ${tcl_MANN})
		EXEC_PROGRAM(${SH_EXEC} ARGS ${CMAKE_CURRENT_SOURCE_DIR}/../unix/installManPage
			${CMAKE_CURRENT_SOURCE_DIR}/${manfile}
			${CMAKE_CURRENT_BINARY_DIR}/mann OUTPUT_VARIABLE MAN5_SH)
	ENDFOREACH(manfile ${tcl_MANN})

	SET(TCLMAN_GENERATED 1 CACHE BOOL "Flag signifying Tcl man pages are generated" FORCE)
	MARK_AS_ADVANCED(TCLMAN_GENERATED)

ENDIF(SED_EXEC AND SH_EXEC AND NOT TCLMAN_GENERATED)

IF(TCLMAN_GENERATED)
	FILE(GLOB TCL_MAN1 "${CMAKE_CURRENT_BINARY_DIR}/man1/*")
	FILE(COPY ${TCL_MAN1} DESTINATION ${CMAKE_BINARY_DIR}/${MAN_DIR}/man1)
	INSTALL(FILES ${TCL_MAN1} DESTINATION ${MAN_DIR}/man1)

	FILE(GLOB TCL_MAN3 "${CMAKE_CURRENT_BINARY_DIR}/man3/*")
	FILE(COPY ${TCL_MAN3} DESTINATION ${CMAKE_BINARY_DIR}/${MAN_DIR}/man3)
	INSTALL(FILES ${TCL_MAN3} DESTINATION ${MAN_DIR}/man3)

	FILE(GLOB TCL_MANN "${CMAKE_CURRENT_BINARY_DIR}/mann/*")
	FILE(COPY ${TCL_MANN} DESTINATION ${CMAKE_BINARY_DIR}/${MAN_DIR}/mann)
	INSTALL(FILES ${TCL_MANN} DESTINATION ${MAN_DIR}/mann)
ENDIF(TCLMAN_GENERATED)
