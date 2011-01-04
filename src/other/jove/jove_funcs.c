/*
 *			J O V E _ F U N C S . C
 *
 * $Revision$
 *
 * $Log$
 * Revision 14.1  2004/11/16 19:42:20  morrison
 * dawn of a new revision.  it shall be numbered 14 to match release 7.  begin the convergence by adding emacs/vi local variable footer blocks to encourage consistent formatting.
 *
 * Revision 1.1  2004/05/20 14:49:59  morrison
 * Sources that are external to BRL-CAD are moved from the top level to src/other/.
 *
 * Revision 11.5  2000/08/24 23:12:23  mike
 *
 * lint, RCSid
 *
 * Revision 11.4  1998/08/23  00:45:53  mike
 * xterm arrow keys use a different prefix than the regular vt100 ones.
 * Handle it.
 *
 * Revision 11.3  1997/01/03  17:42:17  jra
 * Mods for Irix 6.2
 *
 * Revision 11.2  1995/06/21  03:41:04  gwyn
 * Eliminated trailing blanks.
 * WriteMacs() now uses Dfltmode instead of hard-wired 0644.
 * Changed memcpy calls back to bcopy.
 *
 * Revision 11.1  95/01/04  10:35:13  mike
 * Release_4.4
 *
 * Revision 10.4  94/09/17  04:57:35  butler
 * changed all calls to bcopy to be memcpy instead.  Useful for Solaris 5.2
 *
 * Revision 10.3  1993/12/10  04:25:54  mike
 * Added FindCursorTag(), bound to M-t.
 *
 * Revision 10.2  93/10/26  03:47:08  mike
 * ANSI C
 *
 * Revision 10.1  91/10/12  06:53:58  mike
 * Release_4.0
 *
 * Revision 2.3  91/08/30  19:41:36  mike
 * Added ^Xe to run cake, like ^X^E runs make.
 *
 * Revision 2.2  91/08/30  17:54:33  mike
 * Changed #include directives to distinguish between local and system header
 * files.
 *
 * Revision 2.1  91/08/30  17:49:06  mike
 * Paul Stay mods for ANSI C
 *
 * Revision 2.0  84/12/26  16:46:12  dpk
 * System as distributed to Berkeley 26 Dec 84
 *
 * Revision 1.2  83/12/16  00:08:08  dpk
 * Added distinctive RCS header
 *
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$";
#endif

/*
   Jonathan Payne at Lincoln-Sudbury Regional High School 4-19-83

   jove_funcs.c

   This is like the second half of jove_extend.c.  It was divided
   because the file was taking too long to compile ...
 */
#include "./jove.h"

extern	int Dfltmode;

static int	nfuncs = 0,
		nmacros = 0,
		nvars = 0;

struct function	functions[NFUNCS],	/* Wired functions */
		macros[NMACROS],	/* Macros */
		variables[NVARS];	/* Variables */

void
tmany(str)
char	*str;
{
	complain("too many %s", str);
}

void
DefFunc(name, func)
char	*name;
int	(*func)();
{
	if (nfuncs >= NFUNCS)
		tmany("functions");
	functions[nfuncs].f_name = name;
	functions[nfuncs].f.Func = func;
	functions[nfuncs].f_type = FUNCTION;
	nfuncs++;
}

void
DefVar(name, varptr)
char	*name;
int	*varptr;
{
	if (nvars >= NVARS)
		tmany("variables");
	variables[nvars].f_name = name;
	variables[nvars].f.Var = varptr;
	variables[nvars].f_type = VARIABLE;
	nvars++;
}

void
DefMac(name, macptr)
char	*name;
struct macro	*macptr;
{
	register int	i;

	for (i = 0; i < nmacros; i++)
		if (strcmp(macros[i].f_name, name) == 0) {
			macros[i].f.Macro = macptr;
			return;
		}

	if (nmacros > NVARS)
		tmany("macros");
	macros[nmacros].f_name = name;
	macros[nmacros].f.Macro = macptr;
	macros[nmacros].f_type = MACRO;
	nmacros++;
}

