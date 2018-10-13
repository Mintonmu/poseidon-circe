AM_CPPFLAGS = -Wall -Wextra -Werror -Wconversion -Wsign-conversion	\
	-Winvalid-pch -Wno-missing-field-initializers -Wwrite-strings -Wsuggest-attribute=noreturn	\
	-Wundef -Wshadow -Wstrict-aliasing=2 -Wstrict-overflow=2 -Wno-error=pragmas	\
	-Wno-long-long -Wno-variadic-macros -Wno-error=unused-{function,variable,parameter}	\
	-pipe -fPIC -DPIC -pthread
AM_CXXFLAGS = -Wnon-virtual-dtor

%.hpp.gch: %.hpp ../config.h
	${AM_V_CXX}${LTCXXCOMPILE} -x c++-header -Wno-error $< -o $@.o
	${AM_V_GEN}${LIBTOOL} ${AM_V_lt} --tag=CXX --mode=link sh -c 'mv -f "$$1" "$$3"' _ $@.lo -o $@

BUILT_SOURCES =	\
	circe-common/src/precompiled.hpp.gch

CLEANFILES =	\
	circe-common/src/precompiled.hpp.gch

sysconf_DATA =

pkgsysconfdir = ${sysconfdir}/@PACKAGE@
pkgsysconf_DATA =	\
	etc/circe/main-template.conf	\
	etc/circe/circe-common-template.conf

pkginclude_HEADERS =

pkginclude_protocoldir = ${pkgincludedir}/protocol
pkginclude_protocol_HEADERS =	\
	../protocol/messages_common.hpp	\
	../protocol/messages_gate.hpp	\
	../protocol/messages_foyer.hpp	\
	../protocol/messages_auth.hpp	\
	../protocol/messages_box.hpp	\
	../protocol/messages_pilot.hpp	\
	../protocol/error_codes.hpp	\
	../protocol/exception.hpp

pkginclude_commondir = ${pkgincludedir}/common
pkginclude_common_HEADERS =	\
	circe-common/src/fwd.hpp	\
	circe-common/src/interserver_response.hpp	\
	circe-common/src/interserver_servlet_callback.hpp	\
	circe-common/src/interserver_connection.hpp	\
	circe-common/src/interserver_servlet_container.hpp	\
	circe-common/src/interserver_connector.hpp	\
	circe-common/src/interserver_acceptor.hpp	\
	circe-common/src/define_interserver_servlet_for.hpp	\
	circe-common/src/mmain.hpp

lib_LTLIBRARIES =	\
	lib/libcirce-common.la

lib_libcirce_common_la_SOURCES =	\
	../protocol/messages_external_definitions.cpp	\
	circe-common/src/interserver_response.cpp	\
	circe-common/src/interserver_connection.cpp	\
	circe-common/src/interserver_servlet_container.cpp	\
	circe-common/src/interserver_connector.cpp	\
	circe-common/src/interserver_acceptor.cpp	\
	circe-common/src/mmain.cpp

lib_libcirce_common_la_LDFLAGS =	\
	-Wl,--no-undefined -module -no-undefined -version-info 0:0:0

lib_libcirce_common_la_LIBADD =		\
	-lposeidon-main	\
	-lcrypto
