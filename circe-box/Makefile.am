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
	etc/circe/circe-box-template.conf

pkginclude_HEADERS =

pkginclude_boxdir = $(pkgincludedir)/box
pkginclude_box_HEADERS = \
	circe-box/src/singletons/servlet_container.hpp	\
	circe-box/src/singletons/box_acceptor.hpp	\
	circe-box/src/mmain.hpp

lib_LTLIBRARIES = \
	lib/libcirce-box.la

lib_libcirce_box_la_SOURCES = \
	circe-box/src/singletons/servlet_container.cpp	\
	circe-box/src/singletons/box_acceptor.cpp	\
	circe-box/src/mmain.cpp	\
	circe-box/src/servlets.cpp

lib_libcirce_box_la_LDFLAGS = \
	-module -no-undefined -version-info 0:0:0

lib_libcirce_box_la_LIBADD = \
	../circe-common/lib/libcirce-common.la
