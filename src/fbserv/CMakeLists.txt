include_directories(${DM_INCLUDE_DIRS})

# Optional: link against OpenSSL for TLS support and cryptographically
# strong token generation.  Falls back to /dev/urandom (POSIX) or
# CryptGenRandom (Windows) if OpenSSL is not found.
find_package(OpenSSL QUIET)

brlcad_addexec(fbserv "fbserv.c;server.c" libdm)
add_target_deps(fbserv dm_plugins)
if(OpenSSL_FOUND OR OPENSSL_FOUND)
  target_link_libraries(fbserv OpenSSL::SSL OpenSSL::Crypto)
  target_compile_definitions(fbserv PRIVATE HAVE_OPENSSL_RAND_H=1 HAVE_OPENSSL_SSL_H=1)
endif()

cmakefiles(
  auth.h
  tls_wrap.h
  CMakeLists.txt
)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
