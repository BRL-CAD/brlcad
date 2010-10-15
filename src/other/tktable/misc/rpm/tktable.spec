# RPM specfile provided by Jean-Luc Fontaine
# $Id: tktable.spec,v 1.3 2008/11/14 23:16:52 hobbs Exp $

%define	version	2.10
%define	directory /usr

Summary: table/matrix widget extension to Tcl/Tk.
Name: tktable
Version: %{version}
Release: 1
Copyright: public domain
Group: Development/Languages/Tcl
Source: http://prdownloads.sourceforge.net/tktable/Tktable%{version}.tar.gz
URL: http://tktable.sourceforge.net/
Packager: Jean-Luc Fontaine <jfontain@free.fr>
BuildRequires: XFree86-libs >= 4, XFree86-devel >= 4, tk >= 8.3.1
AutoReqProv: no
Requires: tk >= 8.3.1
Buildroot: /var/tmp/%{name}%{version}

%description
Tktable provides a table/matrix widget for Tk programs. Features:
multi-line cells, embedded windows, variable width columns/height rows
(interactively resizable), scrollbar support, tag styles per row,
column or cell, in-cell editing, works on UNIX, Windows and MacIntosh,
Unicode support with Tk 8.1 and above.

%prep

%setup -q -c

%build
cd Tktable%{version}
./configure --with-tcl=%{directory}/lib --with-tk=%{directory}/lib
make TBL_CFLAGS=-O2

%install
cd Tktable%{version}
DIRECTORY=$RPM_BUILD_ROOT%{directory}/lib/%{name}%{version}
install -d $DIRECTORY
install libTktable%{version}.so $DIRECTORY/
install -m 644 pkgIndex.tcl library/tkTable.tcl library/tktable.py $DIRECTORY
install -d $RPM_BUILD_ROOT%{directory}/man/mann
install -m 644 doc/tkTable.n $RPM_BUILD_ROOT%{directory}/man/mann
install -m 644 ChangeLog README.txt README.blt license.txt ..
install -d ../doc
install -m 644 doc/tkTable.html ../doc

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc ChangeLog README.txt README.blt license.txt doc/tkTable.html
%{directory}/lib/%{name}%{version}
%{directory}/man/mann/tkTable.n.gz