extern int
	EscPrefix(),
	CtlxPrefix(),
	AppReg(),
	Apropos(),
	Bparen(),
	BackWord(),
	Bof(),
	Bol(),
	Bos(),
	Bow(),
	BindAKey(),
	BindMac(),
	BufPos(),
	UnBound(),
	CasRegLower(),
	CasRegUpper(),
	CapChar(),
	CapWord(),
	LowWord(),
	UppWord(),
	ChrToOct(),
	ClAndRedraw(),
	MakeErrors(),
	CakeErrors(),
	CopyRegion(),
	CTab(),
	DelBlnkLines(),
	DelNChar(),
	DelNWord(),
	OneWindow(),
	DelPChar(),
	DelPWord(),
	DelReg(),
	DelWtSpace(),
	DelCurWindow(),
	KeyDesc(),
	DescCom(),
	Eof(),
	Eos(),
	Eow(),
	BufErase(),
	Extend(),
	RunMacro(),
	Leave(),
	RegToShell(),
	FindFile(),
	WindFind(),
	FindTag(),
	FindCursorTag(),
	ToIndent(),
	Fparen(),
	ForWord(),
	FourTime(),
	GoLine(),
	GrowWindow(),
	IncFSearch(),
	IncRSearch(),
	InsFile(),
	Justify(),
	BufKill(),
	KillEOL(),
	BufList(),
	NotModified(),
	Newline(),
	LineAI(),
	NextPage(),
	NextWindow(),
	OverWrite(),
	PopMark(),
	PageNWind(),
	DoParen(),
	CParse(),
	LintParse(),
	PauseJove(),	/* Same as SubShell if no Job-Control */
	SubShell(),
	PrevPage(),
	PrevWindow(),
	QRepSearch(),
	QuotChar(),
	ReadFile(),
	RedrawDisplay(),
	ReInitTTY(),
	RepSearch(),
	DownScroll(),
	UpScroll(),
	ForSearch(),
	RevSearch(),
	SelBuf(),
	SelfInsert(),
	SetVar(),
	SetMark(),
	ShellCom(),
	ShToBuf(),
	ShrWindow(),
	Source(),
	SpellCom(),
	SplitWind(),
	StrLength(),
	TextInsert(),
	TransChar(),
	UnBound(),
	VtKeys(),
	SaveFile(),
	WtModBuf(),
	WriteFile(),
	WrtReg(),
	Yank(),
	YankPop(),
	PrVar(),
	SetQchars(),
	WNumLines();

