#!/usr/bin/env bash
master=rs232_master
cd ${master}
make clean
rmmod ${master}
