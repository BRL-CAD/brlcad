
################################################################################
#               RamDebugger::Instrumenter
################################################################################

namespace eval RamDebugger::Instrumenter {
    variable stack
    variable words
    variable currentword
    variable wordtype
    variable wordtypeline
    variable wordtypepos
    variable DoInstrument
    variable OutputType
    variable NeedsNamespaceClose
    variable braceslevel
    variable snitpackageinserted

    variable level
    variable colors
}

proc RamDebugger::Instrumenter::InitState {} {
    variable stack ""
    variable words ""
    variable currentword ""
    variable wordtype ""
    variable wordtypeline ""
    variable wordtypepos ""
    # = 0 no instrument, consider data; =1 instrument; =2 special case: switch;
    # = 3 do not instrument but go inside
    variable DoInstrument 0
    variable OutputType
    variable NeedsNamespaceClose 0
    variable braceslevel 0
    variable level 0
    variable colors

    foreach i [list return break while eval foreach for if else elseif \
	    error switch default continue] {
	set colors($i) magenta
    }
    foreach i [list variable set global incr lassign] {
	set colors($i) green
    }
}

proc RamDebugger::Instrumenter::PushState { type line newblocknameP newblocknameR } {
    variable stack
    variable words
    variable currentword
    variable wordtype
    variable wordtypeline
    variable wordtypepos
    variable DoInstrument
    variable OutputType
    variable NeedsNamespaceClose
    variable braceslevel
    variable level

    set NewDoInstrument 0
    if { $OutputType == "P" } {
	set NewOutputType PP
    } else { set NewOutputType $OutputType }

    set PushState 0
    if { $type == "\[" } {
	set PushState 1
	if { $DoInstrument == 1 } {
	    set NewDoInstrument 1
	} else {
	    set NewDoInstrument 0
	}
    } else {

	if { [lindex $words 0] == "constructor" && [llength $words] == 2 } {
	    set NewDoInstrument 1
	} elseif { [lindex $words 0] == "destructor" && [llength $words] == 1 } {
	    set NewDoInstrument 1
	} elseif { [lindex $words 0] == "oncget" && [llength $words] == 2 } {
	    if { ![info exists ::RamDebugger::options(nonInstrumentingProcs)] || \
		[lsearch -exact $::RamDebugger::options(nonInstrumentingProcs) \
		[lindex $words 1]] == -1 } {
		set NewDoInstrument 1
	    }
	} elseif { [regexp {^(proc|method|typemethod|onconfigure)$} [lindex $words 0]] && \
	    [llength $words] == 3 } {
	    if { ![info exists ::RamDebugger::options(nonInstrumentingProcs)] || \
	    [lsearch -exact $::RamDebugger::options(nonInstrumentingProcs) \
		[lindex $words 1]] == -1 } {
		set NewDoInstrument 1
	    }
	} elseif { $DoInstrument == 0 } {
	    if { [regexp {^(::)?snit::(type|widget|widgetadaptor)$} [lindex $words 0]] && \
		     [llength $words] == 2 } {
		set PushState 1
	    } elseif { [lindex $words 0] == "namespace" && [lindex $words 1] == "eval" && \
		            [llength $words] >= 3 } {
		set PushState 1
#                 if { $OutputType == "R" } {
#                     upvar 2 $newblocknameP newblock
#                 } else { upvar 2 $newblocknameR newblock }
#                 append newblock "namespace eval [lindex $words 2] \{\n"
#                 set NeedsNamespaceClose 1
	    }
	} elseif { $DoInstrument == 1 } {
	    switch -glob -- [lindex $words 0] {
		"if" {
		    if { [llength $words] == 2 } {
		        set NewDoInstrument 1
		    } elseif { [lindex $words end] == "then" || [lindex $words end] == "else" } {
		        set NewDoInstrument 1
		    } elseif { [lindex $words end-1] == "elseif" } {
		        set NewDoInstrument 1
		    }
		}
		"db" {
		    if { [lindex $words 1] == "eval" && [llength $words] >= 3 } {
		        # this is not a very correct one. It assumes that db is a sqlite3 handle
		        #can fail if db is an interp
		        set NewDoInstrument 1
		    }
		}
		"dict" {
		    if { [lindex $words 1] == "for" && [llength $words] == 4 } {
		        set NewDoInstrument 1
		    }
		}
		"namespace" {
		    if { [lindex $words 1] == "eval" && [llength $words] >= 3 } {
		        set NewDoInstrument 1
		        if { $OutputType == "R" } {
		            upvar 2 $newblocknameP newblockP
		            append newblockP "[list namespace eval [lindex $words 2]] \{\n"
		            set NeedsNamespaceClose 1
		        }
		    }
		}
		"*snit::type" - "*snit::widget" - "*snit::widgetadaptor" {
		    if { [llength $words] == 2 } {
		        set NewDoInstrument 3
		        if { $OutputType == "R" } {
		            upvar 2 $newblocknameP newblockP
		            append newblockP "[lrange $words 0 1] \{\n"
		            set NeedsNamespaceClose 1
		        }
		    }
		}
		"catch" {
		    if { [llength $words] == 1 } {
		        set NewDoInstrument 1
		    }
		}
		"while" {
		    if { [llength $words] == 2 } {
		        set NewDoInstrument 1
		    }
		}
		"*foreach" - mk::loop {
		    if { [llength $words] >= 3 } {
		        set NewDoInstrument 1
		    }
		}
		"for" {
		    if { [llength $words] >= 1 && [llength $words] <= 4 } {
		        set NewDoInstrument 1
		    }
		}
		"eval" - "html::eval" {
		    set NewDoInstrument 1
		}
		"uplevel" {
		    set len [llength $words]
		    if { [regexp {[\#]?[0-9]+} [lindex $words 1]] } {
		        incr len -1
		    }
		    if { $len > 0 } {
		        set NewDoInstrument 1
		    }
		}
		"switch" {
		    for { set i 1 } { $i < [llength $words] } { incr i } {
		        if { [lindex $words $i] == "--" } {
		            incr i
		            break
		        } elseif { ![string match -* [lindex $words $i]] } { break }
		    }
		    set len [expr [llength $words]-$i]
		    if { $len == 1 } {
		        set NewDoInstrument 2
		    }
		}
		"bind" {
		    if { [llength $words] == 3 } {
		        set NewDoInstrument 1
		    }
		}
		default {
		    if { [llength $words] == 4 && [lindex $words 1] eq "sql" && \
		        [lindex $words 2] eq "maplist" } {
		        set NewDoInstrument 1
		    }
		}
	    }
	} elseif { $DoInstrument == 2 } {
	    if { [llength $words]%2 } {
		set NewDoInstrument 1
	    }
	}
    }
    if { !$PushState && !$NewDoInstrument } { return 1 }

    incr level
    lappend stack [list $words $currentword $wordtype $wordtypeline \
	$wordtypepos $DoInstrument $OutputType $NeedsNamespaceClose $braceslevel $line $type]

    set words ""
    set currentword ""
    set wordtype ""
    set wordtypeline ""
    set wordtypepos ""
    set DoInstrument $NewDoInstrument
    set OutputType $NewOutputType
    set NeedsNamespaceClose 0
    set braceslevel 0
    return 0
}

proc RamDebugger::Instrumenter::PopState { type line newblocknameP newblocknameR } {
    variable stack
    variable wordtype
    variable words
    variable currentword
    variable wordtype
    variable wordtypeline
    variable wordtypepos
    variable DoInstrument
    variable OutputType
    variable NeedsNamespaceClose
    variable braceslevel
    variable level

    set lasttype [lindex [lindex $stack end] end]
    if { $type == "\]" && $lasttype != "\[" } { return 1 }

    if { $type == "\}" } {
	if { $wordtype == "w" } {
	    set numopen 0
	    for { set i 0 } { $i < [string length $currentword] } { incr i } {
		switch -- [string index $currentword $i] {
		    "\\" { incr i }
		    \{ { incr numopen }
		    \} { incr numopen -1 }
		}
	    }
	    if { $numopen } { return 1 }
	}
	if { $lasttype != "\{" } {
	    foreach i $stack {
		if { [lindex $i end] == "\{" } {
		    set text "Using a close brace (\}) in line $line when there is an open brace "
		    append text "in line [lindex $i end-1] and an open bracket (\[) in line "
		    append text "[lindex [lindex $stack end] end-1]"
		    error $text
		}
		return 1
	    }
	} 
    }
    if { $currentword ne "" } { lappend words $currentword }
    set words_old $words
    foreach [list words currentword wordtype wordtypeline wordtypepos DoInstrument OutputType \
	NeedsNamespaceClose braceslevel] [lindex $stack end] break
    if { $words_old eq "*" } {
	lappend words "*"
    }
    set stack [lreplace $stack end end]
    incr level -1

    if { $NeedsNamespaceClose } {
	upvar 2 $newblocknameP newblockP
	append newblockP "\}\n"
	set NeedsNamespaceClose 0
    }

    return 0
}

proc RamDebugger::Instrumenter::CheckEndOfFileState {} {
    variable stack

    set text ""
    foreach i $stack {
	set line [lindex $i end-1]
	set type [lindex $i end]
	append text "There is a block of type ($type) beginning at line $line "
	append text "that is not closed at the end of the file\n"
    }
    if { $text != "" } {
	error $text
    }
}

proc RamDebugger::Instrumenter::GiveCommandUplevel {} {
    variable stack

    return [lindex [lindex [lindex $stack end] 0] 0]
}

proc RamDebugger::Instrumenter::IsProc { name } {
    if { [regexp {^(::)?snit::(type|widget|widgetadaptor)$} $name] } { return 1 }
    return [regexp {^(proc|method|typemethod|onconfigure|oncget|constructor|destructor)$} $name]
}

proc RamDebugger::Instrumenter::NeedsToInsertSnitPackage { name } {
    variable snitpackageinserted

    if { !$snitpackageinserted && [regexp {^(::)?snit::(type|widget|widgetadaptor)$} $name] } {
	set snitpackageinserted 1
	return 1
    } else { return 0 }

}

proc RamDebugger::Instrumenter::TryCompileFastInstrumenter { { raiseerror 0 } } {
    variable topdir

    if { $RamDebugger::iswince } { return }
    
    set topdir $RamDebugger::topdir
    set AppDataDir $RamDebugger::AppDataDir

    if { $::tcl_platform(machine) == "amd64" } {
	set dynlib_base RamDebuggerInstrumenter6_x64[info sharedlibextension]
    } else {
	set dynlib_base RamDebuggerInstrumenter6_x32[info sharedlibextension]
    }

    set dynlib [file join $AppDataDir $dynlib_base]

    if { [file readable $dynlib] && [file mtime $dynlib] >= \
	[file mtime [file join $topdir scripts RamDebuggerInstrumenter.cc]] } {
	catch { load $dynlib }
	if {[info commands RamDebuggerInstrumenterDoWork] ne "" } { return }
    }
    file delete -force [file join $AppDataDir compile]
    file copy -force [file join $topdir scripts RamDebuggerInstrumenter.cc] \
	[file join $topdir scripts compile] \
	$AppDataDir

    set sourcefile [file join $AppDataDir RamDebuggerInstrumenter.cc]

    set OPTS [list -shared -DUSE_TCL_STUBS -O3]
    if {$::tcl_platform(platform) ne "windows"} { lappend OPTS "-fPIC" }
    set basedir [file dirname [file dirname [info nameofexecutable]]]
    lappend OPTS -I[file join $basedir include] \
	-I[file join $AppDataDir compile]
    set libs [glob -nocomplain -dir [file join $basedir lib] *tclstub*]
    switch [llength $libs] {
	0 { set lib [file join $AppDataDir compile libtclstub.a] }
	1 { set lib [lindex $libs 0] }
	default {
	    foreach i [glob -dir [file join $basedir lib] libtclstub*.a] {
		regexp {[\d.]+} [file tail $i] version
		if { $version >= 8.5 } { break }
	    }
	    set lib $i
	}
    }
    set LOPTS [list $lib]

    set err [catch {eval exec g++ $OPTS [list $sourcefile -o $dynlib] $LOPTS} errstring]
    if { $err && $raiseerror } {
	WarnWin $::errorInfo
    }
}
    
proc RamDebugger::Instrumenter::DoWorkForTcl { block filenum newblocknameP newblocknameR blockinfoname \
		                                   "progress 1" } {
    variable FastInstrumenterLoaded

    # this variable is used to make tests
    set what c++

    switch $what {
	debug {
	    if { $::tcl_platform(machine) eq "amd64"} {
		set dynlib [file join {C:\Documents and Settings\ramsan\Mis documentos\myTclTk} \
		        RamDebugger RDIDoWork Debug RamDebuggerInstrumenterDoWork6_x64.dll]
	    } else {
		set dynlib [file join {C:\Documents and Settings\ramsan\Mis documentos\myTclTk} \
		        RamDebugger RDIDoWork Debug RamDebuggerInstrumenterDoWork6_x32.dll]
	    }
	    load $dynlib Ramdebuggerinstrumenter
	    set FastInstrumenterLoaded 1
	}
	c++ {
	    if { ![info exists FastInstrumenterLoaded] } {
		if { $::tcl_platform(machine) == "amd64"} {
		    set dynlib_base RamDebuggerInstrumenter6_x64[info sharedlibextension]
		} else {
		    set dynlib_base RamDebuggerInstrumenter6_x32[info sharedlibextension]
		}
		set dynlib [file join $RamDebugger::topdir scripts $dynlib_base]
		set err [catch { load $dynlib }]
		if { $err } {
		    set dynlib [file join $RamDebugger::AppDataDir $dynlib_base]
		}
		catch { load $dynlib }
		if { [info commands RamDebuggerInstrumenterDoWork] eq "" && \
		         $RamDebugger::options(CompileFastInstrumenter) != 0 } {
		    if { $RamDebugger::options(CompileFastInstrumenter) == -1 } {
		        TryCompileFastInstrumenter 0
		        set RamDebugger::options(CompileFastInstrumenter) 0
		    } else {
		        TryCompileFastInstrumenter 1
		    }
		    catch { load $dynlib }
		}
		set FastInstrumenterLoaded 1
	    }
	}
	tcl {
	    # nothing
	}
    }

    if { [info commands RamDebuggerInstrumenterDoWork] ne "" } {
	RamDebuggerInstrumenterDoWork $block $filenum $newblocknameP $newblocknameR \
	    $blockinfoname $progress
    } else {
	uplevel [list RamDebugger::Instrumenter::DoWork $block $filenum $newblocknameP \
		     $newblocknameR $blockinfoname $progress]
    }
}

# newblocknameP is for procs
# newblocknameR is for the rest
proc RamDebugger::Instrumenter::DoWork { block filenum newblocknameP newblocknameR blockinfoname "progress 1" } {

    variable words
    variable currentword
    variable wordtype
    variable wordtypeline
    variable DoInstrument
    variable OutputType
    variable braceslevel
    variable snitpackageinserted
    variable level
    variable colors

    set length [string length $block]
    if { $length >= 1000 && $progress } {
	RamDebugger::ProgressVar 0 1
    }

    # a trick: it is the same to use newblockP or newblockPP. Only convenience
    upvar $newblocknameP newblockP
    upvar $newblocknameP newblockPP
    upvar $newblocknameR newblockR
    upvar $blockinfoname blockinfo
    set newblockP ""
    set newblockR ""
    set blockinfo ""
    set blockinfocurrent [list 0 n]
    InitState

    set DoInstrument 1
    set OutputType R

    append newblockP "# RamDebugger instrumented file. InstrumentProcs=1\n"
    append newblockR "# RamDebugger instrumented file. InstrumentProcs=0\n"

    set snitpackageinserted 0
    set braceslevelNoEval 0
    set checkExtraCharsAfterCQB ""
    set lastc ""
    set lastinstrumentedline ""
    set line 1
    set ichar 0
    set icharline 0
    foreach c [split $block ""] {
	if { $ichar%1000 == 0 && $progress } {
	    RamDebugger::ProgressVar [expr {$ichar*100/$length}]
	}
	if { $checkExtraCharsAfterCQB != "" } {
	    if { ![string is space $c] && $c != "\}" && $c != "\]" && $c != "\\" && $c != ";" } {
		if { $c == "\"" && $checkExtraCharsAfterCQB == "\}" } {
		    # nothing
		} else {
		    set text "There is a non valid character ($c) in line $line "
		    append text "after a closing block with ($checkExtraCharsAfterCQB)"
		    error $text
		}
	    }
	    set checkExtraCharsAfterCQB ""
	}
	if { $DoInstrument == 1 && $lastinstrumentedline != $line && \
	    ![string is space $c] && \
	    $c != "\#" &&  $words == "" } {
	    if { $c != "\}" || ![IsProc [GiveCommandUplevel]] || \
		$RamDebugger::options(instrument_proc_last_line) } {
		append newblock$OutputType "RDC::F $filenum $line ; "
		set lastinstrumentedline $line
	    }
	}
	set consumed 0
	switch -- $c {
	    \" {
		if { $lastc != "\\" && [lindex $words 0] != "\#" } {
		    if { $wordtype == "" } {
		        set wordtype \"
		        set wordtypeline $line
		        set wordtypepos $icharline
		        set consumed 1
		    } elseif { $wordtype == "\"" } {
		        set wordtype ""
		        lappend words $currentword
		        set currentword ""
		        set consumed 1
		        set checkExtraCharsAfterCQB \"

		        if { $wordtypeline == $line } {
		            lappend blockinfocurrent grey $wordtypepos [expr $icharline+1]
		        } else {
		            lappend blockinfocurrent grey 0 [expr $icharline+1]
		        }
		        set wordtypeline 0

		        if { $OutputType == "R" && [IsProc $words] } {
		            if { $lastinstrumentedline == $line } {
		                set numdel [expr [string length $words]+\
		                                [string length "RDC::F $filenum $line ; "]]
		            } else { set numdel [string length $words] }
		            set newblockR [string range $newblockR 0 end-$numdel]
		            if { [NeedsToInsertSnitPackage $words] } {
		                append newblockP "package require snit\n"
		            }
		            append newblockP $words
		            set OutputType P
		        }
		    } elseif { $wordtype == "\{" } {
		        if { ![info exists quoteintobraceline] } {
		            set quoteintobraceline $line
		            set quoteintobracepos $icharline
		        } else {
		            if { $line == $quoteintobraceline } {
		                lappend blockinfocurrent grey $quoteintobracepos [expr $icharline+1]
		            }
		            unset quoteintobraceline quoteintobracepos
		        }
		    }
		}
	    }
	    \{ {
		if { $lastc != "\\" } {
		    if { $wordtype == "\{" } {
		        incr braceslevelNoEval
		    } elseif { $wordtype == "\"" || $wordtype == "w" } {
		        incr braceslevel
		    } else {
		        set consumed 1
		        set fail [PushState \{ $line $newblocknameP $newblocknameR]
		        if { $fail } {
		            set wordtype \{
		            set wordtypeline $line
		            set braceslevelNoEval 1
		        } else {
		            set lastinstrumentedline $line
		        }
		    }
		}
	    }
	    \} {
		if { $lastc != "\\" } {
		    if { $wordtype == "\{" } {
		        incr braceslevelNoEval -1
		        if { $braceslevelNoEval == 0 } {
		            set wordtype ""
		            lappend words $currentword
		            if { [lindex $words 0] != "\#" && $currentword ne "*" } {
		                set checkExtraCharsAfterCQB \}
		            }
		            set currentword ""
		            set consumed 1
		            if { $OutputType == "R" && [IsProc $words] } {
		                if { $lastinstrumentedline == $line } {
		                    set numdel [expr [string length $words]+\
		                                    [string length "RDC::F $filenum $line ; "]]
		                } else { set numdel [string length $words] }
		                set newblockR [string range $newblockR 0 end-$numdel]
		                if { [NeedsToInsertSnitPackage $words] } {
		                    append newblockP "package require snit\n"
		                }
		                append newblockP $words
		                set OutputType P
		            }
		        }
		    } elseif { $braceslevel > 0 } {
		        incr braceslevel -1
		    } else {
		        set wordtype_before $wordtype
		        set fail [PopState \} $line $newblocknameP $newblocknameR]
		        if { !$fail } {
		            if { $wordtype_before == "\"" } {
		                set text "Quoted text (\") in line $line "
		                append text "contains and invalid brace (\})"
		                error $text
		            }
		            set consumed 1
		            if { [lindex $words 0] != "\#" && [lindex $words end] ne "*" } {
		                set checkExtraCharsAfterCQB \}
		            }
		        }
		    }
		}
	    }
	    " " - \t {
		if { $lastc != "\\" } {
		    if { $wordtype == "w" } {
		        set consumed 1
		        set wordtype ""
		        lappend words $currentword
		        set currentword ""

		        if { $OutputType == "R" && [IsProc $words] } {
		            if { $lastinstrumentedline == $line } {
		                set numdel [expr [string length $words]+\
		                                [string length "RDC::F $filenum $line ; "]]
		            } else { set numdel [string length $words] }
		            set newblockR [string range $newblockR 0 end-$numdel]
		            if { [NeedsToInsertSnitPackage $words] } {
		                append newblockP "package require snit\n"
		            }
		            append newblockP $words
		            set OutputType P
		        }

		        if { [IsProc [lindex $words 0]] } {
		            if { [llength $words] == 1 } {
		                set icharlineold [expr $icharline-[string length [lindex $words 0]]]
		                lappend blockinfocurrent magenta $icharlineold $icharline
		            } elseif { [llength $words] == 2 } {
		                set icharlineold [expr $icharline-[string length [lindex $words end]]]
		                lappend blockinfocurrent blue $icharlineold $icharline
		            }
		        } elseif { [info exists colors([lindex $words end])] } {
		            set icharlineold [expr $icharline-[string length [lindex $words end]]]
		            lappend blockinfocurrent $colors([lindex $words end]) $icharlineold \
		               $icharline                        }
		    } elseif { $wordtype == "" } { set consumed 1 }
		}
	    }
	    \[ {
		if { $lastc != "\\" && $wordtype != "\{" && \
		    [lindex $words 0] != "\#" } {
		    if { $wordtype == "" } { set wordtype "w" }
		    set consumed 1
		    PushState \[ $line $newblocknameP $newblocknameR
		    set lastinstrumentedline $line
		}
	    }
	    \] {
		if { $lastc != "\\" && $wordtype != "\"" && $wordtype != "\{" && \
		    [lindex $words 0] != "\#"} {
		    set fail [PopState \] $line $newblocknameP $newblocknameR]
		    if { !$fail } {
		        set consumed 1
		    }
		    # note: the word inside words is not correct when using []
		}
	    }
	    \n {
		if { [lindex $words 0] == "\#" } {
		    lappend blockinfocurrent red $commentpos $icharline
		} elseif { $wordtype == "\"" } {
		    if { $wordtypeline == $line } {
		        lappend blockinfocurrent grey $wordtypepos $icharline
		    } else {
		        lappend blockinfocurrent grey 0 $icharline
		    }
		} elseif { $wordtype == "w" && [info exists colors($currentword)] } {
		    set icharlineold [expr $icharline-[string length $currentword]]
		    lappend blockinfocurrent $colors($currentword) $icharlineold \
		       $icharline
		}
		lappend blockinfo $blockinfocurrent
		incr line
		set blockinfocurrent [expr $level+$braceslevelNoEval]

		if { ($wordtype == "w" || $wordtype == "\"") && $braceslevel > 0 } {
		    lappend blockinfocurrent "n"
		} elseif { $wordtype != "\{" } {
		    set consumed 1
		    if { $lastc != "\\" && $wordtype != "\"" } {
		        set words ""
		        set currentword ""
		        set wordtype ""

		        if { $OutputType == "P" } {
		            append newblockP $c
		            set OutputType R
		        }
		        lappend blockinfocurrent "n"
		    } else {
		        if { $wordtype == "w" } {
		            set wordtype ""
		            if { $currentword != "\\" } {
		                lappend words $currentword
		            }
		            set currentword ""
		        }
		        lappend blockinfocurrent "c"
		    }
		} else { lappend blockinfocurrent "n" }
	    }
	    \# {
		if { [llength $words] == 0 && $currentword == "" && $wordtype == "" } {
		    set consumed 1
		    lappend words \#
		    set commentpos $icharline
		}
	    }
	    ; {
		if { $lastc != "\\" && $wordtype != "\"" && $wordtype != "\{"  && \
		        [lindex $words 0] != "\#"} {
		    set consumed 1
		    set words ""
		    set currentword ""
		    set wordtype ""

		    if { $OutputType == "P" } {
		        append newblockP $c
		        set OutputType R
		    }
		}
	    }
	}
	append newblock$OutputType $c
	if { !$consumed } {
	    if { $wordtype == "" } {
		set wordtype w
		set wordtypeline $line
	    }
	    append currentword $c
	}
	if { $lastc == "\\" && $c == "\\" } {
	    set lastc "\\\\"
	} else { set lastc $c }
	incr ichar

	if { $c == "\t" } {
	    incr icharline 8
	} elseif { $c != "\n" } {
	    incr icharline
	} else { set icharline 0 }
    }
    lappend blockinfo $blockinfocurrent

    if { $wordtype != "" && $wordtype != "w" } {
	set text "There is a block of type ($wordtype) beginning at line $wordtypeline "
	append text "that is not closed at the end of the file"
	error $text
    }
    CheckEndOfFileState

    if { $length >= 1000 && $progress } {
	RamDebugger::ProgressVar 100
    }
}

