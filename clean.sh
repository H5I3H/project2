#!/usr/bin/env bash
master=rs232_master
slave=rs232_slave
cd ${master}
make clean
rmmod ${master}
cd ${slave}
make clean
rmmod ${slave}
