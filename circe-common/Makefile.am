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
	circe-common/src/precompiled.hpp.gch

CLEANFILES = \
	circe-common/src/precompiled.hpp.gch

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
	../protocol/messages_foyer_box.hpp	\
	../protocol/error_codes.hpp	\
	../protocol/utilities.hpp

pkginclude_commondir = $(pkgincludedir)/common
pkginclude_common_HEADERS = \
	circe-common/src/fwd.hpp	\
	circe-common/src/cbpp_response.hpp	\
	circe-common/src/interserver_servlet_callback.hpp	\
	circe-common/src/interserver_connection.hpp	\
	circe-common/src/interserver_servlet_container.hpp	\
	circe-common/src/interserver_connector.hpp	\
	circe-common/src/interserver_acceptor.hpp	\
	circe-common/src/define_interserver_servlet_for.hpp	\
	circe-common/src/mmain.hpp

lib_LTLIBRARIES = \
	lib/libcirce-common.la

lib_libcirce_common_la_SOURCES = \
	../protocol/messages.cpp	\
	circe-common/src/cbpp_response.cpp	\
	circe-common/src/interserver_connection.cpp	\
	circe-common/src/interserver_servlet_container.cpp	\
	circe-common/src/interserver_connector.cpp	\
	circe-common/src/interserver_acceptor.cpp	\
	circe-common/src/mmain.cpp

lib_libcirce_common_la_LDFLAGS = \
	-module -no-undefined -version-info 0:0:0
