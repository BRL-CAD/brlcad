# $Id$
# This file is the basis for a binary Tcl RPM for Linux.

%define version 8.4.2
%define directory /usr/local

Summary: Tcl scripting language development environment
Name: tcl
Version: %{version}
Release: 1
Copyright: BSD
Group: Development/Languages
Source: http://prdownloads.sourceforge.net/tcl/tcl%{version}-src.tar.gz
URL: http://www.tcl.tk/
Packager: Carina
Buildroot: /var/tmp/%{name}%{version}

%description
The Tcl (Tool Command Language) provides a powerful platform for
creating integration applications that tie together diverse
applications, protocols, devices, and frameworks.  When paired with
the Tk toolkit, Tcl provides the fastest and most powerful way to
create GUI applications that run on PCs, Unix, and the Macintosh.  Tcl
can also be used for a variety of web-related tasks and for creating
powerful command languages for applications.

%prep

%build
./configure --prefix %{directory} --exec-prefix %{directory}
make CFLAGS=$RPM_OPT_FLAGS

%install
rm -rf $RPM_BUILD_ROOT
make INSTALL_ROOT=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

# to create the tcl files list, comment out tk in the install section above,
# then run "rpm -bi" then do a find from the build root directory,
# and remove the files in specific directories which suffice by themselves,
# then to create the files list for tk, uncomment tk, comment out tcl,
# then rm -rf $RPM_BUILD_ROOT then rpm --short-circuit -bi then redo a find,
# and remove the files in specific directories which suffice by themselves.
%files
%defattr(-,root,root)
%{directory}/lib
%{directory}/bin
%{directory}/include
%{directory}/man/man1
%{directory}/man/man3
%{directory}/man/mann