proc RamDebugger::Instrumenter::DoWorkForTime { block filename newblockname timedata } {

    upvar $newblockname newblock
    set newblock "# RamDebugger time instrumented file\n"

    set lines [split $block \n]

    set lastline ""
    set lastpos ""
    foreach i $timedata {
	foreach "name file lineini lineend lasttime" $i {
	    if { $file != $filename } { continue }
	    if { $lineini != $lastline } { set lastpos 0 }
	    set text "RDC::MeasureTime [list $name] \[info level] \[time { "
	    set linepos [expr $lineini-1]
	    set line [lindex $lines $linepos]
	    set newline [string range $line 0 [expr $lastpos-1]]
	    append newline $text [string range $line $lastpos end]
	    set lines [lreplace $lines $linepos $linepos $newline]
	    set lastline $lineini
	    set lastpos [expr $lastpos+[string length $text]]

	    set linepos [expr $lineend-1]
	    set newline "[lindex $lines $linepos] }]"
	    set lines [lreplace $lines $linepos $linepos $newline]
	}
    }

    regsub -all {RDC::F [0-9]+ [0-9+] ; } [join $lines \n] {} newblock2
    append newblock $newblock2
}

proc RamDebugger::Instrumenter::DoWorkForC++ { block blockinfoname "progress 1" { indentlevel_ini 0 } \
    { raiseerror 1 } } {
    variable FastInstrumenterLoaded

    # this variable is used to make tests
    set what c++

    switch $what {
	debug {
	   # nothing
	}
	c++ {
	    if { ![info exists FastInstrumenterLoaded] } {
		if { $::tcl_platform(machine) == "amd64"} {
		    set dynlib_base RamDebuggerInstrumenter6_x64[info sharedlibextension]
		} else {
		    set dynlib_base RamDebuggerInstrumenter6_x32[info sharedlibextension]
		}
		set dynlib [file join $RamDebugger::topdir scripts $dynlib_base]
		set err [catch { load $dynlib }]
		if { $err } {
		    set dynlib [file join $RamDebugger::AppDataDir $dynlib_base]
		}
		catch { load $dynlib }
		if { [info commands RamDebuggerInstrumenterDoWorkForXML] eq "" && \
		         $RamDebugger::options(CompileFastInstrumenter) != 0 } {
		    if { $RamDebugger::options(CompileFastInstrumenter) == -1 } {
		        TryCompileFastInstrumenter 0
		        set RamDebugger::options(CompileFastInstrumenter) 0
		    } else {
		        TryCompileFastInstrumenter 1
		    }
		    catch { load $dynlib }
		}
		set FastInstrumenterLoaded 1
	    }
	}
	tcl {
	    # nothing
	}
    }

    if { [info commands RamDebuggerInstrumenterDoWorkForCpp] ne "" } {
	RamDebuggerInstrumenterDoWorkForCpp $block $blockinfoname \
	    $progress $indentlevel_ini
    } else {
	uplevel [list RamDebugger::Instrumenter::DoWorkForC++_do $block $blockinfoname \
		$progress $indentlevel_ini]
    }
}