void
InitFuncs()
{
	DefFunc("Prefix-1", EscPrefix);
	DefFunc("Prefix-2", CtlxPrefix);
	DefFunc("append-region", AppReg);
	DefFunc("apropos", Apropos);
	DefFunc("backward-char", (int (*)())BackChar);
	DefFunc("backward-paren", Bparen);
	DefFunc("backward-word", BackWord);
	DefFunc("beginning-of-file", Bof);
	DefFunc("beginning-of-line", Bol);
	DefFunc("beginning-of-sentence", Bos);
	DefFunc("beginning-of-window", Bow);
	DefFunc("bind-to-key", BindAKey);
	DefFunc("bind-macro-to-key", BindMac);
	DefFunc("buffer-position", BufPos);
	DefFunc("case-char-upper", CapChar);
	DefFunc("case-region-lower", CasRegLower);
	DefFunc("case-region-upper", CasRegUpper);
	DefFunc("case-word-capitalize", CapWord);
	DefFunc("case-word-lower", LowWord);
	DefFunc("case-word-upper", UppWord);
	DefFunc("char-to-octal-insert", ChrToOct);
	DefFunc("clear-and-redraw", ClAndRedraw);
	DefFunc("make", MakeErrors);
	DefFunc("cake", CakeErrors);
	DefFunc("copy-region", CopyRegion);
	DefFunc("c-tab", CTab);
	DefFunc("delete-blank-lines-around-point", DelBlnkLines);
	DefFunc("delete-next-char", DelNChar);
	DefFunc("delete-next-word", DelNWord);
	DefFunc("delete-other-windows", OneWindow);
	DefFunc("delete-previous-char", DelPChar);
	DefFunc("delete-previous-word", DelPWord);
	DefFunc("delete-to-killbuffer", DelReg);
	DefFunc("delete-white-space", DelWtSpace);
	DefFunc("delete-current-window", DelCurWindow);
	DefFunc("describe-key", KeyDesc);
	DefFunc("describe-command", DescCom);
	DefFunc("end-of-file", Eof);
	DefFunc("end-of-line", (int (*)())Eol);
	DefFunc("end-of-sentence", Eos);
	DefFunc("end-of-window", Eow);
	DefFunc("erase-buffer", BufErase);
	DefFunc("exchange-point-and-mark", (int (*)())PtToMark);
	DefFunc("execute-extended-command", Extend);
	DefFunc("execute-keyboard-macro", (int (*)())ExecMacro);
	DefFunc("execute-macro", RunMacro);
	DefFunc("exit-jove", Leave);
	DefFunc("filter-region", RegToShell);
	DefFunc("find-file", FindFile);
	DefFunc("find-file-in-other-window", WindFind);
	DefFunc("find-tag", FindTag);
	DefFunc("find-cursor-tag", FindCursorTag);
	DefFunc("first-non-blank", ToIndent);
	DefFunc("forward-char", (int (*)())ForChar);
	DefFunc("forward-paren", Fparen);
	DefFunc("forward-word", ForWord);
	DefFunc("four-times", FourTime);
	DefFunc("goto-line", GoLine);
	DefFunc("grow-window", GrowWindow);
	DefFunc("i-search-forward", IncFSearch);
	DefFunc("i-search-reverse", IncRSearch);
	DefFunc("insert-file", InsFile);
	DefFunc("init-bindings", (int (*)())InitBindings);
	DefFunc("justify-paragraph", Justify);
	DefFunc("kill-buffer", BufKill);
	DefFunc("kill-to-end-of-line", KillEOL);
	DefFunc("list-buffers", BufList);
	DefFunc("make-buffer-unmodified", NotModified);
	DefFunc("name-keyboard-macro", (int (*)())NameMac);
	DefFunc("newline", Newline);
	DefFunc("newline-and-backup", (int (*)())OpenLine);
	DefFunc("newline-and-indent", LineAI);
	DefFunc("next-error", (int (*)())NextError);
	DefFunc("next-line", (int (*)())NextLine);
	DefFunc("next-page", NextPage);
	DefFunc("next-window", NextWindow);
	DefFunc("number-lines-in-window", WNumLines);
	DefFunc("replace-char", OverWrite);
	DefFunc("page-next-window", PageNWind);
	DefFunc("paren-flash", DoParen);
	DefFunc("parse-C-errors", CParse);
	DefFunc("parse-LINT-errors", LintParse);
	DefFunc("pause-jove", PauseJove);
	DefFunc("pop-mark", PopMark);
	DefFunc("previous-line", (int (*)())PrevLine);
	DefFunc("previous-page", PrevPage);
	DefFunc("previous-window", PrevWindow);
	DefFunc("print", PrVar);
	DefFunc("query-replace-search", QRepSearch);
	DefFunc("quote-char", QuotChar);
	DefFunc("read-file", ReadFile);
	DefFunc("read-macros-from-file", (int (*)())ReadMacs);
	DefFunc("redraw-display", RedrawDisplay);
	DefFunc("reinitialize-terminal", ReInitTTY);
	DefFunc("replace-search", RepSearch);
	DefFunc("ring-the-bell", (int (*)())Beep);
	DefFunc("scroll-down", DownScroll);
	DefFunc("scroll-up", UpScroll);
	DefFunc("search-forward", ForSearch);
	DefFunc("search-reverse", RevSearch);
	DefFunc("select-buffer", SelBuf);
	DefFunc("self-insert", SelfInsert);
	DefFunc("set", SetVar);
	DefFunc("set-mark", SetMark);
	DefFunc("set-quote-characters", SetQchars);
	DefFunc("shell-command", ShellCom);
	DefFunc("shell-command-to-buffer", ShToBuf);
	DefFunc("shrink-window", ShrWindow);
	DefFunc("source", Source);
	DefFunc("spell-buffer", SpellCom);
	DefFunc("split-current-window", SplitWind);
	DefFunc("start-remembering", (int (*)())Remember);
	DefFunc("stop-remembering", (int (*)())Forget);		/* Not bad */
	DefFunc("string-length", StrLength);
	DefFunc("sub-shell", SubShell);
	DefFunc("suspend-jove", PauseJove);
	DefFunc("text-insert", TextInsert);
	DefFunc("transpose-chars", TransChar);
	DefFunc("unbound", UnBound);
	DefFunc("vt100-arrow-keys", VtKeys);
	DefFunc("write-current-file", SaveFile);
	DefFunc("write-macros-to-file", (int (*)())WriteMacs);
	DefFunc("write-modified-files", WtModBuf);
	DefFunc("write-named-file", WriteFile);
	DefFunc("write-region", WrtReg);
	DefFunc("yank", Yank);
	DefFunc("yank-pop", YankPop);
	DefFunc((char *) 0, (int (*)()) 0);

	DefVar("allow-^S-and-^Q", &OKXonXoff);
	DefVar("auto-indent", &globflags[AUTOIND]);
	DefVar("backup-files", &BackupFiles);
	DefVar("c-mode", &globflags[CMODE]);
	DefVar("case-independent-search", &globflags[CASEIND]);
	DefVar("fast-prompt", &FastPrompt);
	DefVar("files-should-end-with-newline", &EndWNewline);
	DefVar("internal-tabstop", &tabstop);
	DefVar("make-all-at-once", &MakeAll);
	DefVar("over-write", &globflags[OVERWRITE]);
	DefVar("physical-tabstop", &phystab);
	DefVar("regular-expressions", &globflags[MAGIC]);
	DefVar("right-margin", &RMargin);
	DefVar("show-match", &globflags[MATCHING]);
	DefVar("scroll-step", &ScrollStep);
	DefVar("text-fill", &globflags[TEXTFILL]);
	DefVar("use-temporary-buffers", &UseBuffers);
	DefVar("visible-bell", &VisBell);
	DefVar("write-files-on-make", &WtOnMk);
	DefVar((char *) 0, (int *) 0);
}


