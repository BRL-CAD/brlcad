/*
 * This loads in the Iwidgets into the application resource fork.
 */
 
#define IWIDGETS_LIBRARY_RESOURCES 3600

read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+5, "iwidgets", purgeable) 
	":iwidgets.tcl";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+6, "iwidgets:tclIndex", purgeable) 
	":tclIndex";
data 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+7,"iwidgets:pkgIndex", purgeable) 
	{"package ifneeded Iwidgets 2.2.0 {source -rsrc iwidgets}"};
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+8,"iw_buttonbox", purgeable)  
    "::generic:buttonbox.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+9,"iw_canvasprintbox", purgeable)  
    "::generic:canvasprintbox.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+10,"iw_canvasprintdialog ", purgeable)  
	"::generic:canvasprintdialog.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+11,"iw_colors", purgeable) 
	"::generic:colors.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+12,"iw_combobox", purgeable) 
	"::generic:combobox.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+13,"iw_dialog", purgeable) 
	"::generic:dialog.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+14,"iw_dialogshell", purgeable) 
	"::generic:dialogshell.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+15,"iw_entryfield", purgeable) 
	"::generic:entryfield.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+16,"iw_feedback", purgeable) 
	"::generic:feedback.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+17,"iw_fileselectionbox", purgeable) 
	"::generic:fileselectionbox.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+18,"iw_fileselectiondialog", purgeable) 
	"::generic:fileselectiondialog.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+19,"iw_hyperhelp", purgeable) 
	"::generic:hyperhelp.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+20,"iw_labeledwidget", purgeable) 
	"::generic:labeledwidget.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+21,"iw_menubar", purgeable) 
	"::generic:menubar.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+22,"iw_messagedialog", purgeable) 
	"::generic:messagedialog.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+23,"iw_notebook", purgeable) 
	"::generic:notebook.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+24,"iw_optionmenu", purgeable) 
	"::generic:optionmenu.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+25,"iw_pane", purgeable) 
	"::generic:pane.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+26,"iw_panedwindow", purgeable) 
	"::generic:panedwindow.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+27,"iw_promptdialog", purgeable) 
	"::generic:promptdialog.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+28,"iw_pushbutton", purgeable) 
	"::generic:pushbutton.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+29,"iw_radiobox", purgeable) 
	"::generic:radiobox.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+30,"iw_scrolledcanvas", purgeable) 
	"::generic:scrolledcanvas.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+31,"iw_scrolledframe", purgeable) 
	"::generic:scrolledframe.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+32,"iw_scrolledhtml", purgeable) 
	"::generic:scrolledhtml.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+33,"iw_scrolledlistbox", purgeable) 
	"::generic:scrolledlistbox.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+34,"iw_scrolledtext", purgeable) 
	"::generic:scrolledtext.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+35,"iw_selectionbox", purgeable) 
	"::generic:selectionbox.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+36,"iw_selectiondialog", purgeable) 
	"::generic:selectiondialog.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+37,"iw_shell", purgeable) 
	"::generic:shell.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+38,"iw_spindate", purgeable) 
	"::generic:spindate.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+39,"iw_spinint", purgeable) 
	"::generic:spinint.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+40,"iw_spinner", purgeable) 
	"::generic:spinner.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+41,"iw_spintime", purgeable) 
	"::generic:spintime.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+42,"iw_tabnotebook", purgeable) 
	"::generic:tabnotebook.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+43,"iw_tabset", purgeable) 
	"::generic:tabset.itk";
read 'TEXT' (IWIDGETS_LIBRARY_RESOURCES+44,"iw_toolbar", purgeable) 
	"::generic:toolbar.itk";