proc RamDebugger::Instrumenter::DoWorkForC++_do { block blockinfoname "progress 1" { braceslevelIni 0 } } {

    set length [string length $block]
    if { $length >= 5000 && $progress } {
	RamDebugger::ProgressVar 0 1
    } else {
	set progress 0
    }
    upvar $blockinfoname blockinfo
    set blockinfo ""
    set blockinfocurrent [list $braceslevelIni n]


    foreach i [list \#include static const if else new delete for return sizeof while continue \
	    break class typedef struct \#else \#endif \#if] {
	set colors($i) magenta
    }
    foreach i [list \#ifdef \#ifndef \#define \#undef] {
	set colors($i) magenta2
    }
    foreach i [list char int double void ] {
	set colors($i) green
    }

    set wordtype ""
    set wordtypeline ""
    # = -1 -> // ; > 0 -> comment type /*
    set commentlevel 0
    set braceslevel $braceslevelIni
    set braceshistory ""
    set lastc ""
    set line 1
    set ichar 0
    set icharline 0
    set finishedline 1
    set nextiscyan 0
    set simplechar ""
    foreach c [split $block ""] {

	if { $ichar%5000 == 0  && $progress } {
	    RamDebugger::ProgressVar [expr $ichar*100/$length]
	}

	if { $simplechar != "" } {
	    foreach "iline icharinto" $simplechar break
	    if { $line > $iline } {
		error "error in line $iline, position $icharinto. There is no closing (')"
	    }
	    if { $c == "'"  && $lastc != "\\" } {
		set simplechar ""
	    }
	    if { $lastc == "\\" && $c == "\\" } {
		set lastc "\\\\"
	    } else { set lastc $c }

	    incr ichar
	
	    if { $c == "\t" } {
		incr icharline 8
	    } elseif { $c != "\n" } {
		incr icharline
	    } else { set icharline 0 }
	    continue
	}

	switch -- $c {
	    \" {
		if { $commentlevel } {
		    # nothing
		} elseif { $wordtype != "\"" } {
		    set wordtype \"
		    set wordtypeline $line
		    set wordtypepos $icharline
		    set finishedline 0
		} elseif { $lastc != "\\" } {
		    set wordtype ""
		    lappend blockinfocurrent grey $wordtypepos [expr $icharline+1]
		}
	    }
	    ' {
		if { !$commentlevel && $wordtype != "\"" && $lastc != "\\" } {
		    set simplechar [list $line $icharline]
		}
	    }
	    \{ {
		if { $commentlevel || $wordtype == "\"" } {
		    #nothing
		} else {
		    if { $wordtype == "w" } {
		        if { [info exists colors($currentword)] } {
		            lappend blockinfocurrent $colors($currentword) $wordtypepos \
		               $icharline
		            if { $colors($currentword) == "green" } {
		                set nextiscyan 1
		            }
		        } elseif { $nextiscyan } {
		            lappend blockinfocurrent cyan $wordtypepos \
		            $icharline
		            set nextiscyan 0
		        }
		        set wordtype ""
		    }
		    incr braceslevel
		    lappend braceshistory [list o $braceslevel $line $icharline]
		    set finishedline 1
		}
	    }
	    \} {
		if { $commentlevel || $wordtype == "\"" } {
		    #nothing
		} else {
		    if { $wordtype == "w" } {
		        if { [info exists colors($currentword)] } {
		            lappend blockinfocurrent $colors($currentword) $wordtypepos \
		            $icharline
		            if { $colors($currentword) == "green" || \
		                    $colors($currentword) == "magenta2" } {
		                set nextiscyan 1
		            }
		        } elseif { $nextiscyan } {
		            lappend blockinfocurrent cyan $wordtypepos \
		            $icharline
		            set nextiscyan 0
		        }
		        set wordtype ""
		    }
		    incr braceslevel -1
		    lappend braceshistory [list c $braceslevel $line $icharline]
		    if { $braceslevel < 0 } {
		        if { $braceslevelIni == 0 } {
		            RamDebugger::TextOutClear
		            RamDebugger::TextOutRaise
		            RamDebugger::TextOutInsert "BRACES POSITIONS\n"
		            set file $RamDebugger::currentfile
		            foreach i $braceshistory {
		                switch [lindex $i 0] {
		                    o { set tt "open brace" }
		                    c { set tt "close brace" }
		                }
		                set data "$file:[lindex $i 2] $tt pos=[lindex $i 3] "
		                append data "Level after=[lindex $i 1]\n"
		                RamDebugger::TextOutInsert $data
		            }
		        }
		        error "error in line $line. There is one unmatched closing brace (\})"
		    }
		    set finishedline 1
		}
	    }
	    * {
		if { $commentlevel == -1  || $wordtype == "\"" } {
		    #nothing
		} elseif { $lastc == "/" } {
		    if { $commentlevel == 0 } {
		        set wordtype ""
		        set wordtypepos [expr $icharline-1]
		    }
		    incr commentlevel
		} elseif { !$commentlevel && $wordtype == "w" } {
		    if { [info exists colors($currentword)] } {
		        lappend blockinfocurrent $colors($currentword) $wordtypepos \
		        $icharline
		        if { $colors($currentword) == "green" || \
		                    $colors($currentword) == "magenta2" } {
		            set nextiscyan 1
		        }
		    } elseif { $nextiscyan } {
		        lappend blockinfocurrent cyan $wordtypepos \
		        $icharline
		        set nextiscyan 0
		    }
		    set wordtype ""
		}
	    }
	    / {
		if { $commentlevel == -1  || $wordtype == "\"" } {
		    #nothing
		} elseif { !$commentlevel && $lastc == "/" } {
		    set wordtype ""
		    set wordtypepos [expr $icharline-1]
		    set commentlevel -1
		} elseif { $lastc == "*" } {
		    set wordtype ""
		    if { $commentlevel >= 1 } {
		        incr commentlevel -1
		        if { $commentlevel == 0 } {
		            lappend blockinfocurrent red $wordtypepos [expr $icharline+1]
		        }
		    } 
		} elseif { !$commentlevel && $wordtype == "w" } {
		    if { [info exists colors($currentword)] } {
		            lappend blockinfocurrent $colors($currentword) $wordtypepos \
		        $icharline
		        if { $colors($currentword) == "green" || \
		                $colors($currentword) == "magenta2" } {
		            set nextiscyan 1
		        }
		    } elseif { $nextiscyan } {
		        lappend blockinfocurrent cyan $wordtypepos \
		        $icharline
		        set nextiscyan 0
		    }
		    set wordtype ""
		}
	    }
	    \( {
		if { !$commentlevel && $braceslevel == 0 && $wordtype != "\"" } {
		    set ipos [string first :: $currentword]
		    if { $ipos == -1 } {
		        lappend blockinfocurrent blue $wordtypepos $icharline
		    } else {
		        lappend blockinfocurrent green $wordtypepos [expr $wordtypepos+$ipos+2]
		        lappend blockinfocurrent blue [expr $wordtypepos+$ipos+2] $icharline
		    }
		    set nextiscyan 0
		} elseif { $wordtype == "w" } {
		    if { [info exists colors($currentword)] } {
		        lappend blockinfocurrent $colors($currentword) $wordtypepos \
		        $icharline
		        if { $colors($currentword) == "green"  || \
		                $colors($currentword) == "magenta2" } {
		            set nextiscyan 1
		        }
		    } elseif { $nextiscyan } {
		        lappend blockinfocurrent cyan $wordtypepos \
		        $icharline
		        set nextiscyan 0
		    }
		    set wordtype ""
		}
	    }
	    ";" {
		if { !$commentlevel && $wordtype == "w" } {
		    if { [info exists colors($currentword)] } {
		        lappend blockinfocurrent $colors($currentword) $wordtypepos \
		           $icharline
		        if { $colors($currentword) == "green" || \
		                $colors($currentword) == "magenta2" } {
		            set nextiscyan 1
		        }
		    } elseif { $nextiscyan } {
		        lappend blockinfocurrent cyan $wordtypepos \
		        $icharline
		        set nextiscyan 0
		    }
		    set wordtype ""
		}
		if { !$commentlevel && $wordtype != "\"" } {
		    set finishedline 1
		}
	    }
	    \n {
		if { $wordtype == "\"" } {
		    lappend blockinfocurrent grey $wordtypepos $icharline
		    set wordtypepos 0
		} elseif { $wordtype == "w" } {
		    if { [info exists colors($currentword)] } {
		        set icharlineold [expr $icharline-[string length $currentword]]
		        lappend blockinfocurrent $colors($currentword) $icharlineold \
		           $icharline
		        if { $colors($currentword) == "green" || \
		                    $colors($currentword) == "magenta2" } {
		            set nextiscyan 1
		        }
		    } elseif { $nextiscyan } {
		        lappend blockinfocurrent cyan $wordtypepos \
		           $icharline
		        set nextiscyan 0
		    }
		    set wordtype ""
		} elseif { $commentlevel } {
		    lappend blockinfocurrent red $wordtypepos $icharline
		    set wordtypepos 0
		    if { $commentlevel == -1 } { set commentlevel 0 }
		    set finishedline 1
		}
		lappend blockinfo $blockinfocurrent
		incr line
		set blockinfocurrent [expr $braceslevel]

		if { $finishedline } {
		    lappend blockinfocurrent "n"
		} else {
		    lappend blockinfocurrent "c"
		}
	    }
	    default {
		if { $commentlevel || $wordtype == "\"" } {
		    # nothing
		} elseif { $wordtype == "" } {
		    if { [string is wordchar $c] || $c == "\#" || $c == ":" || $c == "," } {
		        set wordtype w
		        set wordtypepos $icharline
		        set currentword $c
		        set finishedline 0
		    }
		} elseif { $wordtype == "w" } {
		    if { [string is wordchar $c] || $c == "\#" || $c == ":" || $c == "," } {
		        append currentword $c
		    } else {
		        if { [info exists colors($currentword)] } {
		            lappend blockinfocurrent $colors($currentword) $wordtypepos \
		               $icharline
		            if { $colors($currentword) == "green" || \
		                    $colors($currentword) == "magenta2" } {
		                set nextiscyan 1
		            }
		        } elseif { $nextiscyan } {
		            lappend blockinfocurrent cyan $wordtypepos \
		               $icharline
		            set nextiscyan 0
		        }
		        set wordtype ""
		    }
		}
	    }
	}
	if { $lastc == "\\" && $c == "\\" } {
	    set lastc "\\\\"
	} else { set lastc $c }
	incr ichar
	
	if { $c == "\t" } {
	    incr icharline 8
	} elseif { $c != "\n" } {
	    incr icharline
	} else { set icharline 0 }
    }
    lappend blockinfo $blockinfocurrent

    if { $wordtype != "" && $wordtype != "w" } {
	set text "There is a block of type ($wordtype) beginning at line $wordtypeline "
	append text "that is not closed at the end of the file"
	error $text
    }
    if { $commentlevel > 0 } {
	error "error: There is a non-closed comment beginning at line $wordtypeline"
    }
    if { $braceslevel } {
	if { $braceslevelIni == 0 } {
	    RamDebugger::TextOutClear
	    RamDebugger::TextOutRaise
	    RamDebugger::TextOutInsert "BRACES POSITIONS\n"
	    set file $RamDebugger::currentfile
	    foreach i $braceshistory {
		switch [lindex $i 0] {
		    o { set tt "open brace" }
		    c { set tt "close brace" }
		}
		set data "$file:[lindex $i 2] $tt pos=[lindex $i 3] Level after=[lindex $i 1]\n"
		RamDebugger::TextOutInsert $data
	    }
	}
	error "error: There is a non-closed brace at the end of the file (see Output for details)"
    }
    if { $progress } {
	RamDebugger::ProgressVar 100
    }
}

