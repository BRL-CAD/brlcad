Name:           libbson
Version:        1.2.0
Release:        1%{?dist}
Summary:        BSON library

License:        ASL 2.0
URL:            https://github.com/mongodb/libbson
Source0:        https://github.com/mongodb/libbson/releases/download/1.2.0-beta/libbson-1.2.0-beta.tar.gz
BuildRequires:  automake

%description
libbson is a library providing useful routines related to 
building, parsing, and iterating BSON documents.

%package        devel
Summary:        Development files for libbson
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description    devel
The %{name}-devel package contains libraries and header files for
developing applications that use %{name}.

%prep
%setup -q -n %{name}-%{version}
automake 

%build
%configure --disable-static --disable-silent-rules --enable-debug-symbols --enable-hardening --enable-optimizations --docdir=%{_pkgdocdir} --enable-man-pages --enable-html-docs
make %{?_smp_mflags}

%check
make check

%install
%make_install
find $RPM_BUILD_ROOT -name '*.la' -exec rm -f {} ';'

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%doc COPYING README NEWS
%{_libdir}/*.so.*

%files devel
%dir %{_includedir}/%{name}-1.0
%{_includedir}/%{name}-1.0/*.h
%{_libdir}/%{name}-1.0.so
%{_libdir}/pkgconfig/%{name}-1.0.pc
%{_prefix}/share/man/man3/*

%changelog

* Tue October 13 2015 A. Jesse Jiryu Davis <jesse@mongodb.com> - 1.2.0-1
- Release 1.2.0

* Wed September 23 2015 A. Jesse Jiryu Davis <jesse@mongodb.com> - 1.1.11-1
- Release 1.1.11

* Tue July 21 2015 A. Jesse Jiryu Davis <jesse@mongodb.com> - 1.1.10-1
- Release 1.1.10

* Sun June 28 2015 A. Jesse Jiryu Davis <jesse@mongodb.com> - 1.1.9-1
- Release 1.1.9

* Sun June 21 2015 A. Jesse Jiryu Davis <jesse@mongodb.com> - 1.1.8-1
- Release 1.1.8

* Tue June 9 2015 A. Jesse Jiryu Davis <jesse@mongodb.com> - 1.1.7-1
- Release 1.1.7

* Tue May 18 2015 A. Jesse Jiryu Davis <jesse@mongodb.com> - 1.1.6-1
- Release 1.1.6

* Tue May 12 2015 A. Jesse Jiryu Davis <jesse@mongodb.com> - 1.1.5-1
- Release 1.1.5

* Wed Apr 1 2015 Jason Carey <jason.carey@mongodb.com> - 1.1.4-1
- Bump for 1.1.4.

* Tue Mar 10 2015 Jason Carey <jason.carey@mongodb.com> - 1.1.3-1
- post-release bump

* Tue Mar 10 2015 Jason Carey <jason.carey@mongodb.com> - 1.1.2-1
- Bump for 1.1.2.

* Wed Jan 28 2015 Jason Carey <jason.carey@mongodb.com> - 1.1.1-1
- post-release bump

* Wed Jan 28 2015 Jason Carey <jason.carey@mongodb.com> - 1.1.0
- Bump for 1.1.0.

* Wed Nov 12 2014 Jason Carey <jason.carey@mongodb.com> - 1.1.0-rc0-1
- Bump for 1.1.0-rc0.

* Thu Oct 09 2014 Jason Carey <jason.carey@mongodb.com> - 1.0.3-1
- post-release bump.

* Wed Oct 08 2014 Jason Carey <jason.carey@mongodb.com> - 1.0.2-1
- Bump for 1.0.2.

* Tue Aug 26 2014 Christian Hergert <christian.hergert@mongodb.com> - 1.0.1-1
- post-release bump.

* Tue Aug 26 2014 Christian Hergert <christian.hergert@mongodb.com> - 1.0.0-1
- Bump for 1.0.0!

* Thu Jul 16 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.98.0-1
- Bump for 0.98.0.

* Thu Jun 20 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.8.5-1
- Bump for development builds.

* Thu Jun 20 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.8.4-1
- Bump for 0.8.4.

* Thu Jun 10 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.8.3-1
- Bump for development builds.

* Thu Jun 05 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.8.2-1
- Release 0.8.2

* Thu May 29 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.8.0-1
- Release 0.8.0

* Tue May 06 2014 Christian Hergert <christian.hergert@mongodb.com> - 0.6.9-1
- Update to 0.6.9 from git.
- Enable debug symbols
- Enable autoharden extensions

* Fri Aug 30 2013 Mike Manilone <crtmike@gmx.us> - 0.2.0-2
- Fix docdir.
- Do not use silent rules.

* Wed Aug 28 2013 Mike Manilone <crtmike@gmx.us> - 0.2.0-1
- Update to 0.2.0.

* Tue Aug 13 2013 Mike Manilone <crtmike@gmx.us> - 0.1.10-7
- Debug level is changed to minimum.
- Disable python package.
- Update (affecting Pidora).

* Tue Aug 13 2013 Mike Manilone <crtmike@gmx.us> - 0.1.10-6
- Add 'check' section.

* Mon Aug 12 2013 Mike Manilone <crtmike@gmx.us> - 0.1.10-5
- Do not 'provide' private libraries.

* Mon Aug 12 2013 Mike Manilone <crtmike@gmx.us> - 0.1.10-4
- Update.

* Mon Aug 12 2013 Mike Manilone <crtmike@gmx.us> - 0.1.10-3
- Fix many problems.

* Mon Aug 12 2013 Mike Manilone <crtmike@gmx.us> - 0.1.10-2
- Make -devel subpackage a noarch package
- Add a patch to make it pass the compilation

* Mon Aug 12 2013 Mike Manilone <crtmike@gmx.us> - 0.1.10-1
- Initial package
