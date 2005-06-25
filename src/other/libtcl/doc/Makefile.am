# $Id$

man1_MANS = tclsh.1

man3_MANS = \
	Access.3 \
	AddErrInfo.3 \
	Alloc.3 \
	AllowExc.3 \
	AppInit.3 \
	AssocData.3 \
	Async.3 \
	BackgdErr.3 \
	Backslash.3 \
	BoolObj.3 \
	ByteArrObj.3 \
	CallDel.3 \
	ChnlStack.3 \
	CmdCmplt.3 \
	Concat.3 \
	CrtChannel.3 \
	CrtChnlHdlr.3 \
	CrtCloseHdlr.3 \
	CrtCommand.3 \
	CrtFileHdlr.3 \
	CrtInterp.3 \
	CrtMathFnc.3 \
	CrtObjCmd.3 \
	CrtSlave.3 \
	CrtTimerHdlr.3 \
	CrtTrace.3 \
	DString.3 \
	DetachPids.3 \
	DoOneEvent.3 \
	DoWhenIdle.3 \
	DoubleObj.3 \
	DumpActiveMemory.3 \
	Encoding.3 \
	Environment.3 \
	Eval.3 \
	Exit.3 \
	ExprLong.3 \
	ExprLongObj.3 \
	FileSystem.3 \
	FindExec.3 \
	GetCwd.3 \
	GetHostName.3 \
	GetIndex.3 \
	GetInt.3 \
	GetOpnFl.3 \
	GetStdChan.3 \
	GetTime.3 \
	GetVersion.3 \
	Hash.3 \
	Init.3 \
	InitStubs.3 \
	IntObj.3 \
	Interp.3 \
	LinkVar.3 \
	ListObj.3 \
	Macintosh.3 \
	Notifier.3 \
	Object.3 \
	ObjectType.3 \
	OpenFileChnl.3 \
	OpenTcp.3 \
	Panic.3 \
	ParseCmd.3 \
	PkgRequire.3 \
	Preserve.3 \
	PrintDbl.3 \
	RecEvalObj.3 \
	RecordEval.3 \
	RegExp.3 \
	SaveResult.3 \
	SetErrno.3 \
	SetRecLmt.3 \
	SetResult.3 \
	SetVar.3 \
	Signal.3 \
	Sleep.3 \
	SourceRCFile.3 \
	SplitList.3 \
	SplitPath.3 \
	StaticPkg.3 \
	StdChannels.3 \
	StrMatch.3 \
	StringObj.3 \
	SubstObj.3 \
	TCL_MEM_DEBUG.3 \
	Tcl_Main.3 \
	Thread.3 \
	ToUpper.3 \
	TraceCmd.3 \
	TraceVar.3 \
	Translate.3 \
	UniCharIsAlpha.3 \
	UpVar.3 \
	Utf.3 \
	WrongNumArgs.3

mann_MANS = \
	Tcl.n \
	after.n \
	append.n \
	array.n \
	bgerror.n \
	binary.n \
	break.n \
	case.n \
	catch.n \
	cd.n \
	clock.n \
	close.n \
	concat.n \
	continue.n \
	dde.n \
	encoding.n \
	eof.n \
	error.n \
	eval.n \
	exec.n \
	exit.n \
	expr.n \
	fblocked.n \
	fconfigure.n \
	fcopy.n \
	file.n \
	fileevent.n \
	filename.n \
	flush.n \
	for.n \
	foreach.n \
	format.n \
	gets.n \
	glob.n \
	global.n \
	history.n \
	http.n \
	if.n \
	incr.n \
	info.n \
	interp.n \
	join.n \
	lappend.n \
	library.n \
	lindex.n \
	linsert.n \
	list.n \
	llength.n \
	load.n \
	lrange.n \
	lreplace.n \
	lsearch.n \
	lset.n \
	lsort.n \
	memory.n \
	msgcat.n \
	namespace.n \
	open.n \
	package.n \
	packagens.n \
	pid.n \
	pkgMkIndex.n \
	proc.n \
	puts.n \
	pwd.n \
	re_syntax.n \
	read.n \
	regexp.n \
	registry.n \
	regsub.n \
	rename.n \
	resource.n \
	return.n \
	safe.n \
	scan.n \
	seek.n \
	set.n \
	socket.n \
	source.n \
	split.n \
	string.n \
	subst.n \
	switch.n \
	tcltest.n \
	tclvars.n \
	tell.n \
	time.n \
	trace.n \
	unknown.n \
	unset.n \
	update.n \
	uplevel.n \
	upvar.n \
	variable.n \
	vwait.n \
	while.n

EXTRA_DIST = \
	$(man1_MANS) \
	$(man3_MANS) \
	$(mann_MANS) \
	license.terms \
	man.macros

install-data-hook:
	@for manpage in $(man1_MANS) ; do \
		echo "Configuring $(DESTDIR)$(mandir)/man1/$$manpage" ; \
		sed -e '/man\.macros/r $(top_srcdir)/src/other/libtcl/doc/man.macros' -e '/man\.macros/d' $(DESTDIR)$(mandir)/man1/$$manpage > $(DESTDIR)$(mandir)/man1/$$manpage.new ; \
		mv $(DESTDIR)$(mandir)/man1/$$manpage.new $(DESTDIR)$(mandir)/man1/$$manpage ; \
	done
	@for manpage in $(man3_MANS) ; do \
		echo "Configuring $(DESTDIR)$(mandir)/man3/$$manpage" ; \
		sed -e '/man\.macros/r $(top_srcdir)/src/other/libtcl/doc/man.macros' -e '/man\.macros/d' $(DESTDIR)$(mandir)/man3/$$manpage > $(DESTDIR)$(mandir)/man3/$$manpage.new ; \
		mv $(DESTDIR)$(mandir)/man3/$$manpage.new $(DESTDIR)$(mandir)/man3/$$manpage ; \
	done
	@for manpage in $(mann_MANS) ; do \
		echo "Configuring $(DESTDIR)$(mandir)/mann/$$manpage" ; \
		sed -e '/man\.macros/r $(top_srcdir)/src/other/libtcl/doc/man.macros' -e '/man\.macros/d' $(DESTDIR)$(mandir)/mann/$$manpage > $(DESTDIR)$(mandir)/mann/$$manpage.new ; \
		mv $(DESTDIR)$(mandir)/mann/$$manpage.new $(DESTDIR)$(mandir)/mann/$$manpage ; \
	done

MAINTAINERCLEANFILES = Makefile.in

include $(top_srcdir)/misc/Makefile.defs

