ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign subdir-objects
AM_CPPFLAGS = -Wall -Wextra -Werror -Wsign-conversion -Wno-error=unused-parameter -Winvalid-pch	\
	-Wno-missing-field-initializers -Wwrite-strings -Wsuggest-attribute=noreturn -Wundef -Wshadow	\
	-Wstrict-aliasing=2 -Wstrict-overflow=2 -Wno-error=pragmas -pipe -fPIC -DPIC -pthread
AM_CXXFLAGS =
AM_LIBS =

%.hpp.gch: %.hpp
	$(CXX) -x c++-header @DEFS@ $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CXXFLAGS) $(CXXFLAGS) -Wno-error $< -o $@

BUILT_SOURCES = \
	src/precompiled.hpp.gch

CLEANFILES = \
	src/precompiled.hpp.gch

sysconf_DATA =

pkgsysconfdir = $(sysconfdir)/@PACKAGE@
pkgsysconf_DATA = \
	etc/circe/main-template.conf	\
	etc/circe/circe-common-template.conf

pkginclude_HEADERS =

pkginclude_protocoldir = $(pkgincludedir)/protocol
pkginclude_protocol_HEADERS = \
	../protocol/messages_gate_auth.hpp	\
	../protocol/messages_gate_foyer.hpp	\
	../protocol/messages_foyer_business.hpp	\
	../protocol/error_codes.hpp	\
	../protocol/utilities.hpp

pkginclude_commondir = $(pkgincludedir)/common
pkginclude_common_HEADERS = \
	src/cbpp_response.hpp	\
	src/interserver_servlet_callback.hpp	\
	src/interserver_connection.hpp	\
	src/interserver_servlet_container.hpp	\
	src/interserver_connector.hpp	\
	src/interserver_acceptor.hpp	\
	src/define_interserver_servlet_for.hpp	\
	src/mmain.hpp

lib_LTLIBRARIES = \
	lib/libcirce-common.la

lib_libcirce_common_la_SOURCES = \
	../protocol/messages.cpp	\
	src/cbpp_response.cpp	\
	src/interserver_connection.cpp	\
	src/interserver_servlet_container.cpp	\
	src/interserver_connector.cpp	\
	src/interserver_acceptor.cpp	\
	src/mmain.cpp

lib_libcirce_common_la_LDFLAGS = \
	-module -no-undefined -version-info 0:0:0
