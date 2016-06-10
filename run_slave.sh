#!/usr/bin/env bash
if [ $# -eq 0 ] || [ $# -ge 3 ]; then
	echo "Usage: ${0} <output file> <ip>"
else
	echo "Both slave and master use mmap"
	temp=0
	for count in {1..10}
	do
		temp=${temp}+$(./slave ${1} mmap ${2} | awk '{print $3}')
	done
	total=$(echo ${temp} | bc -l)
	mmap_average=$(echo ${total}/10 | bc -l)
	echo "mmap average time: ${mmap_average}"

	echo "Both slave and master use fcntl"
	temp=0
	for count in {1..10}
	do
		temp=${temp}+$(./slave ${1} fcntl ${2} | awk '{print $3}')
	done
	total=$(echo ${temp} | bc -l)
	fcntl_average=$(echo ${total}/10 | bc -l)
	echo "fcntl average time: ${fcntl_average}"
fi
