AM_CPPFLAGS = -Wall -Wextra -Werror -Wconversion -Wsign-conversion -Wno-error=unused	\
	-Winvalid-pch -Wno-missing-field-initializers -Wwrite-strings -Wsuggest-attribute=noreturn	\
	-Wundef -Wshadow -Wstrict-aliasing=2 -Wstrict-overflow=2 -Wno-error=pragmas	\
	-pipe -fPIC -DPIC -pthread
AM_CXXFLAGS = -Wnon-virtual-dtor
AM_LIBS =

pkgsysconfdir = ${sysconfdir}/@PACKAGE@
pkgsysconf_DATA =	\
	etc/circe/circe-pilot-template.conf

pkginclude_HEADERS =

pkginclude_pilotdir = ${pkgincludedir}/pilot
pkginclude_pilot_HEADERS =	\
	circe-pilot/src/singletons/servlet_container.hpp	\
	circe-pilot/src/singletons/pilot_acceptor.hpp	\
	circe-pilot/src/orm.hpp	\
	circe-pilot/src/singletons/compass_repository.hpp	\
	circe-pilot/src/compass_key.hpp	\
	circe-pilot/src/compass_lock.hpp	\
	circe-pilot/src/compass_watcher_map.hpp	\
	circe-pilot/src/compass.hpp	\
	circe-pilot/src/mmain.hpp

lib_LTLIBRARIES =	\
	lib/libcirce-pilot.la

lib_libcirce_pilot_la_SOURCES =	\
	circe-pilot/src/singletons/servlet_container.cpp	\
	circe-pilot/src/singletons/pilot_acceptor.cpp	\
	circe-pilot/src/orm.cpp	\
	circe-pilot/src/singletons/compass_repository.cpp	\
	circe-pilot/src/compass_key.cpp	\
	circe-pilot/src/compass_lock.cpp	\
	circe-pilot/src/compass_watcher_map.cpp	\
	circe-pilot/src/compass.cpp	\
	circe-pilot/src/mmain.cpp	\
	circe-pilot/src/servlets.cpp

lib_libcirce_pilot_la_LDFLAGS =	\
	-Wl,--no-undefined -module -version-info 0:0:0

lib_libcirce_pilot_la_LIBADD =	\
	../circe-common/lib/libcirce-common.la	\
	-lposeidon-main