proc RamDebugger::Instrumenter::DoWorkForBas { block blockinfoname "progress 1" { braceslevelIni 0 } } {

    set length [string length $block]
    if { $length >= 5000 && $progress } {
	RamDebugger::ProgressVar 0 1
    }

    upvar $blockinfoname blockinfo
    set blockinfo ""

    foreach i [list gendata matprop cond localaxesdef nelem nnode ndime elems notused onlyincond] {
	set colors($i) blue
    }
    foreach i [list if elseif else endif loop endloop end for endfor set break] {
	set colors($i) magenta
    }
    foreach i [list messagebox] {
	set colors($i) red
    }
    foreach i [list realformat intformat nodesnum nodescoor elemsnum elemsconec \
	    nelem elemsmat matnum NodesCoord] {
	set colors($i) green
    }

    set braceslevel $braceslevelIni
    set bracesstack [string repeat ANY $braceslevelIni]
    set blockinfocurrent [list 0 n]

    set iline 1
    set nlines [regexp {\n} $block]
    foreach line [split $block \n] {
	if { $iline%50 == 0  && $progress } {
	    RamDebugger::ProgressVar [expr $iline*100/$nlines]
	}
	if { [regexp {^\s*\*\#} $line] } {
	    lappend blockinfocurrent red 0 [string length $line]
	}
	foreach "- word" [regexp -inline -indices -all {(?:^|[^*])\*(\w+)} $line] {
	    foreach "i1 i2" $word break
	    set word [string tolower [string range $line $i1 $i2]]
	    if { [info exists colors($word)] } {
		incr i2
		lappend blockinfocurrent $colors($word) $i1 $i2
	    }
	}
	foreach "txt" [regexp -inline -indices -all {\"[^\"]*\"} $line] {
	    lappend blockinfocurrent grey [lindex $txt 0] [expr {[lindex $txt 1]+1}]
	}
	if { [regexp {^\*(if|loop|for)\M} [string tolower $line] - what] } {
	    incr braceslevel
	    lappend bracesstack [list $what $iline]
	}
	if { [regexp {^\*(end|endif|endfor|endloop)\M} [string tolower $line] - what] } {
	    foreach "what_old iline_old" [lindex $bracesstack end] break
	    if { $braceslevel == 0 } {
		set txt "error: there is an '$what' in line $iline "
		append txt "and opening loop cannot be found"
		error $txt
	    }
	    if { $what_old != "ANY" } {
		if { ($what == "endif" && $what_old != "if") || \
		         ($what == "endfor" && $what_old != "for") || \
		         ($what == "endloop" && $what_old != "loop") } {
		    set txt "error: there is an '$what' in line $iline"
		    append txt "and one '$what_old' in line $iline_old"
		    error $txt
		}
	    }
	    incr braceslevel -1
	    set bracesstack [lreplace $bracesstack end end]
	}
	incr iline
	lappend blockinfo $blockinfocurrent
	set blockinfocurrent [list 0 n]
    }
    if { $braceslevel != 0 } {
	foreach "what_old iline_old" [lindex $bracesstack end] break
	set text "There is a block of type ($what_old) beginning at line $iline_old "
	append text "that is not closed at the end of the file (N of open levels: $braceslevel)"
	error $text
     }
    if { $length >= 1000  && $progress } {
	RamDebugger::ProgressVar 100
    }
}

proc RamDebugger::Instrumenter::DoWorkForGiDData { block blockinfoname "progress 1" { braceslevelIni 0 } } {

    set length [string length $block]
    if { $length >= 5000 && $progress } {
	RamDebugger::ProgressVar 0 1
    }

    upvar $blockinfoname blockinfo
    set blockinfo ""

    foreach i [list book title dependencies help tkwidget state] {
	set colors($i) blue
    }
    foreach i [list question value condtype condmeshtype] {
	set colors($i) magenta
    }
#     foreach i [list messagebox] {
#         set colors($i) red
#     }
    foreach i [list number condition end] {
	set colors($i) green
    }

    set braceslevel $braceslevelIni
    set bracesstack [string repeat ANY $braceslevelIni]
    set blockinfocurrent [list 0 n]
    set dep_continuation 0

    set iline 1
    set nlines [regexp {\n} $block]
    foreach line [split $block \n] {
	if { $iline%50 == 0  && $progress } {
	    RamDebugger::ProgressVar [expr $iline*100/$nlines]
	}
	if { $dep_continuation } {
	    if { [regexp {\\\s*$} $line] } {
		set dep_continuation 1
	    } else { set dep_continuation 0 }
	} elseif { [regexp {(?i)^\s*end\M} $line] } {
	    if { $braceslevel > 1 } {
		incr braceslevel -1
		set bracesstack [lreplace $bracesstack end end]
	    } elseif { $braceslevel == 1 } {
		lappend blockinfocurrent $colors(end) 0 [string length $line]
		incr braceslevel -1
	    } else {
		error "error: found an 'end line' without having beginning in line $iline"
	    }
	} elseif { [regexp -indices {(?i)^\s*(INTERVAL|PROBLEM)\s+DATA} $line idxs] } {
	    foreach "idx1 idx2" $idxs break
	    incr idx2
	    lappend blockinfocurrent $colors(condition) $idx1 $idx2
	    incr braceslevel
	    lappend bracesstack [list problem $iline]
	} elseif { [regexp -indices {(?i)^\s*(NUMBER:)\s*[0-9]+\s+((?:CONDITION|MATERIAL):)\s*\S+\s*$} \
		        $line {} idxs1 idxs2] } {
	    foreach "idx1 idx2" $idxs1 break
	    incr idx2
	    lappend blockinfocurrent $colors(number) $idx1 $idx2
	    foreach "idx1 idx2" $idxs2 break
	    incr idx2
	    lappend blockinfocurrent $colors(condition) $idx1 $idx2
	    incr braceslevel
	    lappend bracesstack [list condmat $iline]
	} elseif { [regexp -indices {(?i)^\s*(DEPENDENCIES:)} $line {} idxs] } {
	    foreach "idx1 idx2" $idxs break
	    incr idx2
	    lappend blockinfocurrent $colors(dependencies) $idx1 $idx2
	    if { [regexp {\\\s*$} $line] } {
		set dep_continuation 1
	    }
	} elseif { [regexp -indices {(?i)^\s*(HELP:)} $line {} idxs] } {
	    foreach "idx1 idx2" $idxs break
	    incr idx2
	    lappend blockinfocurrent $colors(help) $idx1 $idx2
	    if { [regexp {\\\s*$} $line] } {
		set dep_continuation 1
	    }
	} elseif { [regexp -indices {\s*([^\s:]+:)} $line {} idxs] } {
	    foreach "idx1 idx2" $idxs break
	    set word [string tolower [string range $line $idx1 [expr {$idx2-1}]]]
	    if { [info exists colors($word)] } {
		incr idx2
		lappend blockinfocurrent $colors($word) $idx1 $idx2
	    }
	    if { [regexp -indices {#(CB|LA)#\([^\)]+\)} $line idxs] } {
		foreach "idx1 idx2" $idxs break
		incr idx2
		lappend blockinfocurrent grey $idx1 $idx2
	    }
	    if { [regexp -indices {#N#\s+[0-9]+} $line idxs] } {
		foreach "idx1 idx2" $idxs break
		incr idx2
		#lappend blockinfocurrent chocolate2 $idx1 $idx2
		lappend blockinfocurrent red $idx1 $idx2
	    }
	} elseif { ![regexp {^\s*$} $line] } {
	    error "error reading line $iline"
	}
	incr iline
	lappend blockinfo $blockinfocurrent
	set blockinfocurrent [list 0 n]
    }
    if { $braceslevel != 0 } {
	foreach "what_old iline_old" [lindex $bracesstack end] break
	set text "There is a block of type ($what_old) beginning at line $iline_old "
	append text "that is not closed at the end of the file (N of open levels: $braceslevel)"
	error $text
     }
    if { $length >= 1000  && $progress } {
	RamDebugger::ProgressVar 100
    }
}

proc RamDebugger::Instrumenter::raise_error { txt } {
    upvar 1 iline iline
    upvar 1 icharline icharline
    upvar 1 raiseerror raiseerror

    if { !$raiseerror } { return }

    set str "error in line=$iline position=[expr {$icharline+1}]. "
    append str $txt
    error $str
}

proc RamDebugger::Instrumenter::push_state { state } {
    upvar 1 statestack statestack
    lappend statestack $state
}

proc RamDebugger::Instrumenter::pop_state {} {
    upvar 1 statestack statestack
    set statestack [lrange $statestack 0 end-1]
}

proc RamDebugger::Instrumenter::state_is { state { idx end } } {
    upvar 1 statestack statestack
    return [string equal $state [lindex $statestack $idx]]
}

proc RamDebugger::Instrumenter::push_tag { tag line } {
    upvar 1 tagsstack tagsstack
    upvar 1 indentlevel indentlevel
    lappend tagsstack [list $tag $line]
    incr indentlevel
}

proc RamDebugger::Instrumenter::pop_tag { tag } {
    upvar 1 tagsstack tagsstack
    upvar 1 indentlevel indentlevel

    if { $tag ne "-" && $tag ne [lindex $tagsstack end 0] } {
	set txt "closing tag '$tag' is not correct. tags stack=\n"
	foreach i $tagsstack {
	    append txt "\t[lindex $i 0]\tLine: [lindex $i 1]\n"
	}
	uplevel 1 [list raise_error $txt]
    }
    set tagsstack [lrange $tagsstack 0 end-1]
    incr indentlevel -1
}

proc RamDebugger::Instrumenter::range { i1 i2 } {
    upvar 1 block block
    return [string range $block [expr $i1] [expr $i2]]
}

proc RamDebugger::Instrumenter::lappend_info { color i1 i2 } {
    upvar 1 blockinfocurrent blockinfocurrent
    lappend blockinfocurrent $color [expr $i1] [expr $i2]
}

proc RamDebugger::Instrumenter::DoWorkForXML { block blockinfoname "progress 1" { indentlevel_ini 0 } \
    { raiseerror 1 } } {
    variable FastInstrumenterLoaded

    # this variable is used to make tests
    set what c++

    switch $what {
	debug {
	   # nothing
	}
	c++ {
	    if { ![info exists FastInstrumenterLoaded] } {
		if { $::tcl_platform(machine) == "amd64"} {
		    set dynlib_base RamDebuggerInstrumenter6_x64[info sharedlibextension]
		} else {
		    set dynlib_base RamDebuggerInstrumenter6_x32[info sharedlibextension]
		}
		set dynlib [file join $RamDebugger::topdir scripts $dynlib_base]
		set err [catch { load $dynlib }]
		if { $err } {
		    set dynlib [file join $RamDebugger::AppDataDir $dynlib_base]
		}
		catch { load $dynlib }
		if { [info commands RamDebuggerInstrumenterDoWorkForXML] eq "" && \
		         $RamDebugger::options(CompileFastInstrumenter) != 0 } {
		    if { $RamDebugger::options(CompileFastInstrumenter) == -1 } {
		        TryCompileFastInstrumenter 0
		        set RamDebugger::options(CompileFastInstrumenter) 0
		    } else {
		        TryCompileFastInstrumenter 1
		    }
		    catch { load $dynlib }
		}
		set FastInstrumenterLoaded 1
	    }
	}
	tcl {
	    # nothing
	}
    }

    if { [info commands RamDebuggerInstrumenterDoWorkForXML] ne "" } {
	RamDebuggerInstrumenterDoWorkForXML $block $blockinfoname \
	    $progress $indentlevel_ini $raiseerror
    } else {
	uplevel [list RamDebugger::Instrumenter::DoWorkForXML_do $block $blockinfoname \
		$progress $indentlevel_ini $raiseerror]
    }
}

proc RamDebugger::Instrumenter::DoWorkForXML_do { block blockinfoname "progress 1" { indentlevel_ini 0 } \
    { raiseerror 1 } } {

    set length [string length $block]
    if { $length >= 5000 && $progress } {
	RamDebugger::ProgressVar 0 1
    }

    set indentlevel $indentlevel_ini
    upvar $blockinfoname blockinfo
    set blockinfo ""
    set blockinfocurrent [list $indentlevel n]

    # colors: magenta magenta2 green red

    set tagsstack ""
    set statestack ""
    set icharline 0
    set iline 1

    for { set i 0 } { $i < $length } { incr i } {
	set c [string index $block $i]

	if { $i%5000 == 0  && $progress } {
	    RamDebugger::ProgressVar [expr $i*100/$length]
	}
	if { [state_is "cdata"] } {
	    if { [range $i-2 $i] eq "\]\]>" } {
		pop_state
		lappend_info green $icharline-2 $icharline+1
	    }
	} elseif { [state_is "comment"] } {
	    if { [range $i-2 $i] eq "-->" } {
		pop_state
		lappend_info red $state_start $icharline+1
	    } elseif { $c eq "\n" } {
		lappend_info red $state_start $icharline
		set start_start 0
	    }
	} elseif { [state_is "doctype"]} {
	    if { $c eq "\[" } {
		pop_state
		push_state doctype+
	    } elseif { $c eq ">" } {
		pop_state
	    }
	    # nothing
	} elseif { [state_is "doctype+"]} {
	    if { [range $i-1 $i] eq "\]>" } {
		pop_state
	    }
	    # nothing
	} else {
	    switch -- $c {
		< {
		    if { [state_is att_dquote] || [state_is att_quote] } {
		        #nothing
		    } elseif { ![state_is ""] && ![state_is text] } {
		        raise_error "Not valid <"
		    } elseif { [range $i $i+1] eq "<?" } {
		        push_state pi
		    } elseif { [range $i $i+8] eq "<!DOCTYPE" } {
		        push_state doctype
		    } elseif { [range $i $i+3] eq "<!--" } {
		        push_state comment
		        set state_start $icharline
		        set state_start_global $i
		    } elseif { [range $i $i+8] eq "<!\[CDATA\[" } {
		        push_state cdata
		        lappend_info green $icharline $icharline+9
		    } else {
		        if { [state_is text] } { pop_state }
		        push_state tag
		        lappend_info magenta $icharline $icharline
		    }
		}
		> {
		    if { [state_is enter_tagtext] } {
		        pop_state
		        if { [state_is tag_end] } {
		            set isend 1
		            pop_state
		        } else { set isend 0 }
		        if { [state_is "tag"] } {
		            if { $isend } {
		                pop_tag [range $state_start_global $i-1]
		            } else {
		                push_tag [range $state_start_global $i-1] $iline
		            }
		            lappend_info magenta $state_start $icharline
		        } else {
		            lappend_info green $state_start $icharline
		        }
		        push_state entered_tagtext
		        
		    }
		    if { [state_is att_dquote] || [state_is att_quote] } {
		        #nothing
		    } else {
		        if { ![state_is entered_tagtext] } {
		            raise_error "Not valid >"
		        }
		        pop_state
		        if {[state_is "pi"] && [range $i-1 $i] eq "?>" } {
		            pop_state
		        } elseif {[state_is "tag"] } {
		            pop_state
		            push_state text
		        } else {
		            raise_error "Not 2 valid >"
		        }
		    }
		}
		/ {
		    if { [state_is tag] || [state_is pi] } {
		        push_state tag_end
		        lappend_info red $icharline $icharline+1
		    } elseif { [state_is enter_tagtext] } {
		        pop_state
		        if { [state_is "tag"] } {
		            push_tag [range $state_start_global $i-1] $iline
		            pop_tag [range $state_start_global $i-1]
		        }
		        lappend_info magenta $state_start $icharline
		        push_state entered_tagtext
		    } elseif { [state_is entered_tagtext] } {
		        pop_tag -
		    } elseif { ![state_is text] && ![state_is att_text] && ![state_is att_quote] \
		        && ![state_is att_dquote] } {
		        raise_error "Not valid /"
		    }
		}
		= {
		    if { [state_is att_entername] || [state_is att_after_name] } {
		        pop_state
		        push_state att_after_equal
		    } elseif { ![state_is text] && ![state_is att_dquote] \
		        && ![state_is att_quote]} {
		        raise_error "Not valid ="
		    }
		}
		' {
		    if { [state_is att_after_equal] } {
		        pop_state
		        push_state att_quote
		        set state_start $icharline
		        set state_start_global $i
		    } elseif { [state_is att_quote] } {
		        if { ![regexp {[\s?/>]} [range $i+1 $i+1]] } {
		            raise_error "Not valid close '"
		        }
		        pop_state
		        lappend_info grey $state_start $icharline+1
		    } elseif { ![state_is text] && ![state_is att_dquote] } {
		        raise_error "Not valid '"
		    }
		}
		\" {
		    if { [state_is att_after_equal] } {
		        pop_state
		        push_state att_dquote
		        set state_start $icharline
		        set state_start_global $i
		    } elseif { [state_is att_dquote] } {
		        if { ![regexp {[\s?/>]} [range $i+1 $i+1]] } {
		            raise_error "Not valid close \""
		        }
		        pop_state
		        lappend_info grey $state_start $icharline+1
		    } elseif { ![state_is text] && ![state_is att_quote] } {
		        raise_error "Not valid \""
		    }
		}
		{ } - \t - \n {
		    if { [state_is enter_tagtext] } {
		        pop_state
		        if { [state_is tag_end] } {
		            set isend 1
		            pop_state
		        } else { set isend 0 }
		        if { [state_is "tag"] } {
		            if { $isend } {
		                pop_tag [range $state_start_global $i-1]
		            } else {
		                push_tag [range $state_start_global $i-1] $iline
		            }
		            lappend_info magenta $state_start $icharline
		        } else {
		            lappend_info green $state_start $icharline
		        }
		        push_state entered_tagtext
		        
		    } elseif { [state_is att_entername] } {
		        pop_state
		        push_state att_after_name
		    }
		}
		default {
		    if { $c ne "" } {
		        if { [state_is tag] || [state_is pi] || [state_is tag_end] } {
		            push_state enter_tagtext
		            set state_start $icharline
		            set state_start_global $i
		        } elseif { [state_is entered_tagtext] } {
		            if { $c ne "?" } {
		                push_state att_entername
		                set state_start $icharline
		                set state_start_global $i
		            }
		        } elseif { ![state_is text] && ![state_is att_quote] \
		            && ![state_is att_dquote] && ![state_is enter_tagtext] && \
		                ![state_is att_entername] } {
		            raise_error "Not valid character '$c'"
		        }
		    }
		}
	    }
	}
	if { $c == "\t" } {
	    incr icharline 8
	} elseif { $c eq "\n" } {
	    incr iline
	    set icharline 0
	    if { $indentlevel < [lindex $blockinfocurrent 0] } {
		if { $iline == 2 } {
		    incr indentlevel
		} else {
		    lset blockinfocurrent 0 $indentlevel
		}
	    }
	    lappend blockinfo $blockinfocurrent
	    set blockinfocurrent $indentlevel
	    lappend blockinfocurrent "n"
	} else {
	    incr icharline
	}
    }
    if { $indentlevel < [lindex $blockinfocurrent 0] } {
	if { $iline == 2 } {
	    incr indentlevel
	} else {
	    lset blockinfocurrent 0 $indentlevel
	}
    }
    lappend blockinfo $blockinfocurrent

    if { [llength $tagsstack] } {
	set txt "There are non-closed tags. tags stack=\n"
	foreach i $tagsstack {
	    append txt "\t[lindex $i 0]\tLine: [lindex $i 1]\n"
	}
	raise_error $txt
    }

    if { $length >= 1000  && $progress } {
	RamDebugger::ProgressVar 100
    }
}

proc _incr_l2 { idx } {
    lassign $idx idx1 idx2
    return [list $idx1 [incr idx2]]
}

proc RamDebugger::Instrumenter::DoWorkForMakefile { block blockinfoname "progress 1" } {

    set length [string length $block]
    if { $length >= 5000 && $progress } {
	RamDebugger::ProgressVar 0 1
    }

    upvar $blockinfoname blockinfo
    set blockinfo ""
    set continuation 0

    set iline 1
    set nlines [regexp {\n} $block]
    foreach line [split $block \n] {
	set line [string map [list "\t" "        "] $line]
	if { $iline%50 == 0  && $progress } {
	    RamDebugger::ProgressVar [expr $iline*100/$nlines]
	}
	if { !$continuation } {
	    set blockinfocurrent [list 0 n]
	} else {
	    set blockinfocurrent [list 0 c]
	}
	if { [regexp -indices {^\s*#.*} $line idxs] } {
	    lappend blockinfocurrent red {*}[_incr_l2 $idxs]
	    set continuation 0
	} else {
	    if { [regexp -indices {^\s*(\w+)\s*=} $line {} idxs] } {
		lappend blockinfocurrent green {*}[_incr_l2 $idxs]
	    }
	    foreach "- idxs"  [regexp -inline -indices {\$\((\w+)\)?} $line] {
		lappend blockinfocurrent magenta {*}[_incr_l2 $idxs]
	    }
	    if { [regexp -indices {^\s*(ifeq|ifneq|else|endif)} $line {} idxs] } {
		lappend blockinfocurrent blue {*}[_incr_l2 $idxs]
	    }
	    if { [regexp -indices {^\s*([^:]+):} $line {} idxs] } {
		lappend blockinfocurrent blue {*}[_incr_l2 $idxs]
	    }
	    set continuation [regexp {\\$} $line]
	}
	incr iline
	lappend blockinfo $blockinfocurrent
    }
    if { $length >= 1000  && $progress } {
	RamDebugger::ProgressVar 100
    }
}
