struct function *
FindFunc(func)
register int 	(*func)();
{
	register struct function	*fp;

  	for (fp = functions; fp->f_name; fp++)
		if (fp->f_type == FUNCTION && fp->f.Func == func)
			return fp;
	return 0;
}

void
BindFunc(map, letter, func)
struct function	*map[];
int	(*func)();
{
	static struct function	*fp = 0;

	if (!fp || fp->f.Func != func)
		fp = FindFunc(func);
	map[letter] = fp;
}

void
ZeroMap(map)
struct function	*map[];
{
	int	i;

	for (i = 0; i < 0200; i++)
		map[i] = 0;
}

void
BindInserts(func)
FUNC	func;
{
	register int	i;

	for (i = ' '; i < 0177; i++)
		BindFunc(mainmap, i, func);
	BindFunc(mainmap, CTL('I'), func);
}

void
InitBindings()
{
	/* Most just insert themselves */
	BindInserts(SelfInsert);

	BindFunc(mainmap, CTL('@'), SetMark);
	BindFunc(mainmap, CTL('A'), Bol);
	BindFunc(mainmap, CTL('B'), (int (*)())BackChar);
	BindFunc(mainmap, CTL('C'), CapChar);
	BindFunc(mainmap, CTL('D'), DelNChar);
	BindFunc(mainmap, CTL('E'), (int (*)())Eol);
	BindFunc(mainmap, CTL('F'), (int (*)())ForChar);
	BindFunc(mainmap, CTL('G'), (int (*)())Beep);
	BindFunc(mainmap, CTL('H'), DelPChar);
	BindFunc(mainmap, CTL('I'), CTab);
	BindFunc(mainmap, CTL('J'), LineAI);
	BindFunc(mainmap, CTL('K'), KillEOL);
	BindFunc(mainmap, CTL('L'), ClAndRedraw);
	BindFunc(mainmap, CTL('M'), Newline);
	BindFunc(mainmap, CTL('N'), (int (*)())NextLine);
	BindFunc(mainmap, CTL('O'), (int (*)())OpenLine);
	BindFunc(mainmap, CTL('P'), (int (*)())PrevLine);
	BindFunc(mainmap, CTL('Q'), QuotChar);
	BindFunc(mainmap, CTL('^'), QuotChar);
	BindFunc(mainmap, CTL('R'), IncRSearch);
	BindFunc(mainmap, CTL('S'), IncFSearch);
	BindFunc(mainmap, CTL('\\'), IncFSearch);
	BindFunc(mainmap, CTL('T'), TransChar);
	BindFunc(mainmap, CTL('U'), FourTime);
	BindFunc(mainmap, CTL('V'), NextPage);
	BindFunc(mainmap, CTL('W'), DelPWord);
	BindFunc(mainmap, CTL('X'), CtlxPrefix);
	BindFunc(mainmap, CTL('Y'), Yank);
	BindFunc(mainmap, CTL('Z'), UpScroll);
	BindFunc(mainmap, CTL('['), EscPrefix);
	BindFunc(mainmap, ')', DoParen);
	BindFunc(mainmap, '}', DoParen);
	BindFunc(mainmap, '\177', DelPChar);

	/* Escape Prefix */

	BindFunc(pref1map, CTL('B'), Bparen);
	BindFunc(pref1map, CTL('E'), RepSearch);
	BindFunc(pref1map, CTL('F'), Fparen);
	BindFunc(pref1map, CTL('H'), DelPWord);
	BindFunc(pref1map, CTL('L'), RedrawDisplay);
	BindFunc(pref1map, CTL('V'), PageNWind);
	BindFunc(pref1map, '<', Bof);
	BindFunc(pref1map, '>', Eof);
	BindFunc(pref1map, '?', DescCom);
	BindFunc(pref1map, 'a', Bos);
	BindFunc(pref1map, 'A', Bos);
	BindFunc(pref1map, 'B', BackWord);
	BindFunc(pref1map, 'b', BackWord);
	BindFunc(pref1map, 'C', CapWord);
	BindFunc(pref1map, 'c', CapWord);
	BindFunc(pref1map, 'D', DelNWord);
	BindFunc(pref1map, 'd', DelNWord);
	BindFunc(pref1map, 'E', Eos);
	BindFunc(pref1map, 'e', Eos);
	BindFunc(pref1map, 'F', ForWord);
	BindFunc(pref1map, 'f', ForWord);
	BindFunc(pref1map, 'G', GoLine);
	BindFunc(pref1map, 'g', GoLine);
	BindFunc(pref1map, 'H', Apropos);
	BindFunc(pref1map, 'h', Apropos);
	BindFunc(pref1map, 'J', Justify);
	BindFunc(pref1map, 'j', Justify);
	BindFunc(pref1map, 'L', LowWord);
	BindFunc(pref1map, 'l', LowWord);
	BindFunc(pref1map, 'M', ToIndent);
	BindFunc(pref1map, 'm', ToIndent);
	BindFunc(pref1map, 'P', PauseJove);
	BindFunc(pref1map, 'p', PauseJove);
	BindFunc(pref1map, 'Q', QRepSearch);
	BindFunc(pref1map, 'q', QRepSearch);
	BindFunc(pref1map, 'R', RevSearch);
	BindFunc(pref1map, 'r', RevSearch);
	BindFunc(pref1map, 'S', ForSearch);
	BindFunc(pref1map, 's', ForSearch);
	BindFunc(pref1map, 'T', FindCursorTag);
	BindFunc(pref1map, 't', FindCursorTag);
	BindFunc(pref1map, 'U', UppWord);
	BindFunc(pref1map, 'u', UppWord);
	BindFunc(pref1map, 'V', PrevPage);
	BindFunc(pref1map, 'v', PrevPage);
	BindFunc(pref1map, 'W', CopyRegion);
	BindFunc(pref1map, 'w', CopyRegion);
	BindFunc(pref1map, 'X', Extend);
	BindFunc(pref1map, 'x', Extend);
	BindFunc(pref1map, 'Y', YankPop);
	BindFunc(pref1map, 'y', YankPop);
	BindFunc(pref1map, 'Z', DownScroll);
	BindFunc(pref1map, 'z', DownScroll);
	BindFunc(pref1map, '[', VtKeys);	/* Vt100 arrows */
	BindFunc(pref1map, 'O', VtKeys);	/* xterm "application" arrow keys */
	BindFunc(pref1map, '!', SubShell);
	BindFunc(pref1map, '\177', DelPWord);
	BindFunc(pref1map, '\\', DelWtSpace);
	BindFunc(pref1map, '~', NotModified);
	BindFunc(pref1map, '.', Eow);
	BindFunc(pref1map, ',', Bow);

	/* CTLX' 'prefix */

	ZeroMap(pref2map);
	BindFunc(pref2map, CTL('B'), BufList);
	BindFunc(pref2map, CTL('C'), Leave);
	BindFunc(pref2map, CTL('E'), MakeErrors);
	BindFunc(pref2map, CTL('F'), FindFile);
	BindFunc(pref2map, CTL('I'), InsFile);
	BindFunc(pref2map, CTL('K'), DelReg);
	BindFunc(pref2map, CTL('L'), CasRegLower);
	BindFunc(pref2map, CTL('M'), WtModBuf);
	BindFunc(pref2map, CTL('N'), (int (*)())NextError);
	BindFunc(pref2map, CTL('O'), DelBlnkLines);
	BindFunc(pref2map, CTL('R'), ReadFile);
	BindFunc(pref2map, CTL('S'), SaveFile);
	BindFunc(pref2map, CTL('T'), FindTag);
	BindFunc(pref2map, CTL('U'), CasRegUpper);
	BindFunc(pref2map, CTL('V'), FindFile);
	BindFunc(pref2map, CTL('W'), WriteFile);
	BindFunc(pref2map, CTL('X'), (int (*)())PtToMark);
	BindFunc(pref2map, CTL('\\'), SaveFile);
	BindFunc(pref2map, '?', KeyDesc);
	BindFunc(pref2map, '1', OneWindow);
	BindFunc(pref2map, '2', SplitWind);
	BindFunc(pref2map, '4', WindFind);
	BindFunc(pref2map, 'B', SelBuf);
	BindFunc(pref2map, 'b', SelBuf);
	BindFunc(pref2map, 'c', StrLength);
	BindFunc(pref2map, 'C', StrLength);
	BindFunc(pref2map, 'D', DelCurWindow);
	BindFunc(pref2map, 'd', DelCurWindow);
	BindFunc(pref2map, 'e', CakeErrors);
	BindFunc(pref2map, 'K', BufKill);
	BindFunc(pref2map, 'k', BufKill);
	BindFunc(pref2map, 'N', NextWindow);
	BindFunc(pref2map, 'n', NextWindow);
	BindFunc(pref2map, 'O', PrevWindow);
	BindFunc(pref2map, 'o', PrevWindow);
	BindFunc(pref2map, 'P', PrevWindow);
	BindFunc(pref2map, 'p', PrevWindow);
	BindFunc(pref2map, 'T', FindTag);
	BindFunc(pref2map, 't', FindTag);
	BindFunc(pref2map, '!', ShellCom);
	BindFunc(pref2map, '^', GrowWindow);
}

