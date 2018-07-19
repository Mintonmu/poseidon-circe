AM_CPPFLAGS = -Wall -Wextra -Wconversion -Wsign-conversion -Wno-error=unused	\
	-Winvalid-pch -Wwrite-strings -Wsuggest-attribute=noreturn -Wswitch-enum -Wundef -Wshadow	\
	-Wstrict-aliasing=2 -Wstrict-overflow=2 -Wno-error=pragmas -Wno-error-warning-option	\
	-pipe -fPIC -DPIC -pthread
AM_CXXFLAGS = -Wnon-virtual-dtor

pkgsysconfdir = ${sysconfdir}/@PACKAGE@
pkgsysconf_DATA =	\
	etc/circe/circe-gate-template.conf

pkginclude_HEADERS =

pkginclude_gatedir = ${pkgincludedir}/gate
pkginclude_gate_HEADERS =	\
	circe-gate/src/singletons/servlet_container.hpp	\
	circe-gate/src/client_http_session.hpp	\
	circe-gate/src/client_websocket_session.hpp	\
	circe-gate/src/singletons/client_http_acceptor.hpp	\
	circe-gate/src/singletons/auth_connector.hpp	\
	circe-gate/src/singletons/foyer_connector.hpp	\
	circe-gate/src/singletons/ip_ban_list.hpp	\
	circe-gate/src/mmain.hpp

lib_LTLIBRARIES =	\
	lib/libcirce-gate.la

lib_libcirce_gate_la_SOURCES =	\
	circe-gate/src/singletons/servlet_container.cpp	\
	circe-gate/src/client_http_session.cpp	\
	circe-gate/src/client_websocket_session.cpp	\
	circe-gate/src/singletons/client_http_acceptor.cpp	\
	circe-gate/src/singletons/auth_connector.cpp	\
	circe-gate/src/singletons/foyer_connector.cpp	\
	circe-gate/src/singletons/ip_ban_list.cpp	\
	circe-gate/src/mmain.cpp	\
	circe-gate/src/servlets.cpp

lib_libcirce_gate_la_LDFLAGS =	\
	-Wl,--no-undefined -module -no-undefined -version-info 0:0:0

lib_libcirce_gate_la_LIBADD =	\
	../circe-common/lib/libcirce-common.la	\
	-lposeidon-main
