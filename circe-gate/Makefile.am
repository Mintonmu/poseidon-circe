ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign subdir-objects
AM_CPPFLAGS = -Wall -Wextra -Werror -Wsign-conversion -Wno-error=unused-parameter -Winvalid-pch	\
	-Wno-missing-field-initializers -Wwrite-strings -Wsuggest-attribute=noreturn -Wundef -Wshadow	\
	-Wstrict-aliasing=2 -Wstrict-overflow=2 -Wno-error=pragmas -pipe -fPIC -DPIC -pthread
AM_CXXFLAGS =
AM_LIBS =

pkgsysconfdir = $(sysconfdir)/@PACKAGE@
pkgsysconf_DATA = \
	etc/circe/circe-gate-template.conf

pkginclude_HEADERS =

pkginclude_gatedir = $(pkgincludedir)/gate
pkginclude_gate_HEADERS = \
	src/client_http_session.hpp	\
	src/client_websocket_session.hpp	\
	src/singletons/client_http_acceptor.hpp	\
	src/singletons/auth_connector.hpp	\
	src/singletons/foyer_connector.hpp	\
	src/mmain.hpp

lib_LTLIBRARIES = \
	lib/libcirce-gate.la

lib_libcirce_gate_la_SOURCES = \
	src/client_http_session.cpp	\
	src/client_websocket_session.cpp	\
	src/singletons/client_http_acceptor.cpp	\
	src/singletons/auth_connector.cpp	\
	src/singletons/foyer_connector.cpp	\
	src/mmain.cpp

lib_libcirce_gate_la_LDFLAGS = \
	-module -no-undefined -version-info 0:0:0

lib_libcirce_gate_la_LIBADD = \
	../circe-common/lib/libcirce-common.la