char *
FuncName()
{
	static char	func[60];

	sprintf(func, ": %s ", LastFunc->f_name);
	return func;
}

int	Interactive;	/* True when we invoke with the command handler? */

void
ExecFunc(fp, interactive)
struct function	*fp;
{
	int	bindinteract = Interactive;

	Interactive = interactive;
	LastFunc = fp;
	switch (fp->f_type) {
	case MACRO:
		DoMacro(fp->f.Macro);
		break;

	case FUNCTION:
		if (fp->f.Func)
			(*fp->f.Func)();
	}
	Interactive = bindinteract;
}

/* Can only define the keyboard macro.  IT can be renamed to another
 * macro.  If the keyboard macro is renamed, we make a copy of it.
 * The keyboard macro is ALWAYS the keyboard macro.
 *
 * We can't define or run the same macro recursively.  Each macro has
 * a bit saying whether or not it is currently being executed/defined.
 */

struct macro	KeyMacro;	/* Macro used for defining */

struct macro	*macstack[NMACROS];
int	stackp = 0;

char	*rem = "remember";

void
FixMacros()
{
	register int	i;
	struct macro	*mp;

	for (i = 0; macstack[i]; i++) {
		mp = macstack[i];
		macstack[i] = 0;
		mp->Flags = mp->Offset = 0;
	}
	stackp = -1;
	KeyMacro.Flags = KeyMacro.Offset = 0;
}

