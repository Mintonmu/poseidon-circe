#!/bin/bash -e

function check_one(){
	_file="$1"
	_cmd="gcc -std=c++11 -I. -x c++ -c -o /dev/null"
	echo "Checking \`#include\` directives:  ${_cmd}  \"${_file}\""
	${_cmd}  "${_file}"
}

export -f check_one
find -L ./circe-*/src/ -name "*.hpp" -print0 | xargs -0 -I {} bash -c 'check_one "$@"' "$0" {}
