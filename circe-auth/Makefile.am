ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign subdir-objects
AM_CPPFLAGS = -Wall -Wextra -Werror -Wsign-conversion -Wno-error=unused-parameter -Winvalid-pch	\
	-Wno-missing-field-initializers -Wwrite-strings -Wsuggest-attribute=noreturn -Wundef -Wshadow	\
	-Wstrict-aliasing=2 -Wstrict-overflow=2 -Wno-error=pragmas -pipe -fPIC -DPIC -pthread
AM_CXXFLAGS =
AM_LIBS =

pkgsysconfdir = $(sysconfdir)/@PACKAGE@
pkgsysconf_DATA = \
	etc/circe/circe-auth-template.conf

pkginclude_HEADERS =

pkginclude_authdir = $(pkgincludedir)/auth
pkginclude_auth_HEADERS = \
	circe-auth/src/singletons/servlet_container.hpp	\
	circe-auth/src/singletons/auth_acceptor.hpp	\
	circe-auth/src/mmain.hpp

lib_LTLIBRARIES = \
	lib/libcirce-auth.la

lib_libcirce_auth_la_SOURCES = \
	circe-auth/src/singletons/servlet_container.cpp	\
	circe-auth/src/singletons/auth_acceptor.cpp	\
	circe-auth/src/mmain.cpp	\
	circe-auth/src/servlets.cpp

lib_libcirce_auth_la_LDFLAGS = \
	-module -no-undefined -version-info 0:0:0

lib_libcirce_auth_la_LIBADD = \
	../circe-common/lib/libcirce-common.la