void
MacErr(name1, name2)
char	*name1,
	*name2;
{
	KeyMacro.Flags = 0;
	MacNolen(&KeyMacro);
	complain("Can't %s recursively; no longer %sing", name1, name2);
}

void
Remember()
{
	if (KeyMacro.Flags & DEFINE)
		MacErr(rem, rem);
	KeyMacro.Flags |= DEFINE;
	MacNolen(&KeyMacro);
	message("Remembering...");
}

/* Is `c' a prefix character */

PrefChar(c)
{
	return (mainmap[c]->f.Func == EscPrefix ||
			mainmap[c]->f.Func == CtlxPrefix);
}

void
Forget()
{
	char	*cp;
	struct macro	*m = &KeyMacro;

	if (m->Flags & DEFINE) {
		message("Keyboard macro defined");
		m->Flags &= ~DEFINE;
		cp = &m->Body[m->MacLength - 2];
		if (PrefChar(*cp))
			m->MacLength -= 2;
		else if (functions[*++cp].f.Func == (int (*)())Forget)
			m->MacLength--;
	}
}

void
ExecMacro()
{
	DoMacro(&KeyMacro);
}

void
DoMacro(mac)
struct macro	*mac;
{
	if (mac->Flags & DEFINE)
		MacErr("define", rem);
	if (mac->Flags & EXECUTE)
		MacErr("execute", "execut");
	if (++stackp >= NMACROS)
		complain("The impossible with macros just happened");
	macstack[stackp] = mac;
	mac->Offset = 0;
	mac->Ntimes = jove_exp;
	mac->Flags |= EXECUTE;
}

