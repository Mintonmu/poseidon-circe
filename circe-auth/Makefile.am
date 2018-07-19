AM_CPPFLAGS = -Wall -Wextra -Wconversion -Wsign-conversion -Wno-error=unused	\
	-Winvalid-pch -Wwrite-strings -Wsuggest-attribute=noreturn -Wswitch-enum -Wundef -Wshadow	\
	-Wstrict-aliasing=2 -Wstrict-overflow=2 -Wno-error=pragmas -Wno-error-warning-option	\
	-pipe -fPIC -DPIC -pthread
AM_CXXFLAGS = -Wnon-virtual-dtor

pkgsysconfdir = ${sysconfdir}/@PACKAGE@
pkgsysconf_DATA =	\
	etc/circe/circe-auth-template.conf

pkginclude_HEADERS =

pkginclude_authdir = ${pkgincludedir}/auth
pkginclude_auth_HEADERS =	\
	circe-auth/src/singletons/servlet_container.hpp	\
	circe-auth/src/singletons/auth_acceptor.hpp	\
	circe-auth/src/singletons/user_defined_functions.hpp	\
	circe-auth/src/mmain.hpp

lib_LTLIBRARIES =	\
	lib/libcirce-auth.la

lib_libcirce_auth_la_SOURCES =	\
	circe-auth/src/singletons/servlet_container.cpp	\
	circe-auth/src/singletons/auth_acceptor.cpp	\
	circe-auth/src/singletons/user_defined_functions.cpp	\
	circe-auth/src/mmain.cpp	\
	circe-auth/src/servlets.cpp

lib_libcirce_auth_la_LDFLAGS =	\
	-Wl,--no-undefined -module -no-undefined -version-info 0:0:0

lib_libcirce_auth_la_LIBADD =	\
	../circe-common/lib/libcirce-common.la	\
	-lposeidon-main
