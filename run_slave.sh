#!/usr/bin/env bash
if [ $# -eq 0 ] || [ $# -ge 4 ]; then
	echo "Usage: ${0} <output file> <method> <ip>"
else
	echo "Slave use ${2}"
	temp=0
	for count in {1..10}
	do
		useless=$(./slave ${1} ${2} ${3})
		echo ${useless}
		temp=${temp}+$(echo ${useless} | awk '{print $3}')
		#temp=${temp}+$(./slave ${1} ${2} ${3} | awk '{print $3}')
	done
	total=$(echo ${temp} | bc -l)
	mmap_average=$(echo ${total}/10 | bc -l)
	echo "Average time: ${mmap_average} ms"
fi
