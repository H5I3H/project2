#!/usr/bin/env bash
if [ $# -eq 0 ] || [ $# -ge 3 ]; then
	echo "Usage: ${0} <input file> <method>"
else
	echo "Master use ${2}"
	for count in {1..10}
	do
		./master ${1} ${2}
	done
fi
