ACLOCAL_AMFLAGS = -I m4
AUTOMAKE_OPTIONS = foreign subdir-objects

EXTRA_DIST = \
	etc/circe/main-template.conf	\
	etc/circe/circe-common-template.conf	\
	etc/circe/circe-gate-template.conf	\
	etc/circe/circe-foyer-template.conf	\
	etc/circe/circe-auth-template.conf

SUBDIRS = \
	circe-common	\
	circe-gate	\
	circe-foyer	\
	circe-auth

pkginclude_HEADERS =

pkginclude_protocoldir = $(pkgincludedir)/protocol
pkginclude_protocol_HEADERS = \
	protocol/messages_gate_foyer.hpp	\
	protocol/messages_foyer_auth.hpp	\
	protocol/messages_foyer_business.hpp	\
	protocol/error_codes.hpp
