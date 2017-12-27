ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign subdir-objects
AM_CPPFLAGS = -Wall -Wextra -Werror -Wconversion -Wsign-conversion -Wno-error=unused-parameter	\
	-Winvalid-pch -Wno-missing-field-initializers -Wwrite-strings -Wsuggest-attribute=noreturn	\
	-Wundef -Wshadow -Wstrict-aliasing=2 -Wstrict-overflow=2 -Wno-error=pragmas	\
	-pipe -fPIC -DPIC -pthread
AM_CXXFLAGS =
AM_LIBS =

pkgsysconfdir = $(sysconfdir)/@PACKAGE@
pkgsysconf_DATA = \
	etc/circe/circe-foyer-template.conf

pkginclude_HEADERS =

pkginclude_foyerdir = $(pkgincludedir)/foyer
pkginclude_foyer_HEADERS = \
	circe-foyer/src/singletons/servlet_container.hpp	\
	circe-foyer/src/singletons/foyer_acceptor.hpp	\
	circe-foyer/src/singletons/box_connector.hpp	\
	circe-foyer/src/mmain.hpp

lib_LTLIBRARIES = \
	lib/libcirce-foyer.la

lib_libcirce_foyer_la_SOURCES = \
	circe-foyer/src/singletons/servlet_container.cpp	\
	circe-foyer/src/singletons/foyer_acceptor.cpp	\
	circe-foyer/src/singletons/box_connector.cpp	\
	circe-foyer/src/mmain.cpp	\
	circe-foyer/src/servlets.cpp

lib_libcirce_foyer_la_LDFLAGS = \
	-module -no-undefined -version-info 0:0:0

lib_libcirce_foyer_la_LIBADD = \
	../circe-common/lib/libcirce-common.la