char *
copystr(str)
char	*str;
{
	char	*val = emalloc(strlen(str) + 1);

	strcpy(val, str);
	return val;
}

struct macro *
macexists(name)
char	*name;
{
	register int	i;

	for (i = 0; i < nmacros; i++)
		if (strcmp(macros[i].f.Macro->Name, name) == 0)
			return macros[i].f.Macro;
	return 0;
}

void
NameMac()
{
	char	*name;
	struct macro	*m;

	if (KeyMacro.MacLength == 0)
		complain("Define it first!");
	if (KeyMacro.Flags & (DEFINE | EXECUTE))
		complain("Can't name while defining/executing");
	if ((m = macexists(name = ask((char *) 0, FuncName()))) == 0)
		m = (struct macro *) emalloc(sizeof *m);
	else {
		if (strcmp(name, KeyMacro.Name) == 0)
			complain("Can't name it that!");
		free(m->Name);
		free(m->Body);
	}
	name = copystr(name);
	*m = KeyMacro;				/* Copy the keyboard macro */
	m->Body = emalloc(m->MacBuflen);
	bcopy(KeyMacro.Body, m->Body, m->MacLength);
	m->Ntimes = m->Flags = m->Offset = 0;	/* At the beginning */
	m->Name = name;
	DefMac(name, m);
}

