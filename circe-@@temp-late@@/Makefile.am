AM_CPPFLAGS = -Wall -Wextra -Werror -Wconversion -Wsign-conversion -Wno-error=unused-parameter	\
	-Winvalid-pch -Wno-missing-field-initializers -Wwrite-strings -Wsuggest-attribute=noreturn	\
	-Wundef -Wshadow -Wstrict-aliasing=2 -Wstrict-overflow=2 -Wno-error=pragmas	\
	-pipe -fPIC -DPIC -pthread
AM_CXXFLAGS = -Wnon-virtual-dtor
AM_LIBS =

pkgsysconfdir = ${sysconfdir}/@PACKAGE@
pkgsysconf_DATA =	\
	etc/circe/circe-@@temp-late@@-template.conf

pkginclude_HEADERS =

pkginclude_@@temp_late@@dir = ${pkgincludedir}/@@temp-late@@
pkginclude_@@temp_late@@_HEADERS =	\
	circe-@@temp-late@@/src/mmain.hpp

lib_LTLIBRARIES =	\
	lib/libcirce-@@temp-late@@.la

lib_libcirce_@@temp_late@@_la_SOURCES =	\
	circe-@@temp-late@@/src/mmain.cpp

lib_libcirce_@@temp_late@@_la_LDFLAGS =	\
	-module -no-undefined -version-info 0:0:0

lib_libcirce_@@temp_late@@_la_LIBADD =	\
	../circe-common/lib/libcirce-common.la
