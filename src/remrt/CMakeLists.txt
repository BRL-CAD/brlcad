# remrt / rtsrv — Network-Distributed Ray Tracing
#
# The daemonization logic in rtsrv.c (fork/setpgid/setsid) is guarded
# by HAVE_FORK / HAVE_SETPGID / HAVE_SETSID at the source level, so
# both executables can build on Windows.  Automatic worker launching
# from remrt (add_host via SSH) requires an SSH client to be present
# on the controller machine; manual / passive worker connections work
# on all platforms.

# Optional: link against OpenSSL for cryptographically strong token
# generation.  Falls back to /dev/urandom (POSIX) or CryptGenRandom
# (Windows) if OpenSSL is not found.
find_package(OpenSSL QUIET)

brlcad_addexec(remrt "../rt/opt.c;ihost.c;remrt.c" "liboptical;libdm")
target_include_directories(remrt BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
add_target_deps(remrt dm_plugins)
if(OpenSSL_FOUND OR OPENSSL_FOUND)
  target_link_libraries(remrt OpenSSL::SSL OpenSSL::Crypto)
  target_compile_definitions(remrt PRIVATE HAVE_OPENSSL_RAND_H=1 HAVE_OPENSSL_SSL_H=1)
endif()

brlcad_addexec(rtsrv "../rt/usage.cpp;../rt/view.c;../rt/do.c;../rt/grid.c;../rt/heatgraph.c;../rt/opt.c;../rt/scanline.c;../rt/worker.c;rtsrv.c" "libdm;liboptical;libpkg;libicv")
target_include_directories(rtsrv BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET rtsrv APPEND PROPERTY COMPILE_DEFINITIONS "RTSRV")
add_target_deps(rtsrv dm_plugins)
if(OpenSSL_FOUND OR OPENSSL_FOUND)
  target_link_libraries(rtsrv OpenSSL::SSL OpenSSL::Crypto)
  target_compile_definitions(rtsrv PRIVATE HAVE_OPENSSL_RAND_H=1 HAVE_OPENSSL_SSL_H=1)
endif()

cmakefiles(
  auth.h
  ihost.c
  ihost.h
  protocol.h
  remrt.c
  rtsrv.c
  tls_wrap.h
)
cmakefiles(CMakeLists.txt)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
