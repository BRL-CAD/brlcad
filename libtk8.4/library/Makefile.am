# $Id$

SUBDIRS = demos msgs

FILES = bgerror.tcl button.tcl choosedir.tcl clrpick.tcl comdlg.tcl console.tcl dialog.tcl entry.tcl focus.tcl listbox.tcl menu.tcl mkpsenc.tcl msgbox.tcl obsolete.tcl optMenu.tcl palette.tcl panedwindow.tcl safetk.tcl scale.tcl scrlbar.tcl spinbox.tcl tclIndex tearoff.tcl text.tcl tk.tcl tkfbox.tcl unsupported.tcl xmfbox.tcl 

install:
	mkdir -p @TK_LIBRARY@
	for a in $(FILES) ; do ${INSTALL_SCRIPT} -m 444 $$a @TK_LIBRARY@/$$a ; done
	for a in $(SUBDIRS) ; do (cd $$a && make install) ; done