void
MacInit()
{
	SetMacro(&KeyMacro, "keyboard-macro");
	FixMacros();
}

void
MacNolen(m)
struct macro	*m;
{
	m->MacLength = m->Offset = 0;
}

void
SetMacro(m, name)
struct macro	*m;
char	*name;
{
	DefMac(name, m);	/* Try this before mallocing memory */
	MacNolen(m);
	m->Name = name;
	m->MacBuflen = 16;
	m->Body = emalloc(m->MacBuflen);
	m->Ntimes = m->Flags = 0;
}

void
MacPutc(c)
int	c;
{
	if (KeyMacro.MacLength >= KeyMacro.MacBuflen) {
		KeyMacro.Body = realloc(KeyMacro.Body, (unsigned) KeyMacro.MacLength + 16);
		if (KeyMacro.Body == 0)
			MacErr("malloc", rem);
		KeyMacro.MacBuflen += 16;
	}
	KeyMacro.Body[KeyMacro.Offset++] = c;
	KeyMacro.MacLength++;
}

MacGetc()
{
	if (macstack[stackp]->Offset == macstack[stackp]->MacLength) {
		macstack[stackp]->Offset = 0;
		if (--macstack[stackp]->Ntimes == 0) {
			macstack[stackp]->Flags &= ~EXECUTE;
			stackp--;
		}
		return (*Getchar)();
	}
	return macstack[stackp]->Body[macstack[stackp]->Offset++];
}

int
RunMacro()
{
	int	com;

	com = findcom(macros, FuncName());
	if (com < 0)
		return 0;
	DoMacro(macros[com].f.Macro);
	return 0;
}

void
WriteMacs()
{
	struct function	*mp;
	struct macro	*m;
	int	macfd,
		namelen;
	char	*file;

	file = ask((char *) 0, FuncName());
	if ((macfd = creat(file, Dfltmode)) == -1)
		complain(IOerr("create", file));
	/* Don't write the keyboard macro which is always the first */
	for (mp = &macros[1]; mp < &macros[NMACROS]; mp++) {
		m = mp->f.Macro;
		if (m == 0 || m->MacLength == 0)
			continue;
		ignore(write(macfd, (char *) &m->MacLength, sizeof m->MacLength));
		namelen = strlen(m->Name) + 1;	/* Including the null */
		ignore(write(macfd, (char *) &namelen, sizeof namelen));
		ignore(write(macfd, m->Name, namelen));
		ignore(write(macfd, m->Body, m->MacLength));
	}
	ignore(close(macfd));
}

void
ReadMacs()
{
	char	*file;
	struct macro	*m;
	int	macfd,
		namelen,
		bodylen;

	file = ask((char *) 0, FuncName());
	if ((macfd = open(file, 0)) == -1)
		complain(IOerr("open", file));

	while (nmacros < NMACROS) {
		if (read(macfd, (char *) &bodylen, sizeof m->MacLength) == 0)
			break;
		m = (struct macro *) emalloc (sizeof *m);
		m->MacLength = bodylen;
		m->MacBuflen = (m->MacLength + 16) & ~017;
		ignore(read(macfd, (char *) &namelen, sizeof namelen));
		m->Name = emalloc(namelen);
		ignore(read(macfd, m->Name, namelen));
		m->Body = emalloc(m->MacBuflen);
		ignore(read(macfd, m->Body, m->MacLength));
		DefMac(m->Name, m);
	}
	ignore(close(macfd));
}
