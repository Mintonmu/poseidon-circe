#!/bin/bash

set -e

_output="etc/circe/circe.sql"
_conf="etc/circe/main.conf"

printf "Dumping MySQL table structure into '%s' using settings from '%s'...\\n" "${_output}" "${_conf}"

_host="$(sed -E "s/mysql_server_addr\\s*=\\s*([^\\s]*)/\\1/;t;d" "${_conf}" | head -n1)"; [[ -z "${_host}" ]] && _host="localhost"
_port="$(sed -E "s/mysql_server_port\\s*=\\s*([^\\s]*)/\\1/;t;d" "${_conf}" | head -n1)"; [[ -z "${_port}" ]] && _port="3306"
_username="$(sed -E "s/mysql_server_username\\s*=\\s*([^\\s]*)/\\1/;t;d" "${_conf}" | head -n1)"; [[ -z "${_username}" ]] && _username="root"
_password="$(sed -E "s/mysql_server_password\\s*=\\s*([^\\s]*)/\\1/;t;d" "${_conf}" | head -n1)"; [[ -z "${_password}" ]] && _password="root"
_schema="$(sed -E "s/mysql_server_schema\\s*=\\s*([^\\s]*)/\\1/;t;d" "${_conf}" | head -n1)"; [[ -z "${_schema}" ]] && _schema="circe"
_use_ssl="$(sed -E "s/mysql_server_use_ssl\\s*=\\s*([^\\s]*)/\\1/;t;d" "${_conf}" | head -n1)"; [[ -z "${_use_ssl}" ]] && _use_ssl="0"
_charset="$(sed -E "s/mysql_server_charset\\s*=\\s*([^\\s]*)/\\1/;t;d" "${_conf}" | head -n1)"; [[ -z "${_charset}" ]] && _charset="utf8"

mysqldump --result-file="${_output}" --host="${_host}" --port="${_port}" --user="${_username}" --password="${_password}"	\
	--default-character-set="${_charset}" $([[ "${_use_ssl}" -ne 0 ]] && echo "--ssl")	\
	--no-data --compact --compress --replace --routines --single-transaction --verbose	\
	--databases "${_schema}"
