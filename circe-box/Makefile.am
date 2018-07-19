AM_CPPFLAGS = -Wall -Wextra -Werror -Wconversion -Wsign-conversion	\
	-Winvalid-pch -Wno-missing-field-initializers -Wwrite-strings -Wsuggest-attribute=noreturn	\
	-Wundef -Wshadow -Wstrict-aliasing=2 -Wstrict-overflow=2 -Wno-error=pragmas	\
	-Wno-long-long -Wno-variadic-macros -Wno-error=unused-{function,variable,parameter}	\
	-pipe -fPIC -DPIC -pthread
AM_CXXFLAGS = -Wnon-virtual-dtor

pkgsysconfdir = ${sysconfdir}/@PACKAGE@
pkgsysconf_DATA =	\
	etc/circe/circe-box-template.conf

pkginclude_HEADERS =

pkginclude_boxdir = ${pkgincludedir}/box
pkginclude_box_HEADERS =	\
	circe-box/src/singletons/servlet_container.hpp	\
	circe-box/src/singletons/box_acceptor.hpp	\
	circe-box/src/websocket_shadow_session.hpp	\
	circe-box/src/singletons/websocket_shadow_session_supervisor.hpp	\
	circe-box/src/singletons/pilot_connector.hpp	\
	circe-box/src/singletons/user_defined_functions.hpp	\
	circe-box/src/mmain.hpp

lib_LTLIBRARIES =	\
	lib/libcirce-box.la

lib_libcirce_box_la_SOURCES =	\
	circe-box/src/singletons/servlet_container.cpp	\
	circe-box/src/singletons/box_acceptor.cpp	\
	circe-box/src/websocket_shadow_session.cpp	\
	circe-box/src/singletons/websocket_shadow_session_supervisor.cpp	\
	circe-box/src/singletons/pilot_connector.cpp	\
	circe-box/src/singletons/user_defined_functions.cpp	\
	circe-box/src/mmain.cpp	\
	circe-box/src/servlets.cpp

lib_libcirce_box_la_LDFLAGS =	\
	-Wl,--no-undefined -module -no-undefined -version-info 0:0:0

lib_libcirce_box_la_LIBADD =	\
	../circe-common/lib/libcirce-common.la	\
	-lposeidon-main
